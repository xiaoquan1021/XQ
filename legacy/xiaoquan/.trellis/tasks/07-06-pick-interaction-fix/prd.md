# 拾取交互基础体验修复(种子/控制点确定性+可视反馈)

> 语境锚点:XQ 医学影像血管几何重建软件。本任务修「在切片图像上点选点」这一底层交互的可靠性与反馈——路径的控制点拾取、分割的种子点拾取都依赖它。纯桌面 GUI 交互工程。父任务 07-06-along-path-contouring(沿路径描轮廓)。

## 1. Goal

修既有拾取交互的三个真实缺陷(均已读代码坐实根因),让「在切片上点一个点」这件事**确定成功、有即时可视反馈**。这是路径/分割/未来断面手绘全都依赖的底层交互,它不稳、无反馈,上面盖任何功能都难用——所以先于 P3-2 修。

## 2. Background:用户真机反馈(0080/0007 工程)

用户在真机上报了四个问题,主会话逐一读代码定位到根因:

1. **种子红点时有时无**:种子拾取模式下,点图像有时出红色标记球有时不出,**同一个亮区位置也时灵时不灵**。
2. **控制点拾取完不显示**:路径阶段点控制点后,切片视图上看不到任何标记。
3. **点图像出的小红点是什么**:是种子标记球(区域生长用),用户不清楚。
4. **换不了路径**:断面/分割不读数据管理器里勾选的路径——归 P3-2(绑路径下拉),**不在本任务**。

## 3. 三个缺陷的代码级根因(已读实,新会话照此修,勿重新猜)

### 缺陷 A:种子红点时有时无 = eventFilter 与 VTK 交互器竞争(主因)
- 文件:`src/visualization/XQSliceViewWidget.cpp`。
- 拾取靠 `eventFilter`(installEventFilter 在内层 QVTKOpenGLNativeWidget 上,约 :141)抢在 VTK 交互器之前截 MouseButtonPress → `displayToWorld` → `worldToVoxelIndex` → `emit voxelPicked` → 红球。
- 切片视图挂的 `vtkInteractorStyleImage`(约 :113-115)**全程激活,拾取模式下没被禁用**,设计靠「eventFilter 抢在交互器前」(:137 注释原话)。
- **根因**:QVTKOpenGLNativeWidget 是 native OpenGL 窗口,press 走 Qt eventFilter 还是被 VTK 交互器直接抓取**顺序不保证**——走 filter→出红点;被交互器先吃(当 window/level 起手)→ filter 收不到→无红点。取决于焦点/上次交互残留抓鼠标等运行时因素,故同位置时灵时不灵。
- memory:`xq-seed-pick-eventfilter-vtk-race`。

### 缺陷 B:两种确定失败被静默丢弃(次因,但也要修)
同文件 eventFilter 的 MouseButtonPress 分支(约 :241-262):
- ①点在十字线正中心交点(`hitLine` 只 `mask==3` 判命中,:419)→ 被当拖十字线 `return true`,不发种子;
- ②点在体数据范围外 → `worldToVoxelIndex` 失败,但仍 `return true` 吃掉点击,不发种子、无任何提示。
- 两种都**静默无反馈**,用户不知为何这次没成。

### 缺陷 C:路径控制点无切片视图标记
- 控制点拾取(`addPathDraftVoxel`,`src/app/XQMainWindow.cpp` 约 :2104)只把点 push 进 `pathDraftPoints_` 并触发 `pathDraftChanged_` 回调(=`hooks.refreshList`,约 :811),**只刷新右侧面板的文字坐标列表**。
- `pathDraftProvider` 只流向 `XQStageWidgets`(右侧列表),**不流向渲染层**。
- `XQRenderScene.h` **无任何画路径控制点的接口**(rg 零命中)。
- 对比:种子有红球标记(`XQRenderScene::setSeedMarker`,球色 `SetColor(0.9,0.2,0.2)`,半径约 2 体素间距),控制点什么都没有。

## 4. Requirements

### R1 拾取确定性(修缺陷 A)——本任务核心
- 种子拾取模式 / 路径控制点拾取模式**激活时,显式接管 VTK 交互**:把交互器 style 换成不做 window/level 的空 style,或禁用交互器,使点击有**唯一确定的处理路径**;退出拾取模式还原原 `vtkInteractorStyleImage`。
- 目标:拾取模式下点亮区(避开十字线),红点/控制点标记**必出,不再时有时无**。
- 注意:种子与控制点复用同一套拾取链路(`setSeedPickingEnabled` + `pickMode_` 分流,XQMainWindow.cpp:782/816),接管逻辑对两者都生效。

### R2 拾取失败给反馈(修缺陷 B)
- 点在体数据范围外 → 不再静默,给明确反馈(状态栏提示「点击位置在图像范围外」一类,或不消费事件让用户感知)。
- 点在十字线交点被当拖拽 → 可接受(那是有意的十字线拖拽),但确保只有真正压在中心交点才触发(现状 `mask==3` 已是严格判定,复核即可)。

