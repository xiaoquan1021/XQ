# Implement — GUI 接真实影像管线 (`06-30-gui-real-pipeline`)

> 配套 `prd.md` + `design.md`。在 worktree `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`(分支 `feat/gui-mitk-layout`)执行。
> 每步改完即构建 + 跑相关测试,绿了再下一步。**不动 services/adapters/io/core 算法,不碰大规模 handle。**

## 验证命令(固定)
```bash
# 构建(worktree 专用脚本)
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"

# 全量 ctest(offscreen)
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui
export PATH="/c/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin:/c/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/vtk-9.3.0/bin:$PATH"
export QT_QPA_PLATFORM=offscreen
ctest --output-on-failure          # 基线 49/49 必须保持

# 真实样本
#   .vti  : C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H/Images/OSMSC0090-cm.vti
#   .ctgr : C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H/Segmentations/aorta_final.ctgr
#   .flow : C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H/flow-files/inflow_1d.flow
```
**字体/视图验证不靠离屏直方图**:glyph 可用性程序检查 + 用户真机目视(memory `harness-cannot-read-local-images`)。

## 执行顺序

### 步骤 0 — 基线确认
- [ ] 跑全量 ctest,确认 49/49 绿(改动前基线)。
- [ ] 确认真实 .vti/.ctgr/.flow 三个文件在盘(`ls`)。

### 步骤 1 — 字体根治(R4,最先做:不修字体后面都看不见)
- [ ] `main.cpp` + probe:`QFontDatabase::addApplicationFont` 加载雅黑(先试系统路径 `C:/Windows/Fonts/msyh.ttc`;不命中再评估嵌 qrc)。用返回 family 设 `app.setFont` + 对齐 qss。
- [ ] 写 glyph 可用性检查(`QRawFont::glyphIndexesForChars` 对「轴」断言 ≠0),作为一个小测试或 probe。
- [ ] 构建 + ctest 绿。**真机目视确认汉字非方框**(用户)。
- 回退点:仅动 main/probe/qss/资源,易回退。

### 步骤 2 — 第四格复核(R5)
- [ ] 真机/真窗口确认 Quad 四格全可见。已加黑底兜底;若仍缺,定位 grid span 残留 or VTK 控件尺寸,修。
- [ ] 构建 + ctest 绿。

### 步骤 3 — 打开真实 .vti(R1 核心,AC1)
- [ ] 主窗口加 `activeImage_` 成员(持有解码 volume+buffer,类比 `demoVolume_`)。
- [ ] File 菜单加「打开图像(.vti)」action(objectName 如 `xqOpenImageAction`,进 retranslateUi + .ts 中文)。
- [ ] 槽:QFileDialog → `VtkImageAdapter::loadVtiWithBuffer` → 失败 QMessageBox(LoadStatus 可读消息);成功:经 command seed Image 节点 + `mprView_->setImage(真实)` + 滑条 range 重设 + 状态栏维度。
- [ ] 构建 + ctest 绿(`test_main_window` 不退化)。
- [ ] **真机:打开 OSMSC0090-cm.vti,MPR 三视图显示真实主动脉切片(非 demo)**(AC1)。
- 回退点:纯增量 action + 成员,不改 showImage 签名。

### 步骤 4 — 打开 SV 项目(R1,AC2)
- [ ] File 菜单加「打开 SimVascular 项目」→ QFileDialog → `SvProjectReader::read` → 经 command 批量 seed Image/Path/ContourGroup/算例节点。
- [ ] 含图像则复用步骤 3 的 setImage 路径。
- [ ] 构建 + ctest 绿。**真机:打开 0007 项目,节点树出现 图像+路径+轮廓+波形**(AC2)。

### 步骤 5 — stage 接真数据:provider 注入(R2)
- [ ] 设计 provider:`populateStagePanels` 增注入(`std::function` 提供「当前活动 image+buffer」「当前选中轮廓组」「当前选中算例」)。面板执行时调 provider 填 Intent,不持有大规模数据、不依赖主窗口具体类型。
- [ ] 主窗口实现 provider(返回 `activeImage_`/选中节点 payload)。
- [ ] 解灰:阈值分割页接真 image/buffer(ThresholdIntent);Modeling 接选中轮廓;Flow 接选中算例。取不到→按钮禁用+提示「请先选中 X」。
- [ ] 构建 + ctest 绿(`test_workflow_*` 不退化)。
- 回退点:provider 是新增注入参数,旧调用可传空 provider 退化为现状。

### 步骤 6 — 阈值分割端到端(R2b 一半,AC3/AC3b 阈值部分)
- [ ] 选中真实图像 → 阈值分割页填上下阈值 → 执行 → 真掩膜节点入树(可撤销)。
- [ ] **真机:对 OSMSC0090 跑阈值分割出真掩膜**。

