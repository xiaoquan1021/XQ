# Implementation Plan

## Scope

Editable scope:
- `XQ/src/core/asset/AssetRegistry.h`
- `XQ/src/core/asset/AssetRegistry.cpp`
- `XQ/src/core/XQProject.cpp`
- `XQ/tests/core/test_asset_registry.cpp`
- `XQ/tests/core/test_project_lifecycle.cpp`
- `.trellis/spec/XQ/core/command-and-scene.md`

## Steps

1. Write failing tests first:
   - `test_project_lifecycle`: closing an open project clears asset records and relations, and reopen does not restore old asset ids.
   - `test_asset_registry`: `AssetRegistry::clear()` clears records and relations but keeps id allocation monotonic.
2. Build/run targeted tests and confirm RED for the expected missing cleanup/API.
3. Add `AssetRegistry::clear()`.
4. Call `assetRegistry_.clear()` inside `XQProject::close()`.
5. Re-run targeted tests until green.
6. Update core lifecycle spec with the close/reset contract.
7. Run full Release build/test.

## Validation Commands

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"" --build XQ\build_m9be --target test_project_lifecycle test_asset_registry test_project_roundtrip test_project_lazy_geometry --config Release"
```

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && set QT_QPA_PLATFORM=offscreen&& ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"" --test-dir XQ\build_m9be -C Release --output-on-failure -R ""test_project_lifecycle|test_asset_registry|test_project_roundtrip|test_project_lazy_geometry"""
```

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && set QT_QPA_PLATFORM=offscreen&& ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"" --test-dir XQ\build_m9be -C Release --output-on-failure"
```

## Rollback Point

If core cleanup breaks IO roundtrip expectations, stop and inspect whether a caller is relying on closed projects retaining assets. Do not preserve stale assets silently; either adjust the caller to save/load before close or document a separate export lifecycle.
