# design.md — GUI v2 + 大规模内存优化 技术方案(决策已拍死,已对源码逐项核实)

> **⚠ 2026-07-04 已被取代(superseded)**:本任务六批执行完、ctest 全绿,但真机验收失败——
> 渲染架构级错误(离屏渲染贴 QLabel + 只渲染选中单节点),性能与功能均不达标。
> 可视化层由 `07-04-render-arch-rebuild` 推倒重写。本文档及 EXECUTE-B1~B6 仅供历史参考,
> **不得作为任何后续执行依据**。保留成果:工作区打开/保存、XQTaskRunner+忙态、预算传导、R0/R6 算法优化。

> 所有行号基于 `feat/gui-v2`(合并提交 5891c07)工作树,写 EXECUTE 文档时须 grep 复核(行号会漂移,符号为准)。
> 分层铁律不破:services/core/controllers 零 Qt/VTK(xq_controllers 纯 C++,CMake ~:251);Source 1.0 签名冻结;新增公共接口只加不改。
> 本文所有「已核实」标注 = 主审对真实源码 Read/Grep 过,不是凭记忆。

---

## S0 前置修复:XQCommandStack::redo 失败丢命令

**现状**(已核实 `src/core/command/XQCommandStack.cpp:45-58`):`redo()` 先 `pop_back`,`execute()` 失败直接 `return false`——命令已弹出又没进 undo 栈,**永久丢失**。

**修复**:失败时放回 redo 栈顶再返回 false:

```cpp
bool XQCommandStack::redo()
{
    if (redo_stack_.empty()) {
        return false;
    }

    std::unique_ptr<XQCommand> command = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    if (!command->execute()) {
        redo_stack_.push_back(std::move(command));  // keep it; caller may retry
        return false;
    }
    undo_stack_.push_back(std::move(command));
    return true;
}
```

**测试**(加进已有 `tests/core/test_command_stack.cpp`,先红后绿):FlakyCommand(第 1 次 execute 成功,undo 后第 2 次 execute 返回 false,第 3 次成功)。push→undo→redo(返回 false)→**断言 `can_redo()==true`**(当前实现此处红)→再 redo(true)→断言效果在。

---

## S1 无用功能清零 + 打开/保存工作区

### S1a 删除清单(每项:声明+实现+调用点+测试+CMake 全删)

| # | 目标 | 动作 + 已核实的连带影响 |
|---|---|---|
| 1 | `makeDemoVolume()` | 删 `XQDemoVolume.cpp` 里该函数与 .h 声明。**struct XQDemoVolume 保留**(`activeImage_` 容器,XQMainWindow.cpp:1494/1620/1902 在用),文件顶注释改为 "decoded volume + buffer holder"。 |
| 2 | `XQImageViewer` 整类 | 删 `src/visualization/XQImageViewer.{h,cpp}`、`tests/visualization/test_image_viewer.cpp`;CMake 删 xq_visualization 源两行 + test_image_viewer 的 add_executable/add_test/PATH env 名单。**连带(已核实)**:`XQMainWindow.h:5` include 该头、`:68 ImageRenderResult showImage(...)`、`.h:179 XQImageViewer imageViewer_` 成员;`XQSceneRenderer.h:20/:71` 两处注释提到它(注释改写,代码不动)。`ImageRenderResult` 类型随头文件一起消亡(除 showImage 外无别的使用者,已 grep 核实)。 |
| 3 | `showImage`/`lastRgbaByteCount`/`imageLabel_` | 删 XQMainWindow.cpp:355-373 方法体、`lastRgba_` 成员、`imageLabel_`(:195/:250-253/:264/:357-372,已核实全部使用点)与 `kDefaultRenderWidth/Height` 常量(若仅剩 showImage 使用;`imageLabel_->setMinimumSize` 用了它,一起删;**先 grep 确认无其他用途再删,有则保留常量**)。`centralStack_` 保留(只剩 mprView_ 一页,对象名与既有断言不动)。**测试连带**:①`test_main_window.cpp:146-154`(showImage rgba 断言块)整段删;②`test_app_startup.cpp` 惰性选中用例:imageLabel 翻页步骤删除,断言改为——`stack->count()==1`、`stack->currentWidget()==window.findChild<QWidget*>("xqMprView")` 的宿主页、`currentWidget()->isAncestorOf(renderWidget)`、选中 Lazy Surface/Lazy Mesh 后 `QApplication::processEvents()` 不崩且 renderWidget 仍可见。 |
| 4 | Data Manager 装饰控件 | 删 `opacitySlider_`/`colorButton_`/`propertiesToggle_`(构建代码 :855-883 附近 + 头成员)。**测试连带(已核实 test_main_window.cpp:235-240)**:现断言 opacitySlider 与 timeSlider「存在且 disabled」→ 改为断言两者 `findChild == nullptr`(诚实缺席)。 |
| 5 | 工具栏 3D Seg 按钮 | 删 `tbSeg3dAction_`(:824-827 附近 + 头成员)。agent 先 `rg xqToolbarSeg3dAction` 全仓,若测试引用一并删断言。2D Seg 按钮文案改「分割」(ts 同步)。 |
| 6 | Time 滑块 + Loc spin | 删 `timeSlider_`(:983-996 + 头 :186)与 3×locSpin(:914-923 附近);连带测试见 #4(同一断言块)。 |

