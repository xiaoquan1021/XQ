# XQ Execution Log

## 2026-06-07

- Continued the Windows monolith migration loop from the pushed `feature/windows-monolith-foundation` branch.
- Verified current starting point:
  - `xiaoquan1021/Externals` branch `feature/windows-monolith-foundation` at `a195f74`.
  - `xiaoquan1021/XQ` branch `feature/windows-monolith-foundation` at `4521e21`.
  - XQ configure had previously reached `build/windows-msvc-release`; the active task is now build convergence.
- Created this execution log and `plan.md` to make autonomous progress auditable.
- Ran `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`.
  - Failure: MSVC does not define the non-standard `M_PI` macro.
  - Affected first compiled files: `xq_CircularProfile.cxx` and `xq_EllipticProfile.cxx`.
  - Added `tests/test_windows_no_m_pi_macro.ps1`.
  - Replaced `M_PI` with local `constexpr` pi constants in segmentation profile code and legacy plugin sources.
  - Verification: `tests/test_windows_no_m_pi_macro.ps1` passed.
- Continued XQ build convergence after the first MSVC failure.
  - Failure: `xq_ProfileGroup` defaulted its destructor inline while owning `std::unique_ptr<xq_LumenProfile>` through a forward declaration.
  - Added `tests/test_windows_profilegroup_complete_destructor.ps1`.
  - Moved `xq_ProfileGroup::~xq_ProfileGroup()` defaulting into `xq_ProfileGroup.cxx` after `xq_LumenProfile.h` is included.
  - Verification: profile-group destructor test passed.
- Re-ran `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`.
  - Failure: MSVC instantiated copy assignment for `xq_FlowSolverRegistry`, which owns `std::vector<std::unique_ptr<xq_FlowSolverBackend>>`.
  - Added `tests/test_windows_flow_registry_noncopyable.ps1`.
  - Explicitly deleted copy and move operations for the singleton registry.
  - Verification: flow-registry noncopyable test passed.
- Re-ran XQ build.
  - Failure: `ProjectManagement` used POSIX `dirent.h` directory APIs unavailable on MSVC.
  - Added `tests/test_windows_project_management_no_dirent.ps1`.
  - Replaced `opendir` / `readdir` / `closedir` directory scans with `std::filesystem::directory_iterator`.
  - Verification: ProjectManagement no-dirent test passed.
- Re-ran XQ build.
  - Failure: `xq_WorkspaceManager` used `_mkdir` without the MSVC declaration and mixed C/POSIX path helpers.
  - Added `tests/test_windows_workspace_manager_filesystem_paths.ps1`.
  - Replaced `DirExists`, `FileExists`, and `CreateDir` with `std::filesystem` helpers.
  - Verification: workspace-manager filesystem-path test passed.
- Re-ran XQ build.
  - Failure: `M_PI` was still used in `Code/Testing/test_seg_preprocess.cxx`; the earlier test only scanned `Code/Source`.
  - Expanded `tests/test_windows_no_m_pi_macro.ps1` to scan all of `Code`.
  - Replaced testing `M_PI` references with a local `constexpr` pi constant.
  - Verification: expanded no-`M_PI` test passed.
- XQ build succeeded:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` exited `0`.
  - Incremental verification showed `ninja: no work to do`.
- CTest convergence:
  - Initial `ctest --test-dir build\windows-msvc-release --output-on-failure` hung because generated CTest environment appended raw Windows `$ENV{PATH}` and test output was buffered.
  - Added `tests/test_windows_ctest_output_unbuffered.ps1` and enabled unbuffered stdout/stderr in C++ test mains.
  - Added `tests/test_windows_tests_no_host_specific_paths.ps1`; replaced a hardcoded `/home/xiaoquan/0007_H_AO_H` dependency with generated minimal legacy SV project files, and replaced `rm -rf` cleanup with `std::filesystem::remove_all`.
  - Added `tests/test_windows_ctest_path_escaping.ps1`; escaped host PATH in `Code/Testing/CMakeLists.txt` and regenerated the build tree.
  - Added `tests/test_windows_no_unguarded_gcc_pragmas.ps1`; guarded GCC-only diagnostic pragmas so MSVC builds are warning-clean.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: `2/2`.
  - `git diff --check` passed in both `XQ` and `Externals`.

## 2026-06-08

- Continued the requested unattended loop after the user enabled full access mode.
- Reviewed `plan.md`, `execution_log.md`, and the monolith scaffold:
  - `ApplicationContext` currently owns MITK `DataStorage`, active node, and a fire-and-forget diagnostic signal.
  - `MainWindow` currently hardcodes workflow pages directly in Presentation.
- Completed the autonomous research phase using comparable medical imaging workstations:
  - 3D Slicer: module/workflow navigation and Segment Editor-style interactive segmentation.
  - SimVascular: project-centered image-to-path-to-model-to-mesh-to-simulation pipeline.
  - MITK Workbench: DataStorage-centered rendering, segmentation, diagnostics, and toolkit structure.
  - OHIF: mode/extension registry model for workflow-specific panels and commands.
  - ITK-SNAP/MONAI Label: focused segmentation UX, semi-automatic/AI-assisted entry points, and user-visible feedback loops.
- Updated `plan.md` with the next executable phase: Monolith Workflow Foundation.
- Selected the first implementation slice:
  - Add a typed Core workflow registry covering the first-version XQ workflow pages.
  - Extend `ApplicationContext` with queryable diagnostic history.
  - Drive `MainWindow` page creation from the registry.
  - Add tests first, then implementation.
- Added failing monolith Core tests first:
  - `test_monolith_workflows` failed because `Core/xq_WorkflowRegistry.h` did not exist.
  - `test_monolith_application_context` failed because `ApplicationContext::Diagnostics()` did not exist.
- Implemented the monolith workflow foundation:
  - Added `xq::core::WorkflowDescriptor`, `WorkflowCategory`, `DefaultWorkflowRegistry()`, and `FindWorkflowById()`.
  - Added deterministic first-version workflow ids/titles for project, data, image preprocessing, path, 2D/3D segmentation, modeling, meshing, flow, ROM, multiphysics, and Python API.
  - Extended `ApplicationContext` with queryable diagnostic history while preserving `DiagnosticPosted`.
  - Ignored empty/whitespace-only diagnostics.
  - Updated `MainWindow` to create workflow pages from the Core registry instead of hardcoded UI strings.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 4/4.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Committed and pushed XQ iteration:
  - Commit: `97a123c Add monolith workflow registry foundation`.
  - Remote branch: `feature/windows-monolith-foundation`.
- Started the next unattended loop iteration:
  - Promoted Monolith Workflow Foundation to completed in `plan.md`.
  - Added Active Phase: Monolith Project Service Foundation.
  - Selected the next implementation slice: fresh `.xqproj` schema `2.0` project service owned by `ApplicationContext`.
- Added failing ProjectService regression test first:
  - `test_monolith_project_service` failed because `Core/xq_ProjectService.h` did not exist.
- Implemented the monolith project service foundation:
  - Added `xq::core::ProjectService` and `ProjectMetadata`.
  - New projects use fresh schema version `2.0`.
  - `SaveProject` writes JSON `.xqproj` files with project name and relative `workspace` directory.
  - `OpenProject` restores schema `2.0` metadata and rejects unsupported schema versions.
  - `ApplicationContext` now owns and exposes `ProjectService` through `Projects()`.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 5/5.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Prepared the next unattended loop phase in `plan.md`:
  - Promoted Monolith Project Service Foundation to completed.
  - Added Active Phase: Monolith Selection API Foundation.
- Committed and pushed XQ iteration:
  - Commit: `efd8975 Add monolith project service foundation`.
  - Remote branch: `feature/windows-monolith-foundation`.
- Started the next unattended loop iteration:
  - Added failing selection regression test first.
  - `test_monolith_selection_context` failed because `SelectionChanged` and `ClearActiveNode` did not exist on `ApplicationContext`.
- Implemented the monolith selection API foundation:
  - Added `ApplicationContext::ClearActiveNode()`.
  - Added `SelectionChanged(mitk::DataNode::Pointer)` while preserving the existing `ActiveNodeChanged()` signal.
  - Reused the existing pointer equality guard so repeated selection of the same node is a no-op.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 6/6.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Prepared the next unattended loop phase in `plan.md`:
  - Promoted Monolith Selection API Foundation to completed.
  - Added Active Phase: Monolith Task Runner Foundation.
- Committed and pushed XQ iteration:
  - Commit: `41cef64 Add monolith selection API foundation`.
  - Remote branch: `feature/windows-monolith-foundation`.
- Started the next unattended loop iteration:
  - Added failing task runner regression test first.
  - `test_monolith_task_runner` failed because `Core/xq_TaskRunner.h` did not exist.
- Implemented the monolith task runner foundation:
  - Added `xq::core::TaskRunner` and `TaskRecord`.
  - Added synchronous `RunBlocking` for deterministic first-stage task execution.
  - Added task started/finished signals and queryable task history.
  - Empty task names fail without invoking work.
  - `ApplicationContext` now owns and exposes `TaskRunner` through `Tasks()`.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 7/7.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Prepared the next phase in `plan.md`:
  - Promoted Monolith Task Runner Foundation to completed.
  - Added Next Phase: Monolith Preferences Foundation.
- Committed and pushed XQ iteration:
  - Commit: `bae1923 Add monolith task runner foundation`.
  - Remote branch: `feature/windows-monolith-foundation`.
- Started the next unattended loop iteration:
  - Promoted Monolith Preferences Foundation to active in `plan.md`.
  - Selected the next implementation slice: in-memory preferences with explicit JSON save/load, independent from legacy BlueBerry preferences.
- Added failing PreferencesService regression test first:
  - `test_monolith_preferences_service` failed because `Core/xq_PreferencesService.h` did not exist.
- Implemented the monolith preferences foundation:
  - Added `xq::core::PreferencesService`.
  - Supports string, boolean, and integer application settings.
  - Saves and loads explicit JSON independent from legacy BlueBerry preferences.
  - Invalid JSON and unsupported schema fail with useful errors.
  - `ApplicationContext` now owns and exposes `PreferencesService` through `Preferences()`.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 8/8.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Preferences Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable workstation direction: MITK/Slicer/SimVascular-style
    flows keep processing state tied to project scene/data-tree context.
  - After Image Preprocessing result activation, the next missing continuity
    point is project persistence for selected preprocessing operation and
    edited parameters.
  - Chosen next slice: save/open `WorkflowOperationService` state for Image
    Preprocessing through the fresh `.xqproj` schema.
- Added next executable phase to `plan.md`: Image Preprocessing Operation
  State Persistence.
- Started the next unattended loop iteration:
  - Adding failing project/session persistence tests for workflow operation
    state first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding `test_monolith_project_workflow_operation_persistence`.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed because
    `ProjectService::SaveProject()` and `OpenProject()` did not accept
    `WorkflowOperationService`.
- Implemented Image Preprocessing operation-state persistence:
  - `WorkflowOperationService` now exposes state snapshots, validated state
    application, and state replacement while preserving registered descriptors.
  - Project JSON writes `workflowOperations` with selected operations and
    parameter values.
  - Project open restores operation state transactionally after catalog and
    hierarchy parse succeeds.
  - `ProjectSessionService` and `ApplicationContext` now wire project save/open
    through the context `WorkflowOperationService`.
- Debugging note:
  - The first implementation segfaulted in ProjectSession tests because
    `ApplicationContext` declared `m_WorkflowOperationService` after
    `m_ProjectSessionService`, so C++ initialized it too late despite the
    initializer-list order.
  - Reordered the member declarations so ProjectSession receives an initialized
    operation service.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(project_workflow_operation_persistence|project_session_service|project_session_state_replacement|workflow_operation_service)"`
    passed: 4/4.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 60/60.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Image Preprocessing Operation State Persistence to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the persisted Image Preprocessing operation state against the
    existing Presentation wiring.
  - `MainWindow` updates operation controls on `SelectedOperationChanged`, but
    project open currently replaces operation state without emitting UI refresh
    signals.
  - Chosen next slice: refresh the Image Preprocessing page after project open
    restores operation state.
- Added next executable phase to `plan.md`: Image Preprocessing UI State
  Restore.
- Started the next unattended loop iteration:
  - Adding failing MainWindow project-open operation restore test first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed after
    extending `test_monolith_image_preprocessing_operation_page`.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_operation_page`
    failed because project open restored Core operation state without
    refreshing the existing selector.
- Implemented Image Preprocessing UI state restore:
  - `WorkflowOperationService::ReplaceStateWith()` now emits selection and
    parameter-value change signals for changed restored state.
  - Existing `MainWindow` operation-control wiring now refreshes the selector,
    action text, and parameter panel after project open.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_operation_page`
    passed: 1/1.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 65/65.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Workflow Point List Parameter Editor to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run: Workflow Operation Rich Parameter Persistence

- Completed autonomous research refresh:
  - After option parameters and point-list editing landed, the next risk is
    `.xqproj` roundtrip fidelity for non-scalar workflow operation parameters.
  - Current persistence uses JSON variants and should already handle strings
    and arrays, but option ids and point triplets were not explicitly covered.
  - Chosen next slice: add project persistence coverage for Flow Simulation
    option parameters and Image Preprocessing seed point lists.
- Added next executable phase to `plan.md`: Workflow Operation Rich Parameter
  Persistence.
- Started the next unattended loop iteration:
  - Adding failing-or-confirming persistence tests first.
- Target verification:
  - Extended `test_monolith_project_workflow_operation_persistence` to cover
    Flow Simulation option ids, Image Preprocessing seed point-list arrays,
    and invalid persisted option ids.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed after the
    test addition.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_project_workflow_operation_persistence`
    passed: 1/1.
- Implementation note:
  - No production code change was required in this slice because existing JSON
    variant persistence and `WorkflowOperationService::SetParameterValue()`
    validation already preserve these richer parameter values.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 65/65.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Workflow Operation Rich Parameter Persistence to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run: Legacy BlueBerry Include Isolation

- Completed autonomous research refresh:
  - Rechecked remaining default-build legacy surface after monolith workflow
    parameter coverage.
  - Legacy BlueBerry targets are opt-in, but `Code/CMakeLists.txt` still adds
    MITK/BlueBerry plugin include directories unconditionally in default
    monolith configuration.
  - Chosen next slice: guard BlueBerry plugin include directories behind
    `XQ_BUILD_LEGACY_BLUEBERRY`, matching the existing legacy plugin target
    guard.
- Added next executable phase to `plan.md`: Legacy BlueBerry Include
  Isolation.
- Started the next unattended loop iteration:
  - Extending the monolith scaffold PowerShell test first.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 60/60.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Image Preprocessing UI State Restore to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Image Preprocessing now has operation metadata, parameter state,
    MITK-backed execution, result activation, project persistence, and UI
    restore.
  - The next first-version workflow still only has generic placeholder
    controls: 2D/3D Segmentation.
  - Chosen next slice: add segmentation operation descriptors and reuse the
    monolith operation selector/parameter panel infrastructure.
- Added next executable phase to `plan.md`: Segmentation Operation Foundation.
- Started the next unattended loop iteration:
  - Adding failing Domain/UI tests for segmentation operation registration and
    controls first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding segmentation operation tests.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|segmentation_operation_page)"`
    failed because segmentation workflows did not register operations and
    pages did not expose selectors.
