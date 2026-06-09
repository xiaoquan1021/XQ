# XQ Execution Log

## Current Run: Workbench Data Manager Panel Restore

- Continued after `7a81df2` with the next Workbench fidelity gap:
  - The left Data Manager dock existed, but it was only a flat tree.
  - The original XQ Data Explorer also exposed search, opacity/color controls,
    and a collapsible properties table.
- Scope decision:
  - Do not copy the legacy BlueBerry Data Manager plugin wholesale in this
    slice.
  - Keep the monolith `DataHierarchyModel` as the tree model so existing
    project/catalog/selection behavior remains stable.
  - Restore the old panel structure and UX anchors first; defer full
    `QmitkDataStorageTreeModel` migration to a later isolated slice.
- RED test observed before production code:
  - Extended `test_monolith_main_window_data_panel` to require
    `xqDataManagerSearchBox`, `xqDataOpacitySlider`,
    `xqDataOpacityValueLabel`, `xqDataColorButton`,
    `xqDataPropertiesToggle`, and `xqDataPropertiesTable`.
  - The test first failed because the search box did not exist.
- Implemented the slice:
  - Wrapped the Data Manager dock content in `xqDataManagerPanel`.
  - Added the search box with placeholder `Search nodes...`.
  - Added opacity controls and a Color button.
  - Added a collapsed properties toggle/table pair.
  - Added view-layer search filtering through `QTreeView::setRowHidden()`.
- Debugging note:
  - The first properties toggle assertion failed because the test window was
    not shown, so Qt's `isVisible()` reflected parent visibility rather than
    the table's own toggle state in a real window.
  - Updated the Data Manager panel test to show the `MainWindow` before
    asserting visibility behavior.
- Targeted verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release -R "test_monolith_main_window_data_panel|test_monolith_main_window_selection_sync|test_monolith_main_window_data_actions" --output-on-failure --timeout 120`
    passed: 3/3.

## Current Run: Workbench View Menu Dock Toggles

- Continued after `5c42b89` with the next Workbench fidelity gap:
  - The monolith had the correct dock layout and top workflow toolbar, but no
    `View` menu for restoring/hiding Workbench panes.
  - Original Workbench users expect panel visibility to be recoverable through
    the menu rather than only by dragging docks.
- RED test observed before production code:
  - Extended `test_monolith_main_window_workbench_layout` to require
    `ViewMenu` and dock toggle actions for Data Manager, Image Navigator,
    Tools, Diagnostics, and Task History.
  - The test first failed because `ViewMenu` did not exist.
- Implemented the slice:
  - Added `ViewMenu`.
  - Stored Data Manager, Image Navigator, Workflow Tools, Diagnostics, and
    Task History docks as `MainWindow` members.
  - Added stable toggle action object names:
    `xqToggleDataManagerDockAction`,
    `xqToggleImageNavigatorDockAction`,
    `xqToggleWorkflowToolsDockAction`,
    `xqToggleDiagnosticsDockAction`, and
    `xqToggleTaskHistoryDockAction`.
  - Toggle actions reuse `QDockWidget::toggleViewAction()` so visibility and
    checked state stay synchronized.
- Debugging note:
  - The first toggle test failed because the test window was never shown, so
    Qt dock/action visibility state was not synchronized the same way it is in
    a real window.
  - Updated the test to show the `MainWindow` before asserting dock toggle
    behavior.
- Targeted verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release -R "test_monolith_main_window_workbench_layout|test_monolith_main_window_workflow_selection" --output-on-failure --timeout 120`
    passed: 2/2.
- Full verification before commit:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - Full XQ `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - View-menu runtime smoke launched `build\windows-msvc-release\bin\XQ.exe`,
    kept it alive for 10 seconds, and closed it cleanly.
  - `git diff --check` passed in both XQ and Externals.

## Current Run: Workbench Workflow Toolbar Restore

- Continued after `790214a` with the next UI-fidelity gap:
  - The original Workbench had prominent top-level workflow/tool entries with
    XQ SVG icons.
  - The monolith could only discover workflows through the right-dock list,
    which made it feel like a generic workflow shell.
- RED test observed before production code:
  - Extended `test_monolith_main_window_workflow_selection` to require
    `xqViewToolBar`, icon-backed actions for Image/Path/2D Seg/3D Seg/Model/
    Mesh/Simulation, and action-to-Core selection routing.
  - The test first failed because `xqViewToolBar` did not exist.
- Implemented the slice:
  - Added `xqViewToolBar` in `MainWindow`.
  - Added checkable workflow actions with stable object names
    `xqToolAction_<workflow-id>`.
  - Actions reuse original resources such as `:/xq/tool-process.svg`,
    `:/xq/tool-path.svg`, and `:/xq/tool-flow.svg`.
  - Triggering an action updates `WorkflowSelectionService`; Core-driven
    selection changes update the toolbar checked state.
- Debugging note:
  - The first implementation created toolbar actions but their icons were
    empty in Presentation-only tests because the old resource package was
    compiled into `xqMonolithApplication`, not `xqMonolithPresentation`.
  - Moved `xqApplication.qrc` to the Presentation target and explicitly
    registered it before MainWindow creates icon-backed actions.
- Targeted verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release -R "test_monolith_main_window_workflow_selection|test_monolith_main_window_workbench_layout|test_monolith_workbench_theme|test_monolith_application_import_wiring" --output-on-failure --timeout 120`
    passed: 4/4.
- Full verification before commit:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - Full XQ `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - Toolbar runtime smoke launched `build\windows-msvc-release\bin\XQ.exe`,
    kept it alive for 10 seconds, and closed it cleanly.
  - `git diff --check` passed in both XQ and Externals.

## Current Run: Monolith Workbench Theme Resource Restore

- Continued after `cad2162` with the next UI-fidelity gap:
  - The monolith had the Workbench dock shape and MITK Image Navigator, but it
    still did not load the original XQ visual resource package.
  - The legacy Workbench assets already exist in
    `org.xq.core.application/resources`, including `xq.qss`, `icon.png`, and
    workflow/tool SVG icons.
- RED test observed before production code:
  - Added `test_monolith_workbench_theme`.
  - The first build failed because `xq::ApplyXqWorkbenchTheme()` did not
    exist.
- Implemented the slice:
  - Added `xq::ApplyXqWorkbenchTheme(QApplication&, QString*)`.
  - The function registers the old `xqApplication.qrc`, applies the original
    Arctic Light palette, loads `:/xq/xq.qss`, and sets `:/xq/icon.png` as
    the application icon.
  - `main.cxx` now applies the theme during monolith startup and emits a
    warning if the resource cannot be loaded.
  - `xqMonolithApplication` now compiles the existing
    `org.xq.core.application/resources/xqApplication.qrc` with `AUTORCC`.
- Debugging note:
  - The first implementation compiled but the theme test failed because the
    resource object from the static library was not registered in the test
    executable.
  - Added explicit `Q_INIT_RESOURCE(xqApplication)` in the theme loader.
  - The first `Q_INIT_RESOURCE` attempt was inside an anonymous namespace and
    produced an unresolved `qInitResources_xqApplication` symbol; moving the
    helper to global namespace fixed the generated-symbol lookup.
- Targeted verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release -R "test_monolith_workbench_theme|test_monolith_main_window_workbench_layout|test_monolith_application_import_wiring" --output-on-failure --timeout 120`
    passed: 3/3.
- Full verification before commit:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - Full XQ `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - Themed runtime smoke launched `build\windows-msvc-release\bin\XQ.exe`,
    kept it alive for 10 seconds, and closed it cleanly.
  - `git diff --check` passed in both XQ and Externals.

## Current Run: MITK Image Navigator Dock

- Continued from pushed `f927c25` after the Workbench-style shell correction.
- Current UI fidelity gap:
  - `xqImageNavigatorDock` existed in the correct left-bottom Workbench
    position, but still contained only a placeholder widget.
  - The original XQ/MITK Workbench expectation is a real Image Navigator tied
    to the active MITK multi-widget render windows.
- RED test observed before production code:
  - `test_monolith_main_window_workbench_layout` was extended to require
    `MainWindow::SetImageNavigatorWidget()` and verify that the dock accepts
    an installed navigator widget.
  - The first build failed because `SetImageNavigatorWidget()` did not exist.
- Implemented the slice:
  - Added `MainWindow::SetImageNavigatorWidget()` and stored the
    `xqImageNavigatorDock` pointer for production composition.
  - Added `CreateMitkImageNavigator()` in the monolith composition layer.
  - The navigator creates axial, sagittal, coronal, and time
    `QmitkSliceNavigationWidget` controls.
  - Axial/sagittal/coronal controls are wired to `QmitkStdMultiWidget`
    render-window steppers; time is wired to MITK's time navigation
    controller.
  - `main.cxx` now installs the real MITK Image Navigator after creating the
    `QmitkStdMultiWidget` render host.
- Debugging note:
  - The first full build failed with
    `fatal error C1083: Cannot open include file: 'QmitkRenderWindow.h'`
    because `xqMonolithApplication` used MITK QtWidgets headers but did not
    link the `MitkQtWidgets` target.
  - Fixed the build graph by adding `MitkQtWidgets` and
    `Qt6::OpenGLWidgets` to `xqMonolithApplication`.
- Targeted verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release -R "test_monolith_main_window_workbench_layout|test_monolith_simulation_operation_pages|test_monolith_application_import_wiring" --output-on-failure --timeout 120`
    passed: 3/3.
- Full verification before commit:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - Full XQ `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 74/74.
  - Runtime smoke launched `build\windows-msvc-release\bin\XQ.exe`, kept it
    alive for 10 seconds, and closed it cleanly.
  - `git diff --check` passed in both XQ and Externals.

## Current Run: Windows V1 Solver Execution Scope Decision

- Continued from `006b652` with the user's clarified decision:
  - Do not use SimVascular-related code, schemas, solvers, or dependencies.
  - Do not wire ROM or MultiPhysics solver execution for Windows v1.
  - Keep ROM/MultiPhysics configuration and existing-result review.
- Started TDD from the existing blocker:
  - Updated tests so default ROM operations expose only `build-1d-network` and
    `calibrate-boundary-conditions`.
  - Updated tests so default MultiPhysics operations expose only
    `configure-coupling` and `review-coupled-results`.
  - Updated operation page tests so `run-rom-solver` and `run-coupled-solve`
    are not visible or user-selectable.
  - Updated direct guard fixtures to manually inject hidden solver operation ids
    and verify the Infrastructure handlers still fail honestly.
- Red test observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - Targeted `ctest` failed because the default operation registry still
    exposed `run-rom-solver` and `run-coupled-solve`.
- Implemented the v1 scope decision:
  - Removed solver-run descriptors from default ROM and MultiPhysics operation
    registration.
  - Kept Infrastructure fallback guards for stale/manual solver operation ids.
- Targeted green verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(domain_workflow_action_handlers|simulation_operation_pages|rom_simulation_workflow_action_handler|multiphysics_workflow_action_handler)"`
    passed: 4/4.
- Updated `plan.md` from solver-runtime blocker to completed Windows v1 scope
  decision.

## Current Run: Native ROM and Coupled Solver Runtime Blocker

- Continued from clean pushed `feature/windows-monolith-foundation` state after
  `183c6c2`.
- Re-scanned remaining unsupported/runtime guards:
  - Active solver gaps are `rom-simulation/run-rom-solver` and
    `multiphysics/run-coupled-solve`.
  - `calibrate-boundary-conditions` and `review-coupled-results` are already
    wired through native non-solver Infrastructure actions.
- Re-scanned XQ source for real backend candidates:
  - ROM code provides `xq_ROMJob`, `xq_MitkROMJob`, XML/project IO, job
    creation, network/configuration metadata, and boundary-condition
    calibration.
  - MultiPhysics code provides `xq_MultiPhysicsJob`, XML/project IO, coupling
    configuration metadata, and review for existing SimulationResult nodes.
  - Legacy ROM and MultiPhysics views explicitly state native solver execution
    is unavailable/disabled and that fake run/result nodes must not be
    created.
  - Flow Simulation remains the only current monolith workflow with a native
    solver/run/import backend.
- Re-scanned Externals recipes/manifests:
  - No Windows-buildable ROM 1D/0D solver, coupled/FSI solver, or matching
    result-import dependency is present.
- Decision:
  - Keep `run-rom-solver` and `run-coupled-solve` on deterministic
    unsupported-operation guards.
  - Do not create fake solver outputs or mark jobs as solver-run.
  - Resume implementation only after a real solver backend and result import
    contract are added to Externals/XQ with tests.
- Added `Blocked Phase: Native ROM and Coupled Solver Runtime Integration` to
  `plan.md`.

## Current Run: ROM Boundary Calibration Infrastructure Action

- Continued from clean pushed `feature/windows-monolith-foundation` state after
  `loft-profiles` was pushed as `9c33955`.
- Completed the active autonomous research refresh:
  - Remaining ROM/MultiPhysics gaps are solver/review-heavy except
    `calibrate-boundary-conditions`.
  - ROM already has `xq_ROMJob` RCR storage, `xq_MitkROMJob` deep-copy
    semantics, and generated ROM catalog support.
  - The narrow native slice is therefore a real ROM boundary-condition
    configuration/calibration action, not a solver run.
