# CHECK-B6a — 复核:四页 NodeId 手填换节点下拉 + 右键直达 + 选中带入

> 你是 check worker,只复核不扩改。worktree:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`,基线 HEAD = ead692c(B5),工作树含 B6a 未提交改动(8 文件,+390/−40)。
> 实现简报:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\EXECUTE-B6a.md`(验收标准);其 §5 已裁决跳过(核对发现同步回退路径本来就恰一次刷新,async 双刷是幂等冗余留档不改)。
> 已知(主审已做,非缺陷):implement worker 死在验证中途,主审已接管——lrelease(199 finished/0 unfinished)、全新构建、全量 ctest 68/68 绿、两项假绿抽查闭环(枚举恒空→红、去切页→红,均还原绿)。

## 复核清单(逐项 PASS/FAIL + 证据)

1. **白名单对照**:git status/diff --stat 只含简报白名单文件(XQStageWidgets.{h,cpp}、XQMainWindow.{h,cpp}、PathController.{h,cpp} 仅注释、ts+qm、test_main_window.cpp);PathController diff 亲读确认零代码逻辑变更。
2. **四页替换完整性**:XQStageWidgets.cpp 中 QSpinBox 手填 NodeId 已全部消失(grep `idFromSpin`、`sourceSpin` 应零命中;`<QSpinBox>` include 已删),四页 NodeComboBox objectName 齐(xqModelingSourceCombo/xqMeshingSourceCombo/xqFlowSourceCombo/xqAiSourceCombo),domain 绑定正确(ContourGroup/SurfaceModel/SimulationCase/FlowResult)。
3. **NodeComboBox 语义**:repopulate 保留选中(remember currentData → findData 恢复);showPopup 先 repopulate 再弹;idFromCombo 无效返回 NodeId(0)(旧拒绝语义)。亲读实现确认无 Q_OBJECT(不需 moc,只重写 virtual)。
4. **右键直达与选中带入**:onSceneContextMenu 的 domain 分支(ContourGroup→Model from/SurfaceModel→Mesh from,置于 Delete 上方带分隔线);onSceneSelectionChanged 只 preselect 不切页;activateStageFromNode 单一实现被真 action 与 test 入口共享。
5. **测试实质性**:test_main_window 新增 ~100 行断言(枚举数量=场景 ContourGroup 数、userData 有效、文本含节点名、选中带入不切页、Model-from 切页+预选)。检查断言不是恒真:数量断言用 `!=` 精确比、预选断言 parked page 0 防掩蔽。
6. **假绿抽查(亲手做一项新的)**:篡改 `preselectStageSource`(findData 后不 setCurrentIndex)→ 重编(核对 test_main_window.exe 时间戳真变)→ 单跑 test_main_window 应转红(预选/带入断言);还原 → 重编 → 单跑绿。抽查后 git diff 必须只剩 B6a 本身改动。
7. **i18n**:ts diff 应为:2 条右键新串(Model from/Mesh from,context xq::XQMainWindow)+ 6 条 label/hint 改写(XQStageWidgets);文件保持 UTF-8 无 BOM+LF(`file` 命令验证);lrelease 重跑应 199 finished/0 unfinished 且 qm 与 ts 一致。
8. **收尾一致性**:若做了第 6 项,最后全量 ctest 贴总结行(须 68/68);git status 与第 1 项一致。

## 禁做

不修任何发现的问题(报告主审裁决);不动白名单外文件;不 commit;ts 只能用 Edit 工具改;报告纯文本收尾。

## 报告格式

逐项 PASS/FAIL+证据(命令输出摘录);抽查红绿证据(FAIL 行原文+exe 时间戳);问题单列;末行总体结论(PASS/FAIL+阻塞项)。
