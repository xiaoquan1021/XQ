# EXECUTE-B3 — 可见性场景树:勾选显隐 + Opacity/Color + 轮廓解析与切片叠加(M3)

> 活动任务:`07-04-render-arch-rebuild`。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD 6f30159 = B2b)。
> 纪律:工具调用必须真正以工具形式发出;完成报告是纯文本,不要以工具调用收尾。

## 目标(四块)

1. 数据管理器**勾选框列**控节点显隐(接 XQRenderScene::setNodeVisible);
2. **Opacity 滑块 + Color 按钮**面板(07-03 B2 误删回滚,接 setNodeOpacity/setNodeColor);
3. **.ctgr 轮廓**后台解析(照 .mdl 先例)+ 3D 轮廓线 + 切片叠加;
4. **28 vs 14 节点差异**查清并写结论(不盲改)。

## 源码事实(已核,直接用)

- `XQSceneModel`(src/ui/):3 列(DisplayName/DomainType/Stale),`node_ids_` 平铺向量,`nodeForIndex` 公开;**无勾选列**——本批加。可见性态存 UI 层(design §3.4 定论,core 不加字段)。
- 树视图经 `sceneFilter_`(QSortFilterProxyModel,XQMainWindow.cpp:263-266)挂 `sceneTreeView_`。
- `CTGRContourReader::read(path, CTGRReadResult*)` 现成(io/project);`CTGRReadResult{ XQContourGroup group; groupName; sourceRelativePath }`;`XQContour{ contourId, pathArcLength, frame, type, points(vector<Point3>), closed }`。
- **core 无 ContourGroupPayload**——本批要新建(照 `XQPathPayload` 模板,core 只加新文件不动旧接口:`XQContourGroupPayload : XQPayload`,持 XQContourGroup,domainType()==ContourGroup,clone() 值拷贝)。
- .mdl 后台解析先例:`pendingModelLoads_`/`startNextModelLoad`(XQMainWindow.cpp:1915-1959,QueuedConnection 链式出队)——**轮廓解析照此模式泛化**(建议改造成一个 pendingParses_ 队列存 {NodeId, path, kind:Mdl|Ctgr},统一 startNextParse;保留现函数名兼容或直接重构,自定,报告说明)。
- `syncRenderScene`(XQMainWindow.cpp:2036-)domainType switch:ContourGroup 现落 default"无 3D 几何"——本批给 XQRenderScene 加 contour 装配后接上。
- 真实工程 0007_H_AO_H 文件账(已数):Images 1 + Paths 5 + Segmentations(.ctgr) 5 + Models(.mdl) 1 + Meshes(.msh) 1 + Simulations(.sjb) 1 = **14 节点,与现 reader 读出数一致**。旧 XQ 的 28 = MITK 把每个数据文件的内部构件(轮廓组内逐轮廓、模型内 face 等)也计为节点。→ **差异不是丢数据,是节点粒度定义不同**。本批只在报告里确认此结论 + 状态栏节点计数改为一致口径(见步骤 5),不改 reader。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 新建 | `src/core/XQContourGroupPayload.h`(header-only,照 XQPathPayload)|
| 修改 | `src/ui/XQSceneModel.{h,cpp}` |
| 修改 | `src/visualization/XQRenderScene.{h,cpp}` |
| 修改 | `src/app/XQMainWindow.{h,cpp}` |
| 修改 | `src/io/project/SvProjectReader.cpp`(**仅当**步骤 5 查实 reader 有可修口径问题;默认不动)|
| 修改 | `resources/i18n/xq_zh_CN.ts` + 重生成 `.qm`(lrelease 在 Externals qt-6.7.0/bin)|
| 修改 | `tests/ui/test_scene_model.cpp`(若存在;先 Glob 确认)、`tests/visualization/test_render_scene.cpp`、`tests/app/test_main_window.cpp` |
| 修改 | `CMakeLists.txt`(仅当新增 core 头需要列入 install/源组时;STATIC 源列表若 header-only 可不动)|

## 步骤 1:XQSceneModel 勾选列

- 列序:**CheckColumn=0(新)**、DisplayName=1、DomainType=2、Stale=3(ColumnCount=4)——勾选框放行首对齐 MITK。**先 grep 现有 Column 枚举的全部消费者**(sceneFilter_ 的 FilterKeyColumn 用 DisplayNameColumn、树列宽设置、测试断言),逐个跟改。
- `QHash<NodeId,bool> visible_`(默认 true);`flags()` CheckColumn 加 `Qt::ItemIsUserCheckable`;`data()` CheckStateRole 返回勾选态;`setData()` CheckStateRole 写 visible_ + emit dataChanged + **新信号 `nodeVisibilityChanged(NodeId,bool)`**;refresh/setScene 保留已知 id 的态(新 id 默认 true)。
- 提供 `bool nodeVisible(NodeId)` 查询。

## 步骤 2:主窗口接线 + Opacity/Color 面板

