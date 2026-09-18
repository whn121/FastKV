#include "MemTable.h"


bool MemTable::Put(const std::string& key, const std::string& value)
{
    Entry entry {value, false};
    map_[key] = entry; //定义put为创建或更新
    return true;
}

bool MemTable::Get(const std::string& key, Entry& entry)
{
    auto it = map_.find (key);
    if (it != map_.end())
    {
        entry.deleted = it->second.deleted;
        entry.value = it->second.value;
        return true;
    }
    return false;
}

bool MemTable::Delete(const std::string &key)
{
    Entry entry;
    entry.value = "";
    entry.deleted = true;

    map_[key] = entry;

    return true;
}

const std::map<std::string, Entry> &MemTable::Data() const
{
    return  map_;
}

void MemTable::Clear()
{
    map_.clear();
}

bool MemTable::if_need_flush()
{
    if (map_.size() > 1024*20) return true;
    return false;
}
