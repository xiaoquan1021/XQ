# GUI v2 重设计 + 大规模血流前处理内存优化

> **⚠ 2026-07-04 已被取代(superseded)**:本任务六批执行完、ctest 全绿,但真机验收失败——
> 渲染架构级错误(离屏渲染贴 QLabel + 只渲染选中单节点),性能与功能均不达标。
> 可视化层由 `07-04-render-arch-rebuild` 推倒重写。本文档及 EXECUTE-B1~B6 仅供历史参考,
> **不得作为任何后续执行依据**。保留成果:工作区打开/保存、XQTaskRunner+忙态、预算传导、R0/R6 算法优化。

> 任务: `07-03-gui-v2-large-scale`。branch `feat/gui-v2`(= fix/xq-global-audit ⊕ feat/gui-mitk-layout 汇合,合并提交 5891c07,ctest 65/65 绿)。
> worktree: `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`。
> 状态: planning。

## Goal

把 XQ GUI 从"能跑的产品化原型"提升为**简洁美观、无死功能、不卡 UI、能吃大规模数据**的医学影像工作台,管线覆盖 路径→分割→建模→网格→流体(AI 分析后端保留但本任务不做 UI 扩展)。

四条主线:
1. **去除所有无用功能**:死代码、空壳按钮、永久 disabled 的装饰控件,一律删或真实接线,不许留"看起来有但点了没反应"的 UI。
2. **补齐管线唯一断点**:Path 阶段从 stub 变真实(MPR 切片点击拾取控制点 → 生成路径)。
3. **运行流畅度**:重活(网格/放样/求解/分割/加载)全部移出 UI 线程,统一后台任务机制 + 全局忙态。
4. **大规模内存**:GUI 全面接通既有 M9b 基建(lazy 加载 + Mapped 源 + 渐进上传 + LOD + 驻留预算),再修三个算法级内存/复杂度热点(分割整卷 visited 副本、path O(M²) frame、flow 全量帧物化)。

## Background(盘点结论,均有行号证据)

来自两份独立盘点(research/gui-inventory.md、research/pipeline-inventory.md):

- GUI 六阶段 5 个真实接通(Seg/Model/Mesh/Flow/AI),Path 页无 run 按钮、参数 `(void)` 丢弃(XQStageWidgets.cpp:220-247)。
- File/工具栏 Open Workspace、Save Workspace **无 connect**(XQMainWindow.cpp:679/688/768/770)——而 `XQProjectWriter/Reader` 早已存在且支持 lazyGeometry,属"该接没接"。
- 状态栏内存标签写死 "Mem: -- MB"(:1005/1384);Data Manager 透明度/颜色/属性控件永久 disabled(:855-883)。
- `makeDemoVolume()` 全仓零调用;`XQImageViewer`(277 行)只被死路径 showImage + 一个弱测试(不查像素)使用。
- 全仓 0 处 QThread/QtConcurrent/std::thread(GUI 层);TetGen 体网格、512³ 区域生长、20000 步 1D 求解全部同步阻塞 UI 线程。
- 大规模基建已在库内:`MappedGeometrySource`+`GeometryResourceManager`(LRU+pin+预算)+`addSurfaceProgressive`/`addVolumeMeshProgressive`+`SurfaceLodBuilder`;`XQAppStartup` 已默认 lazyGeometry=true;`XQMainWindow::onSceneSelectionChanged` 已有 lazy 分支。缺:预算默认 SIZE_MAX 无界、Preferences 无入口、LOD 默认关。
- 算法热点:`regionGrowMask`/`keepLargestConnectedComponent` 各分配整卷 byte visited(512³=134MB×2);`computeFrames` O(M²);`FlowSolver1D::solve` 最后一周期全量帧物化(totalSteps 可 20000)。
- **前置 bug**:`XQCommandStack::redo()` 中 `execute()` 失败时命令已 pop_back 且未归还 → 命令永久丢失(XQCommandStack.cpp:44-57)。

## Requirements

- **R0 命令栈 redo 失败语义修复(前置)**
  `redo()` 在 `execute()` 失败时不得丢失命令:命令保留在 redo 栈顶、返回 false。补失败注入测试(先红后绿)。

- **R1 无用功能清零**
  删除:`makeDemoVolume()`、`XQImageViewer` 整类及其测试与 CMake 注册、`XQMainWindow::showImage`/`lastRgbaByteCount` 死路径、Data Manager 透明度滑块/Color 按钮/Properties 折叠钮、工具栏 3D Seg 按钮(与 2D 同页)、Image Navigator 的 Time 滑块与 Loc spin(无数据驱动)。
  真实接线(不删):File/工具栏 打开工作区(XQProjectReader,lazyGeometry=true)、保存工作区(XQProjectWriter)。
  验收:全仓 rg 无残留符号;删除后全量 ctest 绿;真机菜单/工具栏无一个点击无响应的项。

