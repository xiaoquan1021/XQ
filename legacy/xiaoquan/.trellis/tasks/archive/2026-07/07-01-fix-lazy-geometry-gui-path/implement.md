# Implementation Plan: Lazy Geometry GUI Rendering Path

## Step 1: Red Tests

- Extend the app/window tests to build a small native project containing a surface and volume mesh, save it, load it lazily, and attach the loaded scene to `XQMainWindow`.
- Drive the real tree selection path:
  - call `showImage()` first so the central stack is on the image label;
  - select the lazy surface row and assert the central stack switches to `xqRenderWidget`;
  - select the lazy mesh row and assert the central stack switches to `xqRenderWidget`.
- Extend startup tests to prove path startup uses lazy geometry and records the asset root.
- Run the new/modified test before production code changes; it must fail because the window has no lazy-resource attachment API and selection ignores `geometryAssetId`.

## Step 2: Minimal Implementation

- Add asset-root state to `XQAppStartupState`.
- Load native project paths with `XQProjectReadOptions{.lazyGeometry = true}`.
- Derive `<project-stem>.assets` in the app startup layer and store it.
- Add `XQMainWindow::attachGeometryResources(...)` to construct a `GeometryResourceManager`.
- Update `attachMainWindowWorkflow()` to attach geometry resources when a startup state has a non-empty asset root.
- Update `onSceneSelectionChanged()`:
  - keep resident surface/mesh branches first;
  - if resident geometry is absent, resolve lazy source and call the appropriate progressive renderer method;
  - do nothing when resolution fails.

## Step 3: Validation

Targeted build:

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"" --build XQ\build_m9be --target test_main_window test_app_startup test_geometry_source_resolver test_scene_renderer_progressive --config Release"
```

Targeted CTest:

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && set QT_QPA_PLATFORM=offscreen&& ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"" --test-dir XQ\build_m9be -C Release --output-on-failure -R ""test_main_window|test_app_startup|test_geometry_source_resolver|test_scene_renderer_progressive"""
```

Final gate:

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && set QT_QPA_PLATFORM=offscreen&& ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"" --test-dir XQ\build_m9be -C Release --output-on-failure"
```

## Rollback Points

- If `XQMainWindow` ownership becomes too broad, keep the tests and narrow the API to attach an already-owned `GeometryResourceManager`.
- If lazy startup breaks existing app tests, preserve no-argument behavior and limit lazy loading to project-path startup.
- Do not alter reader schema or blob resolver policy in this task.
