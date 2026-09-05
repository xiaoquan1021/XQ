# Implementation Plan: Real App Entry And Workflow Wiring

## Step 1: Red Tests

- Add a startup-focused app test that does not enter `QApplication::exec()`.
- Cover three behaviors first:
  - no project path creates an open empty project and attaches workflow;
  - native `.xqproj` path loads real nodes and attaches workflow;
  - invalid project path returns failure and does not create a fake demo scene.
- Add the test target and run it before production code changes. It must fail against the current `main.cpp`-only implementation because no startup helper exists and the current entry seeds demo nodes.

## Step 2: Minimal Implementation

- Add the app startup helper in `XQ/src/app/`.
- Move startup state creation out of `main.cpp`.
- Replace hard-coded demo inserts in `main.cpp` with:
  - parse startup args;
  - initialize startup state;
  - construct `XQMainWindow`;
  - `attachWorkflow`;
  - show and execute the Qt event loop.
- On startup initialization failure, return a non-zero code without showing the window.

## Step 3: CMake Wiring

- Compile the startup helper into a target reusable by `xq_app` and the new test.
- Link the minimal required dependencies:
  - app startup helper: `xq_app_shell`, `xq_io`;
  - test target: startup helper and Qt test environment if the test constructs `XQMainWindow`.
- Keep `xq_app_shell` free of direct `xq_io` dependency unless the implementation proves there is no smaller boundary.

## Step 4: Green And Regression

Targeted validation:

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"" --build XQ\build_m9be --target xq_app test_app_startup test_main_window test_project_roundtrip --config Release"
```

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && set QT_QPA_PLATFORM=offscreen&& ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"" --test-dir XQ\build_m9be -C Release --output-on-failure -R ""test_app_startup|test_main_window|test_project_roundtrip"""
```

Final child-task validation:

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && set QT_QPA_PLATFORM=offscreen&& ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"" --test-dir XQ\build_m9be -C Release --output-on-failure"
```

## Rollback Points

- If startup helper boundaries become too broad, keep the tests and reduce the helper to `initializeAppStartup` plus `attachMainWindowWorkflow`.
- If linking `xq_io` into the executable causes target-order issues, introduce `xq_app_startup` after `xq_io` is declared and link `xq_app` there.
- Do not revert unrelated dirty changes from earlier audit-fix child tasks.
