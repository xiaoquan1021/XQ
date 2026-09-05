# scale-probe — 真规模 + 真最难存储探针(冻接口前打穿)

> 父任务 `06-29-xq-rebuild`,准则「先证后冻 + 拿最难场景定型」的执行载体(见父 ROADMAP 主轴准则修订)。
> **定位**:M8b-1 收尾后启动,位于 libXQ/Source 冻结门**之前**、M9a 迁移消费者**之前**。
> **性质**:丢弃式 spike——目标是产出"接口形状结论",代码本身可弃。成本/返工不计。

## Goal

在 M8b-1 定义的 `IVoxelSource`/`IGeometrySource`/`ReadLease` 接口冻结之前,用**真实规模负载**
和**真实最难后备存储**把接口"打穿":让接口在它最终必须支撑的场景(2000万三角/1000万 tet、
mmap 磁盘后备、后台并发驱逐)下暴露形状缺陷,把缺陷回灌进接口签名,**然后才允许冻结**。
避免把所有消费者(M9a)迁移到一个只在"小规模 + resident vector"下验证过的接口上。

## Background(为什么需要这个探针)

2026-06-30 多维评审 + 红队对抗发现:M8b-1 的接口若只在 resident vector + 单数据集(0007)下验证,
有四类形状缺陷到 M9b 真规模时才会暴露,届时消费者已迁在错接口上、方向已偏:

1. **完整性 vs 流式冲突**:M8a `BlobStore` 把 blob 身份定义为整文件 SHA-256、每次 read 全量重算
   (`XQ/src/io/blob/BlobStore.h:12-20,40`)。mmap 懒映射"只触碰可见块"与"全量验证"结构性冲突
   → `acquire` 粒度被完整性模型反向决定(可能强制 all-or-nothing)。
2. **reader 端拷贝是真墙**:`BlobStore::get` 返回 `std::vector` 且逐元素 LE 解码、非 memcpy
   (`BlobStore.h:23,63-65`)。`MappedVoxelSource` 无法复用 `get()`,需要全新
   "map+verify+reinterpret-span" 零拷贝读路径。
3. **lease 是并发原语不是单线程 RAII**:后台 load/evict 时渲染线程持有视图
   → `ReadLease` 必须是对抗并发驱逐的引用计数读锁。M8b-1 当前实现是单线程 move-only RAII。
4. **AoS `Point3` 布局**:`Point3{double x,y,z}` 24 字节/点(`GeometryTypes.h:8`),2000万点的
   GPU 上传/mmap 友好性存疑,可能需要 SoA。

## Requirements

- **R1 合成负载生成器**:可生成 ~2000万三角 surface 与 ~1000万 tet 体网格(确定性、可复现、
  落成 M8a 格式的 sidecar blob),不依赖跑真实分割/建模/网格流水线。
- **R2 真 `MappedVoxelSource`(探针级,可弃)**:Win32 `CreateFileMapping`/`MapViewOfFile` 只读映射
  M8a sidecar blob,实现 map → verify(完整性策略候选)→ 暴露为 Source 接口的 span。
- **R3 并发 evict harness**:后台线程在渲染/读取线程持有 lease 期间尝试 unmap/释放,
  验证 lease 的生命周期契约在并发下是否成立(暴露第 3 类缺陷)。
- **R4 接口打穿报告**:针对四类缺陷各产出"当前 M8b-1 接口能否承载 / 需要怎么改签名"的结论,
  含具体接口形状建议(lease 语义、acquire 粒度与失败模式、完整性策略、AoS vs SoA)。
- **R5 完整性策略候选对比**:至少对比"验证一次后信任映射" vs "分段 Merkle/segment 哈希"
  两条路径在懒映射下的可行性与代价,给 M8b-2 决策提供数据。
- **R6 不污染主线**:探针代码隔离(独立目录/可选 CMake 开关,默认 OFF),不改 M8b-1 在飞的
  `XQ/src/core/source/` 接口实现,不改任何现有消费者。Source 契约打穿用,接口若需改由 M8b-1 收口。

## Acceptance Criteria

- [ ] AC1 合成负载生成器可产出 20M 三角 + 10M tet 资产并写成 M8a sidecar blob(SHA-256 校验通过)。
- [ ] AC2 真 `MappedVoxelSource` 能 mmap 该 blob 并经 `IVoxelSource`/`IGeometrySource` 接口吐出 span,
      内容与生成器源数据逐元素一致。
- [ ] AC3 在该规模下测得:open-time(map+verify)、内存峰值、(若接 VTK)上传耗时,形成基线数字。
- [ ] AC4 并发 evict harness 跑通,明确给出"当前 lease 契约在并发驱逐下是否安全"的结论
      (是→记录;否→给出 lease 需要的并发签名)。
- [ ] AC5 产出接口打穿报告(R4)+ 完整性策略对比(R5),作为冻结门与 M8b-2 §冲突决策的输入。
- [ ] AC6 主线零污染:探针默认 CMake OFF,M8b-1 接口实现与现有消费者/测试不被改动,Release 全量 ctest 仍绿。

## Out of Scope

- 不做 M8b-2 的生产级 `GeometryResourceManager` / 内存预算 / 真正去 vector 化(那是 M8b-2,探针只验证接口形状)。
- 不做 LOD / 分块渲染 / 渐进上传(M9b)。
- 不迁移真实消费者到 Source(M9a)。
- 不在探针里冻结任何接口——探针只产出"该怎么冻"的结论,冻结动作在 M8b-1 收口 + 冻结门。

## Notes

- 复杂任务:`task.py start` 前补 `design.md`(探针架构 + 完整性策略候选)与 `implement.md`。
- 依赖:M8b-1(`06-30-source-interface`)接口落定后启动——探针要喂的就是它的接口。
- 关联 memory:`m8a-wholefile-sha-vs-streaming`(完整性 vs 流式冲突的根因记录)。
- 平台:Windows + Git Bash,mmap 走 Win32 API(非 POSIX `mmap`),构建配方见 memory `xq-build-recipe`。
