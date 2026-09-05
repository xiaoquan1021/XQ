# EXECUTE-B2 — S1 无用功能清零 + attachWorkflow 重入改造 + 打开/保存工作区

> **执行者须知**:本文档每处 before 代码均逐字对照过 `feat/gui-v2`(合并提交 5891c07)真实源码。你的任务是**照做**,不需要也不允许做本文之外的设计决策。若某处 before 与实际源码对不上(行号漂移正常,内容对不上才算),**停下报告,不要自行发挥**。

---

## 0. 环境与固定纪律

- 工作树:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/gui-v2`。Git Bash 下路径为 `/c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`。
- 完整构建(configure+build,禁用单 target 增量):
  ```bash
  cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ && cmd //c build_gui_wt.bat
  ```
- 全量测试:
  ```bash
  cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ && cmd //c ctest_merge.bat
  ```
- 单测快速回路(先完整 build,再):
  ```bash
  cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui && \
  "C:/software/Visual Studio/Visual Studio2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe" \
    -R '^test_app_startup$' --output-on-failure
  ```
- **绝不使用 `cmake --build --target <单目标>`**(实测会静默跳过链接跑旧 exe 假绿)。每次改完跑完整 `build_gui_wt.bat`,并 `ls -l build_gui/<测试名>.exe` 核对时间戳更新后再 ctest。
- 测试纪律:副作用调用(save/load/openWorkspaceFromPath 等)**先存变量再 CHECK**,绝不包进宏参数(Release /DNDEBUG 删成假绿)。
- commit 精确列文件,**绝不 `git add -A` / `git add .`**;status 若见大数据目录变 `??` 立即停。
- 全局约束:只做本批,不顺手做 B3~B6;不加未要求的兜底/降级;不覆盖用户未提交改动;services/core 不得引 Qt/VTK(架构护栏 test_arch_boundaries 会红);不破坏 Source 1.0 签名;**不参考 XQ1**;没验证过不写"完成/通过"。
- 基线:开工前跑一次全量 ctest,确认 **65/65 通过**;不通过先停下报告。

本批 3 个 commit,顺序固定:S1a 删除清单 → attachWorkflow 重入改造 → S1b 打开/保存。每个 commit 前:全量 ctest 全绿 + 对应假绿抽查完成并还原。

---

## 1. rg 定位(执行前先跑,行号漂移以此为准)

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rg -n 'makeDemoVolume|XQImageViewer|ImageRenderResult|showImage|lastRgbaByteCount' src tests CMakeLists.txt
rg -n 'opacitySlider_|colorButton_|propertiesToggle_|tbSeg3dAction_|timeSlider_|timeSpin_|navTimeLabel_|locSpinX_' src/app tests
rg -n 'buildEditMenu|buildStagePanel|attachWorkflow' src/app
rg -n 'derive_payload_assets|visit_assets' src/io/project src/core/asset
rg -n 'openAction_|saveAction_|tbOpenAction_|tbSaveAction_' src/app/XQMainWindow.cpp
```

已核实的关键坐标(当前行号,仅供参考):
- `XQMainWindow.h:5` include XQImageViewer.h;`:68-69` showImage/lastRgbaByteCount;`:176` `imageLabel_`;`:179-180` `imageViewer_`/`lastRgba_`;`:186` `timeSlider_`;`:190` `timeSpin_`;`:199-202` `navTimeLabel_`/locSpin×3;`:206-210` opacity/color/propertiesToggle;`:251` `tbSeg3dAction_`。
- `XQMainWindow.cpp:114-115` kDefaultRenderWidth/Height;`:195` imageLabel_ 构造;`:250-253` imageLabel_ 配置;`:255-265` centralStack_ 注释与 addWidget;`:355-380` showImage/lastRgbaByteCount 实现;`:825-828` tbSeg3dAction_;`:877-898` opacity/color/properties 构建;`:928-945` navLocLabel_+locSpin 构建;`:982-997` timeSlider_/timeSpin_ 构建;`:1332-1334` retranslateUi 的 3D Seg;`:1365-1373` retranslateUi 的 opacity/color/properties;`:1376-1390` retranslateUi 的 navLoc/navTime。
- `CMakeLists.txt:125-126` xq_visualization 列 XQImageViewer;`:845-853` test_image_viewer 注册;`:1008` add_test(test_image_viewer);`:1047` VTK PATH 名单含 test_image_viewer。
- `test_main_window.cpp:145-155` showImage 断言块;`:235-240` opacitySlider/timeSlider 断言。
- `test_app_startup.cpp:246-276` imageLabel 翻页断言(两处 `stack->setCurrentWidget(imageLabel)`)。
- `XQProjectWriter.cpp:916-957` derive_payload_assets(只给 `!node.hasAssetId()` 派生 blob → save-as 换目录时惰性节点 blob 不落新 assets,存档坏死)。
- `XQMainWindow.cpp:382-410` attachWorkflow;`:1407-1428` buildEditMenu(无条件 addAction);`:422-687` buildStagePanel(每次 new QStackedWidget + new QDockWidget)。

---

## 2. Commit 1 — S1a 死代码删除(7 项)

**等价删除,无先红后绿;门禁 = rg 零残留 + 全量 ctest 绿 + 真机可启动。测试连带同 commit 改。**

### 2.1 `makeDemoVolume()`(struct 保留)

`src/visualization/XQDemoVolume.h`:删 `:14-19` 的注释块与声明(`// Builds a dim^3 ...` 到 `XQDemoVolume makeDemoVolume(int dim = 64);`)。struct XQDemoVolume 本体**保留**(activeImage_ 容器在用),把 `:7-9` 顶注释:
```cpp
// A synthetic, deterministic image volume for exercising the MPR views when no
// real dataset is loaded. NOT a real dataset -- purely a built-in demo so the
// 2x2 viewer shows responsive slices. Holds the volume plus its scalar buffer.
```
改为:
```cpp
// Decoded image volume + its scalar buffer, held together so the MPR view's
// borrowed pointers stay valid for the lifetime of the loaded dataset.
```

