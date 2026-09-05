# M9b-A 生产几何 mmap 源 + acquireGeometrySource 工厂 + borrow-faceId(P0 基座)

## Goal

在已冻 **libXQ / Source 1.0** 之上,补齐 M9a 显式留下的「几何懒驻留基座」缺口,全部以**纯增量「加不破」**方式落地三件互相咬合的能力:

1. **生产级 `MappedGeometrySource`(io)** — 照 `MappedVoxelSource` 范式,对磁盘上的内容寻址几何 blob(`points` F64x3 / `tris` I32x3 / `faceId` I32x1 / `volPoints` F64x3 / `tets` I32x4)做 **map → verify → reinterpret 零拷贝**,实现 `IGeometrySource` 三个 acquire。完整性默认 **FullVerify**(writer 不产几何 `.merkle`,见 Constraints)。
2. **`GeometryResourceManager::acquireGeometrySource` 工厂(services)** — 与现有 `acquireVoxelSource` 对称:`assetId + spec → 带 pin 的 handle`,命中缓存不重映射,超预算 LRU 只驱逐 `use_count==1` 的块;`ResidentBlock` 泛化为可同时承载 voxel 与 geometry 两类源,预算按**整 asset 的多个 blob 字节求和**。
3. **`TriangleLease` borrow-faceId 零拷贝变体(core)** — 新增第二 keepalive 成员 + 新静态工厂,消灭 `ResidentSurfaceSource::acquire_triangles` 每次 O(N) 的 faceId 物化,并让 mmap 几何源把独立的 faceId blob 直接借出。原 `TriangleLease::borrow` 签名/语义**一字不改**。

这是 **E(payload 去 vector 化)** 的前置基座,但 **A 本身不改任何 payload 结构、不改任何消费者读路径**。完成后,reader 不再被迫整块常驻几何 blob(那一步在 E 接线),A 只交付「源 + 工厂 + 零拷贝 faceId」三块可独立验收的底座。

## Background(调研已核实坐标)

