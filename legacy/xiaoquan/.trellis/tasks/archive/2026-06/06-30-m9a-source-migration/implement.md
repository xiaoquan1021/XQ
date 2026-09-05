# M9a 实现计划 — 分阶段执行 + 验收门 + 回滚点

> 顺序原则:**先 renderer 上传核(瓶颈 + 不退化核心、独立可验),再网格服务/TetGen(接口重塑、ripple 面大),最后 io writer(flatten 等价替换)**。每阶段独立编译 + 跑相关测试绿后再进下一阶段;全部完成跑 Release 全量 ctest。

## 前置:不退化基线(阶段 0)
- 确认当前分支 Release 全量 ctest 绿(53/53 起步)。
- 对关键场景留迁移前基线:test_scene_renderer 渲染输出、VolumeMeshServiceTest / test_tetgen_volume_mesh 网格元素、project round-trip。基线用于阶段末等价对比(像素 hash 或元素级 dump,具体随测试现有断言形态)。
- **回滚点 R0**:基线 commit 前的干净树。

## 阶段 1:renderer 上传核重写(visualization)
**目标 AC1/AC2/AC3/AC8。** 独立于网格服务,先做。

1. `build_surface` 改签名为 `build_surface(const IGeometrySource&)`(或保留 handle 入参、内部包 ResidentSurfaceSource——优先改签名,入口 `addSurface` 包源)。内部:
   - points:`acquire_points().span()` → `vtkDoubleArray`(SetNumberOfTuples + memcpy,`static_assert(sizeof(Point3)==24)`)→ `vtkPoints::SetData`。
   - triangles:`acquire_triangles().view().triangles` → int32 connectivity 数组 + 生成 offsets(`i*3`)→ `vtkCellArray::SetData` → `SetPolys`。
   - faceId:`view().faceIds` 并行 span → `vtkFloatArray`(批量转 float)→ `GetCellData()->SetScalars`。
   - **删除逐点 `SetPoint`、逐三角 `InsertNextCell`/`triangleFaceId(i)`**。
2. `build_volume` 同法:points 批量 + tets(`acquire_tetrahedra().span()` int32 → offsets `i*4` + connectivity)→ `SetCells(VTK_TETRA, cells)`。**删除 per-tet `vtkNew<vtkTetra>` + `Allocate` 循环**。
3. `addSurface`/`addVolumeMesh`:入口把 handle 包 `ResidentSurfaceSource`/`ResidentTetSource`(或改签名收 `const IGeometrySource&`,调用方 `XQMainWindow.cpp:280/288/290` 同步包源)。空几何 guard 改经 `meta()`/`span().empty()`。渲染管线(normals/lut/geometryFilter/extractEdges)**不动**。
4. **风险先行(R1)**:先只改 surface points 一项跑通 test_scene_renderer,确认 VTK 9.3 `SetData`/`SetArray` 32 位签名正确,再铺开其余。

**阶段 1 验收**:test_scene_renderer 绿且输出与基线等价;grep 确认 `build_surface`/`build_volume` 无 `SetPoint(`/`InsertNextCell`/`vtkNew<vtkTetra>`。
**回滚点 R1**:阶段 1 commit。

## 阶段 2:ITetMesher 重塑 + TetGen + 网格服务(services/adapters)
**目标 AC4/AC5/AC6(网格服务部分)。**

1. `ITetMesher.h`:`tetrahedralize` 改吃 `const IGeometrySource&`;include 改为 `core/source/IGeometrySource.h`(去 surface handle fwd)。
2. `TetGenTetMesher::tetrahedralize`:
   - pointlist:`acquire_points().span().data()` → `reinterpret_cast<const REAL*>`(`static_assert(sizeof(Point3)==24)` 在 cast 点)批量填 `in.pointlist`(REAL=double)。
   - facets + facetmarkerlist:`acquire_triangles().view()` 的 triangles span 逐 facet 构造(TetGen 要自己的 facet 对象,逐 facet 保留),facetmarker 从 `faceIds` 连续 span 批量填。
   - bbox(resolveMaxVolume):用 points span 批量。
   - 输出侧(读 TetGen `out.*` 建 XQ handle)不变。
