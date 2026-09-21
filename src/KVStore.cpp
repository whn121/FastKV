#include "KVStore.h"
#include <filesystem>



KVStore::KVStore() : wal_("../Logger_txt/WAL_txt")
{
    // 1. 确保目录存在
    std::filesystem::create_directories("../data/level_0");
    std::filesystem::create_directories("../data/level_1");

    // 2. 清理残留的 .tmp 文件
    CleanupTmpFiles("../data/level_0");
    CleanupTmpFiles("../data/level_1");

    // 3. 加载 level_0
    auto lv0_max = LoadAllSSTables("../data/level_0", 0);
    level_0_next_id_ = lv0_max.has_value() ? (lv0_max.value() + 1) : 0;

    // 4. 加载 level_1
    auto lv1_max = LoadAllSSTables("../data/level_1", 1);
    level_1_next_id_ = lv1_max.has_value() ? (lv1_max.value() + 1) : 0;

    // 5. 恢复 WAL（把未落盘的写入重放到 MemTable）
    wal_.Replay(
        [this](Type& type, std::string& key, std::string& value) {
            wal_use_memtable_callback(type, key, value);
        }
    );
}

bool KVStore::Put(const std::string& key, const std::string& value)
{
    Type type = Type::Put; 

    std::string key_v = key;
    std::string value_v = value;

    if (!wal_.Append(type, key_v, value_v)) return false;

    if (!memtable_.Put(key, value)) return false;

    if (memtable_.if_need_flush()) Flush();
    
    return true;
}

bool KVStore::Get(const std::string &key, std::string& value)
{
    Entry entry;
    if (memtable_.Get(key, entry))
    {
        if (!entry.deleted)
        {
            value =entry.value;
            return true;
        }
        else return false;
    }
    else
    {
        uint64_t lv1_max_id = 0;
        uint64_t lv0_max_id = 0;
        if (level_0_next_id_ != 0) lv0_max_id = level_0_next_id_ - 1;
        if (level_1_next_id_ != 0) lv1_max_id = level_1_next_id_ - 1;
        if (sstable_.Get(key, value, lv0_max_id, lv1_max_id)) return true;
    }
    return false;
}

bool KVStore::Delete(const std::string &key)
{
    Type type = Type::Delete; 
    std::string key_v = key;
    std::string value_v = "";
    if (!wal_.Append(type, key_v, value_v)) return false;

    if (!memtable_.Delete(key)) return false;
    
    if (memtable_.if_need_flush()) Flush();

    return true;
}

void KVStore::wal_use_memtable_callback(Type &type, std::string &key, std::string &value)
{
    if (type == Type::Put)
    {
        memtable_.Put (key, value);
    }
    else if (type == Type::Delete)
    {
        memtable_.Delete (key);
    }
    else
    {
        return;
    }
}

void KVStore::Flush()
{
    if (memtable_.Data().empty()) return;

    if (sstable_.Write(memtable_, &level_0_next_id_))
    {
        wal_.Clear();
        memtable_.Clear();
        level_0_next_id_ ++;
    }
    
    if (level_0_next_id_ >= 4)
    {
        if (sstable_.Compact(level_0_next_id_, level_1_next_id_))
        {
            // 删掉 level_0 里的旧文件，但保留目录
            for (uint64_t id = 0; id < level_0_next_id_; ++id) 
            {
                std::string path = "../data/level_0/SSTable_" + std::to_string(id) + ".sst";
                std::error_code ec;
                std::filesystem::remove(path, ec);

                sstable_.RemoveIndex(id, 0);
            }
            level_0_next_id_ = 0;   // 重置计数器
        }
    }

}

std::optional<uint64_t> KVStore::LoadAllSSTables(const std::string& dir, int level)
{
    std::optional<uint64_t> max_id;

    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        auto id = ParseSSTableId(entry.path().filename().string());
        if (!id.has_value()) continue;

        // 加载这个 SSTable（读 Footer + Index + Bloom 到内存）
        if (!sstable_.LoadFile(id.value(), level, entry.path().string())) {
            std::cerr << "Failed to load " << entry.path().string() << std::endl;
            continue;
        }

        // 更新 max_id
        if (!max_id.has_value() || id.value() > max_id.value()) {
            max_id = id.value();
        }
    }

    return max_id;
}

void KVStore::CleanupTmpFiles(const std::string& dir)
{
    for (const auto& entry : std::filesystem::directory_iterator(dir))
    {
        if (entry.path().extension() == ".tmp") {
            std::error_code ec;
            std::filesystem::remove(entry.path(), ec);
        }
    }
}

std::optional<uint64_t> KVStore::ParseSSTableId(const std::string& filename)
{
    // 格式：SSTable_<id>.sst
    // 最小长度：SSTable_0.sst = 13 字符
    if (filename.size() < 13) return std::nullopt;
    if (filename.substr(0, 8) != "SSTable_") return std::nullopt;
    if (filename.substr(filename.size() - 4) != ".sst") return std::nullopt;

    // 提取 id 部分
    // filename = "SSTable_0.sst"
    // substr(8, size-12) = substr(8, 1) = "0"
    std::string id_str = filename.substr(8, filename.size() - 12);
    if (id_str.empty()) return std::nullopt;

    try {
        return std::stoull(id_str);
    } catch (...) {
        return std::nullopt;
    }
}