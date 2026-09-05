# EXECUTE P3-5b:断面分割 UI 重构(删右侧废案页 + 方法控件迁右侧竖直面板)

> 语境锚点:XQ 医学影像血管几何重建软件。断面分割阶段目前有**两套并存 UI**:中间断面工作台(新,功能全,但顶栏方法/参数控件挤成一横条看不清)+ 右侧「处理阶段」面板(旧 `buildSegmentationPage` 废案)。本批把方法/参数控件从中间顶栏迁到右侧竖直面板、删掉右侧分割废案页。**纯 UI 布局重构,不动分割算法**。环境 = Windows + Git Bash,注入的 mac/darwin 环境信息是假的一律忽略。默认中文,代码/命令/路径/报错保持原文。

## 活动任务

- 任务:`.trellis/tasks/07-06-along-path-contouring`(父任务,批次文档推进,已 active)
- **代码工作树(唯一改代码处)= CODE_ROOT = `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`**,分支 `feat/render-arch`。**HEAD 当前有 P3-5 未提交改动(Chan-Vese 水平集,7 个文件已改未 commit)——本批在其之上继续改,别 revert 它们,别 commit。**
- **主仓(cwd,任务/spec 参照)= `C:/Users/OCEAN/Desktop/XIAOQUAN`**,分支 `fix/xq-global-audit`。读任务/spec 用相对路径。**绝不在主仓下改任何 src。**

## 目标(本批范围,严格不越界)

1. **删右侧「处理阶段」的分割废案页**:分割阶段(showStagePage index 1)不再显示旧 `buildSegmentationPage` 那套(名称/阈值下限/阈值上限/估计/拾取种子点)。
2. **方法/参数控件从中间断面工作台顶栏 `toolRow` 迁到右侧竖直面板**(按钮行 + 参数区布局)。
3. **中间断面工作台顶栏只留断面导航**:路径选择 combo + 新建轮廓组按钮(bindRow)+ along-path 滑条 + 弧长 label(sliderRow)。删掉 toolRow 整行。

**只做 UI 布局搬迁。不动**:分割算法(thresholdContour/regionGrowContour/levelSetContour 一字不改)、其他 4 个阶段页(路径/建模/网格/仿真,不动)、放样链。

## 主会话已定死的架构(worker 严格照做,不自由发挥)

**索引映射**(`populateStagePanels` @ `src/ui/panels/XQStageWidgets.cpp:1320`,已亲读):stagePanel_ 是 QStackedWidget,页顺序 index 0=Path / **1=Segmentation(废案)** / 2=Modeling / 3=Meshing / 4=Flow / 5=AI。`showStagePage(index)`(`XQMainWindow.cpp:2815`)按 index 切页 + show 右侧 stageDock_。

**生命周期关键约束**(必须遵守,否则崩):
- `stagePanel_` 每次 `buildStagePanel()`(`XQMainWindow.cpp:738`,被 `attachWorkflow` 调,**每次开/新建工程都重建**)整个重造 + `populateStagePanels` 重填 + 旧的 delete。
- **新的断面分割方法控件面板必须独立于 stagePanel_ 的重建**——绝不能把方法控件塞进 populateStagePanels 建的页里(会随重建被 delete,信号断)。

