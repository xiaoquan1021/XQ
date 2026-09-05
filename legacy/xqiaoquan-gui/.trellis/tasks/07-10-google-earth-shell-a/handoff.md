# Handoff: Google Earth 壳档 A (current)

更新时间：2026-07-11（Asia/Shanghai）

## 当前 Clear 判断

- 父任务 `07-10-google-earth-shell-a` 仍为 `in_progress`，目前完成
  **3/6** 个 child。
- 已完成并归档：`domain-contracts`、`dicom-spine`、`profile-assembly`。
- 当前没有已激活 child。剩余 `flow-smoke`、`flow-capability`、`e2e-gate`
  全部仍为 `planning`。
- `get_context.py` 当前显示 `shell-a-e2e-gate`，但 Source 是
  `session-fallback`。这不是 `task.py start`，也不是用户许可；不得把它当
  作活动任务。
- 下一步必须等待用户明确选择 child。不得自动进入 `flow-smoke` 或任何
  其他 child。

## 当前仓库断点

- 仓库：`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui`
- 分支：`feat/google-earth-shell-a`
- Profile 工作提交：
  - `2cf7f71 feat: assemble and publish vessel profiles`
  - `eb6b99c fix: make contour edits semantic and collision-safe`
  - `5c0c5e8 docs: record vessel profile contracts`
- 归档提交：
  - `2737684 chore(task): archive 07-10-shell-a-profile-assembly`
- journal 提交：
  - `d26359d chore: record journal`
- Profile child 归档路径：
  `.trellis/tasks/archive/2026-07/07-10-shell-a-profile-assembly`

## Profile child 已交付

- 纯 C++ `VesselProfileAssembler`：严格 fail-closed 地从已重采样 Path 与
  同 lineage ContourGroup 生成确定性 `VesselProfileV1`。
- 面积仅在 contour 自身 frame 的二维投影上计算；位置与 tangent 仅来自
  Path frame；拒绝 open、退化、重复点/弧长/ID、自交、越界与 frame mismatch。
- 短边相交容差按边长归一化；sample id 稳定映射 contour id；provenance
  保存 algorithm/version/参数与精确输入 stamp。
- typed imported-gold 路径显式支持 `mm/mm2` 与 `cm/cm2`，厘米转换为
  position/arc `x10`、area `x100`，并要求 Path 与 external evidence
  id/fingerprint。
- `VesselProfileController` 使用 owner-thread capture、纯值 worker compute、
  owner-thread guarded commit；重检 project pointer/lifecycle epoch、revision、
  payload identity、stale、ScaleSlot 与 Asset kind/fingerprint。
- Profile node、可选 Asset/binding、双 Scene parent 与 Asset lineage 通过一个
  `ProjectNodeBatchCommand` 原子提交；失败/undo 无半状态。
- MainWindow contour append 已改为 copy-on-write
  `SemanticReplacePayloadCommand`；Scene node 与全部 nested contour 共用全局
  ID 避碰；新 contour-group 创建也走 command stack，阻止 undo 后 ID 复用再
  redo 形成双重身份。
- `XQProject::lifecycleEpoch()` 覆盖 open/close/reopen 与 copy/move assignment，
  防止 PreparedCommand 在项目替换或生命周期重开后发生 ABA。
- 未升级 project schema；未使用 MITK。

## 最终验证

- 使用 `VSLANG=1033` 执行 `cmake --fresh` 与 Release clean-first 全编。
- `rules.ninja`：`msvc_deps_prefix = Note: including file:`。
- Ninja 依赖已恢复：`XQAppStartup.cpp.obj #deps 324`、
  `XQProject.cpp.obj #deps 76`、`VesselProfileController.cpp.obj #deps 89`，
  均为 `VALID` 且 consumer 包含 `XQProject.h`。
- focused Profile/core/MainWindow/startup：**13/13**。
- Release full CTest：**86/86**。
- 构建/生命周期/Profile/contour 契约已写入：
  - `.trellis/spec/XQ/core/vessel-profile.md`
  - `.trellis/spec/XQ/core/command-and-scene.md`
  - `.trellis/spec/XQ/core/build-and-test.md`

## 持续约束

- 不使用 MITK。
- 不升级 project schema，除非后续 child 的已审规划明确要求并重新评审。
- 不把 VTI 或合成 fixture 冒充真实 DICOM 通路。
- 不输出 DICOM 自由文本 tag 或 PHI。
- 保留且不得暂存/删除/清理：`CHECK-*.md`、`EXECUTE-*.md`、
  `XQ/xq_app_dist/` 与本 handoff。
- 严禁 `git add .`、`git clean`、`git reset --hard` 或其他工作区清理。
- 不进入下一 child，直到用户明确指定。

## 新会话恢复顺序

1. 完整阅读本文件顶部 current 区域；下方 2/6 内容仅为历史快照。
2. 运行 `C:\software\anaconda\python.exe .\.trellis\scripts\get_context.py`。
3. 确认分支为 `feat/google-earth-shell-a`，HEAD 至少包含 `d26359d`。
4. 确认父任务为 `in_progress`、3/6，三个剩余 child 均为 `planning`。
5. 忽略 `e2e-gate` 的 `session-fallback` 显示；等待用户明确选择下一 child。

