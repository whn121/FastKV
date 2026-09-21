#include "KVStore.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <algorithm>
#include <filesystem>

using Clock = std::chrono::steady_clock;

std::string make_key(int i) {
    return "key_" + std::to_string(i);
}

std::string make_value(int i) {
    return "value_" + std::to_string(i) + "_" + std::string(50, 'x');
}

void cleanup() {
    std::filesystem::remove_all("../data");
    std::filesystem::remove_all("../Logger_txt");
}

struct Stats {
    double total_ms;
    double ops_per_sec;
    double p50_ms;
    double p95_ms;
    double p99_ms;
    double max_ms;
};

Stats compute_stats(const std::vector<double>& latencies, double total_ms) {
    if (latencies.empty()) return {0, 0, 0, 0, 0, 0};
    std::vector<double> sorted = latencies;
    std::sort(sorted.begin(), sorted.end());

    size_t n = sorted.size();
    Stats s;
    s.total_ms = total_ms;
    s.ops_per_sec = n / (total_ms / 1000.0);
    s.p50_ms = sorted[n * 50 / 100];
    s.p95_ms = sorted[n * 95 / 100];
    s.p99_ms = sorted[n * 99 / 100];
    s.max_ms = sorted.back();
    return s;
}

void print_stats(const std::string& name, const Stats& s, int N) {
    std::cout << "\n===== " << name << " =====" << std::endl;
    std::cout << "  Operations:  " << N << std::endl;
    std::cout << "  Total time:  " << s.total_ms << " ms" << std::endl;
    std::cout << "  Throughput:  " << (long long)s.ops_per_sec << " ops/s" << std::endl;
    std::cout << "  P50:         " << s.p50_ms << " ms" << std::endl;
    std::cout << "  P95:         " << s.p95_ms << " ms" << std::endl;
    std::cout << "  P99:         " << s.p99_ms << " ms" << std::endl;
    std::cout << "  Max:         " << s.max_ms << " ms" << std::endl;
}

// ============ 场景 ============

void bench_write_seq(int N) {
    cleanup();
    KVStore db;
    std::vector<double> lat;
    lat.reserve(N);

    auto start = Clock::now();
    for (int i = 0; i < N; ++i) {
        auto t0 = Clock::now();
        db.Put(make_key(i), make_value(i));
        auto t1 = Clock::now();
        lat.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    auto end = Clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    print_stats("write_seq", compute_stats(lat, ms), N);
}

void bench_read_hit(int N) {
    cleanup();
    KVStore db;
    std::cout << "  [准备] 写入 " << N << " 条..." << std::endl;
    for (int i = 0; i < N; ++i) db.Put(make_key(i), make_value(i));

    std::vector<double> lat;
    lat.reserve(N);
    std::string v;

    auto start = Clock::now();
    int found = 0;
    for (int i = 0; i < N; ++i) {
        auto t0 = Clock::now();
        if (db.Get(make_key(i), v)) found++;
        auto t1 = Clock::now();
        lat.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    auto end = Clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    print_stats("read_hit", compute_stats(lat, ms), N);
    std::cout << "  Found: " << found << " / " << N << std::endl;
}

void bench_read_miss(int N) {
    cleanup();
    KVStore db;
    std::cout << "  [准备] 写入 " << N << " 条..." << std::endl;
    for (int i = 0; i < N; ++i) db.Put(make_key(i), make_value(i));

    std::vector<double> lat;
    lat.reserve(N);
    std::string v;

    auto start = Clock::now();
    for (int i = 0; i < N; ++i) {
        int k = N + i;
        auto t0 = Clock::now();
        db.Get(make_key(k), v);
        auto t1 = Clock::now();
        lat.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    auto end = Clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    print_stats("read_miss", compute_stats(lat, ms), N);
}

void bench_overwrite(int N) {
    cleanup();
    KVStore db;
    std::vector<double> lat;
    lat.reserve(N);

    auto start = Clock::now();
    for (int i = 0; i < N; ++i) {
        int k = i % 1000;
        auto t0 = Clock::now();
        db.Put(make_key(k), make_value(i));
        auto t1 = Clock::now();
        lat.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    auto end = Clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    print_stats("overwrite", compute_stats(lat, ms), N);
}

void bench_delete(int N) {
    cleanup();
    KVStore db;
    std::cout << "  [准备] 写入 " << N << " 条..." << std::endl;
    for (int i = 0; i < N; ++i) db.Put(make_key(i), make_value(i));

    std::vector<double> lat;
    lat.reserve(N);

    auto start = Clock::now();
    for (int i = 0; i < N; ++i) {
        auto t0 = Clock::now();
        db.Delete(make_key(i));
        auto t1 = Clock::now();
        lat.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    auto end = Clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    print_stats("delete", compute_stats(lat, ms), N);
}

void bench_readwrite_mix(int N) {
    cleanup();
    KVStore db;
    for (int i = 0; i < N; ++i) db.Put(make_key(i), make_value(i));

    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, N - 1);
    std::uniform_int_distribution<int> op_dist(0, 1);

    std::vector<double> lat;
    lat.reserve(N);
    std::string v;

    auto start = Clock::now();
    for (int i = 0; i < N; ++i) {
        int k = dist(rng);
        auto t0 = Clock::now();
        if (op_dist(rng) == 0) db.Put(make_key(k), make_value(k));
        else                    db.Get(make_key(k), v);
        auto t1 = Clock::now();
        lat.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    auto end = Clock::now();

    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    print_stats("readwrite_mix", compute_stats(lat, ms), N);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <scenario> <N>\n"
                  << "Scenarios: write_seq read_hit read_miss overwrite delete readwrite_mix\n";
        return 1;
    }

    std::string scenario = argv[1];
    int N = std::atoi(argv[2]);

    std::cout << "===== FastKV Benchmark =====" << std::endl;
    std::cout << "  Scenario: " << scenario << std::endl;
    std::cout << "  N:        " << N << std::endl;

    if      (scenario == "write_seq")      bench_write_seq(N);
    else if (scenario == "read_hit")       bench_read_hit(N);
    else if (scenario == "read_miss")      bench_read_miss(N);
    else if (scenario == "overwrite")      bench_overwrite(N);
    else if (scenario == "delete")         bench_delete(N);
    else if (scenario == "readwrite_mix")  bench_readwrite_mix(N);
    else {
        std::cerr << "Unknown scenario: " << scenario << std::endl;
        return 1;
    }

    return 0;
}