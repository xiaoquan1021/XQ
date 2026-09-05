# EXECUTE-B3 — S3 XQTaskRunner + 后台任务化 + 重活迁移

> **执行者须知**:本文档每处 before 代码均逐字对照过 `feat/gui-v2` 真实源码(B2 完成后行号会漂移,**符号为准,改前 rg 定位**)。你的任务是**照做**,不允许做本文之外的设计决策。before 与实际源码内容对不上时停下报告。
> **依赖:B2 已完成**(attachWorkflow 已可重入;若 `git log` 里没有 B2 的三个 commit,停下报告)。

---

## 0. 环境与固定纪律

- 工作树:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/gui-v2`(Git Bash:`/c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`)。
- 完整构建:`cmd //c build_gui_wt.bat`;全量测试:`cmd //c ctest_merge.bat`;单测:
  ```bash
  cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui && \
  "C:/software/Visual Studio/Visual Studio2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe" \
    -R '^test_task_runner$' --output-on-failure
  ```
- **绝不 `cmake --build --target <单目标>`**;改完必跑完整 build_gui_wt.bat 并核对 exe 时间戳。
- 副作用调用先存变量再 CHECK;绝不 `git add -A`。
- 全局约束:只做本批(**Path 页真实化是 B4,本批 Path 页不动**);不加未要求的兜底/降级;不覆盖用户未提交改动;**services/core/controllers 零 Qt/VTK**(xq_controllers 是纯 C++ 库,prepare* 拆分绝不能引入 Qt 类型;test_arch_boundaries 只查 core/services/io/adapters,controllers 的纪律靠链接边界:xq_controllers 的 target_link_libraries 只有 xq_core+xq_services,引 Qt 头会编译失败);不破坏 Source 1.0 签名;不参考 XQ1;没验证过不写"完成/通过"。
- **线程铁律(work 闭包纪律,每处迁移必须遵守)**:work() 在 worker 线程跑——**零 scene 写、零 widget 触碰、零渲染调用**;commit() 回 GUI 线程做 stack->push 与 UI 更新。
- 基线:开工前全量 ctest **64/64**(B2 后基线;不符停下报告)。

本批 4 个 commit:C1 prepare* 拆分(纯 controllers)→ C2 XQTaskRunner+测试 → C3 页面 hook+窗口接线+忙态 → C4 窗口异步集成测试。

---

## 1. rg 定位

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rg -n 'Status (addPath|threshold|regionGrow|loft|buildSurfaceMesh|buildVolumeMesh|solve)\(' src/ui/controllers
rg -n 'stack_->push' src/ui/controllers
rg -n 'populateStagePanels' src/ui/panels src/app
rg -n 'QObject::connect\(run' src/ui/panels/XQStageWidgets.cpp
rg -n 'onSceneContextMenu' src/app/XQMainWindow.cpp
rg -n 'add_library\(xq_app_shell' -A 12 CMakeLists.txt
```

已核实的结构事实:
- 六 controller 全部两段式:「service 静态函数产出 `{Status, unique_ptr<XQCommand>}` → `stack_->push`」。scene 变更只发生在 push→execute();命令工厂只读输入(AddNode* 构造仅存指针,XQSceneCommands.cpp 已核实)。
- `SegmentationController::threshold/regionGrow` 经私有 `commitMask(newMaskId, name, mask)`(SegmentationController.cpp:17-31)——mask 计算(重活)在 `SegmentationService::thresholdMask/regionGrowMask`,命令构造在 `createMaskNodeCommand`(轻)。
- `MeshingController::buildSurfaceMesh/buildVolumeMesh` 各有一次 `modelOf(scene_, intent.modelNode)` scene 读(MeshingController.cpp:48/:67)。
- `FlowController::solve`:validateAndBind → FlowSolver1D::solve(重)→ buildFlowResultCommand(FlowController.cpp:16-43)。
- `ModelingController::loft`:buildLoftInput → loftSurface(重)→ capModel → createModelNodeCommand(ModelingController.cpp:17-56)。
- `AiController::analyzeFlow`:纯数组度量,秒级,**不拆,保持同步**。
- 页面 run lambda 六处:XQStageWidgets.cpp `:335`(seg)、`:447`(model)、`:516`(mesh)、`:590`(flow)、`:677`(ai);Path 页是 stub(B4 才动)。
- 页面回调模式先例:`ActiveImageProvider` 等 `std::function` 别名(XQStageWidgets.h:37-62),`populateStagePanels` 尾部默认空参(:73-85)。
- 全仓 0 处 QThread/QtConcurrent/std::thread/moveToThread/invokeMethod(rg 已核实)。
- `onSceneContextMenu`(XQMainWindow.cpp:1769)是忙态之外唯一 scene 写入口。
- xq_app_shell:AUTOMOC ON(CMakeLists.txt:189),XQTaskRunner 放这里 moc 无忧。
- CommandResult 形态各 service 自带(同构 `{Status status; unique_ptr<XQCommand> command; ok()}`),controllers 无统一类型 → prepare* 的返回类型在**各 controller 头内独立定义**(见 2.1),不造跨 controller 公共头。

---

## 2. Commit 1 — controller prepare* 拆分(纯追加,零 Qt)

**目的**:把「算」与「推」分开——`prepare*` = 计算+构造命令(worker 可跑),既有同步方法改为 `prepare* + stack_->push`(行为逐字节等价,现测试全绿即证;**无先红后绿,门禁 = 全量绿 + C4 的线程断言**)。

### 2.1 PathController

`src/ui/controllers/PathController.h`,`enum class Status` 之后追加:
```cpp
    // A computed-but-not-committed command: prepare*() runs the service work
    // (safe on a worker thread -- pure computation over the copied intent) and
    // the caller pushes the command on the GUI thread.
    struct PreparedCommand {
        Status status = Status::Rejected;
        std::unique_ptr<XQCommand> command; // null unless status == Ok

        bool ok() const { return status == Status::Ok && command != nullptr; }
    };
    PreparedCommand prepareAddPath(const AddPathIntent& intent);
