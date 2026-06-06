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

## Active Phase: Monolith Workflow Foundation

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
