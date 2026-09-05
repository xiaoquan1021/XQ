# M9b-C 设计 — 渐进/分块上传(多 actor 边建边显)

> 依据实读:`XQSceneRenderer.cpp`(build_surface 189-218 / build_cells 166-187 / upload_points 148-159 / addSurface 373-425 / addVolumeMesh 427-468)、`XQSceneRenderer.h`(RenderStats 21-25)、`XQRenderWidget.cpp`(render 41-47 / mount 27-31)、`IGeometrySource.h:19-26`、`ReadLease.h`(TriangleLease 124-158)、`test_scene_renderer.cpp`(headless 范式 75-107)。

## 架构总览

设计前提:**VTK 9.3 无单 `vtkPolyData` 高效增量 append**,唯一"边建边显"手段 = 多 actor。renderer 本就是多 actor 场景(`Impl` 持一个 `vtkRenderer`,各 add* 累加 actor)。C 把"一个大几何 → 一个大 actor"改为"一个大几何 → N 个块 actor,逐块构建 + 加进同一 `vtkRenderer`"。

```
源(const IGeometrySource&)               ChunkPlan(按阈值切 cell 区间)
  acquire_points() ─ ReadSpan<Point3>          块0:[0,        T)
  acquire_triangles() ─ tris + faceIds  ──►    块1:[T,      2T)   ← 段偏移 = 块基址
  (全几何连续 span,一次 acquire)               …
                                               块k:[kT, triCount)
                       │ 逐块
                       ▼
   for each chunk c:                          ┌─────────────────────────────┐
     gather 本块点 + 局部重映射 ──► copy ──►   │ vtkPolyData_c (自有数组)      │
     本块 cells(局部索引)──► copy ──►         │  points_c / polys_c / faceId_c│
     本块 faceId 段 ──► copy(global LUT)──►   └──────────────┬────────────────┘
                                                              ▼
                                              vtkPolyDataNormals → mapper(共用全局 LUT range)
                                                              ▼
                                              vtkLODActor_c (B) ──► renderer_->AddActor
                                                              ▼
                                              [首块] render 可见 + 进度回调(c, stats)
                                                              ▼ lease 已释放(copy-on-upload)
```

`XQRenderWidget`(交互窗口)挂同一个 `vtkRenderer`(`cpp:27-31`),其交互器的 `DesiredUpdateRate` 驱动全部块 actor 的 LOD 切级(still/interactive)。

---

## 决策 1:分块粒度 = 按 cell(三角/tet)数阈值,块基址即 cell→faceId 段偏移

### 背景
源经 `acquire_triangles()` 一次吐出全几何连续 `ReadSpan<SourceTriangle>` + 并行 `faceIds` span(`ResidentSurfaceSource.cpp:37-52`)。分块是 **renderer 侧对该连续 span 的区间切分**,不需要源支持 sub-range acquire。

### 取舍
- **按点数切**:三角跨块引用点会撕裂(一个三角的 3 个点落不同块),需额外缝合,复杂。
- **按三角/tet 数切(选定)**:每块 = 连续 cell 区间 `[begin, end)`,块内三角自包含。`begin` 即该块的 **cell→faceId 段基址**:块内局部 cell 下标 j → 全局 faceId = `faceIds[begin + j]`。天然满足父级"显式 cell→faceId 映射"契约(不靠全局下标相等)。
- 与 A 的 acquire 粒度对齐:A 当前吐全几何 span,C 的分块是其上的纯区间视图;**未来 A 支持 sub-range acquire 时,ChunkPlan 的区间可直接对齐 A 的 acquire-block 边界**(设计预留,本期不依赖)。

### 结论
`ChunkPlan` 按 `ChunkUploadSpec.maxCellsPerChunk` 把 `[0, triangleCount)`(或 `[0, tetCount)`)切成等长区间(末块取余)。默认阈值与 B 的 LOD 决策对齐(B 落地后取其建议值,临时默认如 ~1M cells/块,待 B 量化)。每块记 `{ begin, end }`,`begin` = 段偏移。

---

## 决策 2:每块点集重映射为局部索引(紧凑自包含),copy-on-upload

### 背景
块内三角的点索引是**全局**的(指向源全量 points)。两条路:
- **(a) 每块上传全量点 + 本块三角子集**:点索引保持全局、无重映射,但每块 `vtkPolyData` 持全部点 → N 块 × 全量点 = N 倍点内存,违背"降峰值"初衷。
- **(b) 每块只收本块三角引用的点、重映射为局部索引(选定)**:一趟扫本块三角,建 `global→local` 映射(`vector<int>` 大小 = 源点数,初值 -1,首见即分配 local 序号),gather 出本块点子集 copy 进 `vtkDoubleArray`,三角索引改写为 local。每块 `vtkPolyData` 紧凑自包含。

