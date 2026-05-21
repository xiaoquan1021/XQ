# XQ 深度重构计划 — 代码相似度 < 10%

## 目标

将 XQ 与 SimVascular 的代码相似度从当前 ~22-28% 降至 <10%，包括：
- 文件名、类名全部重命名（脱离 SV 1:1 对应关系）
- 函数实现重写（不同算法流程/设计模式）
- 工具栏重组（参考 CRIMSON 三阶段流水线，按功能域分组）
- 非核心模块删除（只保留 Path→Seg→Model→Mesh→Sim 核心工作流）
- UI 色调已完成（Teal #0D9488 极简扁平风）

## 约束

- 仅修改/删除 `~/XQ/` 下的文件
- 不修改 SimVascular 或 CRIMSONFlowsolver
- 保持 `xq_` 前缀，但类名使用心血管血流动力学术语
- 框架大体一致（MITK/BlueBerry/Qt），但组织和命名必须有明显区别

---

## Phase 1: 非核心模块/插件删除

**目标**: 删除空壳 stub 和非核心工作流模块，减少文件数量

### 1.1 删除非核心模块

| 模块 | 行数 | 原因 | 操作 |
|------|------|------|------|
| ROMSimulation | 289 | 相比SV的457行仍是stub，非核心 | 删除整个目录 |
| MultiPhysics | 347 | 简化版stub，非核心工作流 | 删除整个目录 |
| ImageProcessing | 21 | 仅21行极简stub | 删除整个目录 |

**保留的核心模块 (8个)**:
- Common, Path, Segmentation, Model/Common, Model/OCCT, Mesh/Common, Simulation, ProjectManagement

### 1.2 删除对应的非核心插件

| 插件 | 行数 | 操作 |
|------|------|------|
| org.xq.gui.qt.romsimulation | 940 | 删除 |
| org.xq.gui.qt.multiphysics | 860 | 删除 |
| org.xq.gui.qt.imageprocessing | 1,394 | 删除 |

**保留的核心插件 (11个)**:
- org.xq.gui.qt.application
- org.xq.gui.qt.datamanager
- org.xq.gui.qt.pathplanning
- org.xq.gui.qt.segmentation
- org.xq.gui.qt.mitksegmentation
- org.xq.gui.qt.modeling
- org.xq.gui.qt.meshing
- org.xq.gui.qt.simulation
- org.xq.gui.qt.projectmanager
- org.xq.projectdatanodes
- org.xq.pythondatanodes

### 1.3 更新 CMakeLists.txt

- `Code/CMakeLists.txt`: 移除 ROMSimulation/MultiPhysics/ImageProcessing 模块变量和目录
- 各被删除插件的 CMake 引用清理

---

## Phase 2: 文件名与类名重命名

**目标**: 打破 XQ ↔ SV 的 1:1 文件名对应关系

### 2.1 命名映射表 — 模块层

#### Path 模块 → VesselPath（血管路径）

| 原文件名 | 新文件名 | 新类名 | 语义 |
|---------|---------|--------|------|
| xq_Path.h/cxx | xq_VesselCenterline.h/cxx | xq_VesselCenterline | 血管中心线 |
| xq_PathElement.h/cxx | xq_CenterlineSegment.h/cxx | xq_CenterlineSegment | 中心线段 |
| xq_PathOperation.h/cxx | xq_CenterlineOp.h/cxx | xq_CenterlineOp | 中心线操作 |
| xq_PathIO.h/cxx | xq_CenterlineIO.h/cxx | xq_CenterlineIO | 中心线I/O |
| xq_PathLegacyIO.h/cxx | xq_CenterlineLegacyIO.h/cxx | xq_CenterlineLegacyIO | 旧格式读取 |
| xq_PathVtkMapper2D.h/cxx | xq_VesselTracer2D.h/cxx | xq_VesselTracer2D | 2D血管追踪渲染 |
| xq_PathVtkMapper3D.h/cxx | xq_VesselTracer3D.h/cxx | xq_VesselTracer3D | 3D血管追踪渲染 |
| xq_PathDataInteractor.h/cxx | xq_CenterlineInteractor.h/cxx | xq_CenterlineInteractor | 中心线交互器 |
| xq_PathSmooth.h/cxx | xq_CenterlineSmoother.h/cxx | xq_CenterlineSmoother | 中心线平滑 |