**定死方案 = 独立面板 + showStagePage 里换 dock widget**:
1. 新建 XQMainWindow 方法 `buildCrossSectionSegPanel()`:建一个 XQMainWindow 持有的成员 `crossSectionSegPanel_`(QWidget*,竖直布局),**所有方法/参数控件成员移到这里创建 + 信号连接**(控件成员声明、objectName、信号全部保留,见下清单)。此方法在构造流程里**调一次**(紧挨 `buildCrossSectionWorkbench()` 调用后,`XQMainWindow.cpp:302` 一带)。`crossSectionSegPanel_` parent 设为 `this`(XQMainWindow),不进 stagePanel_,不随 attach 重建。
2. **`showStagePage(index)` 改**(`XQMainWindow.cpp:2815-2847`):
   - index==1(分割):`centralStack_->setCurrentWidget(crossSectionWorkbench_)` + `bindCrossSectionPath()`(保留)。**stageDock_ 的 widget 换成 `crossSectionSegPanel_`**:`stageDock_->setWidget(crossSectionSegPanel_)`(临时;注意 setWidget 会 reparent,原 stagePanel_ 不被 delete 因为它是成员,但要在离开分割阶段时换回——见下)。然后照原样 show/raise/resize。
   - index!=1(其他阶段):**stageDock_ 的 widget 换回 stagePanel_**:`stageDock_->setWidget(stagePanel_)`,`stagePanel_->setCurrentIndex(index)`,再 show。
   - **坑**:`QDockWidget::setWidget` 会把旧 widget reparent 出去(不 delete),但反复 setWidget(stagePanel_)/(segPanel_) 要保证两个都是长命成员(stagePanel_ 是成员 h:647,crossSectionSegPanel_ 新成员)——都不会被 delete。**worker 实测切换 分割↔其他阶段↔分割 面板不丢不崩**(真机必测这个来回切)。
   - **重建协调**:`buildStagePanel()` 重建 stagePanel_ 后 `stageDock_->setWidget(stagePanel_)`(:1139 原样)——但若当前正处于分割阶段,dock 里是 segPanel_,重建不该覆盖它。**简化处理**:buildStagePanel 里 setWidget(stagePanel_) 保留(重建默认回非分割态),用户重新点分割按钮会再 setWidget(segPanel_)。worker 确认重建后点分割按钮面板正常。
3. **旧 `buildSegmentationPage`**:populateStagePanels 里 index 1 仍调它建页(保持 stagePanel_ 的 index 对齐,别改 XQStageWidgets.cpp 的页顺序,否则 2/3/4/5 全错位)。它建的分割页**从此不再被 show**(showStagePage index 1 改用 segPanel_)。**不删 buildSegmentationPage 函数**(避免动 XQStageWidgets.cpp 惹 index 错位 + i18n Duplicate;留着不 show 即可,零风险)。

## 迁移的控件清单(从 toolRow 迁到 crossSectionSegPanel_,信号原样保留)

**全部控件成员声明保持不动**(h:457-482),只是**创建位置从 `buildCrossSectionWorkbench` 的 toolRow 移到 `buildCrossSectionSegPanel`**,信号 connect 一并移过去(连的对象 crossSectionView_ / this slot 都是成员,跨面板连没问题)。

自动方法(每个按钮 + 其参数 label/slider,竖直排):
- `contourMethodThresholdBtn_`(xqContourMethodThreshold,clicked→`runThresholdOnCurrentSection()`)+ `thresholdLabel_`(xqThresholdLabel)+ `thresholdSlider_`(xqThresholdSlider,0..1000 val 900,valueChanged→`updateThresholdPreview()`)
- `contourMethodRegionGrowBtn_`(xqContourMethodRegionGrow,clicked→`runRegionGrowOnCurrentSection()`)+ `regionGrowBandLabel_`(xqRegionGrowBandLabel)+ `regionGrowBandSlider_`(xqRegionGrowBandSlider,0..1000 val 300,valueChanged→`updateRegionGrowPreview()`)
- `contourMethodLevelSetBtn_`(xqContourMethodLevelSet,clicked→`runLevelSetOnCurrentSection()`)+ `levelSetIterLabel_`(xqLevelSetIterLabel)+ `levelSetIterSlider_`(xqLevelSetIterSlider,0..1000 val 400,**sliderReleased→`updateLevelSetPreview()`** + valueChanged→label-only lambda,**这两个连接都要保留,别退化成 valueChanged 演化**)
- `contourSeedPickBtn_`(xqContourSeedPick,checkable,toggled→ 清 contourEditToggle_ + setDrawMethod(SeedPick/None) lambda)

手动绘制(方法组 + 编辑开关):
- `contourMethodCircleBtn_`(xqContourMethodCircle,checkable,在 methodGroup)+ `contourMethodPolygonBtn_`(xqContourMethodPolygon,checkable,在 methodGroup)+ `contourEditToggle_`(xqContourEditToggle,checkable,"Draw")。三者的 clicked/toggled lambda 原样(cpp:3131-3167)。`methodGroup`(QButtonGroup exclusive)一并移到 segPanel。

