#include "BloomFilter.h"
#include <cstring>



uint32_t BloomFilter::Hash(const std::string& key, uint32_t seed)
{
    uint32_t h = seed;
    for (char c : key)
    {
        h ^= static_cast<uint8_t>(c);
        h *= 16777619;
    }
    return h;
}

size_t BloomFilter::BitPos(uint32_t h1, uint32_t h2, int i) const
{
    return (h1 + static_cast<uint32_t>(i) * h2) % num_bits_;
}

BloomFilter::BloomFilter(std::size_t n, int bits_per_key)
{
    if (n == 0) return;

    num_bits_ = n * bits_per_key;
    if (num_bits_ < 64) num_bits_ = 64;

    num_hashes_ = static_cast<int>(bits_per_key * 0.69);
    if (num_hashes_ < 1) num_hashes_ = 1;
    if (num_hashes_ > 30) num_hashes_ = 30;

    bits_.resize((num_bits_ + 7) / 8, 0);

}

void BloomFilter::Add(const std::string &key)
{
    if (num_bits_ == 0) return;

    //两个种子来自levelDB
    uint32_t h1 = Hash(key, 0xBC9F1D34);
    uint32_t h2 = Hash(key, 0x811C9DC5);

    for (int i = 0; i < num_hashes_; ++i)
    {
        size_t pos = BitPos(h1, h2, i);
        bits_[pos / 8] |= (1 << (pos % 8));
    }

}

bool BloomFilter::MayContain(const std::string &key) const
{
    if (num_bits_ == 0) return false;

    //两个种子来自levelDB
    uint32_t h1 = Hash(key, 0xBC9F1D34);
    uint32_t h2 = Hash(key, 0x811C9DC5);

    for (int i = 0; i < num_hashes_; ++i)
    {
        size_t pos = BitPos(h1, h2, i);
        if (!(bits_[pos / 8] & (1 << (pos %8))))
        {
            return false;
        }
    }
    return true;
}

std::string BloomFilter::Serialize() const
{
    std::string result;

    // 1. 写 num_bits（4 字节）
    uint32_t nb = static_cast<uint32_t>(num_bits_);
    result.append(reinterpret_cast<const char*>(&nb), sizeof(nb));

    // 2. 写 num_hashes（1 字节）
    result.push_back(static_cast<char>(num_hashes_));

    // 3. 写 bits_（N 字节）
    result.append(reinterpret_cast<const char*>(bits_.data()), bits_.size());

    return result;   
}

bool BloomFilter::Deserialize(const std::string &data)
{
    // 至少要有 4 + 1 字节的头
    if (data.size() < 5) return false;

    // 1. 读 num_bits
    uint32_t nb;
    std::memcpy(&nb, data.data(), 4);
    num_bits_ = nb;

    // 2. 读 num_hashes
    num_hashes_ = static_cast<uint8_t>(data[4]);

    // 3. 读 bits_
    size_t expected_bytes = (num_bits_ + 7) / 8;
    if (data.size() < 5 + expected_bytes) return false;

    bits_.resize(expected_bytes);
    std::memcpy(bits_.data(), data.data() + 5, expected_bytes);

    return true;
}
