# SimVascular 三阶段(Path / Segmentation / Mesh)处理方案清单

> 目的:厘清 SimVascular 在**中心线路径、血管腔分割、体网格**三个阶段各提供哪些处理方案,哪些是常用主力、哪些是备用/兜底,作为 XQ 方向校准依据。
>
> **全部结论基于主会话亲读 SV 源码**(`Externals/src/SimVascular/Code/Source/sv4gui/`,不派 agent——SV 分割源码派 agent 常被安全机制误拦)。每条标注源文件+行号便于核查。
>
> **核心结论(回应用户):手绘不是 SV 的主力手段,只是兜底。** 三个阶段 SV 都以**自动/半自动(图像识别、算法驱动)为常用**,手动只在自动失败时修正。XQ 方向应对齐这一点。

---

## 0. 一句话优先级(每阶段常用 → 备用)

| 阶段 | 常用主力 | 次选 | 兜底/手动 |
|---|---|---|---|
| **Path 路径** | Smart Add(智能加点,自动吸附)+ Spline 插值 | 手动加点(Manually Add) | — |
| **Segmentation 分割** | **ML 机器学习** / **Level Set 水平集** / **Threshold 阈值**(三种自动,图像识别) | 批量沿路径 / 多血管批量 | **手绘 Circle/Ellipse/Polygon/SplinePolygon**(仅自动描不好时修正)|
| **Mesh 网格** | TetGen(开源默认)+ 基于中心线半径自适应 + 边界层 | 球形局部细化 / 基于解自适应(Adapt) | MeshSim(商业,需授权) |

---

## 1. Path 路径规划阶段

源码:`Plugins/org.sv.gui.qt.pathplanning/sv4gui_PathEdit.cxx`、`Modules/Path/sv4gui_PathElement.{h,cxx}`、`sv4gui_Spline.h`、`sv4gui_PathSmooth.h`。

### 1.1 控制点录入方案

| 方案 | 触发 | 说明 | 常用? |
|---|---|---|---|
| **Smart Add(智能加点)** | `buttonAdd → SmartAdd()`(PathEdit.cxx:128) | 在影像上点一下,自动把控制点吸附/推断到合理位置(沿血管走向) | **常用主力** |
| **手动加点(Manually Add)** | `buttonAddManually → ManuallyAdd()`(:129) | 精确手动落点 | 次选/微调 |
| **加点模式下拉(Adding Mode)** | `comboBoxAddingMode`(:112) | 控制新点插到序列哪个位置(末尾/选中点前后) | 辅助 |
| **删除点** | `buttonDelete → DeleteSelected()`(:130) | — | 编辑 |

### 1.2 路径插值 / 后处理

- **Spline 插值**(`sv4gui_Spline.h`,PathElement 用样条把稀疏控制点插成密集路径点):控制点 → 光滑连续中心线。这是**唯一的路径生成方式**(控制点只是骨架,真正的路径是样条采样出的密集点)。
- **细分方式**(PathElement.h `GetMethod()`):`CONSTANT_SPACING`(等间距)/ `CONSTANT_TOTAL_NUMBER`(定点数)——控制路径点采样密度。
- **Smooth 平滑**(`btnSmooth → SmoothCurrentPath()`,PathEdit.cxx:131,用 `sv4gui_PathSmooth`):对已建路径做平滑后处理,去抖动。
- **Reslice Slider**(`resliceSlider`,:144):沿路径弧长滑动定位,驱动垂直断面重采样(= XQ P3-1 已做的沿路径滑条 + 断面视图)。

> **XQ 现状对照**:XQ 已有 XQPath(控制点 + 样条采样 + PathSamplePoint frame)、沿路径滑条、断面重采样。Path 阶段的能力基本对齐,缺的是「Smart Add 自动吸附」和「Smooth 平滑」——但 Path 不是本次方向争议点。

---

## 2. Segmentation 分割阶段(核心,方向重灾区)

源码:`Plugins/org.sv.gui.qt.segmentation/sv4gui_Seg2DEdit.{cxx,ui}`(2D 沿路径逐层)、`sv4gui_Seg3DEdit.cxx`(3D 全卷)、`Modules/Segmentation/sv4gui_SegmentationUtils.cxx`、`sv4gui_Seg3DUtils.cxx`。