- Added next executable phase to `plan.md`: ROM Boundary Calibration
  Infrastructure Action.
- Starting RED tests first:
  - `calibrate-boundary-conditions` should require a selected ROM job.
  - A valid ROM job should create a calibrated copy with scaled outlet RCR
    resistances and honest `not_solver_run` metadata.
  - Production composition and the ROM operation page should validate
    calibration through the Infrastructure handler rather than the unsupported
    guard.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed after
    fixing one missing test include.
  - Targeted `ctest` failed because calibration still returned
    `Calibrate Boundary Conditions is not wired to a native ROM Simulation runtime yet.`
    through the unsupported-operation guard.
- Implemented ROM boundary calibration Infrastructure action:
  - The dynamic ROM handler now routes `calibrate-boundary-conditions` to a
    native configuration path while keeping `run-rom-solver` unsupported.
  - The handler resolves an existing `xq_MitkROMJob`, validates the source
    `xq_ROMJob`, clones it, scales outlet RCR resistances, and records
    target-flow/scale metadata.
  - Successful runs commit a generated ROMSimulation entry, select it, refresh
    rendering, and record `not_solver_run` limitations so no solver execution
    is implied.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(rom_simulation_workflow_action_handler|simulation_operation_pages|application_import_wiring)"`
    passed: 3/3.
- Full-gate verification before commit:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - XQ `tests\*.ps1` passed.
  - Full XQ `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - XQ `git diff --check` passed.
  - Externals `tests\*.ps1` passed.
  - Externals `git diff --check` passed.
- Marked ROM Boundary Calibration Infrastructure Action completed in
  `plan.md`.

## Current Run Update: MultiPhysics Result Review Infrastructure Action

- Continued from clean pushed `feature/windows-monolith-foundation` state after
  `ae60938`.
- Completed the next autonomous research refresh:
  - Remaining solver actions, `run-rom-solver` and `run-coupled-solve`, still
    lack native solver/result backends and should stay guarded.
  - `review-coupled-results` can be a real non-solver action when the selected
    data is an already-imported SimulationResult with named result fields.
  - The implementation can reuse `xq_ResultImport::SetActiveScalar` and store
    MultiPhysics review metadata without synthesizing result data.
- Added next executable phase to `plan.md`: MultiPhysics Result Review
  Infrastructure Action.
- Starting RED tests first:
  - `review-coupled-results` should require a selected result node.
  - A valid result node should activate a preferred coupled scalar, mark it
    visible, record review metadata, and refresh rendering.
  - Production composition and the MultiPhysics operation page should validate
    review through the Infrastructure handler rather than the unsupported
    guard.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed after
    fixing one missing test include.
  - Targeted `ctest` failed because `review-coupled-results` still returned
    `Review Coupled Results is not wired to a native Multi-Physics runtime yet.`
    through the unsupported-operation guard.
- Implemented MultiPhysics result review Infrastructure action:
  - The dynamic MultiPhysics handler now routes `review-coupled-results` to a
    native review path while keeping `run-coupled-solve` unsupported.
  - The handler resolves an existing SimulationResult node, discovers
    point/cell result fields, activates a preferred coupled scalar, marks the
    node visible, and stores `xq.review.multiphysics.*` metadata.
  - The action only prepares review for already-present result data and does
    not synthesize coupled solve results.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(multiphysics_workflow_action_handler|simulation_operation_pages|application_import_wiring)"`
    passed: 3/3.
- Full-gate verification before commit:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - XQ `tests\*.ps1` passed.
  - Full XQ `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - XQ `git diff --check` passed.
  - Externals `tests\*.ps1` passed.
  - Externals `git diff --check` passed.
- Marked MultiPhysics Result Review Infrastructure Action completed in
  `plan.md`.

## Previous Run: Loft Profiles Segmentation Infrastructure Action

- Continued from clean pushed `feature/windows-monolith-foundation` state after
  `threshold-contour` was pushed as `ad3237d`.
- Completed the active autonomous research refresh:
  - Remaining exposed 2D Segmentation gap is `loft-profiles`.
  - ROM/MultiPhysics still have solver/review gaps that should not be faked
    without a real backend/result path.
  - XQ already ships `xq_ContourGroupMigration`, canonical `xq_ProfileGroup`,
    and `xq_SegmentationUtils::LoftProfileGroup`, so the narrow native slice is
    to finalize contour/profile segmentations for downstream Modeling.
- Added next executable phase to `plan.md`: Loft Profiles Segmentation
  Infrastructure Action.
- Starting RED tests first:
  - `loft-profiles` should require a real contour/profile segmentation node.
  - A valid contour group should migrate to a canonical `xq_ProfileGroup`,
    attach a non-empty loft surface cache, register catalog/hierarchy/data-node
    bindings, select it, and refresh rendering.
  - Production composition and the 2D Segmentation page should validate
    `loft-profiles` through the Infrastructure handler rather than the
    unsupported-operation guard.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - Targeted `ctest` failed because `loft-profiles` still returned
    `Loft Profiles is not wired to a native 2D Segmentation runtime yet.`
    and production/page tests still expected unsupported behavior.
- Implemented Loft Profiles Segmentation Infrastructure action:
  - The dynamic Segmentation handler now routes `loft-profiles` to a native
    path.
  - The handler accepts canonical `xq_ProfileGroup` nodes or migrates legacy
    unbound `xq_ContourGroup` nodes through `xq_ContourGroupMigration`.
  - Successful runs generate a loft surface cache with
    `xq_SegmentationUtils::LoftProfileGroup`, commit a generated
    Segmentation/ProfileGroup result, select it, and refresh rendering.
  - Metadata records whether the result migrated from contours and clarifies
    that this is not Model-stage solid generation.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(segmentation_workflow_action_handler|segmentation_operation_page|application_import_wiring)"`
    passed: 3/3.
- Full-gate verification before commit:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - XQ `tests\*.ps1` passed.
  - Full XQ `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - XQ `git diff --check` passed.
  - Externals `tests\*.ps1` passed.
  - Externals `git diff --check` passed.
- Marked Loft Profiles Segmentation Infrastructure Action completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Update: Workbench Data Manager Core Menu Actions

- Continued from the fresh UI clone:
  - Active worktree:
    `C:\Users\OCEAN\Desktop\XIAOQUAN\XQ-fresh-ui`.
  - Branch: `feature/windows-monolith-foundation`.
  - First committed/pushed the previously verified representation-action
    slice as `bcf8df4`.
- Compared the monolith Data Manager against the original
  `xq_DataExplorerView` context menu.
  - Next UI fidelity gap chosen: restore `Rename...`, `Remove`,
    `Reinitialize Node`, and `Global Reinit` without reintroducing the
    BlueBerry Data Manager plugin.
- RED test 1:
  - Extended `test_monolith_main_window_data_panel`.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_main_window_data_panel"`
    failed as expected with:
    `Data Manager should restore rename, remove, and reinit context actions`.
- Implemented the UI slice:
  - Added stable Data Manager context actions:
    `xqRenameDataAction`, `xqRemoveSelectedDataAction`,
    `xqReinitializeSelectedDataAction`, and
    `xqGlobalReinitializeDataAction`.
  - Restored Workbench shortcuts: `F2` for rename and `Delete` for remove.
  - Wired remove through `DataManagementService`.
  - Wired selected/global reinit to MITK rendering initialization APIs.
- RED test 2:
  - Extended `test_monolith_data_management_service` to verify a successful
    service-level remove also removes the bound MITK node from
    `ApplicationContext::DataStorage()`.
  - The test failed as expected with:
    `remove should clear the bound node from DataStorage`.
- Implemented the service fix:
  - `DataManagementService` now optionally owns the application
    `mitk::DataStorage` pointer and removes the previously bound node from
    storage after a successful catalog/hierarchy/registry delete.
  - `ApplicationContext` passes its `DataStorage` into the management
    service.
  - Removed duplicate DataStorage deletion responsibility from
    `MainWindow::RemoveSelectedData()`.
- Targeted green verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(data_management_service|main_window_data_panel)"`
    passed: 2/2.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke passed: `build\windows-msvc-release\bin\XQ.exe` stayed
    running for 10 seconds and was then closed.

## Current Run Update: Workbench Path Planning Tool Panel Restore

- Compared the monolith Path page against the original
  `xq_VesselPlanningView.ui`.
  - Gap: the monolith page still exposed a generic selector/form instead of
    the old path-oriented panels: `Paths`, `Control Points`, and
    `Path Tools`.
- RED test:
  - Extended `test_monolith_path_operation_page` to require restored path
    planning group boxes, path/point table anchors, path operation buttons,
    and button-to-Core operation synchronization.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_path_operation_page"`
    failed as expected with:
    `Path page should restore legacy path planning panel groups`.
- Implemented the UI slice:
  - Added monolith Path group boxes:
    `xqPathPlanningPathsGroup`,
    `xqPathPlanningControlPointsGroup`, and
    `xqPathPlanningToolsGroup`.
  - Added `QTableView` anchors:
    `xqPathPlanningPathTableView` and `xqPathPlanningPointTableView`.
  - Added visible checkable path operation buttons:
    `xqPathPlanningAddPathButton`,
    `xqPathPlanningEditControlPointsButton`, and
    `xqPathPlanningSmoothPathButton`.
  - Kept the existing operation selector as the Core compatibility anchor and
    synchronized it with the visible restored path buttons.
  - Kept real execution routed through the existing workflow action handlers.
- Targeted green verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_path_operation_page"`
    passed: 1/1.
- Related targeted verification:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(path_operation_page|main_window_workflow_selection|workflow_primary_action_page|application_import_wiring)"`
    passed: 4/4.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke passed: `build\windows-msvc-release\bin\XQ.exe` stayed
    running for 10 seconds and was then closed.

## Current Run Update: Workbench 2D Segmentation Tool Panel Restore

- Compared the monolith 2D Segmentation page against the original
  `xq_LumenContouringView.ui`.
  - Gap: the monolith page still exposed a generic selector/form instead of
    the old contour-oriented panels: `Path Selection`, `Contour Groups`, and
    `Contour Tools`.
- RED test:
  - Extended `test_monolith_segmentation_operation_page` to require restored
    2D Segmentation group boxes, visible contour tool buttons, a contour-tool
    stack anchor, and tool-button-to-Core operation synchronization.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_segmentation_operation_page"`
    failed as expected with:
    `2D segmentation page should restore legacy contour panel groups`.
- Implemented the UI slice:
  - Added monolith 2D Segmentation group boxes:
    `xqSegmentation2DPathSelectionGroup`,
    `xqSegmentation2DContourGroupsGroup`, and
    `xqSegmentation2DContourToolsGroup`.
  - Added visible checkable contour operation buttons:
    `xqSegmentation2DThresholdContourButton`,
    `xqSegmentation2DManualContourButton`, and
    `xqSegmentation2DLoftProfilesButton`.
  - Kept the existing operation selector as the Core compatibility anchor and
    synchronized it with the visible restored contour buttons.
  - Kept real execution routed through the existing workflow action handlers.
- Targeted green verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_segmentation_operation_page"`
    passed: 1/1.
- Related targeted verification:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(segmentation_operation_page|main_window_workflow_selection|workflow_primary_action_page|application_import_wiring)"`
    passed: 4/4.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke passed: `build\windows-msvc-release\bin\XQ.exe` stayed
    running for 10 seconds and was then closed.

## Current Run Update: Workbench 3D Segmentation Tool Panel Restore

- Compared the monolith 3D Segmentation page against the original
  `xq_MitkSegmentationView.ui`.
  - Gap: the monolith page still looked like a generic operation selector and
    parameter form, while the original view used explicit tool panel groups:
    `Reference Image`, `Segmentation Tools`, and `Tool Parameters`.
- RED test:
  - Extended `test_monolith_segmentation_operation_page` to require restored
    3D Segmentation group boxes, visible tool buttons, a parameter-stack
    anchor, and tool-button-to-Core operation synchronization.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_segmentation_operation_page"`
    failed as expected with:
    `3D segmentation page should restore legacy tool panel groups`.
- Implemented the UI slice:
  - Added monolith 3D Segmentation group boxes:
    `xqSegmentation3DReferenceImageGroup`,
    `xqSegmentation3DSegmentationToolsGroup`, and
    `xqSegmentation3DToolParametersGroup`.
  - Added visible checkable tool buttons:
    `xqSegmentation3DThresholdButton`,
    `xqSegmentation3DRegionGrowButton`, and
    `xqSegmentation3DSurfacePreviewButton`.
  - Kept the existing operation selector as the Core compatibility anchor and
    synchronized it with the visible restored tool buttons.
  - Kept real execution routed through the existing workflow action handlers.
- Targeted green verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_segmentation_operation_page"`
    passed: 1/1.
- Related targeted verification:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(segmentation_operation_page|main_window_workflow_selection|workflow_primary_action_page|application_import_wiring)"`
    passed: 4/4.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke passed: `build\windows-msvc-release\bin\XQ.exe` stayed
    running for 10 seconds and was then closed.

## Previous Run: Threshold Contour Segmentation Infrastructure Action

- Continued from clean pushed `feature/windows-monolith-foundation` state after
  `trim-branches` was pushed as `c63a15b`.
