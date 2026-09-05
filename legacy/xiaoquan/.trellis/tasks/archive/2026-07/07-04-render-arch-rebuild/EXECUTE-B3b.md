# EXECUTE-B3b — 数据管理器树化:分组树+组勾选+名称列拉伸+建模优先默认可见性

> 活动任务:`07-04-render-arch-rebuild`。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD = B3 提交后)。
> 纪律:工具调用必须真正以工具形式发出;完成报告是纯文本,不要以工具调用收尾。
> **已知坑(B3 实测)**:改 Q_OBJECT 头(XQSceneModel 要大改)后,增量 ninja 的 moc 可能陈旧 → GUI 测试析构 0xC0000409 假崩。**本批构建一律先 `rm -rf build_gui` 全新构建**,不做增量二分。

## 用户需求(已确认)

1. 数据管理器平铺表格 → **两级树**(MITK 风格):类型分组(Images/Paths/Segmentations/Models/Meshes/Simulations),组可折叠、带**三态勾选框**(全选/半选/全不选),组勾选联动整组显隐;
2. **名称显示完全**:勾选框并入名称列行首(树形+勾选同列),名称列 stretch 占满;现 72px 截断("OS...""aor...")必须消失;
3. **默认可见性 = 建模优先**:打开工程后 **Image ✓ + Model(SurfaceModel)✓,其余(Paths/Segmentations/Meshes/Simulations)全不勾**;不勾图像 = 切片灰度图+3D 三平面全隐。

## 源码事实(已核,直接用)

