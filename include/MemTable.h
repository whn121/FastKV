#pragma once
#include <string>
#include <map>

struct Entry
{
    std::string value;
    bool deleted;
};


class MemTable
{
public:

    bool Put(const std::string& key, const std::string& value);
    bool Get(const std::string& key, Entry& entry);
    bool Delete(const std::string& key);
    const std::map<std::string, Entry>& Data() const;

    void Clear();
    bool if_need_flush();

private:
    std::map<std::string, Entry> map_;

};