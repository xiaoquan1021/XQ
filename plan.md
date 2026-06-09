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

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Data Panel Actions Foundation

1. Add a minimal command surface to the Project/Data tree panel.
   - Expose a `QAction` with object name `xqRemoveDataAction`.
   - Place the action in a small toolbar attached to the data tree panel.
   - Keep the first action set limited to removing the selected data entry.
2. Bind action state to Core selection.
   - `xqRemoveDataAction` should be disabled when there is no selected data
     catalog entry.
   - The action should become enabled after import or programmatic data
     selection.
   - Clearing/removing selection should disable the action again.
3. Execute removal through Core services.
   - Triggering the action should call `DataManagementService::RemoveEntry`
     for the selected catalog entry.
   - Successful removal should update catalog, hierarchy, selection, tree, and
     action state through existing service notifications.
   - Failed removal should post a diagnostic and keep UI state conservative.
4. Add C++/Qt regression tests before implementation:
   - MainWindow exposes `xqRemoveDataAction`.
   - The action is disabled with no selection.
   - Importing data enables the action.
   - Triggering the action removes the selected data and disables the action.
   - Triggering with stale/missing selection posts a diagnostic without
     crashing.
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

## Completed Phase: Monolith Project State Notification Foundation

1. Add project lifecycle notifications to Core.
   - `ProjectService` should emit `ProjectChanged` after successful
     `CreateProject` and successful `OpenProject` overloads.
   - The signal should include enough data for Presentation to update project
     title/status without immediately re-querying when possible.
   - Failed create/open operations must not emit the signal.
2. Keep save behavior conservative.
   - `SaveProject` should not emit `ProjectChanged` unless metadata changes in
     a future phase.
   - `ProjectSessionService::Open` should emit exactly one project change
     through the underlying successful `ProjectService::OpenProject` call.
3. Preserve current metadata semantics.
   - `CurrentProject()` should still return the active project after create or
     open.
   - Failed open should leave the previous current project untouched.
4. Add C++ regression tests before implementation:
   - Successful create emits one project change with the new metadata.
   - Save emits no project change.
   - Failed create/open emits no project change.
   - Successful open emits one project change and preserves loaded metadata.
   - `ProjectSessionService::Open` emits one project change through context.
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

## Completed Phase: Monolith Project Window State Foundation

1. Bind project lifecycle state into `xq::presentation::MainWindow`.
   - Listen to `ProjectService::ProjectChanged`.
   - Update the window title from `XQ` to include the active project name.
   - Add a stable status bar with object name `xqProjectStatusBar`.
2. Keep the status message conservative and useful.
   - New windows without an active project should show a neutral no-project
     message.
   - Successful project create/open should show the project name and project
     file path.
   - Failed open/save operations should not overwrite the last successful
     project state.
3. Keep the implementation as a shell concern.
   - Do not add project file dialogs or save/open buttons in this slice.
   - Use the `ProjectChanged` signal rather than polling `CurrentProject()`
     after every UI event.
4. Add C++/Qt regression tests before implementation:
   - MainWindow exposes `xqProjectStatusBar` and starts with title `XQ`.
   - Creating a project updates the title and status message.
   - Saving the project does not change project window state.
   - Failed open leaves title and status message unchanged.
   - ProjectSession open updates title and status message through
     `ProjectChanged`.
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

## Completed Phase: Monolith Project Save Action Foundation

1. Add a window-level save command for the active monolith project.
   - Expose a `QAction` with object name `xqSaveProjectAction`.
   - Place it in a small project toolbar with object name `xqProjectToolbar`.
   - Keep this slice limited to saving the already-active project; do not add
     file dialogs or Save As.
2. Bind action state to Core project lifecycle.
   - The action should be disabled when no project is active.
   - Successful project create/open should enable the action through
     `ProjectChanged`.
   - Failed open should leave the previous enabled state unchanged.
3. Execute save through Core services.
   - Triggering the action should call `ProjectSessionService::Save`.
   - Successful save should write the `.xqproj` file with current catalog and
     hierarchy metadata.
   - Failed save should post a diagnostic and keep action state conservative.
4. Add C++/Qt regression tests before implementation:
   - MainWindow exposes `xqSaveProjectAction` and starts disabled.
   - Creating a project enables the action.
   - Triggering save after import writes the project file with data catalog
     and hierarchy metadata.
   - Forced stale save without an active project posts a diagnostic and
     disables the action.
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

## Completed Phase: Monolith Task Diagnostics Bridge Foundation

1. Bridge task execution into application diagnostics.
   - `ApplicationContext` should listen to `TaskRunner::TaskFinished`.
   - Successful tasks should post a concise diagnostic containing the task
     name and success message when one exists.
   - Failed tasks should post a diagnostic containing the task name and failure
     message.
2. Keep diagnostics signal semantics stable.
   - Empty task messages should still produce a useful task-level diagnostic.
   - Diagnostics should be stored through existing `PostDiagnostic` so
     `Diagnostics()` and `DiagnosticPosted` stay the single public channel.
   - Empty/whitespace diagnostics should continue to be ignored by
     `PostDiagnostic`.
3. Avoid duplicate UI-specific logging.
   - Do not add direct service-to-widget logging in this slice.
   - `MainWindow` should continue listening only to
     `ApplicationContext::DiagnosticPosted`.
4. Add C++ regression tests before implementation:
   - Import through `ApplicationContext` posts one successful task diagnostic.
   - Failed duplicate import posts one failed task diagnostic.
   - ProjectSession save posts a successful save diagnostic even if the task
     message is empty.
   - Manual empty diagnostic posting remains ignored.
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

## Completed Phase: Monolith Diagnostics Panel Task Log Foundation

1. Make the MainWindow diagnostics panel a stable Presentation surface.
   - Give the diagnostics text log object name `xqDiagnosticsLog`.
   - Keep the existing diagnostics dock object name `xqDiagnosticsDock`.
   - Keep the log read-only.
2. Bind task diagnostics into the visible UI.
   - Diagnostics emitted through `ApplicationContext::DiagnosticPosted` should
     append to `xqDiagnosticsLog`.
   - Task diagnostics from import/save/open should appear without direct
     service-to-widget logging.
   - Manual diagnostics should continue to append through the same signal.
3. Keep this slice display-only.
   - Do not add clear/export/filter controls yet.
   - Do not change Core diagnostics storage behavior.
4. Add C++/Qt regression tests before implementation:
   - MainWindow exposes a read-only `xqDiagnosticsLog`.
   - Manual `PostDiagnostic` appends to the log.
   - Import through `ApplicationContext` appends the task diagnostic to the
     log.
   - Empty diagnostics remain absent from the visible log.
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

## Completed Phase: Monolith Workflow Page Identity Foundation

1. Bind workflow registry ids into MainWindow navigation and pages.
   - Navigation items should store `WorkflowDescriptor::Id` in
     `Qt::UserRole`.
   - Workflow pages should use stable object names
     `xqWorkflowPage_<workflow-id>`.
   - Workflow pages should keep their existing visible titles.
2. Preserve navigation behavior.
   - Page count and navigation count should still match
     `DefaultWorkflowRegistry()`.
   - Selecting a navigation row should still switch to the matching page.
   - The first workflow should still be selected at startup.
3. Keep this slice structural.
   - Do not implement workflow-specific page controls yet.
   - Do not change registry ordering or workflow titles.
4. Add C++/Qt regression tests before implementation:
   - Every navigation row stores the expected workflow id.
   - Every stacked page exposes the expected object name.
   - Navigation row changes still switch to the expected page.
   - Startup selection still lands on the first workflow.
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

## Completed Phase: Monolith Project Workflow Page Foundation

1. Turn the `project` workflow page into a real project overview surface.
   - Keep the stable page object name `xqWorkflowPage_project`.
   - Add stable labels for project name, project file path, schema version,
     and data item count.
   - Use object names `xqProjectPageName`, `xqProjectPagePath`,
     `xqProjectPageSchema`, and `xqProjectPageDataCount`.
2. Bind the page to Core state.
   - A new window with no project should show neutral no-project metadata.
   - Successful project create/open should update the page through
     `ProjectChanged`.
   - Data imports/removes should update the data item count through
     `DataCatalogService::EntriesChanged`.
   - Failed open should leave the last successful project metadata unchanged.
3. Keep this slice display-only.
   - Do not add file dialogs, create/open buttons, or editable fields yet.
   - Preserve existing workflow navigation and toolbar behavior.
4. Add C++/Qt regression tests before implementation:
   - Project workflow page exposes all overview labels.
   - Creating a project updates name/path/schema labels.
   - Importing/removing data updates the data count label.
   - Failed open leaves project page metadata unchanged.
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

## Completed Phase: Monolith Data Workflow Page Foundation

1. Turn the `data` workflow page into a selected-data overview surface.
   - Keep the stable page object name `xqWorkflowPage_data`.
   - Add labels with object names `xqDataPageSelection`,
     `xqDataPageCatalogId`, `xqDataPageDisplayName`, and
     `xqDataPageSourcePath`.
2. Bind the page to Core data state.
   - A new window with no selected data should show neutral no-selection
     metadata.
   - Successful import should update the page through
     `DataSelectionService::SelectionChanged`.
   - Rename/remove should update the page through catalog and selection
     changes.
   - Missing/stale selection should not crash and should show no selected data.
3. Keep this slice display-only.
   - Do not add import dialogs, rename controls, delete controls, or editing
     fields in this page yet.
   - Preserve existing data tree, toolbar actions, and workflow navigation.
4. Add C++/Qt regression tests before implementation:
   - Data workflow page exposes all selected-data labels.
   - Importing data updates selected id, display name, and source path labels.
   - Renaming selected data updates the display name label.
   - Removing selected data returns the page to no-selection metadata.
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

## Completed Phase: Monolith Workflow Selection Context Foundation

1. Add a Core workflow selection service.
   - Introduce `xq::core::WorkflowSelectionService`.
   - Default selected workflow id should be the first
     `DefaultWorkflowRegistry()` entry.
   - `SelectWorkflow(id)` should succeed only for known workflow ids.
   - Valid changes should emit `WorkflowChanged(id)` exactly once.
   - Re-selecting the current workflow should be a no-op with no signal.
   - Missing or invalid ids should fail without mutating current state.
2. Expose workflow selection through `ApplicationContext`.
   - Add `ApplicationContext::WorkflowSelection()`.
   - Keep registry ownership and ordering unchanged.
   - Do not add workflow-specific domain services in this slice.
3. Bind MainWindow navigation to Core workflow selection.
   - Startup navigation/page state should match the Core selected workflow id.
   - Selecting a navigation row should call
     `WorkflowSelectionService::SelectWorkflow`.
   - Programmatic Core workflow selection should update navigation and stacked
     page state when the id is known.
   - Invalid programmatic selections should leave UI state unchanged.
4. Add C++/Qt regression tests before implementation:
   - A default `ApplicationContext` exposes workflow selection service.
   - Default selected workflow id is the first registry id.
   - Selecting a valid workflow changes id and emits one signal.
   - Invalid workflow id fails with no mutation and no signal.
   - MainWindow navigation changes update Core selected workflow id.
   - Programmatic Core workflow selection updates navigation/page state.
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

## Completed Phase: Monolith Workflow Data Context Foundation

1. Add a Core workflow data context service.
   - Introduce `xq::core::WorkflowContextService`.
   - The service should combine `WorkflowSelectionService`,
     `DataSelectionService`, and `DataCatalogService`.
   - Expose a value snapshot with active workflow id/title, whether selected
     data is required, selected catalog entry id/display name/role, and
     whether the current selection is compatible with the active workflow.
   - Emit `ContextChanged()` only when the snapshot actually changes.