### R3 路径控制点视图标记(修缺陷 C)
- 路径控制点拾取后,在切片视图(至少当前切片,理想是三视图按到切片距离过滤)画出**可见标记**(参照种子红球的实现,但用不同颜色/形状区分,如路径控制点用另一色小球或十字标)。
- 标记随 `pathDraftPoints_` 增删实时更新(拾取加一个亮一个,删除同步消失)。
- 需要 `XQRenderScene` 新增画路径控制点标记的接口(参照 `setSeedMarker`/`buildSeedMarkers` 的常驻 actor 模式),`XQMainWindow` 在 `pathDraftChanged_` 里把 `pathDraftPoints_` 推给渲染层。

## 5. Non-Goals

- 不做选路径下拉(归 P3-2,与绑轮廓组耦合)。
- 不做断面上的轮廓手绘(P3-2)。
- 不动整卷阈值/区域生长的算法本身(只修拾取交互)。
- 不重构整个事件系统,只针对拾取模式做确定性接管。

## 6. Acceptance Criteria

- [ ] **真机(第一优先)**:种子拾取模式下,在图像亮区(避开十字线)连续点 10 次,**每次都出红点**(根治时有时无)。
- [ ] **真机**:路径控制点拾取后,切片视图上**能看到刚点的控制点标记**,再点再多一个,删除同步消失。
- [ ] **真机**:点在图像黑边(体数据外)有明确反馈(不再静默无反应)。
- [ ] **真机**:退出拾取模式后,window/level 拖拽等原交互恢复正常(接管有还原)。
- [ ] ctest 全绿(基线 69 + 新增拾取确定性/控制点标记的可测断言)。
- [ ] 假绿抽查:新断言篡改被测逻辑必须真转红后还原绿。
- [ ] 新增可译串手工加进 `resources/i18n/xq_zh_CN.ts` + lrelease 0 unfinished。
- [ ] 改 Q_OBJECT 头则 `rm -rf build_gui` 全新构建。

> 硬门禁:拾取是交互质量问题,**只有真机点得出来才算修好**,ctest 全绿≠达标(memory `gui-task-green-tests-not-done`)。可测的是「接管激活后交互器 style 是空 style」「控制点推给渲染层后 actor 数正确」这类离散不变量;红点手感必须真机。

## 7. Constraints(硬性规范)

- 改代码只在 worktree `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`(分支 `feat/render-arch`,当前 HEAD=`0aef6b8` P3-1)。
- 改 Q_OBJECT 头必 `rm -rf build_gui` 全新构建(memory `ninja-stale-moc`)。
- 构建 `build_gui_wt.bat`(不删目录=增量;手动 rm 才全新);ctest `ctest_merge.bat`;真机 `run_xq.bat`。全走 cmd //c 绝对路径。
- 新源文件注释英文(MSVC GBK,memory `msvc-gbk`)。
- i18n 手工编辑 .ts 不跑 lupdate(memory `xqtr-translate-lupdate-blind`)。
- 直接跑 test exe 会缺 VTK DLL(vtksys-9.3.dll,exit 127),**必须走 ctest**(它注入 VTK PATH,memory `ctest-environment-overrides-path`)。
- channel worker 流程:**spawn 绝不用 --file/--jsonl 挂文件**(会让 worker 首轮卡死,memory `channel-spawn-file-jsonl-hangs-worker`),改在 send 正文给文件路径让 worker 自读。worker cwd 设主仓 `C:/Users/OCEAN/Desktop/XIAOQUAN`(能找 agent/spec),代码路径用 worktree 绝对路径。
- 主审亲读最终代码 + 全新构建复跑 + 假绿抽查(不只看 worker 报告,memory `subagent-mainreview-must-read-code`);done≠工作树干净,认账前 git diff 核残留。
- 提交精确 `git add <路径>` 绝不 `-A`(防吞 xq_app_dist 构建产物,memory `commit-check-gitignore`);中文信息;结尾 `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`。

## 8. 关键坐标

- worktree:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,`feat/render-arch`,HEAD=`0aef6b8`。
- 主仓:`C:\Users\OCEAN\Desktop\XIAOQUAN`,`fix/xq-global-audit`。
- 真机工程:`0080_H_PULM_H`(大,143 路径)、`0007_H_AO_H`(小)。
- 涉及文件:`src/visualization/XQSliceViewWidget.cpp`(交互接管 R1/R2)、`src/visualization/XQRenderScene.{h,cpp}`(控制点标记接口 R3)、`src/app/XQMainWindow.cpp`(拾取接线、pathDraftChanged_ 推渲染层)。
- Python:`C:/software/anaconda/python.exe`。