验收 rg(零命中,XQDemoVolume struct 本体除外):`makeDemoVolume|XQImageViewer|ImageRenderResult|showImage|lastRgbaByteCount|opacitySlider_|colorButton_|propertiesToggle_|tbSeg3dAction_|timeSlider_`。

### S1b 打开/保存工作区

**已核实的关键事实**:
- `derive_payload_assets`(XQProjectWriter.cpp:916-957)只给 `!node.hasAssetId()` 的节点派生 blob;惰性载入的节点已有 assetId + 空 handle → **换目录保存时 blob 不会重写,新 `<stem>.assets` 里没有那些文件 → 存档坏死**。
- `attachWorkflow`(XQMainWindow.cpp:388-410)**不可重入**:`buildEditMenu`(:1407,无条件 addAction → 重复 Undo/Redo)、`buildStagePanel`(:424,每次 new QStackedWidget 且 :679 每次 new QDockWidget → 第二个 Stages dock)、页面 lambda 捕获旧 controller 裸指针(controllers reset 后悬垂)。

**决策**:

1. **保存** `saveWorkspaceFile(const QString& path)`(带参供测试;无参槽弹 `QFileDialog::getSaveFileName` filter `"XQ 工作区 (*.xqproj)"` 后转发):
   - 若 `state->assetRootDir` 非空 且 目标 assets 目录 ≠ 现 assetRootDir:**先复制被引用 blob** —— `registry.visit_assets` 收集每个 `record.blobs[].second.relPath`,从旧根复制到 `<新stem>.assets/`(`std::filesystem::create_directories` + `copy_file(overwrite_existing)`);任一失败即中止并报错,不写主档。
   - 然后 `XQProjectWriter::save(state->project, path)`;成功后 `workspacePath_ = path`,窗口标题追加文件名;失败 QMessageBox 含 Status 枚举名。
   - `openAction_`/`saveAction_`/`tbOpenAction_`/`tbSaveAction_`(:679/:688/:768/:770)connect 到对应槽。
2. **打开** `openWorkspaceFromPath(const QString& path)`(公开方法供测试;菜单槽弹框转发):
   - 构造 `XQAppStartupConfig{path}` → `initializeAppStartup` 到**新建的 `XQAppStartupState`**;失败弹框、旧状态原样保留、直接 return。
   - 成功:窗口成员 `std::unique_ptr<XQAppStartupState> ownedState_` 接管新 state(**决策:窗口增加此成员**;main.cpp 启动路径的 state 仍归 main 栈上所有,窗口只借——`attachMainWindowWorkflow` 不变;打开动作产生的新 state 归窗口所有,旧 ownedState_ 顺势析构);随后依次:`attachWorkflow(&scene,&stack)` + `attachGeometryResources(&registry, assetRootDir)` + 复位 `activeImage_/activeImageNodeId_/activeSeedValid_/pathDraft/workspacePath_=path` + `mprView_->clearImage()` + 3D renderer clear+render + `refreshSceneTree()`。
