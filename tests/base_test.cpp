
#include "KVStore.h"

#include <cassert>
#include <iostream>
#include <string>

void TestPutAndGet(KVStore& kv)
{
    std::cout << "[1] Put -> Get\n";

    assert(kv.Put("name", "Tom"));

    std::string value;
    assert(kv.Get("name", value));
    assert(value == "Tom");

    std::cout << "    PASS\n";
}

void TestUpdate(KVStore& kv)
{
    std::cout << "[2] Update -> Get\n";

    assert(kv.Put("name", "Jack"));

    std::string value;
    assert(kv.Get("name", value));
    assert(value == "Jack");

    std::cout << "    PASS\n";
}

void TestDelete(KVStore& kv)
{
    std::cout << "[3] Delete -> Get\n";

    assert(kv.Put("delete_key", "hello"));
    assert(kv.Delete("delete_key"));

    std::string value;
    assert(!kv.Get("delete_key", value));

    std::cout << "    PASS\n";
}

void TestFlush(KVStore& kv)
{
    std::cout << "[4] Flush -> Get\n";

    assert(kv.Put("flush_key", "hello"));
    kv.Flush();

    std::string value;
    assert(kv.Get("flush_key", value));
    assert(value == "hello");

    std::cout << "    PASS\n";
}

void TestMultipleFlush(KVStore& kv)
{
    std::cout << "[5] Multiple Flush -> Multiple SSTable -> Get\n";

    assert(kv.Put("key1", "value1"));
    kv.Flush();

    assert(kv.Put("key2", "value2"));
    kv.Flush();

    assert(kv.Put("key3", "value3"));
    kv.Flush();

    std::string value;

    assert(kv.Get("key1", value));
    assert(value == "value1");

    assert(kv.Get("key2", value));
    assert(value == "value2");

    assert(kv.Get("key3", value));
    assert(value == "value3");

    std::cout << "    PASS\n";
}

void TestTombstone(KVStore& kv)
{
    std::cout << "[6] Old value + New tombstone -> Get should fail\n";

    // 先写入旧 SSTable
    assert(kv.Put("tombstone_key", "old_value"));
    kv.Flush();

    // 再删除，形成新的 tombstone
    assert(kv.Delete("tombstone_key"));
    kv.Flush();

    std::string value;

    // 新 SSTable 中存在 tombstone，
    // 所以不能继续去旧 SSTable 找 old_value
    assert(!kv.Get("tombstone_key", value));

    std::cout << "    PASS\n";
}

int main()
{
    std::cout << "========== FastKV Test ==========\n\n";

    KVStore kv;

    TestPutAndGet(kv);
    TestUpdate(kv);
    TestDelete(kv);
    TestFlush(kv);
    TestMultipleFlush(kv);
    TestTombstone(kv);

    std::cout << "\n========== ALL TESTS PASSED ==========\n";

    return 0;
}
