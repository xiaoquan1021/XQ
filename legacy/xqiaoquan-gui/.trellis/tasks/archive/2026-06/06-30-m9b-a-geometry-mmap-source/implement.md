# M9b-A 实现计划 — 分阶段 + 验收门 + 回滚点

> 顺序原则:**先 core 的 borrow-faceId(最底层、独立可验、Resident 立即受益),再 io 的 mmap 源(依赖 borrow-faceId),再 services 工厂(依赖 mmap 源)**。每阶段独立编译 + 跑相关测试绿后再进下一阶段;全部完成跑 Release 全量 ctest + 假绿抽查。

## 前置:基线(阶段 0)
- 确认当前分支 Release 全量 ctest 绿(记录起步数,如 N/N)。
- grep 锁定改动面:`TriangleLease::borrow` 所有调用方(确认 borrow-faceId 新增不影响它们)、`ResidentSurfaceSource::acquire_triangles` 调用方、`acquireVoxelSource` 调用方(对称参考)。
- **回滚点 R0**:基线 commit 前干净树。

## 阶段 1:TriangleLease borrow-faceId + Resident 接线(core)
**目标 AC5(+ AC6 签名零违反)。** 最底层,独立可验。

1. `ReadLease.h`:`TriangleLease` 加私有成员 `std::shared_ptr<const void> faceIdKeepalive_;` + 新静态工厂 `borrow_borrowed_faceids(triKeepalive, triangles, faceIdKeepalive, faceIds_span)`(见 design 决策 1)。**既有 `borrow`/`empty`/`ownedFaceIds_`/`keepalive_` 一字不改。**
2. `XQTriangleSurfaceGeometryHandle`:加 `const std::vector<int>& triangleFaceIds() const`(返回内部 `triangleFaceIds_`,加不破)。
3. `ResidentSurfaceSource.cpp:37-52`:`acquire_triangles` 改走 `borrow_borrowed_faceids`(两 keepalive 都传 `handle_`),**删除 `:44-51` 的 O(N) 物化循环**。
4. 测试:新增/扩 `ReadLease` / `ResidentSurfaceSource` 单测——断言 `view().faceIds.data() == handle->triangleFaceIds().data()`(证零拷贝、零物化),且 faceId 值逐元素 == 源;原 `borrow` 物化路径单测保持绿(证不破)。

**阶段 1 验收**:相关单测绿;grep 确认 `ResidentSurfaceSource.cpp` 无 `triangleFaceId(i)` 物化循环;`static_assert` 编译通过。
**回滚点 R1**:阶段 1 commit。

## 阶段 2:MappedGeometrySource(io)
**目标 AC1/AC2。** 依赖阶段 1 的 borrow-faceId。

1. 新建 `src/io/source/MappedGeometrySource.{h,cpp}`,实现 `IGeometrySource`。构造收 blob spec(各 role path + elementCount + components + elementType)+ `IntegrityMode`(默认 FullVerify);对存在 role `map_file_readonly` + 校验 `size == count*comp*sizeof(elem)` + 元素类型/端序守门。
2. `verify_range` 逐 blob 惰性 memo(仿 `MappedVoxelSource.cpp:91-149`,每 blob 独立 `fullVerified_` + `IntegrityStats`);`SegmentedMerkle` 读路径并存(`MerkleSidecar` 现成),缺 sidecar 回落 FullVerify。
3. 三个 acquire 零拷贝 reinterpret + borrow(points/tets 用 `GeometryLease::borrow`;triangles 用阶段 1 的 `borrow_borrowed_faceids`,两 keepalive=tris/faceId 各自 mmap 块)。`static_assert(sizeof(Point3)==24)` 在 cast 点。缺 role → empty lease。
4. 加入 `CMakeLists.txt`(`src/io` 目标源列表,参 `MappedVoxelSource` 所在处)。
5. 测试 `test_mapped_geometry_source`:
   - 用 `XQProjectWriter`/`BlobStore` 落几个几何 blob(或直接 dump 已知字节)→ map → 逐元素 == 源(points 三分量 / tris / faceId / tets),AC1。
   - 缺 tets role → `acquire_tetrahedra().span().empty()`;0 字节 / size 不符 / 类型不符 → `valid()==false`,AC2。
   - FullVerify:首次 acquire 后 `stats().bytesHashed>0`,二次 acquire 不再增长(memo)。
   - 假绿抽查:篡改 blob 一字节,确认有断言能检出(校验不进 assert)。

**阶段 2 验收**:`test_mapped_geometry_source` 绿;io 目录无 `vtk*`。
**回滚点 R2**:阶段 2 commit。

## 阶段 3:acquireGeometrySource 工厂 + ResidentBlock 泛化(services)
**目标 AC3/AC4。** 依赖阶段 2。

