# Implementation Plan

## Red

1. Add `XQ/tests/cmake/check_portable_test_data_root.cmake`.
2. Register `test_portable_test_data_root` with CTest.
3. Build/regenerate and run only `test_portable_test_data_root`.
4. Confirm it fails because `XQ/CMakeLists.txt` still hard-codes the local
   `0007_H_AO_H` path.

## Green

1. Add `XQ_TEST_DATA_ROOT` cache path to `XQ/CMakeLists.txt`.
2. Normalize it to an absolute CMake-style path.
3. Derive all real-data test subpaths from that root.
4. Replace every affected compile definition with the derived variables.
5. Re-run `test_portable_test_data_root` and confirm it passes.

## Targeted Verification

Run the portability test and the real-data tests that consume the macros:

- `test_portable_test_data_root`
- `test_modeling_integration`
- `test_meshing_integration`
- `test_flow_integration`
- `test_ai_integration`
- `test_workflow_integration`
- `test_segmentation_integration`
- `test_vtk_image_adapter`
- `test_mdl_reader`
- `test_msh_reader`
- `test_pth_reader`
- `test_ctgr_reader`
- `test_svproject_reader`

Also search `XQ/CMakeLists.txt`, `XQ/tests`, and `XQ/src` for the removed
machine-specific path.

## Full Verification

Build Release and run full Release `ctest --output-on-failure` with
`QT_QPA_PLATFORM=offscreen`.

## Rollback Point

If the build system fails to regenerate or any real-data test receives a wrong
path, revert only this task's CMake/test-script edits and keep previously
archived audit-fix changes untouched.
