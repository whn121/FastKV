#include "KVStore.h"
#include <iostream>

int main() {
    std::cout << "===== FastKV Demo =====" << std::endl;

    KVStore db;

    // 写入
    std::cout << "Put key1=value1" << std::endl;
    db.Put("key1", "value1");

    // 读取
    std::string v;
    if (db.Get("key1", v)) {
        std::cout << "Get key1 -> " << v << std::endl;
    } else {
        std::cout << "Get key1 -> NOT FOUND" << std::endl;
    }

    // 覆盖
    db.Put("key1", "value2");
    if (db.Get("key1", v)) {
        std::cout << "After overwrite: " << v << std::endl;
    }

    // 删除
    db.Delete("key1");
    if (!db.Get("key1", v)) {
        std::cout << "After delete: NOT FOUND" << std::endl;
    }

    std::cout << "===== Done =====" << std::endl;
    return 0;
}