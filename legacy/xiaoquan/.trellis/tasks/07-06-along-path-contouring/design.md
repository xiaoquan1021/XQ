# 技术设计:沿血管中心线逐层轮廓提取

> 语境锚点:XQ 医学影像软件的血管几何重建。本设计描述「沿路径逐层描轮廓」工作流的技术实现,对标 SimVascular sv4gui 分割模块。纯几何建模 + 影像可视化工程。

## 0. 决策快照(用户已确认)

- **第一版完整对齐 SimVascular**(全套自动 + 手绘 + 批量 + 多血管)。
- **放样预览:手动触发**(用现有「选轮廓组 → 点放样」链,不做每加一轮廓实时重建)。
- **断面视图落位:轮廓阶段专用断面视图**(进入轮廓提取阶段右侧主视图区切换为断面工作台,退出还原四视图)。

## 1. 分层归属(铁律)

| 层 | 新增/改动 | 允许依赖 |
|---|---|---|
| **core** | `ContourFrame` 已有;可能补断面 2D 几何小工具(点在多边形内、面积)。纯 C++,无 VTK/Qt。 | 无 |
| **services/segmentation** | 新增断面 2D 轮廓生成:`ContourExtractionService`(断面阈值 / 断面区域生长 / 断面水平集,输入 2D 断面像素 + 参数,输出 2D 轮廓点)。纯域,无 VTK/Qt。 | core |
| **services/modeling** | 放样链已现成(ContourLoftInputBuilder + ModelingService),不改。 | core |
| **visualization** | **核心新增**:`XQCrossSectionResampler`(vtkImageReslice 封装,按 PathSamplePoint 坐标系出 2D 断面 vtkImageData);断面视图组件 `XQCrossSectionViewWidget`(挂 reslice 输出 + 断面内轮廓绘制交互 + 浮层)。碰 VTK,无 Qt 逻辑外泄到 core/services。 | core, VTK |
| **app** | `XQMainWindow` 接线:阶段切换驱动断面工作台显隐、滑条 ↔ reslice ↔ 断面视图、方法选择、轮廓入组命令、批量/多血管循环。 | 全部 |
| **ui/panels** | `XQStageWidgets` 轮廓提取页改造:路径下拉、方法工具栏、滑条、进入/退出编辑、批量/多血管入口。 | app 信号 |

**断面 2D 轮廓生成算法放 services 层(纯域)**是关键:阈值/区域生长/水平集在 2D 断面像素上算,可 headless 测(离散不变量),不依赖 VTK。reslice 出的 2D 断面像素由 visualization 层喂给 service。

## 2. 断面重采样(技术核心,R1)

### 2.1 XQCrossSectionResampler(visualization 层新增)

封装 `vtkImageReslice`:

- 输入:resident 体数据 vtkImageData(复用 XQRenderScene 的那份,或独立持有引用)、一个 `PathSamplePoint`(position + tangent + normal + binormal)、输出断面尺寸(mm 范围 + 分辨率,如 40mm × 40mm、0.2mm/px)。
- 设 `ResliceAxes`:x 轴 = normal,y 轴 = binormal,原点 = position,z 轴 = tangent(断面法向 = 路径切向)。
- `SetInterpolationModeToCubic()`(对标 SV RESLICE_CUBIC)。
- `SetOutputDimensionality(2)`。
- 输出:一张 2D vtkImageData(断面像素)+ 该断面的世界坐标系(= ContourFrame:origin=position, normal=tangent, xAxis=normal, yAxis=binormal)。

**断面坐标系 ↔ XQContour::frame 一致性**是端到端正确的命门:reslice 的 ResliceAxes 必须与写进 XQContour 的 ContourFrame 完全对应,这样断面 2D 像素坐标 →(unprojectFromFrame)→ 世界坐标 → 放样,几何才对得上。这一条要在 P3-1 用离散测试锁死(取一个已知 frame,reslice 中心像素反投影回世界坐标应等于 path position)。

### 2.2 XQCrossSectionViewWidget(visualization 层新增)

- 挂 reslice 输出的 vtkImageSlice(独立 vtkRenderer,不复用四视图的)。
- 窗位与主视图联动(共享 window/level)。
- 断面内轮廓绘制交互层:鼠标事件 → 2D 断面像素坐标 → 当前方法的控制点/生成逻辑 → 预览轮廓 actor。
- 视图内浮层(R10):当前方法操作提示、Esc 退出、空态引导。

### 2.3 专用断面工作台落位(用户已定)