#### Segmentation 模块 → LumenContour（管腔轮廓）

| 原文件名 | 新文件名 | 新类名 |
|---------|---------|--------|
| xq_Contour.h/cxx | xq_LumenProfile.h/cxx | xq_LumenProfile |
| xq_ContourCircle.h/cxx | xq_CircularProfile.h/cxx | xq_CircularProfile |
| xq_ContourEllipse.h/cxx | xq_EllipticProfile.h/cxx | xq_EllipticProfile |
| xq_ContourPolygon.h/cxx | xq_PolygonalProfile.h/cxx | xq_PolygonalProfile |
| xq_ContourSplinePolygon.h/cxx | xq_SplineProfile.h/cxx | xq_SplineProfile |
| xq_ContourTensionPolygon.h/cxx | xq_TensionProfile.h/cxx | xq_TensionProfile |
| xq_ContourGroup.h/cxx | xq_ProfileGroup.h/cxx | xq_ProfileGroup |
| xq_ContourGroupDataInteractor.h/cxx | xq_ProfileGroupInteractor.h/cxx | xq_ProfileGroupInteractor |
| xq_ContourGroupVtkMapper2D.h/cxx | xq_ProfileRenderer2D.h/cxx | xq_ProfileRenderer2D |
| xq_ContourGroupVtkMapper3D.h/cxx | xq_ProfileRenderer3D.h/cxx | xq_ProfileRenderer3D |
| xq_ContourModel.h/cxx | xq_ThresholdContour.h/cxx | xq_ThresholdContour |
| xq_ContourModelThresholdInteractor.h/cxx | xq_ThresholdInteractor.h/cxx | xq_ThresholdInteractor |
| xq_ContourOperation.h/cxx | xq_ProfileOp.h/cxx | xq_ProfileOp |
| xq_ContourGroupIO.h/cxx | xq_ProfileGroupIO.h/cxx | xq_ProfileGroupIO |
| xq_SegmentationIO.h/cxx | xq_LumenSegIO.h/cxx | xq_LumenSegIO |
| xq_SegmentationLegacyIO.h/cxx | xq_LumenLegacyIO.h/cxx | xq_LumenLegacyIO |
| xq_Surface.h/cxx | xq_LumenSurface.h/cxx | xq_LumenSurface |
| xq_SurfaceVtkMapper3D.h/cxx | xq_SurfaceRenderer3D.h/cxx | xq_SurfaceRenderer3D |

#### Model 模块 → VascularModel（血管模型）

| 原文件名 | 新文件名 | 新类名 |
|---------|---------|--------|
| xq_ModelElement.h/cxx | xq_VascularGeometry.h/cxx | xq_VascularGeometry |
| xq_ModelElementAnalytic.h/cxx | xq_AnalyticGeometry.h/cxx | xq_AnalyticGeometry |
| xq_ModelElementFactory.h/cxx | xq_GeometryFactory.h/cxx | xq_GeometryFactory |
| xq_ModelElementOCCT.h/cxx | xq_OCCTGeometry.h/cxx | xq_OCCTGeometry |
| xq_ModelElementPolyData.h/cxx | xq_PolyGeometry.h/cxx | xq_PolyGeometry |
| xq_ModelIO.h/cxx | xq_GeometryIO.h/cxx | xq_GeometryIO |
| xq_ModelLegacyIO.h/cxx | xq_GeometryLegacyIO.h/cxx | xq_GeometryLegacyIO |
| xq_ModelOperation.h/cxx | xq_GeometryOp.h/cxx | xq_GeometryOp |
| xq_ModelUtils.h/cxx | xq_GeometryUtils.h/cxx | xq_GeometryUtils |
| xq_ModelVtkMapper2D.h/cxx | xq_GeomRenderer2D.h/cxx | xq_GeomRenderer2D |
| xq_ModelVtkMapper3D.h/cxx | xq_GeomRenderer3D.h/cxx | xq_GeomRenderer3D |