### 步骤 7 — 切片种子拾取(R2b 最高风险,AC3b 区域生长部分)
- [ ] XQMprView 加 picking 模式 + `seedPicked(int,int,int)` signal;记录每格离屏 `vtkRenderer*` + axis/slice。
- [ ] 实现方案 A(display→world→voxel,见 design §3.1);**先写往返校验测试**(体素→world→display→反推,误差≤1)。
- [ ] 校验不过则退方案 B(in-plane 线性映射 + 实测标定),implement 记录实际采用哪个。
- [ ] 分割页「区域生长」模式:进入点选→收 seedPicked→填 RegionGrowIntent.seed→执行;SeedOutOfRange/SeedNotInThreshold 明确提示。
- [ ] 构建 + ctest 绿(含新拾取往返测试)。**真机:点选种子跑区域生长出真掩膜**(AC3b)。
- 回退点:拾取是 MPR 新增模式,默认关闭不影响现有切片浏览。

### 步骤 8 — 结果渲染确认(R3,AC4)
- [ ] 确认每种结果节点 payload 与 `onSceneSelectionChanged` dispatch 分支匹配;选中分割/曲面/体网格/血流节点 → 3D 格渲染非空。基本现成,主要是验证 + 补缺。
- [ ] **真机:选中结果节点,3D 格出几何**(AC4)。

### 步骤 9 — 端到端链路 + 收尾(AC3/AC7/AC8)
- [ ] 从 GUI 跑通至少一条真实链路 路径→分割→建模→网格→血流→AI,结果入树可撤销(AC3)。
- [ ] 全量 ctest 绿、Release 绿、现有 49 不退化(AC7)。
- [ ] 代码评审:无大规模 handle 依赖、UI 只经 command+NodeId+AssetId、未动渲染器内核(AC8)。
- [ ] 清理一次性 probe/截图(不进 commit);中文 commit 精确列路径。

## 风险文件 / 回退点汇总
- `XQMprView.{h,cpp}`(步骤 7 拾取)——最高风险,坐标变换;往返测试守门,可退方案 B。
- `main.cpp`/资源(步骤 1 字体)——ttc 嵌入与否待评估。
- `XQStageWidgets.cpp` + provider(步骤 5)——注入改动面,旧路径用空 provider 退化。
- `XQMainWindow.cpp`(步骤 3/4 加载 + activeImage_ 所有权)——所有权悬空风险,成员持有。

## 收尾后续检查(task.py start 前不阻塞,完成时核对)
- [ ] AC1–AC8 + AC3b 全绿(真机项由用户确认)。
- [ ] 不违架构铁律(评审签字)。
- [ ] PROGRESS / memory 更新并 commit。

## 进度记录(2026-06-30 本会话)

worktree `feat/gui-mitk-layout`,全程 offscreen ctest 50/50 绿(基线 49 + 新增 test_ui_font_glyphs)。

**已完成(代码 + 测试,真机目视项待用户确认):**
- 步骤 1 字体根治(R4):`installUiFont` 显式 `addApplicationFont(msyh.ttc)`,新增 `test_ui_font_glyphs`(QRawFont glyphIndex 对 轴/中/文 断言 ≠0,非离屏直方图)。commit 285536b。**真机目视待确认。**
- 步骤 3 打开 .vti(R1/AC1):File 菜单「打开图像(.vti)」→ loadVtiWithBuffer → activeImage_ 持有 → useVolume 替换 demo + 状态栏维度 + seed Image 节点(可撤销)。commit da409a1。**真机目视待确认。**
- 步骤 5/6 阈值分割(R2/AC3b 阈值半):ActiveImageProvider 注入 populateStagePanels,分割页解灰跑 ThresholdIntent → 掩膜入树。commit fc63e81。
- 步骤 4 打开 SV 项目(R1/AC2):File 菜单「打开 SimVascular 项目」→ SvProjectReader::load → clone 节点并入活动 scene(可撤销)+ 含图像走 loadVtiWithBuffer 显示。commit 4926990。修 test_main_window 的 ctest PATH 补 tinyxml2(shell 现链 xq_io,否则 0xc0000135)。

**剩余(需真机交互/目视 或 更大整合,本会话未做):**
- 步骤 2 第四格复核(R5/AC6):纯真机目视。
- 步骤 7 切片种子拾取(R2b/AC3b 区域生长半):XQMprView 加 picking 模式 + display→world→voxel(XQImageVolume 已有 worldToVoxel,缺 MPR 离屏 renderer 的 display→world);最高风险,需往返校验测试 + 真机点选。
- 步骤 5 余项:Modeling 接选中轮廓组 / Flow 接选中算例 —— **障碍**:SV 载入的 ContourGroup/SimCase 节点只带 XQSourcePayload(路径),LoftIntent 需 XQContourGroup(已解析的环数据),需重读 .ctgr 并加「选中节点 provider」,非纯接线。
- 步骤 8 结果渲染确认(R3/AC4):基本现成,需真机选中节点看 3D 格。
- 步骤 9 端到端链路(AC3):真机跑通一条链。