```
头文件 include 区补 `#include "core/command/XQCommand.h"` 与 `#include <memory>`(现状经 services 头间接可见,显式补上防脆弱)。

`PathController.cpp` 的 `addPath` 改为,before(逐字,:16-30):
```cpp
PathController::Status PathController::addPath(const AddPathIntent& intent)
{
    if (scene_ == nullptr || stack_ == nullptr) {
        return Status::NullScene;
    }

    PathService::Result result = PathService::createPathCommand(
        scene_, intent.newPathId, intent.name, intent.sourceImageNode,
        intent.controlPoints, intent.spacing);
    if (!result.ok() || result.command == nullptr) {
        return Status::Rejected;
    }

    return stack_->push(std::move(result.command)) ? Status::Ok : Status::Rejected;
}
```
after:
```cpp
PathController::PreparedCommand PathController::prepareAddPath(const AddPathIntent& intent)
{
    PreparedCommand prepared;
    if (scene_ == nullptr || stack_ == nullptr) {
        prepared.status = Status::NullScene;
        return prepared;
    }

    PathService::Result result = PathService::createPathCommand(
        scene_, intent.newPathId, intent.name, intent.sourceImageNode,
        intent.controlPoints, intent.spacing);
    if (!result.ok() || result.command == nullptr) {
        prepared.status = Status::Rejected;
        return prepared;
    }

    prepared.status = Status::Ok;
    prepared.command = std::move(result.command);
    return prepared;
}

PathController::Status PathController::addPath(const AddPathIntent& intent)
{
    PreparedCommand prepared = prepareAddPath(intent);
    if (!prepared.ok()) {
        return prepared.status;
    }
    return stack_->push(std::move(prepared.command)) ? Status::Ok : Status::Rejected;
}
```

### 2.2 其余五个 controller 同构

每个 controller 头加同形 `struct PreparedCommand`(字段/ok() 完全同 2.1,status 用各自的 Status 枚举)+ prepare 方法;.cpp 把原方法体搬进 prepare*(**push 之前的全部逻辑逐字搬**,包括 scene 读、参数检查、service 调用),原方法改成「prepare + push」三行式。对照表:

| controller | 新 prepare 方法 | 搬入 prepare 的内容(逐字保序) |
|---|---|---|
| Segmentation | `prepareThreshold(const ThresholdIntent&)` / `prepareRegionGrow(const RegionGrowIntent&)` | null 检查 → image/buffer 检查 → `SegmentationService::thresholdMask/regionGrowMask` → `createMaskNodeCommand`(commitMask 里 push 之前那半) |
| Modeling | `prepareLoft(const LoftIntent&)` | null 检查 → group 拷贝+setId → buildLoftInput → loftSurface → capModel → createModelNodeCommand |
| Meshing | `prepareSurfaceMesh(const SurfaceMeshIntent&)` / `prepareVolumeMesh(const VolumeMeshIntent&)` | null 检查 → `modelOf(scene_, ...)` → build*MeshCommand(mesher_ 透传) |
| Flow | `prepareSolve(const SolveIntent&)` | null 检查 → validateAndBind → FlowSolver1D::solve → buildFlowResultCommand |
| Ai | **不拆** | — |

