#pragma once

#include <iostream>
#include <string>
#include <cstdint>
#include <fstream>
#include <cstring>
#include <functional>
#include <filesystem>

#include <fcntl.h>      // open 数据先进入内存,在进入磁盘
#include <unistd.h>     // write, close, fsync 使用fsync把因为中断停在内存里的数据,写入磁盘


enum class Type : uint32_t
{
    Put,
    Delete
};

class WAL
{
public:
    WAL(const std::string& path);
    ~WAL();

    bool Append(Type& type, std::string& key, std::string& value);
    bool Replay(std::function<void(Type&, std::string&, std::string&)>);

    bool Clear(); //防止一直累积,先全部删除

private:
    int fd_ = -1;
    std::string path_;

};