3. **attachWorkflow 重入改造**(本项与 2 同批,先做):
   - `buildEditMenu`:`if (undoAction_ != nullptr) return;`(槽读 `commandStack_` 成员,换栈无需重连,已核实 :1418/:1423 connect 到成员函数)。
   - `buildWindowMenu`:已建则 return(dock 指针不变)。
   - `buildStagePanel`:`stageDock_` 已存在 → 新建 stagePanel_ 后 `stageDock_->setWidget(stagePanel_)`,旧 panel `deleteLater()`;不再二次 new QDockWidget。页面 lambda 因此全部重建,拿到新 controller 指针,无悬垂。
   - `applyProductDockLayout`/`retranslateUi` 本就幂等(重复调用无害,已核实为设置类操作)。
4. 测试:
   - `test_app_startup.cpp` 加 roundtrip:建工程(含一个 eager surface payload 节点)→ `XQProjectWriter::save` → `initializeAppStartup` 读回 → 节点数/域/assetRootDir 断言。
   - **save-as 血案回归测试**(先红后绿的"红"来自现状 bug):惰性打开工程 A → `saveWorkspaceFile(B.xqproj)` → 断言 `B.assets/` 下 registry 引用的每个 relPath 文件存在 → 再 `initializeAppStartup(B)` 成功且几何 resolver 可解析(或至少 reader Status::Ok + 节点守恒)。
   - 重入测试:同窗口连续 `openWorkspaceFromPath` 两个不同工程 → 断言 Edit 菜单 action 数不增长、`findChildren<QDockWidget*>` 数量不变、场景树行数=第二工程节点数。

---

## S2 Path 阶段真实化

**已核实**:`XQMprView::seedPicked(int i,int j,int k)` 信号 + 窗口处理(XQMainWindow.cpp:296-307);`XQImageVolume::voxelToWorld(const double[3], double[3]) const`(XQImageVolume.h:105);`buildPathPage`(XQStageWidgets.cpp:220-247)收 `PathController* path, const IdCounter& counter, const StageChangedCallback&` 但全 `(void)`;**IdCounter 机制已存在**(`using IdCounter = std::shared_ptr<NodeId::ValueType>`,:49;`nextId(counter)`,:51;初值 1000,:715)——**Path 页直接用它分配 NodeId,不再另造 nextNodeId()**。

**决策**:

1. 窗口增 `enum class PickMode { None, Seed, PathPoint };` 成员 `pickMode_`。seedPicked 处理 lambda(:296)按 pickMode_ 分流:
   - `Seed`(现行为不变):写 activeSeed_,状态栏 Seed voxel 文案。
   - `PathPoint`:`activeImage_` 为空直接忽略;否则 `voxelToWorld({i,j,k})` → append 到 `pathDraftPoints_`(`std::vector<PathControlPoint>` 成员)→ 通知 Path 页刷新(见 3)。**不写 activeSeed_、不改 seg 状态文案**。
   - 既有 `seedPickingSetter`(:457)进入时设 `pickMode_=Seed`;Path 页的拾取 toggle 设 `PathPoint`;互斥:任一进入即把另一 toggle 置 unchecked(通过窗口信号或直接操作,详见 EXECUTE)。
2. `buildPathPage` 整段重写(去掉三个 `(void)`):
   - 控件:名称 QLineEdit(默认 `path-1` 自增后缀)、「拾取控制点」QPushButton(checkable,objectName `xqPathPickButton`)、QListWidget(objectName `xqPathPointList`,行文案 `#n (x.x, y.y, z.z)`)、「删除选中点」「清空」、spacing QDoubleSpinBox(默认 0.5,0.01~100,后缀 " mm")、「生成路径」run(objectName `xqPathRunButton`,`draft.size()>=2` 才 enable)。
   - 数据流:新增 provider 对 `PathDraftProvider`(读 draft 快照)+ `PathDraftMutator`(append 由窗口 seedPicked 侧做;页面只需 removeAt/clear)+ 拾取 toggle 回调 —— 照 `ActiveSeedProvider/SeedPickingSetter` 既有模式(XQStageWidgets.h:37-60)在头文件加 3 个 `std::function` 别名,`populateStagePanels` 尾部**追加默认空参**(签名纯追加,已核实现签名 :73-85)。
   - run:`PathController::AddPathIntent{ newPathId=nextId(counter), name, sourceImageNode=activeImage 的 nodeId(经 ActiveImageProvider 取,已有), controlPoints=draft, spacing }` → 经 **S3 的 AsyncCommandRunner**(见下)执行;成功后清 draft+列表、status 显示「已创建 <name>」,失败显示映射文案(Rejected→「路径参数被拒绝(点数/间距)」等)。
   - 窗口 draft 变化 → 页面列表刷新:页面构建时把刷新闭包注册给窗口(provider 对的一部分)。