- Implemented Segmentation Operation Foundation:
  - Domain workflow registration now registers 2D Segmentation operations:
    Threshold Contour, Manual Contour, and Loft Profiles.
  - Domain workflow registration now registers 3D Segmentation operations:
    Threshold Region, Region Growing, and Surface Preview.
  - Generic workflow parameter controls now use workflow-neutral object names
    while preserving Image Preprocessing's existing object names.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|segmentation_operation_page|image_preprocessing_operation_page)"`
    passed: 3/3.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 61/61.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Segmentation Operation Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - GitHub connector initially hit a transport failure, so web search was used as fallback and the GitHub connector was retried with exact official repository queries.
  - Confirmed official comparable repositories: `Slicer/Slicer` and `OHIF/Viewers`.
  - Research conclusion: comparable medical imaging workstations converge on a central data catalog/import layer before deeper workflow pages. 3D Slicer emphasizes DICOM/data management, OHIF emphasizes data-source abstraction and DICOMweb, and SimVascular-style workflows depend on image data being cataloged before path, segmentation, modeling, meshing, and simulation steps.
- Added next executable phase to `plan.md`: Monolith Data Catalog Foundation.
- Started the next unattended loop iteration:
  - Added failing data catalog regression test first.
  - `test_monolith_data_catalog` failed because `Core/xq_DataCatalogService.h` did not exist.
- Implemented the monolith data catalog foundation:
  - Added `xq::core::DataCatalogService`, `DataCatalogEntry`, and `DataWorkflowRole`.
  - Supports ordered metadata registration, lookup by id, duplicate id rejection, and empty source-path rejection.
  - `ApplicationContext` now owns and exposes `DataCatalogService` through `DataCatalog()`.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 9/9.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Prepared the next unattended loop phase in `plan.md`:
  - Promoted Monolith Data Catalog Foundation to completed.
  - Added Active Phase: Monolith Project/Data Persistence Integration.
- Committed and pushed XQ iteration:
  - Commit: `e1dcf6e Add monolith data catalog foundation`.
  - Remote branch: `feature/windows-monolith-foundation`.
- Started the next unattended loop iteration:
  - Added failing project/data persistence regression test first.
  - `test_monolith_project_data_persistence` failed because `ProjectService` did not provide DataCatalog save/open overloads.
- Implemented project/data persistence integration:
  - Added `ProjectService::SaveProject(const DataCatalogService&, ...)`.
  - Added `ProjectService::OpenProject(..., DataCatalogService&, ...)`.
  - Persisted catalog entries under `.xqproj` schema `2.0` project JSON.
  - Restored catalog entry order and workflow role metadata.
  - Unknown workflow role values fail with an error.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 10/10.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Prepared the next unattended loop phase in `plan.md`:
  - Promoted Monolith Project/Data Persistence Integration to completed.
  - Added Active Phase: Monolith Data Import Service Foundation.
- Committed and pushed XQ iteration:
  - Commit: `276884f Persist monolith data catalog in projects`.
  - Remote branch: `feature/windows-monolith-foundation`.
- Started the next unattended loop iteration:
  - Added failing data import regression test first.
  - `test_monolith_data_import_service` failed because `Core/xq_DataImportService.h` did not exist.
- Implemented the monolith data import service foundation:
  - Added `xq::core::DataImportService`, `DataImportRequest`, and `DataImportResult`.
  - Imports are metadata-only and do not decode MITK/DICOM data yet.
  - Successful imports register entries in `DataCatalogService`.
  - Import operations run through `TaskRunner`, creating task history.
  - Missing source paths and duplicate ids fail without registering catalog data.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 11/11.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Prepared the next unattended loop phase in `plan.md`:
  - Promoted Monolith Data Import Service Foundation to completed.
  - Added Active Phase: Monolith Data Import Context Integration.
- Committed and pushed XQ iteration:
  - Commit: `a39d3e0 Add monolith data import service foundation`.
  - Remote branch: `feature/windows-monolith-foundation`.
- Started the next unattended loop iteration:
  - Added failing data import context regression test first.
  - `test_monolith_data_import_context` failed because `ApplicationContext::DataImports()` did not exist.
- Implemented data import context integration:
  - `ApplicationContext` now owns a `DataImportService`.
  - The context importer is constructed from the context-owned `DataCatalogService` and `TaskRunner`.
  - Imports through `ApplicationContext::DataImports()` register data in the shared context catalog and record task history in the shared context task runner.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 12/12.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Prepared the next unattended loop phase in `plan.md`:
  - Promoted Monolith Data Import Context Integration to completed.
  - Added Active Phase: Monolith Project Session Service Foundation.
- Committed and pushed XQ iteration:
  - Commit: `3edce80 Expose data imports through monolith context`.
  - Remote branch: `feature/windows-monolith-foundation`.
- Started the next unattended loop iteration:
  - Added failing project session regression test first.
  - `test_monolith_project_session_service` failed because `Core/xq_ProjectSessionService.h` did not exist.
- Implemented the monolith project session service foundation:
  - Added `xq::core::ProjectSessionService`.
  - The service coordinates `ProjectService`, `DataCatalogService`, and `TaskRunner`.
  - Save/open operations use the schema `2.0` project persistence path with catalog metadata.
  - Save/open operations run through `TaskRunner`, recording task history.
  - `ApplicationContext` now exposes `ProjectSession()`.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 13/13.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Project Session Service Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Reviewed comparable workstation concepts around post-import organization.
  - 3D Slicer emphasizes Data module / Subject Hierarchy style organization for scene data.
  - OHIF hanging protocols and layouts depend on organized display-set state rather than ad hoc flat imports.
  - MITK Workbench data-manager workflows reinforce the need for stable selection and data-tree state.
- Added next executable phase to `plan.md`: Monolith Data Hierarchy Foundation.
- Started the next unattended loop iteration:
  - Added failing data hierarchy regression test first.
  - `test_monolith_data_hierarchy` failed because `Core/xq_DataHierarchyService.h` did not exist.
- Implemented the monolith data hierarchy foundation:
  - Added `xq::core::DataHierarchyService`, `DataHierarchyNode`, and `DataHierarchyNodeKind`.
  - The hierarchy starts with a stable root node.
  - Folder and data-entry nodes preserve parent/child order.
  - Duplicate node ids and missing parent ids fail without mutating the hierarchy.
  - `ApplicationContext` now exposes `DataHierarchy()`.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 14/14.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Hierarchy Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable workstation direction around persisted project trees.
  - Slicer-style Subject Hierarchy and MITK Data Manager workflows both depend on
    a stable data tree being part of the project/session state, not just a flat
    import list.
  - OHIF display-set/hanging-protocol organization reinforces that downstream
    workflow views need stable structured data state after project open.
  - Chosen next slice: persist `DataHierarchyService` inside fresh `.xqproj`
    schema `2.0` and wire project session save/open through catalog + hierarchy.
- Added next executable phase to `plan.md`: Monolith Data Hierarchy Persistence
  Integration.
- Started the next unattended loop iteration:
  - Added failing hierarchy persistence regression test first.
  - `test_monolith_project_hierarchy_persistence` failed because
    `ProjectService` did not provide catalog + hierarchy save/open overloads.
- Implemented data hierarchy persistence integration:
  - Added ordered node snapshots through `DataHierarchyService::Nodes()`.
  - Added `ProjectService::SaveProject(catalog, hierarchy)` and
    `ProjectService::OpenProject(path, catalog, hierarchy)`.
  - Persisted non-root hierarchy nodes under `.xqproj` schema `2.0`
    `project.dataHierarchy`.
  - Restored folder and data-entry hierarchy nodes in order.
  - Rejected unsupported hierarchy node kind values with a useful error.
  - `ProjectSessionService` now saves/opens the context hierarchy together with
    the catalog.
- Verification for this iteration:
  - Red test observed: `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    failed because the new hierarchy persistence overloads were missing.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 15/15.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Hierarchy Persistence Integration to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable project/session behavior after hierarchy persistence.
  - Slicer/MITK/SimVascular-style workstations treat opening a project or scene
    as switching the active session state, not appending the new project tree to
    stale data from the previous session.
  - OHIF hanging protocol/display-state reset concepts reinforce that workflow
    state should be explicit and replaceable when entering a new context.
  - Chosen next slice: make monolith project open transactional for catalog and
    hierarchy state so successful opens replace the live session, while failed
    opens leave existing session state untouched.
- Added next executable phase to `plan.md`: Monolith Project Session State
  Replacement.
- Started the next unattended loop iteration:
  - Added failing project session state replacement regression test first.
  - `test_monolith_project_session_state_replacement` failed because opening a
    second project appended catalog/hierarchy state instead of replacing it.
- Implemented project session state replacement:
  - Added `DataCatalogService::ReplaceWith()`.
  - Added `DataHierarchyService::ReplaceWith()`.
  - Updated stateful `ProjectService::OpenProject()` overloads to parse into
    temporary catalog/hierarchy services first.
  - Live catalog, hierarchy, and project metadata are committed only after the
    whole project open validates.
  - Failed stateful opens leave existing catalog, hierarchy, and project
    metadata untouched.
- Verification for this iteration:
  - Red test observed: `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_project_session_state_replacement`
    failed with `second open should replace catalog entries`.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 16/16.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Project Session State Replacement to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable data-import/data-tree behavior after transactional
    project open landed.
  - Slicer-style data loading places loaded data into a subject/data hierarchy,
    MITK Workbench centers imported data in Data Manager, and SimVascular
    workflows organize imported images before downstream paths, segmentations,
    models, meshes, and simulations.
  - Chosen next slice: wire metadata-only monolith data imports into
    `DataHierarchyService` so imported entries appear in the project tree as
    deterministic role folders and data-entry nodes.
- Added next executable phase to `plan.md`: Monolith Data Import Hierarchy
  Integration.
- Started the next unattended loop iteration:
  - Added failing data import hierarchy regression test first.
  - `test_monolith_data_import_hierarchy` failed because context imports did not
    create a role folder or data-entry node in `DataHierarchyService`.
- Implemented data import hierarchy integration:
  - Added a `DataImportService` constructor that accepts `DataHierarchyService`.
  - `ApplicationContext` now wires the importer to the context hierarchy.
  - Context imports create deterministic role folders such as `images` and
    deterministic data nodes such as `data-image-001`.
  - Multiple imports reuse the role folder and preserve data-node order.
  - Import with hierarchy now uses temporary catalog/hierarchy services and
    commits both only after the whole import validates.
  - The existing catalog-only importer constructor remains available and its
    tests continue to pass.
- Verification for this iteration:
  - Red test observed: `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_import_hierarchy`
    failed with `context image import should create one role folder`.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 17/17.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Import Hierarchy Integration to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable selection behavior after imported data now lands in
    the project hierarchy.
  - Slicer/SV/MITK/OHIF-style workstation flows all need a stable current data
    selection that workflow pages can read without reconstructing tree/catalog
    relationships themselves.
  - Chosen next slice: add a monolith `DataSelectionService` that tracks the
    selected hierarchy node and resolved catalog entry independently from MITK
    `DataNode` selection.
- Added next executable phase to `plan.md`: Monolith Data Selection Service
  Foundation.
- Started the next unattended loop iteration:
  - Added failing data selection regression test first.
  - `test_monolith_data_selection_service` failed because
    `Core/xq_DataSelectionService.h` did not exist.
- Implemented the monolith data selection service foundation:
  - Added `xq::core::DataSelectionService`.
  - The service tracks selected hierarchy node id and resolved catalog entry id
    independently from MITK `DataNode` selection.
  - Selecting a hierarchy data node resolves its catalog entry id.
  - Selecting a catalog entry resolves the first hierarchy data-entry node that
    references it.
  - Missing hierarchy/catalog ids fail without mutating the current selection.
  - Re-selecting the same data is a no-op and does not emit a duplicate signal.
  - `ApplicationContext` now owns and exposes `DataSelection()`.
- Verification for this iteration:
  - Red test observed: `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    failed because `Core/xq_DataSelectionService.h` was missing.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120` passed: 18/18.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Selection Service Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable workstation lifecycle behavior after adding
    metadata-only data selection.
  - Slicer, MITK Workbench, SimVascular, and OHIF-style workstations all keep a
    clear current data/display selection so workflow pages can act on the
    newly loaded item without rediscovering tree/catalog relationships.
  - Opening a different project or scene is treated as a session switch; stale
    selected data from the previous session should not survive a successful
    state replacement.
  - Chosen next slice: wire `DataSelectionService` into successful imports and
    successful project opens while keeping failed lifecycle operations
    conservative.
- Added next executable phase to `plan.md`: Monolith Data Selection Lifecycle
  Integration.
- Started the next unattended loop iteration:
  - Adding failing data selection lifecycle regression test first.
  - Red test observed: `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_selection_lifecycle`
    failed with `successful import should select the imported catalog entry`.
- Implemented data selection lifecycle integration:
  - Added optional `DataSelectionService` lifecycle wiring to
    `DataImportService`.
  - `ApplicationContext` imports now auto-select the newly imported catalog
    entry and its resolved hierarchy node after the catalog/hierarchy commit
    succeeds.
  - Added optional `DataSelectionService` lifecycle wiring to
    `ProjectSessionService`.
  - Successful project opens now clear stale data selection after replacement
    state loads; failed opens leave the previous selection untouched.
  - Existing narrow importer/session constructors remain available for tests
    that do not need selection lifecycle behavior.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_selection_lifecycle`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 19/19.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Selection Lifecycle Integration to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable project/data tree operations after selection lifecycle
    integration landed.
  - Slicer Data/Subject Hierarchy, MITK Data Manager, SimVascular Data Manager,
    and OHIF display-set flows all treat rename/remove-style data management
    as core workflow operations rather than ad hoc UI mutations.
  - Chosen next slice: add a metadata-only monolith data management service
    that coordinates catalog, hierarchy, selection, and task history for
    rename/remove operations.
- Added next executable phase to `plan.md`: Monolith Data Management Service
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing data management regression test first.
  - Red test observed: `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    failed because `Core/xq_DataManagementService.h` was missing.
- Implemented the monolith data management service foundation:
  - Added `xq::core::DataManagementService`.
  - The service coordinates metadata-only rename/remove operations across
    `DataCatalogService`, `DataHierarchyService`, `DataSelectionService`, and
    `TaskRunner`.
  - Rename operations update catalog and hierarchy display names while
    preserving the current selection.
  - Remove operations delete the catalog entry and all hierarchy data nodes
    that reference it, and clear selection only when the removed entry was
    selected.
  - Failed rename/remove operations use temporary catalog/hierarchy services
    and leave live catalog, hierarchy, and selection state untouched.
  - `ApplicationContext` now owns and exposes `DataManagement()`.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_management_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 20/20.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Management Service Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable data-view update behavior after adding metadata-only
    data management operations.
  - OHIF-style display-set services expose added/changed/removed events,
    MITK-style workbenches observe DataStorage node changes, and
    Slicer/SimVascular-style data trees are central workflow surfaces.
  - Chosen next slice: add monolith Core change notifications for catalog and
    hierarchy services so future Project/Data UI can subscribe to state changes
    instead of polling.
- Added next executable phase to `plan.md`: Monolith Data Change Notification
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing data change notification regression test first.
  - Red test observed: `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    failed because `DataCatalogService::EntriesChanged` and
    `DataHierarchyService::NodesChanged` did not exist.
