# M9b-B 设计 — LOD/降采样上传(单 actor 多级 + 离线 decimation + CPU 选级)

> 依据实读:`XQSceneRenderer.cpp:189-218`(build_surface)、`:373-425`(addSurface)、`:595-667`(renderOffscreenToRgba 无 interactor)、`XQSceneRenderer.h:21-25`(RenderStats)、`CMakeLists.txt:67-84,104-107,109-127`(VTK COMPONENTS / autoinit / xq_visualization)、VTK 9.3 头与 `VTK::RenderingLOD` 模块存在性已实编核实。

## 架构总览

只动 surface 上传链,volume/path/image/mask 路径不碰。在 `XQSceneRenderer::Impl` 与 `build_surface` 之间插入一个 **LOD 装配层**,decimation 由后台线程算,主线程只装配 actor + CPU 侧选级。

```
addSurface(handle [, LodOptions])
   │  (ResidentSurfaceSource, copy-on-upload —— 不改 M9a 借用模型)
   ▼
build_surface(source)  ──►  full vtkPolyData (level 0, faceId 恒等映射)        [现状,保留]
   │
   ▼ (LodOptions.enabled)
SurfaceLodBuilder (visualization,新)
   │   后台线程 std::async:
   │     · 中模 = 分 faceId region 各自 vtkQuadricDecimation → 合并 → 重建 cell→faceId 表(保 faceId)
   │     · 远景 = vtkQuadricClustering(CopyCellData off,改拓扑,无 faceId,平色)
   ▼
LodLevels { level[0]=full, level[1]=medium(faceId), level[2]=far(no faceId) }
            每级随附 CellFaceIdMap(恒等 / region重建 / invalid)
   │
   ├── 交互路径:vtkLODActor.AddLODMapper(各级 mapper),DesiredUpdateRate 驱动 still/motion
   └── headless/固定级路径:直接取 selectedLevel 的 mapper 建普通 vtkActor(确定性)
   │
   ▼
RenderStats { ok, actorCount, pointCount=源点数(不变), uploadedPointCount=选中级点数(新) }
```

新增组件(全在 visualization,VTK 私有):
- `SurfaceLodBuilder`(`.h/.cpp`):输入一个 full `vtkPolyData`(或 `const IGeometrySource&`),输出 `LodLevels`;decimation 在后台线程。
- `CellFaceIdMap`:`std::vector<int>`(输出 cellId → faceId)+ `bool valid`。full 级 valid 且 = 原 faceId span;far 级 invalid。
- `LodBudget` / `LodOptions`:CPU 侧选级输入(目标最大上传三角数/点数 + 是否启用 + 固定级覆盖)。
- `LodLevelSelector`:纯函数,`(LodBudget, LodLevels.meta) → levelIndex`。

---

## 决策 1:LOD 实现 = vtkLODActor(交互)+ 固定级直挂(headless),不只靠 vtkLODActor 自动取级

### 背景(实测)
`vtkLODActor` 的级选择依赖 `vtkRenderer::AllocatedRenderTime`,后者由 interactor 的 `DesiredUpdateRate`/`StillUpdateRate` 在交互/静止时设置。`renderOffscreenToRgba`(`:595-667`)自建 throwaway 离屏窗口、**无 interactor**,AllocatedRenderTime 不被驱动 → vtkLODActor 在 headless 下恒走 full-res,**LOD 降量在离屏 ctest 不可观测、不可断言**。

### 取舍
- **A 纯 vtkLODActor + 测试塞 interactor + 设 DesiredUpdateRate**:headless 起 interactor 不稳定(无 GL 事件循环),取级时机依赖内部估时启发式,**不可确定性复现**,违验收铁律。
- **B(选定)双路径**:交互窗口(`XQRenderWidget`)用 `vtkLODActor`(DesiredUpdateRate 驱动 still/motion,这是 vtkLODActor 的正道);headless/测试/CPU 预算选级走 **固定级覆盖入口** —— 由 `LodLevelSelector` 在 CPU 侧定 levelIndex,直接取该级 mapper 建普通 `vtkActor` 渲染。`uploadedPointCount` 报该级点数,确定性可断言。

### 结论
选 **B**。CPU 侧选级(父任务"VTK 无 byte-budget API,只能 CPU 控量"的直接落地)= 单一可信级选择源;vtkLODActor 仅在真交互窗口做 still/motion 平滑,不作为验收测量点。`forceLodLevel`(或 `LodOptions.fixedLevel`)是 headless 与 CPU 预算共用的确定性钩子。

---

## 决策 2:decimation 分级策略 + faceId 保级矩阵

