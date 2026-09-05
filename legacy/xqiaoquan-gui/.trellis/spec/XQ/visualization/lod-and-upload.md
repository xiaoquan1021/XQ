# Surface LOD / 上传降量 / 渐进分块上传(visualization layer,M9b-B + M9b-C)

> 来源:`06-30-m9b-b-lod-decimation` + `06-30-m9b-c-progressive-upload`(prd/design/implement + 大规模 probe 实测)。
> 写 surface 渲染 / LOD / decimation / 渐进上传代码前先读本文。VTK 只在 visualization 私有实现,io/core 永无 `vtk*`。

---

## 组件与契约

- **`SurfaceLodBuilder`**(visualization,VTK 私有):输入 full `vtkPolyData`(level 0)+ 每三角 faceId span,出 3 级 `LodLevels`:
  - `level[0]` full — 恒等 cell→faceId(`CellFaceIdMap.valid=true`)。
  - `level[1]` medium — **分 faceId region 各自 `vtkQuadricDecimation` 再 append**(QuadricDecimation 不携带 cell data,故必须分区重建表);保 faceId,`valid=true`。
  - `level[2]` far — `vtkQuadricClustering`(改拓扑、`CopyCellDataOff`),平色,`valid=false`。
  - `buildSync` 确定性(测试 + async body);`buildAsync` 用 `std::async` + **先 deep-copy `full` + 拷 faceId span**(VTK 非线程安全 + 调用方 lease 可能立即释放,future 必须自持输入)。回填 mapper/actor 只能在主线程。
- **`RenderStats.uploadedPointCount`**(M9b-B 新增):实际上传级点数;LOD 关 / level 0 时 == `pointCount`。`pointCount` 恒报**源点数**,语义不可改(`test_scene_renderer` 断言依赖)。
- **`LodOptions`**:`enabled`(默认关 → M9a 等价)、`fixedLevel`(≥0 确定性取级)、`budgetTriangles`(CPU 侧选级)、`interactive`(交互窗口装 `vtkLODActor`)。

## 两条硬约束(踩坑沉淀)

1. **headless 取不了 `vtkLODActor` 的级**:`vtkLODActor` 选级靠 `vtkRenderer::AllocatedRenderTime`,由 interactor 的 DesiredUpdateRate 驱动;`renderOffscreenToRgba` 自建离屏窗口**无 interactor** → 恒走 full-res,降量不可观测、不可断言。
   - **结论**:CPU 侧 `selectLodLevel`(`fixedLevel`/`budgetTriangles`)是**唯一可信、可测的选级源**;`vtkLODActor` 只在真交互窗口(`XQRenderWidget`)做 still/motion 平滑,**不作验收测量点**。

2. **分区 decimate 会复制 region 间共享点 → 碎片 faceId 下 medium 反而更大**:`extractRegion` 对每个 faceId region 独立紧凑化点集,region 边界上的点被多份复制。当 region 极碎(每 region 三角极少、空间散布,如合成 `t%64`)时 medium 点数膨胀甚至超过 full。
   - **诚实回退**:`buildSync` 在 `level[1].pointCount >= level[0].pointCount` 时把 medium 回退成 full,保证 `uploadedPointCount ≤ full`。绝不把「比 full 还大的级」当 LOD 呈现。
   - 连续解剖面 region(每 region 大片三角)下 medium 正常降量,这是真实使用形态。

## 架构边界:LOD 砍上传量,砍不掉 host 峰值

大规模 probe 实测(20M 三角,各上传模式**独立进程**——`PeakWorkingSet` 进程内单调,同进程比不出):

| 模式 | uploadedPointCount | 进程峰值工作集 |
|------|-------------------|---------------|
| full(M9a) | 10,002,000(100%) | 3422 MiB |
| medium | 碎片回退到 full | 3328 MiB |
| far | 960(0.0%) | 3102 MiB(**−9%**) |

