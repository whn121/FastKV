# FastKV

C++17 实现的 LSM-Tree 持久化 KV 存储引擎。

## 特性

- **LSM-Tree 架构**：MemTable → SSTable
- **WAL (Write-Ahead Log)**：崩溃恢复，fsync 强持久化
- **Tombstone**：DELETE 语义，跨 SSTable 可见
- **两级 Compaction**：level_0 → level_1 合并
- **索引分离**：内存索引 + 磁盘索引文件，重启快速恢复

## 目录结构
