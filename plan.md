# XQ Windows Migration Plan

## Completed Phase: Windows Build Convergence

1. Keep the Windows x64 / VS2022 Externals stack usable from source-built installs.
2. Build XQ with `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`.
3. For each configure/build failure:
   - Record the failure in `execution_log.md`.
   - Add the smallest practical PowerShell/CMake regression test first.
   - Fix the recipe, CMake, or source code at the root cause.
   - Rerun the targeted test and the failed command.
4. Once XQ builds, run the available PowerShell tests, CMake build checks, and `diff --check`.
5. Commit and push the completed iteration.

## Completed Research: Comparable Workstations

The next monolith slice is grounded in these comparable systems:

- 3D Slicer: module/workflow navigation and Segment Editor-style 2D/3D segmentation tools.
- SimVascular: explicit project pipeline from image data to paths, segmentations, modeling, meshing, CFD, ROM, and multiphysics simulation.
- MITK Workbench: DataStorage-centered rendering, segmentation tools, diagnostics, and toolkit integration.
- OHIF: mode/extension registry concepts for workflow-specific panels and commands.
- ITK-SNAP/MONAI Label: focused segmentation UX, semi-automatic/AI-assisted entry points, and clear feedback loops.

## Completed Phase: Monolith Workflow Foundation

1. Add a typed `xq::core::WorkflowDescriptor` and deterministic default workflow registry in the monolith Core layer.
   - Include all first-version workflows: project/data, image preprocessing, path, 2D segmentation, 3D segmentation, modeling, meshing, flow, ROM, multiphysics, and Python API.
   - Keep this registry free of Qt widget ownership so it can be unit-tested and reused by Presentation.
2. Extend `xq::core::ApplicationContext` with queryable diagnostics history.
   - `PostDiagnostic` must still emit the existing signal.
   - Empty diagnostics must be ignored.
   - Stored diagnostics should be available to tests and future diagnostics UI.
3. Move `xq::presentation::MainWindow` page creation to the Core workflow registry instead of hardcoded UI strings.
4. Add C++ regression tests for the workflow registry and `ApplicationContext` diagnostic history before production code.
5. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
6. Commit and push the verified XQ iteration.

## Completed Phase: Monolith Project Service Foundation

1. Add a monolith Core `xq::core::ProjectService` for the fresh project format.
   - Use schema version `2.0`.
   - Save and open `.xqproj` files with project name, schema version, and relative workspace directory.
   - Do not attempt legacy project migration in this phase.
2. Add `ProjectService` ownership/access through `xq::core::ApplicationContext`.
   - UI and future workflow pages should ask the context for project operations instead of calling legacy project code directly.
3. Add C++ regression tests before implementation:
   - New service starts without an active project.
   - `CreateProject` + `SaveProject` writes schema `2.0`.
   - `OpenProject` restores the same project metadata.
   - Unsupported schema versions fail with an error.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Monolith Selection API Foundation

1. Strengthen `xq::core::ApplicationContext` selection behavior for workflow pages.
   - Keep the existing active-node interface available.
   - Add explicit `ClearActiveNode()`.
   - Emit selection-change notifications only when the selected node pointer actually changes.
   - Emit the selected node pointer with the change signal so future Presentation code does not need to re-query global state.
2. Add C++ regression tests before implementation:
   - Default context starts without an active node.
   - Setting a node stores it and emits one selection signal.
   - Setting the same node again is a no-op.
   - Clearing selection emits once and leaves no active node.
3. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
4. Commit and push the verified XQ iteration.

## Completed Phase: Monolith Task Runner Foundation

1. Add a monolith Core task runner service for long-running workflow operations.
   - Keep the first implementation synchronous/blocking for deterministic tests.
   - Emit task started/finished signals.
   - Store queryable task history with task name, success flag, and message.
   - Reject empty task names before running work.
2. Add `TaskRunner` ownership/access through `xq::core::ApplicationContext`.
   - Future workflow pages should dispatch import/segmentation/modeling jobs through this service.
3. Add C++ regression tests before implementation:
   - New runner has empty history.
   - Successful task emits start/finish and records success.
   - Failed task emits finish and records failure message.
   - Empty task name fails without invoking work.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Monolith Preferences Foundation