- Completed the active autonomous research refresh:
  - Remaining exposed 2D Segmentation gaps are `threshold-contour` and
    `loft-profiles`; ROM/MultiPhysics still have solver/review gaps that
    should not be faked without a real backend.
  - 3D Slicer Segment Editor and MITK segmentation documentation both treat
    thresholding as a standard segmentation operation.
  - SimVascular documentation describes vessel workflows as path-based 2D
    contour extraction followed by lofting/modeling, matching XQ's existing
    `xq_SegmentationPipelineService::ExtractContours` route.
- Added next executable phase to `plan.md`: Threshold Contour Segmentation
  Infrastructure Action.
- Starting RED tests first:
  - `threshold-contour` should require a real Path plus a resolvable source
    Image.
  - A valid Path + Image should create a generated contour/profile result,
    record source/threshold metadata, register catalog/hierarchy/data-node
    bindings, select it, and refresh rendering.
  - Production composition and the 2D Segmentation page should validate
    `threshold-contour` through the Infrastructure handler rather than the
    unsupported-operation guard.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - Targeted `ctest` failed because `threshold-contour` still returned
    `Threshold Contour is not wired to a native 2D Segmentation runtime yet.`
    and production/page tests still expected unsupported behavior.
- Implemented Threshold Contour Segmentation Infrastructure action:
  - The dynamic Segmentation handler now routes `threshold-contour` to a native
    path.
  - The handler resolves a selected Path node, resolves its source Image node,
    calls `xq_SegmentationPipelineService::ExtractContours`, and commits the
    generated contour group into catalog/hierarchy/data-node services.
  - Successful runs stamp source image/path metadata, threshold lower/upper
    metadata, and an honest range-collapsed capability diagnostic.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(segmentation_workflow_action_handler|segmentation_operation_page|application_import_wiring)"`
    passed: 3/3.
- Full-gate verification before commit:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - XQ `tests\*.ps1` passed.
  - Full XQ `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - XQ `git diff --check` passed.
  - Externals `tests\*.ps1` passed.
  - Externals `git diff --check` passed.
- Marked Threshold Contour Segmentation Infrastructure Action completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Previous Run: Trim Branches Modeling Infrastructure Action

- Continued from clean pushed `feature/windows-monolith-foundation` state after
  `edit-control-points` was pushed as `b351f61`.
- Completed the active autonomous research refresh:
  - The remaining Modeling operation gap is `trim-branches`.
  - `xq_ModelPipelineService::CreateModel` already accepts `pathFilter` and
    records `xq.model.trim.path`, so the narrow native slice is a filtered
    branch/profile-group rebuild.
  - This slice will not claim complete boolean branch trimming beyond the
    existing model pipeline capabilities.
- Added next executable phase to `plan.md`: Trim Branches Modeling
  Infrastructure Action.
- Starting RED tests first:
  - `trim-branches` should require a real contour/profile-group segmentation
    node.
  - A valid profile group should create a generated `xq_Model` result, record
    trim-path/filter metadata, register catalog/hierarchy/data-node bindings,
    select it, and refresh rendering.
  - Production composition and the Modeling page should validate
    `trim-branches` through the Infrastructure handler rather than the
    unsupported-operation guard.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - Targeted `ctest` failed because `trim-branches` still returned
    `Trim Branches is not wired to a native Modeling runtime yet.` instead of
    creating a filtered model result.
- Implemented Trim Branches Modeling Infrastructure action:
  - The dynamic Modeling handler now routes `trim-branches` to a native path.
  - The handler resolves a selected `xq_ProfileGroup`, reads its `path_name`,
    and calls `xq_ModelPipelineService::CreateModel` with `pathFilter`.
  - Successful runs stamp trim-path/filter-only metadata, register
    catalog/hierarchy/data-node bindings, select the result, and refresh
    rendering.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(modeling_workflow_action_handler|modeling_meshing_operation_pages|application_import_wiring)"`
    passed: 3/3.
- Full-gate verification before commit:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - XQ `tests\*.ps1` passed.
  - Full XQ `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - XQ `git diff --check` passed.
  - Externals `tests\*.ps1` passed.
  - Externals `git diff --check` passed.
- Marked Trim Branches Modeling Infrastructure Action completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Previous Run: Edit Control Points Infrastructure Action

- Continued from clean pushed `feature/windows-monolith-foundation` state after
  `smooth-path` was pushed as `a93de18`.
- Completed the active autonomous research refresh:
  - Remaining exposed Path gap is `edit-control-points`.
  - XQ already ships `xq_CenterlineInteractor`, `xq_CenterlineOp`, and Path
    interaction resources used by the legacy centerline plugin.
  - The narrow native slice is to enable edit mode on the selected Path node,
    not to fake interactive point edits in a batch action.
- Added next executable phase to `plan.md`: Edit Control Points Infrastructure
  Action.
- Starting RED tests first:
  - `edit-control-points` should require a real Path DataNode.
  - A valid `xq_VesselCenterline` should be marked editable, show control
    points, receive a Path interactor, preserve the selected catalog id, avoid
    registering generated data, and refresh rendering.
  - Production composition and the Path page should validate
    `edit-control-points` through the Infrastructure handler rather than the
    unsupported-operation guard.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - Targeted `ctest` failed because `edit-control-points` still returned
    `Edit Control Points is not wired to a native Path runtime yet.` instead
    of enabling the Path interactor/editing state.
- Implemented Edit Control Points Infrastructure action:
  - The dynamic Path handler now routes `edit-control-points` to a native path.
  - The handler resolves a selected `xq_VesselCenterline`, loads the existing
    Path interaction state machine/config on `xq_CenterlineInteractor`, and
    attaches it to the selected node.
  - Successful runs set editable/control-point metadata, preserve the current
    catalog selection, avoid generated catalog entries, and refresh rendering.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(path_workflow_action_handler|path_operation_page|application_import_wiring)"`
    passed: 3/3.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Edit Control Points Infrastructure Action to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Previous Run: Smooth Path Infrastructure Action

- Continued from clean pushed `feature/windows-monolith-foundation` state after
  `loft-surface` was pushed as `1bb8e0d`.
- Completed the active autonomous research refresh:
  - Remaining exposed unsupported operations include Path `smooth-path` and
    `edit-control-points`, Modeling `trim-branches`, ROM/MultiPhysics solver
    follow-ups, and review actions.
  - `smooth-path` is the next narrow native slice because
    `xq_PathPipelineService::ExtractPathFromCenterline` already turns an
    existing `xq_VesselCenterline` into a smoothed Path node with real
    metadata.
  - `edit-control-points` remains later work because it needs interactive
    editing semantics rather than a batch generated result.
- Added next executable phase to `plan.md`: Smooth Path Infrastructure Action.
- Starting RED tests first:
  - Path workflow should accept selected Path catalog entries.
  - `smooth-path` should require a real Path DataNode.
  - A valid `xq_VesselCenterline` should generate a smoothed Path result,
    register catalog/hierarchy/data-node bindings, select it, and refresh
    rendering.
  - Production composition should validate `smooth-path` through the
    Infrastructure handler rather than the unsupported-operation guard.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - Targeted `ctest` failed because Path workflow did not accept Path catalog
    entries yet, so `smooth-path` failed at `Select compatible data before
    running Path.` before reaching the native handler.
- Implemented Smooth Path Infrastructure action:
  - `WorkflowContextService` now lets the Path workflow accept generated Path
    catalog entries.
  - The dynamic Path handler now routes `smooth-path` to a native path.
  - The handler resolves the selected `xq_VesselCenterline`, calls
    `xq_PathPipelineService::ExtractPathFromCenterline` with smoothing enabled,
    and stamps `smooth-path` preservation metadata.
  - Successful runs register catalog/hierarchy/data-node bindings, select the
    smoothed Path result, and refresh rendering.
  - `edit-control-points` remains on the unsupported-operation guard.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(path_operation_page|workflow_context_service|path_workflow_action_handler|application_import_wiring)"`
    passed: 4/4.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Smooth Path Infrastructure Action to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Previous Run: Loft Surface Modeling Infrastructure Action

- Continued from clean `feature/windows-monolith-foundation` checkouts after
  `generate-surface-mesh` was pushed.
- Completed the active autonomous research refresh:
  - Remaining exposed gaps include Path edit/smooth operations, Modeling
    `loft-surface` / `trim-branches`, and solver/review follow-ups for ROM and
    MultiPhysics.
  - `loft-surface` is the next narrow native slice because the test fixture
    already creates an `xq_ProfileGroup`, `xq_SegmentationUtils` can produce
    a lofted `vtkPolyData`, and `xq_Model` / `xq_PolyGeometry` can hold the
    generated surface honestly without claiming OCCT solid modeling.
  - ROM/MultiPhysics solver-like actions remain later work until a real
    backend/result path exists.
- Added next executable phase to `plan.md`: Loft Surface Modeling
  Infrastructure Action.
- Starting RED tests first:
  - `loft-surface` should require a real profile-group segmentation node.
  - A valid profile group should create a generated `xq_Model` surface result,
    register catalog/hierarchy/data-node bindings, stamp loft-surface
    metadata, select it, and refresh rendering.
  - Production composition should validate `loft-surface` through the
    Infrastructure handler rather than the unsupported-operation guard.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - Targeted `ctest` failed because `loft-surface` still returned
    `Loft Surface is not wired to a native Modeling runtime yet.` instead of
    creating a generated model result.
- Implemented Loft Surface Modeling Infrastructure action:
  - The dynamic Modeling handler now routes `loft-surface` to a native path.
  - The handler resolves the selected contour-group segmentation, validates an
    `xq_ProfileGroup`, reuses or regenerates its lofted `vtkPolyData`, and
    stores the surface in an `xq_Model` / `xq_PolyGeometry`.
  - Successful runs stamp loft-surface / surface-only metadata, register
    catalog/hierarchy/data-node bindings, select the result, and refresh
    rendering.
  - `trim-branches` remains on the unsupported-operation guard.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(modeling_workflow_action_handler|application_import_wiring)"`
    passed: 2/2.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Loft Surface Modeling Infrastructure Action to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Previous Run: 3D Threshold Region Infrastructure Action

- Continued from clean `feature/windows-monolith-foundation` checkouts after
  `boundary-layers` was pushed.
- Completed the active autonomous research refresh:
  - Remaining native 3D Segmentation gaps are `threshold-region` and
    `surface-preview`.
  - `xq_Seg3DUtils::ThresholdSegmentation` and `xq_MitkSeg3D::THRESHOLD`
    already exist, so `threshold-region` is the next narrow native slice.
  - `surface-preview` remains unsupported until a separate preview-specific
    action is designed.
- Added next executable phase to `plan.md`: 3D Threshold Region Infrastructure
  Action.
- Starting RED tests first:
  - `threshold-region` should require a real MITK image node.
  - A valid synthetic MITK image should create an `xq_MitkSeg3D` threshold
    result, register catalog/hierarchy/data-node bindings, select it, and
    refresh rendering.
  - Production composition should validate `threshold-region` through the
    Infrastructure handler rather than the Domain placeholder.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - Targeted `ctest` failed because `threshold-region` still returned
    `Threshold Region is not wired to a native 3D Segmentation runtime yet.`
    instead of creating a native `xq_MitkSeg3D` result.
- Implemented 3D Threshold Region Infrastructure action:
  - The dynamic Segmentation handler now routes `segmentation-3d` /
    `threshold-region` to a native path.
  - The handler resolves the selected MITK image, reads `threshold-lower` and
    `threshold-upper`, validates the threshold range, and calls
    `xq_Seg3DUtils::ThresholdSegmentation`.
  - Successful runs create an `xq_MitkSeg3D` threshold node, stamp
    `Segmentation3D` pipeline metadata, register catalog/hierarchy/data-node
    bindings, select the result, and refresh rendering.
  - `surface-preview` remains on the unsupported-operation guard.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(segmentation_workflow_action_handler|segmentation_operation_page|application_import_wiring)"`
    passed: 3/3.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted 3D Threshold Region Infrastructure Action to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Comparable workstations keep 3D segmentation surface/closed-surface
    preview close to the segmentation workflow before downstream modeling.
  - The monolith now supports `threshold-region` and `region-growing`; the
    remaining exposed 3D Segmentation operation is `surface-preview`.
  - XQ already has `xq_LumenSurface` plus `xq_Seg3DUtils::SmoothSurface` and
    `ComputeNormals`, so the next narrow native slice is preview generation
    from an existing `xq_MitkSeg3D`.
- Added next executable phase to `plan.md`: 3D Surface Preview Infrastructure
  Action.
- Starting RED tests first:
  - 3D Segmentation workflow should accept selected Segmentation entries so
    generated 3D segmentation nodes can be previewed.
  - `surface-preview` should require a real `xq_MitkSeg3D` node.
  - A valid `xq_MitkSeg3D` should create an `xq_LumenSurface` preview,
    register catalog/hierarchy/data-node bindings, select it, and refresh
    rendering.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - Targeted `ctest` failed because 3D Segmentation did not accept generated
    Segmentation entries yet, so `surface-preview` failed at
    `Select compatible data before running 3D Segmentation.`
