# FastKV

C++17 实现的 LSM-Tree 持久化 KV 存储引擎。

## 特性

- **LSM-Tree 架构**：MemTable → SSTable
- **WAL (Write-Ahead Log)**：崩溃恢复，Group Commit 批量刷盘
- **Tombstone**：DELETE 语义，跨 SSTable 可见
- **两级 Compaction**：level_0 → level_1 合并
- **Bloom Filter**：FP 率 ~1%，加速不存在 key 查询
- **单文件 SSTable**：Footer + Data + Index + Bloom Section
- **原子写入**：.tmp + rename，崩溃一致性保证
- **Group Commit**：批量 fsync，写吞吐提升 400 倍

## 目录结构

    FastKV/
    ├── include/
    │   ├── KVStore.h
    │   ├── MemTable.h
    │   ├── SSTable.h
    │   ├── WAL.h
    │   └── BloomFilter.h
    ├── src/
    │   ├── KVStore.cpp
    │   ├── MemTable.cpp
    │   ├── SSTable.cpp
    │   ├── WAL.cpp
    │   └── BloomFilter.cpp
    ├── tests/
    │   ├── test_kv.cpp
    │   ├── test_compaction.cpp
    │   ├── test_crash.cpp
    │   └── test_bloom.cpp
    ├── benchmark/
    │   └── benchmark.cpp
    └── CMakeLists.txt

## 编译

    mkdir -p build
    g++ -std=c++17 -O2 tests/test_kv.cpp \
        src/KVStore.cpp src/MemTable.cpp src/SSTable.cpp \
        src/WAL.cpp src/BloomFilter.cpp \
        -Iinclude -o build/test_kv

## 测试

    ./build/test_kv           # 功能测试（30 项）
    ./build/test_compaction   # Compaction 正确性
    ./build/test_crash write 10000
    ./build/test_crash read 10000
    ./build/test_bloom        # Bloom Filter 单元测试

## Benchmark

    ./build/benchmark write_seq 100000
    ./build/benchmark read_hit 100000
    ./build/benchmark read_miss 100000
    ./build/benchmark readwrite_mix 50000

### 性能数据

环境：Linux, GCC, -O2, 单线程

| Scenario | N | Throughput | P50 | P99 |
|----------|---|-----------|-----|-----|
| write_seq | 100000 | 400,012 ops/s | 0.84us | 3.52us |
| read_hit | 100000 | 558,084 ops/s | 1.89us | 3.63us |
| read_miss | 100000 | 7,408,064 ops/s | 0.11us | 0.21us |
| overwrite | 100000 | 1,048,053 ops/s | 0.74us | 2.15us |
| delete | 100000 | 484,281 ops/s | 0.70us | 1.86us |
| readwrite_mix | 50000 | 377,514 ops/s | 1.40us | 11.87us |

### 对比：同步 fsync 模式

| Scenario | N | Throughput |
|----------|---|-----------|
| write_seq | 100000 | 835 ops/s |

**Group Commit 让写吞吐提升 400+ 倍**（835 → 400,012 ops/s）。

## 关键设计

### 1. WAL 强持久化 + Group Commit

- 每次 PUT/DELETE 先追加 WAL
- 使用 POSIX open + write（而不是 ofstream）
- Group Commit：PUT 只写 page cache 并标记 need_sync，后台线程每 10ms 批量 fsync
- 代价：崩溃时可能丢最后 10ms 的数据

### 2. 单文件 SSTable

    ┌─────────────────────────────────┐
    │  Data Section                    │
    │  [deleted][key_len][key][value]..│
    ├─────────────────────────────────┤
    │  Index Section                   │
    │  [key_len][key][offset]...       │
    ├─────────────────────────────────┤
    │  Bloom Section                   │
    │  [num_bits][num_hashes][bits]    │
    ├─────────────────────────────────┤
    │  Footer（52 字节）                │
    │  offsets + sizes + magic          │
    └─────────────────────────────────┘

- 先写 .tmp 再 rename，保证原子性
- Footer 用 magic 验证文件完整性
- Index 里存的是相对 Data Section 的偏移

### 3. Bloom Filter

- 每个 SSTable 一个 Bloom Filter
- bits_per_key = 10, num_hashes = 7
- 无 False Negative，FP 率约 1%
- 实测 FP 率：0.45%

### 4. Tombstone

DELETE 不直接删除 key，而是写入 deleted=true 标记。跨 SSTable 合并时能正确隐藏旧值。

### 5. 两级 Compaction

- level_0 累积 4 个 SSTable 后合并到 level_1
- 用 std::map 合并，从旧到新遍历，后写的覆盖先写的
- 输出按 key 有序（Sorted String Table）
- 合并时保留 tombstone，避免读到 level_1 的老值

### 6. 崩溃恢复

- WAL：记录未落盘的操作，重启时 Replay
- SSTable：.tmp + rename 保证要么不存在要么完整
- 启动清理：删除残留的 .tmp 文件

## 已知限制

- 单线程（MemTable 用 std::map，未做并发优化）
- level_1 无限增长（未实现多级 LSM）
- 未实现 LRU Block Cache
- 未实现 Raft 分布式一致性

## 后续计划

- [ ] 多级 LSM（level_2, level_3）
- [ ] Immutable MemTable + 后台 Flush
- [ ] 后台 Compaction
- [ ] LRU Block Cache
- [ ] Raft 分布式扩展

## 技术栈

- C++17
- Linux / POSIX IO
- CMake
