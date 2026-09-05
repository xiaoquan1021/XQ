# Implementation Plan：壳 A 端到端验收

## Steps

1. 建立去标识 fixture、金 Path/Contour 和固定期望，不引入网络/PHI。
2. 新增 headless `test_shell_a_e2e`，贯通导入、Profile、smoke、保存、销毁、重开与 checksum/lineage 比较。
3. 新增 negative matrix 与 transitive stale/undo assertions。
4. 接通最小 GUI 操作和真机 checklist；不得创建第二套 orchestration service。
5. 运行 Flow ON/OFF、focused、全量与既有 0007/VTI 回归，填写最终验证记录。

## Validation

```powershell
cmake --build XQ/build_gui --config Release --target test_shell_a_e2e test_dicom_import_integration test_vessel_profile_assembler test_flow_geometry_smoke test_project_roundtrip test_scene_relations test_workflow_integration test_main_window test_app_startup
ctest --test-dir XQ/build_gui -C Release --output-on-failure -R "shell_a_e2e|dicom|vessel_profile|flow_geometry_smoke|project_roundtrip|scene_relations|workflow_integration|main_window|app_startup"
cmd /c XQ\build_gui_wt.bat
ctest --test-dir XQ/build_gui -C Release --output-on-failure
cmake -S XQ -B XQ/build_gui -DXQ_DICOM_TEST_DATA_ROOT='C:\path\to\public-anonymized-series'
cmake --build XQ/build_gui --config Release --target test_dicom_real_series test_shell_a_real_data
ctest --test-dir XQ/build_gui -C Release --output-on-failure -R "dicom_real_series|shell_a_real_data"
cmd /c XQ\run_xq.bat
cmd /c XQ\build_shell_noflow_wt.bat
ctest --test-dir XQ/build_shell_noflow -C Release --output-on-failure -R "shell_a_noflow|project_roundtrip|app_startup|main_window|arch_boundaries"
```

## Review and rollback

- Final gate：任何 production path 被 test double 替代、任何 PHI/network CI 依赖、任何 full ctest 失败都不得完成。
- Commit A：fixture/headless success；Commit B：negative/stale；Commit C：GUI/acceptance record。
- GUI 问题只回滚 Commit C；不得削弱 headless assertions。若上游契约缺陷暴露，返回对应 child 修复并重新全链检查。

## Completion Record (2026-07-11)

### Implemented

- Added Shell A headless, GUI, and Flow-OFF regression gates without a mock
  solver, Noop provider, fabricated `FlowResult`, or test-only orchestration
  path. Historical Flow fixture content was produced through the production
  `FlowSolver1D` chain.
- Startup now carries `projectFilePath`; `XQWorkflowSession` owns and exposes the
  production `VesselProfileController`; Flow-OFF keeps DICOM import and
  Path/ContourGroup/VesselProfile assembly while disabling only new smoke
  execution.
- GUI-created ContourGroups now publish the `Path -> ContourGroup` Scene relation
  atomically and inherit an explicit Path ScaleSlot. DICOM series display labels
  include a deterministic ordinal so technical-label collisions cannot select
  the wrong first series.
- Updated the Chinese TS/QM catalog while preserving the literal
  `XQStageWidgets` context used by `xqTr`.

### Verification Evidence

- Flow-ON Release full CTest: **92/92 passed**.
- Flow-OFF Release full CTest: **83/83 passed**.
- Local DICOM technical gate: **2/2 passed**:
  `test_dicom_real_series` and `test_shell_a_real_data` read a 65-file,
  34,199,674-byte multi-slice series through the production reader/import/
  save/reopen/resolver path. The rerun used the external public TCIA LIDC-IDRI
  sample, not the repository fixture.
- External provenance validation passed: the saved ZIP SHA-256 matched, all
  65 provider-supplied MD5 hashes matched, and all 65 files had the DICM
  preamble. The adjacent external SOURCE/LICENSE bundle records the official
  collection/download identity, CC BY 3.0 license, and TCIA de-identification
  basis; none of that external data is committed here.
- Flow-OFF `build.ninja` contains none of the six execution sources:
  `BoundaryConditionService.cpp`, `FlowSolver1D.cpp`, `FlowInputAssembler.cpp`,
  `FlowGeometrySmokeService.cpp`, `FlowController.cpp`, or
  `FlowSmokeController.cpp`.
- Chinese catalog audit: 355 finished translations, 0 unfinished, and 0 missing
  touched source keys. `git diff --check` passed after verification.
- The default `XQ_DICOM_TEST_DATA_ROOT` was restored to empty after the optional
  local gate, so normal builds do not silently depend on that machine path.

### Acceptance Boundary

- The external authorized/de-identified real-data evidence gate is now backed by
  the local SOURCE/LICENSE/hash bundle plus the production 2/2 run. A same-turn
  attempt to reach the official collection page returned no HTTP response, so
  this record does not claim live web revalidation; it relies on the previously
  downloaded official provenance artifacts and the independently rechecked
  hashes.
- Automated GUI tests ran against production controllers in Qt offscreen mode;
  they do not replace the user's pending physical-machine GUI acceptance.
- Keep this child task and the parent task in progress until that acceptance is
  recorded. Do not archive them at this commit boundary.

### Durable Lessons

- Payload binding is not Scene lineage: setting
  `XQContourGroup::sourcePathNode()` without the Scene relation creates an
  orphan for stale/persistence/controller consumers.
- `xqTr` calls must stay under TS context `XQStageWidgets`; treating the helper
  as an ordinary lupdate `tr()` alias misclassifies them under `xq`.
- A successful real-file technical test cannot establish dataset authorization
  by itself. Require a separate source/license/de-identification/hash bundle,
  verify it, and distinguish local evidence verification from live web access.