### 取舍
- (b) 的代价:每块一趟 O(本块三角×3) 重映射 + 一个 O(源点数) 的 remap buffer(可跨块复用、每块 reset 命中项)。相对一次性整块,这是分块换来的额外 CPU,但单块小、与"首块快可见"目标一致,且峰值点内存 = 单块而非全量。
- copy-on-upload(承接 M9a 决策 1):本块点/cells/faceId 全部 `std::memcpy` / 批量拷进 VTK 自有数组(`upload_points` 同款 `cpp:148-159`,`build_cells` 同款 `cpp:166-187`),lease 在本块 `build` 返回即释放。actor 不持 source 指针 → 无悬垂(AC6)。

### 结论
选 (b)。新增内部 `build_surface_chunk(source, chunkRange, globalFaceRange, remapScratch)`:复用 `upload_points`/`build_cells` 的批量拷贝原语,只是喂"本块子集"。faceId 段 = `faceIds.subspan(begin, end-begin)`,批量转 float 灌 `vtkFloatArray`。

---

## 决策 3:全局 faceId LUT range(跨块颜色一致)

### 背景
现 `addSurface` 用本 polyData 的 faceId `GetRange` 设 LUT `TableRange`/`ScalarRange`(`cpp:403-416`)。分块后每块 faceId 子集 range 不同 → 同一 faceId 在不同块映射到不同颜色 → 颜色断层。

### 结论
渐进上传**起始**先算一次全几何 faceId range(扫 `acquire_triangles().view().faceIds` 全量 `GetRange`,或源 meta 若已带 range),作为 `globalFaceRange` 传给每块 mapper 的 `SetTableRange`/`SetScalarRange`。所有块 LUT 配置一致(色相 0.55→0.0 不变,`cpp:407`)。法线管线 `vtkPolyDataNormals`(AutoOrient+Consistency,`cpp:397-401`)每块独立保留(winding 非全局一致,memory `xq-surface-winding-not-consistent`);分块在块内自洽,块间法线方向由各自 AutoOrient 决定,视觉等价整块(AC2 以"非黑 + 覆盖一致"核,不强求逐像素 bit 等)。

---

## 决策 4:渐进入口 + 进度回调(headless 可验)

### 背景
header VTK-free,headless 经 `RenderStats` + `renderOffscreenToRgba` 验(`test_scene_renderer.cpp:75-107`)。"边建边显"需在离屏证明"首块早于整块"。

### 取舍
- **逐块返回多个 stats**:入口返回 `vector<RenderStats>`,调用方自己驱动——但与现 `add*` 单返回不一致。
- **进度回调(选定)**:入口签名
  ```cpp
  using ChunkProgressFn = std::function<void(const RenderStats& cumulative)>;
  RenderStats addSurfaceProgressive(const IGeometrySource& src,
                                    const ChunkUploadSpec& spec,
                                    const ChunkProgressFn& onChunk = {});
  ```
  每块 `AddActor` 后填 `cumulative{ ok, actorCount=已加块数, pointCount=源总点数, chunkCount=N, completedChunkCount=c+1 }` 调 `onChunk`,最后返回末次 cumulative。
  - headless(AC1):回调里记首次触发时 `completedChunkCount==1 && chunkCount>1 && actorCount>=1`,且此刻可 `renderOffscreenToRgba` 得非黑 → 证"首块可见早于整块"。
  - 真实窗口:`onChunk` 里调 `XQRenderWidget::render()`(`cpp:41-47`)刷新 + 让 UI 显进度。

### 结论
选进度回调。`std::function` 在 VTK-free 头可用(`<functional>`)。`ChunkUploadSpec`(VTK-free struct,放 `XQSceneRenderer.h`):`{ std::size_t maxCellsPerChunk; /* 预留 LOD 级提示,对齐 B */ }`。

---

## 决策 5:RenderStats 纯追加字段(多 actor stats 语义)

### 结论
`XQSceneRenderer.h:21-25` `RenderStats` 追加:
```cpp
struct RenderStats {
    bool ok = false;
    int actorCount = 0;          // 旧义:本次 add 后场景 actor 计数 → 渐进下=本次新增块 actor 数
    long long pointCount = 0;    // 旧义不变:源总点数(父级硬约束)
    int chunkCount = 0;          // 新增:本次渐进的总块数(非渐进 add 为 0)
    int completedChunkCount = 0; // 新增:已成功上传的块数
};
```
- `pointCount` = 源总点数(`source.meta().pointCount`),**不**因分块只算单块点(父级硬约束、headless 可断言、AC3)。
- `actorCount`:渐进入口起始可先 `clear()` 或在已有场景上累加——语义定为"本次渐进新增的块 actor 数"(= `chunkCount`,成功时)。设计取**渐进入口不隐式 clear**(与现 add* 一致,clear 由调用方显式),`actorCount` 报渐进结束时场景总 actor 数(与现 `addSurface` 返回 `actorCount()` 一致语义),`chunkCount`/`completedChunkCount` 专表分块进度。
- 任一块失败:停止后续块,`ok=false`,`completedChunkCount`=已成功数,`chunkCount`=计划数。
- 非渐进旧入口(`addSurface` 等)`chunkCount=completedChunkCount=0`,旧测试断言不受影响(新字段默认 0)。

---

