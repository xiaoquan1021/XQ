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

## Active Phase: Autonomous Research Refresh

1. Search comparable medical imaging workstation projects and documentation
   again.
2. Extract the next high-value monolith migration slice.
3. Write the next executable phase into this plan.
4. Immediately return to plan execution.
