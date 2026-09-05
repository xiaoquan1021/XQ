# 修复 GeometryResourceManager 句柄生命周期

## Goal

`GeometryResourceManager::VoxelSourceHandle` 和 `GeometrySourceHandle` 必须在句柄自身存活期间保持底层 source 对象有效，即使创建该句柄的 `GeometryResourceManager` 已经析构。禁止保留“`valid()==true` 但 `source()` 指向已释放对象”的悬空访问路径。

## Confirmed Evidence

- `XQ/src/services/resource/GeometryResourceManager.h:57-74` 的 `VoxelSourceHandle` 保存 `const IVoxelSource* source_` 和 `std::shared_ptr<void> pin_`，`pin_` 只用于驱逐门控，不拥有 `source_`。
- `XQ/src/services/resource/GeometryResourceManager.h:97-114` 的 `GeometrySourceHandle` 同样保存裸 `const IGeometrySource* source_`。
- `XQ/src/services/resource/GeometryResourceManager.cpp:124` 使用默认析构；manager 析构时 `blocks_` 被销毁，`ResidentBlock` 内 `std::unique_ptr<...Source>` 释放 source。
- `XQ/src/services/resource/GeometryResourceManager.cpp:177-195` 和 `292-308` 创建 `unique_ptr` resident block 后只把裸指针与 `pin` 返回给 handle。handle 越过 manager 生命周期后仍可能 `valid()==true`。
- 现有 `XQ/tests/services/resource/GeometryResourceManagerTest.cpp:338-380`、`408-478`、`616-642` 覆盖了 manager 存活期间的驱逐 pin 行为，但未覆盖 manager 析构后的 handle/lease 访问。

## Requirements

### R1. Handle 必须拥有 source 生命周期

有效的 `VoxelSourceHandle` / `GeometrySourceHandle` 必须持有足够所有权，保证 `source()` 和 `operator->()` 在 handle 存活期间不会悬空。manager 析构不得使既有有效 handle 变为悬空。

### R2. 保持既有驱逐和缓存语义

当前 LRU、budget、`pin.use_count()` 驱逐门控、`mapCount()`、`hitCount()`、`evictCount()` 行为必须保持。live handle 仍必须阻止 manager 存活期间的 resident block 被驱逐；handle 释放后仍可被 budget 驱逐。

### R3. Lease 视图必须可跨 manager 生命周期读取

从 live handle 获取的 `VoxelLease`、`GeometryLease`、`TriangleLease` 必须在 manager 析构后继续维持底层映射有效，直到 lease 自身释放。

### R4. 不扩大公共调用面

保持现有调用方式：`valid()`、`source()`、`operator->()`、copy/move 语义和 acquire 方法签名不变。不引入 Qt/VTK 依赖到 core header；修复限定在 services/resource 与测试。

## Acceptance Criteria

- [ ] 新增 voxel 回归：handle 从局部 manager 中返回；manager 析构后，`handle.valid()` 仍为 true，`handle.source().acquire_whole()` 可读取正确数据。
- [ ] 新增 geometry 回归：geometry handle 从局部 manager 中返回；manager 析构后，`acquire_points()` 和 `acquire_triangles()` 可读取正确数据。
- [ ] 新增 lease 回归：在 manager 析构前取得 lease，manager 析构后继续读取 lease span，数据仍正确。
- [ ] 既有 budget/LRU/pin/concurrency 测试仍通过，证明没有削弱驱逐门控。
- [ ] Targeted: `test_geometry_resource_manager`、`test_geometry_source_resolver`、`test_mapped_voxel_source`、`test_mapped_geometry_source` 通过。
- [ ] Final child check: Release 全量 `ctest` 通过。

## Out of Scope

- 不改 `IVoxelSource` / `IGeometrySource` 公共接口。
- 不重写资源管理器为异步/background cache。
- 不改变 blob schema、sidecar resolver 或 renderer connectivity；这些由其它子任务处理。