- Implemented the monolith data change notification foundation:
  - Added `DataCatalogService::EntriesChanged`.
  - Added `DataHierarchyService::NodesChanged`.
  - Successful catalog register/rename/remove/replace operations emit one
    catalog change signal.
  - Successful hierarchy add/rename/remove/replace operations emit one
    hierarchy change signal.
  - Failed data operations do not emit live data change signals.
  - Higher-level import and data management operations keep their transaction
    behavior and emit only through the final live service replacement.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_change_notifications`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 21/21.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Change Notification Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable project/data tree presentation patterns after adding
    Core data change notifications.
  - Slicer/MITK/SimVascular-style workstations present a persistent data tree
    over the loaded project data, while OHIF-style viewers keep UI panels in
    sync from display-set events.
  - Chosen next slice: add a Presentation-layer Qt hierarchy model backed by
    `DataHierarchyService` so the monolith Project/Data pages can bind to the
    real project tree instead of showing placeholders.
- Added next executable phase to `plan.md`: Monolith Data Hierarchy Qt Model
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing Qt data hierarchy model regression test first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed because
    `Presentation/xq_DataHierarchyModel.h` did not exist.
- Implemented the monolith data hierarchy Qt model foundation:
  - Added `xq::presentation::DataHierarchyModel`, a read-only
    `QAbstractItemModel` backed by `DataHierarchyService`.
  - The model exposes hierarchy display names through `Qt::DisplayRole` and
    stable node ids through `NodeIdRole`.
  - The model preserves parent/child relationships from the Core hierarchy
    service.
  - The model subscribes to `DataHierarchyService::NodesChanged` and resets
    itself after import, rename, and remove operations update the hierarchy.
  - Added `xqMonolithPresentation` as a reusable Presentation static library
    and linked `XQMonolith` through it.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_hierarchy_model`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 22/22.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Hierarchy Qt Model Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked data-panel patterns in comparable medical imaging workstations.
  - 3D Slicer subject hierarchy, MITK Data Manager, SimVascular Data Manager,
    and OHIF display-set panels all keep a persistent data tree/list visible as
    workflow context rather than hiding loaded data inside individual tools.
  - Chosen next slice: bind the new monolith `DataHierarchyModel` into
    `MainWindow` as a persistent Project/Data tree and route tree selection to
    `DataSelectionService`.
- Added next executable phase to `plan.md`: Monolith Project Data Panel
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing MainWindow data panel regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_data_panel`
    timed out before entering `main()`.
  - Root cause: `MainWindow` hard-coded `QmitkStdMultiWidget` in the
    Presentation translation unit, so a UI shell test pulled the heavy MITK
    render host into process startup before it could exercise the data-panel
    behavior.
- Implemented the monolith project data panel foundation:
  - Moved MITK render host construction from `MainWindow` to `main.cxx`.
  - Added `MainWindow::SetRenderHost(QWidget*)` and a stable
    `xqRenderHostContainer` so the application still injects the real MITK
    render widget while Presentation tests can construct the shell without
    Qmitk startup.
  - Added a persistent `xqDataHierarchyView` backed by
    `DataHierarchyModel` in the left workflow area.
  - Added stable object names for workflow navigation and page stack.
  - Routed tree selection of data-entry nodes to
    `DataSelectionService::SelectHierarchyNode`; folder selection preserves
    the current data selection.
  - Updated `test_monolith_scaffold.ps1` to protect render-host injection
    instead of requiring `MainWindow` to hard-code Qmitk.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_data_panel`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 23/23.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Project Data Panel Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked data panel selection behavior after the first Project/Data tree
    integration landed.
  - Comparable workstations keep the visible data tree/list synchronized with
    application selection state: MITK/Slicer-style data managers show the
    current data node in the tree, SimVascular keeps selected project data
    tied to workflow pages, and OHIF display-set panels track active display
    selection from shared services.
  - Chosen next slice: synchronize `DataSelectionService::SelectionChanged`
    back into the monolith tree view so imports, programmatic selection, clear,
    and remove lifecycle operations are visible in the UI.
- Added next executable phase to `plan.md`: Monolith Data Panel Selection
  Synchronization.
- Started the next unattended loop iteration:
  - Adding failing data-panel selection synchronization regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_selection_sync`
    failed with `import should select the imported data row in the tree`.
- Implemented the monolith data panel selection synchronization:
  - Added `DataHierarchyModel::IndexForNodeId` for stable tree lookup by
    hierarchy node id.
  - `MainWindow` now listens to `DataSelectionService::SelectionChanged`.
  - Core-driven selection expands the parent folder, sets the tree current
    index, and scrolls to the selected data row.
  - Clearing selection clears the tree current index.
  - Selection-model signals are blocked during Core-driven tree updates to
    avoid duplicate `DataSelectionService` calls.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_selection_sync`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 24/24.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Panel Selection Synchronization to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked data-panel command patterns after selection synchronization.
  - Comparable workstations expose common data operations, especially
    remove/delete, near the project data tree or display-set list rather than
    burying them in individual workflow pages.
  - Chosen next slice: add a minimal remove-selected-data action to the
    monolith data panel, driven by `DataSelectionService` and executed through
    `DataManagementService`.
- Added next executable phase to `plan.md`: Monolith Data Panel Actions
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing data-panel action regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_data_actions`
    failed with `MainWindow should expose a remove data action`.
- Implemented the monolith data panel actions foundation:
  - Added `xqDataPanelToolbar` and `xqRemoveDataAction`.
  - Bound remove action enabled state to `DataSelectionService`.
  - Triggering remove calls `DataManagementService::RemoveEntry` for the
    selected catalog entry.
  - Successful removal updates catalog, hierarchy, selection, tree, action
    state, and diagnostics through existing services.
  - Forced stale action trigger posts a diagnostic without crashing.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_data_actions`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 25/25.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Panel Actions Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked project/session lifecycle needs after adding the first data-panel
    command surface.
  - Comparable workstations update project/scene/window state from lifecycle
    events instead of polling active project metadata from every UI surface.
  - Chosen next slice: add `ProjectService` project-change notifications so
    future monolith Project UI and window chrome can track create/open state
    reliably.
- Added next executable phase to `plan.md`: Monolith Project State
  Notification Foundation.
- Started the next unattended loop iteration:
  - Adding failing project-state notification regression test first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed because
    `ProjectService::ProjectChanged` did not exist.
- Implemented the monolith project state notification foundation:
  - Added `ProjectService::ProjectChanged(const ProjectMetadata&)`.
  - Successful `CreateProject` and all successful `OpenProject` overloads now
    emit one project-change signal with current metadata.
  - Failed create/open operations do not emit and failed open preserves the
    previous current project.
  - `SaveProject` remains conservative and emits no project-change signal.
  - `ProjectSessionService::Open` emits exactly one project change through its
    successful underlying `ProjectService::OpenProject` call.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_project_state_notifications`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 26/26.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Project State Notification Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked project/window-state behavior after adding `ProjectChanged`.
  - Comparable workstation shells keep the active scene/project visible in
    window chrome or persistent status surfaces, so users can tell which study
    or project workflow pages are acting on.
  - Chosen next slice: connect `ProjectService::ProjectChanged` to
    `MainWindow` title/status state without adding file dialogs yet.
- Added next executable phase to `plan.md`: Monolith Project Window State
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing project-window-state regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_project_state`
    failed with `MainWindow should expose a project status bar`.
- Implemented the monolith project window state foundation:
  - Added stable `xqProjectStatusBar` to `MainWindow`.
  - New windows show base title `XQ` and `No project` status.
  - `MainWindow` listens to `ProjectService::ProjectChanged`.
  - Successful project create/open updates title to `XQ - <project>` and
    status to include project name and project file path.
  - Save and failed open leave the last successful project window state
    unchanged.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_project_state`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 27/27.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Project Window State Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked project command surfaces after wiring project window state.
  - Comparable workstation shells expose a central Save Project/Save Scene
    command once a project or scene is active.
  - Chosen next slice: add a MainWindow save-current-project action that calls
    `ProjectSessionService::Save` and writes current catalog/hierarchy state
    without introducing file dialogs yet.
- Added next executable phase to `plan.md`: Monolith Project Save Action
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing project-save-action regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_project_save_action`
    failed with `MainWindow should expose a save project action`.
- Implemented the monolith project save action foundation:
  - Added `xqProjectToolbar` and `xqSaveProjectAction`.
  - Save action starts disabled and enables after successful project
    create/open through `ProjectChanged`.
  - Triggering save calls `ProjectSessionService::Save`.
  - Successful save writes `.xqproj` project, data catalog, and hierarchy
    metadata.
  - Forced stale save without an active project posts a diagnostic and
    disables the action.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_project_save_action`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 28/28.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Project Save Action Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked diagnostic/task feedback patterns after adding project save.
  - Comparable workstation shells provide a persistent operation log,
    notification service, or error/status panel so task results are visible
    outside the tool that started them.
  - Chosen next slice: bridge `TaskRunner::TaskFinished` into
    `ApplicationContext` diagnostics so imports, saves, opens, and future
    workflow jobs surface through one diagnostic channel.
- Added next executable phase to `plan.md`: Monolith Task Diagnostics Bridge
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing task-diagnostics bridge regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_task_diagnostics_bridge`
    failed with `successful import should post one task diagnostic`.
- Implemented the monolith task diagnostics bridge foundation:
  - `ApplicationContext` now listens to `TaskRunner::TaskFinished`.
  - Successful tasks post diagnostics containing task name and message.
  - Failed tasks post diagnostics containing task name, failure state, and
    message.
  - Empty task messages still produce a useful `<task> succeeded.` diagnostic.
  - All task diagnostics flow through existing `PostDiagnostic`.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_task_diagnostics_bridge`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 29/29.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Task Diagnostics Bridge Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked diagnostic-log UI patterns after task diagnostics were bridged
    into `ApplicationContext`.
  - Comparable workstation shells expose persistent logs, notification panes,
    or error/status views that make background operation results inspectable
    after the initiating command completes.
  - Chosen next slice: make the monolith diagnostics text log a stable
    Presentation surface and verify task diagnostics appear there through the
    existing `DiagnosticPosted` signal.
- Added next executable phase to `plan.md`: Monolith Diagnostics Panel Task
  Log Foundation.
- Started the next unattended loop iteration:
  - Adding failing diagnostics-panel regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_diagnostics_log`
    failed with `MainWindow should expose a diagnostics log`.
- Implemented the monolith diagnostics panel task log foundation:
  - Added stable object name `xqDiagnosticsLog` to the read-only diagnostics
    text log.
  - Existing `ApplicationContext::DiagnosticPosted` binding now has a stable
    UI surface for tests and future controls.
  - Manual diagnostics and task diagnostics from import append to the visible
    log through the same signal.
  - Empty diagnostics remain absent from the visible log.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_diagnostics_log`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 30/30.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Diagnostics Panel Task Log Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked workflow/module navigation patterns after making diagnostics
    visible.
  - Comparable workstations expose stable module, view, or workflow identities
    behind their navigation surfaces so individual pages can be filled in
    incrementally without losing routing semantics.
  - Chosen next slice: bind `DefaultWorkflowRegistry()` ids into MainWindow
    navigation items and stacked page object names.
- Added next executable phase to `plan.md`: Monolith Workflow Page Identity
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing workflow-page identity regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_workflow_identity`
    failed with `workflow navigation item should store registry id`.
- Implemented the monolith workflow page identity foundation:
  - Navigation items now store `WorkflowDescriptor::Id` in `Qt::UserRole`.
  - Workflow pages now expose object names
    `xqWorkflowPage_<workflow-id>`.
  - Visible workflow titles and registry ordering are preserved.
  - Navigation row changes continue to switch to the matching stacked page.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_workflow_identity`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 31/31.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Workflow Page Identity Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked workflow page composition after binding workflow ids to
    navigation/page surfaces.
  - Comparable workstation project/workflow shells expose an active project or
    scene overview as a stable surface before deeper tool-specific controls
    are added.
  - Chosen next slice: make the `project` workflow page display current project
    metadata and data count from Core services.
- Added next executable phase to `plan.md`: Monolith Project Workflow Page
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing project workflow page regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_project_workflow_page`
    failed with `project page should expose a project name label`.
- Implemented the monolith project workflow page foundation:
  - Added stable project overview labels to `xqWorkflowPage_project`:
    `xqProjectPageName`, `xqProjectPagePath`, `xqProjectPageSchema`, and
    `xqProjectPageDataCount`.
  - New windows show neutral no-project metadata.
  - Successful project create/open updates project page metadata through
    `ProjectChanged`.
  - Data import/remove updates the data item count through
    `DataCatalogService::EntriesChanged`.
  - Failed open leaves the last successful project page metadata unchanged.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_project_workflow_page`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 32/32.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Project Workflow Page Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked selected-data detail patterns after the project workflow page
    landed.
  - Comparable workstation Data views keep the currently selected image/model
    metadata visible near the data tree so workflow pages know what object they
    act on.
  - Chosen next slice: make the `data` workflow page show current selected
    catalog entry id, display name, and source path from Core selection.
- Added next executable phase to `plan.md`: Monolith Data Workflow Page
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing data workflow page regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_workflow_page`
    failed with `data page should expose a selection label`.
- Implemented the monolith data workflow page foundation:
  - Added stable selected-data labels to `xqWorkflowPage_data`:
    `xqDataPageSelection`, `xqDataPageCatalogId`,
    `xqDataPageDisplayName`, and `xqDataPageSourcePath`.
  - New windows show neutral no-selection metadata.
  - Successful import updates selected data metadata through
    `DataSelectionService::SelectionChanged`.
  - Rename/remove refresh the selected data overview through catalog and
    selection changes.
  - Removed or stale selection returns the page to no-selection metadata.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_workflow_page`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 33/33.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Workflow Page Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked module/workflow context patterns in comparable workstation
    shells after adding concrete project and data workflow pages.
  - 3D Slicer organizes tools around a selected module in the module panel,
    MITK Workbench uses active views/perspectives to coordinate the workbench,
    and OHIF modes compose services/extensions around the active viewer
    workflow.
  - Chosen next slice: add a Core workflow selection service and bind
    MainWindow navigation/page state to it, so future workflow pages can
    consume one application-level active workflow context.
- Added next executable phase to `plan.md`: Monolith Workflow Selection
  Context Foundation.
- Started the next unattended loop iteration:
  - Adding failing workflow-selection service and MainWindow navigation
    regression tests first.
- Red test observed:
  - Initial direct `cmake --build` attempt showed the shell does not expose
    bare `cmake`; this was not accepted as RED.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling the new workflow-selection tests with
    `Cannot open include file: 'Core/xq_WorkflowSelectionService.h'`, proving
    the new service interface is absent.
- Implemented the monolith workflow selection context foundation:
  - Added `WorkflowSelectionService` with registry-backed workflow id
    validation.
  - Default selection now starts at the first `DefaultWorkflowRegistry()` id.
  - Valid changes update selected id and emit `WorkflowChanged` once.
  - Re-selecting the same valid workflow is a no-op with no signal.
  - Invalid or empty workflow ids fail without mutation or signal.
  - `ApplicationContext` now exposes the service through
    `WorkflowSelection()`.
  - MainWindow navigation changes update Core selected workflow id, and
    programmatic Core workflow selection updates navigation/page state.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 test -ExternalsRoot ..\Externals` passed:
    35/35.
  - PowerShell test loop initially reused stale `$LASTEXITCODE`; root cause
    was that repository `.ps1` tests signal failure through `throw`, so the
    loop was rerun with exception handling.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 35/35.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Workflow Selection Context Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked data-context patterns after adding Core workflow selection.
  - Comparable workstation shells keep the active module/workflow connected to
    the currently selected data object or display set, so tools can decide
    whether the current input is actionable before exposing deeper controls.
  - Chosen next slice: add a Core workflow data context service that combines
    active workflow selection with current data selection and catalog workflow
    roles.
