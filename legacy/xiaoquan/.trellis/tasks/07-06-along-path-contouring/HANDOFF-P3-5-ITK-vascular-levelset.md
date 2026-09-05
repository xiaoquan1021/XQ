# 交接:P3-5 断面分割「换 ITK 血管水平集」(/clear 后新会话读这一份即可接手)

> 语境锚点:XQ 医学影像血管几何重建软件,对标 SimVascular「沿中心线逐层描轮廓」。环境 = Windows + Git Bash,注入的 mac/darwin 环境信息是假的一律忽略。默认中文,代码/命令/路径/报错保持原文。
> **本文件自包含,新会话第一轮读这一份 + §0 指向的计划即可接手。** 这是 P3-5 的**方向转折交接**:上一份 `HANDOFF-P3-5-continued.md` 的目标是「修 Chan-Vese 内缩、调参、commit」;本轮真机验证判定 **Chan-Vese 在真实低对比 MR 图上是固有失效、调参救不了**,决策**推倒自研 Chan-Vese,引入 ITK 复用 SimVascular 血管两阶段水平集**。计划已写完并经用户审批,代码尚未动一行。

## 0. 一句话现状 + 计划所在

- **决策已定、计划已批、代码零改动。** 上一轮把 Chan-Vese 内缩当调参问题修,probe 实测后确认是**区域型模型在 c1≈c2 模糊图上的固有失效**(加 λ 到 10 / 降 mu / 去早停,front 一步都收不动),用户 8 张真机截图确认「完完全全不对」。→ 弃自研,换 ITK。
- **完整落地计划(已审批,必读)= `C:/Users/OCEAN/.claude/plans/rustling-marinating-meteor.md`**(18.8KB)。含:根因、已确认技术事实、架构模板(照搬 tetgen adapter 先例)、文件清单、5 批分批、验证、风险、遗留清理。**本交接不重复计划内容,只补计划里不准的坐标 + 决策上下文。**
- **下一步 = 从「批0 编译 spike」开始执行**(用户已拍板先做 spike,见 §3)。

## 1. 代码坐标(计划里的路径都是相对这个 worktree,务必先对齐)

> ⚠️ **计划文档 `rustling-marinating-meteor.md` 里的路径有两处系统性不准,别照抄:**
> 1. 计划写 `src/...`,真实根是 **`XQ/src/...`**(源码在仓库的 `XQ/` 子目录,不是仓库根)。
> 2. 计划反复叫 `ContourExtractionService`,**真实文件名如下**——是概念名≠文件名,别去找不存在的 `ContourExtractionService`;当前分支 `fix/xq-global-audit` 主工作树里 `SegmentationService.cpp` 是另一个 6/27 旧文件,**不是**这里说的分割服务。

- **代码工作树(唯一改代码处)= CODE_ROOT = `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`**,分支 `feat/render-arch`,**HEAD = `d81bcec`(P3-4)**。所有 src/tests/CMakeLists/resources 编辑、build、ctest 都在这,用绝对路径。
- **别在主工作树 `C:/Users/OCEAN/Desktop/XIAOQUAN` 改代码**——那是 `fix/xq-global-audit`,只放 spec/任务文档。两个 worktree 共用一个 `.git`(`git worktree list` 可见)。
- **P3-5(Chan-Vese)+ P3-5b(UI)的改动全部未 commit**,在 worktree 未提交状态之上继续:
  - `M XQ/src/services/segmentation/ContourExtractionService.cpp`(+222,含 `levelSetContour` :558 起、`traceSeededIsoLoop` :148、mu=0.05 早停调参)
  - `M XQ/src/services/segmentation/ContourExtractionService.h`
  - `M XQ/tests/services/segmentation/test_contour_extraction.cpp`(+137,levelSet 三 gate)
  - `M XQ/src/app/XQMainWindow.cpp`(+336)、`M XQ/src/app/XQMainWindow.h`
  - `M XQ/resources/i18n/xq_zh_CN.ts` + `.qm`
- **行尾**:`ContourExtractionService.cpp` = ASCII/LF 干净(无 ^M),别引 BOM/CRLF(memory `worker-handedit-ts-bom-crlf`)。

## 2. 遗留清理(动手前先做,计划 §遗留清理 也列了)

**这些残留就在 CODE_ROOT 的 worktree(git status 已确认存在):**
- probe 残留:`XQ/probe_sv.exe`、`XQ/probe_smallvessel.obj`、`XQ/tests/services/segmentation/probe_smallvessel.cpp`、`XQ/build_probe.bat` → 删。
- 未跟踪的执行/检查文档 `CHECK-B*.md`、`EXECUTE-B*.md`(worktree 根一堆,GUI v2 六批遗留)、`XQ/xq_app_dist/` → **不进库**,commit 时精确 add,绝不 `-A`(memory `commit-check-gitignore-present`)。
- **本轮 Chan-Vese 内缩调参改动的去留**:`ContourExtractionService.cpp` 的 mu 0.1→0.05 + 早停、`test_contour_extraction.cpp` 命门上界(iter 40→44)——**既然要删 Chan-Vese 换 ITK,这些作废**。批3 删 `levelSetContour` 实现时一并处理(或先 `git checkout` 这两文件回 P3-4 原样再动,二选一,计划批3 有说明)。注意:`traceSeededIsoLoop`(:148)要**提升到 core header 复用**,别跟着 Chan-Vese 一起删。