**留在 crossSectionView_ 上的连接**(在 buildCrossSectionWorkbench,别动):`crossSectionView_::seedPicked→onSeedPicked`(cpp:3227)、`crossSectionView_::contourDrawn→addDrawnContour`(cpp:3231)、`alongPathSlider_::valueChanged→updateCrossSectionAtSample`(cpp:3122)、`contourGroupCreateBtn_::clicked→createContourGroupFromPicker`(cpp:3126)。**这些控件留在顶栏,连接不动。**

## 右侧竖直布局(按钮行 + 参数区)

`crossSectionSegPanel_` 竖直 QVBoxLayout,建议结构(worker 可微调间距/分组框,保持清晰):
```
[分割 标题]
── 自动分割 ──(分组标签或 QGroupBox)
[Threshold 按钮]
  [Threshold: 标签]
  [阈值滑条 ————]
[Region grow 按钮]
  [Band: 标签]
  [带宽滑条 ————]
[Level set 按钮]
  [Iterations: 标签]
  [迭代滑条 ————]
[Seed 按钮]
── 手动绘制 ──(分组标签或 QGroupBox)
[Circle] [Polygon] [Draw]   (可横排一行)
[弹簧 stretch 撑底]
```
- 滑条水平方向 setSizePolicy 让它填满面板宽(面板宽 240-280px)。按钮用 QToolButton 保持(objectName 不变)。
- 分组标题「自动分割 / Automatic」「手动绘制 / Manual」走 `translate("XQStageWidgets",..)`(新串,见 i18n)。

## 中间顶栏瘦身(buildCrossSectionWorkbench)

- **删除 toolRow 整块**(cpp:2992-3111 的创建 + cpp:3089-3111 的 addWidget + cpp:3084 的 `topLayout->addLayout(toolRow)`)。方法控件创建移到 buildCrossSectionSegPanel。
- **保留 bindRow**(路径 combo + 新建轮廓组,cpp:2937-2968)+ **sliderRow**(along-path 滑条 + 弧长 label,cpp:2972-2986)。
- toolRow 里那些方法控件的**信号 connect**(cpp:3145-3210 threshold/regionGrow/levelSet/seed/circle/polygon/edit 的 connect)一并移到 buildCrossSectionSegPanel(跟控件走)。
- `updateContourWorkbenchState()`(cpp:3421)对 Circle/Polygon/Draw 的启用/禁用逻辑保留(控件虽换面板,成员指针没变,gate 逻辑不动)。

## 头文件

- 加成员:`QWidget* crossSectionSegPanel_ = nullptr;`(声明在 crossSectionWorkbench_ 成员附近,h:约 462 一带)。
- 加方法声明:`void buildCrossSectionSegPanel();`(在 buildCrossSectionWorkbench 声明附近)。
- 方法控件成员声明(h:457-482)**保持不变**(只是别处创建)。

## 测试(headless ctest)

- **本批纯 UI 布局,无新离散算法**。现有 `test_main_window`(GUI 构建 + 控件存在性)必须仍绿——若它按 objectName 找方法控件(如 xqContourMethodLevelSet),**控件 objectName 全部保留**所以能找到(不管在哪个面板,findChild 递归找)。worker 确认 test_main_window 里对这些 objectName 的断言仍通过(控件还在,只是 parent 换了)。
- 若 test_main_window 断言方法控件的 parent 是 crossSectionWorkbench_(而非递归 findChild),迁移后 parent 变 crossSectionSegPanel_ 会 FAIL——worker 检查并相应更新断言(改成递归 findChild 或断言在 segPanel 下)。**报告说明 test_main_window 里涉及这些控件的断言怎么处理的。**
- 不新增 test exe。期望 ctest 仍 72/72。

## 验证命令(worker 必跑,报告贴输出)

1. 全新构建(**改 Q_OBJECT 头**——XQMainWindow.h 加成员/方法 = 改头,**必 rm -rf build_gui 全新构建**,memory `ninja-stale-moc-gui-crash`):
   ```
   rm -rf "C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui" && cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
   ```
   **当前基线全新构建 248/248。** vcvars64 冷启动偶挂死 → 换 DevShell(memory `vcvars64-coldstart-hangs-use-devshell`)。