- Added next executable phase to `plan.md`: Monolith Workflow Data Context
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing workflow data context regression test first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling the new workflow-context test with
    `Cannot open include file: 'Core/xq_WorkflowContextService.h'`, proving
    the new context service interface is absent.
- Implemented the monolith workflow data context foundation:
  - Added `WorkflowContextService` and `WorkflowContextSnapshot`.
  - The service combines active workflow selection, selected catalog entry,
    and catalog workflow role metadata.
  - Added conservative accepted data roles for image preprocessing, path,
    segmentation, modeling, meshing, simulation, and no-data workflows.
  - `ContextChanged()` now emits only when the derived snapshot changes.
  - `ApplicationContext` now exposes the service through
    `WorkflowContext()`.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 36/36.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Workflow Data Context Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked UI-facing workflow context patterns now that Core can derive
    workflow/data compatibility.
  - Comparable workstation shells surface current input readiness inside the
    active module/page before exposing tool-specific controls, so users see
    whether the selected data can drive the current workflow.
  - Chosen next slice: add stable workflow context status labels to
    data-dependent workflow pages and bind them to `WorkflowContextService`.
- Added next executable phase to `plan.md`: Monolith Workflow Context Status
  Page Foundation.
- Started the next unattended loop iteration:
  - Adding failing workflow context status page regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_workflow_context_status_page`
    failed with `data-dependent workflow page should expose a context status label`.
- Implemented the monolith workflow context status page foundation:
  - Added stable `xqWorkflowContextStatus_<workflow-id>` labels to
    data-dependent workflow pages.
  - Bound status text to `WorkflowContextService::ContextChanged()`.
  - Missing input, incompatible input, compatible selected data, and selected
    data rename changes now update the active workflow page status.
  - Kept this slice display-only with no workflow action buttons or algorithm
    migration.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_workflow_context_status_page`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 37/37.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Workflow Context Status Page Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked workflow page readiness after status labels landed.
  - Comparable workstation modules pair input-readiness feedback with a stable
    command surface, so the next migration step can replace placeholder
    commands with real domain services without changing page routing.
  - Chosen next slice: add stable primary action buttons to data-dependent
    workflow pages, bind enablement to `WorkflowContextService`, and route
    placeholder triggers through diagnostics.
- Added next executable phase to `plan.md`: Monolith Workflow Primary Action
  Surface Foundation.
- Started the next unattended loop iteration:
  - Adding failing workflow primary action page regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_workflow_primary_action_page`
    failed with `data-dependent workflow page should expose a primary action button`.
- Implemented the monolith workflow primary action surface foundation:
  - Added stable `xqWorkflowPrimaryAction_<workflow-id>` buttons to
    data-dependent workflow pages.
  - Button enablement now follows `WorkflowContextService` compatibility for
    the active workflow.
  - Inactive workflow buttons are disabled on context refresh to avoid stale
    enabled states.
  - Triggering an enabled placeholder action posts a diagnostic containing the
    workflow title and selected data display name.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_workflow_primary_action_page`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 38/38.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Workflow Primary Action Surface Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the primary action surface after buttons and placeholder
    diagnostics landed.
  - The UI still owns placeholder message construction, which would make later
    algorithm migration noisier.
  - Chosen next slice: introduce a Core `WorkflowActionService` so MainWindow
    primary action clicks request workflow execution through Core.
- Added next executable phase to `plan.md`: Monolith Workflow Action Service
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing workflow action service regression test first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling the new workflow-action test with
    `Cannot open include file: 'Core/xq_WorkflowActionService.h'`, proving
    the Core action service interface is absent.
- Implemented the monolith workflow action service foundation:
  - Added `WorkflowActionService` with
    `RequestActiveWorkflowAction(QString*)`.
  - The service consumes `WorkflowContextService` and centralizes compatible,
    incompatible, and no-data placeholder action messages.
  - `ApplicationContext` now exposes the service through
    `WorkflowActions()`.
  - MainWindow primary action clicks now route through Core while preserving
    the visible diagnostics contract.
- Verification for this iteration:
  - Red/green target tests:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_workflow_(action_service|primary_action_page)"`
    passed after implementation: 2/2.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 39/39.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Workflow Action Service Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked workflow action execution after extracting request messages into
    Core.
  - Successful placeholder actions still bypass `TaskRunner`, unlike imports,
    saves, and other Core operations.
  - Chosen next slice: add a task-running workflow action API so successful
    workflow clicks participate in task history and the existing task
    diagnostics bridge.
- Added next executable phase to `plan.md`: Monolith Workflow Action Task
  Runner Foundation.
- Started the next unattended loop iteration:
  - Adding failing workflow action task-runner regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_workflow_action_service` because
    `WorkflowActionService` had no `RunActiveWorkflowAction` member.
- Implemented the monolith workflow action task runner foundation:
  - Added `WorkflowActionService::RunActiveWorkflowAction`.
  - Compatible placeholder actions now create a `Run <WorkflowTitle>` task
    through `TaskRunner`.
  - Rejected workflow actions keep task history unchanged and return the same
    rejection message.
  - MainWindow now calls the task-running API and relies on the existing task
    diagnostics bridge for successful action diagnostics.
- Verification for this iteration:
  - Red/green target tests:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_workflow_(action_service|primary_action_page)"`
    passed after implementation: 2/2.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 39/39.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Workflow Action Task Runner Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked operation feedback after workflow actions started using
    `TaskRunner`.
  - Comparable workstation shells keep an inspectable history/log surface for
    operation outcomes, not only transient button state.
  - Chosen next slice: add a stable MainWindow task history table bound to
    `TaskRunner::History()` and `TaskRunner::TaskFinished`.
- Added next executable phase to `plan.md`: Monolith Task History Panel
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing task history panel regression test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_task_history_panel`
    failed with `MainWindow should expose a task history table`.
- Implemented the monolith task history panel foundation:
  - Added `xqTaskHistoryDock` and read-only `xqTaskHistoryTable`.
  - The table exposes `Task`, `Status`, and `Message` columns.
  - Existing `TaskRunner::History()` entries are rendered on window creation.
  - `TaskRunner::TaskFinished` appends one row per finished task.
  - Import, duplicate import failure, and workflow action task rows now appear
    in the task history panel.
  - Diagnostics log behavior remains unchanged.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_task_history_panel`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 40/40.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Task History Panel Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked workflow action execution after task history landed.
  - The action service now has a stable task surface, but still cannot delegate
    to per-workflow implementations.
  - Chosen next slice: add per-workflow handler registration and dispatch in
    `WorkflowActionService`, preserving placeholder behavior when no handler is
    registered.
- Added next executable phase to `plan.md`: Monolith Workflow Action Handler
  Dispatcher Foundation.
- Started the next unattended loop iteration:
  - Adding failing workflow action handler dispatch regression test first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_workflow_action_service` because
    `WorkflowActionService` had no handler registration/query API.
- Implemented the monolith workflow action handler dispatcher foundation:
  - Added `WorkflowActionService::WorkflowActionHandler`.
  - Added validated `RegisterHandler` and `HasHandler`.
  - `RunActiveWorkflowAction` now dispatches to a registered handler for the
    active workflow when one exists.
  - Workflows without handlers preserve placeholder task behavior.
  - Handler-provided task messages are stored in task history.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_workflow_action_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 40/40.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Workflow Action Handler Dispatcher Foundation to completed
  in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked monolith layering after adding handler dispatch in Core.
  - The next architectural gap is a Domain-level registration point so real
    workflow services can attach without editing Presentation.
  - Chosen next slice: add `xqMonolithDomain` and default workflow action
    handler registration for data-dependent workflows.
- Added next executable phase to `plan.md`: Monolith Domain Workflow Handler
  Registrar Foundation.
- Started the next unattended loop iteration:
  - Adding failing domain workflow handler registrar regression test first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_domain_workflow_action_handlers` with
    `Cannot open include file: 'Domain/xq_WorkflowActionHandlers.h'`.
- Implemented the monolith domain workflow handler registrar foundation:
  - Added `xqMonolithDomain` as a static library depending on
    `xqMonolithCore`.
  - Added `xq::domain::RegisterDefaultWorkflowActionHandlers`.
  - Registered default handlers for image preprocessing, path, 2D/3D
    segmentation, modeling, meshing, flow, ROM, and multiphysics workflows.
  - Left project, data, and Python API workflows without domain handlers in
    this slice.
  - Wired `XQMonolith` bootstrap to register default domain handlers after
    creating `ApplicationContext`.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_domain_workflow_action_handlers`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 41/41.
- Promoted Monolith Domain Workflow Handler Registrar Foundation to completed
  in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable workstation docs:
    3D Slicer Segment Editor keeps image/segmentation operations as selectable
    effects, MITK viewer examples center DataStorage-backed data/render
    integration, OHIF modes compose workflow-specific extensions and commands,
    and SimVascular keeps the vascular pipeline staged from image data toward
    path/model/mesh/simulation.
  - Rechecked local old image-processing utilities:
    `xq_ImageProcessingUtils` already contains thresholding, connected
    thresholding, smoothing, morphology, crop, resample, and marching-cubes
    primitives.
  - Chosen next slice: introduce a metadata-only Domain image-preprocessing
    workflow service and route the registrar's image-preprocessing handler
    through it before moving old algorithms.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing Domain
  Service Foundation.
- Started the next unattended loop iteration:
  - Adding failing image-preprocessing domain service regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_workflow_service` with
    `Cannot open include file: 'Domain/xq_ImagePreprocessingWorkflowService.h'`.
- Implemented the monolith image-preprocessing domain service foundation:
  - Added `ImagePreprocessingWorkflowService` with typed result fields for
    success, source catalog entry id, selected data display name, and message.
  - Added validation for the `image-preprocessing` workflow id.
  - Added validation for selected image-preprocessing-compatible data roles:
    `DICOMSeries` and `Image`.
  - Preserved the existing successful task message:
    `Image Preprocessing domain workflow accepted <data>.`
  - Routed the registrar's `image-preprocessing` handler through the new
    domain service while leaving other data-dependent workflows on the generic
    domain placeholder.
- Verification for this iteration:
  - Red/green target tests:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(image_preprocessing_workflow_service|domain_workflow_action_handlers)"`
    passed after implementation: 2/2.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 42/42.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Domain Service Foundation to completed
  in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked Slicer Segment Editor-style effect organization and the local
    `xq_ImageProcessingUtils` operation surface.
  - The image-preprocessing service now has a workflow entry point but no
    stable catalog of available preprocessing operations for future UI and
    execution wiring.
  - Chosen next slice: expose a metadata-only Domain image-preprocessing
    operation catalog with stable ids and lookup.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Operation Catalog Foundation.
- Started the next unattended loop iteration:
  - Adding failing image-preprocessing operation catalog regression tests
    first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_workflow_service` because
    `ImagePreprocessingWorkflowService` had no `Operations` or
    `FindOperation` members.
- Implemented the monolith image-preprocessing operation catalog foundation:
  - Added `ImagePreprocessingOperationDescriptor`.
  - Added deterministic `ImagePreprocessingWorkflowService::Operations()`.
  - Added `FindOperation(operationId)` with trimmed id lookup.
  - Covered six metadata-only preprocessing operations: binary threshold,
    connected threshold, Gaussian smoothing, morphology open/close, crop, and
    resample.
  - Left marching cubes out of this preprocessing catalog because it produces
    surface/model output.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_workflow_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 42/42.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Operation Catalog Foundation to
  completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the operation catalog against Slicer-style effect execution and
    the next local integration step.
  - The service now lists available preprocessing operations but cannot yet
    validate a concrete operation request.
  - Chosen next slice: add metadata-only operation request execution that
    validates operation ids and selected data before any old algorithm calls.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Operation Request Foundation.
- Started the next unattended loop iteration:
  - Adding failing image-preprocessing operation request regression tests
    first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_workflow_service` because
    `ImagePreprocessingWorkflowService` had no `RunOperation` member and the
    result type had no operation metadata fields.
- Implemented the monolith image-preprocessing operation request foundation:
  - Added operation id and title fields to
    `ImagePreprocessingWorkflowResult`.
  - Added `RunOperation(snapshot, operationId)`.
  - Reused existing snapshot validation before operation success.
  - Validated operation ids through the operation catalog with trimmed lookup.
  - Kept `Run(snapshot)` behavior and message stable.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_workflow_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 42/42.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Operation Request Foundation to
  completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked old preprocessing utility signatures after operation request
    validation landed.
  - The next missing contract is per-operation parameter metadata, so UI and
    algorithm execution can share one ordered schema.
  - Chosen next slice: add metadata-only image-preprocessing operation
    parameter descriptors.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Operation Parameter Schema.
- Started the next unattended loop iteration:
  - Adding failing image-preprocessing parameter schema regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_workflow_service` because
    `ImagePreprocessingParameterDescriptor`,
    `ImagePreprocessingParameterValueType`, and operation `Parameters` did not
    exist.
- Implemented the monolith image-preprocessing operation parameter schema:
  - Added `ImagePreprocessingParameterValueType`.
  - Added `ImagePreprocessingParameterDescriptor`.
  - Added ordered parameter metadata to each preprocessing operation
    descriptor.
  - Covered binary threshold, connected threshold, Gaussian smoothing,
    morphology open/close, crop, and resample parameter contracts.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_workflow_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 42/42.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Operation Parameter Schema to
  completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked parameter schema after the descriptor catalog landed.
  - The next useful boundary is metadata-only parameter validation so future UI
    and algorithm execution can reject malformed requests consistently.
  - Chosen next slice: add `QVariantMap` parameter validation for known image
    preprocessing operations.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Parameter Validation Foundation.
- Started the next unattended loop iteration:
  - Adding failing image-preprocessing parameter validation regression tests
    first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_workflow_service` because
    `ImagePreprocessingWorkflowService` had no
    `ValidateOperationParameters` member.
