cmake_minimum_required(VERSION 3.16)
project(FastKV LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ═══════ 公共源文件（不含 main.cpp）═══════
set(SRC_COMMON
    src/KVStore.cpp
    src/MemTable.cpp
    src/SSTable.cpp
    src/WAL.cpp
    src/BloomFilter.cpp
)

# ═══════ 主程序 ═══════
add_executable(fastkv
    src/main.cpp
    ${SRC_COMMON}
)
target_include_directories(fastkv PRIVATE ${PROJECT_SOURCE_DIR}/include)

# ═══════ 测试 ═══════
add_executable(test_kv
    tests/test_kv.cpp
    ${SRC_COMMON}
)
target_include_directories(test_kv PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(test_compaction
    tests/test_compaction.cpp
    ${SRC_COMMON}
)
target_include_directories(test_compaction PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(test_crash
    tests/test_crash.cpp
    ${SRC_COMMON}
)
target_include_directories(test_crash PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_executable(test_bloom
    tests/test_bloom.cpp
    src/BloomFilter.cpp
)
target_include_directories(test_bloom PRIVATE ${PROJECT_SOURCE_DIR}/include)

# ═══════ Benchmark（如果存在）═══════
if(EXISTS ${CMAKE_SOURCE_DIR}/benchmark/benchmark.cpp)
    add_executable(benchmark
        benchmark/benchmark.cpp
        ${SRC_COMMON}
    )
    target_include_directories(benchmark PRIVATE ${PROJECT_SOURCE_DIR}/include)
endif()
