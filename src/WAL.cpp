#include "WAL.h"

//4字节type + 4字节len + 4字节len + 内容

WAL::WAL(const std::string& path) : path_(path)
{
    // 从路径里提取目录部分，不存在就创建 //成员初始化在构造函数之前,kv先调用这个wal_构造,在调用kv构造
    // 但是kv构造找文件不存在所以在这个执行
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos) {
        std::string dir = path.substr(0, pos);
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        // 如果失败，ec 会有错误码，继续尝试 open（可能目录已存在）
    }

    // O_APPEND: 每次写追加到末尾
    // O_CREAT:  文件不存在则创建
    // O_WRONLY: 只写
    fd_ = ::open(path_.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd_ < 0) std::cerr << "WAL open failed: " << strerror(errno) << std::endl;
}

WAL::~WAL()
{
    if (fd_ >= 0)
    {
        ::fsync(fd_);
        ::close(fd_);
    }
}

bool WAL::Append(Type& type, std::string& key, std::string& value)
{
    if (fd_ < 0) return false;

    uint32_t key_len = key.size();
    uint32_t value_len = value.size();

    // 用一个 buffer 一次性组装，减少 write 次数
    std::string buf;
    buf.reserve (sizeof(type) + sizeof(key_len) + sizeof(value_len) + key_len + value_len);
    buf.append(reinterpret_cast<const char*>(&type), sizeof(type));
    buf.append(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
    buf.append(key);
    buf.append(reinterpret_cast<const char*>(&value_len), sizeof(value_len));
    buf.append(value);

    //单次写
    ssize_t written = ::write(fd_, buf.data(), buf.size());
    if (written != static_cast<ssize_t>(buf.size()))
    {
        std::cerr << "WAL write failed: " << strerror(errno) << std::endl;
        return false;
    }

    // 关键：fsync 保证数据落盘
    if (::fsync(fd_) != 0) {
        std::cerr << "WAL fsync failed: " << strerror(errno) << std::endl;
        return false;
    }

    return true;

}

bool WAL::Replay(std::function<void(Type&, std::string&, std::string&)> callback_use_memtable)
{
    std::ifstream read (path_, std::ios::binary);
    if (!read) return false;

    while(1)
    {
        Type type;
        if (!read.read(reinterpret_cast<char*> (&type), sizeof (type)))
        {
            break;
        }

        uint32_t key_len;
        if (!read.read(reinterpret_cast<char*> (&key_len), sizeof (key_len)))
        {
            return false;
        }

        std::string key;
        key.resize(key_len);
        if (!read.read(reinterpret_cast<char*> (key.data()), key_len))
        {
            return false;
        }

        uint32_t value_len;
        if (!read.read(reinterpret_cast<char*> (&value_len), sizeof (value_len)))
        {
            return false;
        }

        std::string value;
        value.resize(value_len);
        if (!read.read(reinterpret_cast<char*> (value.data()), value_len))
        {
            return false;
        }

        if (callback_use_memtable) callback_use_memtable (type, key, value);
    }

    return true;
}

bool WAL::Clear()
{
    // 用 truncate 清空文件，但保持 fd
    if (ftruncate(fd_, 0) != 0) return false;
    // 因为 O_APPEND，下次写会自动回到末尾
    return true;
}
