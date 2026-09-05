# EXECUTE-B4 — S2 Path 阶段真实化(MPR 拾取控制点 → 生成路径)

> **执行者须知**:本文档每处 before 代码均逐字对照过 `feat/gui-v2` 真实源码(B2/B3 完成后行号漂移,**符号为准,改前 rg 定位**)。照做,不做本文之外的设计决策;before 对不上内容时停下报告。
> **依赖:B3 已完成**(AsyncCommandRunner hook 已在;populateStagePanels 已有 asyncRunner 尾参。没有则停下报告)。

---

## 0. 环境与固定纪律

- 工作树:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/gui-v2`(Git Bash:`/c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`)。
- 完整构建:`cmd //c build_gui_wt.bat`;全量测试:`cmd //c ctest_merge.bat`;单测:
  ```bash
  cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui && \
  "C:/software/Visual Studio/Visual Studio2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe" \
    -R '^test_path_stage$' --output-on-failure
  ```
- **绝不 `cmake --build --target <单目标>`**;副作用先存变量再 CHECK;绝不 `git add -A`。
- 全局约束:只做本批;不加未要求兜底/降级;不覆盖未提交改动;services/core/controllers 零 Qt/VTK(本批不改 services/core;PathController 已有 prepare*,不再动);不破坏 Source 1.0 签名;不参考 XQ1;没验证过不写"完成/通过"。
- 基线:全量 ctest **65/65**(B3 后基线)。

本批 2 个 commit:C1 拾取分流 + Path 页重写 + provider 接线 + i18n;C2 test_path_stage(先红要点见 5.1)。

---

## 1. rg 定位

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rg -n 'seedPicked' src/app/XQMainWindow.cpp src/visualization/XQMprView.h
rg -n 'buildPathPage' src/ui/panels/XQStageWidgets.cpp
rg -n 'seedPickingSetter|SeedPickingSetter' src/ui/panels src/app
rg -n 'voxelToWorld' src/core/XQImageVolume.h
rg -n 'populateStagePanels' src/ui/panels/XQStageWidgets.h src/app/XQMainWindow.cpp
rg -n 'pickMode|PickMode' src   # 应零命中(新概念)
```

已核实的事实:
- `XQMprView::seedPicked(int i,int j,int k)` 信号(XQMprView.h:60);`setSeedPickingEnabled`/`clearSeedMarker`(:55-57)。
- 窗口 seedPicked 处理 lambda(XQMainWindow.cpp:296-307):写 activeSeed_ + 状态栏 Seed voxel 文案。
- `seedPickingSetter`(XQMainWindow.cpp:457-466):enabled 时清 seed 并 `mprView_->setSeedPickingEnabled(enabled)`。
- `XQImageVolume::voxelToWorld(const double voxel[3], double world_out[3]) const` → `TransformStatus`(XQImageVolume.h:100-105)。
- `buildPathPage`(XQStageWidgets.cpp:220-247)三个 `(void)` stub。
- IdCounter 机制:`using IdCounter = std::shared_ptr<NodeId::ValueType>`(:49),`nextId(counter)`(:51-54),初值 1000(:715)。**Path 页直接用它,不另造 id 分配**。
- `PathController::AddPathIntent{newPathId,name,sourceImageNode,controlPoints,spacing}`(PathController.h:29-35);`prepareAddPath`(B3 加);`PathService::createPathCommand` 校验:points<2 → NotEnoughControlPoints,spacing<=0 → InvalidSpacing(PathService.cpp:96-101)。
- `PathControlPoint{Point3 position}`(XQPath.h:19-21);`Point3{x,y,z}`(GeometryTypes.h:8-12)。
- text(PathController::Status) 映射已在(XQStageWidgets.cpp:145-156)。
- 真实 .vti 测试路径宏:`XQ_TEST_VTI_PATH`(test_main_window 的 target_compile_definitions,CMakeLists.txt:901-904)。
- undo/redo 对称断言先例:test_main_window.cpp:376-383。

---

## 2. Commit 1(a) — 窗口:PickMode 分流 + path draft 状态

### 2.1 XQMainWindow.h

成员区(activeSeed_ 附近)追加:
```cpp
    // MPR click routing: seed picking (segmentation) vs path control-point
    // picking share the one seedPicked signal; pickMode_ decides the consumer.
    enum class PickMode { None, Seed, PathPoint };
    PickMode pickMode_ = PickMode::None;

    // Path-stage draft: world-space control points picked in the MPR. Owned by
    // the window (outlives stage-panel rebuilds); the Path page reads/mutates
    // it through the provider pair below.
    std::vector<PathControlPoint> pathDraftPoints_;
    // Hooks the Path page registers on (re)build: list refresher + toggle
    // setter (the window forces the pick toggle off when seed picking starts).
    std::function<void()> pathDraftChanged_;
    std::function<void(bool)> pathPickToggleSetter_;