3. i18n:新串全部 `xqTr()`(该文件既有习惯,已核实 :224 等)+ ts 补翻 + lrelease 重新生成 .qm(命令行写进 EXECUTE:`C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lupdate.exe / lrelease.exe`)。
4. 测试 `tests/app/test_path_stage.cpp`(link xq_app_shell,offscreen):加载真实 .vti → 窗口新公开测试方法 `appendPathDraftPointForTest(Point3)`×3(**决策:为可测性公开,命名带 ForTest 后缀**)→ 找 `xqPathRunButton` click → 事件循环等 taskFinished(S3 信号)→ 断言 Path 节点入 scene、undo/redo 对称、draft 清空、列表空。

---

## S3 后台任务化(已按真实 controller/service 签名定形)

**已核实的关键事实**:六个 controller 全部是「service 静态函数产出 `CommandResult{Status, unique_ptr<XQCommand>}` → `stack_->push`」两段式(PathController.cpp:16-31、MeshingController.cpp:43-80、FlowController.cpp:16-44、AiController.cpp:19-44、SegmentationController、ModelingController 同构);**scene 变更只发生在 `push→execute()`**,command 工厂只读输入、构造时不碰 scene 状态(AddNode* 系命令构造仅存指针,已核实 XQSceneCommands.cpp:59-80)。

### S3.1 controller 拆分(纯追加,零 Qt)

每个 controller 增加 `prepare*` 变体:计算 + 构造命令,**不 push**:

```cpp
// PathController.h 追加(其余五个同构):
struct PreparedCommand {
    Status status = Status::Rejected;
    std::unique_ptr<XQCommand> command;   // null unless status == Ok
    bool ok() const { return status == Status::Ok && command != nullptr; }
};
PreparedCommand prepareAddPath(const AddPathIntent& intent);
```

既有同步方法改为 `prepare* + stack_->push`(行为逐字节等价,现测试全绿即证)。六个方法对照表(work 内容 = prepare 体,均已核实签名):

| controller | prepare | 内部调用(全在 worker 安全:纯计算+只读输入) |
|---|---|---|
| Path | prepareAddPath | `PathService::createPathCommand`(PathService.cpp:87) |
| Segmentation | prepareThreshold / prepareRegionGrow | `SegmentationService::thresholdMask/regionGrowMask` + `createMaskNodeCommand`(.h:83/89/111) |
| Modeling | prepareLoft | `ContourLoftInputBuilder::buildLoftInput` + `ModelingService::loftSurface/capModel/createModelNodeCommand`(.h:72/80/92) |
| Meshing | prepareSurfaceMesh / prepareVolumeMesh | `SurfaceMeshService::buildSurfaceMeshCommand`(.h:69)/`VolumeMeshService::buildVolumeMeshCommand`(.h:91,mesher 透传) |
| Flow | prepareSolve | `BoundaryConditionService::validateAndBind` + `FlowSolver1D::solve` + `buildFlowResultCommand`(.h:112) |
| Ai | (不拆,保持同步) | analyzeFlow 是纯数组度量(FlowMetricsService.h:75),秒级,留 UI 线程 |