- **根因**:`build_surface` 总是先在 host **完整物化**整网 vtkPolyData(handle 向量 + 完整 polyData ≈ 峰值主导)。`SurfaceLodBuilder` 在其**下游**跑,只能减 GPU 上传量,减不掉已经发生的 host 物化。probe 在**不 Render**(normals/上传未执行)下 full 已达 3422 MiB,坐实峰值主导是 host 完整物化,不是上传或 normals deep copy。
- **要真降 host 峰值**:decimation 必须在完整 host 物化**之前**、从 `IGeometrySource` 流式/分区降采,超出 `build_surface` 现有契约,**移交 A(几何 mmap 源)或后续「源头流式降采」任务**。
- **本层 LOD 的真实价值**:GPU 上传量下降(far 三个数量级)+ `vtkLODActor` 交互 still/motion 平滑。这是 M9b-B 收口的边界,别再把「host 峰值减半」算到下游 LOD 头上。

## 验收

- LOD 默认关零退化(`test_scene_renderer` 全绿,M9a 等价);`test_surface_lod` 覆盖固定级降量、faceId 保级不串色、async==sync 等价 + 输入销毁后存活、交互 actor 装配+离屏非黑。
- 副作用调用(decimate/acquire)不进 assert(验收铁律);假绿抽查篡改 region faceId → `test_surface_lod` 变红。

---

## 渐进 / 分块上传(M9b-C)

VTK 无单一 `vtkPolyData` 高效 append,故大几何「边建边显」靠**多 actor 分块**:按 cell 区间切块,每块独立构 `vtkPolyData`/actor 逐块 `AddActor`,首块尽快可见。

- **入口**:`XQSceneRenderer::addSurfaceProgressive` / `addVolumeMeshProgressive`(收 `const IGeometrySource&` + `ChunkUploadSpec` + 可选逐块进度回调),与一次性 `addSurface`/`addVolumeMesh` 并存,旧入口语义不动。
- **`ChunkUploadSpec`**:`maxCellsPerChunk`(默认 1<<20)+ 内嵌 `LodOptions lod`(每块 actor 的 LOD,默认关 → 普通 actor)。
- **`RenderStats` 纯追加**:`chunkCount`(规划块数)/`completedChunkCount`(已上传块数);`pointCount` 仍报**源总点数**,`actorCount` 报新增块 actor 数。
- **全局 faceId LUT range**:渐进上传前算一次全几何 faceId range,所有块 mapper 共用 → 跨块着色无断层。
- **每块 copy-on-upload**:`build_cell_chunk` 收集本块 cell 引用的点、重映射为局部索引、拷进 VTK 自有数组;`remap` 是复用 scratch(只重置本块触碰的项,免 O(N) 清零);lease 不跨块借给 actor(无悬垂)。
- **interactive**:每块按 `spec.lod.interactive` 走 `mountLodActor` 挂 `vtkLODActor`;真交互窗口由 `QVTKOpenGLNativeWidget` 默认交互器驱动 `DesiredUpdateRate`(同 B 的约束,headless 不可测切级)。

### 第三条硬约束:段偏移正确性在「分块划分」处验,不在像素处验

cell→faceId 段偏移正确的前提是 `plan_chunks` 把 `[0,cellCount)` **精确铺满**(连续、无缝、无叠、每 cell 恰覆盖一次)。