---

# Historical Handoff: 2/6 snapshot (superseded above)

更新时间：2026-07-10（Asia/Shanghai）

## Clear 判断

- 可以在核对本文件后 clear，并在新会话继续父任务。
- 父任务 07-10-google-earth-shell-a 仍为 in_progress，目前完成 2/6 个 child。
- 已完成并归档：domain-contracts、dicom-spine。
- 剩余四个 child 全部仍是 planning；本会话没有进入、启动或实现任何下一个 child。
- get_context.py 当前可能显示 shell-a-e2e-gate，但 Source 是 session-fallback。这只是会话回退推断，不是 task.py start，也不是用户同意进入 e2e-gate。新会话不得把它当作已激活任务。
- 父任务 children 顺序中第一个未完成项是 profile-assembly，但必须等待用户明确指令后再进入；本交接不授权自动启动它。

## 仓库断点

- 仓库：C:\Users\OCEAN\Desktop\XQIAOQUAN-gui
- 分支：feat/google-earth-shell-a
- 基线分支：feat/render-arch
- 当前 HEAD：19be882 chore: record journal
- 父任务：.trellis/tasks/07-10-google-earth-shell-a
- 父任务进度：2/6 done

最近关键提交：

1. 9bb0341 feat: add shell domain contracts
2. 130b43a feat: persist shell domain schema 1.3
3. e4f531c feat: add reproducible project mutations
4. 7b797fb chore(task): archive 07-10-shell-a-domain-contracts
5. 0f5de07 feat: define DICOM series reader contract
6. 37e5fc1 feat: add audited DICOM series adapter
7. c0a18c0 feat: prepare atomic DICOM project imports
8. d9b7575 feat: resolve persisted DICOM image resources
9. 7d7cf82 chore(task): archive 07-10-shell-a-dicom-spine
10. 19be882 chore: record journal

## 已完成 child 1：domain-contracts

- 已完成 ScaleSlot、Image metadata、VesselProfileV1、schema 1.3 兼容持久化。
- 已完成 contentRevision / DerivationStamp、精确 stale undo/redo、项目级多父 node/asset 原子提交和后台物化竞态保护。
- 已归档到 .trellis/tasks/archive/2026-07/07-10-shell-a-domain-contracts。

## 已完成 child 2：dicom-spine

- 已完成 XQ-owned DICOM reader contract 和 GDCM/ITK adapter。
- 公共 service API 不泄漏 ITK、GDCM、Qt 类型；没有使用 MITK。
- 已完成显式 SeriesInstanceUID 选择、LPS/mm geometry、斜位 direction、IOP/IPP 排序、Float32 modality-rescaled buffer 和版本化 fingerprint。
- 已完成纯准备型原子 import：metadata-only image payload、ScaleSlot::Organ、ExternalSource/Image asset、Scene node/binding、undo/redo 与失败零副作用。
- GeometryResourceManager 已支持 project-scoped resident voxel install/acquire/remove、独立 cache flavor、预算/LRU/pin 和同指针 ABA generation 防护。
- ImageResourceResolver 已支持 resident-first、相对 .xqproj locator 优先、绝对 fallback、持久化 UID、完整 metadata/fingerprint/geometry/scalar/intensity/buffer 校验，以及 reader I/O 前后 AssetRecord 重校验。
- Windows C:series 与 \series rooted-relative 绕过已拒绝。
- reader diagnostics 只映射固定安全模板；payload 始终 metadata-only，XQProjectReader 不解码 DICOM。
- 完整契约已写入 .trellis/spec/XQ/core/source-interface.md。
- 独立终审无剩余 actionable correctness、privacy 或 MITK 问题。
- 已归档到 .trellis/tasks/archive/2026-07/07-10-shell-a-dicom-spine。
- 归档交接：.trellis/tasks/archive/2026-07/07-10-shell-a-dicom-spine/handoff.md

## 真实 DICOM 验收

最终通过样例：

- 来源：TCIA LIDC-IDRI
- Public subject identifier：LIDC-IDRI-0957
- Modality / body part：CT / CHEST
- SeriesInstanceUID：1.3.6.1.4.1.14519.5.2.1.6279.6001.314917368146772872954571551463
- 切片数：65
- 许可：CC BY 3.0
- TCIA 官方声明公开 DICOM 经标准化去标识并满足 HIPAA Safe Harbor。
- 外部数据目录：D:\XQ\data\dicom\tcia_lidc_idri_0957_ct\series
- 来源说明：D:\XQ\data\dicom\tcia_lidc_idri_0957_ct\SOURCE.md
- ZIP SHA-256：ca4324bb14873c64f9bd616002cf380f02f8f215105e22fb7b28b2769c6925ee
- TCIA 随包 MD5：65/65 匹配。
- 65 个文件均为 .dcm 且有标准 DICM preamble。