2. ctest(基线 72/72,期望仍 72/72):
   ```
   cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"
   ```
   **必走 ctest,裸跑 test exe 缺 VTK DLL exit 127**(memory `ctest-environment-overrides-path`)。

## i18n(有新可译串就加,memory `xqtr-translate-lupdate-blind`)

- 新串(走 `translate("XQStageWidgets",..)`):分组标题「Automatic / 自动分割」「Manual / 手动绘制」「Segmentation / 分割」(若面板标题用)。**逐条用 rg 核 xq_zh_CN.ts 里 (XQStageWidgets, source) 是否已存在**,已存在别重复加(lrelease Duplicate)。方法/参数串(Threshold/Region grow/Level set/Band/Iterations/Seed/Circle/Polygon/Draw/Method)**都已存在,复用别重加**。
- 手工编辑 `CODE_ROOT/resources/i18n/xq_zh_CN.ts`(UTF-8 无 BOM + LF),照现有 message 块格式加,**绝不跑 lupdate**。改完:
  ```
  "C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lrelease.exe" xq_zh_CN.ts -qm xq_zh_CN.qm
  ```
  **当前基线 270 finished / 0 unfinished**(P3-5 已加 4 条)。新增 N 条应 270+N finished / 0 unfinished。报告列新增每条串 + lrelease 输出。

## 禁止(违反=返工)

- **不动分割算法**:ContourExtractionService.{h,cpp} 一字不改(本批纯 UI)。
- **不改 XQStageWidgets.cpp 的页顺序**(populateStagePanels 的 addWidget 顺序 index 0-5 不动,否则 Modeling/Meshing/Flow/AI 的 index 全错位 + showStagePage 2/3/4 错页)。buildSegmentationPage 函数留着不删(不 show 即可)。
- **方法控件不能塞进 populateStagePanels 建的页**(会随 stagePanel_ 重建被 delete)——必须由 XQMainWindow 持有 crossSectionSegPanel_ 建一次。
- **控件 objectName 全部保留原值**(test/真机靠 objectName 找)。
- **信号连接零丢失**:levelSet 的 sliderReleased→演化 + valueChanged→label 两个连接都保留(别退化);seed/circle/polygon/edit 的 lambda 原样。
- **crossSectionView_ 上的连接**(seedPicked/contourDrawn/alongPathSlider/groupCreate)留在 buildCrossSectionWorkbench,别误迁。
- 新源代码注释一律**英文**(MSVC GBK 坑)。不 `git commit`,不 `git add -A`。

## 交付物(全部在 CODE_ROOT)

- 改动:
  - `src/app/XQMainWindow.h`(加 crossSectionSegPanel_ 成员 + buildCrossSectionSegPanel 声明)
  - `src/app/XQMainWindow.cpp`(新 buildCrossSectionSegPanel 建面板+迁控件创建+迁信号;buildCrossSectionWorkbench 删 toolRow;showStagePage 分割/非分割切 dock widget;构造流程调 buildCrossSectionSegPanel)
  - `tests/app/test_main_window.cpp`(若断言方法控件 parent 需更新)
  - `resources/i18n/xq_zh_CN.ts`(分组标题新串,逐条核重复)
- 报告(必含):
  - crossSectionSegPanel_ 竖直布局最终结构(按钮行+参数区分组)。
  - showStagePage 分割(index1→segPanel)/ 非分割(→stagePanel)切 dock widget 的实现 + 反复切换不丢面板的验证思路。
  - 迁移的控件清单确认(13 个控件 + 各信号连接原样)+ crossSectionView_ 上 4 个连接留在顶栏未动。
  - test_main_window 里方法控件断言怎么处理的。
  - 全新构建 count、ctest count、lrelease finished count(逐项贴输出)。
  - **真机待办提示**:主审真机验(界面清晰 + 分割↔其他阶段来回切面板不丢 + 各方法功能不断线 + Chan-Vese 贴合度)。