`src/visualization/XQDemoVolume.cpp`:整个文件只剩 makeDemoVolume 及其匿名 namespace helper(Blob/gaussian)→ **删除整个文件**,CMakeLists.txt 的 xq_visualization 源列表删两行:
```cmake
    src/visualization/XQDemoVolume.cpp
    src/visualization/XQDemoVolume.h
```
注意:.h 被 XQMainWindow.cpp include(`#include "visualization/XQDemoVolume.h"`),**头文件本体保留在磁盘**,只从库源列表里删 .cpp(header-only struct 不需要编译单元)。`XQDemoVolume.h` 从源列表删除后仍可被 include(include 路径 `src/` 是 PUBLIC include dir)。

### 2.2 `XQImageViewer` 整类

- 删文件:`src/visualization/XQImageViewer.h`、`src/visualization/XQImageViewer.cpp`、`tests/visualization/test_image_viewer.cpp`(git rm)。
- CMakeLists.txt:
  - xq_visualization 源列表删 `src/visualization/XQImageViewer.cpp` 与 `src/visualization/XQImageViewer.h` 两行(`:125-126`)。
  - 删 test_image_viewer 整段(`:845-853`):add_executable + target_link_libraries + vtk_module_autoinit。
  - 删 `add_test(NAME test_image_viewer COMMAND test_image_viewer)`(`:1008`)。
  - `:1047` `set_tests_properties(test_vtk_image_adapter test_image_viewer test_scene_renderer ...)` 里删 `test_image_viewer` 一个词(其余测试名不动)。
- `src/visualization/XQSceneRenderer.h` 两处注释改写(代码不动):
  - `:20` before:`// Per-add summary, mirroring XQImageViewer's ImageRenderResult contract: an`
    after:`// Per-add summary: an`
  - `:71` before:`// Public header is VTK-free (pimpl), the same boundary discipline XQImageViewer`(下一行 `// uses: all vtk* types live only in the .cpp Impl. ...`)
    改两行为:`// Public header is VTK-free (pimpl): all vtk* types live only in the .cpp Impl.`(承接原第二行剩余内容,保持注释连贯)。
- `src/visualization/XQSceneRenderer.cpp:97` 注释 `// otherwise every voxel takes the midpoint of the intensity range -- a neutral` 下一行 `// fill, deliberately not the synthetic gradient the early XQImageViewer used.` 改为 `// fill.`(仅注释)。

### 2.3 `showImage`/`lastRgbaByteCount`/`imageLabel_`/`imageViewer_`/`lastRgba_`

`src/app/XQMainWindow.h`:
- 删 `:5` `#include "visualization/XQImageViewer.h"`。
- 删 `:68-69`:
  ```cpp
    ImageRenderResult showImage(const XQImageVolume& image);
    std::size_t lastRgbaByteCount() const;
  ```
- 删成员 `:176` `QLabel* imageLabel_;`、`:179` `XQImageViewer imageViewer_;`、`:180` `std::vector<unsigned char> lastRgba_;`。

`src/app/XQMainWindow.cpp`:
- 构造函数初始化列表(`:190-199`)删 `imageLabel_(new QLabel(this))`、`imageViewer_()`、`lastRgba_()` 三项(注意逗号连贯)。
- 删 `:250-253` imageLabel_ 配置四行(objectName/Alignment/setMinimumSize/setSizePolicy)。
- `:255-265` centralStack_ 段,before(逐字):
  ```cpp
      // The stack holds the 2x2 MPR view (default workspace) and the legacy
      // offscreen-RGBA label (showImage path, test_main_window depends on it).
      // showImage raises the label; otherwise the MPR view is shown.
      centralStack_->setObjectName(QStringLiteral("xqCentralStack"));
      centralStack_->setMinimumSize(0, 0);
      centralStack_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
      mprView_->setMinimumSize(0, 0);
      mprView_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
      centralStack_->addWidget(mprView_);
      centralStack_->addWidget(imageLabel_);
      centralStack_->setCurrentWidget(mprView_);
  ```
  after(注释改写 + 删 addWidget(imageLabel_) 一行;centralStack_ 对象名与既有断言不动):
  ```cpp
      // The stack hosts the 2x2 MPR view (single workspace page). Kept as a
      // QStackedWidget so the object name and page-raising contract stay stable.
      centralStack_->setObjectName(QStringLiteral("xqCentralStack"));
      centralStack_->setMinimumSize(0, 0);
      centralStack_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
      mprView_->setMinimumSize(0, 0);
      mprView_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
      centralStack_->addWidget(mprView_);
      centralStack_->setCurrentWidget(mprView_);
  ```
- 删 `:355-380` 两个方法体(`ImageRenderResult XQMainWindow::showImage(...)` 与 `std::size_t XQMainWindow::lastRgbaByteCount() const`)整段。
- 常量 `kDefaultRenderWidth/kDefaultRenderHeight`(`:114-115`):已核实全部使用点仅 `:252`(imageLabel_->setMinimumSize)与 `:357-358`(showImage)——两处都随本节删除,**常量两行一并删**。执行前跑 `rg -n 'kDefaultRenderWidth|kDefaultRenderHeight' src/app/XQMainWindow.cpp`,若命中数 ≠ 4(2 定义 + 2 使用)则**停下报告**。
- `:326` 注释 `// window that test_main_window builds before showImage().` 改为 `// window that test_main_window builds without a workflow attached.`(仅注释)。

**测试连带 ①** `tests/app/test_main_window.cpp:145-155`,before(逐字):
```cpp
    xq::XQImageVolume image = make_image();
    const xq::ImageRenderResult result = window.showImage(image);
    if (!result.ok) {
        return fail("showImage renders a synthetic image");
    }

    const std::size_t expected_rgba_bytes =
        static_cast<std::size_t>(result.width) * static_cast<std::size_t>(result.height) * 4U;
    if (window.lastRgbaByteCount() != expected_rgba_bytes) {
        return fail("showImage produces one RGBA buffer with four bytes per pixel");
    }
```
整段删除。连带删除(均已核实):`make_image()`(仅 :145 一处调用)与 `make_geometry()`(仅被 make_image 调用)两个 helper,以及 include 区的 `#include <core/XQImageVolume.h>`(两个 helper 删除后文件内无 XQImageVolume 引用)。

