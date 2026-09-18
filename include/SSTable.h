#pragma once

#include "MemTable.h"
#include <fstream>
#include <string>
#include <cstring>
#include <cstdint>


class SSTable
{
public:
    bool Write (const MemTable& memtable, uint64_t* next_id);

    bool Get (const std::string& key, std::string& value, uint64_t lv0_startid, uint64_t lv1_startid);

    bool SubIndex(uint64_t id, std::string& key, uint64_t offset, int level);

    bool Compact(uint64_t& level_0_next_id, uint64_t& level_1_next_id);

    void RemoveIndex(uint64_t id, int level);

private:
    std::map<uint64_t, std::map<std::string, uint64_t>> level0_index_;
    std::map<uint64_t, std::map<std::string, uint64_t>> level1_index_;
};