## 进度记录(2026-07-01 本会话)

worktree `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`,分支 `feat/gui-mitk-layout`。本轮继续步骤 5/7/8/9 的最小真实链路接线,未改 services/adapters/io/core 算法。

**本轮新增(代码已实现,质量门已复跑;真机目视项除外):**
- 步骤 5 余项 Modeling:新增 `ContourGroupProvider`,从选中/指定 ContourGroup 节点的 `XQSourcePayload` 解析 `.ctgr`,填 `ModelingController::LoftIntent`,经 `ModelingController::loft` 和命令栈产出模型节点。
- 步骤 5/9 Flow 最小真实样例路径:新增 `FlowInputProvider`,从选中/首个 SimulationCase 找 mesh、contour group、`flow-files/inflow_1d.flow`,读取 `.msh`/`.ctgr`/`.flow`,构造 `XQSimulationCase`/`XQMesh`/`FlowSolver1D::SolverInput`,经 `FlowController::solve` 产出 flow result；若 2000 step 求解失败,重试 20000 step。
- 步骤 7 区域生长 UI 接线:分割页新增 Threshold / Region grow 两模式、Pick Seed 按钮、`XQMprView::seedPicked(int,int,int)` 信号、seed marker、`SegmentationController::regionGrow` 调用。
- 主窗口补充:SV 相对 source path 通过 `activeProjectDir_` 解析；command push 后刷新场景树；选中 Image 节点时尝试加载真实 source image,不再回退到 demo。
- 测试补充:`test_main_window` 覆盖 MPR seed click 发出 `seedPicked`;并新增 dialog-free 的 `loadImageFromPath` / `loadSvProjectFromDirectory` 回归,不弹 QFileDialog 但复用真实 reader + command stack 路径,验证 0007 `.vti` 进入 Image 节点且 MPR slice count 为 `100 x 512 x 512`,验证 SV project 进入 Image/Path/ContourGroup/Mesh/SimulationCase 节点且 MPR 显示项目图像。
- CMake 测试环境补强:`test_main_window` 注入 `XQ_TEST_VTI_PATH` / `XQ_SVPROJECT_DIR`;`test_ui_font_glyphs` 补 Qt runtime PATH + offscreen plugin 环境,避免全量 ctest 中 0xc0000135。
- Spec 回填:`.trellis/spec/XQ/core/build-and-test.md` 新增 CTest Qt GUI 测试运行环境契约,记录 Qt `PATH` / `QT_QPA_PLATFORM` / `QT_PLUGIN_PATH` 的必需性。

**质量检查观察(代码侧已收口;真机项待用户验收):**
- 本轮 2.2 复跑: `git diff --check` 通过；`cmd /c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"` 通过；针对性 `ctest --output-on-failure -R test_main_window` 通过；针对性 `ctest --output-on-failure -R test_ui_font_glyphs` 通过；全量 `ctest --output-on-failure` 结果 `50/50 passed`。
- AC3b 的 seed picking 已从 QLabel 尺寸线性主路径改为 VTK picker:QLabel 点击位置→离屏 render pixel→`vtkCellPicker` world→`XQImageVolume::worldToVoxel`;仍保留线性回退以防某些 offscreen backend picker 失败。测试覆盖 seed click 发 signal,但真窗口点选位置精度仍需人工确认。
- Flow provider 的边界条件 face 选择仍是样例级启发式(`faces.front()`/`faces[1]`/`faces.back()`),因为 `MSHMeshReader` 保留 faceId 但没有 face kind；能支撑 0007 最小验证,不是通用 SV simulation parser。
- AC1/AC2 已有程序化回归覆盖“真实数据进 scene + MPR 切换真实图像”,但“用户肉眼看到主动脉切片/项目节点树”仍需真窗口确认。AC3/AC3b/AC4/AC5/AC6 仍需真窗口/真机目视或交互确认；AC7 当前代码已构建 + 全量 ctest 绿；AC8 需最终评审签字。

**收尾记录(2026-07-01):**
- AC8 架构审查:未改 `XQSceneRenderer` 公共签名/实现;新增 UI 写 scene 仍经 `XQCommandStack` + command;未新增大规模 `GeometryHandle` 持有。
- 最终代码提交:GUI worktree `feat/gui-mitk-layout` commit `f1ef461 feat: GUI 真实管线收尾`。
- 最终质量门:Release 构建脚本通过;全量 offscreen `ctest --output-on-failure` 结果 `50/50 passed`。
