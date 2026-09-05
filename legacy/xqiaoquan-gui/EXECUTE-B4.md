# EXECUTE-B4 — 主窗口瘦身重接:WorkflowSession 拆分 + StageWidgets 参数打包 + Loc 坐标/Position 状态栏 + 渲染同步统一时机

> 活动任务:`07-04-render-arch-rebuild`(M4)。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD = 66f1635,B3c 已提交)。
> 纪律:工具调用必须真正以工具形式发出;完成报告纯文本收尾,不要以工具调用结束回合。
> **已知坑**:①改 Q_OBJECT 头(XQMainWindow.h 本批必改)后必须 `rm -rf build_gui` 全新构建,防陈旧 moc 假崩;②调 .bat 必须绝对路径包 `cmd //c "C:\...\build_gui_wt.bat"`(相对路径找不到,实测);③LNK1104 先 `taskkill /IM xq_app.exe /F`。

## 批次目标(prd M4 + visual-baseline 5-3/5-8)

1. **拆 XQWorkflowSession**:XQMainWindow(2761 行上帝类)把 scene+stack+六 controllers+volumeMeshKernel+taskRunner 打包进新类;命令栈 push/undo/redo 统一走 session 网关,成功后回调 → 渲染同步时机唯一化;
2. **StageWidgets 参数打包**:populateStagePanels 的 6 controller 指针 + 10 个 std::function 尾参(prd 写 12,实数 10,报告按实数申报)collapse 成一个 context 结构体;
3. **导航器 Loc.(mm) 坐标**(基线 §6,07-03 B2 误删回滚):三个世界坐标 double spin,与切片索引双向联动;
4. **状态栏 Position**(基线 §8):`Position: <x.xx, y.yy, z.zz> mm` 随切片/十字线变化实时更新。

**不做**:删旧文件(XQMprView/XQRenderWidget 归 B5);色阶条(后置);工作区持久化。

## 源码事实(已核,行号以 66f1635 为准)

- `populateStagePanels`:声明 `src/ui/panels/XQStageWidgets.h:110-126`(6 controller + 10 个带默认值的 std::function 尾参);**唯一调用点** `src/app/XQMainWindow.cpp:1006-1022`;实现 `XQStageWidgets.cpp:990`。
- `attachWorkflow` `XQMainWindow.cpp:480-508`:重建 6 controllers + volumeMeshKernel(MMG/TetGen 条件编译)→ buildStagePanel。**析构顺序契约**(XQMainWindow.h 注释):volumeMeshKernel_ 声明在 meshingController_ 之前(controller 先亡);taskRunner_ 声明在**最后**(最先亡,join 时 controllers/scene 还活着)——session 化后此契约必须原样保住。
- `commandStack_` 消费点全集:push 在 `:997`(asyncRunner done 回调,push 后紧跟 refreshSceneTree `:1002`)、`:1997`、`:2095`、`:2365`;undo/redo 在 `:2667-2676`(成功才 refreshSceneTree)。XQCommandStack 是 core 纯类无信号(`src/core/command/XQCommandStack.h`),通知只能由 app 层网关做。
- `refreshSceneTree()`(:2370,内含 expandAll+状态栏计数+syncRenderScene)其余调用点(:690 工程加载、:2001、:2099、:2175、:2208 异步加载落地、:2366、:2668、:2675)——命令路径的收进回调,**非命令路径(工程加载/异步解析落地)保留显式调用**。
- taskRunner 信号接线在构造函数 `:438-440`(connect 一次)——session 若持 taskRunner,session 必须**构造一次**(窗口构造时),attachWorkflow 只 rebind scene/stack/controllers,不重建 session,否则 connect 断。
- 导航器:`buildImageNavigatorDock()` `:1313-1360`,slider/spin 轴序数组 `{sagittal, coronal, axial}` = renderScene axis 0/1/2(x/y/z,LPS);`XQMprWidget::sliceChanged(axis,index)` 信号(XQMprWidget.h:60)已接 `:355-370`。
- 坐标 API 现成(**不改 visualization 层**):`XQRenderScene::sliceWorldCoord(axis)`(XQRenderScene.h:72,当前切片平面世界坐标)、`worldToVoxelIndex(world,&i,&j,&k)`(:78)。Loc 显示 = 三轴 sliceWorldCoord;Loc 编辑 = 组 world[3] → worldToVoxelIndex → 走既有 spin/slider 路径设三轴。
- 状态栏:`statusPositionLabel_`(objectName `xqStatusPosition`,`:1417-1419`)+ `statusShowingIdle_` 标志;忙态/加载消息优先(`:1826-1827` Ready、`:2005`、`:2117` 等)——Position 更新只在 `statusShowingIdle_` 时写,别覆盖忙态消息。
- 测试依赖面:`test_main_window.cpp`/`test_app_startup.cpp`/`test_task_runner.cpp` 引用 XQTaskRunner/xqStagePanel/controllers——先 grep 三个文件确认受影响断言再动。
- CMake:app 源列表 `CMakeLists.txt:183` 附近(新 .cpp 要加);测试注册模式参考 `:850`(test_task_runner)/`:906`(test_main_window)。
- 架构边界:ctest 有 check_arch_boundaries(core/services/io/adapters 规则)。**ui/panels 不得 include app 头**(分层,虽然脚本不查也不许)——context 结构体定义在 XQStageWidgets.h 自己里,不引 XQWorkflowSession。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 新建 | `src/app/XQWorkflowSession.{h,cpp}`(注意:新文件注释写英文,MSVC GBK 坑)|
| 修改 | `src/app/XQMainWindow.{h,cpp}`、`src/app/XQAppStartup.cpp`(若 attach 接线牵连)|
| 修改 | `src/ui/panels/XQStageWidgets.{h,cpp}` |
| 修改 | `CMakeLists.txt`(新源文件;新测试若建)|
| 修改 | `resources/i18n/xq_zh_CN.ts` + lrelease `.qm` |
| 修改 | `tests/app/test_main_window.cpp`、`tests/app/test_app_startup.cpp`;`tests/app/test_workflow_session.cpp`(可选新建,见步骤 5)|