**⚠️ scene 读取点移出 worker**:MeshingController 的 `modelOf(scene_, intent.modelNode)` 与 AiController 的 `scene_->find` 是 controller 内的 scene 读——`prepare*` 在 worker 跑时,GUI 线程同时只读 scene(渲染),**读读无冲突**;但为绝对安全,**决策:prepare* 入口的 scene 查找照旧保留(读读并发允许),busy 期间禁用一切 scene 写入口(undo/redo/run/open 已禁)即是全局写锁**。EXECUTE 里明写:busy 期间**场景树右键删除也必须禁用**(onSceneContextMenu 入口加 busy 检查——这是唯一漏网的 scene 写入口,已核实 :851)。

### S3.2 输入生命周期(闭包捕获规则,逐类写死)

worker 跑期间 GUI 线程可能经 `onSceneSelectionChanged` Image 分支(:1894-1917)**替换 `activeImage_`** → 借用指针悬垂。规则:

- threshold/regionGrow:job 闭包捕获 `XQImageVolume`(**按值拷**,纯元数据+geometry,轻)+ `std::shared_ptr<XQMemoryImageBufferHandle>`(从 `activeImage_->buffer` **拷 shared_ptr 保活**);intent 里的裸指针字段指向闭包内这两份。
- loft:`LoftIntent` 本就按值持 `XQContourGroup`(已核实 .h:43),直接拷 intent。
- meshing:job 捕获 `std::shared_ptr<const XQTriangleSurfaceGeometryHandle>`(payload 的 `triangleGeometry()` 返回 shared_ptr,拷一份保活)+ `std::vector<ModelFace>` 按值;prepare 变体签名相应收 handle 引用(引用指向闭包持有物)。**mesher_ 指针**:单任务串行保证不并发调用,adapter 无跨调用可变态(TetGen 每次 mesh 独立 tetgenio),安全。
- flow:`SolveIntent` 按值持 mesh/simulationCase/solverInput(已核实 .h:35-42),直接拷。
- open image/SV/workspace:job 里做 `VtkImageAdapter::loadVtiWithBuffer`(VTK 读文件不渲染,worker 安全)/`SvProjectReader::load`/`initializeAppStartup`,产物 heap 持有;commit 在 GUI 线程做入 scene/换 state/刷 UI。

### S3.3 XQTaskRunner(新文件 `src/app/XQTaskRunner.{h,cpp}`,xq_app_shell)

```cpp
class XQTaskRunner : public QObject {
    Q_OBJECT
public:
    explicit XQTaskRunner(QObject* parent = nullptr); // starts the worker thread
    ~XQTaskRunner() override;                          // quit() + wait()
    bool busy() const;
    // work runs on the worker thread; commit(result) runs back on this
    // (GUI) thread. Returns false without running anything while busy.
    bool run(const QString& label,
             std::function<std::shared_ptr<void>()> work,
             std::function<void(std::shared_ptr<void>)> commit);
signals:
    void taskStarted(const QString& label);
    void taskFinished(const QString& label);
private:
    QThread thread_;
    QObject workerContext_;   // moveToThread(&thread_); lambda 的执行锚
    bool busy_ = false;       // GUI 线程读写(run/finish 都在 GUI 线程),无需原子
};
```

实现要点(定死,不给发挥空间):`run` 里 `busy_=true; emit taskStarted;` 然后
`QMetaObject::invokeMethod(&workerContext_, [=]{ auto r = work(); QMetaObject::invokeMethod(this, [=]{ commit(r); busy_=false; emit taskFinished(label); }, Qt::QueuedConnection); }, Qt::QueuedConnection);`
—— 双跳队列投递,**不需要 qRegisterMetaType**(functor invoke,Qt 6 原生支持)。析构 `thread_.quit(); thread_.wait();`。不做取消(粒度分钟内;取消语义留后续任务)。**work 闭包纪律:零 scene 写、零 widget、零渲染**(注释 + EXECUTE 醒目位置)。

### S3.4 页面接线(AsyncCommandRunner hook,签名纯追加)

`XQStageWidgets.h` 追加:

```cpp
struct StageCommandOutcome {
    std::unique_ptr<XQCommand> command;  // null on failure
    QString message;                     // status text for the page label
};
using StageCommandJob = std::function<StageCommandOutcome()>;
// Returns false if a task is already running (page keeps its button enabled state).
using AsyncCommandRunner =
    std::function<bool(const QString& label, StageCommandJob job,
                       std::function<void(bool ok, const QString& message)> done)>;
```