- Implemented the monolith image-preprocessing parameter validation foundation:
  - Added `ImagePreprocessingParameterValidationResult`.
  - Added `ValidateOperationParameters(operationId, QVariantMap)`.
  - Validated unknown operation ids with the existing operation-not-found
    message.
  - Validated required parameters, numeric scalar values, integer scalar
    values, and integer point-list seed values.
  - Kept this slice metadata-only without old algorithm calls or data mutation.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_workflow_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 42/42.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Parameter Validation Foundation to
  completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable workflow/tool models around parameterized effects and
    command requests.
  - The image-preprocessing service now has parameter schema plus validation,
    but operation execution requests cannot yet carry those parameters.
  - Chosen next slice: add metadata-only parameterized operation requests that
    validate parameters before accepting an operation.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Parameterized Operation Request.
- Started the next unattended loop iteration:
  - Adding failing parameterized image-preprocessing operation request tests
    first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_workflow_service` because
    `ImagePreprocessingWorkflowService::RunOperation` did not accept a
    `QVariantMap` parameter argument.
- Implemented the monolith image-preprocessing parameterized operation request:
  - Added `RunOperation(snapshot, operationId, QVariantMap parameters)`.
  - Reused existing snapshot validation.
  - Reused `ValidateOperationParameters` before accepting the operation.
  - Preserved existing metadata-only `RunOperation(snapshot, operationId)`
    behavior.
  - Kept this slice metadata-only without old algorithm calls or data mutation.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_workflow_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 42/42.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Parameterized Operation Request to
  completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the local legacy image-processing module and monolith layering.
  - The next useful bridge is not another Domain metadata slice, but an
    Infrastructure adapter that can call a legacy image-processing algorithm
    while keeping Domain independent from legacy modules.
  - Chosen next slice: add `xqMonolithInfrastructure` and a Gaussian smoothing
    adapter backed by `xq_ImageProcessingUtils::SmoothGaussian`.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Infrastructure Adapter Foundation.
- Started the next unattended loop iteration:
  - Adding failing image-preprocessing infrastructure adapter regression tests
    first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_algorithm_adapter` with
    `Cannot open include file: 'Infrastructure/xq_ImagePreprocessingAlgorithmAdapter.h'`.
- Implemented the monolith image-preprocessing infrastructure adapter
  foundation:
  - Added `xqMonolithInfrastructure`.
  - Added namespace `xq::infrastructure`.
  - Added `ImagePreprocessingAlgorithmAdapter`.
  - Linked Infrastructure to `xqMonolithDomain` and legacy
    `xqModuleImageProcessing`, while leaving Domain independent from legacy
    algorithm modules.
  - Added Gaussian smoothing execution through
    `xq_ImageProcessingUtils::SmoothGaussian`.
  - Reused Domain parameter validation before invoking the algorithm.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_algorithm_adapter`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 43/43.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Infrastructure Adapter Foundation to
  completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked legacy image-processing primitives after the first Gaussian
    smoothing Infrastructure adapter landed.
  - Binary threshold is the next low-risk operation because it uses scalar
    parameters already covered by the Domain schema and returns a VTK image.
  - Chosen next slice: add binary-threshold execution to the same
    Infrastructure adapter.
- Added next executable phase to `plan.md`: Monolith Binary Threshold
  Infrastructure Adapter.
- Started the next unattended loop iteration:
  - Adding failing binary-threshold adapter regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_algorithm_adapter` because
    `ImagePreprocessingAlgorithmAdapter` had no `RunBinaryThreshold` member.
- Implemented the monolith binary-threshold infrastructure adapter:
  - Added `RunBinaryThreshold(vtkImageData*, QVariantMap parameters)`.
  - Reused Domain validation for `binary-threshold` parameters.
  - Delegated valid requests to `xq_ImageProcessingUtils::BinaryThreshold`.
  - Preserved layer separation: Domain still does not link legacy algorithm
    modules.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_algorithm_adapter`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 43/43.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Binary Threshold Infrastructure Adapter to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked legacy connected-threshold implementation after binary threshold
    adapter coverage landed.
  - Connected threshold is the next useful slice because it exercises the
    integer point-list parameter schema and seed conversion into the legacy
    algorithm call.
  - Chosen next slice: add connected-threshold execution to the Infrastructure
    adapter.
- Added next executable phase to `plan.md`: Monolith Connected Threshold
  Infrastructure Adapter.
- Started the next unattended loop iteration:
  - Adding failing connected-threshold adapter regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_algorithm_adapter` because
    `ImagePreprocessingAlgorithmAdapter` had no `RunConnectedThreshold`
    member.
- Implemented the monolith connected-threshold infrastructure adapter:
  - Added `RunConnectedThreshold(vtkImageData*, QVariantMap parameters)`.
  - Reused Domain validation for `connected-threshold` parameters.
  - Converted validated `QVariantList` seed points into
    `std::vector<std::array<int, 3>>`.
  - Delegated valid requests to `xq_ImageProcessingUtils::ConnectedThreshold`.
  - Preserved layer separation: Domain still does not link legacy algorithm
    modules.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_algorithm_adapter`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 43/43.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Connected Threshold Infrastructure Adapter to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable preprocessing tools and legacy
    `MorphologicalOpenClose` after connected-threshold seed conversion landed.
  - Morphology open/close is the next low-risk adapter because it exercises the
    integer radius schema and returns a VTK image without data-store mutation.
  - Chosen next slice: add morphology open/close execution to the Infrastructure
    adapter.
- Added next executable phase to `plan.md`: Monolith Morphology
  Infrastructure Adapter.
- Started the next unattended loop iteration:
  - Adding failing morphology adapter regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_algorithm_adapter` because
    `ImagePreprocessingAlgorithmAdapter` had no `RunMorphologyOpenClose`
    member.
- Implemented the monolith morphology infrastructure adapter:
  - Added `RunMorphologyOpenClose(vtkImageData*, QVariantMap parameters)`.
  - Reused Domain validation for `morphology-open-close` parameters.
  - Delegated valid requests to
    `xq_ImageProcessingUtils::MorphologicalOpenClose`.
  - Preserved layer separation: Domain still does not link legacy algorithm
    modules.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_algorithm_adapter`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 43/43.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Morphology Infrastructure Adapter to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked legacy crop implementation after morphology coverage landed.
  - Crop is the next useful adapter because it exercises six integer parameters
    and returns a VTK image with changed dimensions.
  - Chosen next slice: add crop execution to the Infrastructure adapter.
- Added next executable phase to `plan.md`: Monolith Crop Infrastructure
  Adapter.
- Started the next unattended loop iteration:
  - Adding failing crop adapter regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_algorithm_adapter` because
    `ImagePreprocessingAlgorithmAdapter` had no `RunCrop` member.
- Implemented the monolith crop infrastructure adapter:
  - Added `RunCrop(vtkImageData*, QVariantMap parameters)`.
  - Reused Domain validation for `crop` parameters.
  - Delegated valid requests to `xq_ImageProcessingUtils::Crop`.
  - Preserved layer separation: Domain still does not link legacy algorithm
    modules.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 43/43.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Crop Infrastructure Adapter to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked image preprocessing practices in comparable workstation
    documentation and the local legacy resample implementation.
  - Resample is the next useful adapter because it exercises physical spacing
    parameters and changes both image spacing and output dimensions.
  - Chosen next slice: add resample execution to the Infrastructure adapter.
- Added next executable phase to `plan.md`: Monolith Resample Infrastructure
  Adapter.
- Started the next unattended loop iteration:
  - Adding failing resample adapter regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_algorithm_adapter` because
    `ImagePreprocessingAlgorithmAdapter` had no `RunResample` member.
- Implemented the monolith resample infrastructure adapter:
  - Added `RunResample(vtkImageData*, QVariantMap parameters)`.
  - Reused Domain validation for `resample` parameters.
  - Delegated valid requests to `xq_ImageProcessingUtils::Resample`.
  - Preserved layer separation: Domain still does not link legacy algorithm
    modules.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_algorithm_adapter`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 43/43.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Resample Infrastructure Adapter to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the completed preprocessing adapter coverage after resample
    landed.
  - All six Domain preprocessing operation ids now have per-operation
    Infrastructure adapter methods, but there is no unified operation-id
    execution entry point yet.
  - Chosen next slice: add operation dispatch to the Infrastructure adapter so
    future UI and task services do not duplicate algorithm switches.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Operation Dispatch Adapter.
- Started the next unattended loop iteration:
  - Adding failing operation dispatch regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_algorithm_adapter` because
    `ImagePreprocessingAlgorithmAdapter` had no `RunOperation` member.
- Implemented the monolith image-preprocessing operation dispatch adapter:
  - Added `RunOperation(QString operationId, vtkImageData*, QVariantMap
    parameters)`.
  - Reused the Domain operation catalog to reject unknown operation ids with
    the existing operation-not-found diagnostic.
  - Routed all six current Domain preprocessing operation ids to their
    existing Infrastructure adapter implementations.
  - Preserved layer separation: Domain still does not link legacy algorithm
    modules.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_algorithm_adapter`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 43/43.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Operation Dispatch Adapter to completed
  in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the operation dispatch boundary after all preprocessing operation
    ids were routed through the Infrastructure adapter.
  - The next integration gap is a service-level execution boundary that combines
    Domain workflow/parameter validation with Infrastructure algorithm
    execution without mutating storage.
  - Chosen next slice: add a monolith image-preprocessing execution service in
    Infrastructure.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Execution Service.
- Started the next unattended loop iteration:
  - Adding failing execution service regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_execution_service` because
    `Infrastructure/xq_ImagePreprocessingExecutionService.h` did not exist.
- Implemented the monolith image-preprocessing execution service:
  - Added `ImagePreprocessingExecutionRequest` and
    `ImagePreprocessingExecutionResult`.
  - Added `ImagePreprocessingExecutionService::Run`.
  - Reused Domain workflow and parameter validation before algorithm
    execution.
  - Reused the Infrastructure algorithm adapter operation dispatcher for
    actual VTK image processing.
  - Preserved layer separation: Domain still does not link legacy algorithm
    modules, and this slice does not mutate DataStorage or catalog state.
- Debugging note:
  - The first target test run segfaulted because the test stored a raw
    `vtkImageData*` from a temporary `vtkSmartPointer`; the test now keeps the
    smart pointer alive for the request lifetime.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_execution_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 44/44.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Execution Service to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the legacy preprocessing view's image input/output path after
    the execution service landed.
  - The legacy view extracts `vtkImageData*` from `mitk::Image` and deep-copies
    algorithm output back into a new `mitk::Image`.
  - Chosen next slice: add a monolith MITK/VTK image boundary adapter in
    Infrastructure.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing MITK
  Image Adapter.
- Started the next unattended loop iteration:
  - Adding failing MITK image adapter regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_mitk_image_adapter` because
    `Infrastructure/xq_ImagePreprocessingMitkImageAdapter.h` did not exist.
- Implemented the monolith MITK image adapter:
  - Added structured extraction of `vtkImageData*` from a selected
    `mitk::DataNode`.
  - Added structured creation of a deep-copied `mitk::Image` from algorithm
    output `vtkImageData`.
  - Reused legacy diagnostics for unusable selected image nodes.
  - Preserved layer separation: Domain still does not know MITK/VTK conversion
    details, and this slice does not mutate DataStorage or catalog state.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_mitk_image_adapter`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 45/45.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing MITK Image Adapter to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the legacy preprocessing view's result node naming and metadata
    after the MITK image adapter landed.
  - The next integration gap is creating a result `mitk::DataNode` without
    adding it to DataStorage yet.
  - Chosen next slice: add a monolith image-preprocessing result node factory
    in Infrastructure.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing Result
  Node Factory.
- Started the next unattended loop iteration:
  - Adding failing result node factory regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_result_node_factory` because
    `Infrastructure/xq_ImagePreprocessingResultNodeFactory.h` did not exist.
- Implemented the monolith image-preprocessing result node factory:
  - Added `ImagePreprocessingResultNodeFactory`.
  - Created result image `mitk::DataNode`s from execution result images through
    the MITK image adapter.
  - Preserved legacy result naming and image-processing metadata.
  - Marked generated nodes with the shared pipeline metadata helper.
  - Added `xqModuleCommon` to Infrastructure linkage for pipeline metadata.
  - Kept this slice node-creation only; it does not add nodes to DataStorage or
    mutate catalog/project state.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_result_node_factory`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 46/46.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Result Node Factory to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the completed image extraction, execution, and result node
    creation boundaries after the result node factory landed.
  - The next integration gap is the first explicit storage mutation service:
    add the created result node under the source node in MITK DataStorage.
  - Chosen next slice: add a monolith image-preprocessing storage commit
    service in Infrastructure.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Storage Commit Service.
- Started the next unattended loop iteration:
  - Adding failing storage commit service regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_storage_commit_service`
    because `Infrastructure/xq_ImagePreprocessingStorageCommitService.h` did
    not exist.
- Implemented the monolith image-preprocessing storage commit service:
  - Added `ImagePreprocessingStorageCommitRequest` and
    `ImagePreprocessingStorageCommitResult`.
  - Extracted source VTK input through the MITK image adapter.
  - Executed the requested operation through the execution service.
  - Created the output node through the result node factory.
  - Added the result node under the source node in MITK DataStorage.
  - Kept this slice storage-focused; it does not mutate DataCatalog or project
    files and is not wired into UI yet.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_storage_commit_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 47/47.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Storage Commit Service to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked monolith data catalog and hierarchy registration semantics after
    the storage commit service landed.
  - Generated preprocessing outputs need a non-empty virtual source path to be
    registered in `DataCatalogService` and displayed through
    `DataHierarchyService`.
  - Chosen next slice: add an image-preprocessing catalog commit service in
    Infrastructure.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Catalog Commit Service.
- Started the next unattended loop iteration:
  - Adding failing catalog commit service regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_catalog_commit_service`
    because `Infrastructure/xq_ImagePreprocessingCatalogCommitService.h` did
    not exist.
- Implemented the monolith image-preprocessing catalog commit service:
  - Added `ImagePreprocessingCatalogCommitRequest` and
    `ImagePreprocessingCatalogCommitResult`.
  - Registered successful generated preprocessing outputs in
    `DataCatalogService` with a stable virtual source path.
  - Ensured the Images hierarchy folder exists and added a data hierarchy node
    for the generated catalog entry.
  - Preserved storage failure diagnostics and kept this slice independent from
    MITK `DataStorage` mutation, project writes, and UI wiring.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_catalog_commit_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 48/48.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Catalog Commit Service to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked comparable workstation architecture:
    MITK keeps rendering data centered on `DataStorage`, Slicer modules
    separate widget actions from scene/logic mutation, SimVascular presents a
    project pipeline from images through paths, segmentation, modeling,
    meshing, simulation, ROM, and multiphysics, and OHIF routes toolbar/workflow
    behavior through command/service registration.
  - The completed preprocessing slices now cover Domain validation, algorithm
    dispatch, MITK image conversion, result node creation, storage commit, and
    catalog/hierarchy commit.
  - Chosen next slice: add an Infrastructure application commit service that
    composes storage and catalog commits and preflights generated metadata
    targets before mutating `DataStorage`.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Application Commit Service.
- Started the next unattended loop iteration:
  - Adding failing application commit service regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_application_commit_service`
    because
    `Infrastructure/xq_ImagePreprocessingApplicationCommitService.h` did not
    exist.
