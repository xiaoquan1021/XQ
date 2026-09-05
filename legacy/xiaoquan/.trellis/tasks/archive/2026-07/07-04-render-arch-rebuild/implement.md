# implement.md — 执行计划(批次 / 验证 / 门禁 / 回滚)

> 任务 `07-04-render-arch-rebuild`。执行前先读 prd.md + design.md + research/ 两份盘点。
> worktree:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(已建,基于 feat/gui-v2 @ a3715e5)。
> 主仓 `.trellis` 是唯一任务文档源;代码只改 worktree。

## 0. 铁律(每批开工前重读)

1. **串行单写手**:所有批次同一 worktree,绝不并发两个写手(memory: gui-v2-batches-serial-worktree)。
2. **每批门禁 = implement → check → 主审亲读关键代码 → 真机**;B1b/B2 批**用户过目通过才进下一批**(prd 硬约束 1)。
3. ctest 全绿只是下限;**流畅度/观感/功能对齐只认真机 `run_xq.bat` + 0007_H_AO_H 工程**。
4. 产品运行时不得触达 renderOffscreenToRgba / 每帧建窗 / 整卷重拷路径(spec: visualization 渲染架构定论)。
5. 新增测试只断言离散不变量,不做像素断言;每个新测试做假绿抽查(篡改被测逻辑必须转红)。
6. 不动 core/services/io/adapters 接口;呈现属性(可见/透明度/颜色)只存 UI 层。