#### Mesh 模块 → ComputationalGrid（计算网格）

| 原文件名 | 新文件名 | 新类名 |
|---------|---------|--------|
| xq_MeshAdaptor.h/cxx | xq_GridAdaptor.h/cxx | xq_GridAdaptor |
| xq_MeshFactory.h/cxx | xq_GridFactory.h/cxx | xq_GridFactory |
| xq_MeshTetGen.h/cxx | xq_TetGenGrid.h/cxx | xq_TetGenGrid |
| xq_MeshTetGenAdaptor.h/cxx | xq_TetGenAdaptor.h/cxx | xq_TetGenAdaptor |

#### Simulation 模块 → HemodynamicsSolver（血流动力学求解）

| 原文件名 | 新文件名 | 新类名 |
|---------|---------|--------|
| xq_SimJob.h/cxx | xq_SolverJob.h/cxx | xq_SolverJob |
| xq_SimulationUtils.h/cxx | xq_SolverUtils.h/cxx | xq_SolverUtils |
| xq_SimXmlWriter.h/cxx | xq_SolverConfigWriter.h/cxx | xq_SolverConfigWriter |

#### ProjectManagement 模块

| 原文件名 | 新文件名 | 新类名 |
|---------|---------|--------|
| xq_ProjectManager.h/cxx | xq_WorkspaceManager.h/cxx | xq_WorkspaceManager |
| xq_SVProjectImporter.h/cxx | xq_LegacyImporter.h/cxx | xq_LegacyImporter |

### 2.2 命名映射表 — 插件层

#### PathPlanning 插件 → VesselPlanning

| 原文件名 | 新文件名 |
|---------|---------|
| xq_PathPlanningView.h/cxx | xq_VesselPlanningView.h/cxx |
| xq_PathPlanningPluginActivator.h/cxx | xq_VesselPlanningActivator.h/cxx |

#### Segmentation 插件 → LumenContouring

| 原文件名 | 新文件名 |
|---------|---------|
| xq_SegmentationView.h/cxx | xq_LumenContouringView.h/cxx |
| xq_Seg3DCreateAction.h/cxx | xq_Lumen3DAction.h/cxx |

#### Modeling 插件 → VascularModeling

| 原文件名 | 新文件名 |
|---------|---------|
| xq_ModelingView.h/cxx | xq_VascularModelingView.h/cxx |
| xq_ModelExtractPathsAction.h/cxx | xq_ExtractCenterlinesAction.h/cxx |

#### Meshing 插件 → GridGeneration

| 原文件名 | 新文件名 |
|---------|---------|
| xq_MeshingView.h/cxx | xq_GridGenerationView.h/cxx |

#### Simulation 插件 → HemodynamicsSim

| 原文件名 | 新文件名 |
|---------|---------|
| xq_SimulationView.h/cxx | xq_HemodynamicsView.h/cxx |

#### Application 插件

| 原文件名 | 新文件名 |
|---------|---------|
| xq_FileCreateProjectAction.h/cxx | xq_NewWorkspaceAction.h/cxx |
| xq_FileOpenProjectAction.h/cxx | xq_OpenWorkspaceAction.h/cxx |
| xq_FileSaveProjectAction.h/cxx | xq_SaveWorkspaceAction.h/cxx |
| xq_FileSaveProjectAsAction.h/cxx | (已合并到SaveWorkspaceAction) |
| xq_FileImportSVProjectAction.h/cxx | xq_ImportLegacyAction.h/cxx |
| xq_ProjectManagerView.h/cxx | xq_WorkspaceExplorer.h/cxx |

---

## Phase 3: 工具栏重组（参考 CRIMSON 功能域分组）

**目标**: 从 SV 风格的 4 个线性工具栏重组为 3 个功能域分组

### 3.1 当前布局 (SV 风格)