- Implemented the monolith image-preprocessing application commit service:
  - Added `ImagePreprocessingApplicationCommitRequest` and
    `ImagePreprocessingApplicationCommitResult`.
  - Added target preflight for duplicate generated catalog ids and generated
    hierarchy node ids before invoking image-processing algorithms.
  - Composed `ImagePreprocessingStorageCommitService` and
    `ImagePreprocessingCatalogCommitService` into one Infrastructure
    application commit path.
  - Preserved storage failure diagnostics without catalog/hierarchy mutation
    and kept this slice free of UI, `ApplicationContext` registration, and
    project-file writes.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_application_commit_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 49/49.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Application Commit Service to completed
  in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the command/action boundaries after the application commit
    service landed.
  - 3D Slicer keeps loaded data in a scene of nodes and lets modules operate on
    selected scene nodes; OHIF registers commands and services through managers
    so UI triggers stay separated from implementation services.
  - The next integration gap is an explicit action handler bridge between
    `WorkflowActionService` and the completed preprocessing application commit
    pipeline.
  - Chosen next slice: add an Infrastructure workflow action handler registrar
    with configurable operation parameters for future Presentation wiring.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Workflow Action Handler.
- Started the next unattended loop iteration:
  - Adding failing workflow action handler regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_image_preprocessing_workflow_action_handler`
    because `Infrastructure/xq_ImagePreprocessingWorkflowActionHandler.h` did
    not exist.
- Implemented the monolith image-preprocessing workflow action handler:
  - Added `ImagePreprocessingWorkflowActionOptions`.
  - Added `RegisterImagePreprocessingWorkflowActionHandler()` to install a
    configurable `image-preprocessing` handler in `WorkflowActionService`.
  - The handler uses `ApplicationContext` active MITK node, workflow snapshot,
    `DataStorage`, `DataCatalogService`, and `DataHierarchyService` to call
    `ImagePreprocessingApplicationCommitService`.
  - Generated result catalog ids now follow `<source-id>-<operation-id>`.
- Debugging note:
  - The first target test run failed on the missing-active-node case.
  - Root cause: `WorkflowActionService::RunActiveWorkflowAction()` ran
    registered handlers through `TaskRunner` but always returned `true` after
    handler execution, even when the task failed.
  - Fixed the Core service to return the actual `TaskRunner::RunBlocking()`
    result for registered handlers.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_workflow_action_handler`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 50/50.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Workflow Action Handler to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the data-node identity boundary after the workflow action handler
    landed.
  - The handler can run through `ApplicationContext::ActiveNode()`, but future
    workflow actions need a stable way to resolve selected catalog entries to
    MITK `DataNode`s, matching MITK/Slicer-style node-centered workspaces.
  - Chosen next slice: add a Core data-node registry that binds monolith
    catalog entry ids to MITK nodes.
- Added next executable phase to `plan.md`: Monolith Data Node Registry
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing data-node registry regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_data_node_registry` because
    `Core/xq_DataNodeRegistryService.h` did not exist.
- Implemented the monolith data-node registry foundation:
  - Added Core `DataNodeRegistryService`.
  - Supported catalog id to MITK `DataNode` binding, lookup, ordered bound-id
    listing, rebinding, and removal.
  - Rejected empty catalog ids and null nodes with clear diagnostics.
  - Added ownership/access through `ApplicationContext::DataNodes()`.
  - Kept this slice Core-only; image-preprocessing handler, file import UI,
    and project persistence were not changed.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_node_registry`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 51/51.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Node Registry Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the image-preprocessing handler now that Core can bind catalog
    entry ids to MITK `DataNode`s.
  - The next integration gap is using those bindings during workflow action
    execution so catalog selection can drive processing without relying on a
    transient active node.
  - Chosen next slice: make the image-preprocessing workflow action handler
    resolve selected source nodes from `DataNodeRegistryService`, with active
    node as a fallback.
- Added next executable phase to `plan.md`: Monolith Image Preprocessing
  Handler DataNode Registry Integration.
- Started the next unattended loop iteration:
  - Adding failing handler registry-resolution regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` compiled the
    updated test successfully.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_workflow_action_handler`
    failed because the handler still required `ApplicationContext::ActiveNode()`
    and did not resolve the selected catalog entry through
    `DataNodeRegistryService`.
- Implemented image-preprocessing handler data-node registry integration:
  - The workflow action handler now first resolves the selected source node via
    `ApplicationContext::DataNodes()->FindNode(snapshot.SelectedCatalogEntryId)`.
  - Active node fallback remains available for existing manual/test workflows.
  - The existing missing-node diagnostic is preserved when neither source is
    available.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_workflow_action_handler`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 51/51.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Image Preprocessing Handler DataNode Registry Integration
  to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked Core data lifecycle after handler registry resolution landed.
  - The next gap is cleanup: `DataManagementService::RemoveEntry()` removes
    catalog, hierarchy, and selection state, but bound MITK node registry
    entries need to be cleared with the same data lifecycle.
  - Chosen next slice: integrate optional `DataNodeRegistryService` cleanup into
    Core data removal.
- Added next executable phase to `plan.md`: Monolith Data Management DataNode
  Registry Cleanup.
- Started the next unattended loop iteration:
  - Adding failing data-management registry cleanup regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` compiled the
    updated data-management test successfully.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_management_service`
    failed because successful `RemoveEntry()` left the bound data-node registry
    entry behind.
- Implemented data-management data-node registry cleanup:
  - Added an overload so `DataManagementService` can receive
    `DataNodeRegistryService`.
  - `ApplicationContext` now constructs `DataManagementService` with the shared
    data-node registry.
  - Successful removes clear any existing node binding for the removed catalog
    id.
  - Metadata-only removes still succeed because missing node bindings are
    harmless.
  - Failed removes preserve existing node bindings.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_management_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 51/51.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Management DataNode Registry Cleanup to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the import path after data-node registry cleanup landed.
  - `DataImportService` is intentionally metadata-only; now that Core has
    registry lifecycle, the next gap is a bridge for already-created MITK nodes
    that imports metadata, adds the node to `DataStorage`, and binds the node to
    the imported catalog id.
  - Chosen next slice: add an Infrastructure `DataNodeImportService` without
    file decoding or UI dialogs.
- Added next executable phase to `plan.md`: Monolith Data Node Import Service
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing data-node import service regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_data_node_import_service` because
    `Infrastructure/xq_DataNodeImportService.h` did not exist.
- Implemented the monolith data-node import service foundation:
  - Added Infrastructure `DataNodeImportService`.
  - The service composes Core `DataImportService`,
    `DataNodeRegistryService`, and MITK `DataStorage` for already-created
    MITK nodes.
  - Missing storage, Core import service, registry, and node inputs are rejected
    with explicit diagnostics.
  - Failed metadata imports do not mutate `DataStorage` or registry bindings.
  - Valid imports add the node to `DataStorage`, register metadata/hierarchy
    through Core import behavior, select the imported entry, and bind the
    catalog id to the node.
- Verification for this iteration:
  - Red/green target test:
    `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_node_import_service`
    passed after implementation: 1/1.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 52/52.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Node Import Service Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the monolith import chain after `DataNodeImportService` landed.
  - MITK's local-file import API exposes `mitk::IOUtil::Load(path)` and
    `mitk::IOUtil::Load(path, DataStorage&)`; comparable medical imaging
    workstations keep file loading as the scene/data-tree entrypoint before
    workflow-specific tools operate on selected data.
  - Chosen next slice: add a small Infrastructure `MitkFileImportService` that
    loads one local file into a MITK data object, wraps it in a `DataNode`, and
    reuses `DataNodeImportService` to enter Core catalog/hierarchy/selection
    and the MITK node registry.
- Added next executable phase to `plan.md`: Monolith MITK File Import Service
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing file import service regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_mitk_file_import_service` because
    `Infrastructure/xq_MitkFileImportService.h` did not exist.
- Implemented the monolith MITK file import service foundation:
  - Added `MitkFileReader` and production `MitkIOFileReader`, with the default
    reader calling `mitk::IOUtil::Load(path)` and converting MITK/std
    exceptions to diagnostics.
  - Added `MitkFileImportService`, which validates dependencies/source path,
    accepts exactly one loaded MITK data object, wraps it in a `DataNode`, and
    reuses `DataNodeImportService` to commit storage, Core metadata/hierarchy,
    selection, and node-registry binding.
  - Reader failures, empty/multi-object reads, and metadata import failures do
    not mutate `DataStorage` or rebind registry entries.
- Red/green target verification:
  - After implementation, the target build passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_mitk_file_import_service`
    passed: 1/1.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 53/53.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith MITK File Import Service Foundation to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the monolith import path after `MitkFileImportService` landed.
  - Comparable medical imaging workstations expose loading/importing as a main
    data-scene action before workflow-specific tools operate on selected data.
  - Chosen next slice: add a Presentation-safe data import toolbar action with
    an injectable command boundary, leaving real `QFileDialog`, DICOM, and
    multi-file behavior for later slices.
- Added next executable phase to `plan.md`: Monolith Data Import Action Shell.
- Started the next unattended loop iteration:
  - Adding failing MainWindow import action regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_main_window_import_action` because
    `Presentation/xq_DataImportCommand.h` did not exist.
- Implemented the monolith data import action shell:
  - Added Presentation `DataImportCommand` and `DataImportCommandResult`.
  - Added `MainWindow::SetDataImportCommand()` for testable command injection.
  - Added enabled data toolbar action `xqImportDataAction`.
  - Missing import command posts a clear diagnostic and does not mutate state.
  - Configured commands run through the window context and refresh data page,
    project data count, and data action enablement after returning.
- Red/green target verification:
  - After implementation, the target build passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_main_window_import_action`
    passed: 1/1.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 54/54.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Import Action Shell to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the new Presentation import action shell and the existing
    `MitkFileImportService`.
  - The next gap is a command object that adapts a chosen file path into the
    service without coupling `MainWindow` tests to system file dialogs.
  - Chosen next slice: add an Infrastructure `MitkFileDataImportCommand` with
    injectable path provider and reader.
- Added next executable phase to `plan.md`: Monolith MITK File Data Import
  Command.
- Started the next unattended loop iteration:
  - Adding failing command-level regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_mitk_file_data_import_command` because
    `Infrastructure/xq_MitkFileDataImportCommand.h` did not exist.
- Implemented the monolith MITK file data import command:
  - Moved the data import command port to Core as `DataImportCommand` and
    `DataImportCommandResult`, keeping Presentation and Infrastructure pointed
    inward at Core.
  - Added `FileImportPathProvider` for testable path selection.
  - Added `MitkFileDataImportCommand`, implementing the Core
    `DataImportCommand`.
  - The command converts a selected file path into a single-image
    `DataImportRequest`, delegates to `MitkFileImportService`, and returns the
    command-level result.
  - Missing provider, cancellation, reader failure, and successful import are
    covered without opening a real file dialog.
- Red/green target verification:
  - After implementation, the target build passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_mitk_file_data_import_command`
    passed: 1/1.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 55/55.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith MITK File Data Import Command to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the command path after `MitkFileDataImportCommand` landed.
  - The next gap is production wiring: the Import action is testable, and the
    MITK import command exists, but `main.cxx` still constructs `MainWindow`
    without a command.
  - Chosen next slice: add a Qt file path provider and wire the monolith
    composition root while keeping tests away from real native dialogs.
- Added next executable phase to `plan.md`: Monolith Qt File Import Wiring.
- Started the next unattended loop iteration:
  - Adding failing Qt provider and composition wiring tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new tests.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_qt_file_import_path_provider` because
    `Presentation/xq_QtFileImportPathProvider.h` did not exist.
  - The same build failed while compiling
    `test_monolith_application_import_wiring` because
    `xq_MonolithApplication.h` did not exist.
- Implemented monolith Qt file import wiring:
  - Moved `FileImportPathProvider` into Core beside `DataImportCommand`.
  - Added Presentation `QtFileImportPathProvider` using
    `QFileDialog::getOpenFileName` with a medical-image filter.
  - Added `ConfiguredMainWindow` and `CreateConfiguredMainWindow()` as the
    composition root for MainWindow plus the MITK file import command.
  - Updated `main.cxx` to create the configured window before setting the MITK
    render host.
- Red/green target verification:
  - After implementation, the target build passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(qt_file_import_path_provider|application_import_wiring|mitk_file_data_import_command)"`
    passed: 3/3.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 57/57.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Qt File Import Wiring to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked MITK render-view refresh patterns in the existing legacy modules
    and MITK sources.
  - Existing XQ code calls `mitk::RenderingManager::RequestUpdateAll()` after
    data mutations, and MITK examples initialize views from DataStorage bounds
    after load/add-data operations.
  - The next gap is render refresh after the new monolith file import path adds
    nodes to `DataStorage`.
- Added next executable phase to `plan.md`: Monolith Import Render Refresh.
- Started the next unattended loop iteration:
  - Adding failing render-refresh regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_mitk_file_data_import_command` because
    `Core/xq_RenderRefreshService.h` did not exist.
  - The same build failed while compiling
    `test_monolith_application_import_wiring` because
    `ConfiguredMainWindow` did not expose a `RenderRefresh` service.
- Implemented monolith import render refresh:
  - Added Core `RenderRefreshService`.
  - Added Infrastructure `MitkRenderRefreshService`, which initializes MITK
    views from the current `DataStorage` bounds and requests a global render
    update.
  - `MitkFileDataImportCommand` now accepts an optional refresh service and
    calls it only after successful imports.
  - `CreateConfiguredMainWindow()` owns and injects the MITK render refresh
    service into the import command.
- Red/green target verification:
  - After implementation, the target build passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(mitk_file_data_import_command|application_import_wiring)"`
    passed: 2/2.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 57/57.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Import Render Refresh to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked file import after render refresh wiring landed.
  - The current command imports every file as an image, which is too narrow for
    the monolith plan's modeling, meshing, and simulation-result workflows.
  - Chosen next slice: deterministic role inference from file name/extension
    for single-file imports.
- Added next executable phase to `plan.md`: Monolith File Import Role
  Inference.
- Started the next unattended loop iteration:
  - Adding failing role-inference regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed after
    compiling the updated test.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_mitk_file_data_import_command`
    failed because imported segmentation/model/mesh/result files still used
    the `image-` catalog id prefix and image workflow role.
- Implemented monolith file import role inference:
  - Added deterministic file-name and extension rules in
    `MitkFileDataImportCommand`.
  - Image remains the default.
  - Segmentation-like names use `DataWorkflowRole::Segmentation` and
    `segmentation-` ids.
  - Model-like names/extensions use `DataWorkflowRole::Model` and `model-`
    ids.
  - Mesh-like names/extensions use `DataWorkflowRole::Mesh` and `mesh-` ids.
  - Simulation-result-like names use `DataWorkflowRole::SimulationResult` and
    `result-` ids.
- Red/green target verification:
  - After implementation, the target build passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_mitk_file_data_import_command`
    passed: 1/1.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 57/57.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith File Import Role Inference to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked Data page visibility after file role inference landed.
  - The Data page still shows selection, id, name, and source path, but not the
    inferred workflow role, making imported model/mesh/result files less
    transparent to users.
  - Chosen next slice: surface workflow role on the monolith Data page.
