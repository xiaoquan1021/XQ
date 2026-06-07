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