- **Source 1.0 形状(实读)**:`IGeometrySource.h:19-26` 三个纯虚(`meta` / `acquire_points` / `acquire_triangles` / `acquire_tetrahedra`)。`SourceViews.h`:`Point3`=packed `double[3]`(`GeometryTypes.h:8-12`,24B 无填充)、`SourceTriangle`=`std::array<int,3>`、`SourceTet`=`std::array<int,4>`,`TriangleView` 含 `triangles` + 等长并行 `faceIds`(`SourceViews.h:100-103`)。
- **mmap 体素范式(实读,直接照搬)**:`MappedVoxelSource.{h,cpp}` = 构造期 `map_file_readonly`(`MmapBlob.h:27`,返回 `MappedFile{ok,base,size,keepalive,lastError}`,0 字节文件 `ok=false`)→ 校验 `mapped_.size == totalBytes()` → `valid_`。读路径 `acquire_whole` 零拷贝 borrow(keepalive = mmap 控制块)、`verify_range` 惰性 memoized(`FullVerify` 整块 hash 一次置 `fullVerified_`;`SegmentedMerkle` 走 `MerkleSidecar::verify_segment` 按段)。`IntegrityStats{bytesHashed,segmentsChecked,acquireCount}` 供测试证惰性。
- **探针弃码参考(实读,生产重写不复用)**:`XQ/probe/MappedGeometrySource.cpp` 已实证 mmap 基址(页对齐满足 8B)`reinterpret_cast<const Point3*>` / `const SourceTet*` / `const SourceTriangle*` 零拷贝、AC2 逐元素 == 源全 PASS。其 `:74-77` 明确标注 faceId 因 `TriangleLease::borrow` 只收 owned `vector<int>` 被迫每次拷 ~80MB(20M 三角)——这正是本任务 borrow-faceId 要消灭的点。探针无完整性校验、无资源管理,生产版补齐。
- **faceId 物化点(实读)**:`ResidentSurfaceSource.cpp:44-51` 每次 acquire 跑一遍 `for i: handle_->triangleFaceId(i)` 物化整个 `vector<int>`。底层 `XQTriangleSurfaceGeometryHandle.h:59` 其实把 `triangleFaceIds_` 存成**连续 `std::vector<int>`**,只是仅暴露标量访问器 `triangleFaceId(i)`(`:46`)、无 vector 访问器——故 Resident 侧 borrow-faceId 需给 handle **加一个 vector 访问器**(加不破)。
- **lease 形状(实读)**:`ReadLease.h:124-158` `TriangleLease` 现持单 `keepalive_`(:155)+ `ownedFaceIds_`(:156)+ `view_`(:157),`borrow(keepalive, triangles, faceIds)`(:136-147)把 triangles 借出、faceIds 物化进 `ownedFaceIds_`。move-only + defaulted move。
- **资源管理器对称面(实读)**:`GeometryResourceManager.{h,cpp}` 现仅 voxel。`ResidentBlock`(`.h:106-111`)持 `unique_ptr<IVoxelSource> source` + `shared_ptr<void> pin` + `bytes` + `lruIt`;`acquireVoxelSource`(`.cpp:60-118`)命中 bump LRU + 发新 pin、未命中经 `voxelsRelPath` 解析 role→构造 `MappedVoxelSource`→resident+预算;`evictToBudgetLocked`(`.cpp:126-147`)LRU 尾向前、`pin.use_count()>1` 跳过(协作式 gating,acquire/evict 共一 `mutex_` 防 TOCTOU)。
- **几何 blob role 与布局(实读)**:`XQProjectWriter.cpp:725` `put_triangle_geometry` 写 `points`/`tris`/`faceId`(surface)或 `surfPoints`/`surfTris`/`surfFaceId`(mesh 表面)+ `volPoints`/`tets`(mesh 体)。blob 是无头裸小端字节(`BufferRef.h:24-33` 自描述 `elementType`/`components`/`elementCount`/`byteCount`/`sha256`/`relPath`);`AssetRecord.blobs`(`AssetRecord.h:58`)= `vector<pair<role, BufferRef>>`。
- **完整性现状(实证)**:writer 不产几何 `.merkle` sidecar(父任务 grep 实证)→ 几何 mmap 源默认 **FullVerify**,否则老工程几何 asset 静默 acquire 失败。

## Scope

### In scope
- **`MappedGeometrySource`(io,`src/io/source/`)**:新建 `IGeometrySource` 实现,构造收一个描述 blob 路径 + 元数据(每 role 的 `elementCount`/`components`/`elementType`)+ 完整性模式的 spec;对存在的 role 各 `map_file_readonly`、校验 `mapped_.size == elementCount*components*sizeof(elem)`;`verify_range` 惰性 memoized(默认 FullVerify,SegmentedMerkle 读路径并存但写路径 out-of-scope);三个 acquire 零拷贝 reinterpret + borrow(keepalive=各自 blob 的 mmap 控制块)。部分源(只表面 / 只体)缺失 role 返回 empty lease。
- **`acquireGeometrySource` 工厂(services)**:`GeometryResourceManager` 加 `GeometrySourceSpec`(各 role 的解码元数据 + `segmented` 标志)+ `GeometrySourceHandle`(与 `VoxelSourceHandle` 对称,copy 即加 pin)+ `acquireGeometrySource(assetId, spec)`;`ResidentBlock` 泛化为可承载 voxel **或** geometry 源(两者共用 pin/bytes/lruIt/LRU/预算机制);`bytes` 按该 asset 全部几何 blob 的 `byteCount` 求和;命中缓存不重映射、超预算 LRU 只回收 `use_count==1`。
- **`TriangleLease` borrow-faceId(core)**:新增静态工厂(如 `borrow_borrowed_faceids(triKeepalive, triangles, faceIdKeepalive, faceIdSpan)`)+ 第二 keepalive 成员;`view_.faceIds` 直接指向借入的连续 span,零物化。原 `borrow` 与 `empty` 不动。
- **Resident 侧受益**:给 `XQTriangleSurfaceGeometryHandle` 加 `const std::vector<int>& triangleFaceIds() const` 访问器(加不破),`ResidentSurfaceSource::acquire_triangles` 改走 borrow-faceId(消灭 `:44-51` 的 O(N) 物化)。
- **独立验收测试**:mmap 几何源逐元素 == 源 / 越界 invalid / 工厂命中不重映射 / 超预算 LRU 只驱逐 `use_count==1` / borrow-faceId 零物化 / 分层零违反。

