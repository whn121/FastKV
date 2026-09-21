#include "KVStore.h"
#include <iostream>
#include <cstdlib>
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " write|read N [sleep_ms]" << std::endl;
        std::cerr << "  write 模式：写 N 条数据后强制 _Exit(0)" << std::endl;
        std::cerr << "  read  模式：读 N 条数据，打印找到多少" << std::endl;
        std::cerr << "  sleep_ms：write 模式下，写完先等 X 毫秒再 _Exit" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    int N = std::atoi(argv[2]);
    int sleep_ms = (argc > 3) ? std::atoi(argv[3]) : 0;

    if (mode == "write") {
        KVStore db;
        std::cout << "Writing " << N << " keys..." << std::endl;
        for (int i = 0; i < N; ++i) {
            db.Put("key_" + std::to_string(i), "value_" + std::to_string(i));
        }
        std::cout << "Done. ";

        if (sleep_ms > 0) {
            std::cout << "Waiting " << sleep_ms << " ms for fsync..." << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        } else {
            std::cout << "Forcing _Exit(0) immediately..." << std::endl;
        }

        // ═══════ 关键：强制退出，跳过所有析构 ═══════
        // 相当于进程被 kill -9
        std::_Exit(0);
    }
    else if (mode == "read") {
        KVStore db;
        std::string v;
        int found = 0;
        for (int i = 0; i < N; ++i) {
            if (db.Get("key_" + std::to_string(i), v)) {
                if (v == "value_" + std::to_string(i)) found++;
            }
        }
        std::cout << "Found " << found << " / " << N << std::endl;
        return (found == N) ? 0 : 1;
    }

    return 1;
}