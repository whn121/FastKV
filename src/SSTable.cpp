#include "SSTable.h"
#include <filesystem>
#include <iostream>


//标记 1 + kl 4 + k + vl 4 +vl

bool SSTable::Write(const MemTable &memtable, uint64_t* next_id)
{
    std::string path = "../data/level_0/SSTable_" + std::to_string(*next_id) + ".sst";
    std::string tmp_path = path + ".tmp";

    // 用 trunc 创建临时文件
    std::ofstream f(tmp_path, std::ios::binary | std::ios::trunc);
    if (!f) return false;

    Footer footer{};

    // 1.Data Section
    footer.data_offset = f.tellp();

    BloomFilter bloom(memtable.Data().size());

    for (const auto& it : memtable.Data())
    {
        uint8_t deleted = it.second.deleted;
        uint32_t key_len = static_cast<uint32_t> (it.first.size());
        uint32_t value_len = static_cast<uint32_t> (it.second.value.size());

        // 相对 data section 的偏移
        uint64_t offset_in_data = static_cast<uint64_t> (f.tellp()) - footer.data_offset;

        level0_index_[*next_id][it.first] = offset_in_data;
        bloom.Add(it.first);

        f.write(reinterpret_cast<const char*>(&deleted), sizeof(deleted));
        f.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        f.write(it.first.data(), key_len);
        f.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
        f.write(it.second.value.data(), value_len);

        if (!f) return false;

    }

    footer.data_size = static_cast<uint64_t>(f.tellp()) - footer.data_offset;

    //2.Index Section
    footer.index_offset = f.tellp();

    for (const auto& [key, offset] : level0_index_[*next_id])
    {
        uint32_t key_len = static_cast<uint32_t>(key.size());
        f.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        f.write(key.data(), key_len);
        f.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
    }

    footer.index_size = static_cast<uint64_t>(f.tellp()) - footer.index_offset;

    //3.Bloom Section
    footer.bloom_offset = f.tellp();

    level0_bloom_[*next_id] = std::move(bloom);
    std::string bloom_data = level0_bloom_[*next_id].Serialize();
    f.write(bloom_data.data(), bloom_data.size());

    footer.bloom_size = static_cast<uint64_t>(f.tellp()) - footer.bloom_offset;

    //4.Footer
    footer.magic = FOOTER_MAGIC;
    f.write(reinterpret_cast<const char*>(&footer), sizeof(footer));

    f.close();

    //5. 原子 rename 全部写完变更名字,防止写一半中断存在不完整的.sst文件,现在要么没有要么完整
    try 
    {
        std::filesystem::rename(tmp_path, path);
    } 
    catch (const std::exception& e) 
    {
        std::cerr << "rename failed: " << e.what() << std::endl;
        return false;
    }

    return true;
}


bool SSTable::Get(const std::string& key, std::string& value, uint64_t lv0_startid, uint64_t lv1_startid)
{
    //level_0
    if (lv0_startid != UINT64_MAX)
    {
        for (uint64_t id = lv0_startid + 1; id-- > 0; )
        {
            // 1. Bloom 过滤
            if (!BloomMayContain(id, 0, key)) {
                if (id == 0) break;
                continue;
            }

            //2. 查内存 Index
            auto idx_it = level0_index_.find(id);
            if (idx_it == level0_index_.end())
            {
                if (id == 0) break;
                continue;
            }
            auto it = idx_it->second.find(key);
            if (it == idx_it->second.end())
            {
                if (id == 0) break;
                continue;
            }

            //3. 从磁盘读 data
            std::string path = "../data/level_0/SSTable_" + std::to_string(id) + ".sst";
            std::ifstream f(path, std::ios::binary);
            if (!f)
            {
                if (id == 0) break;
                continue;
            }

            uint64_t data_offset = level0_data_offset_[id];
            f.seekg(data_offset + it->second);

            if (ReadEntry(f, value)) return true;
            return false;   // tombstone 或读失败

            if (id == 0) break;

        }
    }

    //level_1
    if (lv1_startid != UINT64_MAX)
    {
        for (uint64_t id = lv1_startid + 1; id-- > 0; )
        {
            if (!BloomMayContain(id, 1, key)) {
                if (id == 0) break;
                continue;
            }

            auto idx_it = level1_index_.find(id);
            if (idx_it == level1_index_.end()) {
                if (id == 0) break;
                continue;
            }
            auto it = idx_it->second.find(key);
            if (it == idx_it->second.end()) {
                if (id == 0) break;
                continue;
            }

            std::string path = "../data/level_1/SSTable_" + std::to_string(id) + ".sst";
            std::ifstream f(path, std::ios::binary);
            if (!f) {
                if (id == 0) break;
                continue;
            }

            uint64_t data_offset = level1_data_offset_[id];
            f.seekg(data_offset + it->second);

            if (ReadEntry(f, value)) return true;
            return false;

            if (id == 0) break;
        }
    }

    return false;

}

