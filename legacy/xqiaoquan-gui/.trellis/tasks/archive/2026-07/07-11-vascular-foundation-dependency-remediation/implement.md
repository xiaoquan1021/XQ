# Implementation Plan: 依赖生产基线整改

## Steps

1. Snapshot XQ/Externals git state、旧 Qt hash/imports、当前 broad ITK link line和新隔离目录不存在的证据。
2. 运行 Externals Qt recipe tests，并核对当前 dirty diff 不改变 Qt build branch。
3. 使用 `windows-x64-vascular` / Release / bounded parallel jobs 构建并安装 Qt 6.7.0；不覆盖旧 prefix。
4. 验证新 Qt cache、package version、DLL hashes 和递归 imports；若仍有 zstd/host ingress，停止 XQ 集成并先定位 recipe/build cache。
5. 读取 `trellis-before-dev` 上下文后修改 XQ CMake：精确版本、显式 ITK components、target-specific module closures、冻结/恢复显式 VTK set、移除 `ITK_USE_FILE`/`${ITK_LIBRARIES}`、修正 TetGen 1.5 并加入 research acknowledgement gate。
6. 更新审计构建脚本/测试，使 ON/ON research build 显式 acknowledgement，并增加 package/link/runtime boundary regression。
7. 在新的 `XQ/build_dependency_remediation` Release tree 中显式指定新 Qt 和锁定 VTK/ITK/GDCM/tinyxml2；先 focused build/tests，再全量 build/CTest。
8. 从 build graph 和 PE graph 两侧扫描 forbidden dependencies；运行错误 package path、TetGen 未 acknowledgement 等负向 configure 用例。
9. 写 `research/remediation-report.md`，同步必要 code-spec，运行 `trellis-check` 最终复核。

## Primary Commands

```powershell
# Externals recipe regression
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tests/test_qt_windows_no_host_zstd.ps1

# Isolated Qt build; existing windows-x64 install is untouched
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File build_all.ps1 `
  -Profile xq `
  -Target Qt `
  -Platform windows-x64-vascular `
  -BuildType Release `
  -Jobs 8

# XQ clean dependency-baseline configure (exact paths frozen in report/script)
cmake --fresh -S XQ -B XQ/build_dependency_remediation -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DQt6_DIR=<windows-x64-vascular/qt-6.7.0/lib/cmake/Qt6> `
  -DVTK_DIR=<windows-x64/vtk-9.3.0/lib/cmake/vtk-9.3> `
  -DITK_DIR=<windows-x64/itk-5.4.0/lib/cmake/ITK-5.4> `
  -Dtinyxml2_DIR=<windows-x64/tinyxml2-8.0.0/lib/cmake/tinyxml2> `
  -DXQ_TEST_DATA_ROOT=<0007_H_AO_H> `
  -DXQ_DICOM_TEST_DATA_ROOT=
cmake --build XQ/build_dependency_remediation --config Release
ctest --test-dir XQ/build_dependency_remediation -C Release --output-on-failure `
  -R "itk|dicom|arch_boundaries|app_startup|main_window"
ctest --test-dir XQ/build_dependency_remediation -C Release --output-on-failure
```

The final report records the actual resolved command paths and exit codes. Full suites from separate build trees run sequentially.

## Files/Areas at Risk

- `XQ/CMakeLists.txt`
- `XQ/probes/vascular_foundation/run_mesh_on_audit.bat`
- dependency-baseline validation scripts/tests under `XQ/probes/vascular_foundation/` or `XQ/tests/cmake/`
- `.trellis/spec/XQ/architecture/external-libs.md` only if implementation reveals a new reusable contract
- new remediation research report

Externals source files are read-only for this child; only new `build/windows-x64-vascular` and `install/windows-x64-vascular` outputs are produced there.

## Stop / Failure Rules

- If either isolated Qt directory already exists unexpectedly, inspect before using; never delete or overwrite it blindly.
- If the Qt recipe test passes but new Qt still imports zstd, do not copy a DLL workaround and do not continue to product acceptance.
- If explicit ITK imported targets do not provide required usage requirements, determine the smallest additional documented target; do not restore `${ITK_LIBRARIES}`.
- If exact package configuration falls back to a host path, fail the build and tighten explicit path validation.
- Do not mark TetGen production-approved; acknowledgement only enables the recorded research build.
- Preserve all unrelated dirty/untracked files and existing install/build trees.

## Completion Record

- Exact source/recipe/package identities and hashes.
- Old-vs-new Qt import comparison.
- Exact ITK component set and per-target link mapping.
- Link graph forbidden-pattern scan.
- Recursive PE closure and unresolved DLL count.
- Negative configure matrix.
- Focused/full CTest totals.
- Explicit remaining limits: TetGen license, source-only backend integration, real CTA algorithms/data gate.