1. Add a monolith Core preferences service for app-level settings.
   - Keep the first implementation in-memory with explicit JSON save/load for deterministic tests.
   - Support string, boolean, and integer values.
   - Keep settings independent from legacy BlueBerry preferences.
2. Add `PreferencesService` ownership/access through `xq::core::ApplicationContext`.
3. Add C++ regression tests before implementation:
   - New service starts empty.
   - Values roundtrip through set/get.
   - JSON save/load restores all supported value types.
   - Invalid preference files fail with an error.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search current comparable medical imaging workstation projects and documentation.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Data Catalog Foundation

1. Add a monolith Core `xq::core::DataCatalogService`.
   - Keep the first implementation metadata-only and independent from MITK file import.
   - Track imported data entries with stable id, display name, source path, modality, and workflow role.
   - Reject empty source paths and duplicate ids.
   - Support query by id and ordered listing for future project/data-management UI.
2. Add `DataCatalogService` ownership/access through `xq::core::ApplicationContext`.
   - Future DICOM/image import, project tree, and workflow pages should register data through this service.
3. Add C++ regression tests before implementation:
   - New catalog starts empty.
   - Registering image/DICOM metadata creates deterministic entries.
   - Duplicate ids fail without changing existing entries.
   - Lookup by missing id returns null.
   - `ApplicationContext` exposes an empty catalog.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Monolith Project/Data Persistence Integration

1. Extend the fresh `.xqproj` schema `2.0` project service to persist DataCatalog metadata.
   - Save registered data entries under the project JSON.
   - Restore catalog entries when opening a project.
   - Preserve entry order, id, display name, source path, modality, and workflow role.
   - Reject unsupported workflow role values while opening.
2. Add a `ProjectService` save/open path that accepts a `DataCatalogService`.
   - Keep existing metadata-only `SaveProject` and `OpenProject` behavior working.
   - Avoid coupling `ProjectService` ownership to `ApplicationContext`; pass catalog explicitly for persistence.
3. Add C++ regression tests before implementation:
   - Saving a project with a catalog writes all data entries.
   - Opening a project with saved catalog entries restores them.
   - Unknown workflow role values fail with an error.
   - Existing metadata-only project tests still pass.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Monolith Data Import Service Foundation

1. Add a monolith Core `xq::core::DataImportService`.
   - Keep the first implementation metadata-only; do not perform MITK/DICOM decoding yet.
   - Accept import requests with source path, display name, modality, and workflow role.
   - Generate deterministic catalog ids when the caller does not provide one.
   - Register successful imports in `DataCatalogService`.
   - Run imports through `TaskRunner` so diagnostics/history can observe future long-running work.
2. Add C++ regression tests before implementation:
   - Importing an image request registers a catalog entry and task history.
   - Caller-provided ids are preserved.
   - Missing source path fails and does not register data.
   - Duplicate ids fail through catalog validation.
3. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
4. Commit and push the verified XQ iteration.

## Completed Phase: Monolith Data Import Context Integration

1. Add `DataImportService` ownership/access through `xq::core::ApplicationContext`.
   - Construct it from the existing `DataCatalogService` and `TaskRunner`.
   - Expose it through `DataImports()`.
   - Ensure it starts usable from a default context without caller wiring.
2. Add C++ regression tests before implementation:
   - `ApplicationContext::DataImports()` is non-null.
   - Import through the context importer registers data in the context catalog.
   - Import through the context importer records task history in the context task runner.
3. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
4. Commit and push the verified XQ iteration.

## Completed Phase: Monolith Project Session Service Foundation

1. Add a monolith Core `xq::core::ProjectSessionService`.
   - Coordinate `ProjectService`, `DataCatalogService`, and `TaskRunner`.
   - Provide one application-level save/open entry point for `.xqproj` schema `2.0` with catalog persistence.
   - Run save/open operations through `TaskRunner`.
2. Add `ProjectSessionService` ownership/access through `xq::core::ApplicationContext`.
   - Expose it through `ProjectSession()`.
   - Future UI should call this service instead of manually wiring project and catalog services.