- Implemented 3D Surface Preview Infrastructure action:
  - `WorkflowContextService` now lets `segmentation-3d` accept generated
    Segmentation catalog entries.
  - The dynamic Segmentation handler now routes `segmentation-3d` /
    `surface-preview` to a native path.
  - The handler resolves the selected `xq_MitkSeg3D`, reuses its surface mesh,
    applies optional smoothing plus normal generation, and stores the preview
    in `xq_LumenSurface`.
  - Successful runs stamp preview metadata, register catalog/hierarchy/data-node
    bindings, select the generated preview, and refresh rendering.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(workflow_context_service|segmentation_workflow_action_handler|application_import_wiring)"`
    passed: 3/3.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted 3D Surface Preview Infrastructure Action to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Remaining unsupported exposed operations were reviewed across Path,
    Modeling, Meshing, ROM, MultiPhysics, and Python API.
  - `generate-surface-mesh` is the next narrow native slice because the
    selected Model already carries a `vtkPolyData` surface and `xq_MitkGrid`
    can store a surface mesh without claiming full volume/TetGen behavior.
  - ROM/MultiPhysics solver/review operations remain later slices because
    creating fake solver results would be misleading.
- Added next executable phase to `plan.md`: Surface Meshing Infrastructure
  Action.
- Starting RED tests first:
  - `generate-surface-mesh` with a real model should create a surface-only
    `xq_MitkGrid` result instead of returning the unsupported diagnostic.
  - Production composition should validate `generate-surface-mesh` through the
    Infrastructure handler.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - Targeted `ctest` failed because `generate-surface-mesh` still returned
    `Generate Surface Mesh is not wired to a native Meshing runtime yet.`
- Implemented Surface Meshing Infrastructure action:
  - The dynamic Meshing handler now routes `generate-surface-mesh` to a native
    surface-preserve path.
  - The handler resolves the selected Model node, copies the upstream model
    `vtkPolyData` into an `xq_TetGenGrid` surface-only container, wraps it in
    `xq_MitkGrid`, and records `surface_only` / `surface-preserve` metadata.
  - Successful runs register Mesh catalog/hierarchy/data-node bindings, select
    the generated result, and refresh rendering.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(meshing_workflow_action_handler|application_import_wiring)"`
    passed: 2/2.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Surface Meshing Infrastructure Action to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

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

- Continued the autonomous Windows monolith migration loop after the user
  enabled full review/full access mode.
- Verified both local repositories were clean and synced on
  `feature/windows-monolith-foundation`.
- Reviewed Flow Simulation pipeline and handler state:
  - Existing Infrastructure Flow handler supports `configure-cfd-job`.
  - `xq_SimulationPrepPipelineService` already exposes
    `ExportForSolver` and `RunSolverAndImportResults`.
  - `xq_FlowSolverRegistry` registers the serial native `xq_simple_flow`
    backend, which can prepare, run, and import deterministic steady-flow
    results.
- Promoted the research refresh to completed in `plan.md`.
- Started Active Phase: Flow Simulation Steady Run Infrastructure Action
  Handler.
- Starting RED tests first:
  - `run-steady-flow` should reject non-SimulationPrep selections.
  - A valid SimulationPrep node should run/import through `xq_simple_flow`,
    register a SimulationResult catalog/hierarchy/data-node binding, select
    the first result, and refresh rendering.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed
    after extending the Flow Simulation handler test.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed after
    the test compiled.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R test_monolith_flow_simulation_workflow_action_handler`
    failed because `configure-cfd-job` still generated `xq_export_only`
    jobs, leaving `run-steady-flow` on placeholder behavior.
- Implemented Flow Simulation steady run infrastructure action:
  - `configure-cfd-job` now maps the steady solver profile to the registered
    native `xq_simple_flow` backend.
  - `run-steady-flow` now requires a selected SimulationPrep MITK node.
  - The handler calls
    `xq_SimulationPrepPipelineService::RunSolverAndImportResults`.
  - Imported result nodes are registered as SimulationResult catalog entries,
    added under the Simulations hierarchy, bound in the DataNode registry,
    selected, and followed by a render refresh.
  - `CreateConfiguredMainWindow()` composition-root coverage now verifies
    that `run-steady-flow` uses Infrastructure validation instead of the
    Domain placeholder.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(flow_simulation_workflow_action_handler|application_import_wiring)"`
    passed: 2/2.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 70/70.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Flow Simulation Steady Run Infrastructure Action Handler to
  completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Pushed XQ commit:
  - `6a1742f Wire steady flow simulation run action`.
- Continued immediately into the next autonomous research refresh:
  - Rechecked comparable result-review direction after native steady run
    import landed.
  - SimVascular-style workflows surface pressure, velocity, and wall-shear
    result fields immediately after a solver run.
  - MITK/Slicer-style workstations keep result review tied to selected data
    nodes and rendering properties.
  - Chosen next slice: Flow Results Review Infrastructure Action Handler.
- Promoted the research refresh to completed in `plan.md`.
- Started Active Phase: Flow Results Review Infrastructure Action Handler.
- Starting RED tests first:
  - `review-flow-results` should reject non-SimulationResult selections.
  - A valid imported result node should get active scalar/review metadata and
    a render refresh.
  - Production composition should validate `review-flow-results` through the
    Infrastructure handler, not the Domain placeholder.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed
    after extending Flow review tests.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed after
    the tests compiled.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(flow_simulation_workflow_action_handler|application_import_wiring)"`
    failed because `review-flow-results` still used the operation-aware
    Domain placeholder.
- Implemented Flow Results Review infrastructure action:
  - `review-flow-results` now requires a selected SimulationResult node.
  - The handler discovers imported result fields, prefers pressure, and calls
    `xq_ResultImport::SetActiveScalar`.
  - It marks the result visible/scalar-visible, writes
    `xq.review.flow.*` metadata, and refreshes rendering.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(flow_simulation_workflow_action_handler|application_import_wiring)"`
    passed: 2/2.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 70/70.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Flow Results Review Infrastructure Action Handler to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Pushed XQ commit:
  - `08bd316 Wire flow result review action`.
- Continued immediately into the next autonomous research refresh:
  - Rechecked available ROM/MultiPhysics modules after Flow run/review landed.
  - ROM already has `xq_ROMJob`, `xq_MitkROMJob`, and XML persistence support.
  - The smallest next monolith slice is therefore ROM job/network
    configuration, not solver execution.
  - Chosen next slice: ROM Build Network Infrastructure Action Handler.
- Promoted the research refresh to completed in `plan.md`.
- Started Active Phase: ROM Build Network Infrastructure Action Handler.
- Starting RED tests first:
  - `build-1d-network` should reject selections that do not resolve to a Mesh
    or SimulationPrep MITK node.
  - A valid Mesh selection should create and register an `xq_MitkROMJob`.
  - Production composition should validate `build-1d-network` through an
    Infrastructure handler, not the Domain placeholder.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed because
    `Infrastructure/xq_RomSimulationWorkflowActionHandler.h` did not exist.
- Implemented ROM build-network infrastructure action:
  - Added `RegisterDynamicRomSimulationWorkflowActionHandler()` and wired it
    into the monolith composition root after Domain registration.
  - `build-1d-network` now requires a selected Mesh or SimulationPrep MITK
    node and resolves upstream mesh provenance for SimulationPrep inputs.
  - The handler creates a validated `xq_ROMJob` wrapped in
    `xq_MitkROMJob`, marks it as `ROMSimulation`, stores source mesh/status
    metadata, registers catalog/hierarchy/data-node bindings, selects the new
    entry, and refreshes rendering.
  - Added `DataWorkflowRole::ROMSimulation` roundtrip/display/import/context
    support so generated ROM jobs persist and remain compatible selections.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(rom_simulation_workflow_action_handler|application_import_wiring)"`
    passed: 2/2.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 71/71.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted ROM Build Network Infrastructure Action Handler to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Pushed XQ commit:
  - `08c17ff Wire ROM network build action`.
- Continued immediately into the next autonomous research refresh:
  - Rechecked ROM and MultiPhysics modules after ROM network configuration
    landed.
  - ROM currently has job/data/IO support but no native solver backend to run
    without inventing a fake result path.
  - MultiPhysics already has `xq_MultiPhysicsJob`,
    `xq_MitkMultiPhysicsJob`, domain/equation validation, and XML persistence.
  - Chosen next slice: MultiPhysics coupling job configuration, not solver
    execution.
- Promoted the research refresh to completed in `plan.md`.
- Started Active Phase: MultiPhysics Configure Coupling Infrastructure Action
  Handler.
- Starting RED tests first:
  - `configure-coupling` should reject selections that do not resolve to a
    ROMSimulation or SimulationPrep MITK node.
  - A valid ROMSimulation selection should create and register an
    `xq_MitkMultiPhysicsJob`.
  - Production composition should validate `configure-coupling` through an
    Infrastructure handler, not the Domain placeholder.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed because
    `Infrastructure/xq_MultiPhysicsWorkflowActionHandler.h` did not exist.
- Implemented MultiPhysics configure-coupling infrastructure action:
  - Added `RegisterDynamicMultiPhysicsWorkflowActionHandler()` and wired it
    into the monolith composition root after ROM registration.
  - `configure-coupling` now requires a selected ROMSimulation or
    SimulationPrep MITK node.
  - The handler creates a validated `xq_MultiPhysicsJob` wrapped in
    `xq_MitkMultiPhysicsJob` with fluid and solid domains plus one FSI
    equation.
  - Generated MultiPhysics nodes store source ROM/simulation metadata,
    coupling parameters, configured status, catalog/hierarchy/data-node
    bindings, selection, and render refresh.
  - Added `DataWorkflowRole::MultiPhysics` roundtrip/display/import/context
    support so generated coupling jobs persist and remain compatible
    selections.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(multiphysics_workflow_action_handler|application_import_wiring)"`
    passed: 2/2.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 72/72.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted MultiPhysics Configure Coupling Infrastructure Action Handler to
  completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Pushed XQ commit:
  - `dedff64 Wire multiphysics coupling action`.
- Continued immediately into the next autonomous research refresh:
  - Rechecked remaining placeholder-heavy operations after Flow, ROM, and
    MultiPhysics configuration handlers landed.
  - ROM and MultiPhysics solver operations still lack native monolith runtime
    backends, so running them now would be a fake implementation.
  - Python API already has `xq_PythonApiService`, which exposes deterministic
    version and runtime-availability diagnostics without requiring pybind11.
  - Chosen next slice: Python API availability diagnostic for
    `open-python-console`, not script execution.
- Promoted the research refresh to completed in `plan.md`.
- Started Active Phase: Python API Availability Infrastructure Action
  Handler.
- Starting RED tests first:
  - Dynamic Python API handler registration should be discoverable.
  - `open-python-console` should return the real unavailable-runtime
    diagnostic from `xq_PythonApiService`.
  - Unsupported Python API operations should continue using the existing
    operation-aware placeholder.
  - Production composition should validate `open-python-console` through an
    Infrastructure handler, not the Domain placeholder.

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

## Current Run Update: Windows Dotenv Environment Activation

- User asked why there are so many environment bugs and whether a `.env`-style
  virtual environment can be used.
- Root cause framing:
  - XQ is a native Windows MSVC + Qt + MITK application, so Python-style venv
    alone cannot isolate the runtime.
  - The fragile parts are VS2022 toolchain activation, CMake discovery,
    `XQ_EXTERNALS_ROOT`, build directory selection, `PATH`, `QT_PLUGIN_PATH`,
    and `XQ_PLUGIN_PATH`.
  - A project-local `.env` plus PowerShell activation script is the right
    equivalent for this native desktop stack.
- Added RED regression test first:
  - `tests\test_windows_dotenv_virtual_environment.ps1` initially failed
    because `scripts\xq-dotenv.ps1` did not exist.
- Implemented environment activation:
  - Added tracked `.env.example` with `XQ_EXTERNALS_ROOT`, `XQ_BUILD_DIR`,
    `XQ_EXTERNALS_PLATFORM`, `XQ_VS_INSTALL_PATH`, and `XQ_CMAKE`.
  - Added `.env` to `.gitignore`.
  - Added `scripts\xq-dotenv.ps1` shared dotenv parser.
  - Added `scripts\xq-toolchain.ps1` shared VS2022/CMake helper functions.
  - Added `scripts\Enter-XQEnvironment.ps1` to activate VS2022, CMake, XQ
    Externals, and runtime PATH in the current PowerShell process.
  - Updated `scripts\build-xq.ps1`, `scripts\run-xq.ps1`, and
    `scripts\xq-env.ps1` to load `.env`.
  - Kept command-line parameters higher precedence than process env, process
    env higher precedence than `.env`, and `.env` higher precedence than
    defaults.
- Local workstation setup:
  - Created ignored `.env` pointing to `..\Externals`,
    `build\windows-msvc-release`, `windows-x64`, and
    `C:\software\Visual Studio\Visual Studio2022\Community`.
- Targeted verification:
  - `tests\test_windows_dotenv_virtual_environment.ps1` passed.
  - `tests\test_windows_env_scripts.ps1` passed.
  - `scripts\build-xq.ps1 doctor` passed and resolved the local Externals,
    VS2022 Community installation, and VS CMake.