- Added next executable phase to `plan.md`: Monolith Data Page Role Display.
- Started the next unattended loop iteration:
  - Adding failing Data page role label regression tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed after
    compiling the updated test.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_workflow_page`
    failed because the Data page did not expose
    `xqDataPageWorkflowRole`.
- Implemented monolith Data page role display:
  - Added a Presentation workflow-role label on the Data page.
  - Added user-facing role names for DICOM Series, Image, Segmentation,
    Model, Mesh, Simulation Result, and Unknown.
  - The role label updates with the selected catalog entry and clears with the
    rest of the metadata when selection is removed.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_data_workflow_page`
    passed: 1/1.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 57/57.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Monolith Data Page Role Display to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the monolith Image Preprocessing pipeline and comparable
    workstation workflows.
  - The Domain layer already exposes six preprocessing operations and
    parameter descriptors, but the monolith page still shows only a generic
    `Run` button.
  - Chosen next slice: add a Core operation-selection service and surface the
    Image Preprocessing operation selector in the Presentation page.
- Added next executable phase to `plan.md`: Image Preprocessing Operation
  Selector.
- Started the next unattended loop iteration:
  - Adding failing Core operation-service and Image Preprocessing page tests
    first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    registering the new tests.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_workflow_operation_service` and
    `test_monolith_image_preprocessing_operation_page` because
    `Core/xq_WorkflowOperationService.h` did not exist.
- Implemented Image Preprocessing operation selector:
  - Added Core `WorkflowOperationService` with per-workflow ordered
    operations and selected operation state.
  - Exposed workflow operations from `ApplicationContext`.
  - Registered the six Image Preprocessing operations from the Domain layer.
  - The Image Preprocessing page now exposes an operation combo box and updates
    the primary action text to the selected operation.
  - The default domain action handler now runs the selected Image Preprocessing
    operation when operation state is available.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(workflow_operation_service|image_preprocessing_operation_page)"`
    passed: 2/2.

## Current Run Final Update: Run Script Monolith Fallback

- Debugging note:
  - Full XQ PowerShell tests first failed in `test_no_backup_artifacts.ps1`
    because the stale-artifact scan matched its own test filename.
  - Root cause was path-pattern self-matching, not a newly tracked backup
    source artifact.
  - Renamed the regression test to `test_no_stale_artifacts.ps1` without
    relaxing the backup artifact detection pattern.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 65/65.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Run Script Monolith Fallback to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run: Segmentation 2D Infrastructure Action Handler

- Completed autonomous research refresh:
  - Rechecked comparable workstation flow: Slicer/MITK keep segmentation as
    renderable data nodes, while SimVascular treats contour groups as the
    first modeling-ready artifact after Image/Path.
  - XQ already has `xq_SegmentationPipelineService::CreateContourGroup`, but
    monolith 2D Segmentation still only reports operation-aware placeholder
    acceptance.
  - Chosen next slice: route `manual-contour` through the existing
    Segmentation pipeline and commit the generated Segmentation into monolith
    catalog/hierarchy/selection/rendering state.
- Added next executable phase to `plan.md`: Segmentation 2D Infrastructure
  Action Handler.
- Started the next unattended loop iteration:
  - Adding failing 2D Segmentation workflow action handler and composition-root
    tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding the 2D Segmentation handler test target.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed because
    `Infrastructure/xq_SegmentationWorkflowActionHandler.h` did not exist.
- Implemented 2D Segmentation infrastructure action handler:
  - Added `RegisterDynamicSegmentationWorkflowActionHandler()` for
    `segmentation-2d` `manual-contour`.
  - The handler resolves the selected Path node, calls
    `xq_SegmentationPipelineService::CreateContourGroup`, registers the
    generated Segmentation catalog/hierarchy/node binding, selects the result,
    and refreshes rendering.
  - Unsupported 2D operations and 3D Segmentation remain on the operation-aware
    placeholder path for this slice.
  - `CreateConfiguredMainWindow()` now registers the dynamic Segmentation
    handler after Domain handler registration.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(segmentation_workflow_action_handler|application_import_wiring|domain_workflow_action_handlers|segmentation_operation_page)"`
    passed: 4/4.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 67/67.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Segmentation 2D Infrastructure Action Handler to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run: Modeling Infrastructure Action Handler

- Completed autonomous research refresh:
  - Rechecked the now-connected Image -> Path -> 2D Segmentation chain against
    the existing model pipeline contract.
  - XQ already has `xq_ModelPipelineService::CreateModel` and regression
    coverage for loft-ready ProfileGroup inputs, but monolith Modeling still
    reports operation-aware placeholder acceptance.
  - Chosen next slice: route `build-solid-model` through the existing Model
    pipeline and commit the generated Model into monolith
    catalog/hierarchy/selection/rendering state.
- Added next executable phase to `plan.md`: Modeling Infrastructure Action
  Handler.
- Started the next unattended loop iteration:
  - Adding failing Modeling workflow action handler and composition-root tests
    first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding the Modeling handler test target.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed because
    `Infrastructure/xq_ModelingWorkflowActionHandler.h` did not exist.
- Implemented Modeling infrastructure action handler:
  - Added `RegisterDynamicModelingWorkflowActionHandler()` for
    `modeling` `build-solid-model`.
  - The handler resolves the selected Segmentation/ContourGroup node, calls
    `xq_ModelPipelineService::CreateModel`, registers the generated Model
    catalog/hierarchy/node binding, selects the result, and refreshes
    rendering.
  - Unsupported Modeling operations remain on the operation-aware placeholder
    path for this slice.
  - `CreateConfiguredMainWindow()` now registers the dynamic Modeling handler
    after Domain handler registration.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(modeling_workflow_action_handler|application_import_wiring|domain_workflow_action_handlers|modeling_meshing_operation_pages|workflow_context_service)"`
    passed: 5/5.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 68/68.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Modeling Infrastructure Action Handler to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Final Update: Legacy BlueBerry Include Isolation

- Implemented default monolith include isolation:
  - `BERRY_PLUGIN_SOURCE_DIRS`, `BERRY_PLUGIN_BUILD_DIRS`, and the BlueBerry
    plugin `include_directories()` call are now evaluated only when
    `XQ_BUILD_LEGACY_BLUEBERRY` is enabled.
  - Generic MITK module include paths remain available to the monolith build.
- Red/green target verification:
  - `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\test_monolith_scaffold.ps1`
    passed.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 65/65.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Legacy BlueBerry Include Isolation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run: Path Infrastructure Action Handler

- Completed autonomous research refresh:
  - Rechecked comparable workstation flow: Slicer/MITK keep data and
    segmentation/path tools tied to rendering, while SimVascular makes Path the
    first downstream artifact after Image import.
  - XQ's Image Preprocessing workflow already uses an Infrastructure
    MITK-backed handler, but Path still only posts operation-aware placeholder
    acceptance.
  - Chosen next slice: route `create-centerline` through existing
    `xq_PathPipelineService` and commit the generated Path into monolith
    catalog/hierarchy/selection/rendering state.
- Added next executable phase to `plan.md`: Path Infrastructure Action
  Handler.
- Started the next unattended loop iteration:
  - Adding failing Path workflow action handler and composition-root tests
    first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding the Path handler test target.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed because
    `Infrastructure/xq_PathWorkflowActionHandler.h` did not exist.
- Implemented Path infrastructure action handler:
  - Added first-class `DataWorkflowRole::Path` with import folders, project
    schema role roundtrip, workflow compatibility, and data-page display.
  - Added `seed-points` to the `create-centerline` Path operation descriptor.
  - Added `RegisterDynamicPathWorkflowActionHandler()` using the existing
    `xq_PathPipelineService::CreatePath` for `create-centerline`.
  - Successful Path creation now registers catalog/hierarchy/node bindings,
    selects the generated path, and refreshes rendering.
  - Unsupported Path operations continue through the existing operation-aware
    placeholder path for this slice.
  - `CreateConfiguredMainWindow()` now registers the dynamic Path handler after
    Domain handler registration.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(path_workflow_action_handler|mitk_file_data_import_command|project_data_persistence|application_import_wiring|workflow_context_service|domain_workflow_action_handlers|path_operation_page)"`
    passed: 7/7.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 66/66.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Path Infrastructure Action Handler to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run: Workflow Operation Parameter UI Restore

- Completed autonomous research refresh:
  - Rechecked comparable workstation direction:
    3D Slicer's Segment Editor keeps tool parameters visible in the active
    workflow, SimVascular's vascular pipeline depends on stable project state
    across image/path/segmentation/model/mesh/simulation stages, and MITK
    Workbench centers workflows around Data Manager plus active tool panels.
  - Current XQ monolith pages now expose operation controls across all
    first-version workflows, but `MainWindow` only refreshes those controls
    when the selected operation changes.
  - Chosen next slice: refresh generic operation parameter panels when project
    open restores parameter values without changing the selected operation id.
- Added next executable phase to `plan.md`: Workflow Operation Parameter UI
  Restore.
- Started the next unattended loop iteration:
  - Adding a failing Path page restore test first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` initially
    exposed missing ProjectService/ProjectSessionService includes in the new
    test, which were added so the test compiled.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_path_operation_page`
    then failed with
    `project open should refresh unchanged Path operation parameters`.
- Implemented Workflow Operation Parameter UI Restore:
  - `MainWindow` now listens to `WorkflowOperationService::ParameterValueChanged`.
  - Restored parameter values update the existing visible spinbox editor with
    `QSignalBlocker` instead of rebuilding the whole panel during editor
    signals.
  - A first synchronous panel-rebuild attempt caused a target-test segfault by
    deleting the active editor; the final implementation updates editor values
    in place.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_path_operation_page`
    passed: 1/1.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 65/65.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Workflow Operation Parameter UI Restore to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run: Workflow Operation Option Parameters

- Completed autonomous research refresh:
  - Rechecked comparable workstation patterns after generic parameter restore:
    segmentation and simulation tool panels routinely include option-set
    parameters, not only numeric spin boxes.
  - Current XQ operation parameter metadata supports numeric scalar, integer
    scalar, and placeholder point-list values, so solver profiles and mode
    choices cannot be represented in the monolith workflow UI.
  - Chosen next slice: add option-set parameter descriptors and render them as
    generic workflow combo boxes, starting with Flow Simulation solver profile.
- Added next executable phase to `plan.md`: Workflow Operation Option
  Parameters.
- Started the next unattended loop iteration:
  - Adding failing Core option-parameter and Flow Simulation UI tests first.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_workflow_operation_service` because Core did not
    expose `WorkflowOperationParameterOption`, option metadata, or an `Option`
    parameter value type.
- Implemented Workflow Operation Option Parameters:
  - Added Core option parameter descriptors with ordered option id/title
    metadata.
  - Option parameters default to their first option id and reject empty or
    duplicate option lists.
  - `SetParameterValue()` now rejects unknown option values.
  - Generic workflow parameter panels render option parameters as `QComboBox`
    controls and synchronize combo changes into Core state.
  - Flow Simulation `configure-cfd-job` now exposes a `solver-profile` option
    with steady, pulsatile, and transient profiles.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(workflow_operation_service|simulation_operation_pages)"`
    passed: 2/2.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 65/65.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Workflow Operation Option Parameters to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run: Workflow Point List Parameter Editor

- Completed autonomous research refresh:
  - Rechecked workflow parameter coverage after option-set support.
  - The remaining parameter placeholder is `IntegerPointList`, which maps to
    seed-point workflows such as connected-threshold segmentation and should be
    editable before real MITK segmentation tools are wired.
  - Chosen next slice: replace the point-list placeholder row with a simple
    editable text control that stores integer point triplets in Core state.
- Added next executable phase to `plan.md`: Workflow Point List Parameter
  Editor.
- Started the next unattended loop iteration:
  - Adding a failing Image Preprocessing seeds editor test first.
- Red test observed:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_operation_page`
    failed because connected-threshold still exposed `seeds` as a placeholder
    label instead of an editable parameter.
- Implemented Workflow Point List Parameter Editor:
  - `IntegerPointList` parameters now render as `QLineEdit` controls.
  - The editor accepts `x,y,z; x,y,z` text and stores a `QVariantList` of
    integer triplets in `WorkflowOperationService`.
  - Parameter-value restore formats stored triplets back into canonical text.
  - Invalid text posts a diagnostic and leaves the last valid Core value
    unchanged.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_image_preprocessing_operation_page`
    passed: 1/1.

## Current Run: Run Script Monolith Fallback

- Red test observed:
  - `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\test_windows_env_scripts.ps1`
    failed because `scripts/run-xq.ps1` still hardcoded `-Monolith` to only
    `XQMonolith.exe`.
- Implemented run-script monolith fallback:
  - Updated help text so `-Monolith` describes preferring
    `XQMonolith.exe`, then falling back to `XQ.exe`.
  - Replaced single executable-name selection with an ordered candidate list.
- Red/green target verification:
  - `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\test_windows_env_scripts.ps1`
    passed after the run-script update.

- Completed autonomous research refresh:
  - Rechecked the first-version vascular workflow coverage after
    Modeling/Meshing operation controls landed.
  - Flow Simulation, ROM Simulation, and MultiPhysics are still generic
    placeholders, but the Core context already accepts Mesh and
    SimulationResult data for all three workflows.
  - Chosen next slice: add Flow/ROM/MultiPhysics operation descriptors,
    controls, and operation-aware Domain action routing.
- Added next executable phase to `plan.md`: Simulation Operation Foundation.
- Started the next unattended loop iteration:
  - Adding failing Domain/UI tests for Flow/ROM/MultiPhysics operations first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding `test_monolith_simulation_operation_pages`.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|simulation_operation_pages)"`
    failed because Flow/ROM/MultiPhysics operations were not registered and
    their pages had no selectors.
- Implemented Simulation Operation Foundation:
  - Domain workflow registration now registers Flow Simulation operations:
    Configure CFD Job, Steady Flow Solve, and Review Results.
  - Domain workflow registration now registers ROM Simulation operations:
    Build 1D Network, ROM Solver, and Calibrate Boundary Conditions.
  - Domain workflow registration now registers MultiPhysics operations:
    Configure Coupling, Coupled Solve, and Review Coupled Results.
  - Flow/ROM/MultiPhysics pages reuse the generic operation selector and
    parameter panel.
  - Operation-aware Domain action routing now covers Flow/ROM/MultiPhysics.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|simulation_operation_pages)"`
    passed: 2/2.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 64/64.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Simulation Operation Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked first-version workflow coverage after Flow/ROM/MultiPhysics
    operation controls landed.
  - Python API remains the only first-version workflow page with no operation
    selector or action surface in the monolith shell.
  - Chosen next slice: add a non-data-dependent Python API operation/action
    shell.
