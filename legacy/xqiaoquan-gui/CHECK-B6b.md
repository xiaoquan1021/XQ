# CHECK-B6b — 复核:引导条 + tooltip + 阈值估计 + Ctrl+A/Esc/模式浮层

> 你是 check worker,只复核不扩改。worktree:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`,基线 HEAD = 6809589(B6a),工作树含 B6b 未提交改动(11 文件,+726/−27)。
> 实现简报:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\EXECUTE-B6b.md`。
> 已知(主审已验,非缺陷):全新构建 68/68 绿;lrelease 245 finished/0 unfinished;worker 两项假绿抽查(引导恒写 Step 1→红、estimate 不 setValue→红)有时间戳证据;主审已亲读全部 diff。

## 复核清单(逐项 PASS/FAIL + 证据)

1. **白名单对照**:改动面 = XQStageWidgets.cpp、XQMainWindow.{h,cpp}、XQSliceViewWidget.{h,cpp}、XQMprWidget.{h,cpp}、ts+qm、test_main_window.cpp、test_path_stage.cpp;无白名单外文件;core/services/io/controllers 零改动。
2. **引导条覆盖**:六页都有 updateGuidance 且接到正确触发点(Path:toggle/refreshList/useVolume nudge;Seg:radio/pickSeed;Modeling/Meshing/Flow/AI:combo/done 回调)——亲读确认无轮询/定时器;Meshing/AI 页新增了 makeHintBar。
3. **tooltip 完整性**:对照简报控件清单(Path 4+2 按钮、Seg 6+estimate、Modeling 3、Meshing 4、Flow 2、AI 5)逐个 grep setToolTip,列缺失项(若有)。
4. **Ctrl+A/Esc 语义**:Ctrl+A 仅 PathPoint 模式生效且与 voxelPicked 共用 addPathDraftVoxel(无第二份实现);Esc 经 seedPickingSetter_/pathPickToggleSetter_ 退出且 toggle 复位、浮层清空;test-only 入口(addPathDraftAtCrosshairForTest/exitPickingForTest)与快捷键实现一致(diff 对照)。
5. **浮层**:xqSliceModeHint 三切片格都有;空串 hide、非空 show+定位;进入 seed/path 两种模式文案不同;resize 重定位。
6. **i18n 内容抽验**:ts 新增串(约 46 条)全部有中文翻译且 context 归属正确(面板串在 XQStageWidgets、主窗口串在 xq::XQMainWindow);无死串、无重复 source;文件 UTF-8 无 BOM+LF(file 命令);qm 与 ts 一致(重跑 lrelease 应零 diff)。
7. **假绿抽查(亲手做一项新的,与 worker 两项不重复)**:篡改 setModeHint(非空文本也 hide,即 `modeHint_->hide(); return;` 提前)→ 重编(exe 时间戳核对)→ test_path_stage 浮层断言(`!modeHint->isHidden()`)应转红;还原 → 重编 → 单跑绿。抽查后 git diff 必须只剩 B6b 本身改动。
8. **收尾**:做完第 7 项后全量 ctest 贴总结行(须 68/68);git status 与第 1 项一致。

## 禁做

不修问题(报告裁决);不动白名单外;不 commit;ts 只用 Edit 工具;报告纯文本收尾。

## 报告格式

逐项 PASS/FAIL+证据;抽查红绿证据(FAIL 行原文+exe 时间戳);问题单列;末行总体结论。
