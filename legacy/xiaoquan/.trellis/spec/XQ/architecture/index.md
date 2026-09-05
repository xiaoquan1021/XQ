# XQ 架构规范(architecture layer)

> 来源:`plan/README.md`、`plan/00-architecture.md`、`plan/09-dependencies.md`。
> 这是 XQ 血管影像建模 + 血流分析桌面应用的**长期稳定架构约束**。写任何 XQ 代码前先读本层。

---

## 三条铁律(不可违背)

1. **库优先**:每条主线能力(path / segmentation / modeling / meshing / flow / ai)都是
   无 UI、无框架依赖、可单测、可被 CLI 或 GUI 复用的纯 C++ 服务库。UI 后挂,可整体替换。
2. **无治理层**:不写 harness / ledger / contract / task-pack。一块能力对应一份文档,改完跑测试。
3. **外部库当 kernel,不当架构**:VTK / ITK / GDCM / ONNX Runtime 只出现在 adapter / service
   的私有实现里;公开业务 API 只暴露 XQ 自有类型(`XQProject` / `XQScene` / `XQDataNode` / 各 payload)。

## 分层与依赖方向

```
app/        Qt 精简桌面壳(主窗口 / 项目树 / MPR / 3D / 工具面板)   ← 薄,可整体替换
   │  只调用 services + 读 scene
services/   path · segmentation · modeling · meshing · flow · ai   ← 重心,纯算法/领域逻辑,不依赖 Qt
   │
adapters/   vtk · itk · gdcm · onnx   (kernel 封装,不出现在公开 API)
   │
io/         .svproj/.pth/.ctgr/.mdl/.vtp/.vtu 读 + 原生存档
   │
core/       XQProject/Scene/DataNode + 各 payload + 命令栈/undo   ← 零外部依赖
```

- 依赖**只能自上而下**:`app/workbench -> services -> io / adapters -> core`。
- `core` 不得依赖 Qt widgets、VTK renderer、ITK image、GDCM dataset、OCCT shape、MMG mesh、
  ONNX session,也不依赖任何插件注册表或 SWIG 生成对象。
- `services` 是纯算法/领域逻辑,**不依赖 Qt**;需要外部 kernel 时经 `adapters` 调用,公开签名只用 XQ 自有类型。
- `app` 只调用 services、读 scene、把 UI 事件转成 service 输入,**不实现领域算法**。

## 本层其它规范

- [reference-sources.md](./reference-sources.md) — 参考来源规则(SimVascular/MITK 优先,XQ1 全面作废)。
- [external-libs.md](./external-libs.md) — 外部库 kernel 规则 + 依赖基线版本。
- [coordinate-and-units.md](./coordinate-and-units.md) — 坐标系与单位(冻结)、path/contour 语义、变换语义。

> 命令/undo、所有权、source/derived/stale 关系见 `core` layer 的 `command-and-scene.md`;
> 验收原则见 `core` layer 的 `acceptance.md`。

## Scenario: Real app startup boundary

### 1. Scope / Trigger
- Trigger: any change to `xq_app`, `XQMainWindow` startup wiring, command-stack
  ownership at startup, or native project loading from application arguments.
- Purpose: the shipped app must show real project state and workflow controls,
  not a hard-coded demo scene.

### 2. Signatures
- `XQAppStartupConfig parseAppStartupArguments(int argc, char** argv)`
- `XQAppStartupStatus initializeAppStartup(const XQAppStartupConfig& config,
  XQAppStartupState* state)`
- `void attachMainWindowWorkflow(XQMainWindow* window, XQAppStartupState* state)`
- `int runXQApp(int argc, char** argv)`

### 3. Contracts
- No positional project path -> create a fresh `XQProject`, call `open()`, leave
  the scene empty, create a fresh `XQCommandStack`, and attach the main window
  with `XQMainWindow::attachWorkflow(&project.scene(), &commandStack)`.
- First positional project path -> load with `XQProjectReader::load`; the loaded
  project is the displayed project.
- Load failure -> return non-Ok startup status and make `runXQApp` return a
  non-zero code before showing the main window.
- Startup must not insert demo nodes or silently fall back to demo/empty data
  after a path load failure.
- `xq_app` may link `xq_io` privately; `xq_app_shell` must not gain native
  project IO as a persistent dependency.

### 4. Validation & Error Matrix
- no project path -> status `Ok`, project state `Open`, scene node count `0`.
- valid `.xqproj` path -> status `Ok`, loaded node ids are present, demo ids are
  absent.
- missing/unreadable path -> status `ProjectLoadFailed`, state project remains
  `Created`, scene node count `0`, no window is shown by `runXQApp`.
- null startup state -> non-Ok status and no dereference.

### 5. Good/Base/Bad Cases
- Good: `main()` delegates to `runXQApp`; startup helper loads or opens the
  project, then attaches workflow to the same mutable scene displayed in the
  tree.
- Base: no-argument startup is an empty, open project with working controllers
  and undo/redo stack.