`populateStagePanels` 尾部追加 `AsyncCommandRunner asyncRunner = {}`(与 S2 的三个 path provider 一起,一次改签名)。五个数据页(seg×2/model/mesh×2/flow)的 run 处理器改为:收集 intent(现有代码不动)→ 组 job(闭包内调 `controller->prepare*` + 把 Status 映射成现有中文文案)→ `asyncRunner(label, job, done)`;`done` 里 status label 文本 + 清输入焦点。**asyncRunner 为空时走旧同步路径**(controller 同步方法),保证 headless 页面测试与 workflow controller 测试零改动。
窗口安装 asyncRunner:job 转发给 `taskRunner_.run`,commit 里 `if (outcome.command) ok = commandStack_->push(std::move(outcome.command));` + 调 done + `refreshSceneTree()`。
AI 页不接 hook(同步,见 S3.1 表)。

### S3.5 忙态矩阵(taskStarted/taskFinished 连接,窗口侧)

禁用集合:`stagePanel_->setEnabled(false)`、undo/redo action、open/save/openImage/openSvProject action、场景树右键删除(onSceneContextMenu 入口 early-return)、Path 拾取 toggle。状态栏左段 `正在执行:<label>…`,结束还原 Ready/坐标;`QApplication::setOverrideCursor(Qt::BusyCursor)`/restore。

### S3.6 测试

- `tests/app/test_task_runner.cpp`(QCoreApplication):轻任务 run → 事件循环等 taskFinished → 断言 work 在 worker 线程(`QThread::currentThread() != app 线程`,work 里记录)、commit 在 GUI 线程、busy 序列 true→false、busy 期间二次 run 返回 false。
- `test_main_window.cpp` 增:threshold 走 async 路径(installed runner)→ 事件循环等完成 → mask 节点入 scene;忙态期间 undoAction_->isEnabled()==false。
- 既有 `test_workflow_controllers`/`test_workflow_integration` 零改动(同步路径保留)。

---

## S4 大规模内存 GUI 接通

**已核实**:`GeometryResourceManager::setBudgetBytes`(.h:131)已有;manager 由窗口构造持有(`attachGeometryResources`,XQMainWindow.cpp:411-421,`geometryResources_` unique_ptr 成员 .h:299);`addSurface(handle, LodOptions)`(XQSceneRenderer.h:103-104)与 lazy/progressive 分支(:1937-1977)已在。

1. **预算**:`attachGeometryResources` 构造 manager 后,读 `QSettings("XQ","XQ")` 键 `memory/geometryBudgetMiB`(默认 **2048**,int)→ `setBudgetBytes(std::size_t(MiB) << 20)`。`GeometryResourceManager` 追加只读访问器 `std::size_t budgetBytes() const;`(.h 纯追加,供断言)。`XQPreferencesDialog` 增「几何驻留预算 (MiB)」QSpinBox(256~65536,步 256,objectName `xqPrefGeometryBudget`),Accept 后窗口 applyPreferences 里若 manager 存在再 setBudgetBytes。
2. **LOD 默认开**:匿名 namespace 常量
   `const LodOptions kDefaultLodOptions = [] { LodOptions o; o.enabled = true; o.interactive = true; o.budgetTriangles = 2'000'000; return o; }();`
   `onSceneSelectionChanged` 两处 `addSurface(*handle)`(:1937/:1956)改传 `kDefaultLodOptions`;progressive 分支的 `ChunkUploadSpec.lod` 同配(若结构无该字段则按现签名保持——EXECUTE 时以 XQSceneRenderer.h:113 实签名为准)。渲染器自身默认 `LodOptions{}` 不动(库层测试零影响,已核实测试都显式传参)。
3. **状态栏内存真实化**:私有 `updateMemoryStatus()`:
   ```cpp
   PROCESS_MEMORY_COUNTERS pmc = {};
   GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));   // WorkingSetSize
   ```
   include `<windows.h>` + `<psapi.h>`(**在 .cpp 尾部 include 区,避免 min/max 宏污染:`#define NOMINMAX` 已全局与否 EXECUTE 先查,没有则 include 前局部 define**);CMake `target_link_libraries(xq_app_shell PRIVATE psapi)`。文案 `内存 %1 MB · 几何驻留 %2 MB`(manager 为空省略后半)。QTimer 成员 2000ms + taskFinished 后即刷。删除 :1384 写死行。