```
include 区:`#include "core/XQPath.h"`(PathControlPoint)与 `#include <functional>`(现状没有,补)。

公开测试入口(public 段,loadSvProjectFromDirectory 之后):
```cpp
    // Test-only: appends a world-space path control point exactly like an MPR
    // pick in PathPoint mode would (draft + page refresh). Dialog-free tests
    // drive the path stage through this.
    void appendPathDraftPointForTest(const Point3& world);
```

### 2.2 seedPicked 分流

XQMainWindow.cpp 构造函数,before(逐字,:296-307):
```cpp
    QObject::connect(mprView_, &XQMprView::seedPicked, this,
                     [this](int i, int j, int k) {
        activeSeedValid_ = true;
        activeSeed_[0] = i;
        activeSeed_[1] = j;
        activeSeed_[2] = k;
        if (statusPositionLabel_ != nullptr) {
            statusShowingIdle_ = false;
            statusPositionLabel_->setText(
                tr("Seed voxel  i:%1  j:%2  k:%3").arg(i).arg(j).arg(k));
        }
    });
```
after:
```cpp
    QObject::connect(mprView_, &XQMprView::seedPicked, this,
                     [this](int i, int j, int k) {
        if (pickMode_ == PickMode::PathPoint) {
            // Path control-point pick: voxel -> world, append to the draft.
            // Never touches activeSeed_ or the segmentation status text.
            if (!activeImage_) {
                return;
            }
            const double voxel[3] = {static_cast<double>(i),
                                     static_cast<double>(j),
                                     static_cast<double>(k)};
            double world[3] = {0.0, 0.0, 0.0};
            if (activeImage_->image.voxelToWorld(voxel, world)
                != XQImageVolume::TransformStatus::Ok) {
                return;
            }
            pathDraftPoints_.push_back(PathControlPoint{{world[0], world[1], world[2]}});
            if (pathDraftChanged_) {
                pathDraftChanged_();
            }
            return;
        }
        activeSeedValid_ = true;
        activeSeed_[0] = i;
        activeSeed_[1] = j;
        activeSeed_[2] = k;
        if (statusPositionLabel_ != nullptr) {
            statusShowingIdle_ = false;
            statusPositionLabel_->setText(
                tr("Seed voxel  i:%1  j:%2  k:%3").arg(i).arg(j).arg(k));
        }
    });
```

`appendPathDraftPointForTest` 实现(方法区任意合理位置):
```cpp
void XQMainWindow::appendPathDraftPointForTest(const Point3& world)
{
    pathDraftPoints_.push_back(PathControlPoint{world});
    if (pathDraftChanged_) {
        pathDraftChanged_();
    }
}
```

### 2.3 既有 seedPickingSetter 改造 + 新 path 三件套(互斥定形)

**互斥机制(唯一方案,照此实现)**:两个拾取模式共用 `pickMode_`;Seg 页进入 seed 拾取时窗口经 `pathPickToggleSetter_(false)` 强制压灭 Path 页 toggle;Path 页进入拾取时清掉 seed 标记(Seg 页的 Pick Seed 是普通按钮非 checkable,已核实 XQStageWidgets.cpp:281,无需反向压灭)。Path 页把「压 toggle」闭包经 `PathPageHooks` 注册给窗口(类型定义见 3.1)。