3. Add C++ regression tests before implementation:
   - Saving through the session persists context catalog entries.
   - Opening through the session restores project metadata and catalog entries.
   - Save/open operations record task history.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Data Hierarchy Foundation

1. Add a monolith Core `xq::core::DataHierarchyService`.
   - Keep the first implementation metadata-only and independent from MITK/Qt widgets.
   - Maintain a stable root node plus ordered child nodes.
   - Support folder nodes and data-entry nodes that reference `DataCatalogService` entry ids.
   - Reject duplicate node ids and missing parent ids.
2. Add `DataHierarchyService` ownership/access through `xq::core::ApplicationContext`.
   - Expose it through `DataHierarchy()`.
   - Future Project/Data UI should read from this hierarchy instead of hardcoding a flat data list.
3. Add C++ regression tests before implementation:
   - New hierarchy has a stable root.
   - Folders and data-entry nodes preserve parent/child order.
   - Duplicate ids fail without changing existing nodes.
   - Missing parent ids fail with a useful error.
   - Default `ApplicationContext` exposes a hierarchy service.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Data Hierarchy Persistence Integration

1. Extend `.xqproj` schema `2.0` persistence to include
   `DataHierarchyService`.
   - Save folder and data-entry nodes under project JSON.
   - Restore hierarchy nodes when opening a project.
   - Preserve node order, id, parent id, display name, kind, and catalog entry
     references.
   - Do not serialize the implicit root as an ordinary child node.
   - Reject unsupported hierarchy node kind values while opening.
2. Add `ProjectService` save/open overloads that accept both
   `DataCatalogService` and `DataHierarchyService`.
   - Keep metadata-only and catalog-only save/open behavior working.
   - Avoid making `ProjectService` own either service; pass services explicitly.
3. Update `ProjectSessionService` and `ApplicationContext` wiring so project
   save/open roundtrips the context hierarchy together with the catalog.
4. Add C++ regression tests before implementation:
   - Saving a project with catalog + hierarchy writes hierarchy nodes.
   - Opening a project restores folders and data-entry nodes in order.
   - Unknown hierarchy node kind values fail with a useful error.
   - Project session save/open restores context hierarchy.
5. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
6. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Project Session State Replacement

1. Strengthen project open semantics for the monolith session services.
   - Opening a project with catalog/hierarchy state must replace the current
     in-memory catalog and hierarchy, not append to stale session data.
   - Failed project opens must not mutate the existing project metadata,
     catalog, or hierarchy.
   - Keep existing metadata-only and catalog-only open behavior working.
2. Add reset/replace support to `DataCatalogService` and
   `DataHierarchyService`.
   - Use temporary service instances while parsing project JSON.
   - Commit parsed state to the live services only after the whole open
     operation validates.
3. Update `ProjectService` catalog and catalog+hierarchy open paths to use
   transactional replacement.
4. Add C++ regression tests before implementation:
   - Opening a second project replaces existing catalog entries.
   - Opening a second project replaces existing hierarchy nodes.
   - Failed hierarchy open leaves existing catalog and hierarchy untouched.
   - Project metadata is unchanged after a failed stateful open.
5. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
6. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Data Import Hierarchy Integration

1. Integrate `DataImportService` with `DataHierarchyService`.
   - Successful imports register catalog metadata and create project-tree
     hierarchy nodes in one workflow operation.
   - Keep existing catalog-only `DataImportService` construction working for
     narrow unit tests.
   - `ApplicationContext` should wire the importer to the context hierarchy.
2. Add deterministic hierarchy placement for imported entries.
   - Create or reuse a root-level role folder for the imported workflow role.
   - Add a data-entry node under that role folder referencing the catalog entry
     id.
   - Use stable folder/node ids so project persistence roundtrips are
     deterministic.
3. Keep failure behavior conservative.
   - Missing source paths and duplicate catalog ids must not mutate hierarchy.
   - Duplicate hierarchy node ids must fail before catalog mutation when the
     importer owns a hierarchy service.
4. Add C++ regression tests before implementation:
   - Import through `ApplicationContext` creates a role folder and data-entry
     hierarchy node.
   - Multiple image imports reuse the image folder and preserve data-node order.
   - Duplicate imports fail without adding hierarchy nodes.
   - Existing catalog-only importer tests keep passing.
5. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
6. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Data Selection Service Foundation

1. Add a monolith Core `xq::core::DataSelectionService`.
   - Keep it independent from MITK `DataNode` selection.
   - Track the selected hierarchy node id and resolved catalog entry id.
   - Validate hierarchy node ids and catalog entry ids against the live
     `DataHierarchyService` and `DataCatalogService`.
   - Emit selection-change signals only when the selected state changes.
2. Add selection entry points for workflow pages.
   - Select by hierarchy node id.
   - Select by catalog entry id, resolving to the first hierarchy data-entry
     node that references it.
   - Clear data selection.
3. Add `DataSelectionService` ownership/access through
   `xq::core::ApplicationContext`.
   - Expose it through `DataSelection()`.
   - Keep existing MITK active-node selection API unchanged.
4. Add C++ regression tests before implementation:
   - New data selection starts empty.
   - Selecting a hierarchy data node resolves the catalog entry id.
   - Selecting by catalog entry resolves the hierarchy node id.
   - Selecting the same data twice is a no-op.
   - Missing ids fail without mutating the current data selection.
   - Default `ApplicationContext` exposes a data selection service.
5. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
6. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Data Selection Lifecycle Integration

1. Integrate `DataSelectionService` with import and project-open lifecycle
   operations.
   - Successful imports through `ApplicationContext::DataImports()` should
     select the newly imported catalog entry and hierarchy data node.
   - Successful project opens through `ApplicationContext::ProjectSession()`
     should clear stale data selection because the live catalog/hierarchy state
     has been replaced.
   - Failed imports and failed project opens must not mutate the current data
     selection.
2. Keep service-level construction flexible.
   - Existing catalog-only and catalog+hierarchy `DataImportService`
     construction must keep working for narrow unit tests.
   - Project session construction should allow selection lifecycle wiring only
     when the application context provides a `DataSelectionService`.
3. Add C++ regression tests before implementation:
   - Import through the default context auto-selects the new entry.
   - Failed duplicate import leaves the previous data selection unchanged.
   - Successful project open clears stale data selection.
   - Failed project open leaves the previous data selection unchanged.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Data Management Service Foundation

1. Add a monolith Core `xq::core::DataManagementService`.
   - Coordinate `DataCatalogService`, `DataHierarchyService`,
     `DataSelectionService`, and `TaskRunner`.
   - Keep the first implementation metadata-only and independent from MITK
     data node deletion.
   - Support renaming an imported catalog entry and its hierarchy data node.
   - Support removing an imported catalog entry and all hierarchy data nodes
     that reference it.
2. Keep lifecycle behavior conservative.
   - Successful rename should preserve selection and update visible display
     names.
   - Successful remove should clear data selection only when the removed entry
     was selected.
   - Failed rename/remove operations must not mutate catalog, hierarchy, or
     selection.
3. Add `DataManagementService` ownership/access through
   `xq::core::ApplicationContext`.
   - Expose it through `DataManagement()`.
   - Future Project/Data UI should call this service instead of editing
     catalog and hierarchy services directly.
4. Add C++ regression tests before implementation:
   - Default context exposes the data management service.
   - Renaming an imported entry updates catalog and hierarchy display names.
   - Empty rename fails without mutating existing names.
   - Removing a selected entry removes catalog/hierarchy data nodes and clears
     selection.
   - Removing a missing entry fails without mutating existing selection.
5. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
6. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Data Change Notification Foundation

1. Add query-independent change notifications for monolith data services.
   - `DataCatalogService` should emit a signal when its ordered entries change.
   - `DataHierarchyService` should emit a signal when its ordered nodes change.
   - Signals should fire once for successful register/add/rename/remove/replace
     operations and should not fire for failed operations.
2. Preserve transaction behavior in higher-level services.
   - `DataImportService` and `DataManagementService` should still mutate live
     services only after validation succeeds.
   - Successful import should emit one catalog change and one hierarchy change
     for the live services.
   - Failed import/rename/remove should not emit live catalog or hierarchy
     changes.
3. Keep `DataSelectionService` behavior unchanged.
   - Existing selection-change signals remain the selection-specific event
     channel.
   - Data catalog/hierarchy change signals should not duplicate selection
     events.