## 步骤 1:XQWorkflowSession(先做,做完先构建一次隔离破坏面)

`src/app/XQWorkflowSession.{h,cpp}`,**普通类不继承 QObject**(回调用 std::function,避免 moc):

```cpp
// Bundles the workflow wiring that XQMainWindow used to hold loose: the
// attached scene + command stack, the six stage controllers (+ mesh kernel),
// and the resident task runner. Owns the single gateway for command-stack
// mutations so every push/undo/redo triggers exactly one scene-changed
// notification (the unified render-sync point).
class XQWorkflowSession {
public:
    XQWorkflowSession();   // constructed once by the window; taskRunner lives here
    // Rebinds scene/stack and rebuilds the controllers (old ones destroyed).
    void attach(XQScene* scene, XQCommandStack* stack);
    // Command gateway: on success runs sceneChanged (set by the window to
    // refreshSceneTree). Returns the stack's result; false when unattached.
    bool pushCommand(std::unique_ptr<XQCommand> command);
    bool undo();
    bool redo();
    void setSceneChangedCallback(std::function<void()> cb);
    // accessors: scene()/commandStack()/pathController()/.../aiController()/taskRunner()
private:
    XQScene* scene_ = nullptr;
    XQCommandStack* stack_ = nullptr;
    std::function<void()> sceneChanged_;
    std::unique_ptr<PathController> path_;
    ... // volumeMeshKernel_ declared BEFORE meshingController_ (destruction order, copy the .h comment)
    XQTaskRunner taskRunner_;  // declared LAST: destroyed first, joins while controllers alive
};
```

- 窗口构造函数里 `session_ = std::make_unique<XQWorkflowSession>()`,taskRunner 的 `:438-440` connect 改 `&session_->taskRunner()`;
- `attachWorkflow` 改为 `session_->attach(scene, stack)` + 原有 buildStagePanel/菜单流程;`workflowScene_`/`commandStack_`/六个 `xxxController_` 成员**删除**,全部走 `session_->` 访问器(公开访问器 `pathController()` 等保留签名,转发);
- 4 个 push 点、undo/redo 改走 `session_->pushCommand/undo/redo`;`:1002` 与 `:2668/:2675` 的显式 refreshSceneTree 删除(回调统一做);回调在构造时 `session_->setSceneChangedCallback([this]{ refreshSceneTree(); })`;
- **此步完成即构建**(增量即可,此步没动 Q_OBJECT 头的话)+ ctest,绿了再进步骤 2。

## 步骤 2:StagePanelContext

`XQStageWidgets.h`:新增聚合结构(纯 ui 层类型,不引 app 头):

```cpp
// Everything a stage page needs from the shell, bundled: the six controllers
// plus the provider/callback hooks the window used to pass as ten trailing
// std::function parameters.
struct StagePanelContext {
    PathController* path = nullptr;
    ... // 6 controllers
    ActiveImageProvider imageProvider;
    ... // 10 hooks, same names/types as the old trailing params
};
void populateStagePanels(QStackedWidget* panel, const StagePanelContext& context);
```