细节:
- SegmentationController:prepare* 末尾调 `createMaskNodeCommand` 并填 PreparedCommand;同步方法 `threshold/regionGrow` 改「prepare + push」。私有 `commitMask` **保留原样**——拆分后它只剩 `aiSegment` 一个调用方(aiSegment 走 backend,不在六类重活里,不拆不动)。
- MeshingController 的 `modelOf` scene 读**留在 prepare 内**(设计定论:busy 期间全部 scene 写入口被禁 = 全局写锁,worker 读 scene 与 GUI 渲染读是读读并发,安全)。
- Flow 的中间失败映射:prepare 里 `!bound.ok()` → `prepared.status = Status::InvalidBoundaryConditions`,`!solved.ok()` → `SolveFailed`,命令构造失败 → `Rejected`,全部照旧枚举。
- **commitPrepared(本 commit 一并加)**:五个被拆的 controller(Path/Segmentation/Modeling/Meshing/Flow,**不含 Ai**)头文件内各加一个内联公共方法,供 C3 的同步回退路径使用:
  ```cpp
      // Pushes a prepared command on the stack (GUI-thread half of prepare*).
      bool commitPrepared(std::unique_ptr<XQCommand> command)
      {
          return stack_ != nullptr && command != nullptr
              && stack_->push(std::move(command));
      }
  ```
  (纯 std,零 Qt;头文件内联实现,.cpp 不加东西。)

### 2.3 验证与假绿抽查

- 完整构建 → 全量 ctest **64/64**(`test_workflow_controllers`、`test_workflow_integration` 零改动全绿 = 等价性证据)。
- 假绿抽查:把 `PathController::addPath` 里 `stack_->push(std::move(prepared.command))` 临时改为 `true`(不 push)→ 构建 → `test_workflow_controllers` **必红**(场景无节点);还原,构建,绿。

### 2.4 Commit 1

```
refactor: 六 controller 拆出 prepare* 变体 — 计算与提交分离,行为等价
```
文件:
- `src/ui/controllers/PathController.{h,cpp}`
- `src/ui/controllers/SegmentationController.{h,cpp}`
- `src/ui/controllers/ModelingController.{h,cpp}`
- `src/ui/controllers/MeshingController.{h,cpp}`
- `src/ui/controllers/FlowController.{h,cpp}`

---

## 3. Commit 2 — XQTaskRunner + test_task_runner

### 3.1 新文件 `src/app/XQTaskRunner.h`

```cpp
#ifndef XQ_APP_XQ_TASK_RUNNER_H
#define XQ_APP_XQ_TASK_RUNNER_H

#include <QObject>
#include <QString>
#include <QThread>

#include <functional>
#include <memory>

namespace xq {

// One resident worker thread for the app shell's heavy jobs (segmentation,
// meshing, solving, loading). Single-task discipline: while busy() a second
// run() is refused (returns false), which is the whole-app concurrency model --
// the busy matrix in XQMainWindow disables every input that could start work
// or mutate the scene.
//
// work() runs on the worker thread. HARD RULES for work closures: no scene
// writes, no widget access, no rendering, no QSettings -- pure computation over
// inputs the closure owns (copied values / shared_ptr keepalives). commit()
// runs back on the GUI thread and is the only place results touch the scene
// (via the command stack) or the UI.
class XQTaskRunner : public QObject {
    Q_OBJECT
public:
    explicit XQTaskRunner(QObject* parent = nullptr); // starts the worker thread
    ~XQTaskRunner() override;                          // quit() + wait()

    XQTaskRunner(const XQTaskRunner&) = delete;
    XQTaskRunner& operator=(const XQTaskRunner&) = delete;

    bool busy() const;

    // Queues work on the worker thread; commit(result) is delivered back to
    // this object's (GUI) thread after work returns. Returns false without
    // running anything while busy.
    bool run(const QString& label,
             std::function<std::shared_ptr<void>()> work,
             std::function<void(std::shared_ptr<void>)> commit);

signals:
    void taskStarted(const QString& label);
    void taskFinished(const QString& label);

private:
    QThread thread_;
    QObject workerContext_; // lives on thread_; the anchor invokeMethod targets
    bool busy_ = false;     // GUI-thread-only state (run/finish both run there)
};

} // namespace xq

#endif // XQ_APP_XQ_TASK_RUNNER_H
```

### 3.2 新文件 `src/app/XQTaskRunner.cpp`

```cpp
#include "app/XQTaskRunner.h"

#include <utility>

namespace xq {

XQTaskRunner::XQTaskRunner(QObject* parent)
    : QObject(parent)
{
    workerContext_.moveToThread(&thread_);
    thread_.setObjectName(QStringLiteral("xqTaskRunnerWorker"));
    thread_.start();
}

XQTaskRunner::~XQTaskRunner()
{
    thread_.quit();
    thread_.wait();
}

bool XQTaskRunner::busy() const
{
    return busy_;
}

bool XQTaskRunner::run(const QString& label,
                       std::function<std::shared_ptr<void>()> work,
                       std::function<void(std::shared_ptr<void>)> commit)
{
    if (busy_ || !work) {
        return false;
    }
    busy_ = true;
    emit taskStarted(label);

    // Double queued hop: GUI -> worker (run work) -> GUI (commit + finish).
    // Functor-based invokeMethod needs no qRegisterMetaType (Qt 6).
    QMetaObject::invokeMethod(
        &workerContext_,
        [this, label, work = std::move(work), commit = std::move(commit)]() {
            std::shared_ptr<void> result = work();
            QMetaObject::invokeMethod(
                this,
                [this, label, commit, result]() {
                    if (commit) {
                        commit(result);
                    }
                    busy_ = false;
                    emit taskFinished(label);
                },
                Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
    return true;
}

} // namespace xq
```
不做取消(任务分钟内;取消语义留后续任务)。