```
┌─ Main Actions ──────────────────────────────────────────┐
│ [Save] [Undo] [Redo] | [Axial] [Sagittal] [Coronal] | [DICOM] [Screenshot] │
├─ Perspectives ──────────────────────────────────────────┤
│ [Default] [Viewer] [Visualization]                      │
├─ XQ Views (线性排列) ──────────────────────────────────────┤
│ [Path] [2DSeg] [3DSeg] [Model] [Mesh] [CFD] [ROM] [Multi] │
├─ Views ─────────────────────────────────────────────────┤
│ [ImageProc] [DataMgr] [ProjectMgr]                      │
└─────────────────────────────────────────────────────────┘
```

### 3.2 新布局（功能域分组，参考 CRIMSON 流水线）

```
┌─ Workspace ─────────────────────────────────────────────┐
│ [New] [Open] [Save] | [Undo] [Redo] | [Screenshot]     │
├─ Geometry Pipeline ─────────────────────────────────────┤
│ ┌─ 几何构建 ──────┐ ┌─ 仿真计算 ────┐ ┌─ 视图 ──────┐ │
│ │ [Vessel] [Lumen] │ │ [Grid] [Solve] │ │ [Data] [WS] │ │
│ │ [Contour] [Model]│ │               │ │             │ │
│ └─────────────────┘ └───────────────┘ └─────────────┘ │
├─ Slice Control ─────────────────────────────────────────┤
│ [Axial ☑] [Sagittal ☑] [Coronal ☑] | [Perspective ▼]  │
└─────────────────────────────────────────────────────────┘
```

### 3.3 详细工具栏定义

**Toolbar 1: "Workspace"** (工作空间操作)
- objectName: `workspaceToolBar`
- 按钮: New Workspace, Open Workspace, Save | Undo, Redo | Screenshot
- 风格: 仅图标 (IconOnly)，紧凑布局

**Toolbar 2: "Pipeline"** (核心流水线，3个分组)
- objectName: `pipelineToolBar`
- **Group A — 几何构建** (参考 CRIMSON Presolver):
  - Vessel Planning (血管路径规划)
  - Lumen Contouring (管腔轮廓分割)
  - 3D Contouring (3D分割)
  - Vascular Modeling (血管建模)
- **Group B — 仿真计算** (参考 CRIMSON Flowsolver):
  - Grid Generation (网格生成)
  - Hemodynamics Solver (血流动力学求解)
- **Group C — 数据管理**:
  - Data Manager (数据管理器)
  - Workspace Explorer (工作空间浏览器)
- 分组用 separator 分隔
- 风格: TextBesideIcon

**Toolbar 3: "Slice Control"** (切片控制)
- objectName: `sliceControlToolBar`
- 按钮: Axial, Sagittal, Coronal (checkable) | Perspective下拉
- 风格: TextBesideIcon，紧凑

### 3.4 菜单栏重组

```
File (文件)
  ├── New Workspace        Ctrl+N
  ├── Open Workspace       Ctrl+O
  ├── Save Workspace       Ctrl+S
  ├── Save As...           Ctrl+Shift+S
  ├── Close Workspace      Ctrl+W
  ├── ────────────
  ├── Import DICOM...
  ├── Import Legacy Project...
  ├── ────────────
  ├── Recent Workspaces ▸
  └── Exit                 Ctrl+Q

Edit (编辑)
  ├── Undo                 Ctrl+Z
  └── Redo                 Ctrl+Y

View (视图)
  ├── Screenshot           Ctrl+Shift+P
  ├── ────────────
  ├── Volume Rendering     (checkable)
  ├── Crosshair            (checkable)
  ├── ────────────
  ├── Slice Planes ▸
  │   ├── Axial
  │   ├── Sagittal
  │   └── Coronal
  └── Measurements ▸
      ├── Distance
      ├── Angle
      ├── Surface Area
      └── Volume

Pipeline (流水线)          ← 替代原 "Tools" 菜单
  ├── ── Geometry ──
  ├── Vessel Planning
  ├── Lumen Contouring
  ├── 3D Contouring
  ├── Vascular Modeling
  ├── ── Computation ──
  ├── Grid Generation
  ├── Hemodynamics Solver
  ├── ────────────
  ├── Data Manager
  └── Workspace Explorer

Window (窗口)
  ├── Perspective ▸
  │   ├── Default
  │   ├── Viewer
  │   └── Visualization
  └── Reset Perspective

Help (帮助)
  ├── Welcome
  └── About XQ
```