**测试连带 ②** `tests/app/test_app_startup.cpp` 惰性选中用例(`test_lazy_project_startup_renders_lazy_geometry_selection`):
- `:247` `QWidget* imageLabel = window.findChild<QWidget*>("xqImageLabel");` 与 `:251` `CHECK(imageLabel != nullptr);` 删除。
- 断言改写。before(第一处,`:256-266` 逐字):
  ```cpp
      QModelIndex surfaceIndex = find_display_name(tree->model(), "Lazy Surface");
      CHECK(surfaceIndex.isValid());
      stack->setCurrentWidget(imageLabel);
      tree->selectionModel()->setCurrentIndex(
          surfaceIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
      QApplication::processEvents();
      // The 3D render widget lives inside the MPR workspace page; selecting lazy
      // geometry must raise the page that hosts it (not the flat image label).
      CHECK(stack->currentWidget() != nullptr);
      CHECK(stack->currentWidget() != imageLabel);
      CHECK(stack->currentWidget()->isAncestorOf(renderWidget));
  ```
  after:
  ```cpp
      CHECK(stack->count() == 1);
      QWidget* mprHost = window.findChild<QWidget*>("xqMprView");
      CHECK(mprHost != nullptr);
      CHECK(stack->currentWidget() == mprHost);

      QModelIndex surfaceIndex = find_display_name(tree->model(), "Lazy Surface");
      CHECK(surfaceIndex.isValid());
      tree->selectionModel()->setCurrentIndex(
          surfaceIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
      QApplication::processEvents();
      // The 3D render widget lives inside the MPR workspace page; selecting lazy
      // geometry must keep that hosting page current and not crash.
      CHECK(stack->currentWidget() != nullptr);
      CHECK(stack->currentWidget()->isAncestorOf(renderWidget));
      CHECK(renderWidget->isVisibleTo(&window));
  ```
  第二处(`Lazy Mesh`,`:268-276`)同构改写:删 `stack->setCurrentWidget(imageLabel);` 与 `!= imageLabel` 断言,保留 setCurrentIndex/processEvents/isAncestorOf,补 `isVisibleTo`。

### 2.4 Data Manager 装饰控件

`src/app/XQMainWindow.cpp` buildDataManagerDock,before(逐字,`:870-898` 一带):
```cpp
    QWidget* nodeControls = new QWidget(panel);
    nodeControls->setObjectName(QStringLiteral("xqDataNodeControls"));
    QGridLayout* controls = new QGridLayout(nodeControls);
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setHorizontalSpacing(6);
    controls->setVerticalSpacing(6);

    opacityLabel_ = new QLabel(nodeControls);
    opacitySlider_ = new QSlider(Qt::Horizontal, nodeControls);
    opacitySlider_->setObjectName(QStringLiteral("xqOpacitySlider"));
    opacitySlider_->setRange(0, 100);
    opacitySlider_->setValue(100);
    opacitySlider_->setEnabled(false);
    opacityValueLabel_ = new QLabel(QStringLiteral("100%"), nodeControls);
    opacityValueLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    colorButton_ = new QPushButton(nodeControls);
    colorButton_->setObjectName(QStringLiteral("xqNodeColorButton"));
    colorButton_->setEnabled(false);
    controls->addWidget(opacityLabel_, 0, 0);
    controls->addWidget(opacitySlider_, 0, 1);
    controls->addWidget(opacityValueLabel_, 0, 2);
    controls->addWidget(colorButton_, 0, 3);

    propertiesToggle_ = new QToolButton(nodeControls);
    propertiesToggle_->setObjectName(QStringLiteral("xqPropertiesToggle"));
    propertiesToggle_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    propertiesToggle_->setArrowType(Qt::RightArrow);
    propertiesToggle_->setEnabled(false);
    controls->addWidget(propertiesToggle_, 1, 0, 1, 4);
    layout->addWidget(nodeControls);
```
整段删除(`layout->addWidget(sceneTreeView_, 1);` 之后直接 `panel->setLayout(layout);`)。

头文件删成员 `:205-210`:`opacityLabel_`/`opacitySlider_`/`opacityValueLabel_`/`colorButton_`/`propertiesToggle_` 五个(连同其上注释块 `// Data Manager node controls mirror ...` 两行)。

retranslateUi(`:1365-1373`)删三段:
```cpp
    if (opacityLabel_ != nullptr) {
        opacityLabel_->setText(tr("Opacity:"));
    }
    if (colorButton_ != nullptr) {
        colorButton_->setText(tr("Color"));
    }
    if (propertiesToggle_ != nullptr) {
        propertiesToggle_->setText(tr("Properties"));
    }
```

**测试连带** `test_main_window.cpp:235-240`,before(逐字):
```cpp
    QSlider* opacitySlider = workflowWindow.findChild<QSlider*>("xqOpacitySlider");
    QSlider* timeSlider = workflowWindow.findChild<QSlider*>("xqNavTimeSlider");
    if (opacitySlider == nullptr || opacitySlider->isEnabled()
        || timeSlider == nullptr || timeSlider->isEnabled()) {
        return fail("reference-layout placeholder controls are present but honestly disabled");
    }
```
after(诚实缺席):
```cpp
    if (workflowWindow.findChild<QSlider*>("xqOpacitySlider") != nullptr
        || workflowWindow.findChild<QSlider*>("xqNavTimeSlider") != nullptr) {
        return fail("dead placeholder controls (opacity / time) stay deleted");
    }
```

### 2.5 工具栏 3D Seg 按钮

`rg -n 'tbSeg3dAction_|xqToolbarSeg3dAction' src tests` 先跑。已核实命中:XQMainWindow.h:251、XQMainWindow.cpp:825-828 与 :1332-1334、ts 文件。
- 删 XQMainWindow.cpp `:825-828`:
  ```cpp
      // 3D Seg shares the segmentation stage page (no dedicated 3D page yet).
      tbSeg3dAction_ = toolBar->addAction(themeIcon("tool-seg-3d.svg"), QString());
      tbSeg3dAction_->setObjectName(QStringLiteral("xqToolbarSeg3dAction"));
      QObject::connect(tbSeg3dAction_, &QAction::triggered, this, [this]() { showStagePage(1); });
  ```
- 删 retranslateUi `:1332-1334`:
  ```cpp
      if (tbSeg3dAction_ != nullptr) {
          tbSeg3dAction_->setText(tr("3D Seg"));
      }
  ```