buildStagePanel 里,before(逐字,:457-466):
```cpp
    SeedPickingSetter seedPickingSetter = [this](bool enabled) {
        if (mprView_ == nullptr) {
            return;
        }
        if (enabled) {
            activeSeedValid_ = false;
            mprView_->clearSeedMarker();
        }
        mprView_->setSeedPickingEnabled(enabled);
    };
```
after:
```cpp
    SeedPickingSetter seedPickingSetter = [this](bool enabled) {
        if (mprView_ == nullptr) {
            return;
        }
        if (enabled) {
            activeSeedValid_ = false;
            mprView_->clearSeedMarker();
            // One picker at a time: entering seed picking forces the Path
            // page's pick toggle off (its own toggled handler then routes
            // through pathPickingSetter(false, ...) -- harmless, blocked by
            // QSignalBlocker on the page side).
            if (pathPickToggleSetter_) {
                pathPickToggleSetter_(false);
            }
        }
        pickMode_ = enabled ? PickMode::Seed : PickMode::None;
        mprView_->setSeedPickingEnabled(enabled);
    };
```
同函数内(providers 定义区)追加 path 侧三件套:
```cpp
    PathDraftProvider pathDraftProvider = [this]() {
        return pathDraftPoints_; // snapshot copy
    };

    PathDraftMutator pathDraftMutator = [this](int removeIndex) {
        if (removeIndex < 0) {
            pathDraftPoints_.clear();
        } else if (static_cast<std::size_t>(removeIndex) < pathDraftPoints_.size()) {
            pathDraftPoints_.erase(pathDraftPoints_.begin() + removeIndex);
        }
        if (pathDraftChanged_) {
            pathDraftChanged_();
        }
    };

    PathPickingSetter pathPickingSetter = [this](bool enabled, PathPageHooks hooks) {
        // The page (re)registers its hooks on every call; a rebuilt panel
        // hands in fresh closures, so no dangling captures survive.
        pathDraftChanged_ = std::move(hooks.refreshList);
        pathPickToggleSetter_ = std::move(hooks.setPickChecked);
        if (mprView_ == nullptr) {
            return;
        }
        pickMode_ = enabled ? PickMode::PathPoint : PickMode::None;
        // Reuses the seed-picking click plumbing; the seedPicked handler
        // routes by pickMode_. Entering path picking clears any live seed
        // marker so the two pickers never look active at once.
        if (enabled) {
            activeSeedValid_ = false;
            mprView_->clearSeedMarker();
        }
        mprView_->setSeedPickingEnabled(enabled);
    };
```
`populateStagePanels` 调用点(buildStagePanel 尾部)按 3.1 新签名传满全部参数(参数顺序见 3.1)。

---

## 3. Commit 1(b) — XQStageWidgets:Path 页重写

### 3.1 XQStageWidgets.h(纯追加)

`SeedPickingSetter` 别名附近追加:
```cpp
// Path-stage draft plumbing (B4). The window owns the draft (world-space
// control points picked in the MPR); the page renders it and mutates it.
using PathDraftProvider = std::function<std::vector<PathControlPoint>()>;
// removeIndex >= 0 removes that point; removeIndex < 0 clears the draft.
using PathDraftMutator = std::function<void(int removeIndex)>;
// Closures the Path page registers with the window on (re)build.
struct PathPageHooks {
    std::function<void()> refreshList;        // draft changed -> re-render list
    std::function<void(bool)> setPickChecked; // window forces the toggle state
};
// enabled: toggle MPR path picking. hooks: re-registered on every call.
using PathPickingSetter = std::function<void(bool enabled, PathPageHooks hooks)>;
```
include 区补 `#include "core/XQPath.h"`(PathControlPoint;现状有 core/NodeId.h 无 XQPath.h)与 `#include <vector>`。

`populateStagePanels` 形参表:在 `AsyncCommandRunner asyncRunner = {}` **之前**插入三个新参(全带默认空值,B3 加的 asyncRunner 保持末位;既有调用方零改动):
```cpp
                         StageChangedCallback stageChanged = {},
                         PathDraftProvider pathDraftProvider = {},
                         PathDraftMutator pathDraftMutator = {},
                         PathPickingSetter pathPickingSetter = {},
                         AsyncCommandRunner asyncRunner = {});
```
XQMainWindow.cpp 调用点传满全部参数,不依赖默认值。

### 3.2 buildPathPage 整段重写

before:XQStageWidgets.cpp:220-247 现 stub 整个函数体(三个 `(void)`、hint bar、"No fabricated centerline...")。after(整函数替换;签名扩为收新 provider):