- Full verification:
  - `scripts\build-xq.ps1 configure` passed using `.env` without an explicit
    `-ExternalsRoot`.
  - `scripts\build-xq.ps1 build` passed and staged runtime DLLs.
  - Initial PowerShell test harness using `$LASTEXITCODE` misreported failures
    because `$LASTEXITCODE` can carry stale native-command status through
    successful `.ps1` scripts.
  - Re-ran with try/catch script-error detection: all XQ PowerShell tests
    passed, 20/20.
  - Re-ran Externals PowerShell tests with try/catch script-error detection:
    passed, 31/31.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed, 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Clean-PATH direct startup smoke for
    `build\windows-msvc-release\bin\XQ.exe` passed.
- Promoted Windows Dotenv Environment Activation to completed in `plan.md`.

## Current Run Update: Workbench Menu and Toolbar Skeleton Restore

- Continued the active XQ Windows monolith goal after the dotenv environment
  slice.
- Next visible fidelity gap:
  - The monolith had substantial workflow pages and a real MITK render host,
    but its top-level Workbench shell still exposed only a minimal File/View
    menu and a small action toolbar.
  - Original XQ Workbench had File/Edit/View/Tools menus and a project/
    undo-redo/screenshot/workflow toolbar skeleton, so the first-viewport UI
    still looked too unlike the original application.
- RED test:
  - Added `test_monolith_workbench_menu_toolbar`.
  - First run failed as expected with
    `Workbench menu bar should expose the original Edit menu`.
- Additional hygiene found during RED:
  - `.gitignore` used `Testing/`, which also ignored new source tests under
    `Code/Testing`.
  - Changed it to `/Testing/` so only root CTest artifacts are ignored.
- Implementation:
  - Restored File, Edit, View, and Tools top-level menus with stable object
    names.
  - Restored File actions for New Project, Open Project, Save, Save As,
    Close Workspace, Open Data File, Import DICOM, Save All as MITK Scene,
    Recent Projects, and Exit.
  - Restored Edit Undo/Redo, View Screenshot/Volume Rendering/Crosshair/View
    Presets, and Tools Preferences/measurement entries.
  - Restored main toolbar entries for Open Project, Save, Undo, Redo,
    Screenshot, Open Data File, and Remove Data.
  - Existing migrated Save/Open Data/Remove Data behavior remains wired to
    current monolith services.
  - Unmigrated Workbench actions post deterministic Windows v1 diagnostics
    rather than invoking BlueBerry/CTK legacy behavior.
- Targeted verification:
  - `scripts\build-xq.ps1 build` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(workbench_menu_toolbar|main_window_workbench_layout|main_window_import_action|main_window_project_save_action|main_window_data_actions|workbench_theme)"`
    passed, 6/6.
- Full verification:
  - `scripts\build-xq.ps1 configure` passed.
  - `scripts\build-xq.ps1 build` passed.
  - XQ PowerShell tests passed, 20/20.
  - Externals PowerShell tests passed, 31/31.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed, 76/76.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Clean-PATH direct startup smoke for
    `build\windows-msvc-release\bin\XQ.exe` passed.
- Promoted Workbench Menu and Toolbar Skeleton Restore to completed in
  `plan.md`.

## Current Run Final Update: Workbench UI and Direct Startup Runtime

- User feedback:
  - The monolith UI still looked too different from the original XQ/MITK
    Workbench.
  - Directly starting `XQ.exe` from Explorer produced Windows loader dialogs
    for missing `MitkCore.dll`, `MitkQtWidgets.dll`, `ITKCommon-5.4.dll`,
    `CppMicroServices.dll`, and then `zstd.dll`.
- Image Preprocessing UI restoration:
  - Added RED assertions for the original `xq_ImageProcessingView` structure:
    context status, `Input`, `Operation`, `Threshold Parameters`, `Seed`,
    `Crop Region`, `Resample / Surface`, and diagnostics text.
  - Implemented the restored Workbench-style groups in the monolith page while
    preserving the existing operation selector, dynamic parameter panel,
    action button, and Core operation state.
  - Updated workflow-context status coverage so Image Preprocessing can use
    the restored `xqImagePreprocessingContextLabel`.
- Runtime root cause:
  - The earlier smoke test was misleading because it inherited the build/test
    PATH.
  - `build/windows-msvc-release/bin` only contained XQ module DLLs; external
    MITK/ITK/VTK/Qt runtime DLLs were found only through `scripts/xq-env.ps1`.
  - `Qt6Core.dll` imported `zstd.dll`; Qt's build cache showed
    `zstd_DIR=C:/software/anaconda/Library/lib/cmake/zstd`, so the existing Qt
    build had linked against a host Anaconda runtime.
- Runtime implementation:
  - Added `Code/CMake/XQStageWindowsRuntime.cmake`.
  - Added a Windows `xqStageWindowsRuntime` build target to stage external
    runtime DLLs and Qt `platforms/qwindows.dll` next to `XQ.exe` on every
    build.
  - Added explicit zstd runtime staging for the current Qt build, resolved
    from Qt's build cache when Qt was built with host zstd support.
  - Added `tests/test_windows_runtime_staging.ps1`.
- Externals dependency hygiene:
  - Added `tests/test_qt_windows_no_host_zstd.ps1`.
  - Added `-no-feature-zstd` to the Qt recipe so future Qt rebuilds do not link
    `Qt6Core.dll` against host Anaconda zstd.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - XQ PowerShell tests in `tests\*.ps1` passed: 19/19.
  - Externals PowerShell tests in `tests\*.ps1` passed: 31/31.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ` and `Externals`.
  - `Start-XQ.cmd` smoke passed; launcher stayed active for 10 seconds.
  - Clean-PATH direct `build\windows-msvc-release\bin\XQ.exe` smoke passed;
    process stayed alive for 10 seconds.

## Current Run Update: Workbench Workflow Toolbar Fidelity

- Continued UI fidelity after the fresh Workbench panels and Tools dock
  readability correction.
- Source comparison:
  - The original `xq_WorkbenchWindowAdvisor` created explicit workflow
    `QToolButton` widgets for XQ tools and gave them 36px icons.
  - The monolith shell still used default `QToolBar::addAction()` buttons with
    text beside icons, making the first-viewport workflow strip look like a
    generic action bar and prone to horizontal crowding.
- Red test observed:
  - Extended `test_monolith_main_window_workflow_selection` first.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_main_window_workflow_selection"`
    failed on `Workflow tool buttons should have stable Workbench object names`.
- Implemented the toolbar correction:
  - Kept the existing workflow `QAction` objects and Core selection routing.
  - Switched `xqViewToolBar` to icon-over-text layout with 36px icons.
  - Assigned each workflow button a stable `xqToolButton_*` object name.
  - Set compact width/height constraints so the toolbar fits the default
    Workbench window width.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_main_window_workflow_selection"`
    passed: 1/1.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(main_window_workbench_layout|main_window_workflow_selection|workbench_theme|workflow_primary_action_page|simulation_operation_pages|modeling_meshing_operation_pages|path_operation_page|segmentation_operation_page)"`
    passed: 8/8.
- Final verification for this slice:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke started `build\windows-msvc-release\bin\XQ.exe`, kept it
    alive for 10 seconds, and closed it successfully.

## Current Run Update: Workspace Explorer Project Page Restore

- Continued UI fidelity from the pushed toolbar commit
  `5af63c1`.
- Source comparison:
  - The original `xq_WorkspaceExplorer` exposed a `Project Information` group,
    a `Project Structure` tree, and New/Open/Refresh/Open Folder command
    anchors.
  - The monolith Project page still showed only bare metadata labels, making
    the project-entry workflow feel generic and unlike the original XQ
    Workbench.
- Red test observed:
  - Extended `test_monolith_project_workflow_page` first.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_project_workflow_page"`
    failed on `project page should restore the Workspace Explorer project information group`.
- Implemented the Project page restoration:
  - Added `xqProjectInformationGroup` with project metadata labels and
    `xqProjectOpenFolderButton`.
  - Added `xqProjectStructureTree` with `Project Structure` header and
    no-project root state.
  - Added `xqProjectNewButton`, `xqProjectOpenButton`, and
    `xqProjectRefreshButton` command anchors.
  - Refreshed the tree from the new monolith project/data hierarchy services.
  - Kept empty role folders hidden after data removal.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_project_workflow_page"`
    passed: 1/1.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(project_workflow_page|data_workflow_page|main_window_project_state|main_window_project_save_action|main_window_data_panel|main_window_workbench_layout|main_window_import_action|main_window_data_actions)"`
    passed: 8/8.
- Final verification for this slice:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke started `build\windows-msvc-release\bin\XQ.exe`, kept it
    alive for 10 seconds, and closed it successfully.

## Current Run Update: Data Workflow Page Workbench Panel Restore

- Continued UI fidelity after the pushed Project page commit
  `9ee7b7b`.
- Source comparison:
  - The original Data Explorer is a structured data-management view with
    search/tree controls, opacity/color controls, properties, and context
    actions.
  - The monolith Data Manager dock already covers most of that original view,
    but the Data workflow page still showed loose selected-data labels only.
- Red test observed:
  - Extended `test_monolith_data_workflow_page` first.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_data_workflow_page"`
    failed on `data page should restore the Workbench selected data group`.
- Implemented the Data page restoration:
  - Added `Selected Data`, `Provenance`, and `Data Actions` groups.
  - Kept existing metadata labels under the restored groups.
  - Added Open Data File, Rename, Remove, Show Only Selected, and
    Reinitialize Node command anchors.
  - Routed page buttons to the same MainWindow handlers used by the toolbar
    and Data Manager context actions.
  - Synced page button enabled states with selected catalog entry and selected
    MITK node binding state.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_data_workflow_page"`
    passed: 1/1.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(data_workflow_page|main_window_data_panel|main_window_data_actions|main_window_import_action|main_window_selection_sync|project_workflow_page|main_window_workbench_layout)"`
    passed: 7/7.
- Final verification for this slice:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke started `build\windows-msvc-release\bin\XQ.exe`, kept it
    alive for 10 seconds, and closed it successfully.

## Current Run Update: Python API Workbench Panel Restore

- Continued UI fidelity after the pushed Data page commit
  `c95ea96`.
- Source comparison:
  - The Python API module already has a clear C++ service contract: runtime
    availability, node inspection, snippet export, and project save/open
    surfaces.
  - The monolith Python API page still used only the generic operation selector
    and did not make the runtime-unavailable guard visible in the page itself.
- Red test observed:
  - Extended `test_monolith_python_api_operation_page` first.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_python_api_operation_page"`
    failed on `Python API page should restore a runtime status panel`.
- Implemented the Python API page restoration:
  - Added `Runtime Status`, `Snippet Catalog`, and `Project Script Runner`
    groups.
  - Kept the generic operation selector hidden while preserving Core operation
    state.
  - Added `Check Runtime`, `Export Snippets`, and `Run Project Script` command
    buttons.
  - Routed panel buttons through `WorkflowOperationService` and the existing
    `WorkflowActionService` dynamic infrastructure handler.
  - Kept the Windows v1 script runtime notice explicit and honest.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_python_api_operation_page"`
    passed: 1/1.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(python_api_operation_page|python_api_workflow_action_handler|application_import_wiring|domain_workflow_action_handlers|workflow_operation_service|workflow_primary_action_page|workflow_context_status_page)"`
    passed: 7/7.
- Final verification for this slice:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke started `build\windows-msvc-release\bin\XQ.exe`, kept it
    alive for 10 seconds, and closed it successfully.

## Current Run Update: Workbench Tools Dock Readability Correction

- Runtime screenshot QA after restoring workflow panels showed a real UI
  regression:
  - The right-side `Tools` dock still showed the workflow navigation list next
    to the selected page.
  - The selected tool page was squeezed to the right edge and was not readable
    on startup.
- Red tests observed:
  - Extended `test_monolith_main_window_workbench_layout` first.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_main_window_workbench_layout"`
    failed on `Workflow navigation should stay hidden so the right Tools dock shows the selected tool page`.
  - After hiding navigation and making the page stack primary, the same test
    failed on `Workflow Tools dock should be wide enough to show the selected tool page on startup`.
- Implemented the Tools dock correction:
  - Removed the visible horizontal workflow splitter from the Tools dock.
  - Kept `xqWorkflowNavigation` alive but hidden for existing Core/list/page
    synchronization behavior.
  - Made `xqWorkflowPages` the primary visible content in
    `xqWorkflowToolsPanel`.
  - Set practical minimum widths for the Tools panel/page stack.
  - Initialized the right dock width with `resizeDocks`.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(main_window_workbench_layout|main_window_workflow_selection|workflow_primary_action_page|simulation_operation_pages|modeling_meshing_operation_pages|path_operation_page|segmentation_operation_page)"`
    passed: 7/7.
- Final verification for this slice:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke started `build\windows-msvc-release\bin\XQ.exe`, kept it
    alive for 10 seconds, and closed it successfully.

## Current Run Update: Workbench Modeling Tool Panel Restore

- Continued UI fidelity work from the fresh
  `feature/windows-monolith-foundation` clone at
  `C:\Users\OCEAN\Desktop\XIAOQUAN\XQ-fresh-ui`.
- Red test observed:
  - `test_monolith_modeling_meshing_operation_pages` failed on
    `Modeling page should restore legacy model selector row` because the
    monolith Modeling page still exposed only the generic workflow operation
    shell.