### 3.3 CMake

- xq_app_shell 源列表(`add_library(xq_app_shell ...)`)加两行:
  ```cmake
      src/app/XQTaskRunner.cpp
      src/app/XQTaskRunner.h
  ```
- 新测试注册(**定形:test_task_runner 单独编译 XQTaskRunner.cpp,只链 Qt6::Core**——不链 xq_app_shell,避免拖 VTK,零 VTK 依赖,启动快):
  ```cmake
  add_executable(test_task_runner
      src/app/XQTaskRunner.cpp
      src/app/XQTaskRunner.h
      tests/app/test_task_runner.cpp
  )
  set_target_properties(test_task_runner PROPERTIES AUTOMOC ON)
  target_include_directories(test_task_runner PRIVATE src)
  target_link_libraries(test_task_runner PRIVATE Qt6::Core)
  ```
  `add_test(NAME test_task_runner COMMAND test_task_runner)`(加在 add_test 名单区);PATH:加进 `set_tests_properties(test_scene_model PROPERTIES ...)` 那组(`:1088-1090`,同为 Qt-only 测试),即改成 `set_tests_properties(test_scene_model test_task_runner PROPERTIES ...)`。

### 3.4 新文件 `tests/app/test_task_runner.cpp`(先红不适用——新组件;门禁 = 线程断言 + busy 语义断言)

```cpp
#include <app/XQTaskRunner.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

#include <cstdio>
#include <memory>

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

// Runs the event loop until taskFinished fires (or a 30s watchdog trips).
bool waitForFinished(xq::XQTaskRunner* runner)
{
    QEventLoop loop;
    bool finished = false;
    QObject::connect(runner, &xq::XQTaskRunner::taskFinished, &loop,
                     [&loop, &finished](const QString&) {
        finished = true;
        loop.quit();
    });
    QTimer::singleShot(30000, &loop, &QEventLoop::quit); // watchdog
    loop.exec();
    return finished;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QThread* guiThread = QThread::currentThread();

    xq::XQTaskRunner runner;

    // 1) work runs off the GUI thread; commit runs on it; busy sequences.
    {
        QThread* workThread = nullptr;
        QThread* commitThread = nullptr;
        bool busyDuringWork = false;
        int order = 0;
        int workOrder = 0;
        int commitOrder = 0;

        CHECK(!runner.busy());
        const bool started = runner.run(
            QStringLiteral("job-1"),
            [&workThread, &order, &workOrder]() -> std::shared_ptr<void> {
                workThread = QThread::currentThread();
                workOrder = ++order;
                return std::make_shared<int>(42);
            },
            [&commitThread, &order, &commitOrder, &busyDuringWork,
             &runner](std::shared_ptr<void> result) {
                commitThread = QThread::currentThread();
                commitOrder = ++order;
                busyDuringWork = runner.busy(); // still busy inside commit
                const int value = *std::static_pointer_cast<int>(result);
                if (value != 42) {
                    std::exit(2);
                }
            });
        CHECK(started);
        CHECK(runner.busy());

        const bool finished = waitForFinished(&runner);
        CHECK(finished);
        CHECK(!runner.busy());
        CHECK(workThread != nullptr && workThread != guiThread);
        CHECK(commitThread == guiThread);
        CHECK(workOrder == 1 && commitOrder == 2);
        CHECK(busyDuringWork);
    }

    // 2) second run while busy is refused and does not run its work.
    {
        bool secondRan = false;
        const bool first = runner.run(
            QStringLiteral("job-2"),
            []() -> std::shared_ptr<void> {
                QThread::msleep(200); // hold busy long enough to test refusal
                return nullptr;
            },
            [](std::shared_ptr<void>) {});
        CHECK(first);
        const bool second = runner.run(
            QStringLiteral("job-3"),
            [&secondRan]() -> std::shared_ptr<void> {
                secondRan = true;
                return nullptr;
            },
            [](std::shared_ptr<void>) {});
        CHECK(!second);

        const bool finished = waitForFinished(&runner);
        CHECK(finished);
        CHECK(!secondRan);
        CHECK(!runner.busy());
    }

    // 3) null-result work is fine; taskStarted fires with the label.
    {
        QString startedLabel;
        QObject::connect(&runner, &xq::XQTaskRunner::taskStarted, &runner,
                         [&startedLabel](const QString& label) {
            startedLabel = label;
        });
        const bool started = runner.run(
            QStringLiteral("job-4"),
            []() -> std::shared_ptr<void> { return nullptr; },
            [](std::shared_ptr<void> result) {
                if (result != nullptr) {
                    std::exit(3);
                }
            });
        CHECK(started);
        const bool finished = waitForFinished(&runner);
        CHECK(finished);
        CHECK(startedLabel == QStringLiteral("job-4"));
    }

    std::printf("OK: task runner threading + busy discipline\n");
    return 0;
}
```