```cpp
QWidget* buildPathPage(QStackedWidget* panel,
                       PathController* path,
                       const IdCounter& counter,
                       const ActiveImageProvider& imageProvider,
                       const PathDraftProvider& draftProvider,
                       const PathDraftMutator& draftMutator,
                       const PathPickingSetter& pickingSetter,
                       const StageChangedCallback& stageChanged,
                       const AsyncCommandRunner& asyncRunner)
{
    QVBoxLayout* body = nullptr;
    QWidget* page = makeStagePage(
        panel, "Path",
        xqTr("Path"),
        xqTr("Pick control points in the MPR slices, then generate a centerline."),
        &body);
    QLabel* status = makeStatusLabel(page, "Path");

    QGroupBox* group = new QGroupBox(xqTr("Path parameters"), page);
    QFormLayout* form = new QFormLayout(group);
    tidyForm(form);

    QLineEdit* nameEdit = new QLineEdit(QStringLiteral("path-1"), group);
    nameEdit->setObjectName(QStringLiteral("xqPathNameEdit"));

    QPushButton* pickToggle = new QPushButton(xqTr("Pick Control Points"), group);
    pickToggle->setObjectName(QStringLiteral("xqPathPickButton"));
    pickToggle->setCheckable(true);

    QListWidget* pointList = new QListWidget(group);
    pointList->setObjectName(QStringLiteral("xqPathPointList"));
    pointList->setSelectionMode(QAbstractItemView::SingleSelection);

    QPushButton* removeButton = new QPushButton(xqTr("Remove Selected"), group);
    removeButton->setObjectName(QStringLiteral("xqPathRemoveButton"));
    QPushButton* clearButton = new QPushButton(xqTr("Clear"), group);
    clearButton->setObjectName(QStringLiteral("xqPathClearButton"));

    QDoubleSpinBox* spacingSpin = new QDoubleSpinBox(group);
    spacingSpin->setObjectName(QStringLiteral("xqPathSpacingSpin"));
    spacingSpin->setRange(0.01, 100.0);
    spacingSpin->setValue(0.5);
    spacingSpin->setSuffix(QStringLiteral(" mm"));

    form->addRow(xqTr("Name"), nameEdit);
    form->addRow(pickToggle);
    form->addRow(pointList);
    form->addRow(removeButton);
    form->addRow(clearButton);
    form->addRow(xqTr("Sample spacing"), spacingSpin);

    QPushButton* run = new QPushButton(xqTr("Generate Path"), page);
    run->setObjectName(QStringLiteral("xqPathRunButton"));
    styleExecuteButton(run, "tool-path.svg");
    run->setEnabled(false); // needs >= 2 draft points

    QLabel* hint = makeHintBar(
        page, "Path",
        xqTr("Open an image, toggle picking, then click points in an MPR slice."));

    body->addWidget(group);
    body->addWidget(run);
    body->addWidget(hint);
    body->addWidget(status);
    body->addStretch();

    if (path == nullptr || !draftProvider || !draftMutator || !pickingSetter) {
        group->setEnabled(false);
        run->setEnabled(false);
        status->setText(xqTr("Controller not attached"));
        return page;
    }

    // List refresh: re-renders the draft into the list and gates the run
    // button (>= 2 points). Registered with the window via pickingSetter so
    // MPR picks refresh the page too.
    auto refreshList = [pointList, run, draftProvider]() {
        const std::vector<PathControlPoint> draft = draftProvider();
        pointList->clear();
        for (std::size_t i = 0; i < draft.size(); ++i) {
            pointList->addItem(QString::fromLatin1("#%1 (%2, %3, %4)")
                                   .arg(i + 1)
                                   .arg(draft[i].position.x, 0, 'f', 1)
                                   .arg(draft[i].position.y, 0, 'f', 1)
                                   .arg(draft[i].position.z, 0, 'f', 1));
        }
        run->setEnabled(draft.size() >= 2);
    };
    // Toggle setter the window uses to force the pick toggle off (mutual
    // exclusion with seed picking). QSignalBlocker keeps the forced state
    // change from re-entering pickingSetter through the toggled handler.
    auto setPickChecked = [pickToggle](bool on) {
        const QSignalBlocker blocker(pickToggle);
        pickToggle->setChecked(on);
    };
    // Register both hooks immediately (picking disabled) so window-side draft
    // changes (test hook / MPR picks) reach the list from the start.
    pickingSetter(false, PathPageHooks{refreshList, setPickChecked});
    refreshList();

    QObject::connect(pickToggle, &QPushButton::toggled, page,
                     [pickingSetter, refreshList, setPickChecked, status](bool checked) {
        pickingSetter(checked, PathPageHooks{refreshList, setPickChecked});
        status->setText(checked
                            ? xqTr("Click a voxel in an MPR slice to add a control point.")
                            : xqTr("Picking off."));
    });

    QObject::connect(removeButton, &QPushButton::clicked, page,
                     [pointList, draftMutator]() {
        const int row = pointList->currentRow();
        if (row >= 0) {
            draftMutator(row);
        }
    });

    QObject::connect(clearButton, &QPushButton::clicked, page,
                     [draftMutator]() {
        draftMutator(-1);
    });

    QObject::connect(run, &QPushButton::clicked, page,
                     [path, counter, nameEdit, spacingSpin, status, imageProvider,
                      draftProvider, draftMutator, setPickChecked,
                      stageChanged, asyncRunner]() {
        const std::vector<PathControlPoint> draft = draftProvider();
        if (draft.size() < 2) {
            status->setText(xqTr("Pick at least two control points first."));
            return;
        }
        // The path node binds to its source image via a derived relation;
        // link_derived fails on a missing source, so gate on a live image.
        const ActiveImage active = imageProvider ? imageProvider() : ActiveImage{};
        if (!active.valid) {
            status->setText(xqTr("Open an image first."));
            return;
        }

        PathController::AddPathIntent intent;
        intent.newPathId = nextId(counter);
        intent.name = nameEdit->text().toStdString();
        intent.sourceImageNode = active.nodeId;
        intent.controlPoints = draft;
        intent.spacing = spacingSpin->value();

        StageCommandJob job = [path, intent]() {
            StageCommandOutcome outcome;
            PathController::PreparedCommand prepared = path->prepareAddPath(intent);
            outcome.message = prepared.ok()
                ? xqTr("Path created: %1").arg(QString::fromStdString(intent.name))
                : text(prepared.status);
            outcome.command = std::move(prepared.command);
            return outcome;
        };

        auto done = [status, draftMutator, setPickChecked, stageChanged,
                     nameEdit](bool ok, const QString& message) {
            status->setText(message);
            if (ok) {
                draftMutator(-1); // clear the committed draft (refreshes list)
                setPickChecked(false);
                // Auto-increment the default name suffix: path-1 -> path-2.
                const QString name = nameEdit->text();
                const int dash = name.lastIndexOf(QLatin1Char('-'));
                bool numeric = false;
                const int n = name.mid(dash + 1).toInt(&numeric);
                if (dash > 0 && numeric) {
                    nameEdit->setText(name.left(dash + 1) + QString::number(n + 1));
                }
                if (stageChanged) {
                    stageChanged();
                }
            }
        };

        if (asyncRunner) {
            if (!asyncRunner(xqTr("Path"), job, done)) {
                status->setText(xqTr("Another task is still running."));
            }
            return;
        }
        StageCommandOutcome outcome = job();
        const bool ok = outcome.command != nullptr
            && path->commitPrepared(std::move(outcome.command));
        done(ok, outcome.message);
    });

    return page;
}
```
include 区补 `#include <QListWidget>`、`#include <QAbstractItemView>`、`#include <QSignalBlocker>`。
`populateStagePanels` 里对 buildPathPage 的调用改传新参:
```cpp
    panel->addWidget(buildPathPage(panel, path, counter, imageProvider,
                                   pathDraftProvider, pathDraftMutator,
                                   pathPickingSetter, stageChanged, asyncRunner));
```
**图像前置检查依据(已对源码核实)**:`AddNodeWithSourceRelationCommand::execute` 里 `link_derived(source, id)` 对不存在的 source 返回 `SourceMissing` → execute 失败(XQSceneCommands.cpp:59-79 + XQScene.cpp:85-104)——`sourceImageNode` 无效时命令必然执行失败,所以 run 处理器里的 `if (!active.valid)` 检查(上方代码已含)是必需项,不是可选兜底。

