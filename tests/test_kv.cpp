#include "KVStore.h"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <chrono>

static int g_passed = 0;
static int g_failed = 0;

void CHECK(bool cond, const std::string& name) {
    if (cond) { g_passed++; std::cout << "  [PASS] " << name << std::endl; }
    else      { g_failed++; std::cout << "  [FAIL] " << name << std::endl; }
}

void cleanup() {
    std::filesystem::remove_all("../data");
    std::filesystem::remove_all("../Logger_txt");
}

void test_basic_put_get() {
    std::cout << "\n[测试 1] 基础 PUT/GET" << std::endl;
    cleanup();
    KVStore db;
    std::string v;

    CHECK(db.Put("key1", "value1"), "PUT key1");
    CHECK(db.Get("key1", v) && v == "value1", "GET key1");
    CHECK(db.Put("key2", "value2"), "PUT key2");
    CHECK(db.Get("key2", v) && v == "value2", "GET key2");
}

void test_overwrite() {
    std::cout << "\n[测试 2] 覆盖写" << std::endl;
    cleanup();
    KVStore db;
    std::string v;

    db.Put("A", "1");
    db.Put("A", "2");
    db.Put("A", "3");
    CHECK(db.Get("A", v) && v == "3", "GET A 返回最新值");
}

void test_delete() {
    std::cout << "\n[测试 3] DELETE / Tombstone" << std::endl;
    cleanup();
    KVStore db;
    std::string v;

    db.Put("B", "hello");
    CHECK(db.Get("B", v) && v == "hello", "DELETE 前 GET B");

    db.Delete("B");
    CHECK(!db.Get("B", v), "DELETE 后 GET B 返回失败");

    db.Put("B", "world");
    CHECK(db.Get("B", v) && v == "world", "重新 PUT B 后 GET 成功");
}

void test_not_found() {
    std::cout << "\n[测试 4] 不存在的 key" << std::endl;
    cleanup();
    KVStore db;
    std::string v;
    CHECK(!db.Get("nonexistent", v), "GET 不存在的 key");
}

void test_empty_value() {
    std::cout << "\n[测试 5] 空 value" << std::endl;
    cleanup();
    KVStore db;
    std::string v;

    db.Put("empty", "");
    CHECK(db.Get("empty", v) && v == "", "空 value");
}

void test_bulk_write() {
    std::cout << "\n[测试 6] 大量 key 触发 flush" << std::endl;
    cleanup();
    KVStore db;
    std::string v;

    const int N = 50000;
    for (int i = 0; i < N; ++i) {
        db.Put("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    CHECK(db.Get("key_0", v) && v == "value_0", "GET key_0");
    CHECK(db.Get("key_25000", v) && v == "value_25000", "GET key_25000");
    CHECK(db.Get("key_49999", v) && v == "value_49999", "GET key_49999");
    CHECK(!db.Get("key_50000", v), "GET key_50000 不存在");
}

void test_wal_replay() {
    std::cout << "\n[测试 7] WAL Replay 恢复" << std::endl;
    cleanup();

    {
        KVStore db;
        db.Put("persist1", "100");
        db.Put("persist2", "200");
        db.Put("persist3", "300");
    }

    {
        KVStore db;
        std::string v;
        CHECK(db.Get("persist1", v) && v == "100", "Replay persist1");
        CHECK(db.Get("persist2", v) && v == "200", "Replay persist2");
        CHECK(db.Get("persist3", v) && v == "300", "Replay persist3");
    }
}

void test_tombstone_across_sstable() {
    std::cout << "\n[测试 8] Tombstone 跨 SSTable" << std::endl;
    cleanup();
    KVStore db;
    std::string v;

    for (int i = 0; i < 30000; ++i) {
        db.Put("key_" + std::to_string(i), "v1_" + std::to_string(i));
    }

    db.Delete("key_100");
    db.Delete("key_5000");
    db.Delete("key_29999");

    for (int i = 0; i < 30000; ++i) {
        db.Put("another_" + std::to_string(i), "v2_" + std::to_string(i));
    }

    CHECK(!db.Get("key_100", v), "跨 SSTable 删除 key_100");
    CHECK(!db.Get("key_5000", v), "跨 SSTable 删除 key_5000");
    CHECK(!db.Get("key_29999", v), "跨 SSTable 删除 key_29999");
    CHECK(db.Get("key_200", v) && v == "v1_200", "未删除的 key_200 仍在");
}

void test_stress() {
    std::cout << "\n[测试 9] 压力测试" << std::endl;
    cleanup();
    KVStore db;
    std::string v;

    const int N = 100000;
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < N; ++i) {
        db.Put("k" + std::to_string(i), "v" + std::to_string(i));
    }

    auto mid = std::chrono::steady_clock::now();

    int found = 0;
    for (int i = 0; i < N; ++i) {
        if (db.Get("k" + std::to_string(i), v)) found++;
    }

    auto end = std::chrono::steady_clock::now();

    double put_ms = std::chrono::duration<double, std::milli>(mid - start).count();
    double get_ms = std::chrono::duration<double, std::milli>(end - mid).count();

    std::cout << "  PUT " << N << " 条耗时 " << put_ms << " ms ("
              << (N / put_ms * 1000) << " ops/s)" << std::endl;
    std::cout << "  GET " << N << " 条耗时 " << get_ms << " ms ("
              << (N / get_ms * 1000) << " ops/s)" << std::endl;

    CHECK(found == N, "所有 key 都能查到");
}

void test_bloom() {
    std::cout << "\n[测试 10] Bloom Filter 集成" << std::endl;
    cleanup();
    KVStore db;
    std::string v;

    for (int i = 0; i < 50000; ++i) {
        db.Put("key_" + std::to_string(i), "value_" + std::to_string(i));
    }

    CHECK(db.Get("key_100", v) && v == "value_100", "存在的 key");
    CHECK(db.Get("key_49999", v) && v == "value_49999", "存在的 key 2");

    int miss_count = 0;
    for (int i = 50000; i < 50100; ++i) {
        if (!db.Get("key_" + std::to_string(i), v)) miss_count++;
    }
    CHECK(miss_count == 100, "不存在的 key 全部返回 NotFound");
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "FastKV 完整测试" << std::endl;
    std::cout << "========================================" << std::endl;

    test_basic_put_get();
    test_overwrite();
    test_delete();
    test_not_found();
    test_empty_value();
    test_bulk_write();
    test_wal_replay();
    test_tombstone_across_sstable();
    test_stress();
    test_bloom();

    std::cout << "\n========================================" << std::endl;
    std::cout << "结果: " << g_passed << " passed, "
              << g_failed << " failed" << std::endl;
    std::cout << "========================================" << std::endl;

    return g_failed == 0 ? 0 : 1;
}