- 删头文件成员 `QAction* tbSeg3dAction_ = nullptr;`。
- 2D Seg 文案:retranslateUi 中 `tbSeg2dAction_->setText(tr("2D Seg"));` 改为 `tbSeg2dAction_->setText(tr("Seg"));`。**ts 同步**:`resources/i18n/xq_zh_CN.ts` 的 `xq::XQMainWindow` context 里 `<source>2D Seg</source>` 词条改 source 为 `Seg`、translation 为 `分割`;`<source>3D Seg</source>` 词条整个 `<message>` 块删除。改完跑 lrelease 重新生成 .qm:
  ```bash
  /c/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lrelease.exe \
    resources/i18n/xq_zh_CN.ts -qm resources/i18n/xq_zh_CN.qm
  ```
- 图标文件 `resources/icons/tool-seg-3d.svg` 与 qrc 条目 `<file alias="tool-seg-3d.svg">icons/tool-seg-3d.svg</file>`:一并删除(已核实全仓引用仅 qrc 这一条 + 本节删除的 :826 themeIcon 调用)。

### 2.6 Time 滑块 + Loc spin

`src/app/XQMainWindow.cpp` buildImageNavigatorDock:
- 删 navLoc 段(`:928-946` 一带):`navLocLabel_ = new QLabel(panel);` 起到 `++row;` 止(含 locSpins 循环与 4 个 addWidget)。
- 删 time 段(`:982-997` 一带):`navTimeLabel_ = new QLabel(panel);` 起到 `grid->addWidget(timeSpin_, row, 3);` 止。
- navLoc 段连同它自己的 `++row;` 一起删(row 计数器从 0 起直接给三轴行,网格行号连续即可);三轴循环体内的 `++row;` 不动。
- 删 `useVolume`(`:1461-1477` 一带)与 `onNavigatorSliderChanged`(`:1655-1673` 一带)中 locSpin 回写段:useVolume 中删 `const ImageGeometry& geometry = ...` 起到 locSpins if 块闭括号止的整段;onNavigatorSliderChanged 中 `if (activeImage_) { ... }` **整块删除**(已核实该块体只有 voxel/world/locSpins 回写,无其他逻辑)。
- retranslateUi 删两段:`navLocLabel_->setText(tr("Loc. (mm)"));` 与 `navTimeLabel_->setText(tr("Time"));` 所在 if 块。
- 头文件删成员:`timeSlider_`、`timeSpin_`、`navLocLabel_`、`navTimeLabel_`、`locSpinX_`、`locSpinY_`、`locSpinZ_`。
- 常量 `kNavigatorDoubleSpinWidth`(`:124`)仅 locSpin 使用(已核实)→ 一并删;`kNavigatorSpinWidth` 仍被三轴 spin 使用,保留。
- ts 文件:`Loc. (mm)`、`Time` 两词条的 `<message>` 块删除(在 `xq::XQMainWindow` context 内),lrelease 重跑(与 2.5 合并跑一次即可)。
- 测试连带:2.4 已把 timeSlider 断言改为缺席断言,此处无额外改动。

### 2.7 验收(本 commit 门禁)

```bash
rg -n 'makeDemoVolume|XQImageViewer|ImageRenderResult|showImage|lastRgbaByteCount|opacitySlider_|colorButton_|propertiesToggle_|tbSeg3dAction_|timeSlider_|xqOpacitySlider|xqNavTimeSlider|locSpinX_' src tests CMakeLists.txt resources
```
要求:**零命中**(测试里的"缺席断言"字符串 `xqOpacitySlider`/`xqNavTimeSlider` 例外——它们断言 findChild 为 null,属预期命中,rg 结果里只允许这两处)。

完整构建 + 全量 ctest:**64/64**(65 减去删除的 test_image_viewer)。

### 2.8 假绿抽查

把 test_app_startup.cpp 改写后的 `CHECK(stack->currentWidget()->isAncestorOf(renderWidget));` 临时篡改为 `isAncestorOf(tree)`(tree 不在 central stack 里)→ 完整构建 → `test_app_startup` **必红**;还原,构建,确认绿。

### 2.9 Commit 1

```
refactor: 清零死代码 — XQImageViewer/showImage/装饰控件/3D Seg/Time与Loc控件
```
文件(精确 add;git rm 的三个文件也在列):
- `src/visualization/XQDemoVolume.h`
- `src/visualization/XQDemoVolume.cpp`(删除)
- `src/visualization/XQImageViewer.h`(删除)
- `src/visualization/XQImageViewer.cpp`(删除)
- `src/visualization/XQSceneRenderer.h`
- `src/visualization/XQSceneRenderer.cpp`
- `src/app/XQMainWindow.h`
- `src/app/XQMainWindow.cpp`
- `tests/visualization/test_image_viewer.cpp`(删除)
- `tests/app/test_main_window.cpp`
- `tests/app/test_app_startup.cpp`
- `CMakeLists.txt`
- `resources/xq_resources.qrc`
- `resources/icons/tool-seg-3d.svg`(删除)
- `resources/i18n/xq_zh_CN.ts`
- `resources/i18n/xq_zh_CN.qm`

---

## 3. Commit 2 — attachWorkflow 重入改造(S1b 前置)

**现状缺陷(已核实)**:`attachWorkflow` 二次调用时——`buildEditMenu`(:1407)无条件 `editMenu_->addAction` → 重复 Undo/Redo 项;`buildStagePanel`(:422)每次 `new QStackedWidget` + `:679` 每次 `new QDockWidget` → 第二个 Stages dock;页面 lambda 捕获旧 controller 裸指针(reset 后悬垂)。

### 3.1 先写测试(先红)

`tests/app/test_app_startup.cpp` 新增用例(插在 `test_invalid_project_path_fails_closed` 之前,main() 里对应加调用):

