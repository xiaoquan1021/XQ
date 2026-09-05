# Implementation Plan: fix-stale-propagation

## Steps

1. Add RED regressions in `test_scene_relations`
   - Chain: `A -> B -> C`, `mark_source_changed(A)` returns `2` and marks `B`
     and `C`.
   - Diamond: `A -> B`, `A -> C`, `B -> D`, `C -> D`, one call returns `3`.
   - Repeated transitive mark returns `0`.
   - Optional cycle guard if the existing API permits creating `A -> B -> A`.

2. Implement transitive propagation in `XQScene::mark_source_changed`
   - Use iterative worklist and `std::set<NodeId> visited`.
   - Count only newly stale nodes.
   - Skip initial source.
   - Do not create stale entries for missing nodes.

3. Targeted validation
   - Build `test_scene_relations`, `test_project_roundtrip`,
     `test_project_versioned_save`, `test_scene_model`.
   - Run targeted ctest:
     `ctest --test-dir XQ/build_m9be -C Release --output-on-failure -R "scene_relations|project_roundtrip|project_versioned_save|scene_model"`.

4. Final validation
   - Run full Release `ctest`.
   - Update `.trellis/spec/XQ/core/command-and-scene.md` with the transitive
     stale propagation contract.

## Rollback Points

- If project roundtrip expectations change, verify whether tests previously
  called `mark_source_changed()` on every layer to compensate for the missing
  transitive behavior. The public contract should become one upstream call
  stales all downstream nodes.

## Validation Commands

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"" --build XQ\build_m9be --target test_scene_relations test_project_roundtrip test_project_versioned_save test_scene_model --config Release"
```

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && set QT_QPA_PLATFORM=offscreen&& ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"" --test-dir XQ\build_m9be -C Release --output-on-failure -R ""scene_relations|project_roundtrip|project_versioned_save|scene_model"""
```

```bat
cmd.exe /c "call ""C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat"" >nul && set QT_QPA_PLATFORM=offscreen&& ""C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"" --test-dir XQ\build_m9be -C Release --output-on-failure"
```