4. Add C++ regression tests before implementation:
   - Registering catalog data emits one catalog change signal.
   - Failed duplicate catalog registration emits no catalog change signal.
   - Adding hierarchy folder/data emits hierarchy change signals.
   - Import through `ApplicationContext` emits one live catalog change and one
     live hierarchy change.
   - Failed duplicate import emits no additional live data change signals.
   - Rename/remove through `DataManagementService` each emit one catalog change
     and one hierarchy change.
5. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
6. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Data Hierarchy Qt Model Foundation

1. Add a Presentation-layer `xq::presentation::DataHierarchyModel`.
   - Implement a Qt `QAbstractItemModel` backed by
     `xq::core::DataHierarchyService`.
   - Expose display names through `Qt::DisplayRole`.
   - Expose stable hierarchy node ids through a custom role.
   - Preserve parent/child relationships from `DataHierarchyService`.
2. Keep the model reactive and UI-safe.
   - Listen to `DataHierarchyService::NodesChanged`.
   - Reset the model when the hierarchy changes so Project/Data UI can bind to
     it without polling.
   - Keep the first implementation read-only.
3. Build Presentation code as a testable monolith library.
   - Add `xqMonolithPresentation` for reusable Presentation components.
   - Link `XQMonolith` against the Presentation library instead of compiling
     all Presentation sources only into the executable.
4. Add C++/Qt regression tests before implementation:
   - A new model exposes the root's ordered children.
   - Child rows expose data-entry display names and stable node ids.
   - Import through `ApplicationContext` refreshes the model after hierarchy
     changes.
   - Rename/remove through `DataManagementService` refreshes visible model
     data and row counts.
5. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
6. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Project Data Panel Foundation

1. Add a persistent Project/Data tree to `xq::presentation::MainWindow`.
   - Create a `QTreeView` with object name `xqDataHierarchyView`.
   - Back the view with `xq::presentation::DataHierarchyModel`.
   - Place it in the left workflow area above or alongside the workflow
     navigation so data remains visible while switching workflow pages.
2. Keep the UI connected to Core services.
   - Selecting a data-entry hierarchy node in the tree should call
     `DataSelectionService::SelectHierarchyNode`.
   - Folder selection should not replace an existing data selection.
   - Successful import/rename/remove operations should update the tree through
     the existing model notification path.
3. Keep the first UI panel read-only.
   - Do not add context menus, rename actions, delete actions, or file dialogs
     in this slice.
   - Preserve existing workflow navigation behavior and MITK render host setup.
4. Add C++/Qt regression tests before implementation:
   - `MainWindow` exposes a hierarchy tree view backed by
     `DataHierarchyModel`.
   - Importing data through `ApplicationContext` refreshes the visible tree.
   - Selecting an imported data row updates `DataSelectionService`.
   - Selecting a folder row does not overwrite an existing data selection.
   - Workflow navigation still switches pages after adding the data panel.
5. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
6. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Data Panel Selection Synchronization

1. Synchronize Core data selection back into the Project/Data tree.
   - Listen to `DataSelectionService::SelectionChanged` in `MainWindow`.
   - When a selected hierarchy node id exists, locate the matching
     `DataHierarchyModel` index and make it the tree current index.
   - Expand parent folders as needed so externally selected data is visible.
2. Handle lifecycle clears conservatively.
   - Clearing data selection should clear the tree current selection.
   - Removing a selected entry should leave the tree without a stale current
     index after the model resets.
   - Selecting a missing hierarchy node should not crash or mutate the tree.
3. Keep feedback loops quiet.
   - Tree-driven selection should continue to call
     `SelectHierarchyNode` for data-entry rows.
   - Core-driven UI updates should not re-emit duplicate Core selection
     changes for the same node.
4. Add C++/Qt regression tests before implementation:
   - Importing data auto-selects the imported row in the tree.
   - Selecting a different catalog entry through `DataSelectionService`
     updates the tree current row.
   - Clearing selection clears the tree current index.
   - Removing the selected data entry clears stale tree selection.
5. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
6. Commit and push the verified XQ iteration.

## Active Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.
