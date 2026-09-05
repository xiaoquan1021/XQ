# EXECUTE-B4b — 大文件加载卡顿修复:图像解码后台化 + 渲染同步去重 + 忙态文案

> 活动任务:`07-04-render-arch-rebuild`(P0 插批,用户真机反馈:稍大文件加载明显卡顿)。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD = fa0a630,B4 已提交)。
> 纪律:工具调用必须真正以工具形式发出;完成报告纯文本收尾,不要以工具调用结束回合。
> **已知坑**:①本批改 XQMainWindow.h(Q_OBJECT)→ 最终验证必须 `rm -rf build_gui` 全新构建;②.bat 用绝对路径包 `cmd //c "C:\...\build_gui_wt.bat"`;③LNK1104 先 `taskkill /IM xq_app.exe /F`;④ctest 的 PATH 是 ENVIRONMENT 注入,exe 别直接跑。

## 卡顿根因(已定位,行号以 fa0a630 为准)

1. **`.vti` 解码在 GUI 线程同步跑**,三处:
   - `XQMainWindow::loadImageFromPath`(XQMainWindow.cpp:2016):`VtkImageAdapter::loadVtiWithBuffer` 同步 → useVolume → 种 Image 节点;
   - SV 工程加载图像段(:2168,`loadSvProjectFromDirectory` 尾):同步解码 + useVolume;
   - 选中 Image 节点重载(onSceneSelectionChanged 内 ~:2858):payload 源路径同步解码 + useVolume。
   几百 MB 读盘+解压全程冻结 UI,无任何反馈。
2. **syncRenderScene 全量重建**(:2525-2549):每次场景变化(push/undo/redo/加载/解析落地)`clearNodes()` 后对每个可渲染节点重新 upsert,upsertNode(:927)总是 detach+重建,大 surface 每次重跑 `SurfaceLodBuilder::buildSync` decimation(:1894)——payload 没变也白干。

## 既有机制(直接复用,别造新轮子)

- **pendingParses_ 串行后台解析队列**(XQMainWindow.cpp:2187 `startNextParse`):ParseKind::Model/Contour,taskRunner 双跳(worker 线程只读文件、不碰 scene/widget/QSettings;GUI 线程 commit),kickNext 用 QueuedConnection 防 busy 拒绝。**图像解码作为新 ParseKind 进同一队列**。
- **忙态三件套已存在**:taskRunner taskStarted/taskFinished → `setWorkflowBusy`(:2691):禁用动作 + BusyCursor + 状态栏 "Running: %1..."。异步化后图像加载自动获得,只需给任务起名 tr("Loading image...")。
- `XQRenderScene::removeNode(id)`/`hasNode(id)` 已有(test_render_scene 有断言)。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 修改 | `src/app/XQMainWindow.{h,cpp}` |
| 修改 | `resources/i18n/xq_zh_CN.ts` + lrelease `.qm` |
| 修改 | `tests/app/test_main_window.cpp`、`tests/app/test_app_startup.cpp`(异步化断言适配)|

**不动** visualization/core/services/io/adapters;不动 XQTaskRunner;不动 XQWorkflowSession。

## 步骤 1:图像解码进后台队列

- `PendingParse` 扩 `ParseKind::Image`(struct 在 XQMainWindow.h,先读现状);加字段或伴生标志承载"commit 时要不要种 Image 节点 / 绑定哪个已有 nodeId"。
- `startNextParse` 加 Image 分支:worker 线程 `loadVtiWithBuffer` 进局部 `XQDemoVolume`(只读文件);GUI commit:成功 → `activeImage_ = std::move(...)`、`activeImageNodeId_` 绑定、`useVolume(...)`(GPU 上传留 GUI 线程,VTK 不跨线程)、按需种 Image 节点(走 `session_->commandStack()->push` + 显式 refreshSceneTree,照 :2061 现状);失败 → 状态栏文案(照 model parse 失败先例 :2241)。任务标签 `tr("Loading image...")`(新 i18n 串)。
- 三处调用点改队列:
  1. `loadImageFromPath`:**同步部分只留快速失败检查**(QFileInfo::exists → false + QMessageBox 保持既有"文件不存在"语义),然后入队 + `startNextParse()`,返回 true=已排队。读不出/非法图像的错误改到 commit 报(状态栏)。
  2. SV 工程加载(:2168 段):删同步解码,把 image 任务**插到 pendingParses_ 队首**(图像比模型先出,用户先看到底图),model/contour 照旧排后。
  3. 选中 Image 节点重载(~:2858):同队列;`activeImage_ && id==activeImageNodeId_` 的快路径(内存里已有,直接 useVolume)保留不动。