2. Define conservative workflow-to-data role requirements.
   - `project`, `data`, and `python-api` should not require selected data.
   - `image-preprocessing` should accept DICOM series and image data.
   - `path`, `segmentation-2d`, and `segmentation-3d` should accept image
     data.
   - `modeling` should accept segmentation and model data.
   - `meshing` should accept model and mesh data.
   - `flow-simulation`, `rom-simulation`, and `multiphysics` should accept
     mesh and simulation result data.
3. Expose workflow context through `ApplicationContext`.
   - Add `ApplicationContext::WorkflowContext()`.
   - Keep this slice Core-only; do not add page controls yet.
   - Future workflow pages should be able to consume this service instead of
     duplicating workflow/data eligibility logic.
4. Add C++ regression tests before implementation:
   - A default `ApplicationContext` exposes workflow context service.
   - Default snapshot is the first workflow and does not require selected data.
   - Selecting `image-preprocessing` with no selected data requires data and is
     not compatible.
   - Importing/selecting image data makes image preprocessing compatible.
   - Switching to `meshing` with image data is incompatible.
   - Selecting model data makes meshing compatible.
   - Workflow changes, selection changes, and relevant catalog changes emit
     context changes exactly once per snapshot mutation.
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

## Completed Phase: Monolith Workflow Context Status Page Foundation

1. Add stable Presentation labels for workflow data context.
   - Data-dependent workflow pages should expose a label named
     `xqWorkflowContextStatus_<workflow-id>`.
   - Cover `image-preprocessing`, `path`, `segmentation-2d`,
     `segmentation-3d`, `modeling`, `meshing`, `flow-simulation`,
     `rom-simulation`, and `multiphysics`.
   - Keep `project`, `data`, and `python-api` pages unchanged in this slice.
2. Bind labels to `WorkflowContextService`.
   - Selecting a data-dependent workflow with no selected data should show a
     neutral missing-input status.
   - A compatible selected data entry should show the selected display name.
   - An incompatible selected data entry should show an incompatible-input
     status.
   - Rename/import/selection changes should refresh through
     `WorkflowContextService::ContextChanged()`.
3. Keep this slice display-only.
   - Do not add workflow action buttons yet.
   - Do not start migrating image/path/segmentation algorithms yet.
   - Preserve existing navigation, project, data, diagnostics, and render host
     behavior.
4. Add C++/Qt regression tests before implementation:
   - Data-dependent workflow pages expose their context status label.
   - Selecting image preprocessing with no data shows missing-input status.
   - Importing/selecting image data updates image preprocessing status with
     the display name.
   - Switching to meshing with image data shows incompatible-input status.
   - Importing/selecting model data updates meshing status with the display
     name.
   - Renaming selected data refreshes the active workflow status label.
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

## Completed Phase: Monolith Workflow Primary Action Surface Foundation

1. Add stable primary action buttons to data-dependent workflow pages.
   - Each data-dependent workflow page should expose a button named
     `xqWorkflowPrimaryAction_<workflow-id>`.
   - Cover the same data-dependent workflow ids as the context status page.
   - Keep `project`, `data`, and `python-api` pages unchanged in this slice.
2. Bind button state to `WorkflowContextService`.
   - The active workflow button should be disabled when selected data is
     missing or incompatible.
   - The active workflow button should be enabled when selected data is
     compatible.
   - Import/selection/rename/workflow changes should refresh button state
     through the same context binding as the status label.
3. Add a safe placeholder command surface.
   - Triggering an enabled primary action should post a diagnostic through
     `ApplicationContext::PostDiagnostic`.
   - The diagnostic should include the workflow title and selected data display
     name.
   - Do not start migrating actual image/path/segmentation algorithms yet.
4. Add C++/Qt regression tests before implementation:
   - Data-dependent workflow pages expose primary action buttons.
   - Project/data/python-api pages do not expose primary action buttons.
   - Image preprocessing button starts disabled without selected data.
   - Importing/selecting image data enables image preprocessing action.
   - Clicking the enabled image preprocessing action posts a diagnostic with
     workflow title and selected data display name.
   - Switching to meshing with image data disables the meshing action.
   - Importing/selecting model data enables the meshing action.
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

## Completed Phase: Monolith Workflow Action Service Foundation

1. Add a Core workflow action service.
   - Introduce `xq::core::WorkflowActionService`.
   - The service should consume `WorkflowContextService`.
   - Expose `RequestActiveWorkflowAction(QString* message = nullptr)`.
   - Incompatible or missing selected data should return false and provide a
     useful message containing the active workflow title.
   - Compatible selected data should return true and provide a placeholder
     action request message containing workflow title and selected data display
     name.
2. Expose workflow actions through `ApplicationContext`.
   - Add `ApplicationContext::WorkflowActions()`.
   - Keep this slice placeholder-only; do not call image/path/segmentation
     algorithms yet.
3. Route MainWindow primary action clicks through Core.
   - `RunActiveWorkflowAction()` should call `WorkflowActionService`.
   - MainWindow may still post the returned message through diagnostics.
   - Keep existing workflow primary action button behavior and diagnostics
     stable.
4. Add C++ regression tests before implementation:
   - A default `ApplicationContext` exposes workflow action service.
   - Image preprocessing with no selected data rejects action request with a
     workflow-specific message.
   - Image preprocessing with selected image data accepts action request and
     returns the placeholder action message.
   - Meshing with selected image data rejects action request.
   - Meshing with selected model data accepts action request.
   - MainWindow primary action diagnostic remains unchanged when routed
     through the Core service.
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

## Completed Phase: Monolith Workflow Action Task Runner Foundation

1. Route successful workflow placeholder actions through `TaskRunner`.
   - Extend `WorkflowActionService` with
     `RunActiveWorkflowAction(QString* message = nullptr)`.
   - Compatible active workflow actions should create one task named
     `Run <WorkflowTitle>`.
   - The task message should remain the placeholder action request message.
   - Incompatible or missing selected data should return false with the same
     rejection message and should not create a task.
2. Bind MainWindow primary action clicks to the task-running API.
   - Successful primary action clicks should rely on the existing
     `ApplicationContext` task diagnostics bridge.
   - Failed primary action requests may still post the returned rejection
     message directly.
   - Avoid duplicate diagnostics on successful actions.
3. Keep this slice placeholder-only.
   - Do not start image/path/segmentation algorithms yet.
   - Do not change button object names or enablement behavior.
4. Add C++/Qt regression tests before implementation:
   - Running image preprocessing without selected data rejects and leaves
     task history unchanged.
   - Running image preprocessing with selected image data succeeds and records
     one `Run Image Preprocessing` task.
   - The recorded task message includes selected data display name.
   - Running meshing with selected image data rejects and does not add a task.
   - MainWindow primary action click now emits the task-bridge diagnostic and
     does not require direct UI diagnostic construction.
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

## Completed Phase: Monolith Task History Panel Foundation

1. Add a stable task history Presentation surface.
   - MainWindow should expose a table named `xqTaskHistoryTable`.
   - The table should live in a dock named `xqTaskHistoryDock`.
   - Columns should be `Task`, `Status`, and `Message`.
   - The table should be read-only and row-selectable.
2. Bind the table to `TaskRunner`.
   - A new window should render any existing `TaskRunner::History()` entries.
   - `TaskRunner::TaskFinished` should append one row per finished task.
   - Successful tasks should show status `Succeeded`.
   - Failed tasks should show status `Failed`.
   - Message text should match the task record message.
3. Cover both existing and new workflow operations.
   - Importing data should append an import task row.
   - A failed duplicate import should append a failed task row.
   - Running a compatible workflow primary action should append a workflow
     action task row.
4. Keep this slice display-only.
   - Do not add filtering, export, or clear-history controls yet.
   - Do not alter `TaskRunner` history semantics.
   - Keep diagnostics log behavior unchanged.
5. Add C++/Qt regression tests before implementation:
   - MainWindow exposes read-only `xqTaskHistoryTable` with the expected
     headers.
   - Importing data appends a succeeded import row.
   - Duplicate import appends a failed import row.
   - Running workflow primary action appends a succeeded `Run <WorkflowTitle>`
     row.
   - Diagnostics log still receives the task bridge diagnostic.
6. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
7. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Workflow Action Handler Dispatcher Foundation

1. Add per-workflow action handler registration to Core.
   - Extend `WorkflowActionService` with a handler type that receives the
     current `WorkflowContextSnapshot` and writes a task message.
   - Add `RegisterHandler(workflowId, handler, message)` and `HasHandler`.
   - Reject missing workflow ids, unknown workflow ids, and empty handlers.
2. Dispatch successful workflow tasks through registered handlers.
   - `RunActiveWorkflowAction` should keep rejecting missing or incompatible
     data before creating a task.
   - If the active workflow has a registered handler, the task body should call
     the handler.
   - If no handler is registered, preserve the current placeholder task
     behavior.
   - Task names should remain `Run <WorkflowTitle>`.
3. Keep UI behavior stable.
   - MainWindow primary actions should continue to call only
     `WorkflowActionService::RunActiveWorkflowAction`.
   - Existing placeholder diagnostics and task history behavior should remain
     stable when no handler is registered.
4. Add C++ regression tests before implementation:
   - Unknown workflow handler registration is rejected.
   - Empty handler registration is rejected.
   - Valid image preprocessing handler registration succeeds and is reported by
     `HasHandler`.
   - Running image preprocessing with a registered handler calls it once with
     the active snapshot.
   - The resulting task history row uses the handler-provided task message.
   - Running a workflow without a handler preserves placeholder behavior.
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

## Completed Phase: Monolith Domain Workflow Handler Registrar Foundation

1. Add a monolith Domain library.
   - Introduce `xqMonolithDomain`.
   - Keep the library dependent on `xqMonolithCore`, not Presentation.
   - Add namespace `xq::domain`.
2. Add default workflow action handler registration.
   - Introduce `xq::domain::RegisterDefaultWorkflowActionHandlers`.
   - Register handlers for data-dependent workflow ids:
     `image-preprocessing`, `path`, `segmentation-2d`, `segmentation-3d`,
     `modeling`, `meshing`, `flow-simulation`, `rom-simulation`, and
     `multiphysics`.
   - Do not register `project`, `data`, or `python-api` handlers in this
     slice.
   - Handler messages should contain the workflow title and selected data
     display name.
3. Wire monolith app bootstrap.
   - `main.cxx` should register default domain handlers after creating
     `ApplicationContext`.
   - Keep tests that instantiate `ApplicationContext::CreateDefault` without
     domain registration stable.
4. Keep this slice as dispatcher plumbing.
   - Do not call old algorithm utilities yet.
   - Do not alter MainWindow primary action button object names or state.
5. Add C++ regression tests before implementation:
   - Default domain handler registration returns the number of registered
     handlers.
   - Data-dependent workflow ids report handlers after registration.
   - `project`, `data`, and `python-api` do not report handlers.
   - Running image preprocessing after registration invokes the domain handler
     and stores the domain handler message in task history.
   - A workflow without domain registration preserves placeholder behavior in
     a plain `ApplicationContext`.
6. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
7. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Image Preprocessing Domain Service Foundation

1. Add a domain image-preprocessing workflow service.
   - Introduce `xq::domain::ImagePreprocessingWorkflowService`.
   - Keep the first implementation metadata-only and independent from
     Presentation widgets.
   - Accept `WorkflowContextSnapshot` input from Core.
   - Validate that the active workflow is `image-preprocessing`.
   - Validate that the selected data role is image-preprocessing compatible
     (`DICOMSeries` or `Image`).
2. Add service result/request behavior.
   - Return a typed result with success flag, source catalog entry id, selected
     data display name, and message.
   - Preserve the current domain handler task message for successful image
     preprocessing:
     `Image Preprocessing domain workflow accepted <data>.`
   - Reject missing or incompatible selected data with explicit messages.
3. Wire the domain registrar through the service.
   - The `image-preprocessing` handler registered by
     `RegisterDefaultWorkflowActionHandlers` should delegate to
     `ImagePreprocessingWorkflowService`.
   - Other data-dependent workflow handlers may keep the generic domain
     placeholder for this slice.