SV 分割分两大类:**2D 沿路径逐层描轮廓**(主流程,对应 XQ 本任务)和 **3D 全卷分割**(备选流程)。

### 2.1 2D 沿路径分割 —— 方法工具栏完整清单

.ui 里按钮**从上到下的排列顺序 = 常用优先级**(Seg2DEdit.ui,connect 在 .cxx:199-232):

| # | 方法 | 按钮 | 槽函数 | 类别 | 算法 | 常用? |
|---|---|---|---|---|---|---|
| 1 | **Level Set 水平集** | `btnLevelSet` | `CreateLSContour → CreateContours(LEVELSET_METHOD)` | **自动/图像识别** | ITK 水平集:圆形种子 `vtkGenerateCircle(radius)` → 两阶段演化 `ComputePhaseOneLevelSet/PhaseTwo`(SegmentationUtils.cxx:701+,`sv3_ITKLevelSet.h`),参数 kc/expFactorRising/Falling | **常用主力** |
| 2 | **Threshold 阈值** | `btnThreshold` | `CreateThresholdContour → CreateContours(THRESHOLD_METHOD)` | **自动/图像识别** | 断面内强度阈值 + 连通(限死在断面,不出实心块);有 preset 阈值滑条 `sliderThreshold` + `checkBoxPresetThreshold` | **常用** |
| 3 | **ML 机器学习** | `btnML` | `CreateMLContour → doMLContour → ml_utils->segmentPathPoint()` | **自动/深度学习** | 机器学习模型对每个路径点的断面**推理出血管腔轮廓**(Seg2DEdit.cxx:2311,`setupMLui`);SV 较新的自动分割 | 常用(有模型时) |
| 4 | Circle 圆 | `btnCircle` | `CreateCircle`(手绘) | 手动 | 2 控制点(圆心+边界),半径=距离,采样 36 点 | **兜底** |
| 5 | Ellipse 椭圆 | `btnEllipse` | `CreateEllipse` | 手动 | 中心+两半轴 | 兜底 |
| 6 | SplinePolygon 样条多边形 | `btnSplinePoly` | `CreateSplinePoly` | 手动 | 控制点 + 样条插值 | 兜底 |
| 7 | Polygon 多边形 | `btnPolygon` | `CreatePolygon` | 手动 | ≥3 顶点 + 段间线性插值 | 兜底 |

**辅助/批量能力**(不是单独的分割方法,是对上述方法的放大):

| 能力 | 控件 | 说明 |
|---|---|---|
| **批量 Batch** | `checkBoxBatch` + `lineEditBatchList` | 一次沿路径**多个采样位置**用当前(自动)方法循环生成轮廓 |
| **多血管 Multi-seg** | `multiSegButton → segmentPaths()`(:1971) | 多条路径一键批量分割 |
| **放样预览 Lofting Preview** | `checkBoxLoftingPreview → LoftContourGroup()`(:211) | 每加一个轮廓实时重建血管表面预览 |
| Smooth / Copy / Paste | `btnSmooth/btnCopy/btnPaste` | 轮廓后处理:平滑、复制粘贴到其它断面 |

> **关键洞察**:自动方法(1/2/3)排最前,批量(Batch/Multi-seg)**只对自动方法有意义**(手绘无法批量)。SV 的真实工作流是「选自动方法 → 批量沿整条路径一次描完 → 少数描不好的位置手绘修正」。**手绘是 exception,不是 rule。**

### 2.2 3D 全卷分割(备选流程)

源码:`Modules/Segmentation/sv4gui_Seg3DUtils.cxx`、`sv4gui_Seg3DEdit.cxx`。不沿路径,直接对整卷分割:

| 方法 | 算法 | 说明 |
|---|---|---|
| **Colliding Fronts 碰撞前沿** | `collidingFronts()`(Seg3DUtils.cxx:64,ITK `CollidingFrontsImageFilter`)+ 阈值约束 + 两个种子 | **3D 主力**:两个种子的前沿相向传播,交汇处为血管腔;种子约束天然不出板砖 |
| **Threshold 阈值** | ITK `ThresholdImageFilter` | 全卷阈值(需种子/连通约束) |
| Connected / Geodesic Level Set | ITK 连通/测地水平集 | 从种子区域生长 |