1. `GeometryResourceManager.h`:`ResidentBlock` 加 `std::unique_ptr<IGeometrySource> geometrySource;`(与 `voxelSource` 互斥,见 design 决策 3);加 `GeometrySourceSpec` + `GeometrySourceHandle`(对称 `VoxelSourceHandle`)+ `acquireGeometrySource` 声明。fwd-declare `IGeometrySource`(头只见 core 抽象)。
2. `GeometryResourceManager.cpp`:`acquireGeometrySource` 实现——命中 bump LRU + 发新 pin(`++hitCount_` 不重映射);未命中经新 helper(仿 `voxelsRelPath`,按 role 解析多 `BufferRef.relPath`)构造 `MappedGeometrySource`、`valid()` 守门、`bytes=Σ byteCount`、resident + `evictToBudgetLocked`。include `io/source/MappedGeometrySource.h`(.cpp,PRIVATE 链已存在)。
3. `evictToBudgetLocked`/`setBudgetBytes`/`residentBytes`/计数器**不改逻辑**(已与源类型无关);确认 erase geometry 块时析构顺序正确(pin `use_count==1` → 源析构 → mmap 解映射)。
4. 测试 `test_geometry_resource_manager`(对称 voxel 测试):
   - 二次 acquire 同 assetId → `hitCount` +1 / `mapCount` 不变,AC3;未知/缺 blob/invalid → handle invalid。
   - 预算略小于两 asset 之和,持 asset-1 handle 时 acquire asset-2 → asset-1(pin>1)不被驱逐、回收无 handle 的块,`evictCount`/`residentBytes` 正确,AC4。预算用多 blob 求和验证。
   - **pin-before-evict 竞态**(memory):并发场景前台先 pin 再放行 evictor,防提前回收崩进程。

**阶段 3 验收**:`test_geometry_resource_manager` 绿;services→io PRIVATE 边无新环。
**回滚点 R3**:阶段 3 commit。

## 阶段 4:全量验收 + 分层核查
- **AC6 分层**:`rg -n 'vtk' src/io src/core`(本任务新增文件)= 0 命中;`IGeometrySource` 纯虚集未增减(`git diff` 核 `IGeometrySource.h`);`ReadLease.h` 既有签名未改(只增);`static_assert` 编译通过;link 边 `git diff CMakeLists.txt` 确认无新跨层环。
- **Release 全量 ctest**:比照 `xq-build-recipe`(vcvars64 + `CMAKE_PREFIX_PATH` + offscreen),全绿,新增 3 个测试计入。
- **假绿抽查**:篡改 mmap 几何源一处源数据 / 篡改一处 faceId,确认对应断言变红(`acquire_*`/`verify_range` 不进 assert,`no-sideeffect-in-assert`)。
- **增量编译核对**:用完整 `cmake --build <dir>` 而非单 `--target`,核对 exe 时间戳真变(`ninja-target-incremental-fakegreen-trap`)。

## 验证命令
```bash
# Release 配置 + 构建 + 全量 ctest(CRLF .bat,vcvars64 + CMAKE_PREFIX_PATH + offscreen)
# 比照 build_m9a.bat 复制改 build 目录名为 build_m9b_a;CRLF;run exe 需 vtk-9.3.0/bin 在 PATH
cmd //c "C:\\Users\\OCEAN\\Desktop\\XIAOQUAN\\XQ\\build_m9b_a.bat"

# 分层核查:io/core 无 vtk;新源/工厂无逐元素物化
rg -n 'vtk' XQ/src/io/source/MappedGeometrySource.h XQ/src/io/source/MappedGeometrySource.cpp
rg -n 'triangleFaceId\(' XQ/src/core/source/ResidentSurfaceSource.cpp   # 应 0 命中(已改 borrow)

# 签名零改动核查
git diff XQ/src/core/source/IGeometrySource.h XQ/src/core/source/SourceViews.h
git diff XQ/src/core/source/ReadLease.h   # 应只见新增,既有 borrow/empty 不变
```

## 回滚策略
- 各阶段独立 commit(R1/R2/R3),任一阶段验收失败 `git reset --hard` 回上一回滚点(destructive,需用户确认)。
- 阶段解耦:阶段 1(core borrow-faceId)独立交付价值(Resident 提速),即便 2/3 回滚仍可保留;阶段 2(io 源)不依赖 3;阶段 3 依赖 2。

## 子代理派发(可选)
- 阶段 0 的「`TriangleLease::borrow` / `acquireVoxelSource` 调用方全量定位」读代码可派子代理并行;实现改动主会话亲自做(跨层 + 连续编译验证)。
- 三个测试的「对称参考」(voxel 侧已有测试结构)可派子代理扫出范式;改动主会话做。

## 依赖与关联
- 依赖:M8a/M8b-1/scale-probe/M8b-2/M9a 全部已完成归档,Source 1.0 已冻。A 与父任务 B 可并行。
- 关联 memory:`m8b1-consumer-access-patterns`、`m8a-wholefile-sha-vs-streaming`、`evict-harness-pin-before-evict-race`、`xq-build-recipe`、`no-sideeffect-in-assert`、`ninja-target-incremental-fakegreen-trap`、`commit-check-gitignore-present`(commit 精确列文件,别 `-A`)。
- 完成后:E(payload 去 vector 化)的惰性取 source 前置就绪。