4. Add C++ regression tests before implementation:
   - Direct service execution succeeds for selected image data.
   - Direct service execution succeeds for selected DICOM-series data.
   - Direct service execution rejects the wrong workflow id.
   - Direct service execution rejects incompatible selected data roles.
   - Registrar image-preprocessing dispatch still records the existing domain
     handler task message.
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

## Completed Phase: Monolith Image Preprocessing Operation Catalog Foundation

1. Add an image-preprocessing operation catalog to the Domain service.
   - Introduce `xq::domain::ImagePreprocessingOperationDescriptor`.
   - Expose a deterministic
     `ImagePreprocessingWorkflowService::Operations()` list.
   - Keep descriptors metadata-only in this slice.
2. Cover the old image-processing utility surface as first catalog entries.
   - Include operation ids/titles for binary threshold, connected threshold,
     Gaussian smoothing, morphology open/close, crop, and resample.
   - Keep marching cubes out of image preprocessing for this slice because it
     produces surface/model output and belongs closer to modeling.
3. Add operation lookup.
   - Add `FindOperation(operationId)` returning a descriptor pointer or null.
   - Trim operation ids before lookup.
   - Preserve deterministic operation order for future UI action/tool panels.
4. Add C++ regression tests before implementation:
   - Operation list exposes exactly the expected six metadata descriptors.
   - Operation ids are stable and ordered.
   - Operation titles are user-facing and non-empty.
   - `FindOperation` finds trimmed ids.
   - `FindOperation` returns null for unknown ids.
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

## Completed Phase: Monolith Image Preprocessing Operation Request Foundation

1. Add metadata-only operation request execution to the image-preprocessing
   service.
   - Introduce
     `ImagePreprocessingWorkflowService::RunOperation(snapshot, operationId)`.
   - Reuse the existing image-preprocessing snapshot validation.
   - Validate operation ids through `FindOperation`.
2. Extend result metadata for operation requests.
   - Add operation id and operation title fields to
     `ImagePreprocessingWorkflowResult`.
   - Successful operation requests should return selected source data metadata,
     operation metadata, and a deterministic message.
   - Keep `Run(snapshot)` behavior and message stable.
3. Keep this slice metadata-only.
   - Do not call `xq_ImageProcessingUtils` yet.
   - Do not mutate MITK `DataStorage` or `DataCatalogService` yet.
4. Add C++ regression tests before implementation:
   - Running a known operation accepts compatible image data.
   - Operation lookup trims ids before execution.
   - Operation result exposes operation id and title.
   - Unknown operation ids fail with an explicit message.
   - Incompatible selected data fails before reporting operation success.
   - Existing `Run(snapshot)` message remains unchanged.
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

## Completed Phase: Monolith Image Preprocessing Operation Parameter Schema

1. Add metadata-only parameter descriptors for image-preprocessing operations.
   - Introduce `ImagePreprocessingParameterDescriptor`.
   - Add parameter id, title, value type, and required flag.
   - Keep descriptors independent from Qt widgets and old algorithm calls.
2. Extend operation descriptors with ordered parameter lists.
   - Binary threshold: lower, upper, inside value, outside value.
   - Connected threshold: lower, upper, seeds.
   - Gaussian smoothing: sigma.
   - Morphology open/close: radius.
   - Crop: origin x/y/z and size x/y/z.
   - Resample: spacing x/y/z.
3. Add supported parameter value types.
   - Numeric scalar.
   - Integer scalar.
   - Integer point list for connected-threshold seeds.
4. Add C++ regression tests before implementation:
   - Binary threshold exposes four numeric required parameters.
   - Gaussian smoothing exposes a required numeric sigma parameter.
   - Morphology exposes a required integer radius parameter.
   - Crop exposes six required integer parameters in stable order.
   - Resample exposes three required numeric spacing parameters.
   - Connected threshold exposes lower/upper numeric parameters and required
     seed list parameter.
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

## Completed Phase: Monolith Image Preprocessing Parameter Validation Foundation

1. Add metadata-only parameter validation to the image-preprocessing service.
   - Introduce `ImagePreprocessingParameterValidationResult`.
   - Introduce
     `ImagePreprocessingWorkflowService::ValidateOperationParameters`.
   - Accept operation id and `QVariantMap` parameter values.
2. Validate operation and required parameters.
   - Unknown operation ids fail with the existing operation-not-found message.
   - Missing required parameters fail with an explicit parameter id.
   - Known operations with all required parameters succeed.
3. Validate value categories.
   - Numeric scalar accepts numeric `QVariant` values.
   - Integer scalar accepts integer `QVariant` values.
   - Integer point list accepts a non-empty `QVariantList` of 3-value integer
     points.
4. Keep this slice metadata-only.
   - Do not execute `xq_ImageProcessingUtils`.
   - Do not mutate MITK `DataStorage`, `DataCatalogService`, or project files.
5. Add C++ regression tests before implementation:
   - Valid Gaussian smoothing parameter map succeeds.
   - Missing sigma fails with an explicit message.
   - Valid crop integer parameters succeed.
   - Wrong crop integer type fails.
   - Valid connected-threshold seed list succeeds.
   - Unknown operation id fails with the existing operation-not-found message.
6. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
7. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Image Preprocessing Parameterized Operation Request

1. Add parameterized operation request execution to the image-preprocessing
   service.
   - Introduce an overload:
     `RunOperation(snapshot, operationId, QVariantMap parameters)`.
   - Reuse existing snapshot validation.
   - Reuse `ValidateOperationParameters` before reporting operation success.
2. Preserve existing request behavior.
   - The existing `RunOperation(snapshot, operationId)` overload should remain
     available for metadata-only requests.
   - `Run(snapshot)` behavior and message should remain unchanged.
3. Return deterministic metadata-only results.
   - Successful parameterized requests should expose source catalog entry id,
     selected data display name, normalized operation id, operation title, and
     the same operation success message as the metadata-only overload.
   - Invalid parameters should fail with the validator message and should not
     report operation metadata as successful.
4. Keep this slice metadata-only.
   - Do not execute `xq_ImageProcessingUtils`.
   - Do not mutate MITK `DataStorage`, `DataCatalogService`, or project files.
5. Add C++ regression tests before implementation:
   - Valid parameterized Gaussian smoothing request succeeds.
   - Successful parameterized request exposes operation metadata.
   - Missing required Gaussian sigma fails with the validator message.
   - Unknown operation ids fail with the existing operation-not-found message.
   - Existing metadata-only `RunOperation(snapshot, operationId)` remains
     stable.
6. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
7. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Image Preprocessing Infrastructure Adapter Foundation

1. Add a monolith Infrastructure library.
   - Introduce `xqMonolithInfrastructure`.
   - Keep it dependent on `xqMonolithDomain` and legacy algorithm modules as
     needed.
   - Add namespace `xq::infrastructure`.
2. Add a first image-preprocessing algorithm adapter.
   - Introduce `ImagePreprocessingAlgorithmAdapter`.
   - Keep the first adapter focused on Gaussian smoothing only.
   - Accept a `vtkImageData*` input and `QVariantMap` parameters.
   - Use the domain service to validate the `gaussian-smoothing` parameters.
   - Delegate valid requests to `xq_ImageProcessingUtils::SmoothGaussian`.
3. Keep Domain independent from legacy algorithms.
   - Do not link `xqMonolithDomain` to `xqModuleImageProcessing`.
   - Do not mutate MITK `DataStorage`, `DataCatalogService`, or project files.
   - Keep this slice as an algorithm smoke adapter, not UI integration.
4. Add C++ regression tests before implementation:
   - Adapter rejects null input with the legacy diagnostic.
   - Adapter rejects missing sigma through domain parameter validation.
   - Adapter runs Gaussian smoothing on a small VTK image and returns a non-null
     image with matching dimensions.
   - Adapter does not report operation success for invalid parameters.
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

## Completed Phase: Monolith Binary Threshold Infrastructure Adapter

1. Extend the image-preprocessing algorithm adapter with binary threshold.
   - Add `RunBinaryThreshold(vtkImageData*, QVariantMap parameters)`.
   - Use the domain service to validate `binary-threshold` parameters.
   - Delegate valid requests to `xq_ImageProcessingUtils::BinaryThreshold`.
2. Preserve adapter layering.
   - Keep Domain independent from legacy algorithms.
   - Keep Infrastructure as the only monolith layer linking to
     `xqModuleImageProcessing` in this slice.
3. Keep this slice as algorithm smoke coverage.
   - Do not mutate MITK `DataStorage`, `DataCatalogService`, or project files.
   - Do not add UI controls yet.
4. Add C++ regression tests before implementation:
   - Adapter rejects null input with the legacy binary-threshold diagnostic.
   - Adapter rejects missing threshold parameters through domain validation.
   - Adapter runs binary threshold on a small VTK image and returns a non-null
     image with matching dimensions.
   - Adapter output contains the expected inside/outside values for a simple
     voxel sample.
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

## Completed Phase: Monolith Connected Threshold Infrastructure Adapter

1. Extend the image-preprocessing algorithm adapter with connected threshold.
   - Add `RunConnectedThreshold(vtkImageData*, QVariantMap parameters)`.
   - Use the domain service to validate `connected-threshold` parameters.
   - Convert the validated integer point-list `seeds` parameter into
     `std::vector<std::array<int, 3>>`.
   - Delegate valid requests to `xq_ImageProcessingUtils::ConnectedThreshold`.
2. Preserve adapter layering.
   - Keep Domain independent from legacy algorithms.
   - Keep Infrastructure as the only monolith layer linking to
     `xqModuleImageProcessing` in this slice.
3. Keep this slice as algorithm smoke coverage.
   - Do not mutate MITK `DataStorage`, `DataCatalogService`, or project files.
   - Do not add UI controls yet.
4. Add C++ regression tests before implementation:
   - Adapter rejects null input with the legacy connected-threshold diagnostic.
   - Adapter rejects missing seeds through domain validation.
   - Adapter runs connected threshold on a small VTK image and returns a
     non-null image with matching dimensions.
   - Adapter output marks the seeded in-range voxel as selected.
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

## Completed Phase: Monolith Morphology Infrastructure Adapter

1. Extend the image-preprocessing algorithm adapter with morphology open/close.
   - Add `RunMorphologyOpenClose(vtkImageData*, QVariantMap parameters)`.
   - Use the domain service to validate `morphology-open-close` parameters.
   - Delegate valid requests to
     `xq_ImageProcessingUtils::MorphologicalOpenClose`.
2. Preserve adapter layering.
   - Keep Domain independent from legacy algorithms.
   - Keep Infrastructure as the only monolith layer linking to
     `xqModuleImageProcessing` in this slice.
3. Keep this slice as algorithm smoke coverage.
   - Do not mutate MITK `DataStorage`, `DataCatalogService`, or project files.
   - Do not add UI controls yet.
4. Add C++ regression tests before implementation:
   - Adapter rejects null input with the legacy morphology diagnostic.
   - Adapter rejects missing radius through domain validation.
   - Adapter forwards invalid radius values to the legacy diagnostic.
   - Adapter runs morphology open/close on a small binary VTK image and returns
     a non-null image with matching dimensions.
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

## Completed Phase: Monolith Crop Infrastructure Adapter

1. Extend the image-preprocessing algorithm adapter with crop.
   - Add `RunCrop(vtkImageData*, QVariantMap parameters)`.
   - Use the domain service to validate `crop` parameters.
   - Delegate valid requests to `xq_ImageProcessingUtils::Crop`.
2. Preserve adapter layering.
   - Keep Domain independent from legacy algorithms.
   - Keep Infrastructure as the only monolith layer linking to
     `xqModuleImageProcessing` in this slice.
3. Keep this slice as algorithm smoke coverage.
   - Do not mutate MITK `DataStorage`, `DataCatalogService`, or project files.
   - Do not add UI controls yet.
