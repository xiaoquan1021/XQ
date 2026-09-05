# EXECUTE-B5 — 收口(M5):删旧渲染路径 + XQSceneRenderer 标 test-only + i18n/TODO 清理

> 活动任务:`07-04-render-arch-rebuild`(最后一批)。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD = 7097734,B4c 已提交)。
> 纪律:工具调用必须真正以工具形式发出;完成报告纯文本收尾。
> **已知坑**:①本批删 .h/改构建结构 → 最终必须 `rm -rf build_gui` 全新构建;②.bat 绝对路径包 `cmd //c "C:\...\build_gui_wt.bat"`;③LNK1104 先 taskkill xq_app;④ts 手工编辑勿跑 lupdate。

## 批次目标(implement.md B5/M5)

1. **删除 XQMprView + XQRenderWidget**(旧离屏渲染路径,B1 起产品不再实例化);
2. **XQSceneRenderer 标注 test-only**(保留,3 个测试链它);
3. **i18n 收口**:删除死 context,全量 lrelease,复核 0 unfinished;
4. **TODO 清零**:visualization/app/ui 内过期 TODO 处理或改写为事实注释;
5. **忙态矩阵复核**(检查即可,发现问题报告,别扩大改动)。

## 源码事实(已核,行号以 7097734 为准)

- **待删文件**:`src/visualization/XQMprView.{h,cpp}`(905 行)、`src/visualization/XQRenderWidget.{h,cpp}`(129 行)。
- **消费者盘点**(grep 已做):
  - `CMakeLists.txt:153-156`:两对文件在 `xq_render_widget` 库源列表 → 从列表删除(库本身保留,XQSliceViewWidget/XQVolumeViewWidget/XQMprWidget 还在里面);
  - `tests/app/test_i18n_resources.cpp`:**真实代码依赖**——`:2` include XQRenderWidget.h、`:26` 调 `XQRenderWidget::configureDefaultSurfaceFormat()`、`:56-59` 断言 `xq::XQMprView` context 的 "Axial" 译文、`:74-76` 断言 `xq::XQRenderWidget` context 的 "Nothing to render" 译文。改法:include/调用换 `XQVolumeViewWidget.h` + `XQVolumeViewWidget::configureDefaultSurfaceFormat()`(main.cpp:36 同款,即产品真实入口);两段死 context 断言**替换**为活 context 等价断言(如 `xq::XQMprWidget` 的 "Axial"、`xq::XQSceneModel` 的 "Images"——先读 ts 确认活 context 有哪些串,选带中文译文的),断言数量不减,报告申报替换映射;
  - `XQSliceViewWidget.h:32`/`XQSliceViewWidget.cpp:46`/`XQVolumeViewWidget.h:26`:仅注释提及("mirror 旧 XQMprView 命名"),把措辞改成不引用已删类型的事实描述(如 "the corner overlay names predate the rewrite; structural tests key on them");
  - `XQRenderScene.{h,cpp}` 的提及先 grep 确认——若也是注释,同样改写。
- **XQSceneRenderer**:`CMakeLists.txt:125-126` 在 xq_visualization 源列表,测试消费者 test_scene_renderer / test_scene_renderer_progressive / test_surface_lod(:861-875)。**保留编译**;在 `XQSceneRenderer.h` 文件头注释 + CMakeLists.txt:125 上方各加一段 test-only 声明(产品渲染入口是 XQRenderScene;本类仅离屏契约测试用,勿在产品代码引用)。**不改它的代码**。
- **i18n**:ts 里 `xq::XQMprView` 与 `xq::XQRenderWidget` 两个 context(grep 计 2 处 name 命中)在类删除后成死 context → **删除这两个 context 块**;B4c 报告留过两条无引用旧串 `Parsing model...`/`Parsing contours...`(XQMainWindow context 内)→ 一并删;lrelease 重生成,报告贴 finished/unfinished 计数;**先读 test_i18n_resources 改后的断言口径**,确保删的 context 不再被断言。
- **TODO 盘点**(grep 结果,逐条处置):
  - `XQRenderScene.cpp:968`/`1018` `TODO(B3): 勾选框上线后此默认交 UI 管理`——B3 已上线(hasVisibilityState reconcile),TODO 过期:改写为事实注释(默认可见性由 UI 层 reconcile 管理,此处仅首建缺省);**注意这两处是中文注释,改写用英文**(MSVC GBK 坑);
  - `XQRenderScene.cpp:1929` `TODO(B2c): progressive path builds ...`——先读上下文:若描述的是仍未做的事(progressive 路径不走 LOD 单色),改成"当前行为+边界"的事实注释,不留批次号;
  - `XQRenderScene.cpp:2157` `TODO if such data ever appears`——数据边界性说明,保留但去掉 TODO 字样(改 "Note:");
  - app/ui 层 grep 无 TODO(已核)。
- **忙态矩阵复核**(只查不改):对照 `setWorkflowBusy`(XQMainWindow.cpp)禁用的动作清单,检查 B4/B4b 新增的交互入口(Loc spins、批量解析期间的工程切换入口)是否在忙态下被正确禁用/安全。发现漏洞:**报告列出,不自行扩大修改**(主审裁决是否修)。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 删除 | `src/visualization/XQMprView.{h,cpp}`、`src/visualization/XQRenderWidget.{h,cpp}` |
| 修改 | `CMakeLists.txt`(源列表删条目+test-only 注释)|
| 修改 | `src/visualization/XQSceneRenderer.h`(仅文件头注释)|
| 修改 | `src/visualization/XQSliceViewWidget.{h,cpp}`、`src/visualization/XQVolumeViewWidget.h`、`src/visualization/XQRenderScene.{h,cpp}`(仅注释改写)|
| 修改 | `tests/app/test_i18n_resources.cpp` |
| 修改 | `resources/i18n/xq_zh_CN.ts` + lrelease `.qm` |

**不动**:XQMainWindow(除非忙态复核发现主审裁决要修——本批先不改)、core/services/io/adapters、其余测试。

## 步骤

1. grep 双类名全消费者(含注释)→ 按上面盘点逐个处置;删除 4 个文件;CMake 源列表删条目;
2. test_i18n_resources 依赖替换(断言数量不减);
3. XQSceneRenderer.h + CMake test-only 标注;
4. TODO 四处改写(英文);
5. ts 删两个死 context + 两条死串,lrelease;
6. 忙态矩阵复核(只查,报告);
7. 验证:
```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rm -rf build_gui
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"   # 全量 68 全绿
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\run_xq.bat"        # 冒烟
grep -rn "XQMprView\|XQRenderWidget" src tests CMakeLists.txt resources  # 必须零命中(除 git 历史)
```
假绿抽查一项:篡改 test_i18n_resources 替换后的活 context 断言目标串(ts 里临时改错译文)→ 断言转红 → 还原绿(证明替换断言真在校验译文,非恒真)。

## 禁做

白名单外文件;不动 XQSceneRenderer 代码本体;不删 test_scene_renderer 系测试;忙态漏洞不自行修;既有断言只做申报过的等价替换;不 commit;报告纯文本收尾;冲突停下等裁决。

## 完成报告格式

1. 文件清单(含删除);2. 全新构建+全量 ctest 总结行原文+零命中 grep 证据;3. 假绿抽查证据;4. test_i18n_resources 断言替换映射表;5. i18n 计数;6. TODO 处置清单;7. 忙态复核发现(若有);8. 偏离与存疑。