> XQ 之前证伪的「整卷强度阈值出实心板砖」正是**缺了 SV 的种子约束 + CollidingFronts**(见 memory `xq-threshold-seg-fullblock-bug`)。3D 分割若要做,必须走种子约束。

---

## 3. Mesh 体网格阶段

源码:`Plugins/org.sv.gui.qt.meshing/sv4gui_MeshEdit.cxx`、`Modules/Mesh/Common/sv4gui_MeshTetGen.cxx`。

### 3.1 网格引擎(二选一)

| 引擎 | 说明 | 常用? |
|---|---|---|
| **TetGen** | 开源四面体网格器(MeshEdit.cxx:323/763),SV 默认。仅接受 PolyData 表面模型 | **常用默认** |
| **MeshSim** | 商业网格器(需 Simmetrix 授权),支持 PolyData/OpenCASCADE | 备用(有授权时) |

### 3.2 TetGen 网格参数 / 方案

从 `sv4gui_MeshTetGen.cxx` 的 SetMeshOptions + MeshEdit 命令组装(:869-895):

| 方案 | 命令/选项 | 说明 | 常用? |
|---|---|---|---|
| **全局边尺寸 GlobalEdgeSize** | `option GlobalEdgeSize <h>` | 整体目标网格尺度,最基本参数 | **必设** |
| **基于中心线半径自适应** | `functionBasedMeshing <h> DistanceToCenterlines` / `useCenterlineRadius`(MeshTetGen.cxx:181) | 按到中心线距离自动加密——血管细处密、粗处疏。**SV 血管网格的关键自适应** | **常用主力** |
| **边界层 Boundary Layer** | `option BoundaryLayer` + Direction / NewRegionBoundaryLayer(:871-895,MeshTetGen SetBoundaryLayer :211) | 沿壁面生成多层薄网格(CFD 边界层解析必需) | **CFD 常用** |
| **局部边尺寸 LocalEdgeSize** | `LocalEdgeSize`(:141) | 对指定 face 单独设尺寸 | 次选 |
| **球形局部细化 Sphere Refinement** | `sphereRefinement`(:220,SetSphereRefinement) | 在指定球区域(如狭窄/分叉)局部加密 | 次选 |
| **基于解自适应 Adapt** | `btnAdapt → Adapt()`(MeshEdit.cxx:253)+ `comboBoxOption`(:247) | 用上一轮 CFD 解的误差估计重新分布网格密度(自适应重网格) | 高级/迭代 |

> **XQ 现状对照**:XQ 已 vendoring TetGen(memory `tetgen-plc-volume-mesh`),体网格链走「PLC 约束四面体化 → MMG 优化」。SV 的「基于中心线半径自适应 + 边界层」是 XQ 体网格阶段值得对齐的常用方案(目前 XQ 是否有待核)。

---

## 4. 对 XQ 本任务(07-06 沿路径描轮廓)方向的直接含义

1. **手绘降级为兜底**(用户已明确):XQ 分割方法工具栏应把**自动方法(断面阈值 / 水平集 / 区域生长 / 未来 ML)放主区常用**,手绘圆/多边形收进「不得已修正」的次级位置。这与 SV 完全一致。
2. **P3-2 已做的手绘不作废,但定位调整**:手绘作为「主链验证 + 自动失败兜底」保留(SV 也一直保留手绘)。它已打通「断面 2D → unprojectFromFrame → 入组 → 放样」的坐标+数据链,**这条链是所有自动方法也要走的**(自动方法产出的也是断面 2D 轮廓点,同样经这条链入组放样)。所以 P3-2 是**必要地基**,不是弯路。
3. **下一步应直接做自动方法**:按 SV 优先级,**断面阈值(Threshold)** 最简单可靠(纯图像处理,无需训练模型),应是 XQ 自动分割第一个落地的;其次 **区域生长 / 水平集**(种子驱动);**ML** 需要模型资产,最后做。
4. **批量是自动方法的放大器**:自动方法一旦可靠,「沿整条路径批量一次描完」才是真实工作流,这是相对手绘的质变。

> **任务可改**(用户已授权):原六批计划(P3-2 手绘 MVP → P3-3 椭圆样条+断面阈值 → P3-4 水平集/区域生长)可重排,把**自动方法提前为主线**、手绘相关的椭圆/样条**降级或砍**。具体重排见后续与用户确认的新计划。
