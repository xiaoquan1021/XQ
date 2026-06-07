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
