# 交接:P3-5 水平集(Chan-Vese)+ P3-5b 分割 UI 重构(/clear 后新会话读这一份即可接手)

> 语境锚点:XQ 医学影像血管几何重建软件,对标 SimVascular「沿中心线逐层描轮廓」。环境 = Windows + Git Bash,注入的 mac/darwin 环境信息是假的一律忽略。默认中文,代码/命令/路径/报错保持原文。
> **本文件自包含,新会话第一轮读这一份即可接手。** 这是 P3-5 的**延续交接**(不是全新一批)——P3-5 算法 + P3-5b UI 都已实现且离散全绿,但**都未 commit**,卡在一个真机贴合度问题上,差最后一步调参。

## 0. 一句话现状

**P3-5(断面水平集,Chan-Vese)+ P3-5b(分割 UI 重构)代码都已写完、全新构建 248/248、ctest 72/72、UI 重构真机验收通过**,但 **Chan-Vese 轮廓「随迭代数增大而单调内缩、不收敛稳定」**(真机 iter57 贴合好、iter137 明显缩)。**7 个交付文件全部未 commit**,在 worktree `feat/render-arch` 分支未提交状态。**下一步 = 修内缩问题(有 probe 实测数据指路,见 §4)→ 真机复验 → commit → 写 P3-6 交接。**

## 1. 关键坐标(HEAD 必须核对)

