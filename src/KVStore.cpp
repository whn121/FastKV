#include "KVStore.h"
#include <filesystem>



KVStore::KVStore() : wal_("../Logger_txt/WAL_txt") 
{
    std::filesystem::create_directories("../data/level_0");
    std::filesystem::create_directories("../data/level_1");

    uint64_t level_0_max_id = 0;
    uint64_t level_1_max_id = 0;

    bool level_0_has_sstable = false; //解决文件为空,从"1"开始的问题
    bool level_1_has_sstable = false; //解决文件为空,从"1"开始的问题

    for (const auto& entry : std::filesystem::directory_iterator("../data/level_0"))
    {
        std::string filename = entry.path().filename().string();
        
        if (filename.rfind("SSTable_", 0) != 0)
            continue;

        if (filename.size() <= 12)
            continue;

        std::string id_str = filename.substr(8);
        id_str = id_str.substr(0, id_str.size() - 4);

        uint64_t id = std::stoull(id_str);

        if(!level_0_has_sstable || id > level_0_max_id)
        {
            level_0_max_id = id;
            level_0_has_sstable = true;
        }
    }

    if (level_0_has_sstable) 
    {
        std::string path = "../data/level_0";
        LoadIndex(level_0_max_id, path, 0); //放到内存里
        level_0_next_id_ = level_0_max_id + 1;
    }
    else level_0_next_id_ = 0;

    for (const auto& entry : std::filesystem::directory_iterator("../data/level_1"))
    {
        std::string filename = entry.path().filename().string();
        
        if (filename.rfind("SSTable_", 0) != 0)
            continue;

        if (filename.size() <= 12)
            continue;

        std::string id_str = filename.substr(8);
        id_str = id_str.substr(0, id_str.size() - 4);

        uint64_t id = std::stoull(id_str);

        if(!level_1_has_sstable || id > level_1_max_id)
        {
            level_1_max_id = id;
            level_1_has_sstable = true;
        }
    }

    if (level_1_has_sstable) 
    {
        std::string path = "../data/level_1";
        LoadIndex(level_1_max_id, path, 1); //放到内存里
        level_1_next_id_ = level_1_max_id + 1;
    }
    else level_1_next_id_ = 0;

    // 恢复 WAL
    wal_.Replay
    (
        [this](Type& type, std::string& key, std::string& value)
        {
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
                std::filesystem::remove("../data/level_0/SSTable_" + std::to_string(id) + ".sst");
                std::filesystem::remove("../data/level_0/index_" + std::to_string(id) + ".idx");
                sstable_.RemoveIndex(id, 0);  
            }
            level_0_next_id_ = 0;   // 重置计数器
        }
    }

}
void KVStore::LoadIndex(uint64_t max_id, std::string& level_dir, int level)
{
    for (uint64_t id = 0; id <= max_id; ++id)
    {
        std::ifstream read (level_dir + "/index_" + std::to_string(id) + ".idx"
                            , std::ios::binary);
        
        if (!read) continue;

        while(1)
        {
            uint32_t key_len;
            if (!read.read(reinterpret_cast<char*> (&key_len), sizeof(key_len))) break;
            
            std::string key;
            key.resize(key_len);
            if (!read.read(reinterpret_cast<char*> (key.data()), key_len)) break;
            
            uint64_t offset;
            if (!read.read(reinterpret_cast<char*> (&offset), sizeof(offset))) break;

            sstable_.SubIndex(id, key, offset, level);
        }
    }
}