- 队列被工程切换清空的语义(:2125 `pendingParses_.clear()`)保持。

## 步骤 2:syncRenderScene 增量化(payload 指纹 diff)

- XQMainWindow 加成员 `std::unordered_map<NodeId, const XQPayload*> syncedPayloads_`(渲染端已装配节点的 payload 身份指纹;裸指针只做同一性比较,不解引用)。
- syncRenderScene 改:
  - scene null → `clearNodes()` + `syncedPayloads_.clear()`(现状保留);
  - visit 每个可渲染节点:`payload().get()` 与 syncedPayloads_ 记录相同 **且** `renderScene_->hasNode(id)`(或该 kind 本来就不进 nodes_ 的除外,照现分支)→ **跳过重建**;不同/新见 → 照旧 upsert + 更新指纹;
  - visit 完扫 syncedPayloads_:本轮没访问到的 id → `renderScene_->removeNode(id)` + 移除指纹(替代 clearNodes 的清场职责);
  - **可见性 reconcile(hasVisibilityState 分支)对每个节点照常跑**,不随跳过省略(勾选态与重建无关);
  - 函数头注释里"Full rebuild each sync"段落改写成新契约说明。
- 正确性依据:payload 变更都走 `setPayload`(换 shared_ptr,指针必变——model/contour 解析落地、命令 clone 都是);Opacity/Color/visible 是呈现态,存渲染端,跳过重建反而天然保住。若发现有原地改 payload 内容不换指针的路径(grep `->group()` 等可变引用的写用法),停下报告,别蒙。

## 步骤 3:测试适配 + 新增

先 grep test_main_window/test_app_startup 里 `loadImageFromPath`/`loadSvProjectFromDirectory` 之后立即断言图像态的位置(已知:test_main_window.cpp:266 后紧跟 xqNavAxialSpin enabled 断言、:377 imageWindow 流程)。适配模式**照既有先例**(:461-462 的 10s 等待循环):调用后 `while (!spin->isEnabled() && timer.elapsed() < 10000) processEvents;` 再断言。逐条申报,不许删断言。
新增覆盖:
1. 图像异步落地:loadImageFromPath 返回 true 后等待循环 → spins enabled + window/level 正值(改写自既有断言);
2. 增量 sync:SV 工程加载、模型解析落地后(已有 84542 锚点断言保持绿)——再 push 一个无关命令(如右键删除路径节点或 AddNode)→ 模型仍 hasNode 且**不经重建**(探针:RenderStats/uploadedPointCount 只在首建时计;若无现成探针可断言 hasNode + 节点数,报告说明探针边界);
3. 节点移除:undo 掉 AddNode 后渲染端 hasNode 变 false(增量路径的 removeNode 分支覆盖)。

## 步骤 4:i18n

`Loading image...` → `正在加载图像...`(context xq::XQMainWindow,手工 ts 块,勿跑 lupdate)+ 本批其它新串;lrelease 重生成。

## 步骤 5:验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rm -rf build_gui   # 必须:XQMainWindow.h 改动
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"   # 全量 68 全绿
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\run_xq.bat"        # 冒烟
```

假绿抽查两项(必做,exe 时间戳核对):
1. 篡改 Image commit 分支(解码成功但不调 useVolume)→ 图像等待断言转红 → 还原绿;
2. 篡改增量 diff(恒判"未变"永不 upsert)→ 模型装配断言(84542 锚点/hasNode)转红 → 还原绿。

## 禁做

白名单外文件;不动 visualization 层(增量化全在 app 层做);不加"降级同步路径"兜底;不删既有断言(只做申报过的等待循环适配);payload 原地变更疑点停下报告;不 commit;报告纯文本收尾。

## 完成报告格式

1. 文件清单;2. 全新构建+全量 ctest 总结行原文;3. 两项假绿抽查证据;4. 断言适配申报(逐条);5. i18n;6. 偏离与存疑(尤其:payload 指纹方案遇到的边界、探针不足处)。