旧 16 参重载**删除**(唯一调用点同步改,不留兼容壳);.cpp 内部各 buildXxxPage 的参数透传方式自定(可原样解包)。XQMainWindow.cpp:1006 调用点改为组装 context(controller 指针从 session_ 取)。

## 步骤 3:导航器 Loc.(mm) 行

`buildImageNavigatorDock()`:三个切片行之后加一行:label `tr("Loc. (mm)")` + 三个 QDoubleSpinBox(objectName `xqNavLocX/xqNavLocY/xqNavLocZ`,2 位小数,无卷时 disabled,加载后 range 按卷世界包围盒);
- 切片变化 → 三个 Loc spin 显示 `renderScene_->sliceWorldCoord(0/1/2)`(blockSignals 防环,照 `:355-370` 现有 slider/spin 同步手法);
- 用户编辑任一 Loc spin → 组 `world[3]`(三 spin 当前值)→ `worldToVoxelIndex` → 命中则把三个切片 spin 设过去(走既有路径联动渲染),越界则钳制回读;
- useVolume 装载后初始化一次;clearVolume/工程切换 disabled。

## 步骤 4:状态栏 Position

切片/十字线变化处(sliceChanged 接线点 + Loc 联动点收口成一个私有 `updatePositionReadout()`):`statusShowingIdle_ == true` 时 `statusPositionLabel_->setText(tr("Position: <%1, %2, %3> mm").arg(x,0,'f',2)...)`。忙态/加载消息不被覆盖(沿用 statusShowingIdle_ 语义;Position 更新本身**不**把 statusShowingIdle_ 置 false——它就是 idle 态的常驻读数)。seed 拾取(:345-349)已写 Seed voxel 文本,保留。

## 步骤 5:测试

先 grep test_main_window/test_app_startup/test_task_runner 里 controllers/xqStagePanel/populateStagePanels 的既有断言,受签名影响的逐个适配(申报)。新增覆盖:
1. `test_main_window`:加载卷后——xqNavLocX/Y/Z 存在且 enabled;改 xqNavAxialSpin 值 → xqNavLocZ 数值变化(double 容差比较);改 xqNavLocZ → axial spin 跟动;`xqStatusPosition` 文本 contains "mm" 且格式含三个数;undo/redo 经 session 后场景树+渲染仍同步(用既有 renderScene hasNode/nodeCount 探针断言 undo 后节点消失、redo 后回来——若已有等价断言则确认仍绿并申报,不重复造)。
2. `test_app_startup`:attachWorkflow 可重入断言沿用(session rebind 不重建 panel 数);
3. 可选 `test_workflow_session.cpp`(若依赖轻可建:pushCommand 成功触发回调恰一次、undo/redo 成功才触发、未 attach 时 pushCommand 返回 false;若 controllers 拖进重依赖导致新 exe 不划算,就并进 test_main_window,报告说明取舍);
4. 全部离散断言、副作用不进 assert。

## 步骤 6:i18n

新串:`Loc. (mm)`、`Position: <%1, %2, %3> mm`(context 按 tr 所在类,XQMainWindow 的 tr 正常走 context `xq::XQMainWindow`,手工加 ts 块照 B2 先例,**不跑 lupdate**)+ 本批其它新串;lrelease 重生成;先读 test_i18n_resources 口径防破坏。

## 步骤 7:验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rm -rf build_gui   # 必须:XQMainWindow.h(Q_OBJECT)大改
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"   # 全量全绿
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\run_xq.bat"        # 冒烟
```

假绿抽查两项(必做,附证据+exe 时间戳核对):
1. 篡改 session::undo 成功后**不调**回调 → undo 渲染同步断言转红 → 还原绿;
2. 篡改 Loc→切片联动(worldToVoxelIndex 结果丢弃)→ Loc 编辑联动断言转红 → 还原绿。

## 禁做

白名单外文件;不动 core/services/io/adapters/visualization;不删旧渲染文件(B5 的活);不留 populateStagePanels 旧签名兼容壳;既有断言只做申报过的语义等价改写;不 commit;报告纯文本收尾;冲突/不确定停下等裁决。

## 完成报告格式

1. 文件清单;2. 步骤 1 中间构建结果 + 最终全新构建+ctest 总结行原文;3. 两项假绿抽查证据;4. 断言适配申报(逐条);5. i18n 条目;6. 架构决策偏离(如 taskRunner 归属、test_workflow_session 取舍)与存疑。