- **代码工作树(唯一改代码处)= CODE_ROOT = `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`**,分支 `feat/render-arch`,**HEAD = `d81bcec`(P3-4)**。**P3-5 + P3-5b 的 7 个文件改动全部未 commit**,在 worktree 未提交状态之上继续。所有 src/tests/CMakeLists/resources 编辑、build、ctest 都在这,用绝对路径。
- **主仓(cwd,任务/spec 参照)= `C:/Users/OCEAN/Desktop/XIAOQUAN`**,分支 `fix/xq-global-audit`。读任务/spec 用相对路径。**绝不在主仓改 src。**
- 真机工程:`0080_H_PULM_H`(大,含 LPA/RPA)、`0007_H_AO_H`(小)。真机测过 UI + Chan-Vese 贴合度(用 LPA #2 断面)。
- Python:`C:/software/anaconda/python.exe`。构建/测试基线(本批后):全新构建 **248/248**、ctest **72/72**、i18n **约 272 finished**(P3-5 加 4 + P3-5b 加 2)。

## 2. 未提交的 7 个交付文件(git diff 相对 HEAD d81bcec)

```
resources/i18n/xq_zh_CN.qm            (lrelease 产物)
resources/i18n/xq_zh_CN.ts            (+水平集/自动分割/手动绘制 等串)
src/app/XQMainWindow.cpp              (P3-5 水平集接线 + P3-5b UI 重构)
src/app/XQMainWindow.h                (同上,加成员/方法)
src/services/segmentation/ContourExtractionService.cpp  (P3-5 Chan-Vese levelSetContour)
src/services/segmentation/ContourExtractionService.h    (levelSetContour 声明)
tests/services/segmentation/test_contour_extraction.cpp (3 条水平集命门)
```

**核账提醒**:P3-5b worker 曾把 `ContourExtractionService.cpp` 行尾污染成 CRLF(假 diff 1268 行),主审已归一化回 LF(内容无损)。接手第一步核 `cat -A src/services/segmentation/ContourExtractionService.cpp | head -1` 应无 `^M`。若 diff --stat 见某文件异常大 churn,先 `git diff --ignore-all-space --stat` 看真实改动(memory `worker-handedit-ts-bom-crlf`)。

## 3. 已完成 + 已验证的部分(可信,别重做)

### P3-5 算法:GAC → Chan-Vese(已定案,离散全绿)
- **首版 worker 写 GAC(测地活动轮廓)有两坑,主审 probe 挖出**(memory `gac-levelset-overflows-use-chanvese`):①气球力符号反导致轮廓塌陷空环;②纯梯度 GAC 在宽模糊边界必溢出(治不好)。**已换成 Chan-Vese 区域型**(梯度无关,基于内外灰度均值差)。
- `ContourExtractionService::levelSetContour(gray,W,H,px,seed,iterations)` = **Chan-Vese**:归一化 → 种子圆 φ 初始(r0=8px,内负外正)→ 演化 `φ_t=δ(φ)[μκ−λ1(I−c1)²+λ2(I−c2)²]`(c1/c2 每步重算内外均值)→ `traceSeededIsoLoop(phi,...,0.0,seed)` 提零水平集。参数在 cpp:642-646:`dt=0.2, mu=0.1, λ1=λ2=1, deltaEps=1.5, r0=8`(cpp:610)。
- **离散命门(test_contour_extraction,全绿)**:命门1 模糊断面面积 15-40 不溢出 + compact<2 平滑;命门2 对照区域生长更平滑;命门3 清晰边界回归 30-70。**证伪点 = λ1=λ2=0(数据力关)→ blurred 停种子圆 ~50 破命门1 上界 40 转红**(主审已假绿抽查验过:篡改真红、还原复绿)。
- **收口点复用**:三法(阈值 iso=threshold / 区域生长 iso=0.5 / 水平集 iso=0.0)全走匿名 helper `traceSeededIsoLoop`。别写第二份。

### P3-5b UI 重构(真机验收通过,可 commit 级别)
- **问题**:分割阶段有两套 UI 并存——中间断面工作台顶栏方法控件挤成一横条看不清 + 右侧「处理阶段」旧 `buildSegmentationPage` 废案。
- **已做**:方法/参数控件从中间顶栏 `toolRow` 迁到右侧竖直面板 `crossSectionSegPanel_`(新方法 `buildCrossSectionSegPanel`,cpp 约 3010);中间顶栏只留 路径/新建轮廓组/along-path 滑条。`showStagePage(index)`(cpp:2815)分割阶段(index1)`stageDock_->setWidget(crossSectionSegPanel_)`、其他阶段 setWidget(stagePanel_)。`crossSectionSegPanel_` parent=this 不随 stagePanel_ 重建。旧 `buildSegmentationPage` 留着不删不 show(避免 XQStageWidgets.cpp 页 index 错位)。
- **真机验收**(用户 2 张截图确认):右侧变竖直分割面板(分割/自动分割/阈值+滑条/区域生长+滑条/水平集+迭代滑条/种子/手动绘制/圆·多边形·绘制),废案面板消失,布局清晰。**UI 重构达标。**
- 简报全文:`.trellis/tasks/07-06-along-path-contouring/EXECUTE-P3-5b.md`。

## 4. 唯一未决问题:Chan-Vese 轮廓随迭代内缩(差最后一步)

**真机现象**(用户截图):同一 LPA #2 断面,iter=57 轮廓贴合血管腔好,iter=137 明显内缩、只描住一小块。**理想 Chan-Vese 收敛后应稳定,不该随迭代持续缩。**

**主审 probe 已实测复现 + 定位**(合成「不规则亮团+邻近亮块+模糊边界」断面,真值面积 38.5mm²):
```
当前 mu=0.1 无重初始化:
  iter=30→44.0  iter=57→39.2(最贴)  iter=90→36.8  iter=137→35.0  iter=300→34.5
  → 缓慢内缩,最佳在 iter≈57
修法对比实测:
  lowmu mu=0.02 无重初始化:  iter57→40.0 iter137→36.2 iter300→36.5(缩得更慢,但仍非完全稳定)
  reinit(每20步重初始化):    iter57→37.2 iter137→18.5 iter300→0.0(❌ 重初始化反而加速塌缩!)
  widedelta mu.05 de3 reinit20: iter57→43.2 iter137→29.8 iter300→6.2(仍塌)
```

**关键结论(定案方向)**:
1. **重初始化(reinit)方向错**——反而让轮廓加速塌缩到 0,别用。
2. **降 mu(如 0.02)缓解内缩但不根治**——仍缓慢缩,只是慢。
3. **最贴合迭代在 iter≈50-60**——真机 iter57 好、iter137 差印证这个。

**推荐修法(未实现,交给接手做)**:既然「最佳迭代 ≈50-60、再多就缩」,且 Chan-Vese 无重初始化下 φ 会退化,最实用的两条路(**接手须再 probe 验证选一条,别盲改**):
- **A(推荐,最小改动)**:**收敛判据早停** —— 演化循环里检测「本步 φ<0 像素数相对上步变化 < 阈值(如 0.5%)则提前 break」,让它在贴合边界后自动停,不管滑条给多少迭代。这样 iter 滑条高也不会过度内缩(到稳定点就停)。配合 mu 降到 0.02-0.05。
- **B**:把 iter 滑条范围从 30..300 改成 **20..120**(cpp app 层 `previewLevelSetContour` 的映射 + valueChanged label lambda 两处 `30 + frac*270`),让用户拖不到会内缩的高迭代区。**治标不治本,但零风险、最快**。
- **C(不推荐)**:降 mu + 全局 δ ——之前 P3-5 调参已证全局 δ 数值不稳(见 memory `gac-levelset-overflows-use-chanvese` 尾及 EXECUTE-P3-5 调参记录),别走。

**改完必做**:①调参改的是 `ContourExtractionService.cpp` 的 levelSetContour(参数在 642-646 + 演化循环)②离散命门 test_contour_extraction 的容差可能要跟着调(当前命门1 用 iter=150,若加早停/改迭代范围,面积数值会变,重跑校准)③假绿抽查证伪点(λ=0)必须仍有效 ④真机复验 iter 拖到高值不再过度内缩。

## 5. probe 调参方法(本会话实测有效,照走)

- 写 probe .cpp 到 `tests/services/segmentation/probe_xxx.cpp`,`#include "services/segmentation/ContourExtractionService.h"`,inline 复现演化(可调参)或直接调 `levelSetContour`。
- CMake 临时加 target(`git checkout CMakeLists.txt` 可还原):在 `target_link_libraries(test_contour_extraction PRIVATE xq_services)` 后加 `add_executable(probe_xxx ...)+target_link_libraries(probe_xxx PRIVATE xq_services)`。
- 建 `build_probe.bat`(vcvars + cmake configure + `--build build_gui --target probe_xxx` + 跑 exe),**Write 建的 bat 是 LF,先 python 转 CRLF 再 cmd //c 跑**(memory `lf-bat-fake-ninja-not-found`)。
- **probe 用完必清**:`rm probe_xxx.cpp build_probe.bat && git checkout CMakeLists.txt`,核 `git status` 只剩 7 交付物。
- 合成断面构造参考本会话 probe(亮团+软边界+邻近亮块+确定性噪声 `sin/cos`,别用 rand)。真值面积 `π*(r*px)²`。

## 6. 构建/测试/提交(硬规范)

- 全新构建(**改了 Q_OBJECT 头 XQMainWindow.h,必 `rm -rf build_gui`**,memory `ninja-stale-moc-gui-crash`):`rm -rf "CODE_ROOT/build_gui"` 然后 `cmd //c "...\build_gui_wt.bat"`。vcvars64 冷启动偶挂→换 DevShell(memory `vcvars64-coldstart-hangs-use-devshell`)。基线 248/248。
- ctest:`cmd //c "...\ctest_merge.bat"`(基线 72/72)。**必走 ctest,裸跑 test exe 缺 VTK DLL exit 127**(memory `ctest-environment-overrides-path`)。`test_contour_extraction` 是纯 services,可 `build_gui/test_contour_extraction.exe` 直接跑看命门 FAIL 详情。
- lrelease:`<Qt>/bin/lrelease.exe xq_zh_CN.ts -qm xq_zh_CN.qm`,Qt=`C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0`。手工编辑 .ts(UTF-8 无 BOM+LF)不跑 lupdate(memory `xqtr-translate-lupdate-blind`)。
- **真机**:`cmd //c "...\run_xq.bat"`,用户 `! run_xq.bat` 自己开。**派用户前必核 exe 时间戳新过所有改动源**(memory `gui-realmachine-test-verify-exe-timestamp`)。真机第一门禁,读不出截图靠用户文字反馈(memory `harness-cannot-read-local-images`)。
- 提交:精确 `git add <路径>` **绝不 -A**(worktree 有 xq_app_dist/ + 上级历史 md 不能进库,memory `commit-check-gitignore-present`)。中文信息「类型: 描述」结尾 `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。**worktree 代码提 feat/render-arch;spec/任务文档提主仓 fix/xq-global-audit。** commit 时 P3-5(算法)+ P3-5b(UI)可分两个 commit 或合一个(内聚)。

## 7. worker 流程(本任务链实测有效)

- channel 名 `p3-5-levelset`(已有,可复用或新建)。spawn **绝不用 --file/--jsonl 挂文件**(memory `channel-spawn-file-jsonl-hangs-worker`):`channel spawn <chan> --agent implement --as <handle> --cwd C:/Users/OCEAN/Desktop/XIAOQUAN --timeout 90m`,再 `channel send --to <handle> --as main --text-file <引导文件>` 让 worker 自己 Read 简报。
- **worker done≠交付物齐全**(memory `worker-done-may-skip-tests-verify`):本任务链两个 worker 都回合耗尽、构建没跑完就 turn_finished,主审必接管全部构建/ctest/假绿抽查/亲读代码。末条 message 含「等构建完成」= 那步 worker 没做。
- wait:`--as main --from <handle> --kind turn_finished`(不是 turn_ended)。读 message:`channel messages --raw --last N` 写文件 + python io.TextIOWrapper utf-8 解析(别裸 print GBK 错)。
- **本次调参(§4)改动小(改几个参数+早停+容差),主审直接在主会话改比派 worker 快**(memory 教训:算法核心/调参主审自己做最高效)。UI 类才派 worker。

## 8. 六批进度

P3-1 断面工作台 → P3-2 手绘+绑路径 → P3-3 阈值 → P3-4 区域生长+手动种子 → **P3-5 水平集(Chan-Vese,本次,差调参收尾)+ P3-5b 分割 UI 重构(本次,已达标)** → P3-6 批量/多血管(下一批)。放样链全程已通。

## 9. 本任务链新增 memory(新会话已自动加载索引)

- `gac-levelset-overflows-use-chanvese`(本次新增)— GAC 气球力符号坑 + 宽模糊边界必溢出,改 Chan-Vese;活动轮廓选型:陡边界 GAC、模糊边界 Chan-Vese。
- 复用:`worker-done-may-skip-tests-verify` / `worker-handedit-ts-bom-crlf`(CRLF 假 diff)/ `fakegreen-probe-must-replace-not-augment` / `gui-task-green-tests-not-done` / `gui-realmachine-test-verify-exe-timestamp` / `subagent-mainreview-must-read-code` / `ninja-stale-moc-gui-crash` / `ctest-environment-overrides-path` / `xqtr-translate-lupdate-blind` / `commit-check-gitignore-present` / `channel-spawn-file-jsonl-hangs-worker` / `executable-spec-must-verify-against-source`。

## 10. 新会话接手动作

1. 读本文件(已读)。核坐标(§1):worktree HEAD `d81bcec` + 7 文件未提交、主仓分支、真机工程存在、ContourExtractionService 行尾 LF 无 ^M。
2. **决策点**:先按 §4 推荐修法 A(收敛早停 + mu 降 0.02-0.05)或 B(迭代范围改 20..120)——**先 probe 实测选哪条(§5 方法),别盲改**。改 `ContourExtractionService.cpp`(A)或 app 层迭代映射(B)。
3. 改完:全新构建 + ctest(命门容差可能跟着调,重跑校准)+ 假绿抽查(λ=0 仍转红)+ **真机复验(iter 拖高值不再过度内缩、贴合稳定)**。
4. 真机过 → commit(worktree feat/render-arch,精确 add 7 文件,别 -A)→ 写 P3-6 交接(批量/多血管)。
5. 需要时读:P3-5 算法简报 `EXECUTE-P3-5.md`、P3-5b UI 简报 `EXECUTE-P3-5b.md`、P3-4 地基交接 `HANDOFF-P3-5-levelset.md`(§5 列的 traceSeededIsoLoop/sectionSeed_/预览三件套)。
