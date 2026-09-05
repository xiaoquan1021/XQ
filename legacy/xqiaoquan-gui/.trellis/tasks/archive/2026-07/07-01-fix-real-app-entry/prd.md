# Fix Real App Entry And Workflow Wiring

## Goal

Make `xq_app` start from real project state instead of a hard-coded demo scene, and ensure the main window is wired to a live `XQCommandStack` workflow at startup.

This child task fixes the confirmed P1 audit defect recorded in the parent audit: `XQ/src/app/main.cpp` currently inserts demo nodes and constructs `XQMainWindow` with only a const scene pointer, so the shipped app cannot open a real native project file and does not expose the workflow controller/undo path that `XQMainWindow::attachWorkflow()` already implements.

## Confirmed Facts

- `XQ/src/app/main.cpp:14-19` opens an empty `XQProject`, inserts three demo nodes (`Demo Volume`, `Demo Centerline`, `Demo Surface`), and constructs `XQMainWindow window(&project.scene())`.
- `XQ/src/app/main.cpp` does not call `XQProjectReader::load`, does not create an `XQCommandStack`, and does not call `XQMainWindow::attachWorkflow`.
- `XQ/src/app/XQMainWindow.h:47` exposes `attachWorkflow(XQScene*, XQCommandStack*)`; `XQ/src/app/XQMainWindow.cpp:143` wires controllers, the stage panel, and undo/redo.
- `XQ/tests/app/test_main_window.cpp:126-146` tests manual `attachWorkflow`, but no test proves the actual app startup path uses it.
- `XQ/src/io/project/XQProjectReader.h:35-40` provides native `.xqproj` loading; loaded projects are opened by the reader in `XQ/src/io/project/XQProjectReader.cpp:2717`.
- `XQ/CMakeLists.txt:187` links `xq_app` only to `xq_app_shell`; `xq_io` is defined later at `XQ/CMakeLists.txt:194`. If startup uses `XQProjectReader`, the app target must link the required IO dependency without broadening `xq_app_shell`.

## Requirements

### R1. Remove startup demo data

The production `xq_app` entry must not seed hard-coded demo nodes. With no project path argument, it should open an empty `XQProject` and attach workflow to that empty scene.

### R2. Load a real native project from startup arguments

If a project path is provided as the first positional argument, startup must load it with `XQProjectReader::load`. On success, the window must display the loaded scene, not an empty or demo scene.

### R3. Fail closed on project load errors

If a project path is provided but `XQProjectReader::load` returns a non-Ok status, startup must fail before showing a misleading window. The process should return a non-zero exit code and must not silently fall back to demo or empty data.

### R4. Always wire the workflow stack for displayed project state

Both empty startup and loaded-project startup must create an `XQCommandStack` and call `XQMainWindow::attachWorkflow(&project.scene(), &stack)` so stage controllers and undo/redo operate on the same scene shown in the project tree.

### R5. Preserve layer boundaries

The app entry may depend on `xq_io` to read native projects, but `xq_app_shell` must remain a reusable UI shell without taking a new persistent dependency on native project IO. Core, services, and controllers must not gain Qt dependencies.

## Acceptance Criteria

- [ ] AC1: A startup test fails against the current code because app initialization with no project path still seeds three demo nodes and does not attach workflow.
- [ ] AC2: A startup test proves no-argument initialization opens an empty project and attaches workflow with a non-null command stack/controller path.
- [ ] AC3: A startup test writes a native `.xqproj`, initializes startup with that path, and proves the loaded scene nodes are displayed/available without demo nodes.
- [ ] AC4: A startup test passes an invalid path and proves initialization returns a load-failure status without opening a fake project.
- [ ] AC5: `xq_app` builds after linking the minimal IO dependency needed by the entry point.
- [ ] AC6: `test_main_window`, the new startup test, related project reader/writer tests, and full Release `ctest` pass.

## Out of Scope

- Adding full GUI menu/file-dialog project opening.
- Importing SimVascular project directories at app startup.
- Persisting command stack across project reloads.
- Lazy geometry rendering from `geometryAssetId`; that is covered by `07-01-fix-lazy-geometry-gui-path`.
- Project close/reset behavior; that is covered by `07-01-fix-project-close-state-reset`.

## Open Questions

None blocking. The first positional argument is the minimal native project startup contract for this defect.