参考 `XQMprWidget` 的 QGridLayout + frame 显隐机制。方案:
- 主视图区(现在放 XQMprWidget)在轮廓提取阶段切换为「断面工作台」容器:顶部沿路径滑条 + 中间断面视图(XQCrossSectionViewWidget)+(可选)一个小的路径全局定位视图。
- 退出轮廓提取阶段还原 XQMprWidget 四视图。
- 切换用 QStackedWidget 或主视图容器 setCurrentWidget,别销毁重建(reslice pipeline 保持常驻,切换只是显隐 + 重新 attach 数据)。

## 3. 沿路径定位滑条(R2)

- 滑条范围 = 绑定路径的 samplePoints() 索引 [0, M-1](或弧长)。
- 值变 → 取该 PathSamplePoint → 驱动 XQCrossSectionResampler 重采样 → 断面视图刷新。
- 显示「第 N/M 点」+ 弧长。对标 SV UpdatePathPoint。
- 该采样位置若已有轮廓,断面视图叠加显示已存在的轮廓(供查看/重描)。

## 4. 轮廓组绑路径(R3)

- 创建轮廓组对话框:第一项「选择路径」下拉(枚举场景内所有 XQPath 节点),对标 SV ContourGroupCreate。
- 创建时 `contourGroup.setSourcePathNode(pathNode)`;组名留空默认 path 名。
- 进入轮廓提取阶段时,从选中轮廓组读 sourcePathNode → 加载该 path → 滑条绑定其 samplePoints。

## 5. 断面 2D 轮廓生成

### 5.1 手绘(R4,交互在 visualization,几何在 core/service)

各类型的控制点 → 轮廓点算法**照 SV 参照**(已亲读 sv4gui_Contour*.cxx):

| 类型 | 控制点 | 生成算法(2D 断面坐标) | ContourType |
|---|---|---|---|
| 圆 Circle | 2(圆心 + 边界) | radius=dist(c0,c1),采样 ≥36 点 `c + r(cosα,sinα)` | Circle |
| 椭圆 Ellipse | 3~4(中心 + 两半轴) | 参数方程采样 | Ellipse |
| 多边形 Polygon | ≥3 顶点 | 顶点 + 段间线性插值(SV:前 2 点 center/scaling,实际从第 3 点起) | Manual/SplinePolygon |
| 样条多边形 SplinePolygon | ≥3 控制点 | Catmull-Rom / 样条插值加密 | SplinePolygon |

- 交互:圆 = 点圆心 + 拖半径;多边形/样条 = 逐点点击 + 双击/回车闭合;Esc 取消。
- 生成的 2D 点(u,v)→ `XQContourGroup::unprojectFromFrame(frame, u, v)` → 世界坐标 → XQContour{ pathArcLength, frame, type, points, closed=true }。
- **XQ 数据模型直接存世界坐标 points**(不像 SV 存控制点),所以生成即定,重描=替换该 arcLength 位置的轮廓。

### 5.2 断面阈值自动(R5)

`ContourExtractionService::thresholdContour`(services/segmentation,纯域):
- 输入:2D 断面像素(灰度)+ 强度阈值区间 + 断面内种子/中心点。
- 算法:阈值二值化 → 从中心点取包含它的连通域 → 提边界 → Marching Squares / 边界追踪 → 有序闭合 2D 轮廓点。
- **范围被断面像素限死**,天然不出实心块(对比整卷阈值)。ContourType=ThresholdResult。
- 阈值默认按当前断面直方图估计(对标 SV;避免 0/255 无效默认)。

### 5.3 断面水平集/区域生长自动(R6)

`ContourExtractionService::levelSetContour` / `regionGrowContour`(services/segmentation,纯域):
- 区域生长:2D 断面内从种子 6/8-连通 flood-fill within 阈值 → 边界追踪 → 闭合轮廓。
- 水平集:2D 断面内曲线演化(对标 SV LevelSet2D;第一版可先用测地活动轮廓的简化实现,参数少)。ContourType=LevelSetResult。
- 都在 2D 断面上算,可 headless 测。

## 6. 放样打通(R9,手动触发)

- **不做实时预览**(用户已定)。沿路径放若干轮廓后,用现有「选轮廓组 → 建模页 Loft」链:`ContourLoftInputBuilder::buildLoftInput(group)` → `ModelingService::loftSurface` → capModel → 血管表面节点。
- 只需保证 P3-2 结束时,断面上手绘的轮廓入组后 orderedByPathPosition 正确、frame/points 世界坐标对,现有放样链就能出血管。
- 端到端验证:真实工程沿一段路径手绘 3~5 个圈 → 建模 → 出可见血管管。