### 3.3 互斥小结(无新增改动,验收口径)

互斥全部由 2.3(窗口侧 pickMode_ + pathPickToggleSetter_)与 3.2(页面侧 PathPageHooks + QSignalBlocker)覆盖。验收口径:Seg 页点 Pick Seed 后,Path 页 toggle 立即变 unchecked 且 MPR 点击走 Seed 分支;Path 页开拾取后,seed 标记被清、MPR 点击走 PathPoint 分支。真机与 test_path_stage 各覆盖一半(测试只断言 PathPoint 链路;Seed 压灭 toggle 属 B6 真机清单)。

### 3.4 i18n

新 xqTr/tr 串(Pick Control Points / Remove Selected / Clear / Sample spacing / Generate Path / Path created: %1 / Pick at least two control points first. / Click a voxel in an MPR slice to add a control point. / Picking off. / Path parameters / Open an image, toggle picking, then click points in an MPR slice. / 新 subtitle)进 ts:
```bash
/c/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lupdate.exe \
  src -ts resources/i18n/xq_zh_CN.ts -no-obsolete
/c/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lrelease.exe \
  resources/i18n/xq_zh_CN.ts -qm resources/i18n/xq_zh_CN.qm
```
lupdate 后人工补翻全部新词条(中文),并核查 `git diff resources/i18n/xq_zh_CN.ts`:stub 时代旧词条("Path creation is disabled until..." 等)被 `-no-obsolete` 删除是**预期**;除此之外的既有词条被删 → **停下报告,不许自行修 ts**。
**i18n 测试挂钩**:`tests/app/test_i18n_resources.cpp` 追加对 2 条新串的断言(照 :55-60 Axial 模式):
```cpp
    const QString genPath =
        translator.translate("XQStageWidgets", "Generate Path");
    if (genPath.isEmpty() || genPath == QStringLiteral("Generate Path")) {
        return fail("XQStageWidgets Generate Path translates to Chinese");
    }
    const QString pickPoints =
        translator.translate("XQStageWidgets", "Pick Control Points");
    if (pickPoints.isEmpty() || pickPoints == QStringLiteral("Pick Control Points")) {
        return fail("XQStageWidgets Pick Control Points translates to Chinese");
    }
```