## 1. 构建与验证命令(worktree 内,现成勿另造)

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
cmd //c build_gui_wt.bat      # 完整构建(vcvars64 + CMAKE_PREFIX_PATH;绝不用单目标增量,假绿坑)
cmd //c ctest_merge.bat       # 全量 ctest(offscreen)
cmd //c run_xq.bat            # 真机(自动打开 0007_H_AO_H)
```

已知坑(全部踩过,见 memory):单目标增量链接静默跳过→跑旧 exe;新建 .bat 须 CRLF;ctest ENVIRONMENT PATH 是覆盖式快照;Qt 静态库 qrc 需 Q_INIT_RESOURCE(已做,别删)。

## 2. 批次

### B1a — XQRenderScene 渲染内核(xq_visualization,无 Qt)
**产出**:`src/visualization/XQRenderScene.{h,cpp}`(设计 §2.1 接口);payload→actor 构建器从 XQSceneRenderer.cpp 抄 mapper/property 设置(addPath/addSurface(+LOD)/addSurfaceProgressive/addVolumeMesh(+Progressive)/addFlowResult/addSegmentationMask,连通性 bounds gate 一并抄);体数据常驻 vtkImageData 构建(apply_geometry 抄旧 build_image,连续 buffer 走 memcpy);4 renderer + 切片 mapper + 十字线 + 3D 三平面。
**不做**:任何 Qt/widget;不动旧 XQSceneRenderer。
**测试**:新增 `test_render_scene`(链 xq_visualization,QT_QPA offscreen 不需要——纯 VTK 离屏探针):
- setVolume 后 sliceCount 三轴正确、setSliceIndex 探针读回==idx、越界夹紧;
- upsertNode(surface/path/mesh/mask/flow)→ actorCount(Volume3D) 增长、hasNode true;重复 upsert 同 id 不翻倍;
- setNodeVisible(false)→nodeVisible false;removeNode→actorCount 回落;
- LOD/渐进:uploadedPointCount 契约、chunk 计数、无效连通性 ok=false 不加 actor(对齐旧测试同名断言);
- setWindowLevel/crosshair 读回。
**验收**:完整构建 + 全量 ctest 绿 + 假绿抽查(篡改 setNodeVisible 内部→测试转红)。

### B1b — 三个新部件 + 主窗口最小接线(M1 完成)
**产出**:`XQSliceViewWidget`/`XQVolumeViewWidget`/`XQMprWidget`(设计 §2.2,B1b 只做骨架:挂 renderer、滚轮切片、窗格配色框、角标;交互大头留 B2);XQMainWindow:去掉 onSceneSelectionChanged 渲染段(:2104-2237),接 syncRenderScene() + setVolume;导航滑块接 setSliceIndex。旧 XQMprView/XQRenderWidget 暂不删(M5),但主窗口不再实例化。
**前置**:先读 test_main_window/test_app_startup 引用的 objectName 清单,新部件沿用。
**验收**:完整构建 + 全量 ctest(旧结构断言按清单迁移)+ **真机:用户过目**——四格有图(三切片正确 reslice + 3D 合成可见节点)、滑块流畅、Sagittal/Coronal 不黑、Axial 比例正常。**用户不点头不开 B2。**

### B2a — 用户真机反馈批:可拖彩色十字线 + 模型加载即出(2026-07-04 M1 验收反馈,优先)
**背景**:M1 真机过目通过("明显进步"),用户两条新硬需求:①派生数据(路径-分割-建模)加载即后台处理好,血管表面模型自动出现在 3D 前台;②十字线由黄色固定线改为轴配色(红/绿/蓝对应 Axial/Sagittal/Coronal)、光标可拖(单线动单轴、交点动两轴)、拖动与导航器滑块/数值双向联动。
**产出**:XQRenderScene 十字线按代表轴配色 + worldToIndex 探针;XQSliceViewWidget 十字线拖拽交互(命中→抓线→拖动→sliceChanged,光标形状反馈);XQMainWindow 工程加载后 taskRunner 后台解析 .mdl(MDLModelReader)→ commit 换 payload → syncRenderScene 自动显示。.ctgr 解析与切片叠加仍归 B3。
**验收**:构建+ctest+真机:3D 打开即见血管模型;十字线三色正确、可拖、数值联动。**用户过目。**

### B2b — 四视图交互余项(M2 收口)
**产出**:拾取路由恢复(voxelPicked→pickMode_ Seed/PathPoint 互斥,参考旧 XQMainWindow.cpp:310-330 + PathPageHooks;恢复 B1b 弱化的 seed-pick 行为断言)、窗宽窗位数值框、缩放/平移细调。色阶条后置(与用户确认优先级,visual-baseline 遗留项)。
**验收**:构建+ctest+真机:缩放/平移/窗宽窗位全部可用且流畅,seed 拾取在分割页可用。**用户过目。**

### B3 — 可见性场景树 + 切片叠加(M3)
**产出**:XQSceneModel 加 CheckStateRole 勾选列(NodeId→bool,默认 true)+ Opacity/Color 控件(07-03 B2 误删回滚)→ setNodeVisible/Opacity/Color;.ctgr 首次需要时经 CTGRContourReader 解析缓存(设计 §3.3);切片轮廓叠加 + 掩膜第二层 vtkImageSlice + surface 截线(可开关);**查清 28 vs 14 节点差异**(SvProjectReader 覆盖面,先查清再改)。
**验收**:构建+ctest+真机:勾选即显隐、切片上有轮廓/分割叠加(对照基线截图)、节点计数不再矛盾。

### B4 — 主窗口瘦身重接(M4)✅ 2026-07-05 fa0a630
**产出**:拆 WorkflowSession/AppState(scene+stack+controllers+taskRunner+providers 打包);XQStageWidgets 12 个 std::function 尾参换 session 对象;导航器 Loc 坐标 spin、状态栏 Position(B2 误删回滚);syncRenderScene 收进命令栈 push/undo/redo 统一时机。
**验收**:构建+ctest+真机回归(六阶段工作流跑通 + undo/redo 渲染同步)。用户过目通过,新反馈:①工具指引性弱、操作复杂;②稍大文件加载卡顿。→ 调研 SV 工作流(research/sv-workflow-comparison.md),定 B4b/B6/新任务路线。

### B4b — 大文件加载卡顿修复(P0,用户 2026-07-05 反馈插批)✅ 2026-07-05 a7431c7
**产出**(research/sv-workflow-comparison.md P0 三项,细化后):
1. `.vti` 图像解码移后台:统一进 pendingParses_ 串行队列(ParseKind::Image),worker 只读文件,GUI 线程 useVolume 提交;三处同步调用点(loadImageFromPath / SV 工程图像段 / 选中 Image 节点重载)全部改走;
2. 渲染同步去重:syncRenderScene 不再 clearNodes 全量重建,改 payload 身份(weak_ptr)指纹 diff——payload 未变的节点跳过重建(消掉每次 push/undo/redo 对全部 surface 重跑 LOD decimation 的卡顿);SurfaceLodBuilder::buildAsync 留作后续(若首建仍卡);
3. 忙态礼仪:异步加载经 taskRunner 自动获得忙态三件套(状态栏 Running + BusyCursor + 动作禁用,已有),补"Loading image..."文案。
**验收**:构建+ctest+真机(打开大工程/大图像界面不冻结,加载有可见反馈;push/undo 不再全场景重建卡顿)。

### B5 — 收口(M5)✅ 2026-07-05 ead692c,用户真机签收通过(visual-baseline 逐项对照)
**产出**:删除 XQMprView/XQRenderWidget(移出 CMake);XQSceneRenderer 标注 test-only;i18n(xqTr 手工加 ts 块,勿跑 lupdate)+ lrelease;忙态矩阵复核;残留 TODO 清零。
**验收**:构建+ctest+真机**逐项对照 visual-baseline.md 验收映射表**,用户签收。
**执行记**:B4c(真机反馈四项:路径细线+切片满窗+十字线中心拖拽+批量解析)7097734 插批先行;B5 删 1244 行(净),test_i18n_resources 断言迁活 context,ts 删 2 死 context+2 死串(197 finished/0 unfinished),假绿抽查双覆盖;忙态复核无漏洞(Loc spins 与 slice sliders 同级同性质判安全);check worker 中途死于回合中断,主审亲手完成其余抽查闭环(红→绿)+全量 68/68 复跑。

### B6 — 工作流可用性(P1+P2,B5 后)
**产出**(research/sv-workflow-comparison.md P1/P2):建模页 contour group 改下拉/复选列表、网格页 model 改下拉(消灭 NodeId 手填);数据管理器右键"用它建模/建网格"直达;hint 升级分步引导条;参数 tooltip 补齐;阈值"估计"按钮;拾取态 Ctrl+A 加点;显式模式指示浮层。
**验收**:构建+ctest+真机(用户按引导能独立走通 路径→分割→建模→网格 链)。

### 新任务(不在本任务内)— 沿路径 reslice 分割
P3:vtkImageReslice 沿 path 法向截面 + 截面轮廓编辑(LevelSet/阈值/圆/椭圆/多边形手画)+ 批量模式,SV Seg2DEdit 同构。**单独立任务出 PRD 与用户确认范围。**

## 3. 每批流程(channel worker)

1. 主会话按本文写该批 EXECUTE 简报(含:活动任务、目标、可改文件白名单、验证命令、禁做事项);简报里的 before/after 引用须先对真实源码核实(memory: executable-spec-must-verify-against-source)。
2. `trellis channel spawn --agent implement` 执行 → 完整构建 + 全量 ctest。
3. `spawn --agent check` 复核(diff 对简报、假绿抽查、坑清单)。
4. **主审亲读关键代码**(不只看 PASS 报告)→ 真机 → 该批 commit。
5. B1b/B2 额外:用户过目,不过目不进下一批。

## 4. 回滚点

- 每批一个 commit(中文,`类型: 描述`),批内不拆散提交;批失败 `git reset --hard` 到上一批 commit。
- B1a/B1b 期间旧路径(XQMprView/XQRenderWidget)保留未删,主窗口一行开关即可切回旧渲染入口应急;B5 删除后不再可切。
- 整任务失败回退:`git switch feat/gui-v2`(worktree),主仓文档保留复盘。

## 5. 完成定义

- visual-baseline.md 缺陷清单 9 项全消(色阶条若与用户确认后置除外);
- 真机流畅度明显优于 MITK 版(用户判定);
- 全量 ctest 绿(Release);check_arch_boundaries 过;
- spec 更新(visualization layer 补 XQRenderScene 契约)+ 任务 archive。