3. `TetGenThenMmg`:转发新签名(orchestrator,本体不碰几何源,只传递)。
4. `VolumeMeshService::buildVolumeMesh`:入参 surface handle 包 `ResidentSurfaceSource` 传给注入 mesher;`isClosedManifold`/star-fallback/faceId-tagging 的逐元素循环改经 Source span(或在服务内 acquire 一次复用)。`summarizeTetQuality` 读输出 tet handle 处可包 `ResidentTetSource` 或保持(输出 handle 非注入源,低优先,等价即可)。
5. `SurfaceMeshService`:输入 `*model.triangleGeometry()` 包源后取 span;O(faces×triangles) faceId 匹配(`SurfaceMeshService.cpp:62`)用 faceId 并行 span。
6. `ModelingService`:`extractBoundaryLoops`/`loopCentroid` 等逐元素循环改经 Source span(输入来自 `XQSurfaceModel`,包源)。
7. **mock/stub 全补**:`MeshingController`/`XQMainWindow` 装配点、`test_tetgen_volume_mesh`/`test_mmg_volume_mesh`/`VolumeMeshServiceTest` 的直调或 mock mesher 一起改签名(铁律)。

**阶段 2 验收**:test_tetgen_volume_mesh / test_mmg_volume_mesh / VolumeMeshServiceTest 绿;TetGen 四面体化顶点/连接/faceId 与基线守恒(参 `tetgen-plc-volume-mesh`)。
**回滚点 R2**:阶段 2 commit。

## 阶段 3:io writer flatten 迁移(io/project)
**目标 AC6(writer 部分)。**

1. `XQProjectWriter` 的 `put_triangle_geometry`(:720-760)/volume 分支(:798-828):points/triangles/tets flatten 循环改经 `acquire_*().span()` 批量取 + bulk 拷进 `std::vector<double>/<int>`;faceId 用 `acquire_triangles().view().faceIds` 并行 span(不再逐元素 `triangleFaceId(i)`)。
2. 入口把 geom handle 包 `ResidentSurfaceSource`/`ResidentTetSource`。
3. `BlobStore::put` 仍吃 flat vector(不改),只是填充方式从逐元素 push 改批量拷贝。

**阶段 3 验收**:project round-trip 测试绿,写出字节与基线一致(content-addressed SHA 不变)。
**回滚点 R3**:阶段 3 commit。

## 阶段 4:全量验收 + 分层核查
- **AC7 分层**:grep io/core 目录无新增 `vtk` include;Source 头 static_assert 编译通过;link 边方向核对(无环)。
- **AC8 生命周期**:加/确认测试——`addSurface`/`addVolumeMesh` 后释放源 handle 再 render 正确(copy-on-upload 应通过)。
- **Release 全量 ctest**(`build_m8b2.bat` 同款配方,或新 build 目录):全绿,新增本里程碑测试计入。
- **假绿抽查**:篡改一处源数据确认对应断言变红(`acquire_*` 不进 assert)。

## 验证命令
```bash
# Release 配置 + 构建 + 全量 ctest(CRLF .bat,vcvars64 + CMAKE_PREFIX_PATH + offscreen)
cmd //c "C:\\Users\\OCEAN\\Desktop\\XIAOQUAN\\XQ\\build_m9a.bat"
# 上传核 grep 核查
rg -n 'SetPoint\(|InsertNextCell|vtkNew<vtkTetra>' XQ/src/visualization/XQSceneRenderer.cpp
```
(build_m9a.bat 比照 build_m8b2.bat 复制,改 build 目录名;CRLF;run exe 需 vtk-9.3.0/bin 在 PATH。)

## 回滚策略
- 各阶段独立 commit(R1/R2/R3),任一阶段验收失败 `git reset --hard` 回上一回滚点(需用户确认,destructive)。
- 阶段间无强耦合:阶段 1(renderer)与阶段 2/3 解耦,可独立回滚不影响已完成阶段。

## 子代理派发(可选)
- 阶段 1 与阶段 2 的"基线 dump"读代码可并行派子代理;实现改动主会话亲自做(跨文件接口 ripple,需连续编译验证)。
- 阶段 2 的 mock/stub 全量定位(找所有 mesher 装配/直调点)可派子代理扫,但改动主会话做。

## 依赖与关联
- 依赖:M8b-1 / scale-probe / M8b-2(均已完成归档)。
- 关联 memory:`m8b1-consumer-access-patterns`、`xq-build-recipe`、`tetgen-plc-volume-mesh`、`xq-surface-winding-not-consistent`、`no-sideeffect-in-assert`、`ninja-target-incremental-fakegreen-trap`(增量编译核对 exe 时间戳)。
- 完成后:Source 1.0 候选可冻(消费者存活证据齐)。
