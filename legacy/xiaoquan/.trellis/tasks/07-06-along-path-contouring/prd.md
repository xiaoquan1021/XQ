# 沿血管中心线逐层轮廓提取(对标 SimVascular)

> 语境锚点:XQ 是正当的医学影像分析桌面软件,面向科研/临床前的血管几何重建。本任务把「血管腔轮廓提取(分割)」阶段做成对标开源软件 SimVascular 的沿血管中心线逐层描轮廓的临床标准流程。全程是几何建模与影像可视化工程。

## 1. Goal

把血管腔轮廓提取阶段从**已证伪的整卷强度阈值**重构为**沿中心线逐层描轮廓**:选一条中心线路径 → 沿路径法向逐点切出垂直断面图 → 在每个断面上生成闭合血管腔轮廓(自动或手绘)→ 一组轮廓放样成血管表面。第一版目标是**完整对齐 SimVascular** 的 2D 分割工作流(用户已确认范围)。

## 2. Background(为什么重构,别重蹈覆辙)

- 用户真机反馈:当前「整卷阈值提取」对真实工程 `0080_H_PULM_H` 输出一整块实心矩形块,不是血管。
- 根因已诊断清(memory `xq-threshold-seg-fullblock-bug`):整卷强度阈值本质上分不出血管——MR/MRA 影像里软组织强度都落在中间区间,凡「不接近纯黑」的体素都被选中=整个躯体轮廓。B6c 止血补丁(取高端百分位 + 保留最大连通区域,commit `85c357c`)用户实测仍未解决,证明这条路方向就错。
- **结论:不在整卷阈值上打补丁。** 唯一出路是本任务的沿路径 2D 轮廓流程。B6c 止血保留(至少不出实心块),但它不是目标。

## 3. Scope(用户已确认)

**第一版完整对齐 SimVascular**,包含:

| 能力 | 说明 |
|---|---|
| 断面重采样视图 | 垂直于路径法向切出的 2D 断面图(vtkImageReslice 按 PathSamplePoint 坐标系设 ResliceAxes) |
| 沿路径定位滑条 | 沿路径弧长滑动,驱动断面重采样,对标 SV `sv4guiResliceSlider` |
| 轮廓组绑路径 | 创建轮廓组时下拉选一条路径绑定,沿路径每个采样位置放一个轮廓(对标 SV ContourGroupCreate 首项 Select Path) |
| 手绘轮廓 | 圆(2 控制点)、椭圆、多边形、样条多边形(点 + 拖) |
| 自动轮廓:断面阈值 | 在断面内做强度阈值 + 连通,自动描一圈(范围被断面限死,不出实心块) |
| 自动轮廓:断面水平集/区域生长 | 断面内从种子点演化出轮廓 |
| 批量模式 | 一次沿整条路径多个采样位置循环生成轮廓 |
| 多血管模式 | 多条路径一键批量 |
| 放样打通 | 一组沿路径轮廓 → 现有 ModelingService 放样 → 血管表面(数据链已通,验证端到端) |

**断面视图落位(Axial 格切换 vs 新开专用视图)由 design 阶段定**(用户授权我读现有 XQMprWidget/XQRenderScene 布局后决策)。

## 4. Non-Goals(明确不做)

- 不在整卷阈值提取上继续打补丁(已证伪)。
- 不碰网格/仿真/AI 阶段(P3 只碰路径 → 轮廓提取 → 建模链)。
- 不参考 XQ1/ 任何东西(失败产物,spec 硬规则)。
- 不照抄 SimVascular 隐晦的可发现性缺陷(选中节点才激活面板、按钮点两次、操作说明全藏 tooltip、模式态只有一个按钮变蓝)。XQ 目标是**更显式**:明确的进入/退出编辑按钮、视图内浮层操作提示、空态引导。

## 5. 现状(数据结构已就位,缺的是交互 + 断面视图)

已有,不用重建:

- `core/XQContourGroup.h`:`XQContour{ contourId, pathArcLength, ContourFrame frame, ContourType type, points, closed }`;`XQContourGroup` 有 `setSourcePathNode/hasSourcePathNode/sourcePathNode`、`addContour`、`orderedByPathPosition`、`projectToFrame/unprojectFromFrame`(世界坐标 ↔ 断面 2D 坐标)。**断面坐标系概念已在数据模型里。** ContourType 已含 Circle/Ellipse/SplinePolygon/LevelSetResult/ThresholdResult/Manual。
- `core/XQPath.h`:`PathSamplePoint{ position, tangent, normal, binormal, arcLength }`;`samplePoints()`、`frameAtArcLength(arcLength, out)`、`framesForAllSamples`。**沿路径每点的法向断面坐标系已能算。**
- `services/modeling/ContourLoftInputBuilder.h` + `ModelingService::loftSurface/capModel`:轮廓组 → 放样成血管表面 → 封口,**放样链完全现成**(order by path position + 重采样 + 抗扭转 + 邻环缝合)。
- `visualization/XQRenderScene.h`:四视图(Axial/Sagittal/Coronal/Volume3D)、切片、窗位、十字线拖拽、种子标记、轮廓叠加显示(`addContourGroup`/`nodeContourOverlayCount`,按到切片距离过滤显示已存在的世界坐标轮廓)。
- `visualization/XQMprWidget.h`:2×2 四视图容器 + Single 布局模式。
- B6a/B6b 易用性层:轮廓组已是数据管理器节点、可下拉选、右键直达建模、模式浮层/引导条基础设施都在。