| 级 | 工具 | 拓扑 | faceId | 用途 | cell→faceId 映射 |
|----|------|------|--------|------|------------------|
| 0 full | 无(原 build_surface) | 不变 | 精确 | 拾取 + 着色 + still 高保真 | 恒等(cell i→faceId[i]) |
| 1 medium | `vtkQuadricDecimation`,**分 faceId region 独立 decimate** | 减面但 region 边界保留 | **保**(每输出三角唯一归属一 region) | 着色 + 中距 still + 可参与拾取 | 分 region 重建表 |
| 2 far | `vtkQuadricClustering`(CopyCellData off) | 改拓扑/合并 | **不保** | 极远景纯显示,平色 | invalid(标记无 faceId) |

### 关键:中模为何分 region decimate
`vtkQuadricDecimation` 对整网格统一塌边,会跨 faceId region 合并三角 → 输出三角 faceId 归属歧义(父任务隐患)。**按 faceId region 切分子网格、各自 decimate、再合并**,保证:① region 内部减面;② region 边界不被跨 region 塌掉;③ 每个输出三角仍唯一属于一个原 faceId region → cell→faceId 表可逐三角重建(输出 cell 顺序按 region 拼接,记录每 region 的 faceId)。代价是 region 数多时 region 边界三角不被简化(可接受,边界本就要保拾取语义)。

### faceId 着色衔接(现状)
full/medium 级仍走 `vtkFloatArray` cell scalar + LUT(`:204-216,406-416`),只是 medium 级的 faceId 数组从 `CellFaceIdMap` 重建(长度 = 该级三角数),而非原 span。far 级 `ScalarVisibilityOff` 平色。

### normals(winding 坑)
各级 decimate 后仍接 `vtkPolyDataNormals`(AutoOrientNormals + Consistency + SplittingOff,`:397-401`)—— M3 winding 非全局一致(memory `xq-surface-winding-not-consistent`),decimation 不改这一点,法线必须仍由 VTK 重导。**注意**:full 级 normals deep copy 正是 3786 MiB 根因之一,降量收益主要来自 **选中较低级时 normals 作用在小得多的网格上 + 上传量小**。

---

## 决策 3:离线/异步 decimation 落地形态

### 背景
20M 三角单线程 `vtkQuadricDecimation` 预处理会卡主线程(父任务硬约束)。

### 取舍
- **同步预处理**:主线程 decimate → addSurface 卡死数秒,违约束。
- **后台线程(选定)**:`SurfaceLodBuilder::buildAsync` 用 `std::async(std::launch::async)`(或项目既有线程封装,实现期核对 services 是否已有线程池)拷出 full `vtkPolyData` 的几何到后台线程算各级;**full 级先可见**(addSurface 立即用 full 装配并返回),低模就绪后回填 `vtkLODActor` 的 LOD mapper(交互路径)或供后续固定级选取。

### 结论
选后台线程。`addSurface` 提交 build 后立即返回(full 级先上),decimation 不阻塞主线程(AC-B1 可断言"提交后立即返回")。线程安全:decimation 在独立 `vtkPolyData` 副本上算(copy-on-upload 已是 M9a 模型,无悬垂);回填到 actor 必须在主/渲染线程做(VTK 对象非线程安全)—— 后台只产 `vtkPolyData`,主线程建 mapper 并挂 actor。

> headless/测试可走同步路径(`buildSync`)以确定性断言级数与点数;异步性单独用"提交后立即返回 + 完成回调"断言。

---

## 决策 4:RenderStats 扩字段(不破旧义)

`XQSceneRenderer.h:21-25` 追加一个字段:
```cpp
struct RenderStats {
    bool ok = false;
    int actorCount = 0;
    long long pointCount = 0;        // 源点数,语义不变(test_scene_renderer.cpp:91 仍绿)
    long long uploadedPointCount = 0; // 新增:实际上传级点数;LOD 关时 == pointCount
};
```
- LOD 关(默认):`uploadedPointCount = pointCount`(等价 M9a,AC-B6)。
- LOD 开 + 选中级 k:`uploadedPointCount =` level[k] 点数。AC-B2 断言其 ≤ full 的 1/4(选远/中级时)。
- 纯追加字段,既有断言(只读 `ok/actorCount/pointCount`)不破。

---

## 决策 5:F — CMake RenderingLOD 前置

`CMakeLists.txt:67-84` 的 `find_package(VTK 9 REQUIRED COMPONENTS ...)` 追加 `RenderingLOD`(模块 + `vtkRenderingLOD-9.3.lib` 已确认存在)。`xq_visualization`(`:109-127`)的 `vtk_module_autoinit(TARGETS xq_visualization MODULES ${VTK_LIBRARIES})` 已随 `${VTK_LIBRARIES}` 带入新模块的 object factory 注册,无需单列。运行期 `vtkRenderingLOD-9.3.dll` 须在 PATH(memory `ctest-environment-overrides-path`:离屏 ctest 的 ENVIRONMENT PATH 是覆盖式,需在 CMake 注入 vtk bin)。