## 7. 批量与多血管(R7/R8)

- **批量**:取路径 samplePoints 的一个子集(整条或区间,可设步长)→ 对每个采样位置用当前自动方法生成轮廓入组。>阈值个数给进度反馈(对标 SV >50 弹确认 + 进度条)。批量循环在 app 层,单点生成复用 §5 的 service。
- **多血管**:遍历多条选中路径,各自建轮廓组 + 批量。app 层循环。
- 批量只对**自动方法**有意义(手绘无法批量);批量入口在自动方法激活时才可用。

## 8. 显式易用性(R10,补 SV 欠的层)

- 进入/退出「断面编辑」显式按钮(非隐式选中节点激活)。
- 断面视图浮层:当前方法怎么画 + Esc 退出(比 SV 藏 tooltip 显式)。
- 空态引导:未绑路径 → 「先创建或选择一条中心线路径」;已绑未采样 → 「拖动上方滑条定位断面」。
- 方法选择用 radio/toolbar 清晰态(非 SV「点两次变蓝」)。

## 9. 测试策略(离散不变量 + 真机)

headless 可测(ctest):
- **reslice frame 一致性**(P3-1 命门):已知 PathSamplePoint → resampler 的输出 frame == 预期 ContourFrame;断面中心像素反投影回世界 == path position(容差内)。
- **手绘算法**:给定控制点 → 生成轮廓点数/闭合/半径正确(圆:所有点到圆心距离≈r;多边形:顶点在轮廓点集中)。
- **断面阈值/区域生长**:构造合成断面(中心高信号圆斑 + 背景),提取轮廓应闭合且面积≈圆斑(不吞背景=不出板砖,可证伪:篡改成全卷阈值必转红)。
- **绑路径**:轮廓组 setSourcePathNode 后 sourcePathNode 正确;入组轮廓 pathArcLength 单调、orderedByPathPosition 有序。
- **放样打通**:合成一组沿路径轮廓 → ContourLoftInputBuilder → loftSurface 出非空表面(几何守恒断言,别只测背景非黑)。

**真机(第一优先,ctest 全绿≠达标)**:见 prd §7。每批派前先真机验基线(尤其 P3-1 断面重采样对不对——架构先验收再盖楼)。

假绿抽查:每个新断言篡改被测逻辑必须真转红(核 exe 时间戳变)后还原(memory `partition-invariant-verify-at-source-not-pixels`、`ninja-target-incremental-fakegreen-trap`)。

## 10. 风险与对策

| 风险 | 对策 |
|---|---|
| reslice frame 与 ContourFrame 对不上 → 放样几何错位 | P3-1 用一致性离散测试锁死,先于任何交互 |
| 断面视图每帧重建 GL 上下文卡顿(重蹈 GUI v2 覆辙,memory `render-architecture-must-be-validated-first`) | reslice pipeline 常驻,滑条只改 ResliceAxes 参数 + 单次 render;先真机验基线体验再盖交互 |
| 断面阈值仍出实心块 | 范围限死在断面像素 + 连通约束 + 中心种子;合成断面测试可证伪 |
| 水平集参数难调、第一版做不完美 | 第一版先保证区域生长稳,水平集给合理默认 + 参数暴露,真机迭代 |
| 改 Q_OBJECT 头陈旧 moc 崩溃 | 改头必 rm -rf build_gui(memory `ninja-stale-moc`) |
| SV 源码派 agent 被安全机制拦 | 主会话亲读(已亲读 Contour/Circle/Polygon;阈值/水平集算 design 具体批次再读对应 SV 文件) |

## 11. 待 design 具体批次时补读的 SV 参照

- `Modules/Segmentation/sv4gui_ContourEllipse.cxx`、`sv4gui_ContourSplinePolygon.cxx`、`sv4gui_ContourTensionPolygon.cxx`(椭圆/样条算法)。
- `sv4gui_ContourModelThresholdInteractor.cxx`(断面阈值交互)。
- `Plugins/.../sv4gui_LevelSet2DWidget.{cxx,ui}` + `Modules/Segmentation` 水平集实现(2D 水平集参数与演化)。
- `Plugins/.../sv4gui_Seg2DEdit.cxx`(滑条 UpdatePathPoint、方法状态机、批量 CreateContours、多血管 segmentPaths 的接线细节)。

这些属实现批次的算法参照,主会话在对应批次亲读(不派 agent)。