4. Add C++ regression tests before implementation:
   - Adapter rejects null input with the legacy crop diagnostic.
   - Adapter rejects missing crop parameters through domain validation.
   - Adapter forwards out-of-bounds crop requests to the legacy diagnostic.
   - Adapter runs crop on a small VTK image and returns a non-null image with
     the requested output dimensions.
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

## Completed Phase: Monolith Resample Infrastructure Adapter

1. Extend the image-preprocessing algorithm adapter with resample.
   - Add `RunResample(vtkImageData*, QVariantMap parameters)`.
   - Use the domain service to validate `resample` parameters.
   - Delegate valid requests to `xq_ImageProcessingUtils::Resample`.
2. Preserve adapter layering.
   - Keep Domain independent from legacy algorithms.
   - Keep Infrastructure as the only monolith layer linking to
     `xqModuleImageProcessing` in this slice.
3. Keep this slice as algorithm smoke coverage.
   - Do not mutate MITK `DataStorage`, `DataCatalogService`, or project files.
   - Do not add UI controls yet.
4. Add C++ regression tests before implementation:
   - Adapter rejects null input with the legacy resample diagnostic.
   - Adapter rejects missing spacing parameters through domain validation.
   - Adapter forwards non-positive spacing requests to the legacy diagnostic.
   - Adapter runs resample on a small VTK image and returns a non-null image
     with the expected output spacing and dimensions.
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

## Completed Phase: Monolith Image Preprocessing Operation Dispatch Adapter

1. Extend the image-preprocessing algorithm adapter with operation-id dispatch.
   - Add `RunOperation(QString operationId, vtkImageData*, QVariantMap parameters)`.
   - Route each Domain catalog operation to the matching Infrastructure adapter
     method.
   - Reject unknown operation ids with the Domain operation-not-found
     diagnostic.
2. Preserve adapter layering.
   - Keep Domain independent from legacy algorithms.
   - Keep Infrastructure as the only monolith layer linking to
     `xqModuleImageProcessing` in this slice.
3. Keep this slice as a dispatch/service boundary.
   - Do not mutate MITK `DataStorage`, `DataCatalogService`, or project files.
   - Do not add UI controls yet.
   - Do not add a second operation switch in Presentation.
4. Add C++ regression tests before implementation:
   - Dispatch rejects unknown operation ids with the Domain diagnostic.
   - Dispatch routes all six Domain preprocessing operation ids to their
     existing adapter implementations.
   - Dispatch preserves the per-operation result semantics for image-changing
     operations such as crop and resample.
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

## Completed Phase: Monolith Image Preprocessing Execution Service

1. Add an Infrastructure execution service for image preprocessing.
   - Introduce request/result structs carrying `WorkflowContextSnapshot`,
     operation id, parameters, input `vtkImageData*`, output image, and Domain
     operation metadata.
   - Use `ImagePreprocessingWorkflowService::RunOperation` for workflow and
     parameter validation.
   - Use `ImagePreprocessingAlgorithmAdapter::RunOperation` for algorithm
     execution after Domain validation succeeds.
2. Preserve adapter layering.
   - Keep Domain independent from legacy algorithms and VTK execution.
   - Keep Infrastructure as the only monolith layer linking to
     `xqModuleImageProcessing` in this slice.
3. Keep this slice as an execution boundary only.
   - Do not mutate MITK `DataStorage`, `DataCatalogService`, or project files.
   - Do not add UI controls yet.
   - Do not register the service into `ApplicationContext` yet.
4. Add C++ regression tests before implementation:
   - Service rejects incompatible workflow snapshots through Domain validation.
   - Service rejects unknown operation ids through Domain validation.
   - Service forwards algorithm diagnostics for missing input images.
   - Service runs a valid crop operation and returns Domain metadata plus the
     output image.
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

## Completed Phase: Monolith Image Preprocessing MITK Image Adapter

1. Add an Infrastructure adapter for MITK/VTK image boundaries.
   - Extract `vtkImageData*` from a selected `mitk::DataNode` containing a
     usable `mitk::Image`.
   - Create a deep-copied `mitk::Image` from algorithm output `vtkImageData`.
   - Return structured diagnostics instead of UI message boxes.
2. Preserve adapter layering.
   - Keep Domain independent from MITK/VTK conversion details.
   - Keep this adapter in Infrastructure for future workflow action handlers.
3. Keep this slice as an image conversion boundary only.
   - Do not mutate MITK `DataStorage`, `DataCatalogService`, or project files.
   - Do not add UI controls yet.
   - Do not create result `DataNode`s yet.
4. Add C++ regression tests before implementation:
   - Adapter rejects a null input node.
   - Adapter rejects a node without a usable `mitk::Image`.
   - Adapter extracts `vtkImageData*` from a valid MITK image node.
   - Adapter creates a deep-copied MITK image from VTK output.
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

## Completed Phase: Monolith Image Preprocessing Result Node Factory

1. Add an Infrastructure factory for preprocessing result nodes.
   - Create a `mitk::DataNode` from an
     `ImagePreprocessingExecutionResult` image.
   - Use the source node name plus an operation suffix for the result name.
   - Set image-processing metadata properties matching the legacy view.
2. Preserve adapter layering.
   - Keep Domain independent from MITK result node creation.
   - Keep this factory in Infrastructure for future workflow action handlers.
3. Keep this slice as node creation only.
   - Do not add the result node to MITK `DataStorage`.
   - Do not mutate `DataCatalogService` or project files.
   - Do not add UI controls yet.
4. Add C++ regression tests before implementation:
   - Factory rejects execution results without an output image.
   - Factory creates a named image result node for a valid crop execution.
   - Factory sets legacy image-processing metadata and generated pipeline
     properties on the result node.
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

## Completed Phase: Monolith Image Preprocessing Storage Commit Service

1. Add an Infrastructure service that commits preprocessing results to
   `mitk::DataStorage`.
   - Accept `mitk::DataStorage`, source `mitk::DataNode`, workflow snapshot,
     operation id, parameters, and result suffix.
   - Extract the source VTK image through the MITK image adapter.
   - Execute the operation through the execution service.
   - Create the result node through the result node factory.
   - Add the result node under the source node in `DataStorage`.
2. Preserve adapter layering.
   - Keep Domain independent from storage mutation.
   - Keep this storage commit path in Infrastructure.
3. Keep this slice as storage mutation only.
   - Do not mutate `DataCatalogService` or project files.
   - Do not add UI controls yet.
   - Do not register the service into `ApplicationContext` yet.
4. Add C++ regression tests before implementation:
   - Service rejects a missing `DataStorage`.
   - Service rejects an unusable source image node through the MITK adapter.
   - Service executes a valid crop request, creates a result image node, and
     adds it under the source node in `DataStorage`.
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

## Completed Phase: Monolith Image Preprocessing Catalog Commit Service

1. Add an Infrastructure service that registers committed preprocessing
   outputs in monolith catalog state.
   - Accept `DataCatalogService`, `DataHierarchyService`, result catalog id,
     and a successful storage commit result.
   - Register a generated image catalog entry using a stable virtual source path
     such as `xq://generated/image-preprocessing/<entry-id>`.
   - Add the generated entry under the Images hierarchy folder.
2. Preserve adapter layering.
   - Keep Domain independent from catalog/hierarchy mutation.
   - Keep this image-preprocessing-specific catalog commit in Infrastructure.
3. Keep this slice as catalog/hierarchy mutation only.
   - Do not mutate MITK `DataStorage`.
   - Do not write project files.
   - Do not add UI controls yet.
4. Add C++ regression tests before implementation:
   - Service rejects missing catalog and hierarchy services.
   - Service rejects failed storage commit results.
   - Service registers a successful generated image entry with display name,
     virtual source path, role, and hierarchy node.
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

## Completed Phase: Monolith Image Preprocessing Application Commit Service

1. Add an Infrastructure application service that composes the completed image
   preprocessing commit pipeline.
   - Accept `mitk::DataStorage`, `DataCatalogService`, `DataHierarchyService`,
     source `mitk::DataNode`, workflow snapshot, operation id, parameters,
     result catalog id, and result suffix.
   - Preflight generated catalog and hierarchy destination ids before running
     the algorithm so duplicate metadata targets do not create orphan
     `DataStorage` results.
   - Run `ImagePreprocessingStorageCommitService`, then
     `ImagePreprocessingCatalogCommitService`.
2. Preserve application-layer boundaries.
   - Keep Domain independent from MITK, catalog, and hierarchy mutation.
   - Keep this as Infrastructure orchestration for future workflow action
     handlers.
3. Keep this slice free of UI and project writes.
   - Do not register the service into `ApplicationContext` yet.
   - Do not add operation controls to Presentation yet.
   - Do not save `.xqproj` files in this service.
4. Add C++ regression tests before implementation:
   - Service rejects duplicate generated catalog/hierarchy targets before
     mutating `DataStorage`.
   - Service rejects storage failures without catalog/hierarchy registration.
   - Service executes a valid crop request, adds the result node under the
     source node, and registers the generated catalog and hierarchy entry.
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

## Completed Phase: Monolith Image Preprocessing Workflow Action Handler

1. Add an Infrastructure workflow action handler registrar for image
   preprocessing.
   - Accept `ApplicationContext` plus configurable operation id, parameters,
     and result suffix.
   - Register an `image-preprocessing` handler in `WorkflowActionService`.
   - Use the active MITK node, current workflow snapshot, monolith
     `DataStorage`, `DataCatalogService`, and `DataHierarchyService` to call
     `ImagePreprocessingApplicationCommitService`.
   - Generate deterministic result catalog ids from selected catalog id and
     operation id, such as `<source-id>-<operation-id>`.
2. Preserve UI boundaries.
   - Do not replace the monolith `main.cxx` default registrar yet.
   - Do not add Presentation operation controls yet.
   - Keep this registrar available for future UI wiring and tests.
3. Add C++ regression tests before implementation:
   - Registrar rejects missing operation ids.
   - Registered handler fails through `WorkflowActionService` when no active
     MITK source node is available.
   - Registered handler executes a valid crop request through
     `WorkflowActionService`, records task history, adds the result node under
     the source node, and registers catalog/hierarchy metadata.
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

## Completed Phase: Monolith Data Node Registry Foundation

1. Add a monolith Core `xq::core::DataNodeRegistryService`.
   - Bind catalog entry ids to MITK `DataNode` pointers.
   - Support lookup by catalog id, ordered listing of bound catalog ids,
     rebinding an existing id to a new node, and removing bindings.
   - Reject empty catalog ids and null data nodes.
2. Add `DataNodeRegistryService` ownership/access through
   `xq::core::ApplicationContext`.
   - Future workflow action handlers should resolve selected data through this
     registry instead of relying only on a transient active node.
3. Keep this slice Core-only.
   - Do not change the image-preprocessing handler yet.
   - Do not add file decoding/import UI yet.
   - Do not persist node pointers in `.xqproj`.
4. Add C++ regression tests before implementation:
   - New registry starts empty.
   - Invalid binds fail with clear diagnostics.
   - Bind/find/list works for a valid node.
   - Rebinding replaces the node without duplicating the catalog id.
   - Removing a binding clears lookup.
   - `ApplicationContext` exposes an empty registry.
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

## Completed Phase: Monolith Image Preprocessing Handler DataNode Registry Integration

1. Update the Infrastructure image-preprocessing workflow action handler to
   resolve source MITK nodes through Core catalog-node bindings.
   - Prefer `ApplicationContext::DataNodes()` lookup using
     `WorkflowContextSnapshot::SelectedCatalogEntryId`.
   - Fall back to `ApplicationContext::ActiveNode()` for current manual/test
     workflows.
   - Preserve the existing missing-node diagnostic when neither source is
     available.
2. Preserve handler boundaries.
   - Do not add UI controls yet.
   - Do not change Domain validation or preprocessing algorithm services.
   - Do not persist node pointers.