**核心缺口**:渲染层**完全没有 vtkImageReslice**(全仓 `rg Reslice` 零命中)——断面重采样是纯新增能力。这是本任务的技术核心。

## 6. Requirements

### R1 断面重采样视图
- 用 vtkImageReslice 按 `PathSamplePoint` 的 {tangent, normal, binormal} 设 ResliceAxes,三次插值,输出一张垂直于路径法向的 2D 断面图。
- 断面视图挂进 GUI(具体落位 design 定),进入「轮廓提取」阶段且选中一条绑定路径时激活。
- 断面图跟随路径滑条实时更新(对标 SV RESLICE_CUBIC)。

### R2 沿路径定位滑条
- 顶部滑条沿路径弧长滑动,当前采样位置驱动 R1 断面重采样。
- 显示当前位置(第 N/M 个采样点或弧长),对标 SV `UpdatePathPoint`。

### R3 轮廓组绑路径
- 创建轮廓组对话框第一项是「选择路径」下拉(列出场景内全部 path 节点),轮廓组从创建起挂在一条路径上(写入 `setSourcePathNode`)。
- 组名留空默认用 path 名。

### R4 手绘轮廓(断面 2D 交互)
- 圆:点圆心 + 拖半径(2 控制点,采样 ≥36 点,对标 SV ContourCircle)。
- 椭圆:对标 SV ContourEllipse。
- 多边形:点若干控制点、双击/回车闭合,段间插值(对标 SV ContourPolygon,前 2 点为 center/scaling)。
- 样条多边形:控制点 + 样条插值(对标 SV ContourSplinePolygon)。
- 生成的 2D 断面坐标经 `unprojectFromFrame` 回世界坐标存进 `XQContour`,写正确的 `pathArcLength` 与 `frame`。

### R5 自动轮廓:断面阈值
- 在当前断面 2D 图内做强度阈值 + 连通,自动描一圈闭合轮廓。
- 范围被断面限死,不会出实心块。类型标 `ThresholdResult`。

### R6 自动轮廓:断面水平集/区域生长
- 断面内从种子点出发,水平集或区域生长演化出一条闭合轮廓。类型标 `LevelSetResult`。

### R7 批量模式
- 取一串采样位置(整条路径或区间)循环处理,用当前方法批量生成轮廓入组。
- 大批量给进度反馈(对标 SV >50 个弹确认 + 进度条)。

### R8 多血管模式
- 多条路径各建轮廓组、一键批量生成(对标 SV Multi-vessel 页)。

### R9 放样打通
- 一组沿路径轮廓 → `ContourLoftInputBuilder` → `ModelingService::loftSurface` → 血管表面,验证端到端在真实工程 `0080_H_PULM_H` / `0007_H_AO_H` 上跑通。
- 实时放样预览(对标 SV Lofting Preview,每加一个轮廓更新)——若工作量过大可降为「手动触发放样」,在 design 定。

### R10 显式易用性(补 SV 欠的那层)
- 明确的进入/退出「断面编辑」按钮,而非隐式选中节点。
- 断面视图内浮层操作提示(当前方法怎么画、Esc 退出)。
- 空态引导(未绑路径时提示先建/选路径)。
- 方法选择用清晰控件,不是「点两次变蓝」。

## 7. Acceptance Criteria

