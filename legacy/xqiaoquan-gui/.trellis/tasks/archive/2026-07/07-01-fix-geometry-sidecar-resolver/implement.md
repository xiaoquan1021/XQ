# Implementation Plan

## Scope

Editable scope:
- `XQ/src/services/resource/GeometrySourceResolver.h`
- `XQ/src/services/resource/GeometrySourceResolver.cpp`
- `XQ/src/services/resource/GeometryResourceManager.h`
- `XQ/src/io/source/MappedGeometrySource.h`
- `XQ/tests/services/resource/test_geometry_source_resolver.cpp`
- `.trellis/spec/XQ/core/source-interface.md`

## Steps

1. Write failing resolver tests first:
   - Writer-produced lazy project resolves with `SegmentedMerkle`.
   - Same project with `.merkle` files removed falls back to `FullVerify`.
2. Build/run `test_geometry_source_resolver` and confirm RED.
3. Implement segmented-first/fallback logic in `resolveLazyGeometrySource`.
4. Update stale comments in resolver/manager/mapped source headers.
5. Run targeted tests.
6. Update source-interface spec with the resolver strategy.
7. Run full Release build and full Release `ctest`.

## Validation Commands

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"" --build XQ\build_m9be --target test_geometry_source_resolver test_geometry_resource_manager test_payload_roundtrip test_project_lazy_geometry --config Release"
```

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && set QT_QPA_PLATFORM=offscreen&& ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"" --test-dir XQ\build_m9be -C Release --output-on-failure -R ""test_geometry_source_resolver|test_geometry_resource_manager|test_payload_roundtrip|test_project_lazy_geometry"""
```

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && set QT_QPA_PLATFORM=offscreen&& ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"" --test-dir XQ\build_m9be -C Release --output-on-failure"
```

## Rollback Point

If segmented-first makes lazy geometry fail for projects without sidecars, stop and add a FullVerify fallback rather than reverting to FullVerify-only behavior.