**验证**:完整构建 → `test_task_runner` 绿 → 全量 **65/65**(64+新测试)。

### 3.5 假绿抽查

把 XQTaskRunner::run 里外层 `QMetaObject::invokeMethod(&workerContext_, ...)` 临时改成直接在当前线程调用 lambda 体(即 work 在 GUI 线程直跑)→ 构建 → `test_task_runner` **必红**(`workThread != guiThread` 失败);还原,构建,绿。

### 3.6 Commit 2

```
feat: XQTaskRunner 常驻工作线程 — 双跳队列投递,单任务忙态语义
```
文件:
- `src/app/XQTaskRunner.h`
- `src/app/XQTaskRunner.cpp`
- `tests/app/test_task_runner.cpp`
- `CMakeLists.txt`

---

## 4. Commit 3 — 页面 AsyncCommandRunner hook + 窗口接线 + 忙态矩阵

### 4.1 XQStageWidgets.h 追加(签名纯追加)

`StageChangedCallback` 别名之后加:
```cpp
// A stage command computed off the GUI thread: the job runs controller
// prepare*() and maps the status to the page's user-facing message; the shell
// commits the command on the GUI thread and reports back through `done`.
struct StageCommandOutcome {
    std::unique_ptr<XQCommand> command; // null on failure
    QString message;                    // status text for the page label
};
using StageCommandJob = std::function<StageCommandOutcome()>;
// Returns false when a task is already running (the page then reports "busy").
using AsyncCommandRunner =
    std::function<bool(const QString& label, StageCommandJob job,
                       std::function<void(bool ok, const QString& message)> done)>;
```
include 区补 `#include "core/command/XQCommand.h"`、`#include <memory>`、`#include <QString>`(QString 现状未直接引;补上)。
`populateStagePanels` 形参表尾追加一个默认空参(**本批只加这一个;B4 的 path provider 参数 B4 再加**;现状 imageProvider 起的尾参已全部带 `= {}` 默认值——已核实 .h:80-85,新参照此风格排在最后):
```cpp
                         StageChangedCallback stageChanged = {},
                         AsyncCommandRunner asyncRunner = {});
```

### 4.2 五个数据页 run 处理器改造(seg×2 合一处、model、mesh×2 合一处、flow;AI 页不接)

模式(以 seg 页为例,其余四处同构)。before(逐字,XQStageWidgets.cpp:335-392 的 connect 整段,见源码)→ after 把 lambda 体改成:

```cpp
    QObject::connect(run, &QPushButton::clicked, panel,
                     [segmentation, imageProvider, counter, nameEdit, lowerSpin,
                      upperSpin, status, thresholdRadio, seedProvider, seedPickingSetter,
                      stageChanged, asyncRunner]() {
        const ActiveImage active = imageProvider();
        if (!active.valid || active.image == nullptr || active.buffer == nullptr) {
            status->setText(xqTr("Please select an image node first."));
            return;
        }

        // Gather the intent on the GUI thread (widgets are touched only here).
        const bool isThreshold = thresholdRadio->isChecked();
        SegmentationController::ThresholdIntent thresholdIntent;
        SegmentationController::RegionGrowIntent regionGrowIntent;
        if (isThreshold) {
            thresholdIntent.newMaskId = nextId(counter);
            thresholdIntent.name = nameEdit->text().toStdString();
            thresholdIntent.sourceImageNode = active.nodeId;
            thresholdIntent.image = active.image;
            thresholdIntent.buffer = active.buffer;
            thresholdIntent.params.lower = lowerSpin->value();
            thresholdIntent.params.upper = upperSpin->value();
        } else {
            if (!seedProvider) {
                status->setText(xqTr("Please pick a region seed first."));
                return;
            }
            const ActiveSeed seed = seedProvider();
            if (!seed.valid) {
                status->setText(xqTr("Please pick a region seed first."));
                return;
            }
            regionGrowIntent.newMaskId = nextId(counter);
            regionGrowIntent.name = nameEdit->text().toStdString();
            regionGrowIntent.sourceImageNode = active.nodeId;
            regionGrowIntent.image = active.image;
            regionGrowIntent.buffer = active.buffer;
            regionGrowIntent.params.seed[0] = seed.voxel[0];
            regionGrowIntent.params.seed[1] = seed.voxel[1];
            regionGrowIntent.params.seed[2] = seed.voxel[2];
            regionGrowIntent.params.lower = lowerSpin->value();
            regionGrowIntent.params.upper = upperSpin->value();
        }

        // The job body: worker-safe (controller prepare* + text mapping only).
        StageCommandJob job = [segmentation, isThreshold, thresholdIntent,
                               regionGrowIntent]() {
            StageCommandOutcome outcome;
            if (isThreshold) {
                SegmentationController::PreparedCommand prepared =
                    segmentation->prepareThreshold(thresholdIntent);
                outcome.message = prepared.ok()
                    ? xqTr("Threshold mask created.")
                    : xqTr("Segmentation rejected (check seed / thresholds / image).");
                outcome.command = std::move(prepared.command);
            } else {
                SegmentationController::PreparedCommand prepared =
                    segmentation->prepareRegionGrow(regionGrowIntent);
                outcome.message = prepared.ok()
                    ? xqTr("Region-grown mask created.")
                    : xqTr("Segmentation rejected (check seed / thresholds / image).");
                outcome.command = std::move(prepared.command);
            }
            return outcome;
        };

        auto done = [status, seedPickingSetter, stageChanged](bool ok,
                                                              const QString& message) {
            status->setText(message);
            if (ok) {
                if (seedPickingSetter) {
                    seedPickingSetter(false);
                }
                if (stageChanged) {
                    stageChanged();
                }
            }
        };

        if (asyncRunner) {
            if (!asyncRunner(xqTr("Segmentation"), job, done)) {
                status->setText(xqTr("Another task is still running."));
            }
            return;
        }

        // Headless / no-runner fallback: same job + done, run synchronously.
        StageCommandOutcome outcome = job();
        const bool ok = outcome.command != nullptr
            && segmentation->commitPrepared(std::move(outcome.command));
        done(ok, outcome.message);
    });
```