- Implemented the Modeling panel restoration:
  - Restored the legacy `Model:` selector row.
  - Added the faces table anchor.
  - Added Create/Edit/Export operation tabs.
  - Added visible Modeling buttons for Loft Surface, Build Solid Model, and
    Trim Branches.
  - Kept the hidden generic operation selector so existing Core operation
    state and tests remain compatible.
  - Wired Modeling buttons through `WorkflowOperationService` so toolbar,
    selector, and button state stay synchronized.
- Red/green target verification:
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_modeling_meshing_operation_pages"`
    passed: 1/1.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(modeling_meshing_operation_pages|main_window_workflow_selection|workflow_primary_action_page|application_import_wiring)"`
    passed: 4/4.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke passed: `XQ.exe` launched and stayed running for 10 seconds.

## Current Run Update: Workbench ROM and MultiPhysics V1 Panel Restore

- Started the next UI fidelity slice after pushing Flow Simulation.
- Reviewed the original ROM/MultiPhysics plugin views:
  - `xq_ROMSimulationView.cxx`
  - `xq_ROMSimulationView.h`
  - `xq_MultiPhysicsView.cxx`
  - `xq_MultiPhysicsView.h`
- Scope reminder:
  - Windows v1 does not expose ROM or coupled solver execution.
  - `run-rom-solver` and `run-coupled-solve` remain hidden from the page and
    operation selectors.
- Red test observed:
  - Extended `test_monolith_simulation_operation_pages` first.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_simulation_operation_pages"`
    failed on `ROM Simulation page should restore legacy status workflow text`
    because the monolith ROM page still exposed only the generic workflow
    operation shell.
- Implemented ROM/MultiPhysics v1 panel restoration:
  - Restored ROM and MultiPhysics status labels using the legacy
    no-selection/open-view wording.
  - Added read-only workflow step summaries for ROM metadata and MultiPhysics
    XML metadata workflows.
  - Added ROM Build 1D Network and Calibrate Boundary Conditions buttons.
  - Added MultiPhysics Configure Coupling and Review Coupled Results buttons.
  - Added disabled metadata export anchors for future authorized export work.
  - Added explicit solver-deferred notices for ROM and coupled execution.
  - Kept hidden generic operation selectors so existing Core operation state
    and tests remain compatible.
  - Wired restored buttons through `WorkflowOperationService` so selector and
    button state stay synchronized.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_simulation_operation_pages"`
    passed: 1/1.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(simulation_operation_pages|rom_simulation_workflow_action_handler|multiphysics_workflow_action_handler|domain_workflow_action_handlers|application_import_wiring|workflow_primary_action_page)"`
    passed: 6/6.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke passed: `XQ.exe` launched and stayed running for 10 seconds.

## Current Run Update: Workbench Flow Simulation Tool Panel Restore

- Started the next UI fidelity slice after pushing Meshing.
- Reviewed the original Flow Simulation plugin resources:
  - `xq_HemodynamicsView.ui`
  - `xq_SimJobCreate.ui`
  - `xq_CapBCWidget.cxx`
  - `xq_HemodynamicsView.cxx`
- Red test observed:
  - Extended `test_monolith_simulation_operation_pages` first.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_simulation_operation_pages"`
    failed on `Flow Simulation page should restore legacy job row` because
    the monolith Flow Simulation page still exposed only the generic workflow
    operation shell.
- Implemented the Flow Simulation panel restoration:
  - Restored the legacy Job row, Create Job command anchor, and Mesh selector
    row.
  - Added the Basic, Inlet/Outlet BCs, Wall Properties, Solver Parameters,
    Run, and Results tabs.
  - Restored time stepping controls, BC table, wall/deformable controls,
    solver presets/settings, run/export/log controls, and result visualization
    controls.
  - Added visible Configure CFD Job, Steady Flow Solve, and Review Results
    operation buttons mapped to the monolith Flow operations.
  - Kept the hidden generic operation selector so existing Core operation
    state and tests remain compatible.
  - Wired Flow buttons through `WorkflowOperationService` so selector and
    button state stay synchronized.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_simulation_operation_pages"`
    passed: 1/1.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(simulation_operation_pages|flow_simulation_workflow_action_handler|main_window_workflow_selection|workflow_primary_action_page|application_import_wiring|domain_workflow_action_handlers)"`
    passed: 6/6.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke passed: `XQ.exe` launched and stayed running for 10 seconds.

## Current Run Update: Workbench Meshing Tool Panel Restore

- Started the next UI fidelity slice after pushing Modeling.
- Reviewed the original Meshing plugin resources:
  - `xq_GridGenerationView.ui`
  - `xq_MeshCreate.ui`
  - `xq_GridGenerationView.cxx`
- Red test observed:
  - Extended `test_monolith_modeling_meshing_operation_pages` first.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_modeling_meshing_operation_pages"`
    failed on `Meshing page should restore legacy model selector row` because
    the monolith Meshing page still exposed only the generic workflow
    operation shell.
- Implemented the Meshing panel restoration:
  - Restored the legacy `Model:` row and `New Mesh...` command anchor.
  - Added Global Settings, Local Size, Boundary Layer, Refinement Regions, and
    Advanced tabs.
  - Restored TetGen/global edge controls, local-size and refinement-region
    tables, boundary-layer controls, mesh statistics labels, quality report,
    and export anchors.
  - Added visible Surface Mesh, Volume Mesh, and Boundary Layers operation
    buttons mapped to the monolith Meshing operations.
  - Kept the hidden generic operation selector so existing Core operation
    state and tests remain compatible.
  - Wired Meshing buttons through `WorkflowOperationService` so selector and
    button state stay synchronized.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_modeling_meshing_operation_pages"`
    passed: 1/1.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(modeling_meshing_operation_pages|main_window_workflow_selection|workflow_primary_action_page|application_import_wiring|meshing_workflow_action_handler|domain_workflow_action_handlers)"`
    passed: 6/6.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke passed: `XQ.exe` launched and stayed running for 10 seconds.

## Current Run Final Update: Workbench Data Manager Panel Restore

- Restored the monolith Data Manager dock to the original XQ Data Explorer
  panel shape while keeping the current monolith data hierarchy model.
- Added the Workbench-style search box, opacity slider, opacity percentage
  label, Color button, collapsed Properties section, and two-column property
  table.
- Added first-pass panel behavior:
  - search filters tree rows without mutating the underlying model;
  - opacity slider updates its percentage label;
  - Properties toggle shows and hides the property table.
- Updated Workbench layout coverage because the Data Manager dock now owns a
  panel containing the hierarchy tree rather than the tree directly.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ` and `Externals`.
  - Runtime smoke launched `build\windows-msvc-release\bin\XQ.exe`; the
    application stayed running for 10 seconds and was then closed.
- Promoted Workbench Data Manager Panel Restore to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Final Update: Workbench Data Manager MITK Property Sync

- Started the next UI fidelity slice after restoring the Data Manager panel
  shape: make the panel behave more like the original XQ Data Explorer when
  selecting MITK-backed data.
- Red test observed:
  - Extended `test_monolith_main_window_data_panel` to bind the imported
    catalog entry to a `mitk::DataNode` with opacity/color/visibility.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release -R test_monolith_main_window_data_panel --output-on-failure --timeout 120`
    failed because selecting a MITK-backed data row did not load node opacity.
- Implemented Data Manager MITK property sync:
  - MainWindow now keeps member pointers for the restored opacity slider,
    color button, and properties table.
  - Selection changes, catalog changes, and DataNode registry binding changes
    update the Data Manager controls.
  - The opacity slider loads the selected node opacity and writes back to the
    selected node's `opacity` property.
  - The color button reflects the selected node color.
  - The properties table shows catalog metadata plus node name, visibility,
    opacity, color, data type, and common XQ/DICOM property rows.
  - Opacity edits request a MITK rendering update.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release -R test_monolith_main_window_data_panel --output-on-failure --timeout 120`
    passed: 1/1.
  - Related targeted window/data tests passed: 7/7.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ` and `Externals`.
  - Runtime smoke launched `build\windows-msvc-release\bin\XQ.exe`; the
    application stayed running for 10 seconds and was then closed.
- Promoted Workbench Data Manager MITK Property Sync to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Final Update: Workbench Data Manager Visibility Actions

- Started the next UI fidelity slice: restore the original Data Explorer's
  non-dialog visibility actions in the monolith Data Manager.
- Red test observed:
  - Extended `test_monolith_main_window_data_panel` to require a Workbench-style
    action context menu and visibility actions for selected MITK-backed data.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release -R test_monolith_main_window_data_panel --output-on-failure --timeout 120`
    failed because the Data Manager tree did not expose an action context menu.
- Implemented Data Manager visibility actions:
  - The Data Manager tree now uses `Qt::ActionsContextMenu`.
  - Added stable actions:
    `xqToggleDataVisibilityAction`,
    `xqShowOnlySelectedDataAction`,
    `xqMakeAllDataVisibleAction`, and
    `xqMakeAllDataInvisibleAction`.
  - Selection-dependent actions enable only when the selected catalog entry has
    a bound MITK node.
  - Toggle/Show Only/Make All actions update registered MITK nodes' `visible`
    property, refresh the properties table, and request a MITK render update.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release -R test_monolith_main_window_data_panel --output-on-failure --timeout 120`
    passed: 1/1.
  - Related targeted Data Manager/data tests passed: 6/6.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ` and `Externals`.
  - Runtime smoke launched `build\windows-msvc-release\bin\XQ.exe`; the
    application stayed running for 10 seconds and was then closed.
- Promoted Workbench Data Manager Visibility Actions to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Final Update: Fresh Clone UI Workbench Data Manager Representation Actions

- User requested a fresh local clone before continuing UI changes.
- Preserved the previous worktree's uncommitted representation-action WIP in
  `C:\Users\OCEAN\Desktop\XIAOQUAN\XQ` as stash
  `wip-data-manager-representation-actions-before-fresh-clone`.
- Cloned `xiaoquan1021/XQ` branch `feature/windows-monolith-foundation` to
  `C:\Users\OCEAN\Desktop\XIAOQUAN\XQ-fresh-ui`.
  - Fresh clone started clean at `791bca2`.
- Continued Data Explorer fidelity work in the fresh clone by restoring the
  original representation actions for selected MITK-backed data.
- Red test observed:
  - Extended `test_monolith_main_window_data_panel` to require Surface,
    Wireframe, and Points representation actions and verify their MITK property
    writes.
  - First configure/build in the fresh clone passed.
  - `ctest --test-dir .\build\windows-msvc-release -R test_monolith_main_window_data_panel --output-on-failure --timeout 120`
    failed because representation context actions were not present.
- Implemented Data Manager representation actions:
  - Added stable actions:
    `xqSetDataRepresentationSurfaceAction`,
    `xqSetDataRepresentationWireframeAction`, and
    `xqSetDataRepresentationPointsAction`.
  - Actions enable only when the selected catalog entry has a bound MITK node.
  - Surface/Wireframe/Points write the same `material.representation`,
    `material.wireframe`, and `volumerendering` properties used by the legacy
    Data Explorer.
  - Property edits refresh the Data Manager table and request a MITK render
    update.
- Red/green target verification:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release -R test_monolith_main_window_data_panel --output-on-failure --timeout 120`
    passed: 1/1.
  - Related targeted Data Manager/data tests passed: 6/6.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 75/75.
  - `git diff --check` passed in both `XQ-fresh-ui` and `Externals`.
  - Runtime smoke launched `build\windows-msvc-release\bin\XQ.exe`; the
    application stayed running for 10 seconds and was then closed.
- Promoted Workbench Data Manager Representation Actions to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Final Update: Workbench-Style Monolith Shell Correction

- User review corrected the Presentation direction:
  - The Windows monolith UI looked too different from the original
    XQ/BlueBerry/MITK Workbench.
  - Migration direction is now explicitly "original Workbench-style shell on
    top of monolith internals", not a new generic workflow-first interface.
- Root-cause comparison:
  - Original `xq_DefaultPerspective` uses Data Manager on the left, Image
    Navigator below it, a central editor/render area, and right-side
    placeholder folders for tools/logging.
  - The monolith `MainWindow` had moved workflow navigation into the left
    primary area, making the app feel unrelated to original XQ.
- Runtime fix:
  - Added the MITK-built CTK bin path to `scripts\xq-env.ps1` so
    `CTKWidgets.dll` is available during launch.
  - Added `Start-XQ.cmd`, a double-click launcher that calls
    `scripts\run-xq.ps1 -ExternalsRoot ..\Externals`.
  - Extended `tests\test_windows_env_scripts.ps1` to guard both runtime PATH
    and launcher behavior.
- Presentation correction:
  - `xqRenderHostContainer` is now the `QMainWindow` central widget.
  - `xqDataManagerDock` owns `xqDataHierarchyView` on the left.
  - `xqImageNavigatorDock` is docked below Data Manager as the monolith v1
    placeholder for the original Image Navigator view.
  - `xqWorkflowToolsDock` moves workflow navigation/pages to the right.
  - Restored stable shell anchors `FileMenu` and `mainActionsToolBar` while
    preserving existing import/save/remove action object names.
- Red/green evidence:
  - Added `test_monolith_main_window_workbench_layout` before the UI change.
  - Initial run failed as expected with
    `MITK render host should be the central workbench area`.
  - After implementation, XQ C++ tests passed: 74/74.