- `XQScene::Group` 枚举 + `static groupForDomain(domain)` 现成(XQScene.h:18-28):Ungrouped/Images/Paths/Segmentations/Models/Meshes/Simulations。
- `XQSceneModel` 现为平铺(node_ids_ 向量);B3 已有:CheckColumn=0 四列、visible_ unordered_map、nodeVisibilityChanged 信号、hasVisibilityState/setNodeVisibleSilently、refresh 保留存活 id 态。
- 树视图经 `sceneFilter_`(QSortFilterProxyModel,FilterKeyColumn=DisplayNameColumn,XQMainWindow.cpp:270)挂 sceneTreeView_;列宽硬编码在 XQMainWindow.cpp:1218-1220(B3 check 开放项:0 列 84px 偏宽——本批列重构一并消掉)。
- syncRenderScene 的可见性 reconcile 在 XQMainWindow.cpp:2497-2504(hasVisibilityState→UI 权威推渲染;首见→渲染端默认回写)。
- `XQRenderScene`:已有 setImagePlanesVisible3d(只控 3D 三平面);**没有**控 2D 切片图像的开关——本批加。
- Image 节点不进 renderScene nodes_(走 setVolume 路径),reconcile 循环碰不到它——图像可见性要单独接线。
- `tests/ui/test_scene_model.cpp` 存在;**test_main_window.cpp:112 有 `model->rowCount(QModelIndex()) == count_nodes(scene)` 平铺假设断言**——树化后必须改(根行数=非空组数)。**先 grep 所有测试里的 rowCount/index( 用法**,逐个适配,不许静默删断言。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 修改 | `src/ui/XQSceneModel.{h,cpp}`(树化主体)|
| 修改 | `src/visualization/XQRenderScene.{h,cpp}`(仅 setImageVisible/imageVisible 探针)|
| 修改 | `src/app/XQMainWindow.{h,cpp}`(树视图配置/图像可见性接线/默认态)|
| 修改 | `resources/i18n/xq_zh_CN.ts` + lrelease 重生成 `.qm` |
| 修改 | `tests/ui/test_scene_model.cpp`、`tests/app/test_main_window.cpp`、`tests/visualization/test_render_scene.cpp`;grep 后若 test_app_startup/test_path_stage 有平铺假设一并适配 |

## 步骤 1:XQSceneModel 树化

- **两级结构**:根 = 非空组(固定序:Images→Paths→Segmentations→Models→Meshes→Simulations→Ungrouped 若有);子 = 组内节点。内部索引方案自定(internalId 编码或 GroupEntry 指针),要求 parent()/index() 闭环正确。
- **列改三列**:`NameColumn=0`(勾选框+图标+全名,CheckStateRole 与 DisplayRole 同列——Qt 标准做法)、`DomainTypeColumn=1`、`StaleColumn=2`,ColumnCount=3。B3 的独立 CheckColumn 取消。**grep 全部枚举消费者跟改**(FilterKeyColumn/列宽/测试)。
- **组行**:DisplayName=本地化组名(tr("Images")→图像 等,ts 补条目);CheckStateRole=聚合三态(子全勾 Checked/全不勾 Unchecked/混合 PartiallyChecked);setData 组勾选 → 组内全部子节点 setData(逐个发 nodeVisibilityChanged)+ 组行 dataChanged;组行 nodeForIndex 返回 null(不可渲染、不驱动 Opacity/Color)。
- **默认可见性按域**:新 id 首见时 `visible_[id] = defaultVisibleForDomain(domain)`:Image/SurfaceModel→true,其余(Path/ContourGroup/SegmentationMask/Mesh/SimulationCase/FlowResult/Unknown)→false。刷新保留存活 id 态不变。
  - 注意:此后所有 id 都有显式态,reconcile 的"首见回写"分支基本不再触发(mesh 渲染端默认隐藏保留为保险,不删)。
- 信号/查询接口(nodeVisibilityChanged/nodeVisible/hasVisibilityState/setNodeVisibleSilently)签名不变。

## 步骤 2:XQRenderScene 图像可见性

头文件追加(仅这两个):
```cpp
// Shows/hides the volume image everywhere: the three 2D slice images and the
// 3D planes (the latter additionally gated by imagePlanesVisible3d). Crosshairs
// are unaffected. Default true; survives setVolume.
void setImageVisible(bool on);
bool imageVisible() const;
```
实现:`imageVisible_` flag;2D slices 可见性 = imageVisible_;3D 平面可见性 = imageVisible_ && imagePlanes3dVisible_(两处 setter 都按此复合计算;setVolume 重建后重应用)。

## 步骤 3:XQMainWindow 接线

- 树视图:`setRecursiveFilteringEnabled(true)`(搜索命中子节点时组保留);header 第 0 列 `Stretch`、1/2 列 ResizeToContents(替掉 :1218-1220 硬编码);refresh 后 `expandAll()`(默认全展开)。
- nodeVisibilityChanged 处理:节点 domain==Image → `renderScene_->setImageVisible(on)`;其余走既有 setNodeVisible;之后 renderAll。
- useVolume 尾:按 sceneModel_ 当前图像节点勾选态应用 setImageVisible(默认 true 时是 no-op)。
- 默认态变化的下游确认:Paths/轮廓默认不勾 → syncRenderScene reconcile 会把它们 setNodeVisible(false)(hasVisibilityState 现在恒 true),3D/切片开局干净只有图像+模型——正是用户要的。

## 步骤 4:i18n

组名 6 条(Images→图像/Paths→路径/Segmentations→分割/Models→模型/Meshes→网格/Simulations→仿真;context 按 tr 所在类)+ 本批其它新串;lrelease 重生成 .qm(Externals qt-6.7.0/bin/lrelease.exe);先读 test_i18n_resources 防破坏。

## 步骤 5:测试

1. `test_scene_model`:根行数=非空组数(合成场景放 2 组);组行 checkable+初始聚合态正确;组 setData Unchecked → 全子 Unchecked + 每子发信号;一子勾回 → 组 PartiallyChecked;defaultVisibleForDomain:Image/SurfaceModel 节点首见 Checked、Path/Mesh 节点首见 Unchecked;组行 nodeForIndex==null;列数==3。
2. `test_main_window`:**改写 :112 平铺断言**为"根行数==非空组数 && 各组子行数之和==节点数";SV 工程加载后:Model 节点 Checked、Path/ContourGroup 节点 Unchecked(树内定位:先找组行再找子行);名称列 header Stretch(读 header()->sectionResizeMode(0))。
3. `test_render_scene`:setImageVisible(false) 读回 false;与 setImagePlanesVisible3d 复合(imageVisible=false 时 planes3d 开关不使 3D 平面可见——经探针断言 flag 组合,不做像素);setVolume 重载幸存。
4. 全部离散不变量、副作用在断言外。

## 步骤 6:验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rm -rf build_gui   # 必须:Q_OBJECT 头大改,防陈旧 moc 假崩
cmd //c build_gui_wt.bat && cmd //c ctest_merge.bat   # 全量必须全绿
cmd //c run_xq.bat   # 冒烟(先 taskkill xq_app 防 LNK1104)
```
假绿抽查(必做,附证据+exe 时间戳):①篡改组聚合(恒 Checked)→ 三态断言转红→还原绿;②篡改 defaultVisibleForDomain(恒 true)→ 默认态断言转红→还原绿。

## 禁做

白名单外文件;不动 core/services/io/adapters;不做可见性持久化;既有断言只适配不删除(平铺→树的语义等价改写要在报告申报);不 commit;报告纯文本收尾;冲突停下等裁决。

## 完成报告格式

1. 文件清单;2. 构建+ctest 总结行原文(全新构建);3. 两项假绿抽查证据;4. 平铺断言改写申报;5. i18n 条目;6. 偏离与存疑。