### 3.5 Commit 1

```
feat: Path 阶段真实化 — MPR 拾取控制点分流 + Path 页重写接 prepareAddPath
```
文件:
- `src/app/XQMainWindow.h`
- `src/app/XQMainWindow.cpp`
- `src/ui/panels/XQStageWidgets.h`
- `src/ui/panels/XQStageWidgets.cpp`
- `tests/app/test_i18n_resources.cpp`
- `resources/i18n/xq_zh_CN.ts`
- `resources/i18n/xq_zh_CN.qm`

(commit 前:完整构建 + 全量 ctest 65/65;test_i18n_resources 的新断言在 lrelease 后应绿。)

---

## 4. Commit 2 — tests/app/test_path_stage.cpp(offscreen 全链路)

### 4.1 CMake 注册(照 test_main_window 模式)

```cmake
add_executable(test_path_stage
    tests/app/test_path_stage.cpp
)

set_target_properties(test_path_stage PROPERTIES AUTOMOC ON)

target_link_libraries(test_path_stage PRIVATE
    xq_app_shell
)
target_compile_definitions(test_path_stage PRIVATE
    XQ_TEST_VTI_PATH="${_xq_vti_path}"
)
vtk_module_autoinit(
    TARGETS test_path_stage
    MODULES ${VTK_LIBRARIES}
)
```
`add_test(NAME test_path_stage COMMAND test_path_stage)`;PATH/offscreen:把 `test_path_stage` 加进既有 `set_tests_properties(test_main_window test_app_startup test_i18n_resources PROPERTIES ...)` 名单(两个分支——qt_plugin_path 有/无——都加)。

### 4.2 测试内容