### Out of scope(明确划归后续)
- **几何 sidecar 写路径 + SegmentedMerkle 几何完整性**:A 默认 FullVerify;写 `.merkle` 边车 + 段哈希校验作显式后续项(读路径可并存,但不靠它)。
- **payload 去 vector 化 / reader 改走惰性 source(E)**:A 只交付基座,不接线 reader、不改 payload 结构、不改 clone 语义。
- **LOD / 分块 / 渐进上传 / 拾取(B/C/D)**:不碰 visualization。
- **cell→faceId 显式映射表**:A 不重排、不抽稀,faceId 与 triangle 下标天然对齐;显式映射表是 B/C/D 的事(见 Constraints)。
- **region/slab strided 零拷贝子视图**:几何源只整块 acquire(无 region 语义),不引入 stride。
- **不复用 `XQ/probe/` 弃码**:参考可,生产重写在 `src/`。

## Constraints
- **不破已冻 Source 1.0 公共签名**:`IGeometrySource.h:19-26` 三纯虚**绝不加纯虚**;一切新增是「加不破」——新静态工厂、新成员、`GeometrySourceSpec`、manager 新方法、handle 新访问器。`ReadLease.h` 既有 `TriangleLease::borrow`/`empty` 签名一字不改,新增第二 keepalive 成员不破 move-only + defaulted move。`SourceViews.h` 的 `static_assert`(`sizeof(Point3)==3*sizeof(double)` 等)仍成立。
- **分层(ROADMAP 锁死)**:`MappedGeometrySource`→io;`acquireGeometrySource`→services(头只见 core 抽象 `IGeometrySource`/`AssetId`,io 的具体源在 .cpp 构造,沿用现有 `services → io PRIVATE` 边,无新跨层环);borrow-faceId→core。**io/core 永无 `vtk*`**。
- **完整性默认 FullVerify**:writer 不产几何 `.merkle`(实证),几何 mmap 源默认 `FullVerify`(整块 hash 一次 memoized),不依赖 sidecar;`SegmentedMerkle` 仅在 sidecar 存在时可选,缺失不算错。
- **lease 是并发原语(scale-probe 缺陷③已定调)**:借出的 view 指针在 lease 存活期间不得失效;keepalive 即 eviction-gating pin,evictor 只在 `use_count()==1` 回收,acquire/evict 共一锁防 TOCTOU(沿用 voxel 侧机制,几何源 keepalive=mmap 控制块,与 manager 的 per-block pin 双层独立)。
- **cell→faceId 下标契约**:A 借出的 `faceIds` span 与 `triangles` span **等长、同序、下标一一对应**(不重排、不抽稀),维持现「cell 顺序==三角下标==faceId 下标」的隐式契约;显式映射表留 B/C/D。
- **最小改动**:零元素 / 缺 role / 越界 → empty/invalid lease(现契约),不加没要求的降级兜底分支。
- **验收铁律**:Release + 全量 ctest + 假绿抽查;副作用调用(`acquire_*` / `verify_range`)不进 assert(`/DNDEBUG` 删掉=假绿 segfault)。

