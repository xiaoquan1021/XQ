# EXECUTE-B3c — 数据管理器单列化:删"类型/状态"两列,名称独占全宽

> 活动任务:`07-04-render-arch-rebuild`。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD = edc2cca,B3b 已提交)。
> 纪律:工具调用必须真正以工具形式发出;完成报告是纯文本收尾,不要以工具调用结束回合。
> **已知坑(B3/B3b 实测)**:改 XQSceneModel.h(Q_OBJECT 头)后增量 ninja 的 moc 可能陈旧 → GUI 测试析构 0xC0000409 假崩。**本批构建一律先 `rm -rf build_gui` 全新构建**。

## 用户需求(真机验收 B3b 后确认)

数据管理器树只保留**一列**:勾选框 + 图标 + 完整名称。"类型"和"状态"两列删除——它们抢走窄 dock 的宽度导致名称仍截断("ao...""bt...""00...")。信息不丢失:
- 类型:节点已有域类型图标区分,tooltip 已带 `[类型]` 文字;
- 状态(stale):琥珀色名称标示保留,tooltip 可看;
- 单列后表头无意义,隐藏表头(MITK 风格无表头纯树)。

**不改**:树结构、组三态勾选、默认可见性、渲染接线。本批纯 UI 列裁剪。

## 源码事实(已核,行号以 edc2cca 为准)

- 枚举消费点全集(grep `DomainTypeColumn|StaleColumn|ColumnCount` 已盘):
  - `src/ui/XQSceneModel.h:28-30`(枚举定义)
  - `src/ui/XQSceneModel.cpp:100,159`(边界检查/columnCount)、`206-208`(data DisplayRole 分支)、`359-361`(headerData tr("Type")/tr("Stale"))
  - `tests/app/test_main_window.cpp:473`(ColumnCount)、`477`(sectionResizeMode(NameColumn))、`493`(findNodeByDomain 用 DomainTypeColumn 定位)
  - `tests/ui/test_scene_model.cpp:71,77-80,112-114,307`
- 列宽/表头:`src/app/XQMainWindow.cpp:1222`(setHeaderHidden(false))、`1230-1232`(三列 resize mode)
- tooltip:`XQSceneModel.cpp:228` ToolTipRole 已含 `名称 [类型]`(stale 时追加标记)——保留不动。
- `nodeForIndex(index)` 公开方法可从任意列 index 拿到节点(测试改用它按 domainType 定位)。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 修改 | `src/ui/XQSceneModel.{h,cpp}` |
| 修改 | `src/app/XQMainWindow.cpp`(仅表头/列宽段) |
| 修改 | `resources/i18n/xq_zh_CN.ts` + lrelease 重生成 `.qm`(删 Type/Stale 两条废条目;若 test_i18n_resources 反对删除则保留,报告申报) |
| 修改 | `tests/ui/test_scene_model.cpp`、`tests/app/test_main_window.cpp` |

## 步骤 1:XQSceneModel 单列化

- 枚举:`NameColumn = 0, ColumnCount = 1`,删 DomainTypeColumn/StaleColumn;
- `data()`:删 DisplayRole 的 DomainTypeColumn/StaleColumn 分支;NameColumn 的勾选/图标/琥珀色/tooltip 逻辑**原样保留**;
- `headerData()`:单列后只剩 tr("Name")(表头将隐藏,保留最简实现即可);
- index()/parent()/setData() 的列边界检查沿用 ColumnCount,自动收紧,无需另改。

## 步骤 2:XQMainWindow 表头

`buildDataManagerDock()`(:1222-1232):
- `setHeaderHidden(true)`;
- 列宽段只留 `setSectionResizeMode(0, QHeaderView::Stretch)`,删 1/2 两行(列已不存在,不删会越界警告或无效调用)。

## 步骤 3:测试适配(语义等价改写,不静默删覆盖)

1. `test_scene_model.cpp`:
   - `:71` 列数断言 → `ColumnCount == 1` 且 `columnCount() == 1`;
   - `:77-80` 表头断言 → 只验 NameColumn == "Name",删 Type/Stale 表头断言;
   - `:112-114` 原"类型列/状态列数据匹配"断言 → 改走 `nodeForIndex(nameIdx)`:`node->domain_type()` 与 scene 一致、`scene.is_stale(node->id())` 与 ForegroundRole 琥珀色互证(stale 节点 ForegroundRole 非空,非 stale 为空)——类型/状态覆盖不丢,换承载;
   - `:307` 非名称列拒绝 setData 的断言:列没了,改为"NameColumn 之外的无效列 index 无法构造"(`model.index(row, 1, parent)` 返回 invalid);
   - tooltip 断言(若无则新增):节点 tooltip 包含 domain_type 文字——证明类型信息仍可达。
2. `test_main_window.cpp`:
   - `:473` 列数断言跟改;
   - `:493` findNodeByDomain 改用 `sceneModel->nodeForIndex(nameIdx)->domainType()` 定位(经 QTreeView 的 model 是 proxy,注意 mapToSource;或直接遍历时用 data(ToolTipRole) 含 token 判断——二选一,报告写清选了哪个);
   - 新增:`projectTree->isHeaderHidden() == true`。

## 步骤 4:i18n

headerData 删掉的 tr("Type")/tr("Stale") 在 ts 里成废条目:先读 tests 里 test_i18n_resources 的断言口径,允许删则删两条并 lrelease 重生成 .qm;不允许删则保留条目,报告申报。本批无新增串。

## 步骤 5:验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rm -rf build_gui   # 必须:Q_OBJECT 头改动,防陈旧 moc 假崩
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"   # 注意:必须绝对路径调 .bat(相对路径在 cmd //c 下找不到,B3b 实测)
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"    # 全量必须 67/67 绿
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\run_xq.bat"         # 冒烟(先 taskkill /IM xq_app.exe /F 防 LNK1104)
```

假绿抽查(必做,附证据+exe 时间戳核对):篡改 columnCount() 恒返回 3 → 列数断言转红 → 还原绿。

## 禁做

白名单外文件;不动树结构/勾选/默认可见性/渲染层;既有断言只做申报过的语义等价改写,不静默删;不 commit;报告纯文本收尾;冲突停下等裁决。

## 完成报告格式

1. 文件清单;2. 构建+ctest 总结行原文(全新构建);3. 假绿抽查证据;4. 断言改写申报(逐条:旧断言→新承载);5. i18n 处置;6. 偏离与存疑。