**⚠️ 生命周期定论(S3.2,必须照做,这是本批唯一容易写错的点)**:
- 上面 seg 的 intent 持 `active.image/active.buffer` **裸指针**——worker 跑时 GUI 可能换 `activeImage_` → 悬垂。**修法(写死)**:`ThresholdIntent/RegionGrowIntent` 的指针字段不动(冻结签名),但 **job 闭包必须自持保活副本**。给 job 加两个捕获:
  ```cpp
  // Keepalives: the window may replace activeImage_ while the worker runs; the
  // job owns a copy of the volume (light: metadata + geometry) and a shared_ptr
  // to the scalar buffer, and repoints the intent at them.
  auto imageCopy = std::make_shared<XQImageVolume>(*active.image);
  std::shared_ptr<XQMemoryImageBufferHandle> bufferKeepalive = active.bufferShared;
  ```
  其中 `active.bufferShared` 需要 `ActiveImage` 结构**纯追加**一个字段:
  ```cpp
  struct ActiveImage {
      bool valid = false;
      const XQImageVolume* image = nullptr;
      const XQMemoryImageBufferHandle* buffer = nullptr;
      // Shared keepalive for the scalar buffer (async jobs copy this so the
      // buffer outlives an activeImage_ swap mid-task). Null in legacy callers;
      // then `buffer` must only be used synchronously.
      std::shared_ptr<XQMemoryImageBufferHandle> bufferShared;
      NodeId nodeId;
  };
  ```
  窗口的 imageProvider(XQMainWindow.cpp:435-444)加一行 `active.bufferShared = activeImage_->buffer;`(XQDemoVolume::buffer 本就是 shared_ptr)。job 里把 intent 的指针重定向:`thresholdIntent.image = imageCopy.get(); thresholdIntent.buffer = bufferKeepalive.get();`(在 job 闭包体内、调 prepare 之前;imageCopy/bufferKeepalive 按值捕获进 job)。`bufferShared` 为空(旧调用方)时保持旧行为(仅同步路径会走到)。
- model 页:`LoftIntent` 按值持 XQContourGroup(已核实 .h:43)→ intent 整体按值捕获进 job,天然安全,无需额外保活。
- mesh 页:prepare 内 `modelOf(scene_, ...)` 返回的 model 指针指向 payload——payload 由 scene 节点 shared_ptr 持有,busy 期间 scene 写全禁(含右键删除)→ 节点不会消失,安全;**无需改 MeshingController 签名**。
- flow 页:`SolveIntent` 按值持 case/mesh/solverInput(已核实 .h:35-42)→ intent 按值捕获,安全。
- **同步回退路径**:`asyncRunner` 为空时不能再调旧的 `controller->threshold(...)`(intent 已被 job 捕获)。统一写法 = `job()` + `commitPrepared`(Commit 1 已加)+ `done`,与异步路径共享全部逻辑——**test_workflow_controllers/test_workflow_integration 走 controller 同步方法,零改动**;headless 页面测试走此回退,行为不变。message 文案规则(写死):model/mesh/flow 三页的 job 里 `outcome.message = text(prepared.status);`(复用既有映射函数,prepare 失败时 status 与旧同步返回值同款枚举,text() 零改动);seg 页保持它现有的专用文案(如上例)。

### 4.3 窗口接线(XQMainWindow)