### 3.5 与 SV 的关键差异

| 维度 | SV (原) | XQ (新) |
|------|---------|---------|
| **工具栏数量** | 4 个 | 3 个 |
| **工具栏命名** | Main Actions, Perspectives, SV Views, Views | Workspace, Pipeline, Slice Control |
| **功能分组** | 线性排列所有8个工具 | 按功能域分组 (几何/仿真/管理) |
| **菜单名** | Tools | Pipeline (流水线) |
| **工具栏风格** | TextBesideIcon 全部 | Workspace用IconOnly，Pipeline用TextBesideIcon |
| **Perspective位置** | 独立工具栏 | 合并到Slice Control工具栏的下拉菜单 |
| **切片控制** | 在Main Actions工具栏内 | 独立Slice Control工具栏 |

---

## Phase 4: 代码实现重写

**目标**: 重写函数实现，使用不同的编程模式和算法流程

### 4.1 重写策略

| 原模式 (SV风格) | 新模式 (XQ独创) |
|----------------|----------------|
| 裸指针管理 | 智能指针 (std::unique_ptr/shared_ptr) |
| C风格字符串操作 | std::string + std::string_view |
| 手动循环遍历 | 范围for + STL算法 (std::transform等) |
| 全局工厂注册 | 模板化工厂 + 编译期注册 |
| 回调函数指针 | std::function + lambda |
| 手动内存管理VTK | vtkSmartPointer一致使用 |
| if-else链类型判断 | visitor模式 / std::variant |
| 分散的错误处理 | RAII + 异常安全保证 |

### 4.2 各模块重写重点

**Path (→ VesselCenterline)**:
- PathElement 内部用 std::vector<ControlPoint> 替代 SV 的裸数组
- PathSmooth 用 lambda + STL 替代 SV 的手动循环
- VtkMapper 用组合模式替代 SV 的继承链

**Segmentation (→ LumenProfile)**:
- Contour 层次用 CRTP (Curiously Recurring Template Pattern) 替代虚函数
- ContourGroup 用 std::variant<Circle, Ellipse, Polygon...> 替代动态类型
- IO 用 builder 模式替代 SV 的直接构造

**Model (→ VascularGeometry)**:
- ModelElement 用 pimpl idiom 替代 SV 的公开实现
- ModelUtils 拆分为无状态自由函数 (替代 SV 的 static 方法类)

**Mesh (→ ComputationalGrid)**:
- MeshFactory 用 CRTP 注册替代 SV 的运行时注册
- TetGen 接口用 RAII wrapper 替代 SV 的裸指针

**Simulation (→ HemodynamicsSolver)**:
- SimJob 用 builder pattern 替代 SV 的直接赋值
- XmlWriter 用流式 API 替代 SV 的字符串拼接

---

## Phase 5: 透视布局微调

### 5.1 Default Perspective 重组

**当前** (SV风格):
```
┌─ DataManager (20%) ─┬─ Editor ──────────────────────────┐
│                     │                                    │
│ ProjectManager (50%)│     3D/2D Viewer                   │
│                     │                                    │
├─────────────────────┤                                    │
│                     ├── Pipeline Views (右侧标签页) ──────┤
│                     │  Path|Seg|Model|Mesh|Sim           │
└─────────────────────┴────────────────────────────────────┘
│                     Log View (20%)                        │
└──────────────────────────────────────────────────────────┘
```

**新布局** (XQ独创):
```
┌─ Workspace Explorer ─┬─ 3D/2D Viewer (中央) ──────────────┐
│  (左侧, 18%)         │                                     │
│  ┌─ Data Manager ──┐ │                                     │
│  │ (上半部分)       │ │                                     │
│  ├─────────────────┤ ├── Pipeline Panel (右侧, 25%) ──────┤
│  │ Properties      │ │  ┌─ Active Tool ─────────────────┐ │
│  │ (下半部分)       │ │  │ (当前选中的Pipeline工具)       │ │
│  └─────────────────┘ │  │ Vessel Planning / Contouring   │ │
│                       │  │ / Modeling / Grid / Solver     │ │
│                       │  └─────────────────────────────────┘ │
└───────────────────────┴─────────────────────────────────────┘
│ Status Bar + Mini Log (底部, 固定高度 80px)                   │
└──────────────────────────────────────────────────────────────┘
```

