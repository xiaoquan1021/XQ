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

## Active Phase: Monolith Task Runner Foundation

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