3. Add C++ regression tests before implementation:
   - A registered crop handler succeeds when the selected catalog id is bound
     to a MITK `DataNode` in `DataNodeRegistryService`, even without an active
     node.
   - Existing active-node fallback behavior still succeeds.
   - Missing registry and active node still fails.
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

## Completed Phase: Monolith Data Management DataNode Registry Cleanup

1. Integrate `DataNodeRegistryService` into Core data removal lifecycle.
   - Allow `DataManagementService` to receive a data-node registry from
     `ApplicationContext`.
   - When removing a catalog entry succeeds, remove any bound MITK data node
     registry entry for that catalog id.
   - Treat missing registry bindings as harmless; metadata-only imports should
     still remove cleanly.
2. Preserve transactional behavior.
   - Failed catalog/hierarchy removes must not clear data-node bindings.
   - Rename should not alter node bindings.
3. Add C++ regression tests before implementation:
   - Removing a bound selected entry clears its data-node registry binding.
   - Removing metadata-only entries still succeeds.
   - Failed remove keeps existing registry bindings.
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

## Completed Phase: Monolith Data Node Import Service Foundation

1. Add an Infrastructure `DataNodeImportService` for already-created MITK data
   nodes.
   - Accept `mitk::DataStorage`, Core `DataImportService`,
     `DataNodeRegistryService`, a `DataImportRequest`, and a MITK
     `DataNode`.
   - Run metadata import through the existing Core service.
   - Add the node to `DataStorage` and bind the imported catalog id to the node
     in `DataNodeRegistryService`.
2. Keep this slice as a node-import bridge only.
   - Do not decode files from disk yet.
   - Do not add UI import dialogs yet.
   - Do not change project persistence.
3. Preserve failure behavior.
   - Reject missing storage, import service, registry, and node.
   - Failed metadata imports must not mutate `DataStorage` or node registry.
4. Add C++ regression tests before implementation:
   - Service rejects missing dependencies with clear diagnostics.
   - Failed metadata import does not add storage nodes or registry bindings.
   - Valid import adds the node to `DataStorage`, registers catalog/hierarchy
     metadata, selects the imported entry through Core import behavior, and
     binds the catalog id to the node.
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

## Completed Phase: Monolith MITK File Import Service Foundation

1. Add an Infrastructure `MitkFileImportService` that imports one local file
   into the monolith data model.
   - Use MITK file loading as the production reader.
   - Wrap the loaded `mitk::BaseData` in a `mitk::DataNode`.
   - Reuse `DataNodeImportService` to add the node to `DataStorage`, register
     Core metadata and hierarchy, select the imported entry, and bind the
     catalog id to the node registry.
2. Keep this slice as a service boundary only.
   - Do not add Qt file dialogs or data workflow buttons yet.
   - Do not implement DICOM directory or multi-object import yet.
   - Do not change project persistence.
3. Preserve failure behavior.
   - Reject missing storage, import service, registry, source path, reader
     failures, empty reader output, and multi-object reader output.
   - Reader and metadata failures must not mutate `DataStorage`, Core catalog,
     hierarchy, selection, or node registry.
4. Add C++ regression tests before implementation:
   - Service rejects missing dependencies and source path with clear
     diagnostics.
   - Reader failures and empty loads do not mutate application state.
   - Duplicate metadata import after a successful reader load does not add a
     storage node or registry binding.
   - Valid import names the node, adds it to `DataStorage`, registers Core
     metadata/hierarchy, selects the imported entry, and binds the catalog id
     to the MITK node.
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

## Completed Phase: Monolith Data Import Action Shell

1. Add a Presentation-level import command boundary for the monolith data
   toolbar.
   - Expose a small `DataImportCommand` interface that can be injected into
     `MainWindow`.
   - Keep this slice independent from real file dialogs and DICOM directories.
   - Default to a clear diagnostic when no import command is configured.
2. Add a data toolbar import action.
   - Object name: `xqImportDataAction`.
   - Enabled by default.
   - Triggering it must call the injected command if one is configured.
3. Preserve UI refresh behavior.
   - Successful imports refresh the data workflow page, tree state, project
     data count, and data actions through existing services/signals.
   - Failed or missing commands must not mutate data catalog, hierarchy,
     selection, or MITK storage.
4. Add C++ regression tests before implementation:
   - MainWindow exposes an enabled import action.
   - Triggering the action without a command posts a diagnostic and does not
     mutate data state.
   - Triggering the action with a test command imports data, selects it, and
     refreshes visible data UI/actions.
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

## Completed Phase: Monolith MITK File Data Import Command

1. Add an Infrastructure command that implements the Presentation
   `DataImportCommand` interface.
   - Use an injectable path provider instead of opening a real `QFileDialog`
     in this slice.
   - Use `MitkFileImportService` and injectable `MitkFileReader` for file
     loading.
   - Build a single-file `DataImportRequest` from the chosen path, using the
     file name for display-name fallback.
2. Preserve command behavior.
   - Missing path provider returns a clear failure message and no mutations.
   - User cancellation returns a non-mutating "cancelled" message.
   - Reader/import failures propagate the underlying diagnostic.
   - Successful import registers catalog/hierarchy/selection/storage/node
     bindings through `MitkFileImportService`.
3. Keep scope narrow.
   - Do not wire a real file dialog into `MainWindow` yet.
   - Do not add DICOM directory, multiple object, or project persistence
     behavior in this slice.
4. Add C++ regression tests before implementation:
   - Missing provider and cancelled provider do not mutate state.
   - Reader failure propagates the service diagnostic and does not mutate state.
   - Successful command invokes the provider, imports the selected file, selects
     the catalog entry, and leaves MITK storage/node registry bound.
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

## Completed Phase: Monolith Qt File Import Wiring

1. Move the file-path selection port into Core.
   - Add `FileImportPathProvider` alongside `DataImportCommand`.
   - Keep `MitkFileDataImportCommand` depending on Core ports, not Qt widgets.
2. Add a Presentation Qt file-path provider.
   - Use `QFileDialog::getOpenFileName` in production.
   - Keep provider construction and filter text testable without opening a
     native dialog.
3. Wire the monolith executable composition root.
   - Create a Qt file path provider.
   - Create a `MitkFileDataImportCommand`.
   - Inject the command into `MainWindow` before showing the window.
4. Keep scope narrow.
   - Do not add DICOM directory import or multi-file selection.
   - Do not add project persistence changes.
5. Add C++ regression tests before implementation:
   - Qt provider exposes the expected import dialog caption and medical image
     file filters.
   - `MitkFileDataImportCommand` still depends on the Core path-provider port.
   - Monolith window composition injects an import command so triggering Import
     no longer posts the "not configured" diagnostic.
6. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
7. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith Import Render Refresh

1. Add a Core render-refresh port.
   - Provide a small interface that can refresh views for the current
     `mitk::DataStorage`.
   - Keep command tests independent from the global MITK rendering manager.
2. Add a MITK Infrastructure implementation.
   - Use `mitk::RenderingManager::InitializeViewsByBoundingObjects()`.
   - Call `mitk::RenderingManager::RequestUpdateAll()` after initialization.
3. Integrate render refresh into file import command.
   - Successful `MitkFileDataImportCommand` imports call the optional refresh
     service.
   - Cancelled and failed imports do not refresh.
4. Wire production composition.
   - Create the MITK render refresh service in `CreateConfiguredMainWindow()`.
   - Pass it to the import command along with the file path provider.
5. Add C++ regression tests before implementation:
   - Successful import calls the refresh service exactly once with the
     application `DataStorage`.
   - Missing provider, cancellation, and reader failure do not refresh.
   - Composition root owns a refresh service and keeps import wiring intact.
6. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
7. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Monolith File Import Role Inference

1. Teach the monolith single-file import command to infer the catalog workflow
   role from the selected file name.
   - Keep image imports as the default.
   - Map segmentation-like files to `DataWorkflowRole::Segmentation`.
   - Map model-like files to `DataWorkflowRole::Model`.
   - Map mesh-like files to `DataWorkflowRole::Mesh`.
   - Map simulation result-like files to
     `DataWorkflowRole::SimulationResult`.
2. Keep scope narrow.
   - Do not add DICOM directory or multi-file import yet.
   - Do not parse file contents; use deterministic file-name/extension rules
     only.
   - Preserve current successful image import behavior.
3. Add C++ regression tests before implementation:
   - `.nii.gz` remains an image import with `image-` id prefix.
   - segmentation file names produce segmentation role and `segmentation-`
     prefix.
   - `.vtp`/model names produce model role and `model-` prefix.
   - mesh names/extensions produce mesh role and `mesh-` prefix.
   - `.vtu`/result names produce simulation result role and `result-` prefix.
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

## Completed Phase: Monolith Data Page Role Display

1. Surface imported data workflow role on the monolith Data page.
   - Add a role label with object name `xqDataPageWorkflowRole`.
   - Clear the role label when no data is selected.
   - Render user-facing labels for Image, Segmentation, Model, Mesh,
     Simulation Result, DICOM Series, and Unknown.
2. Preserve existing Data page behavior.
   - Selection, catalog id, display name, and source path labels keep their
     current text.
   - Rename/remove still refresh and clear labels correctly.
3. Add C++ regression tests before implementation:
   - Data page exposes the workflow-role label.
   - Image import shows `Role: Image`.
   - Model import shows `Role: Model`.
   - Removing the selected entry clears role text with the rest of metadata.
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

## Completed Phase: Image Preprocessing Operation Selector

1. Add a Core workflow-operation service.
   - Register ordered operation descriptors per workflow.
   - Track a selected operation per workflow.
   - Reject unknown workflows, empty operation ids/titles, and duplicate
     operation ids.
   - Expose `WorkflowOperations()` through `ApplicationContext`.
2. Register image preprocessing operations from the Domain layer.
   - Reuse `ImagePreprocessingWorkflowService::Operations()`.
   - Keep existing default action-handler registration working when callers do
     not pass the operation service.
3. Surface operations on the Image Preprocessing page.
   - Add a `QComboBox` object named
     `xqImagePreprocessingOperationSelector`.
   - Populate it with Binary Threshold, Connected Threshold, Gaussian
     Smoothing, Morphology Open/Close, Crop, and Resample.
   - Keep the first operation selected by default.
   - Update the page primary button text to `Run <operation title>`.
   - Changing the combo updates Core selected operation state.
4. Route the image preprocessing action through the selected operation.
   - If an operation is selected, run that operation through the existing
     domain handler path.
   - Preserve placeholder action behavior for workflows without operation
     selectors.
5. Add C++ regression tests before implementation:
   - Core operation registration, default selection, explicit selection,
     duplicate rejection, and context exposure.
   - Image Preprocessing page exposes the operation selector, preserves
     operation order, updates button text, synchronizes selection, and runs the
     selected operation.
6. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
7. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Image Preprocessing Parameter Panel

1. Extend Core workflow operation descriptors with ordered parameters.
   - Add a generic parameter descriptor with id, title, value type, and
     required flag.
   - Preserve parameters when registering operations.
   - Reject duplicate parameter ids inside a single operation.
2. Map Image Preprocessing domain parameters into Core operation metadata.
   - Numeric scalar parameters become numeric UI controls.
   - Integer scalar parameters become integer UI controls.
   - Integer point-list parameters remain represented for a later richer
     editor.
3. Surface operation parameters on the Image Preprocessing page.
   - Add a panel object named `xqImagePreprocessingParameterPanel`.
   - Default Binary Threshold shows lower, upper, inside value, and outside
     value numeric controls.
   - Gaussian Smoothing shows only sigma.
   - Crop shows origin and size integer controls.
   - Switching operations rebuilds the parameter panel and removes stale
     controls.
4. Keep scope narrow.
   - Do not yet persist parameter values.
   - Do not yet pass UI parameter values into the MITK execution path.
   - Preserve the selected-operation action behavior from the previous slice.
