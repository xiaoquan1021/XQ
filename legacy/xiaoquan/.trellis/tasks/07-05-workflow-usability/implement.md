# implement.md — 执行计划(批次/纪律/验收)

> 模式:沿用 07-04(主审写 EXECUTE 简报 → channel implement worker 照做 → check worker 复核 → 主审亲读+全新构建复跑 → commit → 真机)。
> worktree:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(基线 ead692c)。
> 构建 `build_gui_wt.bat`,测试 `ctest_merge.bat`(全量 68,基线全绿),.bat 一律 `cmd //c "C:\...绝对路径"`。

## 批次(同 worktree 严格串行,绝不并发写手)

- [x] **B6a — 消灭 NodeId 手填 + addPath 回调修复**(EXECUTE-B6a.md;design §1 §2)✅ 2026-07-05 6809589
  - StagePanelContext 加 nodeLister 枚举 provider;四页 sourceSpin → NodeComboBox(showPopup 重填,objectName 锚点);
  - 右键菜单加「用它建模/用它建网格」(切页+预选);选中自动带入;
  - **§2 执行记**:worker 核对推翻简报前提——五页 done 回调已调 stageChanged(同步回退路径本就恰一次刷新),async 是幂等双刷(cpp:1017 网关自刷+done 再刷)留档不改;PathController::addPath GUI 零消费,仅标注 test-only;
  - **抽查记**:check worker(死于 ECONNRESET)留下的 setCurrentIndex 篡改暴露测试假绿(repopulate 后 Qt 默认选 index 0 掩蔽预选缺失),主审强化断言(park 到末项+跨节点预选)后篡改真转红;三项抽查全闭环,68/68 绿。
- [x] **B6b — 引导条 + tooltip + 阈值估计 + Ctrl+A/Esc/模式浮层**(EXECUTE-B6b.md;design §3~§6)✅ 2026-07-05 21a9f3f
  - 六页 updateGuidance(Path 3 步,余 2 步;Meshing/AI 补 hint bar);27 处 tooltip;Estimate(跨步采样 P25/P75);
  - Ctrl+A 与 voxelPicked 共用 addPathDraftVoxel;Esc 经原 setter 退出;xqSliceModeHint 浮层三格转发;
  - **抽查记**:三项闭环(引导恒 Step 1→红、Estimate 不 setValue→红、setModeHint 提前 hide→红);b6b-check 静态 1-6 项全 PASS 后死于回合中断,遗留篡改由主审跑红+还原+全量复绿;lrelease 245/0。
- [ ] **真机签收(硬门禁)**:用户按界面引导独立走通 路径→分割→建模→网格;无手填 NodeId;右键直达/浮层/Ctrl+A 生效。

## 每批固定纪律(写进 EXECUTE 头部)

1. 改 Q_OBJECT 头(XQMainWindow.h/XQSliceViewWidget.h)→ 最终 `rm -rf build_gui` 全新构建;
2. 新文件/新注释英文(MSVC GBK);ts 手工编辑勿跑 lupdate,注意保持 UTF-8 无 BOM + LF(worker 手编易引入 BOM+CRLF 假 diff,memory 有案);
3. 既有断言只做申报过的等价适配,不删;假绿抽查附 exe 时间戳;
4. 不 commit;报告纯文本收尾;冲突/存疑停下等主审。

## 回滚点

- B6a 后:ead692c → B6a commit(单批可 revert);
- B6b 后:B6a → B6b(互不纠缠:B6a 动数据绑定,B6b 动引导层)。

## 验收命令

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rm -rf build_gui
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"   # 全量绿
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\run_xq.bat"        # 冒烟;真机归用户
```
