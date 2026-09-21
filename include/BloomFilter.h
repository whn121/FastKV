#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include <cstdint>
#include <filesystem>
#include <fstream>


class BloomFilter
{
public:
    BloomFilter() = default; // 空构造（反序列化用）先来个空的再传入数据
    BloomFilter(std::size_t n, int bits_per_key = 10); //按key数分配

    void Add(const std::string& key);
    bool MayContain(const std::string& key) const;

    std::string Serialize() const;
    bool Deserialize(const std::string& data);

    bool valid() const { return num_bits_ > 0; }

private:
    std::vector<uint8_t> bits_; 
    size_t num_bits_ = 0; //容量
    int num_hashes_ = 0; //布隆k大小

    static uint32_t Hash(const std::string& key, uint32_t seed);
    size_t BitPos(uint32_t h1, uint32_t h2, int i) const;

};