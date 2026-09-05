# Implementation Plan: fix-renderer-connectivity-validation

## Steps

1. Add RED regressions
   - In `test_scene_renderer.cpp`, add one-shot invalid surface and invalid tet
     handle cases. Assert `ok=false` and `actorCount==0`.
   - In `test_scene_renderer_progressive.cpp`, add invalid `IGeometrySource`
     fixtures for surface and tet progressive paths. Assert `ok=false`,
     `actorCount==0`, `chunkCount==0`, `completedChunkCount==0`, and callback
     count remains 0.
   - Run the two target tests and confirm they fail against the old behavior.

2. Implement renderer validation
   - Add `connectivity_in_bounds()` in the anonymous namespace of
     `XQSceneRenderer.cpp`.
   - Make `build_surface()` and `build_volume()` return `nullptr` when
     connectivity is invalid.
   - Make `addSurface()` / `addVolumeMesh()` return failure stats when the build
     helper returns `nullptr`.
   - Make `addSurfaceProgressive()` and `addVolumeMeshProgressive()` validate
     full connectivity before planning/creating chunks.

3. Validate targeted behavior
   - Build targets:
     `test_scene_renderer`, `test_scene_renderer_progressive`,
     `test_surface_lod`, `test_chunk_plan`.
   - Run targeted ctest:
     `ctest --test-dir XQ/build_m9be -C Release --output-on-failure -R "scene_renderer|surface_lod|chunk_plan"`.

4. Final validation
   - Run full Release `ctest`.
   - Update `.trellis/spec/XQ/visualization/lod-and-upload.md` with the
     renderer connectivity gate contract.

## Rollback Points

- If valid LOD/progressive tests regress, inspect whether validation is using
  `meta.pointCount` while the acquired point span has a different size. The
  renderer should validate against the acquired point span used for upload.
- If VTK returns empty output for invalid one-shot tests, assert only renderer
  `RenderStats` and actor counts; do not rely on crash/no-crash as the test
  oracle.

## Validation Commands

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"" --build XQ\build_m9be --target test_scene_renderer test_scene_renderer_progressive test_surface_lod test_chunk_plan --config Release"
```

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && set QT_QPA_PLATFORM=offscreen&& ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"" --test-dir XQ\build_m9be -C Release --output-on-failure -R ""scene_renderer|surface_lod|chunk_plan"""
```

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && set QT_QPA_PLATFORM=offscreen&& ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"" --test-dir XQ\build_m9be -C Release --output-on-failure"
```
