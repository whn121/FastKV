// #include "KVStore.h"
// #include <iostream>
// #include <cassert>
// #include <filesystem>

// static int g_passed = 0;
// static int g_failed = 0;

// void CHECK(bool cond, const std::string& name) {
//     if (cond) { g_passed++; std::cout << "  [PASS] " << name << std::endl; }
//     else      { g_failed++; std::cout << "  [FAIL] " << name << std::endl; }
// }

// void cleanup() {
//     std::filesystem::remove_all("../data");
//     std::filesystem::remove_all("../Logger_txt");
// }

// void print_dirs() {
//     int lv0 = 0, lv1 = 0;
//     for (auto& e : std::filesystem::directory_iterator("../data/level_0"))
//         if (e.path().extension() == ".sst") lv0++;
//     for (auto& e : std::filesystem::directory_iterator("../data/level_1"))
//         if (e.path().extension() == ".sst") lv1++;
//     std::cout << "  level_0 SSTable 数: " << lv0 << std::endl;
//     std::cout << "  level_1 SSTable 数: " << lv1 << std::endl;
// }

// int main() {
//     std::cout << "===== Compaction 正确性测试 =====" << std::endl;
//     cleanup();

//     KVStore db;
//     std::string v;

//     // 1. 写 200000 条触发多次 flush + compaction
//     std::cout << "\n[阶段 1] 写 200000 条 key" << std::endl;
//     for (int i = 0; i < 200000; ++i) {
//         db.Put("key_" + std::to_string(i), "v1_" + std::to_string(i));
//     }
//     std::cout << "  写入完成" << std::endl;
//     print_dirs();

//     // 2. 覆盖前 50000 条
//     std::cout << "\n[阶段 2] 覆盖前 50000 条" << std::endl;
//     for (int i = 0; i < 50000; ++i) {
//         db.Put("key_" + std::to_string(i), "v2_" + std::to_string(i));
//     }
//     std::cout << "  覆盖完成" << std::endl;
//     print_dirs();

//     // 3. 验证覆盖的 key 返回新值
//     std::cout << "\n[阶段 3] 验证" << std::endl;
//     CHECK(db.Get("key_100", v) && v == "v2_100", "被覆盖的 key_100 返回 v2");
//     CHECK(db.Get("key_49999", v) && v == "v2_49999", "被覆盖的 key_49999 返回 v2");
//     CHECK(db.Get("key_50000", v) && v == "v1_50000", "未覆盖的 key_50000 返回 v1");
//     CHECK(db.Get("key_199999", v) && v == "v1_199999", "最后的 key_199999 返回 v1");

//     // 4. 删除一些 key，再验证
//     std::cout << "\n[阶段 4] 删除 + 验证" << std::endl;
//     for (int i = 0; i < 1000; ++i) {
//         db.Delete("key_" + std::to_string(i));
//     }
//     for (int i = 0; i < 1000; ++i) {
//         CHECK(!db.Get("key_" + std::to_string(i), v), "删除的 key_" + std::to_string(i) + " 返回 NotFound");
//         if (i > 10) break;   // 只打前 10 个，避免刷屏
//     }
//     // 再抽查几个
//     CHECK(!db.Get("key_999", v), "key_999 已删除");
//     CHECK(db.Get("key_1000", v) && v == "v2_1000", "key_1000 未被删除");

//     // 5. 重启后验证（关键：验证 level_1 索引正确加载）
//     std::cout << "\n[阶段 5] 重启 + 验证 level_1 数据" << std::endl;
//     // db 析构
//     {
//         KVStore db2;
//         CHECK(db2.Get("key_50000", v) && v == "v1_50000", "重启后 key_50000 仍在");
//         CHECK(db2.Get("key_1000", v) && v == "v2_1000", "重启后 key_1000 返回 v2");
//         CHECK(!db2.Get("key_999", v), "重启后 key_999 仍被删除");
//     }

//     std::cout << "\n===== 结果: " << g_passed << " passed, " << g_failed << " failed =====" << std::endl;
//     return g_failed == 0 ? 0 : 1;
// }