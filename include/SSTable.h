#pragma once

#include "MemTable.h"
#include <fstream>
#include <string>
#include <cstring>
#include <cstdint>
#include "BloomFilter.h"
#include <unordered_map>


struct Footer {
    uint64_t data_offset;
    uint64_t data_size;
    uint64_t index_offset;
    uint64_t index_size;
    uint64_t bloom_offset;
    uint64_t bloom_size;
    uint32_t magic;   // 魔数，验证文件有效性
};
// 大小: 8*6 + 4 = 52 字节

    static constexpr uint32_t FOOTER_MAGIC = 0xFA57CAFE;    // 魔数验证foooter是否损坏
    static constexpr size_t FOOTER_SIZE = sizeof(Footer);   // 52 字节


class SSTable
{
public:
    bool Write (const MemTable& memtable, uint64_t* next_id);

    bool Get (const std::string& key, std::string& value, uint64_t lv0_startid, uint64_t lv1_startid);

    bool Compact(uint64_t& level_0_next_id, uint64_t& level_1_next_id);

    void RemoveIndex(uint64_t id, int level);

    bool LoadFile(uint64_t id, int level, const std::string& path);

    bool BloomMayContain(uint64_t id, int level, const std::string& key) const;

    bool ReadEntry(std::ifstream& f, std::string& value);

private:
    //表id->[key->相对偏移量]
    std::map<uint64_t, std::map<std::string, uint64_t>> level0_index_;
    std::map<uint64_t, std::map<std::string, uint64_t>> level1_index_;

    //表id->布隆
    std::map<uint64_t, BloomFilter> level0_bloom_;
    std::map<uint64_t, BloomFilter> level1_bloom_;

    //每个 SSTable 文件的 data section 起始偏移 当前意义不大就在开头,方便未来加时间戳
    std::unordered_map<uint64_t, uint64_t> level0_data_offset_;
    std::unordered_map<uint64_t, uint64_t> level1_data_offset_;
};