```cpp
#include <app/XQMainWindow.h>

#include <core/GeometryTypes.h>
#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQScene.h>
#include <core/command/XQCommandStack.h>
#include <visualization/XQRenderWidget.h>

#include <QApplication>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

#include <cstdio>
#include <map>

#ifndef XQ_TEST_VTI_PATH
#error "XQ_TEST_VTI_PATH must be defined"
#endif

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

std::size_t count_domain(const xq::XQScene& scene, xq::XQDomainType domain)
{
    std::size_t count = 0;
    scene.visit_nodes([&count, domain](const xq::XQDataNode& node) {
        if (node.domainType() == domain) {
            ++count;
        }
    });
    return count;
}

} // namespace

int main(int argc, char** argv)
{
    xq::XQRenderWidget::configureDefaultSurfaceFormat();
    QApplication app(argc, argv);

    xq::XQScene scene;
    xq::XQCommandStack stack;
    xq::XQMainWindow window;
    window.setProperty("xqSuppressDialogs", true);
    window.attachWorkflow(&scene, &stack);
    window.resize(1440, 920);
    window.show();
    QApplication::processEvents();

    // Real image: gives the draft picks a source image node + voxel frame.
    const bool imageLoaded =
        window.loadImageFromPath(QString::fromUtf8(XQ_TEST_VTI_PATH));
    CHECK(imageLoaded);

    QPushButton* runButton = window.findChild<QPushButton*>("xqPathRunButton");
    QListWidget* pointList = window.findChild<QListWidget*>("xqPathPointList");
    QDoubleSpinBox* spacing = window.findChild<QDoubleSpinBox*>("xqPathSpacingSpin");
    QLineEdit* nameEdit = window.findChild<QLineEdit*>("xqPathNameEdit");
    CHECK(runButton != nullptr);
    CHECK(pointList != nullptr);
    CHECK(spacing != nullptr);
    CHECK(nameEdit != nullptr);
    CHECK(!runButton->isEnabled()); // no draft yet

    // Three picks through the test hook (same path as an MPR click).
    window.appendPathDraftPointForTest({0.0, 0.0, 0.0});
    window.appendPathDraftPointForTest({10.0, 0.0, 0.0});
    QApplication::processEvents();
    CHECK(pointList->count() == 2);
    CHECK(runButton->isEnabled()); // >= 2 points gate
    window.appendPathDraftPointForTest({10.0, 10.0, 0.0});
    QApplication::processEvents();
    CHECK(pointList->count() == 3);

    spacing->setValue(0.5);
    nameEdit->setText(QStringLiteral("centerline-test"));

    const std::size_t pathsBefore = count_domain(scene, xq::XQDomainType::Path);
    runButton->click();

    // The run goes through the async runner; pump until the Path node lands.
    QElapsedTimer timer;
    timer.start();
    while (count_domain(scene, xq::XQDomainType::Path) == pathsBefore
           && timer.elapsed() < 30000) {
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    CHECK(count_domain(scene, xq::XQDomainType::Path) == pathsBefore + 1);

    // Draft cleared + list emptied + run disabled again.
    QApplication::processEvents();
    CHECK(pointList->count() == 0);
    CHECK(!runButton->isEnabled());

    // undo/redo symmetry through the window path.
    window.undo();
    CHECK(count_domain(scene, xq::XQDomainType::Path) == pathsBefore);
    window.redo();
    CHECK(count_domain(scene, xq::XQDomainType::Path) == pathsBefore + 1);

    // Rejected input surfaces a message, adds nothing: spacing 0 is clamped by
    // the spin box (min 0.01), so drive rejection with a 1-point draft instead.
    window.appendPathDraftPointForTest({0.0, 0.0, 1.0});
    QApplication::processEvents();
    CHECK(!runButton->isEnabled()); // 1 point: gate holds, honest disable

    std::printf("OK: path stage end-to-end\n");
    return 0;
}
```
(`xqSuppressDialogs` 属性是 B2 引入的弹窗抑制;Path 链路不弹窗,属防御。)

### 4.3 先红验证(固定顺序,不可变通)

开发顺序:**先写本测试 + CMake 注册(Commit 1 动工之前)** → 完整构建 → 跑 `test_path_stage` → **红在 `runButton != nullptr`**(buildPathPage 还是 stub,objectName `xqPathRunButton` 不存在)→ 红输出留档进 result → 再做 Commit 1 → 本测试绿。commit 顺序不变(实现是 Commit 1,测试文件与 CMake 注册归 Commit 2),只是测试**文件**先写好。

### 4.4 假绿抽查(做这一处,不做别的)

Commit 1 完成后:把窗口 seedPicked 分流里 PathPoint 分支的 `if (pathDraftChanged_) { pathDraftChanged_(); }` 与 `appendPathDraftPointForTest` 里的同款调用**两处都**临时注释 → 完整构建 → `test_path_stage` **必红**(`pointList->count() == 2` 失败)→ 还原两处,完整构建,确认绿。

### 4.5 Commit 2

```
test: Path 阶段 offscreen 全链路 — 拾取三点生成路径,undo/redo 对称,draft 清空
```
文件:
- `tests/app/test_path_stage.cpp`
- `CMakeLists.txt`

---

## 5. 批次收尾

1. 两个 commit 后删 `build_gui` 全新重建 + 全量 ctest,**66/66 绿**(65 + test_path_stage)。
2. `git log --oneline -2` 对照;`git status` 干净。
3. 汇报:commit hash、4.3 的红输出摘录(或 4.4 抽查红)、i18n 新词条补翻数与 lrelease 输出。**没验证过不写"完成/通过"。**