- Bad: `main()` calls `project.scene().insert(Demo...)` or constructs
  `XQMainWindow(&project.scene())` without calling `attachWorkflow`.

### 6. Tests Required
- Startup test for no-argument empty project plus non-null workflow controller.
- Startup test that writes a native `.xqproj`, loads it through startup, and
  asserts loaded nodes are visible without demo nodes.
- Startup test for invalid path returning failure without opening a fake project.
- Build `xq_app`, then run `test_app_startup`, `test_main_window`,
  project reader/writer regressions, and full Release `ctest`.

### 7. Wrong vs Correct
#### Wrong
```cpp
xq::XQProject project;
project.open();
project.scene().insert(xq::XQDataNode(xq::NodeId(1), "volume", "Demo Volume"));
xq::XQMainWindow window(&project.scene());
```

#### Correct
```cpp
xq::XQAppStartupState state;
if (xq::initializeAppStartup(config, &state) != xq::XQAppStartupStatus::Ok) {
    return 1;
}
xq::XQMainWindow window;
xq::attachMainWindowWorkflow(&window, &state);
```

## Scenario: Background task threading discipline

### 1. Scope / Trigger
- Trigger: any change to `XQTaskRunner`, the app-shell busy matrix, or any page
  handler that moves heavy work (segmentation, modeling loft, surface/volume
  meshing, flow solve) off the GUI thread.
- Purpose: heavy jobs must not block or corrupt the UI, and only the GUI thread
  may touch the scene, widgets, or VTK.

### 2. Signatures
- `bool XQTaskRunner::run(const QString& label,
  std::function<std::shared_ptr<void>()> work,
  std::function<void(std::shared_ptr<void>)> commit)`
- `bool XQTaskRunner::busy() const`
- Controller split: each heavy controller exposes a `prepare*()` variant that
  runs the service work and builds the command, plus `commitPrepared(...)` that
  pushes it. `prepare*` is worker-safe; the push is GUI-thread-only.
- `void XQMainWindow::setWorkflowBusy(bool busy, const QString& label)`.

### 3. Contracts
- `work()` runs on the resident worker thread: no scene writes, no widget access,
  no rendering, no `QSettings`. It computes over inputs it owns (copied values /
  `shared_ptr` keepalives), never over borrowed pointers that a later GUI action
  could invalidate.
- `commit()` runs back on the GUI thread and is the only place a result touches
  the scene (via the command stack) or the UI.
- Single-task discipline: while `busy()`, a second `run()` is refused (returns
  `false`) and its `work` never runs.
- Busy matrix: while a task runs, every input that could start work or mutate the
  scene is disabled — the six stage run buttons, undo/redo, open/save, and the
  scene context menu. Public entry points that bypass actions
  (`openWorkspaceFromPath`/`saveWorkspaceFile`) also early-return while busy.
- Loaders (`loadImageFromPath` / `loadSvProjectFromDirectory` /
  `openWorkspaceFromPath`) stay synchronous: their return value is the load
  contract; mutual exclusion with compute tasks is guaranteed by the busy matrix,
  not by backgrounding them.

### 4. Validation & Error Matrix
- `run()` while idle -> `work` executes off the GUI thread, `commit` on it, and
  `busy()` is true from `run()` until `taskFinished`.
- `run()` while busy -> returns `false`, second `work` does not run.
- `work` closure touches a scene/widget/VTK object -> architecture violation
  (data race), even if tests pass on a fast machine.
- Async page handler with a borrowed image pointer and no keepalive -> dangling
  read when `activeImage_` is swapped mid-task.

### 5. Good/Base/Bad Cases
- Good: page gathers intent on the GUI thread, `work` runs `prepare*()` over an
  owned copy, `commit` pushes the command and refreshes the tree.
- Base: no-runner headless fallback runs `job()` + `commitPrepared` synchronously
  with identical logic, so controller tests keep passing unchanged.
- Bad: page handler calls `controller->threshold(...)` (compute + push together)
  directly on the GUI thread, or a `work` closure writes to the scene.

### 6. Tests Required
- `test_task_runner`: asserts `work` runs off the GUI thread, `commit` on it,
  busy sequencing, and that a second `run()` while busy is refused.
- `test_main_window`: async stage run commits its node via the background task
  and the busy matrix disables undo while the task runs, then restores it.
- Existing `test_workflow_controllers` / `test_workflow_integration` keep passing
  through the synchronous fallback (behavioral equivalence).
- Final gate: Release full `ctest`.

### 7. Wrong vs Correct
#### Wrong
```cpp
// On the GUI thread: heavy compute + scene mutation block the event loop.
SegmentationController::Status s = segmentation->threshold(intent);
```

#### Correct
```cpp
// work() computes off-thread; commit() pushes on the GUI thread.
taskRunner_.run(label,
    [job] { return std::make_shared<StageCommandOutcome>(job()); },
    [this](std::shared_ptr<void> raw) {
        auto out = std::static_pointer_cast<StageCommandOutcome>(raw);
        if (out->command) { commandStack_->push(std::move(out->command)); }
    });
```