```cpp
int test_attach_workflow_is_reentrant()
{
    xq::XQMainWindow window;

    xq::XQScene sceneA;
    xq::XQCommandStack stackA;
    window.attachWorkflow(&sceneA, &stackA);

    // Baseline after the first attach: what a re-attach must not grow.
    const int dockCountAfterFirst =
        static_cast<int>(window.findChildren<QDockWidget*>().size());
    CHECK(window.findChildren<QStackedWidget*>("xqStagePanel").size() == 1);
    CHECK(window.findChildren<QAction*>("xqUndoAction").size() == 1);

    xq::XQScene sceneB;
    sceneB.insert(xq::XQDataNode(xq::NodeId(11), "volume", "B-node-1"));
    sceneB.insert(xq::XQDataNode(xq::NodeId(12), "mesh", "B-node-2"));
    xq::XQCommandStack stackB;
    window.attachWorkflow(&sceneB, &stackB);
    QApplication::processEvents();

    // Re-attach must not duplicate: same dock count, one stage panel, one
    // Undo/Redo pair, and the tree shows the second scene's rows.
    CHECK(static_cast<int>(window.findChildren<QDockWidget*>().size())
          == dockCountAfterFirst);
    CHECK(window.findChildren<QStackedWidget*>("xqStagePanel").size() == 1);
    CHECK(window.findChildren<QAction*>("xqUndoAction").size() == 1);
    CHECK(window.findChildren<QAction*>("xqRedoAction").size() == 1);
    QTreeView* tree = window.findChild<QTreeView*>("xqSceneTreeView");
    CHECK(tree != nullptr);
    CHECK(tree->model()->rowCount(QModelIndex()) == 2);

    return 0;
}
```
(include 区补 `#include <QAction>`、`#include <QDockWidget>`,已有的不重复。)

**验证红**:完整构建 → `ctest -R '^test_app_startup$'` → 必须失败在重复计数(xqStagePanel 或 xqUndoAction size == 2)。不红则停下报告。

### 3.2 修复(后绿)

`src/app/XQMainWindow.cpp`:

**(a) buildEditMenu** 开头加已建即退,before(逐字,`:1407-1415`):
```cpp
void XQMainWindow::buildEditMenu()
{
    // editMenu_ was already created in buildMenus() (to hold its File | Edit |
    // View slot); here we only populate it, once a command stack is attached.
    if (editMenu_ == nullptr) {
        editMenu_ = menuBar()->addMenu(QString());
    }

    undoAction_ = editMenu_->addAction(QString());
```
after:
```cpp
void XQMainWindow::buildEditMenu()
{
    // editMenu_ was already created in buildMenus() (to hold its File | Edit |
    // View slot); here we only populate it, once a command stack is attached.
    // Idempotent: the undo/redo slots read the commandStack_ member, so a
    // workflow re-attach needs no re-wiring -- populate only once.
    if (undoAction_ != nullptr) {
        return;
    }
    if (editMenu_ == nullptr) {
        editMenu_ = menuBar()->addMenu(QString());
    }

    undoAction_ = editMenu_->addAction(QString());
```

**(b) buildStagePanel** 尾段改造,before(逐字,`:679-687`):
```cpp
    QDockWidget* stageDock = new QDockWidget(tr("Stages"), this);
    stageDock->setObjectName(QStringLiteral("xqStageDock"));
    stageDock->setMinimumWidth(kMinimumStageDockWidth);
    stageDock->setMaximumWidth(kMaximumStageDockWidth);
    stageDock->setWidget(stagePanel_);
    addDockWidget(Qt::RightDockWidgetArea, stageDock);
    stageDock_ = stageDock;
    stageDock_->hide();
```
after:
```cpp
    if (stageDock_ == nullptr) {
        QDockWidget* stageDock = new QDockWidget(tr("Stages"), this);
        stageDock->setObjectName(QStringLiteral("xqStageDock"));
        stageDock->setMinimumWidth(kMinimumStageDockWidth);
        stageDock->setMaximumWidth(kMaximumStageDockWidth);
        addDockWidget(Qt::RightDockWidgetArea, stageDock);
        stageDock_ = stageDock;
        stageDock_->hide();
    }
    stageDock_->setWidget(stagePanel_);
```
并在函数开头 `stagePanel_ = new QStackedWidget(this);` 之前加旧 panel 回收:
```cpp
    // Re-attach rebuilds the stage panel so every page lambda captures the new
    // controller pointers (the old ones are reset below in attachWorkflow).
    // The dock is reused; only its inner panel is swapped.
    QStackedWidget* oldPanel = stagePanel_;
```
函数尾(`stageDock_->setWidget(stagePanel_);` 之后)加:
```cpp
    if (oldPanel != nullptr) {
        oldPanel->deleteLater();
    }
```

**(c) buildWindowMenu**:已核实其开头 `windowMenu_->clear()` 后全量重建(toggleViewAction 来自 dock,dock 指针不变),重复调用本就幂等——**不动**。`applyProductDockLayout`/`retranslateUi` 均为设置类操作,幂等,不动。

**验证绿**:完整构建 → `test_app_startup` 绿 → 全量 64/64。

### 3.3 假绿抽查

把 (a) 的 `if (undoAction_ != nullptr) { return; }` 临时注释 → 完整构建 → `test_app_startup` 重入用例**必红**(xqUndoAction 计数 2);还原,构建,确认绿。

### 3.4 Commit 2

```
fix: attachWorkflow 可重入 — Edit 菜单/Stages dock/stage panel 不再重复构建
```
文件:
- `src/app/XQMainWindow.cpp`
- `tests/app/test_app_startup.cpp`

---

## 4. Commit 3 — S1b 打开/保存工作区

### 4.1 已核实的关键事实

- `derive_payload_assets`(XQProjectWriter.cpp:916-957)只给 `payload() && !hasAssetId()` 的节点派生 blob;惰性载入节点已有 assetId + 空 handle → **换目录 save 时其 blob 不会写进新 `<stem>.assets` → 新档 registry 引用的文件缺失 → 存档坏死**(save() 的自检只查本次 derived 的 blob,查不出)。
- 打开/保存 action 现状无 connect:`openAction_`(cpp:694-696)、`saveAction_`(:703-705)、`tbOpenAction_`(:783-784)、`tbSaveAction_`(:785-786)。
- `XQProjectWriter::save(const XQProject&, const std::string&)` → Status{Ok,FileOpenError,WriteError}。
- `initializeAppStartup(config, state*)` 失败时返回非 Ok 且 state 无害(test_invalid_project_path_fails_closed 已证)。
- `AssetRegistry::visit_assets(const std::function<void(const AssetRecord&)>&) const`;`AssetRecord.blobs` 是 `std::vector<std::pair<std::string, BufferRef>>`,`BufferRef.relPath` 形如 `blobs/<xx>/<sha>.bin`。
- asset 根目录规则:`<项目目录>/<stem>.assets`(XQAppStartup.cpp:14-18 与 XQProjectWriter.cpp asset_dir_for 一致)。