4. 测试:窗口 offscreen —— attachGeometryResources 后 `manager->budgetBytes() == QSettings 值`(测试先 QSettings 写 512 再 attach);`statusMemLabel_` 文本匹配 `QRegularExpression("内存 \\d+ MB")`。库层预算驱逐已有 `test_geometry_resource_manager` 覆盖,不重复。

---

## S5 算法热点(services/core,零 Qt)

### S5a 分割 visited 位图
**已核实** SegmentationService.cpp:164(regionGrow)与 :226(largestCC)各 `std::vector<std::uint8_t> visited(voxelCount)`。改 `std::vector<std::uint64_t>`((n+63)/64)+ 文件内 helper(匿名 namespace)`bool test_and_set(std::vector<std::uint64_t>& bits, std::size_t idx)`(返回旧值,置新值;单线程无需原子)。两处调用点语义 = 原「查 visited[i] → continue;否则置 1」逐字对应。512³:134MB→16.8MB×2 处。
测试:`tests/services/segmentation` 现测试全绿即等价;加显式 3×3×3 用例断言重访不重复入队(计数器)。假绿抽查:把 `idx & 63` 篡改成 `idx & 31` → 必红。

### S5b path 复杂度
**已核实** `resample`(XQPath.cpp:209-255):循环体每步调 `point_at_arc_length/tangent_at_arc_length(controlPoints_, cumulative, arcLength)`——这两个 helper 内部从头扫段(O(N)),整体 O(N·M)。`frameAtArcLength`(:256-303):`:275 for i=1..samples` 从头扫,`computeFrames`(CenterlineFrameService.cpp:31-72)对每个 sample 调一次 → O(M²)。

决策(两处独立、纯追加):
- `resample`:helper 增游标重载 `point_at_arc_length(points, cumulative, arcLength, std::size_t* segCursor)`(arcLength 单调递增,cursor 只前进);原无 cursor 重载保留转调(cursor 局部变量)。
- core 增 `XQPath::FrameStatus framesForAllSamples(std::vector<PathFrame>* out) const;`:直接把 `samplePoints_` 各样本的存量 frame(resample 时 `assign_parallel_transport_frames` 已算好,已核实 :251)逐个抄出,O(M);未重采样返回 NotResampled。`CenterlineFrameService::computeFrames` 循环替换为一次调用。`frameAtArcLength` 原样保留(其他调用方 + 对拍用)。
- 测试:`tests/core/test_path.cpp` 加对拍——1000 点路径 resample 后,`framesForAllSamples` 输出与逐点 `frameAtArcLength(sample.arcLength)` 全字段 `fabs ≤ 1e-9` 等价;`tests/services` 的 frame 测试全绿。

### S5c flow 抽帧
**已核实**:录制在 FlowSolver1D.cpp:339-341(`step >= recordStart` 每步 push);测试 FlowSolver1DTest.cpp:210 与 FlowIntegrationTest.cpp:148 断言 `times.size()==numTimeSteps`,且 Flow/Ai 集成测试有 20000 步重试分支(:139/:130)。
**决策(为不动既有断言,默认关)**:`SolverInput` 纯追加 `int maxRecordedFrames = 0;`(**0/负 = 不限 = 旧行为逐字节等价**)。>0 且最后周期步数超限时:`stride = (numTimeSteps + maxRecordedFrames - 1) / maxRecordedFrames`,`(step - recordStart) % stride == 0` 才录,**末步(step == totalSteps-1)恒录**(保证末帧在)。`XQFlowResult` 结构不动(times 与 q/p/a 长度天然一致)。
**GUI 侧**:Flow 页组 SolverInput 时设 `maxRecordedFrames = 2000`(XQStageWidgets.cpp:545-619 组装处)。
测试:`FlowSolver1DTest` 加两用例——`numTimeSteps=10000, maxRecordedFrames=100` → `times.size() <= 101` 且首≈0 末≈period;`maxRecordedFrames=0` → `times.size()==numTimeSteps`(旧行为回归)。既有集成测试**零改动**(默认 0)。`FlowMetricsService::analyzeFlow` 在抽帧结果上出指标:加一个 smoke 断言(analyzeFlow ok)。