## Acceptance Criteria
- [x] **AC1 几何 mmap 源逐元素 == 源**:`MappedGeometrySource` 经 `acquire_points()/acquire_triangles()/acquire_tetrahedra()` 吐出的 span,与写入磁盘 blob 前的源数组**逐元素相等**(points 三分量、tris/tets 各整数、faceId 整数);zero-copy(span.data() 落在 mmap 基址区间内,无中间 `vector` 拷贝)。FullVerify 模式下首次 acquire 触发整块 hash 一次、再次 acquire 不重复 hash(`stats().bytesHashed` 不再增长)。
- [x] **AC2 缺失 role / 越界 / 损坏 → 空或 invalid,不抛**:部分源(只 `points+tris+faceId`,无 tets)`acquire_tetrahedra()` 返回 `span().empty()`;blob 文件缺失 / 0 字节 / `size != count*components*sizeof(elem)` / 元素类型不匹配 → `valid()==false` 且对应 acquire 返回 empty/invalid lease;FullVerify 下篡改 blob 字节(假绿抽查)→ 校验后仍能被独立断言检出(校验路径不被 `/DNDEBUG` 抹除)。
- [x] **AC3 acquireGeometrySource 命中缓存不重映射**:同一 `assetId` 二次 `acquireGeometrySource` → `hitCount()` +1、`mapCount()` 不变(无重新 `map_file_readonly`),LRU 被 bump;返回 handle `valid()` 且指向同一源实例。未知 asset / 缺几何 blob / 映射 invalid → handle `valid()==false`。
- [x] **AC4 超预算 LRU 只驱逐 use_count==1**:`setBudgetBytes` 设为略小于两个几何 asset 之和;持有 asset-1 的 handle(pin 存活)时 acquire asset-2 触发驱逐 → asset-1 因 `pin.use_count()>1` **不被驱逐**、被驱逐的是某个无活 handle 的块;`evictCount()` 与 `residentBytes()` 反映正确;预算字节 = 该 asset 全部几何 blob `byteCount` 之和(多 blob 求和,非单 blob)。
- [x] **AC5 borrow-faceId 零物化**:`TriangleLease` 新静态工厂使 `view().faceIds.data()` 指向借入的连续 faceId 存储(mmap 侧=faceId blob 的 mmap 区间;Resident 侧=handle 的 `triangleFaceIds_` 内部缓冲),**无每次 acquire 的 O(N) 拷贝**;`ResidentSurfaceSource::acquire_triangles` 改造后不再物化(可由「faceId span.data() == handle 内部 vector.data()」断言证明);原 `TriangleLease::borrow` 路径与既有调用方行为不变。
- [x] **AC6 分层 + 签名零违反**:io/core 目录无新增 `vtk*` include;`IGeometrySource` 三纯虚未增减、`ReadLease.h` 既有签名未改、`SourceViews.h` `static_assert` 编译通过;新增 link 边方向合法无环(services→io PRIVATE 复用)。Release 全量 ctest 绿(现基线 + 本任务新增测试)。

> **✅ 完成(2026-06-30,3 commit:阶段1 borrow-faceId / 阶段2 MappedGeometrySource / 阶段3 acquireGeometrySource 工厂)。** 54/54 ctest 绿(新增 test_mapped_geometry_source + 扩 GeometryResourceManagerTest 3 例 + test_source_interface AC2 零拷贝断言);三轮假绿抽查(materialize→AC2 红 / 短路 verifyBlob→memo+篡改红 / 去 pin gating→pinned 红+段错)证测试 live。Source 1.0 公共签名零改动。E 的惰性取 source 前置就绪。

## Notes
- 复杂基座任务,`task.py start` 前补 `design.md`(架构 + 关键决策:① borrow-faceId 第二 keepalive 如何不破原 borrow;② `ResidentBlock` 泛化承载 voxel+geometry 的形态;③ 几何源 spec 如何从 `AssetRecord.blobs` 多 role 解析 + 预算求和;④ FullVerify 惰性 memo)与 `implement.md`(分阶段:borrow-faceId → mmap 源 → 工厂 → Resident 接线)。
- 依赖:M8a/M8b-1/scale-probe/M8b-2/M9a 全部已完成归档,Source 1.0 已冻。A 与父任务 B 可并行(A 是 io/services 纯增量基座,B 用现 `ResidentSurfaceSource` 即可喂)。
- 关联 memory:`m8b1-consumer-access-patterns`(消费者访问粒度=要整块连续视图)、`m8a-wholefile-sha-vs-streaming`(整文件 SHA 与流式冲突,FullVerify 取舍)、`evict-harness-pin-before-evict-race`(pin-before-evict 竞态,测试编排注意)、`xq-build-recipe`、`no-sideeffect-in-assert`、`ninja-target-incremental-fakegreen-trap`(增量编译核对 exe 时间戳)。
- 完成后:E(payload 去 vector 化)的「惰性取 source」前置就绪。
