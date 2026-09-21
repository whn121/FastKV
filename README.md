# FastKV

C++17 实现的 LSM-Tree 持久化 KV 存储引擎。

## 核心特性

- **LSM-Tree 架构**：MemTable → SSTable 分层存储
- **WAL + Group Commit**：崩溃恢复，批量 fsync 提升写吞吐
- **Tombstone**：DELETE 语义，跨 SSTable 可见
- **两级 Compaction**：level_0 → level_1 合并
- **Bloom Filter**：FP 率 ~1%，加速不存在 key 查询
- **单文件 SSTable**：Footer + Data + Index + Bloom Section
- **原子写入**：.tmp + rename 保证崩溃一致性

## 架构

    Put/Get/Delete
         │
         ▼
    ┌──────────┐
    │ KVStore  │
    └────┬─────┘
         │
    ┌────┼────┐
    ▼    ▼    ▼
   WAL  内存  SSTable
    │    │      │
   fsync │    单文件格式
         │
      满 20480 条 → flush
         │
      满 4 个 → Compaction

## 目录结构

    FastKV/
    ├── include/          # KVStore / MemTable / SSTable / WAL / BloomFilter
    ├── src/              # 对应实现
    ├── tests/            # 功能 / Compaction / 崩溃恢复 / Bloom 测试
    ├── benchmark/        # 性能测试
    └── CMakeLists.txt

## 编译

    mkdir -p build
    g++ -std=c++17 -O2 tests/test_kv.cpp \
        src/KVStore.cpp src/MemTable.cpp src/SSTable.cpp \
        src/WAL.cpp src/BloomFilter.cpp \
        -Iinclude -o build/test_kv

## 测试

| 测试 | 覆盖 |
|------|------|
| test_kv | 30 项功能测试（读写/删除/Replay/压力/Bloom） |
| test_compaction | 9 项 Compaction 正确性（20 万条跨层验证） |
| test_crash | 崩溃恢复（SIGKILL + 重启不丢） |
| test_bloom | 9 项 Bloom Filter 单元测试（FP 率/序列化） |

## 性能数据

环境：Linux (WSL2), GCC, -O2, 单线程

| 场景 | N | 吞吐 | P50 | P99 |
|------|---|------|-----|-----|
| write_seq | 100000 | 400,012 ops/s | 0.84us | 3.52us |
| read_hit | 100000 | 558,084 ops/s | 1.89us | 3.63us |
| **read_miss** | 100000 | **7,408,064 ops/s** | 0.11us | 0.21us |
| overwrite | 100000 | 1,048,053 ops/s | 0.74us | 2.15us |
| delete | 100000 | 484,281 ops/s | 0.70us | 1.86us |
| readwrite_mix | 50000 | 377,514 ops/s | 1.40us | 11.87us |

**对比同步 fsync**：835 ops/s → Group Commit：400,012 ops/s（**提升 400+ 倍**）

**Bloom Filter 效果**：read_miss 比 read_hit 快 **13 倍**（跳过所有 SSTable）

## 关键设计

### 1. WAL + Group Commit

- 每次 PUT/DELETE 先追加 WAL（POSIX open/write）
- PUT 只写 page cache 并标记 need_sync，后台线程每 10ms 批量 fsync
- 代价：崩溃时最多丢最后 10ms 数据

### 2. 单文件 SSTable

    ┌──────────────────────────────┐ offset 0
    │ Data Section                 │
    │ [deleted][key_len][key][vlen][value]...
    ├──────────────────────────────┤
    │ Index Section                │
    │ [key_len][key][offset]...
    ├──────────────────────────────┤
    │ Bloom Section                │
    │ [num_bits][num_hashes][bits]
    ├──────────────────────────────┤
    │ Footer（52 字节）             │
    │ data/index/bloom offset+size + magic
    └──────────────────────────────┘

- 先写 .tmp 再 rename，崩溃时要么完整要么不存在
- magic (0xFA57CAFE) 验证文件完整性
- Index 存的是"相对 Data Section 的偏移"
- Get 时 seek 位置 = data_offset + index 里的 offset

### 3. Bloom Filter

- 每个 SSTable 一个 Bloom Filter，`bits_per_key = 10`, `num_hashes = 7`
- **无 False Negative，FP 率约 1%**（实测 0.45%）
- 查询顺序：MemTable → Bloom → Index → 读磁盘

### 4. Tombstone

DELETE 不直接删 key，写入 `deleted=true` 标记。跨 SSTable 合并时能正确隐藏旧值。

### 5. Compaction 合并流程

    level_0 有 4 个 SSTable → 合并 → level_1 一个新 SSTable

    1. 从旧到新遍历 level_0 的 Data Section
    2. 每条 key-value 塞进 std::map（后覆盖先，保留最新）
    3. 按 key 升序写到新文件（SSTable 必须有序）
    4. 重建 Index 和 Bloom
    5. .tmp + rename
    6. 删除 level_0 旧文件

- **从旧到新 + map 覆盖**：保证最新版本胜出
- **Tombstone 必须保留**：遮挡 level_1 的老值
- **崩溃安全**：level_0 旧文件在 compaction 成功后才删，中途崩溃数据不丢

### 6. 崩溃恢复

- **WAL**：记录未落盘操作，重启 Replay 恢复到 MemTable
- **SSTable**：.tmp + rename 保证原子性，启动清理残留 .tmp
- **测试验证**：SIGKILL 中途打断 + 重启，数据不丢

## 已知限制

- 单线程（MemTable 用 std::map，无并发优化）
- level_1 无限增长（未实现多级 LSM）
- 未实现 LRU Block Cache
- 未实现 Raft 分布式一致性

## 后续计划

- [ ] 多级 LSM（level_2, level_3）
- [ ] Immutable MemTable + 后台 Flush
- [ ] LRU Block Cache
- [ ] Raft 分布式扩展

## 技术栈

- C++17
- Linux / POSIX IO
- CMake