头文件:
- include 或前向声明后,成员区加:
  ```cpp
      XQTaskRunner taskRunner_;
      bool workflowBusy_ = false;
  ```
  (XQTaskRunner 按值持有——生命周期同窗口,析构自动 join。头文件需 `#include "app/XQTaskRunner.h"`。)
- private 方法声明:`void setWorkflowBusy(bool busy, const QString& label);`。

XQMainWindow.cpp:
- **构造函数**连接忙态:
  ```cpp
      QObject::connect(&taskRunner_, &XQTaskRunner::taskStarted, this,
                       [this](const QString& label) { setWorkflowBusy(true, label); });
      QObject::connect(&taskRunner_, &XQTaskRunner::taskFinished, this,
                       [this](const QString& label) { setWorkflowBusy(false, label); });
  ```
- **buildStagePanel** 里组装 asyncRunner 并作为第 13 参传给 populateStagePanels:
  ```cpp
      AsyncCommandRunner asyncRunner = [this](const QString& label, StageCommandJob job,
                                              std::function<void(bool, const QString&)> done) {
          return taskRunner_.run(
              label,
              [job = std::move(job)]() -> std::shared_ptr<void> {
                  return std::make_shared<StageCommandOutcome>(job());
              },
              [this, done = std::move(done)](std::shared_ptr<void> raw) {
                  auto outcome = std::static_pointer_cast<StageCommandOutcome>(raw);
                  bool ok = false;
                  if (outcome->command != nullptr && commandStack_ != nullptr) {
                      ok = commandStack_->push(std::move(outcome->command));
                  }
                  if (done) {
                      done(ok, outcome->message);
                  }
                  refreshSceneTree();
              });
      };
  ```
- **setWorkflowBusy** 实现(忙态矩阵,S3.5):
  ```cpp
  void XQMainWindow::setWorkflowBusy(bool busy, const QString& label)
  {
      workflowBusy_ = busy;
      if (stagePanel_ != nullptr) {
          stagePanel_->setEnabled(!busy);
      }
      QAction* const actions[] = {undoAction_, redoAction_, tbUndoAction_, tbRedoAction_,
                                  openAction_, saveAction_, tbOpenAction_, tbSaveAction_,
                                  openImageAction_, openSvProjectAction_};
      for (QAction* action : actions) {
          if (action != nullptr) {
              action->setEnabled(!busy);
          }
      }
      if (busy) {
          if (statusPositionLabel_ != nullptr) {
              statusShowingIdle_ = false;
              statusPositionLabel_->setText(tr("Running: %1...").arg(label));
          }
          QApplication::setOverrideCursor(Qt::BusyCursor);
      } else {
          if (statusPositionLabel_ != nullptr) {
              statusShowingIdle_ = true;
              statusPositionLabel_->setText(tr("Ready"));
          }
          QApplication::restoreOverrideCursor();
      }
  }
  ```
- **onSceneContextMenu** 入口加忙态 early-return(唯一漏网 scene 写入口),before(逐字,函数体第一个 if):
  ```cpp
      const QModelIndex viewIndex = sceneTreeView_->indexAt(pos);
      if (!viewIndex.isValid() || workflowScene_ == nullptr || commandStack_ == nullptr) {
          return;
      }
  ```
  after:
  ```cpp
      const QModelIndex viewIndex = sceneTreeView_->indexAt(pos);
      if (workflowBusy_ || !viewIndex.isValid() || workflowScene_ == nullptr
          || commandStack_ == nullptr) {
          return;
      }
  ```
- **B2 的 openWorkspaceFromPath/saveWorkspaceFile** 入口各加(这不是冗余兜底:菜单/工具栏 action 被忙态矩阵禁用,但这两个是公开方法,测试与自动化**直接调用**、不经 action——此 guard 是公开入口的唯一忙态闸门,属于忙态矩阵的组成部分):
  ```cpp
      if (workflowBusy_) {
          return false; // busy contract: no state swap while a task runs
      }
  ```

**打开影像/工程/工作区(loader 三方法)不迁后台——定论**:`loadImageFromPath` / `loadSvProjectFromDirectory` / `openWorkspaceFromPath` **保持同步,不动**。理由:既有测试断言这三个方法的返回值代表加载成败,异步化会破坏该契约。它们与计算任务的互斥由忙态矩阵保证(忙时 open/save/openImage/openSvProject 四组 action 全被禁用,用户无法触发)。AC3 的"打开"项按此语义验收(忙态互斥覆盖,非后台化)。此决策写进 result 文件。**不要自行发挥去异步化 loader。**

- i18n:新 tr 串 `Running: %1...`(窗口)与新 xqTr 串 `Another task is still running.`(页面)进 ts:
  ```bash
  /c/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lupdate.exe \
    src -ts resources/i18n/xq_zh_CN.ts -no-obsolete
  /c/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lrelease.exe \
    resources/i18n/xq_zh_CN.ts -qm resources/i18n/xq_zh_CN.qm
  ```
  lupdate 后核查 ts diff(`git diff resources/i18n/xq_zh_CN.ts`):新增词条应恰为上述 2 条(补翻:`正在执行:%1…`、`已有任务在运行。`),既有词条不应被删。diff 不符(多增、误删既有词条)→ **停下报告,不许自行修 ts**。