### 4.2 新增公开方法(XQMainWindow.h)

在 `loadSvProjectFromDirectory` 声明之后追加:
```cpp
    // Dialog-free workspace entry points (same real reader/writer paths as the
    // File menu actions; tests drive these directly).
    // Opens a .xqproj (lazy geometry), replacing the current workflow state on
    // success; on failure the current state is left fully intact.
    bool openWorkspaceFromPath(const QString& path);
    // Saves the live project to `path`. When saving into a different assets
    // directory, first copies every registry-referenced blob so the archive
    // stays self-contained (lazy-loaded nodes keep their asset ids and are
    // never re-derived by the writer).
    bool saveWorkspaceFile(const QString& path);
```
private 段加两个对话框槽 + 成员:
```cpp
    void openWorkspaceDialog();
    void saveWorkspaceDialog();
```
```cpp
    // Owned startup state for workspaces opened through File > Open Workspace.
    // The state main() builds at launch stays owned by main(); the window only
    // borrows it (attachWorkflow contract). Opening a new workspace at runtime
    // creates a fresh state the window must own -- it replaces (and destroys)
    // the previous owned one.
    std::unique_ptr<XQAppStartupState> ownedState_;
    QString workspacePath_;
```
头文件 include 区:XQAppStartupState 定义在 `app/XQAppStartup.h`——**不 include**(避免 app 头互相纠缠),用前向声明 `struct XQAppStartupState;`(namespace xq 内既有前向声明区)。unique_ptr 对不完整类型要求析构处可见:XQMainWindow.cpp 已能 include XQAppStartup.h,且 `~XQMainWindow()` 定义在 .cpp(`= default`,:344)——满足。
**CMake 连带(已核实)**:`XQAppStartup.cpp` 目前只编进 `xq_app` 与 `test_app_startup` 两个目标,xq_app_shell 没有它 → XQMainWindow.cpp 调 `initializeAppStartup` 会让链 xq_app_shell 的其他目标(test_main_window / test_i18n_resources)炸 LNK2019。**把 XQAppStartup.{cpp,h} 移入 xq_app_shell 源列表**,并从 `target_sources(xq_app PRIVATE ...)` 段删除(xq_app 链 xq_app_shell,符号仍可达;test_app_startup 的 add_executable 里那两行也删,防重复目标文件)。xq_app_shell 已 PRIVATE 链 xq_io,XQAppStartup.cpp 用的 XQProjectReader 满足。

### 4.3 实现(XQMainWindow.cpp)

include 区补 `#include "app/XQAppStartup.h"`、`#include "io/project/XQProjectWriter.h"`、`#include "core/asset/AssetRegistry.h"`、`#include <filesystem>`。

**(a) connect(buildMenus/buildMainToolBar 各两处)**:
```cpp
    QObject::connect(openAction_, &QAction::triggered, this, &XQMainWindow::openWorkspaceDialog);
```
(`openAction_->setShortcut(QKeySequence::Open);` 之后);saveAction_ 同构 connect 到 `saveWorkspaceDialog`;`tbOpenAction_`/`tbSaveAction_` 同构各一行(buildMainToolBar 内)。

**(b) 对话框槽**:
```cpp
void XQMainWindow::openWorkspaceDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Workspace"), QString(), tr("XQ Workspace (*.xqproj)"));
    if (path.isEmpty()) {
        return; // user cancelled
    }
    openWorkspaceFromPath(path);
}

void XQMainWindow::saveWorkspaceDialog()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Workspace"), workspacePath_, tr("XQ Workspace (*.xqproj)"));
    if (path.isEmpty()) {
        return; // user cancelled
    }
    saveWorkspaceFile(path);
}
```

**(c) saveWorkspaceFile**(核心:先复制被引用 blob,再写主档):
```cpp
bool XQMainWindow::saveWorkspaceFile(const QString& path)
{
    if (workflowScene_ == nullptr) {
        QMessageBox::warning(this, tr("Save Workspace"),
                             tr("No active workspace to save."));
        return false;
    }
    // The window renders a scene owned by a project; saving needs that project.
    // It is either the runtime-opened ownedState_ or the startup state that
    // attached this scene -- resolved through the geometry registry's owner.
    XQProject* project = activeProject();
    if (project == nullptr) {
        QMessageBox::warning(this, tr("Save Workspace"),
                             tr("No active project to save."));
        return false;
    }

    namespace fs = std::filesystem;
    const std::string targetPathUtf8 = path.toStdString();
    fs::path targetPath(targetPathUtf8);
    const fs::path targetAssets =
        targetPath.parent_path() / (targetPath.stem().string() + ".assets");

    // Writer only derives blobs for nodes WITHOUT an asset id. Lazy-loaded
    // nodes already carry ids, so their blobs must be copied by hand whenever
    // the target assets dir differs from the current asset root -- otherwise
    // the new archive references files that were never written (dead archive).
    if (!assetRootDir_.empty() && fs::path(assetRootDir_) != targetAssets) {
        bool copyOk = true;
        std::string firstError;
        project->assetRegistry().visit_assets(
            [&](const AssetRecord& record) {
                for (std::size_t b = 0; copyOk && b < record.blobs.size(); ++b) {
                    const std::string& rel = record.blobs[b].second.relPath;
                    const fs::path from = fs::path(assetRootDir_) / rel;
                    const fs::path to = targetAssets / rel;
                    std::error_code ec;
                    fs::create_directories(to.parent_path(), ec);
                    if (ec) {
                        copyOk = false;
                        firstError = to.parent_path().string();
                        return;
                    }
                    const bool copied = fs::copy_file(
                        from, to, fs::copy_options::overwrite_existing, ec);
                    if (!copied || ec) {
                        copyOk = false;
                        firstError = from.string();
                    }
                }
            });
        if (!copyOk) {
            QMessageBox::warning(
                this, tr("Save Workspace"),
                tr("Could not copy asset blob: %1. The workspace was not saved.")
                    .arg(QString::fromStdString(firstError)));
            return false;
        }
    }

    const XQProjectWriter::Status status =
        XQProjectWriter::save(*project, targetPathUtf8);
    if (status != XQProjectWriter::Status::Ok) {
        const QString reason = status == XQProjectWriter::Status::FileOpenError
            ? QStringLiteral("FileOpenError")
            : QStringLiteral("WriteError");
        QMessageBox::warning(this, tr("Save Workspace"),
                             tr("Could not save the workspace: %1.").arg(reason));
        return false;
    }

    workspacePath_ = path;
    setWindowTitle(QStringLiteral("XQ - %1").arg(QFileInfo(path).fileName()));
    return true;
}
```
其中 `assetRootDir_` 是新增的 `std::string` 私有成员:`attachGeometryResources` 里顺手记下(`assetRootDir_ = assetRootDir;`,函数原有逻辑不变);`activeProject()` 是新增私有 helper:
```cpp
XQProject* XQMainWindow::activeProject() const
{
    return ownedState_ != nullptr ? &ownedState_->project : externalProject_;
}
```
`externalProject_`(`XQProject*`,默认 null)的写入路径(定形):窗口增加公开 setter `void setExternalStartupState(XQAppStartupState* state);`(实现一行:`externalProject_ = state != nullptr ? &state->project : nullptr;`),`attachMainWindowWorkflow`(XQAppStartup.cpp:61-71)在 attachWorkflow 调用后追加一行 `window->setExternalStartupState(state);`。main() 栈上 state 的生命周期长于窗口(既有契约),借用安全。test_main_window 直接 attachWorkflow(scene,stack) 的窗口没有 project → save 弹 "No active project",这是诚实语义(该窗口本就没有可保存工程)。

