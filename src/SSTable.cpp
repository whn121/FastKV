#include "SSTable.h"
#include <filesystem>



//标记 1 + kl 4 + k + vl 4 +vl

bool SSTable::Write(const MemTable &memtable, uint64_t* next_id)
{
    std::string path_s = "../data/level_0/SSTable_" + std::to_string(*next_id) + ".sst";
    std::string path_i = "../data/level_0/index_" + std::to_string(*next_id) + ".idx";

    if (std::filesystem::exists(path_s)) return false; // ID 冲突，不要覆盖
    if (std::filesystem::exists(path_i)) return false; // ID 冲突，不要覆盖

    std::ofstream write_s (path_s, std::ios::trunc | std::ios::binary);//存在清空不存在创建 | 二进制打开
    if (!write_s) return false;
    std::ofstream write_i (path_i, std::ios::trunc | std::ios::binary);//存在清空不存在创建 | 二进制打开
    if (!write_i) return false;

    const auto& map = memtable.Data(); //只拿引用不复制整个map
    for (const auto& it : map)//只拿引用不复制整个map
    {
        uint8_t entry = it.second.deleted;
        uint32_t key_len = it.first.size();
        uint32_t value_len = it.second.value.size();
        
        uint64_t offset = write_s.tellp(); //获得当前位置

        level0_index_[*next_id][it.first] = offset; //

        if (!write_s.write(reinterpret_cast<const char*> (&entry), sizeof(entry))) return false;
        if (!write_s.write(reinterpret_cast<const char*> (&key_len), sizeof(key_len))) return false;
        if (!write_s.write(reinterpret_cast<const char*> (it.first.data()), key_len)) return false;
        if (!write_s.write(reinterpret_cast<const char*> (&value_len), sizeof(value_len))) return false;
        if (!write_s.write(reinterpret_cast<const char*> (it.second.value.data()), value_len)) return false;
    
        if (!write_i.write(reinterpret_cast<const char*> (&key_len), sizeof(key_len))) return false;
        if (!write_i.write(reinterpret_cast<const char*> (it.first.data()), key_len)) return false;
        if (!write_i.write(reinterpret_cast<const char*> (&offset), sizeof(offset))) return false;

    }
    
    write_s.close();
    write_i.close();
    return write_s.good() && write_i.good();//检查write的状态
}

bool SSTable::Get(const std::string& key, std::string& value, uint64_t lv0_startid, uint64_t lv1_startid)
{
    uint64_t lv0_id = lv0_startid;
    while (1)
    {
        std::ifstream read ("../data/level_0/SSTable_" + std::to_string(lv0_id) + ".sst"
                            , std::ios::binary);
        if (!read) //跳过缺失文件
        {
            if (lv0_id == 0) break;
            --lv0_id;
            continue;
        }

        auto file_index = level0_index_.find(lv0_id);

        if (file_index == level0_index_.end()) //跳过
        {
            if (lv0_id == 0) break;
            --lv0_id;
            continue;
        }

        auto it = file_index->second.find(key);

        if (it == file_index->second.end())
        {
            if (lv0_id == 0) break;
            --lv0_id;
            continue;
        }

        read.seekg(it->second);

        uint8_t deleted;
        if (!read.read(reinterpret_cast<char*> (&deleted), sizeof(deleted))) return false;
        
        uint32_t key_len;
        if (!read.read(reinterpret_cast<char*> (&key_len), sizeof(key_len))) return false;
        std::string key1;
        key1.resize(key_len);
        if (!read.read(reinterpret_cast<char*> (key1.data()), key_len)) return false;

        uint32_t value_len;
        if (!read.read(reinterpret_cast<char*> (&value_len),sizeof(value_len))) return false;
        std::string value1;
        value1.resize(value_len);
        if (!read.read(reinterpret_cast<char*> (value1.data()), value_len)) return false;

        if (deleted) return false; //最新是 tombstone.不能继续查旧表

        if (!deleted) 
        {
            value = std::move (value1);

            read.close();

            return true;
        }

        if (lv0_id == 0) break;

        --lv0_id;
    }

    uint64_t lv1_id = lv1_startid;
    while (1)
    {
        std::ifstream read ("../data/level_1/SSTable_" + std::to_string(lv1_id) + ".sst"
                            , std::ios::binary);
        if (!read) //跳过缺失文件
        {
            if (lv1_id == 0) break;
            --lv1_id;
            continue;
        }

        auto file_index = level1_index_.find(lv1_id);

        if (file_index == level1_index_.end()) //跳过
        {
            if (lv1_id == 0) break;
            --lv1_id;
            continue;
        }

        auto it = file_index->second.find(key);

        if (it == file_index->second.end())
        {
            if (lv1_id == 0) break;
            --lv1_id;
            continue;
        }

        read.seekg(it->second);

        uint8_t deleted;
        if (!read.read(reinterpret_cast<char*> (&deleted), sizeof(deleted))) return false;
        
        uint32_t key_len;
        if (!read.read(reinterpret_cast<char*> (&key_len), sizeof(key_len))) return false;
        std::string key1;
        key1.resize(key_len);
        if (!read.read(reinterpret_cast<char*> (key1.data()), key_len)) return false;

        uint32_t value_len;
        if (!read.read(reinterpret_cast<char*> (&value_len),sizeof(value_len))) return false;
        std::string value1;
        value1.resize(value_len);
        if (!read.read(reinterpret_cast<char*> (value1.data()), value_len)) return false;

        if (deleted) return false; //最新是 tombstone.不能继续查旧表

        if (!deleted) 
        {
            value = std::move (value1);

            read.close();

            return true;
        }

        if (lv1_id == 0) break;

        --lv1_id;
    }


    return false;
}

