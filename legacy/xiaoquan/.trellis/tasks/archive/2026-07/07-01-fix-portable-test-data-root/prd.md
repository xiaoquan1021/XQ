# Fix Portable Test Data Root

## Goal

Remove developer-machine absolute paths from XQ test build configuration. Tests
that need the real `0007_H_AO_H` sample data must derive their paths from a
configurable CMake data root instead of hard-coding
`C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H`.

## Problem

`XQ/CMakeLists.txt` currently embeds one local checkout path in compile
definitions for multiple tests. A different checkout location, drive letter, or
developer account will compile tests with stale paths, causing real-data tests to
fail before exercising the code under test.

## Requirements

- Expose one CMake cache variable for the real sample data root.
- Default that variable to the repository-local `../0007_H_AO_H` location used
  by the current workspace.
- Derive all real-data compile definitions from that variable:
  `XQ_CTGR_DIR`, `XQ_FLOW_DIR`, `XQ_TEST_VTI_PATH`, `XQ_MODELS_DIR`,
  `XQ_MESHES_DIR`, `XQ_PTH_DIR`, and `XQ_SVPROJECT_DIR`.
- Preserve existing test behavior and data file names.
- Add a regression test that fails if the developer-machine sample-data path is
  reintroduced into `XQ/CMakeLists.txt`.
- Do not change production source loading semantics.

## Constraints

- Keep the change in CMake/test configuration scope.
- Do not add fallback paths that silently mask a wrong `XQ_TEST_DATA_ROOT`.
- Do not remove or weaken real-data integration tests.
- Keep Windows path handling compatible with MSVC/Ninja and CTest.

## Acceptance Criteria

- [ ] `XQ/CMakeLists.txt` contains no hard-coded
      `C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H` test data paths.
- [ ] A build can override the sample data location with
      `-DXQ_TEST_DATA_ROOT=<path>`.
- [ ] The affected real-data tests still receive the same existing subpaths from
      the configured root.
- [ ] The new portability regression test fails before the fix and passes after
      the fix.
- [ ] Targeted real-data tests and full Release `ctest` pass.