**(d) openWorkspaceFromPath**:
```cpp
bool XQMainWindow::openWorkspaceFromPath(const QString& path)
{
    auto fresh = std::make_unique<XQAppStartupState>();
    XQAppStartupConfig config;
    config.projectPath = path.toStdString();
    const XQAppStartupStatus status = initializeAppStartup(config, fresh.get());
    if (status != XQAppStartupStatus::Ok) {
        // Old state stays fully attached; nothing was swapped yet.
        QMessageBox::warning(this, tr("Open Workspace"),
                             tr("Could not open the workspace: %1.")
                                 .arg(path));
        return false;
    }

    // Take ownership, then re-attach everything to the new state. The previous
    // ownedState_ (if any) is destroyed after the swap; the startup-borrowed
    // state (main's) is simply no longer referenced.
    ownedState_ = std::move(fresh);
    externalProject_ = nullptr;
    attachWorkflow(&ownedState_->project.scene(), &ownedState_->commandStack);
    attachGeometryResources(&ownedState_->project.assetRegistry(),
                            ownedState_->assetRootDir);

    // Reset per-dataset UI state.
    activeImage_.reset();
    activeImageNodeId_ = NodeId();
    activeProjectDir_.clear();
    activeSeedValid_ = false;
    workspacePath_ = path;
    if (mprView_ != nullptr) {
        mprView_->clearImage();
    }
    if (renderWidget_ != nullptr) {
        renderWidget_->renderer().clear();
        renderWidget_->render();
    }
    refreshSceneTree();
    setWindowTitle(QStringLiteral("XQ - %1").arg(QFileInfo(path).fileName()));
    return true;
}
```
时序说明:`ownedState_ = std::move(fresh)` 时旧 ownedState_ 析构,此刻窗口的 stagePanel_/树仍指旧 scene;随后同一函数内立即 attachWorkflow 换到新 scene,期间不回事件循环、无任何 UI 触碰窗口,安全。`attachGeometryResources` 的 assetRootDir 为空串时(工程无 assets)manager 为空,与启动路径行为一致。
`NodeId::invalid()` 已核实存在(src/core/NodeId.h:17),复位一律写 `activeImageNodeId_ = NodeId::invalid();`。

### 4.4 测试(test_app_startup.cpp 追加三用例,main() 里挂上)

**(a) roundtrip(save → reopen 等价)**:
```cpp
int test_workspace_save_reopen_roundtrip()
{
    const xq::NodeId surfaceId(401);
    const xq::NodeId meshId(402);
    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    add_lazy_geometry_fixture_nodes(&project, surfaceId, meshId);

    TempProjectFiles files("xq_ws_roundtrip");
    const xq::XQProjectWriter::Status saved =
        xq::XQProjectWriter::save(project, files.path.string());
    CHECK(saved == xq::XQProjectWriter::Status::Ok);

    xq::XQAppStartupConfig config;
    config.projectPath = files.path.string();
    xq::XQAppStartupState state;
    const xq::XQAppStartupStatus status = xq::initializeAppStartup(config, &state);
    CHECK(status == xq::XQAppStartupStatus::Ok);
    CHECK(count_nodes(state.project.scene()) == 2);
    CHECK(state.assetRootDir == files.assets.string());
    CHECK(state.project.scene().find(surfaceId) != nullptr);
    CHECK(state.project.scene().find(meshId) != nullptr);
    return 0;
}
```