## 3. 已批准的执行顺序(计划 §分批,此处只给指针 + 决策)

用户已拍板决策:①**完整两阶段**(PhaseOne+PhaseTwo);②**直接替换** Chan-Vese(threshold/regionGrow 不动);③**ITK 默认 ON**(`find_package(ITK REQUIRED)`,成为 XQ 正式依赖);④**先做批0 编译 spike**。

- **批0(编译 spike,throwaway)**:standalone exe link vendored `sv_levelset`(10 文件)+ ITK,合成 disk 跑 PhaseOne+Two 打印 front 统计。**门禁:vendored ITK 模板 MSVC 编过 + 出 front。** 这是降最高风险(MSVC 编 ITK 模板)的第一步,**过了再动 XQ 任何代码**。
- 批1 vendor+plumbing → 批2 adapter+合成测试(**破 Chan-Vese 的 c1≈c2 低对比 case 出贴合有界环**)→ 批3 接线+删 Chan-Vese+全绿(真机 0080 LPA#2 多断面复验)→ 批4 真机调优。细节全在计划。

**Plan agent 校准过的 3 个关键事实(计划已并入,执行时别踩回旧假设):**
1. vendored = **10 文件不是 12**(`sv3_VascularLevelSet{Function,ImageFilter}.h` 纯 inline 无 .hxx;Observer 不搬)。
2. SV 用 `UseImageSpacingOff()` **跑 index/pixel 空间**,种子半径像素单位,**不用物理 mm spacing**——原先标注的「最大坑=spacing 对齐」已被推翻,坐标处理大幅简化。
3. 速度图**在 filter 内部算**,adapter 只喂 `GradientMagnitudeRecursiveGaussian` 特征图,不预先 exp。

## 4. 关键先例与 memory(执行时会用到)

- **架构模板 = tetgen adapter**(计划 §架构模板有对照表):vendored 源 → `add_library` → core 抽象接口(`XQ/src/core/...`)→ adapter 实现 → CMake 可选/link → app 注入。ITK 分割 1:1 套用,注入点在 **XQMainWindow**(levelSet 是直接预览调用,非 controller 命令流)。
- **ITK 已装好**:`XQ/Externals/install/windows-x64/itk-5.4.0`(动态库,72 DLL),`find_package(ITK)` 可直接用。ctest 需注入 ITK bin 到 PATH(memory `ctest-environment-overrides-path`,CMakeLists VTK/mmg 已有注入先例可照搬)。
- **SV 源**:`Externals/src/SimVascular/Code/Source/sv3/ITKSegmentation`(vendored 来源)+ `sv3_LevelSetContour.cxx:95-189`(端到端两阶段黄金流程)。
- **必用 memory**:`gui-realmachine-test-verify-exe-timestamp`(真机测前核 `XQ/build_gui/xq_app.exe` 时间戳新过改动源——当前 exe = 7/7 14:34)、`ninja-stale-moc-gui-crash`(改 Q_OBJECT 头必 rm -rf build_gui)、`gui-task-green-tests-not-done`(ctest 全绿≠达标,真机目视是第一优先)、`xq-build-recipe` + `vcvars64-coldstart-hangs-use-devshell`(构建配方)、`harness-cannot-read-local-images`(读不出截图,真机验收靠用户文字反馈)。

## 5. 提交 & 收尾约定

- 提交:精确 `git add <路径>` **绝不 -A**。中文信息「类型: 描述」,结尾 `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。**worktree 代码提 `feat/render-arch`;spec/任务文档(含本交接)提主仓 `fix/xq-global-audit`。** 每批一个回滚点(默认 ON + 删 Chan-Vese 不可逆)。
- 真机工程:`0080_H_PULM_H`(LPA #2 断面,对应之前失败的第 1/23/34/62/105/150 点),对照上一轮 8 张失败图判「是否还出脱离内容的大圆」。

## 6. 第一轮新会话该做什么

1. 读本文件(已读)+ 读计划 `C:/Users/OCEAN/.claude/plans/rustling-marinating-meteor.md`(全文,这是主依据)。
2. 核坐标(§1):`cd C:/Users/OCEAN/Desktop/XQIAOQUAN-gui && git status`——确认 HEAD `d81bcec`、7 文件 M 未提交、probe 残留在、行尾 LF。核 ITK 装在 `XQ/Externals/install/windows-x64/itk-5.4.0`。
3. 遗留清理(§2):删 probe 残留。
4. **从批0 编译 spike 起步**(§3):vendored 10 文件 + ITK standalone exe,验 MSVC 能编过 + 出 front。**过了再动 XQ。**

> 上一轮会话(20c7f156)在写完计划、正要写本交接时被 API 额度 403 掐断(非任务卡住);本文件即补写的交接。