5. Add C++ regression tests before implementation:
   - Core operation-service parameters roundtrip and duplicate parameter ids
     are rejected.
   - Image Preprocessing page exposes a parameter panel and rebuilds controls
     for Binary Threshold, Gaussian Smoothing, and Crop.
6. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
7. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Image Preprocessing Parameter State

1. Add Core parameter value state to `WorkflowOperationService`.
   - Initialize default parameter values from registered operation descriptors.
   - Expose `ParameterValues(workflowId, operationId)`.
   - Add `SetParameterValue(workflowId, operationId, parameterId, value)`.
   - Reject unknown workflow, operation, or parameter ids.
   - Emit parameter-value change notifications only when the stored value
     changes.
2. Wire Image Preprocessing parameter editors to Core state.
   - Numeric and integer controls should load stored values when rebuilt.
   - Editing a control updates Core parameter state.
   - Switching operations should preserve each operation's values.
3. Route selected Image Preprocessing parameters into the domain action.
   - The default domain handler should call parameterized
     `RunOperation(snapshot, operationId, parameters)` when operation state is
     available.
   - Preserve non-operation workflow placeholder behavior.
4. Keep scope narrow.
   - Do not yet persist parameter values into `.xqproj`.
   - Do not yet wire the full MITK image-processing commit handler from the UI.
   - Integer point-list editing remains a placeholder.
5. Add C++ regression tests before implementation:
   - Core parameter defaults, set/get, change notification, and unknown
     parameter rejection.
   - Image Preprocessing sigma edits update Core state and the selected
     operation action still succeeds through the parameterized domain path.
6. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
7. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Image Preprocessing Infrastructure Wiring

1. Promote Image Preprocessing from Domain placeholder action to Infrastructure
   execution in the monolith composition root.
   - Keep Domain registration responsible for workflow operation metadata.
   - Register an Infrastructure Image Preprocessing handler after Domain
     registration so the production action path uses MITK-backed commit logic.
2. Add a dynamic Infrastructure registration path.
   - Resolve the selected operation from `WorkflowOperationService`.
   - Resolve parameter values from `WorkflowOperationService`.
   - Resolve the source MITK node from `DataNodeRegistryService` or active
     node.
   - Preserve the clear missing-node diagnostic when no source node exists.
3. Keep existing fixed-operation Infrastructure tests working.
   - Do not remove `RegisterImagePreprocessingWorkflowActionHandler(options)`.
   - The new dynamic registration should share the same application commit
     behavior.
4. Add C++ regression tests before implementation:
   - Composition root overrides the Domain placeholder handler.
   - Running Image Preprocessing with selected image metadata but no MITK source
     node fails with `Active image node is required for image preprocessing.`
     and records a failed task.
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

## Completed Phase: Image Preprocessing Result Activation

1. Extend Image Preprocessing Infrastructure workflow action registration with
   post-commit result activation.
   - Select the generated catalog entry after a successful preprocessing
     commit.
   - Refresh MITK rendering once after a successful preprocessing commit.
   - Do not select or refresh when preprocessing fails.
2. Keep both fixed-operation and dynamic-operation registrations aligned.
   - Fixed-operation tests should cover selection and refresh.
   - Dynamic production registration should use the same helper path.
3. Wire production composition to reuse the existing render-refresh service.
   - `CreateConfiguredMainWindow()` should pass the render refresh service to
     dynamic Image Preprocessing registration.
4. Add C++ regression tests before implementation:
   - Successful crop handler selects `image-001-crop`.
   - Successful crop handler refreshes the application DataStorage exactly
     once.
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

## Completed Phase: Modeling/Meshing Operation Foundation

1. Add Domain-level operation descriptors for Modeling and Meshing.
   - Modeling: Loft Surface, Build Solid Model, Trim Branches.
   - Meshing: Generate Surface Mesh, Generate Volume Mesh, Boundary Layers.
   - Include initial parameter descriptors using the existing generic
     workflow-operation parameter types.
2. Register Modeling/Meshing operations through `WorkflowOperationService`.
   - Keep Path and Segmentation operation registration working.
   - Keep execution as Domain-level placeholder acceptance in this slice.
3. Surface Modeling/Meshing operations on their existing workflow pages.
   - Both pages should get operation selectors and parameter panels.
   - Primary action text should include the selected operation.
   - Running each workflow should report the selected operation title.
4. Add C++ regression tests before implementation:
   - Domain registration exposes Modeling and Meshing operation descriptors.
   - Modeling page exposes ordered operations and solid-model parameters.
   - Meshing page exposes ordered operations and volume-mesh parameters.
   - Run actions post diagnostics/task history with selected operation titles.
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

## Completed Phase: Simulation Operation Foundation

1. Add Domain-level operation descriptors for simulation workflows.
   - Flow Simulation: Configure CFD Job, Run Steady Flow, Review Results.
   - ROM Simulation: Build 1D Network, Run ROM Solver, Calibrate Boundary
     Conditions.
   - MultiPhysics: Configure Coupling, Run Coupled Solve, Review Coupled
     Results.
   - Include initial parameter descriptors using the existing generic
     workflow-operation parameter types.
2. Register Flow/ROM/MultiPhysics operations through
   `WorkflowOperationService`.
   - Keep Path, Segmentation, Modeling, and Meshing registration working.
   - Keep execution as Domain-level placeholder acceptance in this slice.
3. Surface Flow/ROM/MultiPhysics operations on their existing workflow pages.
   - All three pages should get operation selectors and parameter panels.
   - Primary action text should include the selected operation.
   - Running each workflow should report the selected operation title.
4. Add C++ regression tests before implementation:
   - Domain registration exposes Flow/ROM/MultiPhysics operation descriptors.
   - Flow page exposes ordered operations and steady-flow parameters.
   - ROM page exposes ordered operations and ROM solver parameters.
   - MultiPhysics page exposes ordered operations and coupling parameters.
   - Run actions post diagnostics/task history with selected operation titles.
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

## Completed Phase: Python API Operation Foundation

1. Add Domain-level operation descriptors for the Python API workflow.
   - Open Python Console.
   - Project Script Runner.
   - Export API Snippet.
   - Use initial integer parameters that fit the current generic parameter
     editor.
2. Register Python API operations through `WorkflowOperationService`.
   - Keep Python API non-data-dependent.
   - Keep registration without a `WorkflowOperationService` preserving the
     current no-handler behavior.
3. Surface Python API operations on its existing workflow page.
   - Python API should get an operation selector and parameter panel.
   - Primary action text should include the selected operation.
   - The action should be enabled without selected data.
   - The status text should not ask for compatible data when no data is
     required.
4. Add C++ regression tests before implementation:
   - Domain registration with operation state exposes Python API operations.
   - Python API selected operation action reports no-data operation success.
   - Python API page exposes ordered operations and script-runner parameters.
   - Project/Data pages still do not expose primary workflow action buttons.
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

## Completed Phase: Legacy BlueBerry Default Retirement

1. Make the monolith target the default local CMake configuration path.
   - Keep `XQ_BUILD_LEGACY_BLUEBERRY` available as an opt-in migration flag.
   - Change the global CMake option default from `ON` to `OFF`.
   - Keep the Windows preset explicitly disabling legacy for clarity.
2. Add a PowerShell regression test before implementation:
   - `Code/CMake/XQOptions.cmake` should declare
     `XQ_BUILD_LEGACY_BLUEBERRY` with default `OFF`.
   - The Windows preset should still set `XQ_BUILD_LEGACY_BLUEBERRY=OFF`.
3. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
4. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Default XQ Monolith Target Naming

1. Make the default monolith CMake target name `XQ`.
   - When `XQ_BUILD_LEGACY_BLUEBERRY=OFF`, the monolith executable target
     should be named `XQ`.
   - When legacy BlueBerry is explicitly enabled, keep the monolith target as
     `XQMonolith` so the legacy application can still own target `XQ`.
2. Update the monolith scaffold PowerShell regression test before
   implementation:
   - It should reject an unconditional `add_executable(XQMonolith ...)`.
   - It should require a conditional `XQ_MONOLITH_TARGET` defaulting to `XQ`
     and switching to `XQMonolith` only for legacy builds.
3. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
4. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Backup Artifact Cleanup

1. Remove stale backup artifacts from the repository.
   - Delete files whose names are clearly backup/old/copy artifacts.
   - Keep source, tests, and intentional fixture files untouched.
2. Add a PowerShell regression test before deletion:
   - Repository file names should not match common backup suffixes such as
     `.bak`, `.old`, `~`, or `*_backup_*`.
3. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
4. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Run Script Monolith Fallback

1. Update `scripts/run-xq.ps1` for the new default monolith target naming.
   - Default execution remains `XQ.exe`.
   - `-Monolith` should prefer `XQMonolith.exe` for legacy opt-in builds, then
     fall back to `XQ.exe` for default monolith builds.
   - Help text should describe the fallback instead of claiming
     `XQMonolith.exe` is always the monolith executable.
2. Extend the PowerShell environment-script regression test before
   implementation:
   - Reject the old single-name `$Monolith ? XQMonolith.exe : XQ.exe` logic.
   - Require an executable-candidate list that includes both
     `XQMonolith.exe` and `XQ.exe` for `-Monolith`.
3. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
4. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Workflow Operation Parameter UI Restore

1. Keep all monolith workflow operation parameter panels synchronized after
   project open restores workflow operation state.
   - If the restored selected operation is unchanged but parameter values
     changed, the visible parameter editors should refresh.
   - Apply this to generic workflow pages such as Path, Modeling, Meshing,
     Flow, ROM, MultiPhysics, and Python API, not only Image Preprocessing.
2. Extend the Path workflow page C++ regression test before implementation.
   - Save a project whose default Path operation `create-centerline` has
     `control-point-count` changed from the default value.
   - Open that project through an existing `MainWindow`.
   - The existing Path parameter editor should show the restored integer value
     even though the selected operation id did not change.
3. Implement the minimal Presentation refresh hook.
   - `MainWindow` should listen to `WorkflowOperationService` parameter value
     changes.
   - Refresh operation controls or the affected parameter panel without
     changing Core state.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Workflow Operation Option Parameters

1. Extend monolith workflow operation parameter descriptors with option-set
   values.
   - Add a new `Option` parameter value type.
   - Add ordered option descriptors with stable id and display title.
   - Default option value should be the first option id.
   - Reject option parameters that have no options or duplicate option ids.
2. Render option parameters in generic workflow pages.
   - Use `QComboBox` for option parameters.
   - Object names should follow the existing generic parameter convention:
     `xqWorkflowParameter_<parameter-id>`.
   - Changing the combo box should update `WorkflowOperationService`
     parameter state.
3. Add first domain use in Flow Simulation.
   - `configure-cfd-job` should expose a `solver-profile` option with ordered
     values `steady`, `pulsatile`, and `transient`.
   - Keep execution placeholder-only in this slice.
4. Add regression tests before implementation:
   - Core service preserves option metadata, initializes the first option id as
     the default value, and rejects duplicate option ids.
   - Flow Simulation page exposes a `solver-profile` combo box whose selected
     value updates Core state.
5. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
6. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Workflow Point List Parameter Editor

1. Replace the current point-list placeholder row with an editable monolith UI
   control.
   - Render `IntegerPointList` parameters as `QLineEdit`.
   - Use existing object-name conventions:
     `xqImagePreprocessingParameter_<parameter-id>` for Image Preprocessing
     and `xqWorkflowParameter_<parameter-id>` for generic workflow pages.
   - Accept text in `x,y,z; x,y,z` format and store a `QVariantList` of
     integer triplets in `WorkflowOperationService`.
2. Preserve restore/update behavior.
   - Existing parameter-value restore should update the line edit text without
     recursive Core writes.
   - Stored `QVariantList` values should format back to the same canonical
     text shape.
3. Add regression tests before implementation:
   - Image Preprocessing connected-threshold page exposes `seeds` as a
     `QLineEdit`.
   - Editing seeds to `1,2,3; 4,5,6` updates Core state to two integer
     triplets.
   - Invalid seed text posts a diagnostic and does not replace the last valid
     Core state.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Workflow Operation Rich Parameter Persistence