### 4.4 验证

完整构建 → 全量 ctest **65/65**(既有 workflow/页面测试零改动全绿 = 同步回退等价性证据)。

### 4.5 Commit 3

```
feat: 计算类重活迁 XQTaskRunner — AsyncCommandRunner hook + 全局忙态禁用矩阵
```
文件:
- `src/ui/panels/XQStageWidgets.h`
- `src/ui/panels/XQStageWidgets.cpp`
- `src/app/XQMainWindow.h`
- `src/app/XQMainWindow.cpp`
- `resources/i18n/xq_zh_CN.ts`
- `resources/i18n/xq_zh_CN.qm`

---

## 5. Commit 4 — 窗口异步集成测试(先红后绿)

### 5.1 测试(test_main_window.cpp 追加)

在 `workflowWindow.redo();` 断言块之后追加(利用已加载真实 .vti 的 workflowWindow):

```cpp
    // --- async threshold through the installed runner (B3) ---
    // Find the segmentation page's run button and click it; the mask must
    // arrive via the background task, with the busy matrix engaged meanwhile.
    QStackedWidget* stagePanel =
        workflowWindow.findChild<QStackedWidget*>("xqStagePanel");
    if (stagePanel == nullptr) {
        return fail("stage panel exists for async run");
    }
    QWidget* segPage = workflowWindow.findChild<QWidget*>("xqStagePage_Segmentation");
    if (segPage == nullptr) {
        return fail("segmentation page exists");
    }
    QPushButton* segRun = nullptr;
    const QList<QPushButton*> segButtons = segPage->findChildren<QPushButton*>();
    for (QPushButton* button : segButtons) {
        if (button->property("class").toString() == QStringLiteral("primary")) {
            segRun = button;
        }
    }
    if (segRun == nullptr) {
        return fail("segmentation page exposes its primary run button");
    }

    const std::size_t nodesBeforeAsync = count_nodes(workflowScene);
    QAction* undoDuring = workflowWindow.findChild<QAction*>("xqUndoAction");
    if (undoDuring == nullptr) {
        return fail("undo action exists before async run");
    }

    segRun->click();
    // Busy matrix: the undo action must be disabled while the task runs. The
    // task may be fast; poll the disabled state before waiting for completion.
    bool sawBusy = !undoDuring->isEnabled();

    // Pump the loop until the scene grows (mask committed) or 30s pass.
    QElapsedTimer timer;
    timer.start();
    while (count_nodes(workflowScene) == nodesBeforeAsync && timer.elapsed() < 30000) {
        if (!undoDuring->isEnabled()) {
            sawBusy = true;
        }
        QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    if (count_nodes(workflowScene) != nodesBeforeAsync + 1) {
        return fail("async threshold commits one mask node");
    }
    if (!sawBusy) {
        return fail("undo is disabled while the async task runs");
    }
    // Busy state must be fully restored.
    QApplication::processEvents();
    if (!undoDuring->isEnabled()) {
        return fail("undo re-enables after the async task finishes");
    }
```
include 区补 `#include <QElapsedTimer>`、`#include <QPushButton>`、`#include <QList>`。

**先红验证(固定顺序,不可变通)**:本测试块在 **Commit 3 开工之前**写入 test_main_window.cpp(它只用窗口公开物与稳定 objectName,B2 基线下可编译)→ 完整构建 → 跑 `test_main_window` → **红在 "undo is disabled while the async task runs"**(hook 未接,seg 页走同步路径,`sawBusy` 恒 false;注意同步路径下 mask 节点仍会入 scene,所以只有 sawBusy 断言红)→ 留红输出进 result → 再做 Commit 3 → 绿。Commit 4 只提交测试文件本身(测试代码在 Commit 3 前已写好,按本文档 commit 顺序最后单独成提交)。

### 5.2 假绿抽查

把 `setWorkflowBusy(true, ...)` 的禁用名单里 `undoAction_` 一项临时剔除(actions 数组删掉该元素)→ 完整构建 → `test_main_window` **必红**(sawBusy false);还原,构建,绿。

### 5.3 Commit 4

```
test: 窗口异步阈值分割集成 — 后台完成入 scene + 忙态禁用断言
```
文件:
- `tests/app/test_main_window.cpp`

---

## 6. 批次收尾

1. 全部 commit 后删 `build_gui` 全新重建 + 全量 ctest,**65/65 绿**。
2. `git log --oneline -4` 对照;`git status` 干净。
3. 汇报:各 commit hash、5.1 的红输出摘录、2.3/3.5/5.2 假绿抽查输出摘录、「loader 不异步化」决策已写明。**没跑过不写"通过"。**
