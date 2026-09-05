# PRD — 渲染层做厚并接入主窗口

> 任务 id:`06-28-render-visualization` | 优先级 P1 | package XQ
> 前置:M0~M7 主线已完成(ctest 44/44 真绿)。本任务补"看得见"。

## 1. 背景与问题

主线 M0~M7 已端到端跑通:影像→路径→分割→建模→网格→流体→AI,各步结果入项目树。
但**算得出,看不见**:

- `xq_visualization` 只有 315 行(`XQImageViewer` 一个类),且只能 **offscreen 渲染影像成 RGBA buffer**,
  没有交互式 VTK 窗口。
- `XQMainWindow` 用一个 `QLabel imageLabel_` 贴 RGBA 静态图;没有 3D 视图、不能旋转缩放。
- **路径 / 分割掩膜 / 表面模型 / 体网格 / 流场结果 这些主线产物完全没有渲染**。

目标架构要求"渲染能力由独立库提供"——目前这条基本是空的。本任务把渲染做厚,
并真正接进主窗口,让用户能看见并交互每个阶段的产物。

## 2. 范围

### In scope

1. **`xq_visualization` 库扩展**:在现有 `XQImageViewer` 之外,新增对主线五类产物的渲染能力:
   - 影像:2D 切片(沿轴向,可换层)——交互式,非只 offscreen
   - 路径(`XQPathPayload`):折线 / 管状
   - 分割掩膜(`XQSegmentationMaskPayload`):叠加在影像切片上(着色覆盖)
   - 表面模型(`XQSurfaceModelPayload` → `XQTriangleSurfaceGeometryHandle`):三角面,按 faceId 着色
   - 体网格(`XQMeshPayload` → `XQTetVolumeMeshHandle`):线框 / 边界面
   - 流场结果(`XQFlowResultPayload`):按标量(压力/流量)着色的表面或线
2. **交互式渲染 widget**:基于 `QVTKOpenGLNativeWidget`(或等价)的可交互 3D 视图(旋转/缩放/平移)。
3. **接进 `XQMainWindow`**:把交互 widget 放进中央显示区(替换或并存于现有 `imageLabel_`);
   场景树选中某节点 → 中央区渲染对应 payload。
4. **测试**:offscreen 渲染各类 payload 的无头测试(沿用 `test_image_viewer` 的 offscreen + offscreen platform 模式),
   验证渲染产出非空、几何顶点数 / actor 数与输入一致。

### Out of scope(本任务不做,记后续)

- 体绘制(volume rendering)、MPR 三视图联动、测量工具、剪切面 —— 高级可视化,后续任务。
- 真实 ONNX 推理结果的可视化(依赖 A2)。
- 流场动画 / 时间序列播放。
- 渲染性能优化(LOD、大网格抽稀)。

## 3. 铁律约束(不可违背,违反即 BLOCKER)

1. **core/services/controllers 保持零 VTK**:所有 VTK include 只能出现在 `src/visualization/` 的
   `.cpp`(Impl)里。公开头文件(`.h`)不得 `#include` 任何 `vtk*` / Qt 渲染类型。
   验收做反向依赖审计:`rg "#include.*(vtk|QVTK)" src/core src/services src/ui/controllers` 必须为空。
2. **依赖方向不变**:`app → app_shell → visualization → core`;`xq_visualization` 只 `PUBLIC xq_core`
   + `PRIVATE ${VTK_LIBRARIES}`(现状已如此,不得新增对 services/io/adapters 的依赖)。
3. **表面法线自定向**:M3 表面 winding 全局不一致(闭合 ≠ 法线一致)。渲染表面 / 着色时
   **不得假设三角同向**;按有符号体积或 VTK `vtkPolyDataNormals` 的 auto-orient 处理,否则光照翻面。
4. **数据从 payload 取,不重算**:渲染只消费 scene 里已有的 payload 几何(points/triangles/tets/buffer),
   不在渲染层跑任何主线算法。