- **不能靠渲染图像验**:丢/重几个 cell 只是亚像素空洞 / 微量重绘,渐进 vs 一次性的覆盖率比对(15% 容差,吸收每块 normal AutoOrient 差异)**吞得掉**。实测一个 `begin+=1` 丢首 cell 的篡改,渐进测试照样绿——典型假绿。
- **根治**:`plan_chunks` + `ChunkRange` 抽到 **VTK-free 头 `visualization/ChunkPlan.h`**,`test_chunk_plan.cpp`(header-only 无 VTK 链接)直接断言划分不变量(首块 begin==0、相邻 end==下块 begin、末块 end==cellCount、每块非空且 ≤max、每 cell 覆盖恰一次、块数 == ceil 除)。`test_chunk_faceid_segment_offset` 再用同一 `plan_chunks` 复现 renderer 段偏移并逐 cell 核 faceId。
- **假绿抽查实测**:`ChunkPlan.h` 注入 `{begin+1, end}` → `test_chunk_plan` + AC4 测试**同时转红**,还原 → 57/57 绿。
- **教训**:渐进/分块这类「划分对了渲染就对、划分错了渲染只是亚像素偏差」的特性,验收点必须落在**划分函数的离散不变量**上,不能寄望渲染快照(容差天然吞掉小划分错误 → 假绿)。

## Scenario: renderer connectivity bounds gate

### 1. Scope / Trigger
- Trigger: any code in `XQSceneRenderer` that uploads surface triangles or tet
  cells to VTK, including one-shot and progressive/chunked paths.
- The renderer is the final boundary before VTK and before progressive chunk
  compaction reads `allPoints[g]`; it must not trust connectivity from resident
  handles, mapped sources, or lazy project data.

### 2. Signatures
- Public signatures stay unchanged:
  `addSurface`, `addVolumeMesh`, `addSurfaceProgressive`,
  `addVolumeMeshProgressive`, `RenderStats`, and `ChunkUploadSpec`.
- Internal helper shape:
  `connectivity_in_bounds(const int* flatConn, size_t cellCount,
  int vertsPerCell, size_t pointCount) -> bool`.

### 3. Contracts
- Every vertex index in every uploaded cell must satisfy
  `0 <= index < acquiredPointSpan.size()`.
- Validation must run before VTK cell arrays are created and before any
  progressive chunk actor is added.
- On invalid connectivity, the add call returns `ok=false`, leaves actor count
  unchanged, reports `chunkCount == 0` and `completedChunkCount == 0` for
  progressive calls, and must not call `onChunk`.
- The renderer must reject the whole add call; it must not clamp, skip, or
  partially upload bad cells.
- Valid geometry keeps existing stats semantics: `pointCount` reports source
  point count, and progressive actor/chunk counts still match planned chunks.

### 4. Validation & Error Matrix
- `flatConn == nullptr` with `cellCount > 0` -> invalid add.
- `vertsPerCell <= 0` -> invalid add.
- `cellCount * vertsPerCell` overflow -> invalid add.
- Any vertex index `< 0` -> invalid add.
- Any vertex index `>= acquiredPointSpan.size()` -> invalid add.
- Empty geometry remains governed by existing meta checks and returns `ok=false`
  before connectivity validation.

### 5. Good/Base/Bad Cases
- Good: valid grid surface and tet row render one-shot and progressive with
  unchanged actor/count behavior.
- Base: surface/tet handle constructors may contain invalid indices until
  `is_valid()` is queried; renderer still validates before upload.
- Bad: progressive surface has triangle `{0, 1, pointCount}`; old code reads
  `allPoints[pointCount]` during chunk compaction.

### 6. Tests Required
- `test_scene_renderer`: one-shot invalid surface and invalid tet return
  `ok=false` and add no actor.
- `test_scene_renderer_progressive`: invalid surface/tet source returns
  `ok=false`, adds no actor, reports no chunks, and does not call `onChunk`.
- Existing `test_surface_lod`, `test_scene_renderer_progressive`, and
  `test_chunk_plan` continue to pass for valid inputs.
- Final gate: Release full `ctest`.

### 7. Wrong vs Correct
#### Wrong
```cpp
localPoints.push_back(allPoints[static_cast<std::size_t>(g)]);
```

#### Correct
```cpp
if (!connectivity_in_bounds(flatConn, cellCount, vertsPerCell, pts.size())) {
    return {false, actorCount(), 0, 0, 0, 0};
}
```