---

## S6 视觉打磨(锁定:现浅色 MITK 风格微调,不换肤、不引依赖)

1. **色板 token 归一**(`resources/xq.qss`,794 行,已核实头部注释风格):顶部注释列 token 表——背景 #F5F7FA / 面板 #FFFFFF / 边框 #D5DCE5 / 文字 #1F2937 / 次级 #6B7280 / 强调 #2563EB / 强调淡 #DCEAF9 / 成功 #16A34A / 警示 #DC2626;全文件 grep 每个色值,偏离表内近似色(±1 色阶)统一替换;不改选择器结构。
2. **四视图身份色**:MPR 三格 QLabel 外框 `1px solid` —— Axial #C43C3C / Sagittal #3C9C4A / Coronal #3C6CC4,3D 格 #C4913C;角标签统一半透明深底(rgba(31,41,55,0.72))白字 11px。落点在 XQMprView 构建处(cpp:587-673 一带,objectName 已有 xqMprAxial 等,可用 qss `QLabel#xqMprAxial { border: ... }` 实现,**优先 qss 不改 C++**)。
3. **工具栏**:Open/Save ‖ Undo/Redo ‖ 阶段钮 之间 addSeparator(已有则核对);QToolButton padding 6px、icon 20px;checked 态 #DCEAF9 底 + 1px #2563EB。
4. **状态栏**:段间 `QFrame::VLine`;字号 12px 次级色。Dock 标题 13px 半粗、6px 上下 padding、底 1px 边框。
5. **空场景 3D 提示**:XQRenderWidget 上叠 QLabel「无可渲染节点」(次级色,居中,objectName `xqEmptySceneHint`),窗口在 `onSceneSelectionChanged` 末尾按 `stats.ok` 显隐。
6. ts 补翻 + lrelease;真机 run_xq.bat 目视(用户过目截图)。测试:i18n/glyph 既有测试绿;不写像素断言(假绿)。

---

## 批次与依赖(EXECUTE 按此切;B1、B2 可并行)

| 批 | 内容 | 依赖 |
|---|---|---|
| B1 | S0 + S5a/S5b/S5c(纯 core/services) | 无 |
| B2 | S1a 删除 + attachWorkflow 重入改造 + S1b 打开/保存 | 无 |
| B3 | S3 全部(prepare* 拆分 → TaskRunner → hook → 忙态) | B2(重入改造在先) |
| B4 | S2 Path 真实化 | B3(run 走 asyncRunner) |
| B5 | S4 内存接通 | B2 |
| B6 | S6 视觉 + i18n 收口 + 真机验收 | B1~B5 |

每批固定纪律见 implement.md。假绿抽查(每批 ≥1):B1 撤销 redo 归还行/篡改位移;B2 注释 blob 复制段 → save-as 回归测试必红;B3 把 commit 改在 worker 直跑 → 线程断言红;B5 预算断言改错值红。

## 风险与对策(已核实版)

- **worker 与 GUI 并发**:唯一数据竞争源是 activeImage_ 替换(S3.2 shared_ptr 捕获已消)与 scene 写(忙态矩阵全禁,含右键删除)。渲染读 scene 与 worker 读 scene 是读读并发,安全。
- **attachWorkflow 重入**:S1b#3 三处幂等化;重入测试兜底。
- **save-as 存档坏死**:S1b#1 blob 复制;回归测试先红后绿。
- **抽帧默认关**:既有 20000 步集成断言零触碰;GUI 显式开 2000。
- **lrelease 忘跑**:EXECUTE 给出完整命令行;test_i18n_resources 已断言 .qm 可加载可译,新串漏翻会以英文显形但不红——EXECUTE 要求 agent 对新增 key 抽 2 条加进 i18n 测试断言。
- **Windows 宏污染**:psapi include 处 NOMINMAX 处理写死。