- Verification for this iteration:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `powershell -NoProfile -ExecutionPolicy Bypass -File tests\test_windows_env_scripts.ps1`
    passed.
  - Runtime smoke launched `XQ.exe` through `scripts\run-xq.ps1`, confirmed the
    process stayed alive for 10 seconds, and closed it cleanly.
  - `git diff --check` passed.

## Current Run Update: Image Preprocessing Page Infrastructure Wiring

- Completed autonomous research refresh:
  - Rechecked comparable workstation expectations: mature imaging workstations
    keep preprocessing actions tied to actual image data and rendering state,
    not placeholder success messages.
  - Scanned monolith tests for remaining `operation accepted` diagnostics.
    Domain routing tests intentionally keep generic placeholder coverage, but
    `test_monolith_image_preprocessing_operation_page` still accepted
    `Gaussian Smoothing preprocessing operation accepted CTA Image.`
  - Chosen next slice: wire the Image Preprocessing page test to the existing
    dynamic Infrastructure handler and expect deterministic missing-node
    validation.
- Added next executable phase to `plan.md`: Image Preprocessing Page
  Infrastructure Wiring.
- Started the next unattended loop iteration:
  - Adding failing Image Preprocessing operation page expectation first.
- Red test observed:
  - Updated the Image Preprocessing page action expectation to require
    `Run Image Preprocessing failed: Active image node is required for image preprocessing.`
    before registering the Infrastructure handler.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_image_preprocessing_operation_page"`
    failed because the page test still used the Domain placeholder handler.
- Implemented Image Preprocessing page Infrastructure wiring:
  - `test_monolith_image_preprocessing_operation_page` now registers
    `RegisterDynamicImagePreprocessingWorkflowActionHandler`.
  - The test target now links `xqMonolithInfrastructure`, matching its direct
    Infrastructure registrar dependency.
  - The page assertion now covers the missing active MITK image node validation
    path instead of placeholder success.
- Red/green target verification:
  - Initial build after registration failed with an unresolved
    `RegisterDynamicImagePreprocessingWorkflowActionHandler` symbol.
  - Root cause: the page test target linked Presentation/Domain/Core but not
    `xqMonolithInfrastructure`.
  - After adding the target dependency,
    `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_image_preprocessing_operation_page"`
    passed: 1/1.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Image Preprocessing Page Infrastructure Wiring to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Update: 3D Region Growing Infrastructure Action

- Completed autonomous research refresh:
  - Re-scanned remaining non-Domain placeholder and unsupported-operation
    behavior after the page wiring cleanup.
  - Page-level placeholder success has been removed; remaining `operation accepted`
    diagnostics are intentional Domain service/unit coverage.
  - The next high-value native gap is 3D Segmentation `region-growing`: the
    legacy module already provides `xq_Seg3DUtils::RegionGrowingSegmentation`
    and `xq_MitkSeg3D`, while the monolith Infrastructure handler still reports
    unsupported.
- Added next executable phase to `plan.md`: 3D Region Growing Infrastructure
  Action.
- Started the next unattended loop iteration:
  - Adding a failing 3D Region Growing Infrastructure action test first.
- Red test observed:
  - Extended `test_monolith_segmentation_workflow_action_handler` with a real
    synthetic MITK image, region-growing parameters, and expectations for a
    generated `xq_MitkSeg3D` result node.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_segmentation_workflow_action_handler"`
    failed because `region-growing` still returned
    `Region Growing is not wired to a native 3D Segmentation runtime yet.`
- Implemented 3D Region Growing Infrastructure action:
  - The dynamic Segmentation handler now routes `segmentation-3d` /
    `region-growing` to a native path.
  - It resolves the selected MITK image node, reads `seed-x`, `seed-y`, and
    `threshold-upper` parameters, derives the seed value as the lower
    threshold, and calls `xq_Seg3DUtils::RegionGrowingSegmentation`.
  - Successful runs create an `xq_MitkSeg3D` node, stamp
    `segmentation_3d` pipeline metadata, add generated catalog/hierarchy
    entries, select the result, and refresh rendering.
  - Existing 2D unsupported operation and other 3D unsupported operation guards
    remain deterministic.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_segmentation_workflow_action_handler"`
    passed: 1/1.
  - Related integration/page tests first failed because they still expected the
    old unsupported-operation diagnostic for `region-growing`.
  - Updated those tests to expect the new Infrastructure missing-image
    validation when no real MITK image node is bound.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(application_import_wiring|segmentation_operation_page|segmentation_workflow_action_handler)"`
    passed: 3/3.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted 3D Region Growing Infrastructure Action to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Update: Boundary Layers Meshing Infrastructure Action

- Completed autonomous research refresh:
  - Re-scanned remaining native workflow gaps after 3D Region Growing.
  - `boundary-layers` is a good next slice because `xq_MeshPipelineService`
    already accepts boundary-layer request fields and records explicit fallback
    metadata on generated mesh nodes.
  - Chosen next slice: route the Meshing `boundary-layers` operation through
    the existing native mesh pipeline fallback instead of the unsupported guard.
- Added next executable phase to `plan.md`: Boundary Layers Meshing
  Infrastructure Action.
- Started the next unattended loop iteration:
  - Adding a failing Meshing `boundary-layers` Infrastructure action test
    first.
- Red test observed:
  - Extended `test_monolith_meshing_workflow_action_handler` so
    `boundary-layers` uses a real model node and expects a generated mesh with
    boundary-layer metadata.
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_meshing_workflow_action_handler"`
    failed because `boundary-layers` still returned the unsupported-operation
    diagnostic.
- Implemented Boundary Layers Meshing Infrastructure action:
  - The dynamic Meshing handler now routes `boundary-layers` through
    `xq_MeshPipelineService::CreateVolumeMesh`.
  - The handler maps `layer-count` and `growth-rate` into
    `xq_MeshGenerationRequest` boundary-layer fields.
  - The existing mesh pipeline records explicit VTK fallback metadata and
    boundary-layer parameters on the generated mesh node.
  - `generate-surface-mesh` remains a deterministic unsupported operation.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_meshing_workflow_action_handler"`
    passed: 1/1.
  - Related application/page tests first failed because they still expected the
    old unsupported diagnostic for `boundary-layers`.
  - Updated those tests to expect the new Infrastructure missing-model
    validation when no real MITK model node is bound.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(application_import_wiring|modeling_meshing_operation_pages|meshing_workflow_action_handler)"`
    passed: 3/3.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Boundary Layers Meshing Infrastructure Action to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Update: Path Unsupported Operation Guard

- Completed autonomous research refresh:
  - Rechecked the Path workflow after the native `create-centerline` wiring.
  - The remaining Path operations, `edit-control-points` and `smooth-path`,
    were still reporting successful placeholder execution in the configured
    monolith, which would mislead users before interactive path editing and
    smoothing runtimes exist.
  - Chosen next slice: keep `create-centerline` real and guard unsupported
    Path operations with deterministic failure diagnostics.
- Red tests observed:
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed after
    adding the new Path guard tests.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(path_workflow_action_handler|path_operation_page|application_import_wiring)"`
    failed: 0/3, because unsupported Path operations still returned
    placeholder success.
- Implemented Path unsupported-operation guard:
  - `path/create-centerline` continues to call
    `xq_PathPipelineService::CreatePath`.
  - `path/edit-control-points` and `path/smooth-path` now return failure with
    diagnostics naming the selected operation and Path workflow.
  - The Path operation page test now registers the Infrastructure handler so
    UI diagnostics cover production composition instead of Domain placeholder
    behavior.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(path_workflow_action_handler|path_operation_page|application_import_wiring)"`
    passed: 3/3.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Path Unsupported Operation Guard to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Update: Modeling Unsupported Operation Guard

- Completed autonomous research refresh:
  - Rechecked the current monolith workflow pipeline against comparable
    medical-image workstation flow: segmentation/modeling/meshing remain the
    handoff spine before simulation.
  - `modeling/build-solid-model` already creates a real generated model node,
    but `modeling/loft-surface` and `modeling/trim-branches` still report
    successful placeholder execution in the configured monolith.
  - Chosen next slice: keep `build-solid-model` real and guard unsupported
    Modeling operations with deterministic failure diagnostics.
- Red tests observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding the new Modeling guard tests.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(modeling_workflow_action_handler|modeling_meshing_operation_pages|application_import_wiring)"`
    failed: 0/3, because unsupported Modeling operations still returned
    placeholder success.
- Implemented Modeling unsupported-operation guard:
  - `modeling/build-solid-model` continues to call
    `xq_ModelPipelineService::CreateModel`.
  - `modeling/loft-surface` and `modeling/trim-branches` now return failure
    with diagnostics naming the selected operation and Modeling workflow.
  - The Modeling page test now registers the Infrastructure handler so UI
    diagnostics cover production composition instead of Domain placeholder
    behavior.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(modeling_workflow_action_handler|modeling_meshing_operation_pages|application_import_wiring)"`
    passed: 3/3.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Modeling Unsupported Operation Guard to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Update: Meshing Unsupported Operation Guard

- Completed autonomous research refresh:
  - Rechecked the post-modeling workflow handoff. The configured monolith
    already validates and creates generated mesh nodes for
    `meshing/generate-volume-mesh`.
  - `meshing/generate-surface-mesh` and `meshing/boundary-layers` still report
    successful placeholder execution, which can falsely imply mesh artifacts
    exist before those native runtimes are wired.
  - Chosen next slice: keep `generate-volume-mesh` real and guard unsupported
    Meshing operations with deterministic failure diagnostics.
- Red tests observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding the new Meshing guard tests.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(meshing_workflow_action_handler|modeling_meshing_operation_pages|application_import_wiring)"`
    failed: 0/3, because unsupported Meshing operations still returned
    placeholder success.
- Implemented Meshing unsupported-operation guard:
  - `meshing/generate-volume-mesh` continues to call
    `xq_MeshPipelineService::CreateVolumeMesh`.
  - `meshing/generate-surface-mesh` and `meshing/boundary-layers` now return
    failure with diagnostics naming the selected operation and Meshing
    workflow.
  - The Modeling/Meshing page test now registers the Infrastructure Meshing
    handler so UI diagnostics cover production composition instead of Domain
    placeholder behavior.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(meshing_workflow_action_handler|modeling_meshing_operation_pages|application_import_wiring)"`
    passed: 3/3.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Meshing Unsupported Operation Guard to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Update: ROM Unsupported Operation Guard

- Completed autonomous research refresh:
  - Rechecked the simulation side after Flow validation. Flow simulation now
    has concrete Infrastructure behavior for configure, steady solve, and
    result review.
  - `rom-simulation/build-1d-network` already creates a generated
    `xq_MitkROMJob`, but `rom-simulation/run-rom-solver` and
    `rom-simulation/calibrate-boundary-conditions` still report successful
    placeholder execution in the configured monolith.
  - Chosen next slice: keep `build-1d-network` real and guard unsupported ROM
    operations with deterministic failure diagnostics.
- Red tests observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding the new ROM guard tests.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(rom_simulation_workflow_action_handler|simulation_operation_pages|application_import_wiring)"`
    failed: 0/3, because unsupported ROM operations still returned
    placeholder success.
- Implemented ROM unsupported-operation guard:
  - `rom-simulation/build-1d-network` continues to create a generated
    `xq_MitkROMJob`.
  - `rom-simulation/run-rom-solver` and
    `rom-simulation/calibrate-boundary-conditions` now return failure with
    diagnostics naming the selected operation and ROM Simulation workflow.
  - The Simulation operation page test now registers the Infrastructure ROM
    handler so UI diagnostics cover production composition instead of Domain
    placeholder behavior.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(rom_simulation_workflow_action_handler|simulation_operation_pages|application_import_wiring)"`
    passed: 3/3.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted ROM Unsupported Operation Guard to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Update: MultiPhysics Unsupported Operation Guard

- Completed autonomous research refresh:
  - Rechecked the remaining configured monolith Infrastructure placeholders.
  - `multiphysics/configure-coupling` already creates a generated
    `xq_MitkMultiPhysicsJob`, but `multiphysics/run-coupled-solve` and
    `multiphysics/review-coupled-results` still report successful placeholder
    execution.
  - Chosen next slice: keep `configure-coupling` real and guard unsupported
    MultiPhysics operations with deterministic failure diagnostics.
- Red tests observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding the new MultiPhysics guard tests.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(multiphysics_workflow_action_handler|simulation_operation_pages|application_import_wiring)"`
    failed: 0/3, because unsupported MultiPhysics operations still returned
    placeholder success.
- Implemented MultiPhysics unsupported-operation guard:
  - `multiphysics/configure-coupling` continues to create a generated
    `xq_MitkMultiPhysicsJob`.
  - `multiphysics/run-coupled-solve` and
    `multiphysics/review-coupled-results` now return failure with diagnostics
    naming the selected operation and Multi-Physics workflow.
  - The Simulation operation page test now registers the Infrastructure
    MultiPhysics handler so UI diagnostics cover production composition
    instead of Domain placeholder behavior.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(multiphysics_workflow_action_handler|simulation_operation_pages|application_import_wiring)"`
    passed: 3/3.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted MultiPhysics Unsupported Operation Guard to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Update: Infrastructure Placeholder Success Cleanup