bool SSTable::SubIndex(uint64_t id, std::string &key, uint64_t offset, int level)
{
    if (level == 0)
    {
        level0_index_[id][key] = offset;
    }
    else 
    {
        level1_index_[id][key] = offset;
    }
    return true;
}

bool SSTable::Compact(uint64_t& level_0_next_id, uint64_t& level_1_next_id)
{
    // 没有可合并的文件
    if (level_0_next_id == 0) return false;

    std::string lv1_path_s = "../data/level_1/SSTable_" + std::to_string(level_1_next_id) + ".sst";
    std::string lv1_path_i = "../data/level_1/index_" + std::to_string(level_1_next_id) + ".idx";

    if (std::filesystem::exists(lv1_path_s)) return false; // ID 冲突，不要覆盖
    if (std::filesystem::exists(lv1_path_i)) return false; // ID 冲突，不要覆盖

    std::ofstream lv1_write_i (lv1_path_i, std::ios::trunc | std::ios::binary);
    std::ofstream lv1_write_s (lv1_path_s, std::ios::trunc | std::ios::binary);//存在清空不存在创建 | 二进制打开
    if (!lv1_write_s || !lv1_write_i) return false;

    // 用 map 收集所有 key 的最新版本 map 天然去重
    // 从旧到新遍历 level_0（id 小的是旧的，后遍历的覆盖先遍历的）
    std::map<std::string, std::pair<uint8_t, std::string>> merged;

    for (uint64_t id = 0; id < level_0_next_id; ++id)
    {

        std::string lv0_path_s = "../data/level_0/SSTable_" + std::to_string(id) + ".sst";

        std::ifstream lv0_read_s (lv0_path_s, std::ios::binary);
        if (!lv0_read_s) continue;;

        while (1)
        {
            uint8_t deleted;
            if (!lv0_read_s.read(reinterpret_cast<char*> (&deleted), sizeof(deleted))) break;
            
            uint32_t key_len;
            if (!lv0_read_s.read(reinterpret_cast<char*> (&key_len), sizeof(key_len))) break;
            std::string key;
            key.resize(key_len);
            if (!lv0_read_s.read(reinterpret_cast<char*> (key.data()), key_len)) break;
            
            uint32_t value_len;
            if (!lv0_read_s.read(reinterpret_cast<char*> (&value_len), sizeof(value_len))) break;
            std::string value;
            value.resize(value_len);
            if (!lv0_read_s.read(reinterpret_cast<char*> (value.data()), value_len)) break;

            merged[key] = {deleted, value};
        }
    }

    // 写入level_1新文件
    for (const auto& kv : merged)
    {
        const std::string& key = kv.first;
        uint8_t deleted = kv.second.first;
        const std::string& value = kv.second.second;

        uint32_t key_len = static_cast<uint32_t>(key.size());
        uint32_t value_len = static_cast<uint32_t>(value.size());
        uint64_t offset = lv1_write_s.tellp();

        level1_index_[level_1_next_id][key] = offset;

        lv1_write_s.write(reinterpret_cast<const char*>(&deleted), sizeof(deleted));
        lv1_write_s.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        lv1_write_s.write(key.data(), key_len);
        lv1_write_s.write(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
        lv1_write_s.write(value.data(), value_len);

        lv1_write_i.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
        lv1_write_i.write(key.data(), key_len);
        lv1_write_i.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
    }

    lv1_write_s.close();
    lv1_write_i.close();

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