---

## 数据流(headless 固定级,可测路径)

```
addSurface(handle, LodOptions{enabled=true, fixedLevel=k})
  ResidentSurfaceSource src(handle)               // copy-on-upload
  full = build_surface(src)                        // level0 + 恒等 faceId 表
  levels = SurfaceLodBuilder.buildSync(full, faceIdSpan)  // 测试同步;生产 buildAsync
  k = LodOptions.fixedLevel >= 0 ? fixedLevel
                                 : LodLevelSelector(budget, levels.meta)
  mapper_k = makeMapper(levels[k])  // faceId LUT(k<2) 或平色(k==2)
  actor.SetMapper(mapper_k)
  renderer.AddActor(actor)
  return { ok, actorCount=1, pointCount=src.pointCount(), uploadedPointCount=levels[k].points }
```

交互路径(`XQRenderWidget`):同样建 levels,但 `vtkLODActor` + `AddLODMapper(每级)`,DesiredUpdateRate 由 QVTK interactor 驱动 still/motion;`uploadedPointCount` 在交互路径报 still 级(或不强保证,headless 才是测量基准)。

---

## 分层与依赖

- 新组件 `SurfaceLodBuilder` / `LodLevelSelector` / `LodOptions` 全在 `XQ/src/visualization/`,VTK 私有;io/core 无 `vtk*`。
- 新增 link 边:`xq_visualization` 依赖 `VTK::RenderingLOD`(新),方向合法(visualization→VTK),无环。
- Source 1.0 公共签名零改动(经现有 `acquire_points`/`acquire_triangles` 消费,static_assert 成立)。
- `XQSceneRenderer` 公共头仅追加 RenderStats 字段 + `addSurface` 可选参数(默认值 → 旧调用不变),pimpl 边界(VTK-free 头)保持。

## 验证策略

- **不退化基线**:LOD 默认关,`test_scene_renderer` 全断言绿且输出与 M9a 等价(AC-B6)。
- **降量可核**:扩展 `test_scene_renderer`(或新 `test_surface_lod`)—— 构造中等规模 surface,LOD 开 + fixedLevel=medium/far,断言 `uploadedPointCount < pointCount` 且 ≤ full 的 1/4(数量级),`renderOffscreenToRgba` 仍 ok + 非全黑(AC-B2/B3/B5)。
- **faceId 正确**:构造已知多 region surface(每 region 不同 faceId),medium 级经 `CellFaceIdMap` 查回每三角 faceId,断言落在所属 region 原 faceId(AC-B4);full 级映射恒等且着色像素与 M9a 等价。
- **异步**:断言 `buildAsync` 提交后 addSurface 立即返回(用计时 / future 未就绪)(AC-B1)。
- **大规模峰值**:扩 `scale_probe`(`XQ/probe/main.cpp`,OFF 档)加 LOD 路径,量 large 开 LOD vs M9a 全量的进程峰值工作集(psapi),对照 3786 MiB 基线(AC-B2 量化)。
- **假绿抽查**:篡改一处 faceId region 数据,确认 faceId 断言变红(decimate/acquire 不进 assert)。
- 铁律:Release + 全量 ctest 全绿 + RenderingLOD dll 在运行 PATH。

## 风险

- **R1 vtkLODActor headless 不可测**:已由决策 1 双路径化解 —— 测量走固定级直挂,vtkLODActor 仅交互窗口用。**先写 headless 固定级烟囱测试跑通再铺开**。
- **R2 vtkQuadricDecimation 不传 cell data**:VTK QuadricDecimation 默认不携带 cell scalar → faceId 不能靠 filter 直传,必须靠决策 2 的"分 region decimate + 重建表"。实现期先用小多 region 网格验证 region 不串色再上规模。
- **R3 分 region decimate 的 region 数膨胀**:region 极多时(每三角一 faceId)退化为几乎不减面。需在 builder 里对"region 过碎"降级(该 region 不 decimate,原样保留)或合并策略;design 阶段标记,实现期按真实 faceId 分布定阈值。
- **R4 后台线程 + VTK 对象线程安全**:decimation 只在副本 `vtkPolyData` 上算,actor/mapper 装配回主线程;严禁后台线程碰 renderer/actor。
- **R5 RenderingLOD dll 运行期缺失**:离屏 ctest ENVIRONMENT PATH 覆盖式(memory `ctest-environment-overrides-path`),需 CMake 注入 vtk bin,否则 `vtkLODActor` 实例化 `0xc0000135`。
- **R6 normals deep copy 仍在 full 级**:若 still 默认选 full,峰值不降。需 still 默认选 medium(保 faceId)或确保大规模下 budget 触发降级;design 默认 still=medium、far 仅极远景。
