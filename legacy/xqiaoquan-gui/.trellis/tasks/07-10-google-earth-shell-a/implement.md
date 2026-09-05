# Implement Plan: Google Earth 式数字孪生壳档 A

## Preflight

1. 规划阶段保持 `feat/render-arch` / `f1f6230`，完成父子 PRD/design/implement 与 context manifest，并运行 `task.py validate`。
2. 完成 artifact/sub-agent review，并确认用户已明确授权“先 Trellis 规划，再开分支实现”；该授权已在本任务创建前由用户给出。只有 scope 发生实质变化才重新询问。随后从精确基线创建 `feat/google-earth-shell-a`，记录 parent/child 的 base branch 与 branch，再激活实施任务。
3. 实现前使用 `trellis-before-dev` 加载 XQ 规范；Phase 2 默认用 Trellis channel 的 implement/check workers。
4. 每次编辑前复核工作树，禁止触碰 `CHECK-*.md`、`EXECUTE-*.md` 和 `XQ/xq_app_dist/`。
5. 各 child implement 中的 Commit A/B/... 是该 child 内的连续 focused commit series；不同 child 的 commits 不得交错。child check 全绿后记录 series range/final commit，再进入下一个 child。

## Delivery Sequence

### T1 — `shell-a-domain-contracts`

- 增加 ScaleSlot optional、metadata Image payload、VesselProfileV1、validator、DerivationStamp、contentRevision 与领域/资产类型映射。
- 增加原子多父关系 command 和 semantic replace+invalidate command。
- 补 ContourGroup typed persistence/AssetKind，升级并兼容项目 schema。
- Gate：纯 core/io tests、旧 1.2 读取、新格式 round-trip、无外部类型泄漏全部通过。
- Commit/rollback point：`feat(shell-a): add versioned domain contracts`。

### T2 — `shell-a-dicom-spine`（依赖 T1）

- 在现有 ITK/GDCM adapter 边界实现 series enumerate/read 与结构化诊断。
- 增加 DICOM import transaction、ExternalSource Image asset、typed node 和 ImageSourceResolver。
- 接入 GUI 目录/series 选择，但业务提交留在 service/command；保留 VTI 回归。
- 加入去标识 fixture、source missing/drift/multi-series/rescale/oblique tests 与可配置真实数据门。
- Gate：首次读取→Source→保存→退出→按 UID 重读，几何/体素摘要一致；所有失败零半状态。
- Commit/rollback point：`feat(shell-a): add external DICOM data spine`。

### T3 — `shell-a-profile-assembly`（逻辑依赖 T1；共享工作树调度上必须在 T2 完成后执行）

- 实现 Path+Contour 与 imported-gold 两条 Profile 装配路径和稳定 sample/provenance。
- 把 contour area 从 MainWindow 下沉；legacy cm 必须显式转换。
- 接入 revision、双父 lineage、stale、undo/redo；清除直接 payload mutation。
- Gate：validator 矩阵、synthetic area、cm→mm/mm²、双父/stale/undo、round-trip 全绿。
- Commit/rollback point：`feat(shell-a): assemble canonical vessel profiles`。

### T4 — `shell-a-flow-smoke`（依赖 T3）

- 实现 FlowInputAssembler 的单位转换、均匀重采样和 typed diagnostics。
- 扩展 SimulationCase/FlowResult 的显式 Profile 来源和 derivation stamp。
- 删除 MainWindow 的 contour→solver 拼装，增加固定 `flow_geometry_smoke`。
- Gate：数值单位 UT、非均匀 station UT、固定 solver IT、失败零结果、保存重开一致。
- Commit/rollback point：`feat(shell-a): add profile-to-flow geometry smoke`。

### T5 — `shell-a-flow-capability`（依赖 T4）

- 注入 WorkflowCapabilities/factory，Flow OFF 不构造 controller，UI 明确 unavailable。
- 增加 `XQ_ENABLE_FLOW` CMake 配置与独立 no-flow build/test；历史 result 保持可读。
- Gate：ON/OFF runtime tests、no-flow build、非 Flow 工作流与项目重开通过。
- Commit/rollback point：`feat(shell-a): make flow an optional capability`。

### T6 — `shell-a-e2e-gate`（依赖 T2/T4/T5）

- 建立 headless 与 GUI 共用的完整纵向 E2E；补负路径、busy/防重、属性显示和书面人工清单。
- 运行自动 fixture、`0007_H_AO_H` 回归、真实去标识 DICOM 手工门、GUI true-window 与 no-flow 门。
- 只修集成缺口，不在此新增 atlas/solver/biology 范围。
- Commit/rollback point：`test(shell-a): lock end-to-end shell gate`。

## Focused Validation

具体 target 名允许在实现时按现有 CMake 命名收敛；仅列真实 executable target，CTest-only 名称通过 `ctest -R` 运行：

```powershell
cmake --build build_gui --config Release --target `
  test_domain_contracts `
  test_project_roundtrip `
  test_itk_dicom_series_reader `
  test_image_import_roundtrip `
  test_vessel_profile `
  test_vessel_profile_assembler `
  test_flow_input_assembler `
  test_flow_geometry_smoke `
  test_workflow_capabilities `
  test_main_window
```

```powershell
ctest --test-dir build_gui -C Release --output-on-failure -R `
"(project|asset|command|scene|source|dicom|vessel_profile|flow_geometry|workflow|main_window)"
```

## Full Validation Gates

```powershell
Set-Location C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ
cmd /c build_gui_wt.bat
ctest --test-dir build_gui -C Release --output-on-failure
cmd /c run_xq.bat
```

No-flow 使用 capability child 交付的独立、可复制 wrapper；它必须包含 vcvars64、Ninja、Release、完整 prefix/test-data 配置和 `-DXQ_ENABLE_FLOW=OFF`：

```powershell
cmd /c XQ\build_shell_noflow_wt.bat
ctest --test-dir XQ/build_shell_noflow -C Release --output-on-failure
```

并至少构建应用、运行非 Flow core/io/service/app tests、启动真窗口并打开含历史 FlowResult 的项目。

真实 DICOM 人工门通过：

```powershell
-DXQ_DICOM_TEST_DATA_ROOT=<authorized-deidentified-series-root>
```

该路径/数据/截图/日志不得提交；报告只记录脱敏汇总和测试状态。

## Check Worker Gates

- 架构：依赖方向、外部类型泄漏、GUI 业务逻辑回流、重复真源。
- 数据流：Source→transform→store→reopen→display/solver 每个边界的格式、validation owner 与诊断。
- 持久化：旧 schema、新 schema、typed payload、ScaleSlot、revision、lineage/stale、asset binding。
- 数值：LPS/mm、Profile mm/mm²、唯一 CGS 转换点、rescale 无 double-apply、uniform stations。
- 原子性：DICOM import、多父提交、semantic edit、Flow failure、undo/redo 的零半状态。
- 诚实口径：所有 smoke UI/日志/文档仅称 `L0 geometry smoke`。
- 安全：fixture 许可/去标识，日志无 PHI，未触碰用户未跟踪文件。

## Completion and Stop Rule

只有 parent AC1–AC16 全部有可重复证据、Release 全量 ctest 与 true-window/no-flow/真实 DICOM 人工门通过，才完成壳 A。完成后不继续实现 atlas、Darcy、CTC、PhysiCell 或肿瘤转移；这些进入新的 Trellis 任务。