bool SSTable::LoadFile(uint64_t id, int level, const std::string& path)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);//ate从尾端打开
    if (!f) return false;

    //1.读Footer
    std::streamsize file_size = f.tellg();
    if (file_size < static_cast<std::streamsize>(FOOTER_SIZE)) return false;

    f.seekg(-static_cast<std::streamoff>(FOOTER_SIZE), std::ios::end); //向左移动第一个参数,基于第二个参数

    Footer footer;
    f.read(reinterpret_cast<char*> (&footer), sizeof(footer));
    if (!f) return false;

    //2.验证magic
    if (footer.magic != FOOTER_MAGIC)
    {
        std::cerr << "SSTable magic mismatch: " << path << std::endl;
        return false;
    }

    //3.验证offset合法性
    if (footer.data_offset + footer.data_size != footer.index_offset) return false;
    if (footer.index_offset + footer.index_size != footer.bloom_offset) return false;
    if (footer.bloom_offset + footer.bloom_size + FOOTER_SIZE != static_cast<uint64_t>(file_size)) return false;

    //4.保存data_offset到内存
    if(level == 0) level0_data_offset_[id] = footer.data_offset;
    else level1_data_offset_[id] = footer.data_offset;

    //5.读Index Section
    f.seekg(footer.index_offset);

    uint64_t index_end = footer.index_offset + footer.index_size;
    auto& index_map = (level == 0) ? level0_index_[id] : level1_index_[id];

    while (static_cast<uint64_t>(f.tellg()) < index_end)
    {
        uint32_t key_len;
        if (!f.read(reinterpret_cast<char*>(&key_len), sizeof(key_len))) break;

        std::string key;
        key.resize(key_len);
        if (!f.read(key.data(), key_len)) break;

        uint64_t offset;
        if (!f.read(reinterpret_cast<char*>(&offset), sizeof(offset))) break;

        index_map[key] = offset;
    }

    //6.读Bloom Section
    f.seekg(footer.bloom_offset);

    std::string bloom_data;
    bloom_data.resize(footer.bloom_size);
    if (!f.read(&bloom_data[0], footer.bloom_size)) return false;

    BloomFilter bf;
    if (bf.Deserialize(bloom_data)) 
    {
        if (level == 0) level0_bloom_[id] = std::move(bf);
        else            level1_bloom_[id] = std::move(bf);
    }

    return true;
}

bool SSTable::BloomMayContain(uint64_t id, int level, const std::string &key) const
{
    if (level == 0) 
    {
        auto it = level0_bloom_.find(id);
        if (it == level0_bloom_.end()) 
        {
            // 没找到 bloom，保守返回 true（不确定就查 index）
            return true;
        }
        return it->second.MayContain(key);
    } 
    else 
    {
        auto it = level1_bloom_.find(id);
        if (it == level1_bloom_.end()) 
        {
            return true;
        }
        return it->second.MayContain(key);
    }
    return false;
}