5. **前端仍轻**:界面层(`app/`)只做"选中节点 → 调 viewer 渲染"的接线;渲染逻辑全在 `xq_visualization`。

## 4. 验收标准(逐条可证伪)

- [ ] **AC1 库扩展**:`xq_visualization` 新增至少覆盖 影像切片 / 路径 / 表面模型 / 体网格 / 流场 的渲染入口,
      公开 API 只暴露 XQ 类型(payload / 句柄 / XQ 几何),不暴露 vtk 类型。
- [ ] **AC2 反向依赖零渗透**:`rg "#include.*(vtk|QVTK)" src/core src/services src/ui/controllers` 输出为空。
- [ ] **AC3 交互接入**:`XQMainWindow` 中央区是交互式 VTK widget;选中场景树节点能渲染对应 payload
      (至少影像、表面模型、体网格三类可见)。
- [ ] **AC4 offscreen 测试**:新增 `test_*_render` 无头测试,QT_QPA_PLATFORM=offscreen 下渲染各类 payload,
      断言:渲染 ok、actor / 顶点数与输入几何一致、RGBA buffer 非全黑(或可截图校验)。
- [ ] **AC5 表面定向**:表面 / 体网格边界面渲染做了法线自定向,着色无大面积翻面(测试或目视确认)。
- [ ] **AC6 全量绿 + 假绿抽查**:全新 build 目录构建零错误、全量 ctest 全绿(当前 44 + 新增渲染测试);
      假绿抽查:篡改某渲染 actor 装配(如不加 actor)→ 对应渲染测试 Release FAIL → 恢复 → PASS。
- [ ] **AC7 主线不回归**:原 44 个测试全绿,`XQImageViewer` 旧 offscreen 接口 / `test_image_viewer` 不破坏。

## 5. 已知现状(实测,供执行者接手)

- `XQImageViewer`(`src/visualization/XQImageViewer.{h,cpp}`):仅 `renderOffscreen` / `renderToRgba`,
  把 `XQImageVolume` 渲成 RGBA。Impl 用 pimpl 藏 VTK。315 行。
- `XQMainWindow`:中央/侧栏靠 `QLabel imageLabel_` + `XQImageViewer imageViewer_` + `lastRgba_`;
  `showImage()` 走 offscreen→RGBA→QLabel。无交互 3D。已有 `attachWorkflow` 接 6 Controller + stage 面板。
- 几何数据接口(渲染从这取):
  - `XQTriangleSurfaceGeometryHandle`:`points()` / `triangles()`(`array<int,3>`)/ `triangleFaceId(i)`。
  - `XQTetVolumeMeshHandle`:`points()` / `tets()`(`array<int,4>`)。
  - 影像:`XQImageVolume` + buffer(M2 真标量 buffer)。
  - 路径 / 流场:见 `XQPathPayload` / `XQFlowResultPayload`(执行者按需读)。
- CMake:`xq_visualization` 已 `PUBLIC xq_core` + `PRIVATE ${VTK_LIBRARIES}` + `vtk_module_autoinit`。
  VTK 组件已含 `RenderingCore/RenderingOpenGL2/InteractionStyle`;交互 widget 可能需补
  `GUISupportQt`(`QVTKOpenGLNativeWidget`)——执行者确认 VTK 9.3.0 该模块是否已装。

## 6. 风险 / 注意

- **QVTKOpenGLNativeWidget 依赖 VTK::GUISupportQt**:若 Externals 的 VTK 未编该模块,交互 widget 接入受阻。
  执行者第一步先验证模块可用(`find_package(VTK COMPONENTS GUISupportQt)`);不可用则降级为
  "offscreen 渲染 + QLabel 贴图但支持切层/切节点",并把交互 widget 记为后续。
- **offscreen 测试的 OpenGL**:CI/无头环境 VTK offscreen 渲染依赖 mesa/软渲染;`test_image_viewer` 已能在
  offscreen 跑通,沿用其模式即可,新测试不要引入需要真实 GPU 的特性。
- **表面 winding**:见铁律 3,务必自定向。