验收结果：

- test_dicom_real_series：1/1 通过，约 3.02 秒。
- 覆盖显式 UID、canonical LPS geometry、voxel/world round-trip、原子导入、保存重开、headless lazy acquire 和 SHA-256 voxel byte equality。
- 注册真实门后的 Release 全量 CTest：84/84，通过，用时 19.21 秒。
- 验收后已把 XQ_DICOM_TEST_DATA_ROOT 恢复为空。
- 当前默认 CMake 配置重新注册 83 项测试，test_dicom_real_series 默认不注册。

已排除的候选：

- C:\Users\OCEAN\Desktop\XIAOQUAN\0080_H_PULM_H 只有 VTP/CTGR/PTH/VTI 等工程产物，递归发现 0 个 DICOM series；只能作 VTI/工程回归，不能作真实 DICOM 门。
- TCIA CPTAC-LUAD 胸部 CTA 虽是公开 CC BY 4.0 单 series，但当前 adapter 在 canonical decode 阶段按契约拒绝；没有放宽测试，下载文件已移除。

## 阶段性可视验收

- 当前 Release 构建可启动，主窗口标题为 XQ。
- 直接运行 build_gui/xq_app.exe 曾报告缺少 Qt6OpenGLWidgets/Widgets/Gui/Core DLL；已用 Qt 官方 windeployqt 把 Qt runtime 与 platforms plugin 部署到 XQ/build_gui。该操作只生成构建产物，不修改源码或 Git。
- 当前启动入口：XQ/run_xq.bat。
- 若 build_gui 被重建或清空后再次缺 Qt，先运行：

    C:\Users\OCEAN\Desktop\XIAOQUAN\Externals\install\windows-x64\qt-6.7.0\bin\windeployqt.exe --release --no-translations XQ\build_gui\xq_app.exe

- 当前 GUI 可验收桌面壳、空项目启动和既有 Scene/Project 工作流。
- 本轮 DICOM child 完成的是 reader/service/project/resource spine；app 层尚无 DICOM 目录选择、series 选择和 import 按钮。因此不能在当前 GUI 中直接导入 D:\XQ 下的真实 DICOM，这属于尚未启动的后续 GUI/e2e child。
- DICOM 阶段验收以 test_dicom_real_series 和带真实门 84/84 CTest 为准，不能把现有 GUI 壳显示冒充 DICOM GUI 接入完成。

## 剩余 child

以下任务均保持 planning：

1. 07-10-shell-a-profile-assembly — 壳A：血管剖面装配
2. 07-10-shell-a-flow-smoke — 壳A：1D 血流几何冒烟
3. 07-10-shell-a-flow-capability — 壳A：Flow 能力可关
4. 07-10-shell-a-e2e-gate — 壳A：端到端验收门

没有任何一个被本会话 task.py start。不要因为 session-fallback 显示 e2e-gate 就跳过前置 child。

## 当前 Git 状态

- tracked/staged 工作区干净。
- 仅保留以下未跟踪本地文件，不得删除、暂存或清理：
  - CHECK-*.md
  - EXECUTE-*.md
  - XQ/xq_app_dist/
  - 当前父任务 handoff.md
- 严禁 git add .。
- 严禁 git clean、git reset --hard 或其他工作区清理。

## 持续约束

- 不使用 MITK。
- 不升级 project schema，除非后续 child 的已审规划明确要求并重新评审。
- 外部 kernel 继续只用项目批准的私有 adapter/service 边界。
- 不把 VTI 旁路或合成 fixture 冒充真实 DICOM 通路。
- 不输出 DICOM 自由文本 tag 或 PHI。
- 不进入下一个 Trellis child，直到用户明确指定继续哪个 child。

## 新会话恢复顺序

1. 完整阅读本文件。
2. 运行 C:\software\anaconda\python.exe .\.trellis\scripts\get_context.py。
3. 确认分支仍为 feat/google-earth-shell-a，HEAD 至少包含 19be882。
4. 忽略 get_context 中 session-fallback 自动显示的 e2e-gate；它不是激活许可。
5. 确认父任务仍为 in_progress、完成 2/6，四个剩余 child 仍为 planning。
6. 等待用户明确选择下一个 child；不得自动进入。
7. 选择后按 Trellis planning/start 流程继续，不重做已归档的 DICOM 工作。

## 建议的新会话首条指令

继续 C:\Users\OCEAN\Desktop\XQIAOQUAN-gui 的 .trellis/tasks/07-10-google-earth-shell-a。先完整阅读父任务 handoff.md。domain-contracts 与 dicom-spine 已完成并归档，父任务进度 2/6；真实 TCIA LIDC-IDRI DICOM 门和带门 Release 84/84 已通过，默认配置已恢复 83 项。不要把 get_context 的 e2e-gate session-fallback 当作已激活任务，不要自动进入任何下一个 child；保留 CHECK/EXECUTE 与 XQ/xq_app_dist，不使用 MITK。
