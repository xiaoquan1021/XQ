# Design

## Scope

This task changes only XQ test build configuration. Runtime project IO,
services, adapters, and app behavior remain unchanged.

## CMake Data Root

Add a cache path variable near the top of `XQ/CMakeLists.txt`:

```cmake
set(XQ_TEST_DATA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../0007_H_AO_H"
    CACHE PATH "Root directory for XQ real sample test data")
```

Normalize it to an absolute CMake-style path before deriving compile
definitions:

```cmake
get_filename_component(_xq_test_data_root "${XQ_TEST_DATA_ROOT}" ABSOLUTE)
file(TO_CMAKE_PATH "${_xq_test_data_root}" _xq_test_data_root)
```

Then derive one internal variable per existing subpath:

- `_xq_ctgr_dir = ${_xq_test_data_root}/Segmentations`
- `_xq_flow_dir = ${_xq_test_data_root}/flow-files`
- `_xq_vti_path = ${_xq_test_data_root}/Images/OSMSC0090-cm.vti`
- `_xq_models_dir = ${_xq_test_data_root}/Models`
- `_xq_meshes_dir = ${_xq_test_data_root}/Meshes`
- `_xq_pth_dir = ${_xq_test_data_root}/Paths`
- `_xq_svproject_dir = ${_xq_test_data_root}`

Each existing `target_compile_definitions` entry keeps the same macro name and
only swaps the literal path for the derived variable.

## Regression Test

Add a CMake script test that reads `XQ/CMakeLists.txt` and enforces two
contracts:

- the local developer-machine sample-data path is absent;
- the `XQ_TEST_DATA_ROOT` cache variable is present.

The forbidden string is built in pieces inside the test script so repository
searches for the exact path do not report the test implementation itself as a
false positive.

## Failure Behavior

No automatic fallback is added. If a developer configures an incorrect
`XQ_TEST_DATA_ROOT`, affected tests should fail with the real missing data error
instead of silently switching to another location.