1. Extend project workflow-operation persistence coverage for rich parameter
   values.
   - Option parameters should save/open as stable option id strings.
   - Integer point-list parameters should save/open as arrays of integer
     triplets.
   - Invalid persisted option values should fail without mutating existing
     operation state.
2. Add regression tests before implementation:
   - A Flow Simulation `solver-profile` option value roundtrips through
     `ProjectService`.
   - An Image Preprocessing `seeds` point list roundtrips through
     `ProjectService`.
   - A fixture with an unknown option id is rejected and leaves existing
     operation state unchanged.
3. Implement only if the tests expose a persistence gap.
   - Prefer using existing `WorkflowOperationService::SetParameterValue`
     validation during open.
   - Keep schema version `2.0`.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Legacy BlueBerry Include Isolation

1. Keep default monolith CMake include paths free of legacy BlueBerry plugin
   headers.
   - The generic MITK module include paths may remain available.
   - `BERRY_PLUGIN_SOURCE_DIRS`, `BERRY_PLUGIN_BUILD_DIRS`, and their
     `include_directories()` call should only be evaluated when
     `XQ_BUILD_LEGACY_BLUEBERRY` is enabled.
2. Add a PowerShell scaffold regression test before implementation.
   - Reject unguarded BlueBerry plugin include-directory setup in
     `Code/CMakeLists.txt`.
   - Preserve the existing test that legacy plugin subdirectories are still
     behind `XQ_BUILD_LEGACY_BLUEBERRY`.
3. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
4. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Path Infrastructure Action Handler

1. Promote the Path workflow from Domain placeholder acceptance to an
   Infrastructure action handler for `create-centerline`.
   - Reuse the existing `xq_PathPipelineService::CreatePath`.
   - Resolve the selected image MITK node from `DataNodeRegistryService` or the
     active node.
   - Read seed points and sample count from `WorkflowOperationService`
     parameter values.
   - Register the generated Path result in `DataCatalogService`,
     `DataHierarchyService`, and `DataNodeRegistryService`.
   - Select the generated result and refresh MITK rendering after success.
2. Keep unsupported Path operations on the existing operation-aware Domain
   placeholder path for this slice.
3. Add C++ regression tests before implementation:
   - Missing operation id or missing seed points fail without catalog mutation.
   - A valid `create-centerline` request creates a Path node and metadata
     using the existing pipeline.
   - The configured monolith composition root installs the Infrastructure Path
     handler after Domain registration.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Segmentation 2D Infrastructure Action Handler

1. Promote the 2D Segmentation workflow from Domain placeholder acceptance to
   an Infrastructure action handler for `manual-contour`.
   - Reuse the existing `xq_SegmentationPipelineService::CreateContourGroup`.
   - Resolve the selected Path MITK node from `DataNodeRegistryService` or the
     active node.
   - Register the generated Segmentation result in `DataCatalogService`,
     `DataHierarchyService`, and `DataNodeRegistryService`.
   - Select the generated result and refresh MITK rendering after success.
2. Keep unsupported 2D Segmentation operations and all 3D Segmentation
   operations on the existing operation-aware Domain placeholder path for this
   slice.
3. Add C++ regression tests before implementation:
   - Missing operation id or missing Path node fails without catalog mutation.
   - A valid `manual-contour` request creates a Segmentation node and metadata
     using the existing pipeline.
   - The configured monolith composition root installs the Infrastructure 2D
     Segmentation handler after Domain registration.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Modeling Infrastructure Action Handler

1. Promote the Modeling workflow from Domain placeholder acceptance to an
   Infrastructure action handler for `build-solid-model`.
   - Reuse the existing `xq_ModelPipelineService::CreateModel`.
   - Resolve the selected Segmentation/ContourGroup MITK node from
     `DataNodeRegistryService` or the active node.
   - Read sampling and blend radius from `WorkflowOperationService` parameter
     values where available.
   - Register the generated Model result in `DataCatalogService`,
     `DataHierarchyService`, and `DataNodeRegistryService`.
   - Select the generated result and refresh MITK rendering after success.
2. Keep unsupported Modeling operations on the existing operation-aware Domain
   placeholder path for this slice.
3. Add C++ regression tests before implementation:
   - Missing operation id or missing Segmentation node fails without catalog
     mutation.
   - A valid `build-solid-model` request creates a Model node and metadata
     using the existing pipeline.
   - The configured monolith composition root installs the Infrastructure
     Modeling handler after Domain registration.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Meshing Infrastructure Action Handler

1. Promote the Meshing workflow from Domain placeholder acceptance to an
   Infrastructure action handler for `generate-volume-mesh`.
   - Reuse the existing `xq_MeshPipelineService::CreateVolumeMesh`.
   - Resolve the selected Model MITK node from `DataNodeRegistryService` or the
     active node.
   - Read edge size and optimization parameters from `WorkflowOperationService`
     parameter values where available.
   - Register the generated Mesh result in `DataCatalogService`,
     `DataHierarchyService`, and `DataNodeRegistryService`.
   - Select the generated result and refresh MITK rendering after success.
2. Keep unsupported Meshing operations on the existing operation-aware Domain
   placeholder path for this slice.
3. Add C++ regression tests before implementation:
   - Missing operation id or missing Model node fails without catalog mutation.
   - A valid `generate-volume-mesh` request creates a Mesh node and metadata
     using the existing pipeline.
   - The configured monolith composition root installs the Infrastructure
     Meshing handler after Domain registration.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Flow Simulation Prep Infrastructure Action Handler

1. Promote the Flow Simulation workflow from Domain placeholder acceptance to
   an Infrastructure action handler for `configure-cfd-job`.
   - Reuse the existing
     `xq_SimulationPrepPipelineService::CreateOrUpdateSimulationPrep`.
   - Resolve the selected Mesh MITK node from `DataNodeRegistryService` or the
     active node.
   - Resolve the upstream Model node from pipeline metadata/DataStorage.
   - Read solver profile and count parameters from `WorkflowOperationService`
     parameter values where available.
   - Register the generated SimulationPrep job in `DataCatalogService`,
     `DataHierarchyService`, and `DataNodeRegistryService`.
   - Select the generated job and refresh MITK rendering after success.
2. Keep `run-steady-flow`, `review-flow-results`, ROM, and MultiPhysics on the
   existing operation-aware Domain placeholder path for this slice.
3. Add C++ regression tests before implementation:
   - Missing operation id or missing Mesh node fails without catalog mutation.
   - A valid `configure-cfd-job` request creates an `xq_MitkSolverJob` node and
     metadata using the existing simulation-prep pipeline.
   - The configured monolith composition root installs the Infrastructure Flow
     Simulation handler after Domain registration.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Flow Simulation Steady Run Infrastructure Action Handler

1. Promote the Flow Simulation `run-steady-flow` operation from placeholder
   acceptance to a real Infrastructure action handler.
   - Require the selected data to resolve to a MITK SimulationPrep node.
   - Use `xq_SimulationPrepPipelineService::RunSolverAndImportResults`.
   - Use the registered `xq_simple_flow` native backend for steady CFD jobs.
   - Register imported flow-result nodes in `DataCatalogService`,
     `DataHierarchyService`, and `DataNodeRegistryService`.
   - Select the first generated result and refresh MITK rendering after
     success.
2. Keep `review-flow-results`, ROM, and MultiPhysics on the existing
   operation-aware placeholder path for this slice.
3. Add C++ regression tests before implementation:
   - `run-steady-flow` rejects Mesh or missing SimulationPrep selections.
   - A valid SimulationPrep node runs the native steady solver and registers
     imported result metadata.
   - The configured monolith composition root uses Infrastructure validation
     for `run-steady-flow`.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Flow Results Review Infrastructure Action Handler

1. Promote the Flow Simulation `review-flow-results` operation from
   placeholder acceptance to a real Infrastructure action handler.
   - Require the selected data to resolve to a MITK SimulationResult node.
   - Choose a review scalar from imported result fields, preferring pressure,
     then velocity, then wall-shear metadata.
   - Use the existing result-import scalar activation helper where possible.
   - Mark the result node visible/scalar-visible and persist review metadata
     on the node.
   - Refresh MITK rendering after success.
2. Keep ROM and MultiPhysics on the existing operation-aware placeholder path
   for this slice.
3. Add C++ regression tests before implementation:
   - `review-flow-results` rejects Mesh or missing SimulationResult
     selections.
   - A valid imported result node gets active scalar/review metadata and a
     render refresh.
   - The configured monolith composition root uses Infrastructure validation
     for `review-flow-results`.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: ROM Build Network Infrastructure Action Handler

1. Promote the ROM Simulation `build-1d-network` operation from placeholder
   acceptance to a real Infrastructure action handler.
   - Require the selected data to resolve to a Mesh or SimulationPrep node.
   - Resolve upstream SimulationPrep where available for cap-role and solver
     metadata; otherwise use Mesh provenance.
   - Create an `xq_MitkROMJob` with a validated `xq_ROMJob`.
   - Persist source mesh/simulation metadata and selected operation
     parameters on the generated node.
   - Register the generated ROM job in `DataCatalogService`,
     `DataHierarchyService`, and `DataNodeRegistryService`.
   - Select the generated ROM job and refresh MITK rendering after success.
2. Keep `run-rom-solver`, `calibrate-boundary-conditions`, and MultiPhysics on
   the existing operation-aware placeholder path for this slice.
3. Add C++ regression tests before implementation:
   - `build-1d-network` rejects missing Mesh/SimulationPrep MITK nodes.
   - A valid Mesh selection creates and registers an `xq_MitkROMJob` node.
   - The configured monolith composition root uses Infrastructure validation
     for `build-1d-network`.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: MultiPhysics Configure Coupling Infrastructure Action Handler

1. Promote the MultiPhysics `configure-coupling` operation from placeholder
   acceptance to a real Infrastructure action handler.
   - Require the selected data to resolve to a ROMSimulation or
     SimulationPrep MITK node.
   - Create an `xq_MitkMultiPhysicsJob` with a validated
     `xq_MultiPhysicsJob`.
   - Seed minimal fluid and solid domains plus an FSI equation so validation
     passes without running a solver.
   - Persist source ROM/simulation metadata and selected operation parameters
     on the generated node.
   - Register the generated MultiPhysics job in `DataCatalogService`,
     `DataHierarchyService`, and `DataNodeRegistryService`.
   - Select the generated MultiPhysics job and refresh MITK rendering after
     success.
2. Keep `run-coupled-solve`, `review-coupled-results`, and ROM solver
   execution on the existing operation-aware placeholder path for this slice.
3. Add C++ regression tests before implementation:
   - `configure-coupling` rejects missing ROMSimulation/SimulationPrep MITK
     nodes.
   - A valid ROMSimulation selection creates and registers an
     `xq_MitkMultiPhysicsJob` node.
   - The configured monolith composition root uses Infrastructure validation
     for `configure-coupling`.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Python API Availability Infrastructure Action Handler

1. Promote the Python API `open-python-console` operation from placeholder
   acceptance to a real Infrastructure action handler.
   - Use the existing `xq_PythonApiService` C++ service.
   - Report the service version and Python runtime availability diagnostic.
   - Do not fabricate an interactive Python console while pybind11/runtime is
     unavailable in the current Windows monolith build.
   - Keep the operation result deterministic and task-history friendly.
2. Keep `run-project-script` and `export-api-snippet` on the existing
   operation-aware placeholder path for this slice.
3. Add C++ regression tests before implementation:
   - Dynamic Python API handler registration is discoverable.
   - `open-python-console` returns the real unavailable-runtime diagnostic
     from `xq_PythonApiService` and succeeds as a diagnostic action.
   - Unsupported Python API operations still use the placeholder path.
   - The configured monolith composition root uses Infrastructure behavior
     for `open-python-console`.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Python API Snippet Export Infrastructure Action

