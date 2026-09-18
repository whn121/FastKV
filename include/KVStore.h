#pragma once 
#include <string>
#include "MemTable.h"
#include "WAL.h"
#include "SSTable.h"


class KVStore
{
public:
    KVStore();

    bool Put(const std::string& key, const std::string& value);
    bool Get(const std::string& key, std::string& value);
    bool Delete(const std::string& key);

    void wal_use_memtable_callback(Type& type, std::string& key, std::string& value);

    void Flush(); //memtable满了调用这个,把数据永久化

private:
    MemTable memtable_;
    WAL wal_;
    SSTable sstable_;

    uint64_t level_0_next_id_;
    uint64_t level_1_next_id_;

    void LoadIndex(uint64_t max_id, std::string& level_dir, int level);
};