- Completed autonomous research refresh:
  - Rechecked the configured monolith Infrastructure handler sources after the
    unsupported-operation guard pass.
  - Public unsupported operations now fail deterministically, but several
    Infrastructure handlers still contain residual successful placeholder text
    such as `operation accepted` and `domain workflow accepted`.
  - Chosen next slice: add a regression scan and remove configured
    Infrastructure placeholder success fallbacks while leaving Domain
    placeholder tests intact.
- Red test observed:
  - Added `tests/test_monolith_infrastructure_no_placeholder_success.ps1`.
  - The test failed and listed residual placeholder success text in Flow,
    Meshing, Modeling, MultiPhysics, Path, Python API, ROM, and Segmentation
    Infrastructure workflow handlers.
- Implemented placeholder success cleanup:
  - Removed unused `RunPlaceholder*Operation` helpers from guarded
    Infrastructure handlers.
  - Converted Flow Simulation and Python API unknown-operation fallbacks to
    deterministic unsupported-operation failures.
  - Left Domain placeholder handlers untouched.
- Red/green target verification:
  - `tests/test_monolith_infrastructure_no_placeholder_success.ps1` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(path_workflow_action_handler|segmentation_workflow_action_handler|modeling_workflow_action_handler|meshing_workflow_action_handler|flow_simulation_workflow_action_handler|rom_simulation_workflow_action_handler|multiphysics_workflow_action_handler|python_api_workflow_action_handler|application_import_wiring)"`
    passed: 9/9.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Infrastructure Placeholder Success Cleanup to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run Update: Flow Simulation Page Infrastructure Wiring

- Completed autonomous research refresh:
  - Rechecked UI tests after removing configured Infrastructure placeholder
    success fallbacks.
  - `test_monolith_simulation_operation_pages` still accepted
    `Run Flow Simulation succeeded: Steady Flow Solve flow simulation operation accepted Aorta Mesh.`,
    which exercises the Domain placeholder instead of the configured
    Infrastructure handler.
  - Chosen next slice: wire the Flow Simulation page test to the existing
    Infrastructure Flow handler and expect deterministic validation
    diagnostics.
- Red test observed:
  - Updated the Flow page expectation to require
    `Run Flow Simulation failed: Active simulation prep node is required for steady flow solve.`
    before registering the Flow Infrastructure handler.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_simulation_operation_pages"`
    failed because the page test still used the Domain placeholder handler.
- Implemented Flow page Infrastructure wiring:
  - `test_monolith_simulation_operation_pages` now registers
    `RegisterDynamicFlowSimulationWorkflowActionHandler`.
  - The Flow page assertion now covers the steady-flow Infrastructure
    validation path instead of a placeholder success diagnostic.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_simulation_operation_pages"`
    passed: 1/1.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 18/18.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Flow Simulation Page Infrastructure Wiring to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run: Python API Availability Infrastructure Action Handler

- Continued the requested unattended loop after the Python API phase was made
  active in `plan.md`.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding `test_monolith_python_api_workflow_action_handler` and extending
    `test_monolith_application_import_wiring`.
  - The first build failed because
    `Infrastructure/xq_PythonApiWorkflowActionHandler.h` did not exist.
  - After adding the handler, build failed once more because the new test used
    `TaskRunner::History()` without including `Core/xq_TaskRunner.h`.
- Implemented Python API availability infrastructure action:
  - Added `RegisterDynamicPythonApiWorkflowActionHandler()`.
  - `python-api/open-python-console` now reports the deterministic version and
    unavailable-runtime diagnostic from `xq_PythonApiService`.
  - `run-project-script` and other unsupported Python API operations keep the
    existing operation-aware placeholder behavior.
  - The monolith composition root registers the Infrastructure handler after
    Domain workflow registration.
  - `xqMonolithInfrastructure` now links `xqModulePythonApi`.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(python_api_workflow_action_handler|application_import_wiring)"`
    passed: 2/2.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Python API Availability Infrastructure Action Handler to completed
  in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run: Python API Snippet Export Infrastructure Action

- Completed autonomous research refresh:
  - After Python API availability landed, `run-project-script` still requires a
    real Python runtime and pybind11 bridge, so implementing it now would fake
    execution.
  - `export-api-snippet` can use the existing C++ inspection service surface
    and the already visible `snippet-count` parameter without requiring Python
    runtime availability.
  - Chosen next slice: deterministic Python API snippet export.
- Added next executable phase to `plan.md`: Python API Snippet Export
  Infrastructure Action.
- Started RED tests first:
  - Extended `test_monolith_python_api_workflow_action_handler` so
    `export-api-snippet` must return deterministic snippet text and honor
    `snippet-count`.
  - Extended `test_monolith_application_import_wiring` so configured monolith
    composition must use Infrastructure snippet behavior.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(python_api_workflow_action_handler|application_import_wiring)"`
    failed because `export-api-snippet` still returned the placeholder
    `Export API Snippet python api operation accepted.`
- Implemented Python API snippet export:
  - Added an Infrastructure snippet catalog for version, list-nodes,
    find-node, upstream resolution, model read, and project save examples.
  - `export-api-snippet` now reads `snippet-count` from
    `WorkflowOperationService` and returns a deterministic limited snippet
    message.
  - `run-project-script` remains on the operation-aware placeholder path.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(python_api_workflow_action_handler|application_import_wiring)"`
    passed: 2/2.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Python API Snippet Export Infrastructure Action to completed in
  `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run: Python API Script Runtime Guard

- Completed autonomous research refresh:
  - Comparable workstation Python surfaces are real automation entry points,
    not successful no-op placeholders.
  - The Windows monolith still builds without pybind11/Python extension ABI, so
    `run-project-script` should fail clearly instead of reporting accepted.
  - Chosen next slice: runtime-unavailable guard for `run-project-script`.
- Added next executable phase to `plan.md`: Python API Script Runtime Guard.
- Started RED tests first:
  - Extended `test_monolith_python_api_workflow_action_handler` so
    `run-project-script` must fail with the Python runtime diagnostic and record
    failed task history.
  - Extended `test_monolith_python_api_operation_page` so the UI posts a failed
    diagnostic instead of a succeeded placeholder.
  - Extended `test_monolith_application_import_wiring` so production
    composition uses the Infrastructure guard.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(python_api_workflow_action_handler|application_import_wiring|python_api_operation_page)"`
    failed because `run-project-script` still returned the placeholder
    `Project Script Runner python api operation accepted.`
- Implemented Python API script runtime guard:
  - Added a `run-project-script` Infrastructure branch that calls
    `xq_PythonApiService::IsAvailable()`.
  - When the runtime is unavailable, the action returns false with the existing
    availability diagnostic and records failed task history.
  - `open-python-console` and `export-api-snippet` behavior remains unchanged.
- Debugging note:
  - The first target rerun showed the new UI test referenced the Infrastructure
    handler without linking `xqMonolithInfrastructure`.
  - Added the missing CMake test target dependencies and kept the test scoped
    to production-like handler registration.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(python_api_workflow_action_handler|application_import_wiring|python_api_operation_page)"`
    passed: 3/3.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Python API Script Runtime Guard to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.

## Current Run: Segmentation Unsupported Operation Guard

- Completed autonomous research refresh:
  - After Python API runtime guards, the remaining risky surface was workflow
    operations that still looked executable but only returned placeholder
    success.
  - Segmentation pages expose Threshold, Loft, Region Growing, and Surface
    Preview operations, but only `segmentation-2d/manual-contour` is wired to a
    native monolith pipeline service.
  - Chosen next slice: fail unsupported Segmentation operations explicitly
    while preserving the real manual-contour path.
- Added next executable phase to `plan.md`: Segmentation Unsupported Operation
  Guard.
- Started RED tests first:
  - Extended `test_monolith_segmentation_workflow_action_handler` for
    unsupported 2D and 3D Segmentation diagnostics.
  - Extended `test_monolith_segmentation_operation_page` to run with the
    Infrastructure handler and expect failed diagnostics for unsupported
    operations.
  - Extended `test_monolith_application_import_wiring` so configured monolith
    composition must use the unsupported-operation guard.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - Initial build exposed missing test wiring: the UI test needed the dynamic
    handler signature and `xqMonolithInfrastructure` link dependency.
  - After fixing test wiring, target CTest failed because unsupported
    Segmentation operations still returned placeholder success.
- Implemented Segmentation unsupported-operation guard:
  - Added a deterministic Infrastructure failure path naming the selected
    operation and workflow title.
  - Kept `segmentation-2d/manual-contour` on the existing
    `xq_SegmentationPipelineService::CreateContourGroup` path.
  - Kept Domain placeholder routing unchanged for generic Domain tests.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(segmentation_workflow_action_handler|segmentation_operation_page|application_import_wiring)"`
    passed: 3/3.
- Final verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 73/73.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Segmentation Unsupported Operation Guard to completed in `plan.md`.
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

## Current Run: Meshing Infrastructure Action Handler

- Completed autonomous research refresh:
  - Rechecked the connected Image -> Path -> 2D Segmentation -> Modeling chain
    against the existing mesh pipeline contract.
  - XQ already has `xq_MeshPipelineService::CreateVolumeMesh`, including the
    current VTK Delaunay3D fallback capability metadata, but monolith Meshing
    still reports operation-aware placeholder acceptance.
  - Chosen next slice: route `generate-volume-mesh` through the existing Mesh
    pipeline and commit the generated Mesh into monolith
    catalog/hierarchy/selection/rendering state.
- Added next executable phase to `plan.md`: Meshing Infrastructure Action
  Handler.
- Started the next unattended loop iteration:
  - Adding failing Meshing workflow action handler and composition-root tests
    first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding the Meshing handler test target.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed because
    `Infrastructure/xq_MeshingWorkflowActionHandler.h` did not exist.
- Implemented Meshing infrastructure action handling:
  - Added a dynamic Meshing workflow action handler for
    `generate-volume-mesh`.
  - The handler resolves the selected Model node from the data-node registry or
    active node and validates the Model pipeline stage before meshing.
  - It passes edge-size and optimization parameter state into
    `xq_MeshPipelineService::CreateVolumeMesh`.
  - Successful mesh generation now registers a Mesh catalog entry, adds the
    Meshes hierarchy node, binds the generated MITK grid node, selects it, and
    refreshes rendering.
  - The monolith composition root installs the Meshing infrastructure handler
    after Domain workflow registration, overriding placeholder execution for
    the supported operation.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(meshing_workflow_action_handler|application_import_wiring|domain_workflow_action_handlers|modeling_meshing_operation_pages|workflow_context_service)"`
    passed: 5/5.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 69/69.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Meshing Infrastructure Action Handler to completed in `plan.md`.
- Started Active Phase: Autonomous Research Refresh.
- Completed autonomous research refresh:
  - Rechecked the post-meshing workflow chain against local Simulation module
    contracts and comparable SimVascular-style vascular modeling workflows.
  - XQ already has `xq_SimulationPrepPipelineService` for creating
    SimulationPrep jobs from Model + VolumeMesh nodes, plus solver export/run
    plumbing, while monolith Flow Simulation still reports operation-aware
    placeholder acceptance.
  - Chosen next slice: route `configure-cfd-job` through the existing
    SimulationPrep pipeline and commit the generated solver job into monolith
    catalog/hierarchy/selection/rendering state.
- Added next executable phase to `plan.md`: Flow Simulation Prep
  Infrastructure Action Handler.
- Started the next unattended loop iteration:
  - Adding failing Flow Simulation workflow action handler and composition-root
    tests first.
- Red test observed:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed after
    adding the Flow Simulation handler test target.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` failed because
    `Infrastructure/xq_FlowSimulationWorkflowActionHandler.h` did not exist.
- Implemented Flow Simulation Prep infrastructure action handling:
  - Added a dynamic Flow Simulation workflow action handler for
    `configure-cfd-job`.
  - Added a first-class monolith `SimulationPrep` data workflow role with
    project persistence, import folder, context compatibility, and UI display
    mapping.
  - The handler resolves the selected VolumeMesh node, resolves its upstream
    Model node from pipeline metadata/DataStorage, and calls
    `xq_SimulationPrepPipelineService::CreateOrUpdateSimulationPrep`.
  - Successful simulation prep now registers a SimulationPrep catalog entry,
    adds the Simulations hierarchy node, binds the generated MITK solver job
    node, selects it, and refreshes rendering.
  - Unsupported Flow Simulation operations stay on the existing
    operation-aware placeholder path for this slice.
- Red/green target verification:
  - After implementation, `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals`
    passed.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120 -R "test_monolith_(flow_simulation_workflow_action_handler|application_import_wiring|workflow_context_service|project_data_persistence|data_import_service|simulation_operation_pages)"`
    passed: 6/6.
- Verification for this iteration:
  - `scripts\build-xq.ps1 configure -ExternalsRoot ..\Externals` passed.
  - `scripts\build-xq.ps1 build -ExternalsRoot ..\Externals` passed.
  - All XQ PowerShell tests in `tests\*.ps1` passed: 17/17.
  - All Externals PowerShell tests in `tests\*.ps1` passed: 30/30.
  - `ctest --test-dir .\build\windows-msvc-release --output-on-failure --timeout 120`
    passed: 70/70.
  - `git diff --check` passed in both `XQ` and `Externals`.
- Promoted Flow Simulation Prep Infrastructure Action Handler to completed in
  `plan.md`.
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
