#include "BloomFilter.h"
#include <iostream>
#include <cassert>

static int g_passed = 0;
static int g_failed = 0;

void CHECK(bool cond, const std::string& name) {
    if (cond) { g_passed++; std::cout << "  [PASS] " << name << std::endl; }
    else      { g_failed++; std::cout << "  [FAIL] " << name << std::endl; }
}

int main() {
    std::cout << "===== Bloom Filter 测试 =====" << std::endl;

    // ───── 测试 1：无 False Negative ─────
    std::cout << "\n[测试 1] 无 False Negative" << std::endl;
    BloomFilter bf(10000, 10);
    for (int i = 0; i < 10000; ++i) {
        bf.Add("key_" + std::to_string(i));
    }

    int fn = 0;
    for (int i = 0; i < 10000; ++i) {
        if (!bf.MayContain("key_" + std::to_string(i))) fn++;
    }
    CHECK(fn == 0, "已插入的 key 全部返回 true");

    // ───── 测试 2：FP 率 ─────
    std::cout << "\n[测试 2] FP 率" << std::endl;
    int fp = 0;
    int total = 100000;
    for (int i = 10000; i < 10000 + total; ++i) {
        if (bf.MayContain("key_" + std::to_string(i))) fp++;
    }
    double fp_rate = (double)fp / total;
    std::cout << "  FP rate: " << (fp_rate * 100) << "% (expected ~1%)" << std::endl;
    CHECK(fp_rate < 0.05, "FP 率 < 5%");

    // ───── 测试 3：序列化 / 反序列化 ─────
    std::cout << "\n[测试 3] 序列化/反序列化" << std::endl;
    std::string data = bf.Serialize();
    std::cout << "  序列化大小: " << data.size() << " 字节" << std::endl;

    BloomFilter bf2;
    CHECK(bf2.Deserialize(data), "反序列化成功");

    int mismatch = 0;
    for (int i = 0; i < 10000; ++i) {
        if (!bf2.MayContain("key_" + std::to_string(i))) mismatch++;
    }
    CHECK(mismatch == 0, "反序列化后无 False Negative");

    // ───── 测试 4：小数据量 ─────
    std::cout << "\n[测试 4] 小数据量" << std::endl;
    BloomFilter bf3(10, 10);
    bf3.Add("a");
    bf3.Add("b");
    bf3.Add("c");
    CHECK(bf3.MayContain("a"), "小 bloom 查 a");
    CHECK(bf3.MayContain("b"), "小 bloom 查 b");
    CHECK(bf3.MayContain("c"), "小 bloom 查 c");

    // ───── 测试 5：空 Bloom ─────
    std::cout << "\n[测试 5] 空 Bloom" << std::endl;
    BloomFilter bf4;
    CHECK(!bf4.MayContain("anything"), "空 bloom 返回 false");
    CHECK(bf4.Serialize().size() == 0 || bf4.Serialize().size() == 5,
          "空 bloom 序列化不崩溃");

    // ───── 测试 6：不同参数 ─────
    std::cout << "\n[测试 6] 不同 bits_per_key" << std::endl;
    const int N = 10000;  
    for (int bpk : {4, 8, 10, 16}) {
        BloomFilter b(N, bpk);
        for (int i = 0; i < N; ++i) b.Add("k" + std::to_string(i));
        int fp2 = 0;
        for (int i = N; i < N + 10000; ++i) {
            if (b.MayContain("k" + std::to_string(i))) fp2++;
        }
        std::cout << "  bits_per_key=" << bpk
                  << ", FP rate=" << (fp2 * 100.0 / 10000) << "%" << std::endl;
    }

    std::cout << "\n===== 结果: " << g_passed << " passed, "
              << g_failed << " failed =====" << std::endl;
    return g_failed == 0 ? 0 : 1;
}