bool SSTable::Compact(uint64_t& level_0_next_id, uint64_t& level_1_next_id)
{
    if (level_0_next_id == 0) return false;

    std::string path = "../data/level_1/SSTable_" + std::to_string(level_1_next_id) + ".sst";
    std::string tmp_path = path + ".tmp";

    std::ofstream f(tmp_path, std::ios::binary | std::ios::trunc);
    if (!f) return false;

    Footer footer{};

    //1. 合并 level_0 的所有 key 到 map
    std::map<std::string, std::pair<uint8_t, std::string>> merged;

    for (uint64_t id = 0; id < level_0_next_id; ++id)
    {
        std::string lv0_path = "../data/level_0/SSTable_" + std::to_string(id) + ".sst";
        std::ifstream f0(lv0_path, std::ios::binary | std::ios::ate);
        if (!f0) continue;

        // 读 Footer
        std::streamsize fsize = f0.tellg();
        if (fsize < (std::streamsize)FOOTER_SIZE) continue;
        f0.seekg(-(std::streamoff)FOOTER_SIZE, std::ios::end);

        Footer ft0;
        f0.read(reinterpret_cast<char*>(&ft0), sizeof(ft0));
        if (ft0.magic != FOOTER_MAGIC) continue;

        // 读 Data Section
        f0.seekg(ft0.data_offset);
        uint64_t data_end = ft0.data_offset + ft0.data_size;

        while ((uint64_t)f0.tellg() < data_end)
        {
            uint8_t deleted;
            if (!f0.read(reinterpret_cast<char*>(&deleted), sizeof(deleted))) break;

            uint32_t key_len;
            if (!f0.read(reinterpret_cast<char*>(&key_len), sizeof(key_len))) break;
            std::string key;
            key.resize(key_len);
            if (!f0.read(key.data(), key_len)) break;

            uint32_t value_len;
            if (!f0.read(reinterpret_cast<char*>(&value_len), sizeof(value_len))) break;
            std::string value;
            value.resize(value_len);
            if (!f0.read(value.data(), value_len)) break;

            merged[key] = {deleted, value};   // 后覆盖先
        }
    }

    //2. 写 Data Section
    footer.data_offset = f.tellp();
    BloomFilter bloom(merged.size());

    for (const auto& [key, kv] : merged)
    {
        uint8_t deleted = kv.first;
        const std::string& value = kv.second;

        uint32_t key_len = static_cast<uint32_t>(key.size());
        uint32_t value_len = static_cast<uint32_t>(value.size());

        uint64_t offset_in_data = static_cast<uint64_t>(f.tellp()) - footer.data_offset;

        level1_index_[level_1_next_id][key] = offset_in_data;
        bloom.Add(key);

        f.write(reinterpret_cast<const char*>(&deleted), sizeof(deleted));
        f.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        f.write(key.data(), key_len);
        f.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
        f.write(value.data(), value_len);
    }

    footer.data_size = static_cast<uint64_t>(f.tellp()) - footer.data_offset;

    // 3. 写 Index Section
    footer.index_offset = f.tellp();

    for (const auto& [key, offset] : level1_index_[level_1_next_id])
    {
        uint32_t key_len = static_cast<uint32_t>(key.size());
        f.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        f.write(key.data(), key_len);
        f.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
    }

    footer.index_size = static_cast<uint64_t>(f.tellp()) - footer.index_offset;

    //4. 写 Bloom Section
    footer.bloom_offset = f.tellp();

    level1_bloom_[level_1_next_id] = std::move(bloom);
    std::string bloom_data = level1_bloom_[level_1_next_id].Serialize();
    f.write(bloom_data.data(), bloom_data.size());

    footer.bloom_size = static_cast<uint64_t>(f.tellp()) - footer.bloom_offset;

    //5. 写 Footer
    footer.magic = FOOTER_MAGIC;
    f.write(reinterpret_cast<const char*>(&footer), sizeof(footer));

    f.close();

    //6. rename
    std::filesystem::rename(tmp_path, path);

    ++level_1_next_id;
    return true;
}

void SSTable::RemoveIndex(uint64_t id, int level)
{
    if (level == 0)
    {
        level0_index_.erase(id);
    }
    else
    {
        level1_index_.erase(id);
    }
}


bool SSTable::ReadEntry(std::ifstream& f, std::string& value)
{
    uint8_t deleted;
    if (!f.read(reinterpret_cast<char*>(&deleted), sizeof(deleted))) return false;

    uint32_t key_len;
    if (!f.read(reinterpret_cast<char*>(&key_len), sizeof(key_len))) return false;

    std::string key_tmp;
    key_tmp.resize(key_len);
    if (!f.read(key_tmp.data(), key_len)) return false;

    uint32_t value_len;
    if (!f.read(reinterpret_cast<char*>(&value_len), sizeof(value_len))) return false;

    std::string value_tmp;
    value_tmp.resize(value_len);
    if (!f.read(value_tmp.data(), value_len)) return false;

    if (deleted) return false;   // tombstone

    value = std::move(value_tmp);
    return true;
}