- connect `sceneModel_::nodeVisibilityChanged` → `renderScene_->setNodeVisible(id,on)` + `mprWidget_->renderAll()`。**mesh 默认隐藏的对齐**:syncRenderScene 里 upsert mesh 后,读 `renderScene_->nodeVisible(id)`(首见 false)回写 `sceneModel_` 的 visible_(加 `setNodeVisibleSilently(id,bool)` 之类不发信号的 setter)——树勾选态与渲染态一致,用户勾上 mesh 即显示。
- 数据管理器 dock(sceneTreeView_ 下方)加一行控件:Opacity QSlider(0-100,objectName `xqOpacitySlider`)+ Color QPushButton(`xqColorButton`,QColorDialog);**作用于当前选中节点**(onSceneSelectionChanged 里使能/回填——选中有可渲染节点才 enable);变化 → `renderScene_->setNodeOpacity/setNodeColor` + renderAll。**注意** test_main_window 有一条旧断言「xqOpacitySlider 不存在」(dead placeholder 断言,grep `xqOpacitySlider`)——该断言随本批功能回归而删除,报告申报。
- i18n:新串(不透明度/颜色/选择颜色 等)进 ts 两侧。

## 步骤 3:轮廓解析 + 渲染

- **XQContourGroupPayload**(core 新头):照 XQPathPayload 模板。
- **后台解析**:loadSvProjectFromDirectory 尾部把 ContourGroup 源节点(XQSourcePayload,.ctgr)也入解析队列;work=CTGRContourReader::read;commit=节点仍存在且仍 XQSourcePayload → setPayload(ContourGroup, XQContourGroupPayload) + refreshSceneTree。与 .mdl 队列统一驱动,busy 单任务纪律不变。
- **XQRenderScene 装配**(upsertNode 加 ContourGroup case + buildNodeActors 分支):
  - 3D:每条 closed contour 一条闭合 polyline(points 已是世界坐标),全组合一个 actor(vtkAppendPolyData 或单 polydata 多 cell,自定);颜色统一亮绿(与模型同色系,kSurfaceColor 亮化 1.2 倍夹紧或直接 (0.3,0.95,0.4)),LineWidth 1.5;RenderStats.pointCount=组内点总数。
  - **切片叠加**:contour 组不做平面截切(轮廓本身是平面曲线)——按**距离筛选**:切片视图 axis 上,轮廓质心与当前切片世界坐标差 < spacing[axis]/2 → 该轮廓在此切片视图显示。实现:每切片视图一个常驻 actor(组内所有"命中"轮廓合成),`setSliceIndex` 时重筛重建该视图 polydata(轮廓数量级小,重建廉价;**不**动体数据/截线的既有机制)。探针:`nodeSliceCutCount` 语义扩展到 contour(命中>0 时报 1)或新增 `nodeContourOverlayCount(id, ViewId)`——选后者,别改旧探针语义。
  - 可见性/opacity/color 全走 NodeEntry 既有机制。
- syncRenderScene 的 ContourGroup case:`dynamic_pointer_cast<XQContourGroupPayload>` 非空 → upsertNode(与其它分支同构)。

## 步骤 4:测试

1. `test_scene_model`(先 Glob tests/ 找到它;没有就在 test_main_window 里做):CheckColumn flags 含 UserCheckable;默认 Checked;setData Unchecked → data 读回 + 信号发射(QSignalSpy 或手动 connect 计数);refresh 后态保留。
2. `test_render_scene`:构造合成 XQContourGroup(两条 closed 轮廓,z=1 与 z=2 平面)→ upsertNode ok、hasNode、3D actor 计入 nodeActorCount;setVolume(8,6,4)后 setSliceIndex(2,1) → `nodeContourOverlayCount(id, Axial)==1`(z=1 命中),setSliceIndex(2,3)→0(都不命中);setNodeVisible(false) 后 nodeVisible false。
3. `test_main_window`:SV 工程加载后泵事件(照 .mdl 断言先例)→ 5 个 ContourGroup 节点 payload 全部变 XQContourGroupPayload 且组内 contours() 非空;「xqOpacitySlider 不存在」旧断言删除 + 新断言:xqOpacitySlider/xqColorButton 存在、选中 surface 节点后 enable;勾选列:model 的 index(row,0) checkState==Checked。
4. i18n:test_i18n_resources 保持绿(先读它,若锚 ColumnCount/表头串需同步)。

## 步骤 5:节点计数口径(28 vs 14 收尾)

- 状态栏右下 `节点:N`(grep `Nodes:\|节点`)与左侧「已加载项目:N 个节点」口径统一为 scene 节点数(14);基线截图里"互相矛盾"(左 14 右 0)的 bug 一并修(右侧计数没接 scene 变化,查 refreshSceneTree 是否更新它)。
- 报告里写明 28 vs 14 结论(粒度定义差异,数据无丢失,文件账 14=14)。

## 步骤 6:验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
cmd //c build_gui_wt.bat && cmd //c ctest_merge.bat   # 全量必须全绿
cmd //c run_xq.bat   # 冒烟不崩;注意先 taskkill xq_app 防 LNK1104 占用
```
假绿抽查(必做,附证据+exe 时间戳):①篡改 setData(不写 visible_)→ 勾选断言转红→还原绿;②篡改轮廓距离筛选(恒不命中)→ nodeContourOverlayCount 断言转红→还原绿。

## 禁做

- 白名单外文件;core 只加新 payload 头不动任何旧接口;不动 services/adapters(CTGRContourReader 已在 io 白名单外——**只调用不修改**)。
- 不做工作区可见性持久化(prd 范围外);不动截线/十字线/拾取既有机制。
- 不 commit;报告纯文本收尾;冲突事实停下等裁决。

## 完成报告格式

1. 文件清单;2. 构建+ctest 总结行原文;3. 两项假绿抽查证据;4. 28vs14 结论 + 旧断言删除申报;5. i18n 条目清单;6. 偏离与存疑。