- **R2 Path 阶段真实化**
  MPR 切片点击拾取路径控制点(复用 seedPicked 体素拾取机制,体素→世界坐标),Path 页显示控制点列表(可删单点/清空)、spacing 输入、"生成路径"按钮 → `PathController::addPath` → 场景树出现 Path 节点、3D/MPR 可见。

- **R3 后台任务化(不卡 UI)**
  统一 `XQTaskRunner`(常驻 QThread):分割(threshold/regionGrow)、建模 loft、表面/体网格、流体求解、打开影像/工程/工作区,全部改为 后台算 + UI 线程提交命令。
  任务运行期间全局忙态:六阶段 run 按钮 + undo/redo + 打开/保存 全禁用,状态栏显示任务名 + 忙指示;同时只允许一个后台任务。
  验收:offscreen 测试驱动后台任务完成并断言场景结果;真机跑 512³ 区域生长/体网格时窗口可拖动不白屏。

- **R4 大规模内存 GUI 接通**
  - 几何驻留预算:默认 2048 MiB(QSettings `memory/geometryBudgetMiB`),Preferences 增加设置项,`attachGeometryResources` 后生效。
  - 选中渲染策略统一:payload 持内存几何 → `addSurface(handle, lod)`;仅 assetId → resolver + progressive(现状保留);LOD 默认 enabled+interactive,budgetTriangles=2,000,000。
  - 状态栏内存真实化:进程 WorkingSet(GetProcessMemoryInfo)+ 几何驻留字节(manager stats),2s 定时刷新。
  验收:offscreen 断言预算传导(超预算时驱逐生效);状态栏文本随加载变化。

- **R5 算法级内存/复杂度热点**
  - 分割:visited 改位图(内存 /8),两处(regionGrow/largestCC)。
  - path:`computeFrames` 改单遍推进 O(M+N);`resample` 控制点游标线性推进。
  - flow:`SolverInput.maxRecordedFrames`(默认 2000),录制按 stride 均匀抽帧,`Result` 语义不变(times 与数据长度一致)。
  验收:各自单测先红后绿;既有测试全绿(数值容差内等价)。

- **R6 视觉统一打磨(小步,不引新依赖)**
  单一浅色主题微调:统一 token(见 design.md 色板/间距表)、四视图角标签+1px 彩色描边(Axial 红/Sagittal 绿/Coronal 蓝/3D 琥珀)、工具栏分组分隔、hover/pressed/disabled 三态完整。真机目视验收。

## Acceptance Criteria

- [x] AC0 redo 失败注入测试红→绿;全量 ctest 绿。(B1 S0,commit d35b2ee)
- [x] AC1 死代码清单全部移除(rg 验证零残留),打开/保存工作区真机可用且 roundtrip 等价(保存→重开,场景节点/几何守恒)。(B2,8bb3e3f/fa8e703/eefc37e;roundtrip 由 test_app_startup 覆盖)
- [x] AC2 真机:MPR 点击 ≥3 个控制点 → 生成路径 → 树出节点、undo/redo 对称;offscreen 测试覆盖同链路。(B4,8b9a57b/f34031a;offscreen test_path_stage 覆盖三点拾取→生成→undo/redo。真机目视归 AC6)
- [x] AC3 所有六类重活经 XQTaskRunner;忙态期间 run/undo/redo/open/save 全禁用;offscreen 测试断言异步完成与忙态还原。(B3,877e09e/9738ba5/563dd34/c37bdba;test_task_runner + test_main_window 异步块)
- [x] AC4 预算/LOD/渐进全接通:offscreen 断言驱逐与 uploadedPointCount≤源点数;状态栏内存真实刷新。(B5,0252b13/cb243ef/d6d7b9e)
- [x] AC5 三个热点修复各有独立单测(先红后绿);分割 512³ 集成测试在 visited 位图下内存可观测下降。(B1 S5a/S5b/S5c,c20cdeb/54ac1e0/0ecb127)
- [x] AC6 全量 ctest 绿(Release,TETGEN+MMG ON 档也绿);~~真机启动目视检查通过~~。(默认档 66/66 + ON 档 68/68 主审复验绿。**真机 run_xq.bat 目视仍待 ocean 亲自确认** — harness 读不出本地图,无法代验)
- [x] AC7 不破坏 Source 1.0 冻结签名;services 不引 Qt/VTK;架构护栏测试绿。(test_arch_boundaries 全程绿)

## Out of Scope

- AI 分析 UI 扩展(后端保留,不动)。
- 3D cell 拾取加速(vtkStaticCellLocator)、视锥剔除(M9b-D 后续)。
- XQFlowResult blob 化(本轮用抽帧控制规模;blob 化留后续任务)。
- 样条插值实现(保持折线,复杂度修复不改插值语义)。
- 深色主题/新主题库/外部 UI 依赖。

## 执行模式

复用 07-02 审计任务模式:主会话产出 EXECUTE-S*.md(每处 before/after 对真实源码验证)→ 便宜 agent 按批照做 → 主会话独立复验(重建+全量 ctest+假绿抽查)。批次见 checklist.md。