## 决策 6:still/interactive 切级 = 交互器 DesiredUpdateRate 驱动 B 的 LOD actor

### 背景
B 把块 actor 做成 `vtkLODActor`(多级低模)。VTK 的 LOD 选择由 render 时的 `AllocatedRenderTime` / 交互器 `DesiredUpdateRate` 自动驱动:交互中 `DesiredUpdateRate` 高(目标帧率高)→ 选低模;静止 `StillUpdateRate` 低 → 选高模。

### 结论
C 在 `XQRenderWidget`(挂 `vtkRenderer` 的同一 widget,`cpp:27-31`)接入交互器:`renderWindow()->GetInteractor()->SetDesiredUpdateRate(...)` / `SetStillUpdateRate(...)`,或监听交互 Start/End 事件切。作用于全部块 actor(它们共享一个 `vtkRenderer`)。
- headless 不可测真实交互(无 GL 上下文驱动帧率)。AC5 以可核证据替代:① 块 actor 均为 B 的 LOD actor 类型(渐进路径产出 LOD actor 而非裸 `vtkActor`);② `DesiredUpdateRate`/`StillUpdateRate` 接线在 `XQRenderWidget` 存在。真实切级手动验证记录。
- B 未落地前,C 先以裸 `vtkActor` 跑通分块骨架(AC1-AC4/AC6/AC7),LOD actor 替换与切级接线(AC5)在 B 合入后补。

---

## 数据流(surface 渐进,单块)

1. 入口 `addSurfaceProgressive(src, spec, onChunk)`:`src.meta()` 取 `pointCount`/`triangleCount`;`triangleCount==0` → `{false,...}`。
2. 一次 `acquire_triangles()` 取全量 `tris` + `faceIds` span;算 `globalFaceRange`。`ChunkPlan` 按 `spec.maxCellsPerChunk` 切区间。
3. 一次 `acquire_points()` 取全量 points span(块内 gather 用;copy-on-upload 后 lease 释放,故全程一次 acquire,生命周期在入口内 RAII)。
4. for c in chunks:
   a. `build_surface_chunk`:remap 本块点(决策 2)→ `vtkPoints`(自有);本块三角(局部索引)→ `build_cells` 风格 `vtkCellArray`(自有);本块 faceId 段 → `vtkFloatArray`(自有,转 float)。
   b. `vtkPolyDataNormals`(AutoOrient+Consistency) → `vtkPolyDataMapper`(LUT,`globalFaceRange`) → LOD actor(B) → `renderer_->AddActor`。
   c. 填 `cumulative` 调 `onChunk`;首块后场景即可 render 可见。
5. 返回末次 `cumulative`。所有 lease 在步骤结束(或各块 build 后)释放,actor 持 VTK 自有数组(无悬垂)。

tet 通路(`addVolumeMeshProgressive`)同骨架:`acquire_tetrahedra()` 切 tet 区间,`build_volume` 风格(`SetCells(VTK_TETRA, ...)`,`cpp:227-234`)+ `vtkGeometryFilter`→边线管线(`cpp:443-457`)每块独立。tet 无 faceId 着色,跳过 LUT 段。

---

## 分层与依赖
- 渐进/分块全在 visualization(`XQSceneRenderer.cpp` 新内部函数 + 新公共入口);`ChunkUploadSpec`/`RenderStats` 新字段在 VTK-free 头 `XQSceneRenderer.h`。
- 消费 `const IGeometrySource&`(core 抽象),无新增跨层边;io/core 无 `vtk*`。
- Source 公共签名零改动(`IGeometrySource.h:19-26` 不动,`static_assert(sizeof(Point3)==24)` 成立)。
- 依赖 B 的 LOD actor 类型与 `DesiredUpdateRate` 约定;B 未落地段以裸 actor 占位,接口预留。

## 风险
- **R1 重映射成本**:每块 O(源点数) remap buffer + O(本块三角) 扫。缓解:remap buffer 跨块复用、只 reset 命中项(记本块用到的 global 索引列表);先在 surface 打透量化单块构建耗时,确认"首块可见"目标达成。
- **R2 跨块法线/颜色不连续**:法线各块独立 AutoOrient 可能块间方向不一致(winding 非全局一致),颜色靠 `globalFaceRange` 统一已解决;法线视觉差异以"可识别同一几何"为验收线(AC2),不追逐像素 bit 等。
- **R3 actorCount 语义歧义**:渐进不隐式 clear,场景已有 actor 时计数叠加。决策 5 已定义为"渐进结束时场景总 actor 数 + chunkCount/completedChunkCount 表进度";测试用 `clear()` 后干净场景核 `actorCount==chunkCount`。
- **R4 依赖 B 未就绪**:C 先以裸 `vtkActor` 跑通分块骨架(AC1-4/6/7),LOD/切级(AC5)待 B 合入补。回滚点按阶段切分,B 缺位不阻塞 C 主体。
- **R5 VTK 对象线程安全**:本期主线程逐块(回调让出),不跨线程上传;后台线程化显式后置(VTK 非线程安全)。