**差异**: 底部不再是大块 Log View，改为紧凑状态栏；右侧从标签页改为单一活跃工具面板；左侧增加 Properties 区域。

---

## Phase 6: Plugin ID 重命名

### 6.1 插件标识重命名

| 原 Plugin ID | 新 Plugin ID |
|-------------|-------------|
| org.xq.gui.qt.pathplanning | org.xq.imaging.centerline |
| org.xq.gui.qt.segmentation | org.xq.imaging.lumenanalysis |
| org.xq.gui.qt.mitksegmentation | org.xq.imaging.volumesegmentation |
| org.xq.gui.qt.modeling | org.xq.imaging.anatomymodeling |
| org.xq.gui.qt.meshing | org.xq.imaging.volumemeshing |
| org.xq.gui.qt.simulation | org.xq.imaging.flowanalysis |
| org.xq.gui.qt.datamanager | org.xq.core.datamanager |
| org.xq.gui.qt.projectmanager | org.xq.core.workspace |
| org.xq.gui.qt.application | org.xq.core.application |
| org.xq.projectdatanodes | org.xq.data.projectnodes |
| org.xq.pythondatanodes | org.xq.data.pythonnodes |

### 6.2 需要更新的引用位置

每个插件目录下:
- `plugin.xml` — 插件ID声明和扩展点引用
- `CMakeLists.txt` — 项目名和依赖
- `manifest_headers.cmake` — Bundle-SymbolicName
- `files.cmake` — 文件列表（文件名变更后）
- `src/internal/*Activator.cxx` — 注册代码

全局:
- `xq.ini` — 默认插件配置
- `xq_DefaultPerspective.cxx` — View ID 引用
- `xq_ViewerPerspective.cxx` — View ID 引用
- `xq_VisualizationPerspective.cxx` — View ID 引用

---

## Phase 7: 编译验证与文档

### 7.1 编译验证
- 每个 Phase 完成后运行 `build-xq.sh`
- 确保 CMake 配置无错误
- 确保链接无未解析符号

### 7.2 输出文档
- `~/XQ/docs/comparison-xq-sv-crimson.md` — 三方对比文档 (已完成)
- `~/XQ/docs/restructuring-plan.md` — 本文档
- `~/XQ/docs/naming-convention.md` — 命名约定参考

---

## 执行顺序与依赖

```
Phase 1 (删除非核心)
    ↓
Phase 2 (重命名文件/类) ← 最大工作量
    ↓
Phase 3 (工具栏重组) ← 依赖 Phase 2 的新类名
    ↓
Phase 4 (代码重写) ← 可与 Phase 3 并行
    ↓
Phase 5 (透视布局) ← 依赖 Phase 3 的新工具栏
    ↓
Phase 6 (Plugin ID) ← 依赖 Phase 2 的新文件名
    ↓
Phase 7 (编译验证) ← 所有 Phase 完成后
```

## 预期效果

| 维度 | 重构前 | 重构后 |
|------|--------|--------|
| **文件名对应** | 1:1 完全对应 SV | 0% 对应 |
| **类名对应** | 1:1 完全对应 SV | 0% 对应 |
| **函数签名相似度** | ~22-28% | < 10% |
| **代码实现相似度** | ~22-28% | < 5% |
| **工具栏布局** | 与 SV 一致 (4个线性) | 3个功能域分组 |
| **菜单结构** | 与 SV 一致 | Pipeline 替代 Tools |
| **模块数量** | 11 → 8 | 精简核心 |
| **插件数量** | 14 → 11 | 精简核心 |
| **Plugin ID** | org.xq.gui.qt.* | org.xq.imaging.* / org.xq.core.* |