- [ ] **真机(第一优先)**:在真实工程 `0080_H_PULM_H` 与 `0007_H_AO_H` 上,能选一条路径 → 沿路径滑条移动 → 断面视图实时显示垂直断面 → 在断面上手绘至少圆/多边形两种轮廓 → 沿路径放若干轮廓 → 放样出一段可见的血管表面。断面视图不卡顿。
- [ ] **真机**:断面阈值自动方法在真实断面上能描出贴合血管腔的一圈,不出实心块。
- [ ] **真机**:断面水平集/区域生长自动方法能从种子演化出闭合轮廓。
- [ ] **真机**:批量模式沿整条路径生成一串轮廓;多血管模式对多条路径批量。
- [ ] **真机**:轮廓组创建绑路径下拉可用;进入/退出编辑显式;断面内有浮层操作提示;空态有引导。
- [ ] ctest 全绿(基线 68 + 新增断面重采样/轮廓生成/绑路径的离散不变量测试),Release 全量。
- [ ] 假绿抽查纪律:每个新断言篡改被测逻辑必须真转红(核对 exe 时间戳真变)后还原绿。
- [ ] 新增可译串已手工加进 `resources/i18n/xq_zh_CN.ts` 并 lrelease 报 0 unfinished。
- [ ] `rm -rf build_gui` 全新构建通过(改 Q_OBJECT 头后强制)。

> **硬门禁**:GUI/产品类任务 ctest 全绿 ≠ 达标。轮廓提取质量只有真机对真实工程才验得出——真机目视是第一优先,不是收尾待办(memory `gui-task-green-tests-not-done`、`render-architecture-must-be-validated-first`)。

## 8. Constraints(硬性规范,违反=返工)

- 改 Q_OBJECT 头后必须 `rm -rf build_gui` 全新构建(memory `ninja-stale-moc`)。
- 构建/测试用 `.bat` + cmd //c 绝对路径;ctest 走 `ctest_merge.bat`(基线 68 全绿);冒烟 `run_xq.bat`。
- 新建源文件注释写英文(XQ CMake 无 /utf-8 无 BOM,中文注释被 MSVC 按 GBK 误读,memory `msvc-gbk`)。
- i18n 加串手工编辑 `resources/i18n/xq_zh_CN.ts`(UTF-8 无 BOM + LF),绝不跑 lupdate(GUI 串走 xqTr=translate,lupdate 扫不到还会删在用串,memory `xqtr-translate-lupdate-blind`);改完 lrelease 应报 0 unfinished。
- channel worker 流程:主审写 EXECUTE 简报 → spawn implement → check worker 复核 → **主审亲读最终代码 + 全新构建复跑 + 假绿抽查**(不能只看 worker PASS/FAIL 报告,memory `subagent-mainreview-must-read-code`)→ commit → 真机。done 事件 ≠ 工作树干净,认账前必 git diff 核残留(memory `worker-handedit-ts-bom-crlf`)。
- worker 名额上限 6,旧 channel 完工后清理。
- 分层铁律:core 纯域(无 VTK/Qt),services 纯域,visualization 才碰 VTK,app 接线。断面重采样属 visualization 层。
- **架构先验收再盖楼**(memory `render-architecture-must-be-validated-first`):断面视图是新渲染能力,派批次前先真机验断面重采样基线体验对不对,别让 headless 可测性反向决定产品架构。
- 提交:中文信息「类型: 描述」;精确 `git add <路径>` 绝不 `-A`/`.`(memory `commit-check-gitignore`);结尾 `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`。
- 模型误降级:分割/种子/阈值/区域生长等词撞 Fable 安全词表会自动降级 Opus,工作不中断;读 SV 分割源码别派 subagent(几乎必被拦),主会话自己读(memory `fable-safeguard-fallback-neutral-wording`、`sv-research-agent-cyber-falsepositive`)。

## 9. 关键坐标

- worktree(唯一改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`,HEAD = `85c357c`(B6c)。
- 主仓(Trellis 任务/spec/SV 参照源码):`C:\Users\OCEAN\Desktop\XIAOQUAN`,分支 `fix/xq-global-audit`。
- SV 参照:`Externals/src/SimVascular/Code/Source/sv4gui/`(在主仓,不在 worktree)。Plugins/org.sv.gui.qt.segmentation/sv4gui_Seg2DEdit.{cxx,h,ui} + Modules/Segmentation/sv4gui_Contour*.cxx。
- 真实测试工程:`0080_H_PULM_H`(143 path + 143 contour,大);小工程 `0007_H_AO_H`(1 image + 1 model + 5 contour)。
- Python:`C:/software/anaconda/python.exe`。

## 10. 交付分批建议(design/implement 细化)

拟按 SV 工作流依赖顺序分批(每批 implement→check→主审亲读复跑→真机):
1. **P3-1 断面重采样视图 + 沿路径滑条**(技术核心,先真机验基线断面对不对再往下)。
2. **P3-2 轮廓组绑路径 + 手绘圆/多边形**(打通端到端 MVP:手绘几个圈放样出血管)。
3. **P3-3 手绘椭圆/样条 + 断面阈值自动**。
4. **P3-4 断面水平集/区域生长自动**。
5. **P3-5 批量 + 多血管 + 放样预览**。
6. **P3-6 显式易用性打磨(浮层/空态/进入退出)+ 真机收口**。

分批粒度与依赖在 implement.md 定死。