- Added next executable phase to `plan.md`: Python API Operation Foundation.
- Started the next unattended loop iteration:
  - Adding failing Domain/UI tests for Python API operations first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding `test_monolith_python_api_operation_page`.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|python_api_operation_page|workflow_primary_action_page)"`
    failed because Python API operations were not registered and its page had
    no operation selector.
- Implemented Python API Operation Foundation:
  - Domain workflow registration now registers Python API operations: Open
    Python Console, Project Script Runner, and Export API Snippet.
  - Python API gets an operation-aware no-data handler when a
    `WorkflowOperationService` is supplied.
  - Generic workflow pages now support operation-only workflows that do not
    require selected data.
  - No-data operation pages display `Ready.` instead of asking for compatible
    data.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|python_api_operation_page|workflow_primary_action_page|workflow_context_status_page)"`
    passed: 4/4.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 65/65.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Python API Operation Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked monolith delivery defaults after all first-version workflow pages
    gained operation controls.
  - `CMakePresets.json` disables `XQ_BUILD_LEGACY_BLUEBERRY` for Windows, but
    `Code/CMake/XQOptions.cmake` still defaults the global option to `ON`.
  - Chosen next slice: make legacy BlueBerry opt-in by default so local CMake
    configuration paths also build the monolith delivery target first.
- Added next executable phase to `plan.md`: Legacy BlueBerry Default
  Retirement.
- Started the next unattended loop iteration:
  - Adding a failing PowerShell test for the global legacy option default
    first.
- Red test observed:
  - `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\test_windows_legacy_blueberry_default_off.ps1`
    failed because `XQ_BUILD_LEGACY_BLUEBERRY` still defaulted to `ON`.
- Implemented Legacy BlueBerry Default Retirement:
  - `Code/CMake/XQOptions.cmake` now defaults
    `XQ_BUILD_LEGACY_BLUEBERRY` to `OFF`.
  - The Windows preset remains explicitly set to `OFF`.
- Red/green target verification:
  - `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\test_windows_legacy_blueberry_default_off.ps1`
    passed.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 16/16.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 65/65.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Legacy BlueBerry Default Retirement to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked default delivery target naming after legacy BlueBerry became
    opt-in by default.
  - The monolith executable output is named `XQ`, but the CMake target is still
    unconditionally declared as `XQMonolith`.
  - Chosen next slice: make the default monolith CMake target `XQ`, falling
    back to `XQMonolith` only when legacy BlueBerry is explicitly enabled.
- Added next executable phase to `plan.md`: Default XQ Monolith Target Naming.
- Started the next unattended loop iteration:
  - Updating the monolith scaffold test to fail on unconditional
    `XQMonolith` first.
- Red test observed:
  - `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\test_monolith_scaffold.ps1`
    failed because `Code/Source/Monolith/CMakeLists.txt` unconditionally
    declared `add_executable(XQMonolith ...)`.
- Implemented Default XQ Monolith Target Naming:
  - `XQ_MONOLITH_TARGET` now defaults to `XQ`.
  - Legacy BlueBerry opt-in builds switch the monolith target to `XQMonolith`
    so the legacy application can still use target `XQ`.
  - Include/link properties now apply through the conditional target variable.
- Red/green target verification:
  - `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\test_monolith_scaffold.ps1`
    passed.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 16/16.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 65/65.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Default XQ Monolith Target Naming to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Scanned repository filenames for stale backup artifacts.
  - Found
    `Code/Source/ImagingWorkbench/Plugins/org.xq.data.projectnodes/src/internal/xq_ProjectDataNodesPluginActivator.cxx.missing_target_backup_20260501_231206`.
  - Chosen next slice: add a backup-artifact guard test and remove the stale
    backup file.
- Added next executable phase to `plan.md`: Backup Artifact Cleanup.
- Started the next unattended loop iteration:
  - Adding a failing PowerShell test for backup artifact filenames first.
- Red test observed:
  - `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\test_no_backup_artifacts.ps1`
    failed on the stale tracked backup file under
    `org.xq.data.projectnodes`.
- Implemented Backup Artifact Cleanup:
  - Deleted
    `Code/Source/ImagingWorkbench/Plugins/org.xq.data.projectnodes/src/internal/xq_ProjectDataNodesPluginActivator.cxx.missing_target_backup_20260501_231206`.
  - The guard test now reports backup artifact filenames that still exist in
    the current working tree.
- Red/green target verification:
  - `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tests\test_no_backup_artifacts.ps1`
    passed.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 65/65.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Backup Artifact Cleanup to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked Windows run scripts after the default monolith CMake target was
    renamed to `XQ`.
  - `scripts/run-xq.ps1 -Monolith` still hardcodes `XQMonolith.exe`, which is
    only produced for legacy opt-in builds after the target rename.
  - Chosen next slice: make `-Monolith` prefer `XQMonolith.exe` but fall back
    to default `XQ.exe`.
- Added next executable phase to `plan.md`: Run Script Monolith Fallback.
- Started the next unattended loop iteration:
  - Extending `test_windows_env_scripts.ps1` to fail on hardcoded monolith exe
    selection first.

## Current verified checkpoint

- Modeling/Meshing Operation Foundation is complete and promoted in `plan.md`.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 63/63.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Started Active Phase: Autonomous Research Refresh.

## Current verified checkpoint

- Completed autonomous research refresh:
  - Segmentation operation selectors and parameter panels now exist, but the
    Run action still uses the generic Domain placeholder instead of the
    selected segmentation operation.
  - Chosen next slice: route segmentation Domain actions through selected
    operation state so task history and diagnostics show the selected
    operation.
- Added next executable phase to `plan.md`: Segmentation Operation Action
  Routing.
- Started the next unattended loop iteration:
  - Adding failing Domain/UI tests for selected segmentation operation action
    messages first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed after
    extending Domain and Segmentation UI tests.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|segmentation_operation_page)"`
    failed because segmentation action messages still used generic workflow
    placeholders instead of selected operation titles.
- Implemented Segmentation Operation Action Routing:
  - Domain registration now installs operation-aware handlers for
    `segmentation-2d` and `segmentation-3d` when a
    `WorkflowOperationService` is supplied.
  - Selected segmentation operation titles are read from registered operation
    descriptors and included in task/diagnostic messages.
  - Registration without an operation service still preserves the generic
    placeholder handler behavior.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|segmentation_operation_page)"`
    passed: 2/2.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 61/61.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Segmentation Operation Action Routing to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the vascular workflow sequence after segmentation operation
    routing. SimVascular-style workflows depend on explicit path planning
    before segmentation/modeling, and Slicer-style workflows use editable curve
    objects as the path boundary.
  - The monolith Path page still has only generic placeholder behavior.
  - Chosen next slice: add Path operation descriptors, controls, and
    operation-aware Domain action routing.
- Added next executable phase to `plan.md`: Path Operation Foundation.
- Started the next unattended loop iteration:
  - Adding failing Domain/UI tests for Path operations and selected action
    messages first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding `test_monolith_path_operation_page`.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|path_operation_page)"`
    failed because Path operations were not registered and the Path page had
    no operation selector.
- Implemented Path Operation Foundation:
  - Domain workflow registration now registers Path operations: Create
    Centerline, Edit Control Points, and Smooth Path.
  - The Path page now reuses the generic operation selector and parameter
    panel.
  - Path action routing reports the selected operation title while preserving
    generic placeholder behavior when no operation service is supplied.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|path_operation_page|segmentation_operation_page)"`
    passed: 3/3.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 62/62.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Path Operation Foundation to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Path, 2D Segmentation, and 3D Segmentation now expose operation controls
    and selected-operation action messages.
  - Modeling and Meshing remain generic placeholders even though they are the
    next core stages in the vascular pipeline.
  - Chosen next slice: add Modeling/Meshing operation descriptors, controls,
    and operation-aware Domain action routing.
- Added next executable phase to `plan.md`: Modeling/Meshing Operation
  Foundation.
- Started the next unattended loop iteration:
  - Adding failing Domain/UI tests for Modeling and Meshing operations first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding `test_monolith_modeling_meshing_operation_pages`.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|modeling_meshing_operation_pages)"`
    failed because Modeling/Meshing operations were not registered and their
    pages had no selectors.
- Implemented Modeling/Meshing Operation Foundation:
  - Domain workflow registration now registers Modeling operations: Loft
    Surface, Build Solid Model, and Trim Branches.
  - Domain workflow registration now registers Meshing operations: Generate
    Surface Mesh, Generate Volume Mesh, and Boundary Layers.
  - Modeling/Meshing pages reuse the generic operation selector and parameter
    panel.
  - Operation-aware Domain action routing now covers Modeling and Meshing.
- Debugging note:
  - The first target run exposed a test setup issue: after Meshing import, the
    selected data was a Model, so later Segmentation assertions had an
    incompatible selection. The test now restores an Image selection before
    running Segmentation assertions.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|modeling_meshing_operation_pages|path_operation_page|segmentation_operation_page)"`
    passed: 4/4.

## Current verified checkpoint

- Image Preprocessing Result Activation is complete and promoted in `plan.md`.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 59/59.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Started Active Phase: Autonomous Research Refresh.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 59/59.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Image Preprocessing Parameter Panel to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the new parameter panel against Slicer-style effect parameter
    state and the existing Domain parameterized `RunOperation()` path.
  - The panel now displays controls, but edited values are not stored in Core
    and the default Image Preprocessing action does not consume them.
  - Chosen next slice: add Core parameter value state and pass selected
    Image Preprocessing parameters into the domain action.
- Added next executable phase to `plan.md`: Image Preprocessing Parameter
  State.
- Started the next unattended loop iteration:
  - Adding failing Core parameter-value and Image Preprocessing parameter
    state tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    extending the operation-service and operation-page tests.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_workflow_operation_service` and
    `test_monolith_image_preprocessing_operation_page` because
    `WorkflowOperationService` did not expose `ParameterValues`,
    `SetParameterValue`, or `ParameterValueChanged`.
- Implemented Image Preprocessing parameter state:
  - `WorkflowOperationService` now initializes and stores per-operation
    parameter values.
  - Added parameter value lookup, mutation, unknown-parameter rejection, and
    change notifications.
  - Image Preprocessing parameter editors load stored values and update Core
    state on edit.
  - The Domain default Image Preprocessing handler now passes selected
    operation parameter values into the parameterized `RunOperation()` path.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(workflow_operation_service|image_preprocessing_operation_page|domain_workflow_action_handlers|image_preprocessing_workflow_service)"`
    passed: 4/4.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 59/59.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Image Preprocessing Parameter State to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked production composition after parameter state landed.
  - `main.cxx` registers Domain workflow handlers and the composition root
    currently configures import/render services, but Image Preprocessing still
    runs through the Domain placeholder path instead of the existing
    MITK-backed Infrastructure commit handler.
  - Chosen next slice: wire dynamic Image Preprocessing Infrastructure handler
    registration in the monolith composition root.
- Added next executable phase to `plan.md`: Image Preprocessing Infrastructure
  Wiring.
- Started the next unattended loop iteration:
  - Adding failing composition-root regression test first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    extending the composition-root test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_application_import_wiring`
    failed because the configured Image Preprocessing action still succeeded
    through the Domain placeholder instead of requiring a MITK source node.
- Implemented Image Preprocessing infrastructure wiring:
  - Added dynamic Infrastructure workflow-action registration for Image
    Preprocessing.
  - The dynamic handler resolves the selected operation and parameter values
    from `WorkflowOperationService`.
  - It resolves the MITK source node from the data-node registry or active
    node and uses the existing application commit service.
  - `CreateConfiguredMainWindow()` registers the dynamic Infrastructure
    handler after Domain workflow registration, overriding the placeholder for
    production composition.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(application_import_wiring|image_preprocessing_workflow_action_handler)"`
    passed: 2/2.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 59/59.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Image Preprocessing Infrastructure Wiring to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked Image Preprocessing success behavior after infrastructure wiring.
  - Successful preprocessing commits add result data, but the handler does not
    yet activate the new result selection or refresh MITK rendering.
  - Chosen next slice: select generated preprocessing data and refresh
    rendering after successful preprocessing commits.
- Added next executable phase to `plan.md`: Image Preprocessing Result
  Activation.
- Started the next unattended loop iteration:
  - Adding failing fixed-operation handler test for result selection and render
    refresh first.
  - Related compatibility tests
    `test_monolith_workflow_action_service`,
    `test_monolith_domain_workflow_action_handlers`,
    `test_monolith_image_preprocessing_workflow_service`, and
    `test_monolith_workflow_primary_action_page` passed: 4/4.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    extending the fixed-operation handler test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed because
    `RegisterImagePreprocessingWorkflowActionHandler()` did not accept the
    render-refresh service and the first implementation temporarily broke
    existing `QString* message` call sites.
- Implemented Image Preprocessing result activation:
  - Successful Infrastructure preprocessing commits now select the generated
    catalog entry.
  - Successful commits call the optional render-refresh service once with the
    application `DataStorage`.
  - Failed commits still return before selection or refresh.
  - Fixed-operation and dynamic-operation registrations share the same
    activation helper, and source-compatible `QString* message` overloads keep
    existing call sites working.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(image_preprocessing_workflow_action_handler|application_import_wiring)"`
    passed: 2/2.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All PowerShell tests in `tests\*.ps1` passed: 15/15.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 59/59.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Image Preprocessing Operation Selector to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the new Image Preprocessing operation selector against the
    existing Domain parameter descriptors.
  - The page can now choose an operation, but it still does not expose the
    operation's parameters, leaving the workflow too generic for real
    preprocessing use.
  - Chosen next slice: carry operation parameter descriptors through Core and
    render a dynamic Image Preprocessing parameter panel.
- Added next executable phase to `plan.md`: Image Preprocessing Parameter
  Panel.
- Started the next unattended loop iteration:
  - Adding failing Core parameter metadata and Image Preprocessing parameter
    panel tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    extending the existing operation-service and operation-page tests.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed while
    compiling `test_monolith_workflow_operation_service` because Core did not
    expose workflow operation parameter descriptors or value types.
- Implemented Image Preprocessing parameter panel:
  - Extended Core workflow operation descriptors with ordered parameter
    descriptors.
  - `WorkflowOperationService` now preserves parameters and rejects duplicate
    parameter ids inside an operation.
  - Mapped Image Preprocessing Domain parameters into Core operation metadata.
  - Added a dynamic Image Preprocessing parameter panel that rebuilds controls
    when the selected operation changes.
  - Numeric scalar parameters use `QDoubleSpinBox`; integer scalar parameters
    use `QSpinBox`; integer point lists get a placeholder row for a later
    richer editor.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(workflow_operation_service|image_preprocessing_operation_page)"`
    passed: 2/2.

## Current Run Final Update: Run Script Monolith Fallback

- Debugging note:
  - Full XQ PowerShell tests first failed in `test_no_backup_artifacts.ps1`
    because the stale-artifact scan matched its own test filename.
  - Root cause was path-pattern self-matching, not a newly tracked backup
    source artifact.
  - Renamed the regression test to `test_no_stale_artifacts.ps1` without
    relaxing the backup artifact detection pattern.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 65/65.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Run Script Monolith Fallback to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