**(b) save-as 血案回归(先红后绿的"红"来自现状 bug)**:惰性打开 A → 窗口 saveWorkspaceFile(B) → B.assets 里 registry 引用的每个 relPath 都存在 → 重新 initializeAppStartup(B) 成功且节点守恒。
```cpp
int test_workspace_save_as_carries_lazy_blobs()
{
    const xq::NodeId surfaceId(501);
    const xq::NodeId meshId(502);
    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    add_lazy_geometry_fixture_nodes(&project, surfaceId, meshId);

    TempProjectFiles filesA("xq_ws_saveas_a");
    CHECK(xq::XQProjectWriter::save(project, filesA.path.string())
          == xq::XQProjectWriter::Status::Ok);

    // Lazy-open A (nodes now carry asset ids + empty handles).
    xq::XQAppStartupConfig config;
    config.projectPath = filesA.path.string();
    xq::XQAppStartupState state;
    CHECK(xq::initializeAppStartup(config, &state) == xq::XQAppStartupStatus::Ok);

    xq::XQMainWindow window;
    xq::attachMainWindowWorkflow(&window, &state);

    // Save-as into a different directory pair.
    TempProjectFiles filesB("xq_ws_saveas_b");
    const bool savedAs = window.saveWorkspaceFile(
        QString::fromStdString(filesB.path.string()));
    CHECK(savedAs);

    // Every registry-referenced blob must exist under B.assets.
    std::size_t missing = 0;
    state.project.assetRegistry().visit_assets(
        [&](const xq::AssetRecord& record) {
            for (std::size_t b = 0; b < record.blobs.size(); ++b) {
                const std::filesystem::path blob =
                    filesB.assets / record.blobs[b].second.relPath;
                std::error_code ec;
                const bool exists = std::filesystem::exists(blob, ec);
                if (!exists) {
                    ++missing;
                }
            }
        });
    CHECK(missing == 0);

    // And B must load: node count conserved, both nodes present.
    xq::XQAppStartupConfig configB;
    configB.projectPath = filesB.path.string();
    xq::XQAppStartupState stateB;
    CHECK(xq::initializeAppStartup(configB, &stateB) == xq::XQAppStartupStatus::Ok);
    CHECK(count_nodes(stateB.project.scene()) == 2);
    CHECK(stateB.project.scene().find(surfaceId) != nullptr);
    CHECK(stateB.project.scene().find(meshId) != nullptr);
    return 0;
}
```
include 区补 `#include <core/asset/AssetRecord.h>`、`#include <core/asset/AssetRegistry.h>`。
**先红验证(固定顺序,不可变通)**:先实现 (a)(b)(d) 与 connect,**唯独不写 blob 复制段**(saveWorkspaceFile 里暂缺 `if (!assetRootDir_.empty() ...)` 块)→ 构建 → 本用例红在 `missing == 0`(registry 引用的 blob 不在 B.assets)→ 把红时 FAIL 输出片段留档进 result → 再补复制段 → 绿。

**(c) 窗口 open 替换状态**:
```cpp
int test_window_open_workspace_swaps_state()
{
    // Startup: empty workflow.
    xq::XQAppStartupState startupState;
    CHECK(xq::initializeAppStartup(xq::XQAppStartupConfig{}, &startupState)
          == xq::XQAppStartupStatus::Ok);
    xq::XQMainWindow window;
    xq::attachMainWindowWorkflow(&window, &startupState);

    // A real project on disk to open at runtime.
    const xq::NodeId surfaceId(601);
    const xq::NodeId meshId(602);
    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    add_lazy_geometry_fixture_nodes(&project, surfaceId, meshId);
    TempProjectFiles files("xq_ws_window_open");
    CHECK(xq::XQProjectWriter::save(project, files.path.string())
          == xq::XQProjectWriter::Status::Ok);

    const bool opened = window.openWorkspaceFromPath(
        QString::fromStdString(files.path.string()));
    CHECK(opened);
    QApplication::processEvents();

    QTreeView* tree = window.findChild<QTreeView*>("xqSceneTreeView");
    CHECK(tree != nullptr);
    CHECK(tree->model()->rowCount(QModelIndex()) == 2);
    // Re-attach happened through the reentrant path: still one stage panel.
    CHECK(window.findChildren<QStackedWidget*>("xqStagePanel").size() == 1);

    // Open failure leaves the swapped-in state intact.
    const bool openedBad = window.openWorkspaceFromPath(
        QStringLiteral("Z:/definitely/missing.xqproj"));
    CHECK(!openedBad);
    CHECK(tree->model()->rowCount(QModelIndex()) == 2);
    return 0;
}
```
**弹窗抑制(定形)**:QMessageBox::warning 是模态弹窗,offscreen 测试走到失败路径会挂死。统一用私有 helper 替代直接弹窗:
```cpp
void XQMainWindow::reportWorkspaceError(const QString& title, const QString& text)
{
    // Tests drive the dialog-free entry points headlessly; a modal box would
    // deadlock offscreen. Suppressible via object property, default = show.
    if (property("xqSuppressDialogs").toBool()) {
        qWarning().noquote() << title << ":" << text;
        return;
    }
    QMessageBox::warning(this, title, text);
}
```
saveWorkspaceFile/openWorkspaceFromPath 中所有 QMessageBox::warning 一律换成 `reportWorkspaceError(...)`。测试 (c) 在构造 window 后加 `window.setProperty("xqSuppressDialogs", true);`(测试 (b) 的 save 全程走成功路径,但也加同一行,防御失败时挂死)。头文件 private 段补该 helper 声明。

**验证**:完整构建 → `test_app_startup` 绿 → 全量 64/64 绿。

### 4.5 假绿抽查

4.4(b) 的先红(缺 blob 复制段 → 红 → 补上 → 绿)**就是本节的抽查**,红绿证据已在开发顺序中产生,直接引用进 result,本节不再重复操作。

### 4.6 Commit 3

```
feat: 打开/保存工作区真实接线 — save-as 复制惰性 blob,open 换状态可重入
```
文件:
- `src/app/XQMainWindow.h`
- `src/app/XQMainWindow.cpp`
- `src/app/XQAppStartup.cpp`(attachMainWindowWorkflow 增 setExternalStartupState 调用;XQAppStartup.h 不改——XQAppStartupState 的前向声明加在 XQMainWindow.h)
- `CMakeLists.txt`(XQAppStartup 移入 xq_app_shell)
- `tests/app/test_app_startup.cpp`
- `resources/i18n/xq_zh_CN.ts` + `resources/i18n/xq_zh_CN.qm`(新增 tr 串:Open/Save Workspace 弹窗文案、XQ Workspace filter;lrelease 重跑)

---

## 5. 批次收尾

1. 3 个 commit 落盘后,删 `build_gui` 全新重建 + 全量 ctest,**64/64 绿**。
2. `git log --oneline -3` 对照本文档提交信息;`git status` 干净(大数据目录变 `??` 立即停)。
3. 汇报:每个 commit 的 hash、先红后绿(3.1 重入红 / 4.4(b) blob 红)与假绿抽查的实际输出摘录。**没跑过的步骤不许写"通过"。**
