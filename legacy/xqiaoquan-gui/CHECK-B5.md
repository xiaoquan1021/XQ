# CHECK-B5 — 复核收口批(删旧渲染路径 + i18n/TODO 清理)

> 你是 check worker,只复核不扩改。worktree:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`,基线 HEAD = 7097734(B4c),工作树含 B5 未提交改动。
> 实现简报:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\EXECUTE-B5.md`(验收标准以它为准)。
> 已知:主审已把 ts 从 UTF-8 BOM+CRLF 归一化回 UTF-8 无 BOM+LF 并重跑 lrelease(197 finished/0 unfinished),之后全新构建 68/68 绿——这是预期状态,不是缺陷。

## 复核清单(逐项给 PASS/FAIL + 证据)

1. **删除完整性**:`git status`/`git diff --stat` 对照 EXECUTE-B5 白名单;XQMprView.{h,cpp}/XQRenderWidget.{h,cpp} 确已删除;改动面无白名单外的**代码逻辑**变更(xq.qss/XQMprWidget.{h,cpp} 三处是申报过的纯注释改写,亲读 diff 确认零逻辑变化)。
2. **零命中 grep**(大小写敏感):`grep -rn "XQMprView\|XQRenderWidget" src tests CMakeLists.txt resources` 必须零命中。小写 `xqMprView`/`xqRenderWidget` objectName 属测试锚点,保留是正确的,不算命中。
3. **test_i18n_resources 替换语义**:亲读 diff——include/configureDefaultSurfaceFormat 换到 XQVolumeViewWidget(与 main.cpp:36 一致);两段断言换到 `xq::XQMprWidget`/"Axial" 与 `xq::XQVolumeViewWidget`/"Nothing to render",断言口径(非空/非源串/contains)不减;目标串确实在活代码里 tr()(XQMprWidget.cpp:85、XQVolumeViewWidget.cpp:71)。
4. **假绿抽查(亲手重做一次)**:临时改 ts 里 `xq::XQVolumeViewWidget` 的 "Nothing to render" 译文为错串 → lrelease → 重编(注意 qrc 要重编,核对 test_i18n_resources.exe 时间戳真变了)→ 单跑 test_i18n_resources 必须转红;还原 → lrelease → 重编 → 单跑转绿。**注意**:worker 之前抽查的是 Axial 断言,你换另一段(Nothing to render)覆盖;抽查后必须把 ts/qm 完全还原(git diff 只剩 B5 本身的改动)。
5. **ts 内容**:对 HEAD 归一化对比只有纯删除(2 死 context + Parsing model.../Parsing contours... 2 死串),无意外增改;带 %1 的活串(Parsing models (%1).../Parsing contours (%1)...)仍在。
6. **TODO 清零**:`grep -rn "TODO" src/visualization src/app src/ui` 零命中;四处改写读上下文确认语义如实(不是删信息)。
7. **XQSceneRenderer 未动代码**:`git diff src/visualization/XQSceneRenderer.h` 仅注释;无 .cpp 改动;CMake 里三个测试(test_scene_renderer/progressive/surface_lod)仍在。
8. **最终一致性**:如果你做了第 4 项抽查,收尾后重编并跑全量 ctest,贴总结行(须 68/68);核对 git status 与第 1 项一致。

## 禁做

不修任何发现的问题(报告给主审裁决);不动白名单外文件;不 commit;报告纯文本收尾,工具调用必须真正以工具形式发出。

## 报告格式

逐项 PASS/FAIL + 证据(命令输出原文摘录);抽查红绿证据(FAIL 行原文 + exe 时间戳);发现的问题单列;最后一行给总体结论(PASS / FAIL + 阻塞项)。