1. Promote the Python API `export-api-snippet` operation from placeholder
   acceptance to a deterministic Infrastructure action.
   - Generate a small usage snippet catalog from the existing
     `xq_PythonApiService` C++ inspection surface.
   - Respect the `snippet-count` operation parameter so the UI control has
     observable behavior.
   - Keep output text deterministic and task-history friendly.
2. Keep `run-project-script` on the existing placeholder path for this slice.
   - Do not fabricate script execution while pybind11/runtime is unavailable.
3. Add C++ regression tests before implementation:
   - `export-api-snippet` returns real snippet text including version,
     list-nodes, and find-node usage.
   - `snippet-count` limits the number of generated snippets.
   - Unsupported Python API operations still use the placeholder path.
   - The configured monolith composition root uses Infrastructure snippet
     behavior for `export-api-snippet`.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Python API Script Runtime Guard

1. Promote the Python API `run-project-script` operation from placeholder
   acceptance to an Infrastructure runtime guard.
   - Use `xq_PythonApiService::IsAvailable()` and
     `GetAvailabilityDiagnostic()` to detect the unavailable Python runtime.
   - Return failure with a deterministic diagnostic while pybind11/runtime is
     unavailable.
   - Do not fabricate script execution or mark script runs as successful.
2. Keep `open-python-console` and `export-api-snippet` Infrastructure behavior
   from the previous slices.
3. Add C++ regression tests before implementation:
   - `run-project-script` returns false and reports the runtime-unavailable
     diagnostic.
   - Task history records the script attempt as failed.
   - The Python API operation page posts a failed diagnostic for the script
     runner instead of a succeeded placeholder.
   - The configured monolith composition root uses Infrastructure guard
     behavior for `run-project-script`.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Segmentation Unsupported Operation Guard

1. Stop reporting unsupported Segmentation operations as successful
   Infrastructure actions.
   - Keep `segmentation-2d/manual-contour` on the existing
     `xq_SegmentationPipelineService::CreateContourGroup` path.
   - Return failure for `segmentation-2d/threshold-contour`,
     `segmentation-2d/loft-profiles`, and all `segmentation-3d` operations
     until native MITK/ITK runtime integration exists.
   - Use deterministic diagnostics that include the selected operation title
     and workflow title.
2. Keep Domain placeholder behavior intact for tests that intentionally verify
   generic operation routing without Infrastructure.
3. Add C++ regression tests before implementation:
   - Dynamic handler rejects unsupported 2D Segmentation operations.
   - Dynamic handler rejects 3D Segmentation operations.
   - The Segmentation operation page posts failed diagnostics when wired with
     the Infrastructure handler.
   - The configured monolith composition root uses the Infrastructure guard.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Path Unsupported Operation Guard

1. Stop reporting unsupported Path operations as successful Infrastructure
   actions.
   - Keep `path/create-centerline` on the existing
     `xq_PathPipelineService::CreatePath` path.
   - Return failure for `path/edit-control-points` and `path/smooth-path`
     until native interactive path editing/smoothing runtime integration
     exists.
   - Use deterministic diagnostics that include the selected operation title.
2. Keep Domain placeholder behavior intact for tests that intentionally verify
   generic operation routing without Infrastructure.
3. Add C++ regression tests before implementation:
   - Dynamic handler rejects `edit-control-points`.
   - Dynamic handler rejects `smooth-path`.
   - The Path operation page posts a failed diagnostic when wired with the
     Infrastructure handler.
   - The configured monolith composition root uses the Infrastructure guard.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Modeling Unsupported Operation Guard

1. Stop reporting unsupported Modeling operations as successful Infrastructure
   actions.
   - Keep `modeling/build-solid-model` on the existing
     `xq_ModelPipelineService::CreateModel` path.
   - Return failure for `modeling/loft-surface` and `modeling/trim-branches`
     until native modeling runtime integration exists for those operations.
   - Use deterministic diagnostics that include the selected operation title
     and workflow title.
2. Keep Domain placeholder behavior intact for tests that intentionally verify
   generic operation routing without Infrastructure.
3. Add C++ regression tests before implementation:
   - Dynamic handler rejects `loft-surface`.
   - Dynamic handler rejects `trim-branches`.
   - The Modeling operation page posts a failed diagnostic when wired with the
     Infrastructure handler.
   - The configured monolith composition root uses the Infrastructure guard.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Meshing Unsupported Operation Guard

1. Stop reporting unsupported Meshing operations as successful Infrastructure
   actions.
   - Keep `meshing/generate-volume-mesh` on the existing
     `xq_MeshPipelineService::CreateVolumeMesh` path.
   - Return failure for `meshing/generate-surface-mesh` and
     `meshing/boundary-layers` until native meshing runtime integration exists
     for those operations.
   - Use deterministic diagnostics that include the selected operation title
     and workflow title.
2. Keep Domain placeholder behavior intact for tests that intentionally verify
   generic operation routing without Infrastructure.
3. Add C++ regression tests before implementation:
   - Dynamic handler rejects `generate-surface-mesh`.
   - Dynamic handler rejects `boundary-layers`.
   - The Meshing operation page posts a failed diagnostic when wired with the
     Infrastructure handler.
   - The configured monolith composition root uses the Infrastructure guard.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: ROM Unsupported Operation Guard

1. Stop reporting unsupported ROM Simulation operations as successful
   Infrastructure actions.
   - Keep `rom-simulation/build-1d-network` on the existing
     `xq_MitkROMJob` creation path.
   - Return failure for `rom-simulation/run-rom-solver` and
     `rom-simulation/calibrate-boundary-conditions` until native ROM solver
     and calibration runtime integration exists.
   - Use deterministic diagnostics that include the selected operation title
     and workflow title.
2. Keep Domain placeholder behavior intact for tests that intentionally verify
   generic operation routing without Infrastructure.
3. Add C++ regression tests before implementation:
   - Dynamic handler rejects `run-rom-solver`.
   - Dynamic handler rejects `calibrate-boundary-conditions`.
   - The ROM operation page posts a failed diagnostic when wired with the
     Infrastructure handler.
   - The configured monolith composition root uses the Infrastructure guard.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Completed Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: MultiPhysics Unsupported Operation Guard

1. Stop reporting unsupported MultiPhysics operations as successful
   Infrastructure actions.
   - Keep `multiphysics/configure-coupling` on the existing
     `xq_MitkMultiPhysicsJob` creation path.
   - Return failure for `multiphysics/run-coupled-solve` and
     `multiphysics/review-coupled-results` until native coupled solver and
     result review runtime integration exists.
   - Use deterministic diagnostics that include the selected operation title
     and workflow title.
2. Keep Domain placeholder behavior intact for tests that intentionally verify
   generic operation routing without Infrastructure.
3. Add C++ regression tests before implementation:
   - Dynamic handler rejects `run-coupled-solve`.
   - Dynamic handler rejects `review-coupled-results`.
   - The MultiPhysics operation page posts a failed diagnostic when wired with
     the Infrastructure handler.
   - The configured monolith composition root uses the Infrastructure guard.
4. Run:
   - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals`
   - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
   - all XQ `tests\*.ps1`
   - all Externals `tests\*.ps1`
   - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
   - `git diff --check`
5. Commit and push the verified XQ iteration.

## Active Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.

## Completed Phase: Path Operation Foundation

1. Add Domain-level operation descriptors for the Path workflow.
   - Create Centerline.
   - Edit Control Points.
   - Smooth Path.
   - Include initial parameter descriptors using the existing generic
     workflow-operation parameter types.
2. Register Path operations through `WorkflowOperationService`.
   - Keep existing Image Preprocessing and Segmentation registration working.
   - Keep Path action execution as Domain-level placeholder acceptance in this
     slice.
3. Surface Path operations on the existing Path workflow page.
   - Path page should get an operation selector and parameter panel.
   - Primary action text should include the selected Path operation.
   - Running Path should report the selected Path operation title.
4. Add C++ regression tests before implementation:
   - Domain registration exposes Path operation descriptors.
   - Path page exposes ordered operations and smoothing parameters.
   - Path Run posts diagnostics/task history with the selected operation title.
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

## Completed Phase: Segmentation Operation Action Routing

1. Route 2D/3D Segmentation workflow actions through selected operation state.
   - Use `WorkflowOperationService::SelectedOperationId()` for segmentation
     workflows.
   - Produce task messages that include the selected segmentation operation
     title and selected data label.
   - Preserve the existing generic placeholder behavior when no operation
     service is supplied.
2. Keep scope narrow.
   - Do not yet execute MITK segmentation tools or write segmentation result
     nodes in this slice.
   - Do not change Image Preprocessing Infrastructure routing.
3. Add C++ regression tests before implementation:
   - Domain handler for `segmentation-2d` reports selected operation title.
   - Domain handler for `segmentation-3d` reports selected operation title.
   - UI Run on the Segmentation pages posts diagnostics/task history with the
     selected operation title.
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

## Completed Phase: Segmentation Operation Foundation

1. Add Domain-level operation descriptors for segmentation workflows.
   - `segmentation-2d`: Threshold Contour, Manual Contour, Loft Profiles.
   - `segmentation-3d`: Threshold Region, Region Growing, Surface Preview.
   - Include initial parameter descriptors using the existing generic
     workflow-operation parameter types.
2. Register segmentation operations through the monolith
   `WorkflowOperationService`.
   - Keep Image Preprocessing registration unchanged.
   - Keep segmentation actions as Domain-level placeholder acceptance in this
     slice.
3. Surface segmentation operations on the existing workflow pages.
   - Both 2D and 3D Segmentation pages should get operation selectors.
   - Primary action text should include the selected segmentation operation.
   - Parameter panels should rebuild for selected segmentation operations.
4. Add C++ regression tests before implementation:
   - Domain registration exposes segmentation operation descriptors.
   - 2D Segmentation page exposes ordered operations and threshold parameters.
   - 3D Segmentation page exposes ordered operations and region-growing
     parameters.
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

## Completed Phase: Image Preprocessing UI State Restore

1. Keep the Image Preprocessing page synchronized after project open restores
   workflow operation state.
   - Operation selector should switch to the restored selected operation.
   - Primary action text should show the restored operation title.
   - Parameter panel should rebuild using restored parameter values.
2. Preserve existing operation selector behavior.
   - User-driven selector changes still update Core state.
   - Core-driven selection changes still update the selector without feedback
     loops.
3. Add C++ regression tests before implementation:
   - Open a project whose persisted Image Preprocessing state selects
     Gaussian Smoothing with sigma `2.25`.
   - Existing `MainWindow` updates selector, action button, and sigma editor
     after `ProjectSession()->Open()`.
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

## Completed Phase: Image Preprocessing Operation State Persistence

1. Persist monolith workflow operation state in the fresh `.xqproj` schema.
   - Save selected operations per workflow.
   - Save parameter values per workflow operation.
   - Keep the first implementation scoped to the existing
     `WorkflowOperationService`; do not change schema version `2.0`.
2. Restore operation state on project open.
   - Apply selected operation ids after operation descriptors are registered.
   - Apply parameter values only for known workflow/operation/parameter ids.
   - Reject unknown persisted operation or parameter ids with a useful error.
   - Failed opens must not mutate project metadata, catalog, hierarchy, or
     workflow operation state.
3. Extend project/session wiring.
   - Add `ProjectService` save/open overloads that accept
     `WorkflowOperationService`.
   - Update `ProjectSessionService` and `ApplicationContext` wiring so
     save/open roundtrips workflow operation state alongside catalog and
     hierarchy.
4. Add C++ regression tests before implementation:
   - Saving a project writes the selected Image Preprocessing operation and
     edited sigma value.
   - Opening a project restores the selected operation and parameter value.
   - Unknown persisted parameter ids fail without mutating existing workflow
     operation state.
   - Project session save/open restores operation state through
     `ApplicationContext`.
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
