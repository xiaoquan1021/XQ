# Canonical Shell A implementation and automated evidence

Date: 2026-07-12

Status: **implementation complete; automated evidence collected; manual GUI
acceptance pending**.

This report does not close A1, A15, the parent task, or Shell A. The user will
perform the final real-machine review.

## Implemented boundary

- Versioned public entries:
  - `XQ/build_gui_wt.bat`
  - `XQ/build_shell_noflow_wt.bat`
  - `XQ/run_xq.bat`
- Shared implementation:
  - `XQ/probes/vascular_foundation/canonical_shell_env.bat`
  - `XQ/probes/vascular_foundation/configure_canonical_shell.bat`
  - `XQ/verify_canonical_shell_a.bat`
  - `XQ/probes/vascular_foundation/CANONICAL_SHELL_A.md`
- Regression/audit changes:
  - `.gitignore` explicitly versions the three public wrappers.
  - `XQ/tests/cmake/check_dependency_baseline.cmake` guards source contracts.
  - `check_dependency_baseline.ps1` now verifies six exact prefix entries,
    registry disablement, ITK-embedded GDCM/HDF5 paths and version metadata.
  - `AGENTS.md` forbids Claude for this project.

## Review findings fixed before the evidence run

1. LF batch files could not reliably resolve `call :label`; canonical scripts
   now use straight-line fail-fast logic without batch labels.
2. Shared env validation initially made GUI startup depend on Python, test data,
   old Qt and build tools. Configure and runtime preflight are now separated;
   launch clears Python/Conda/configure-only variables.
3. GDCM/HDF5 directory names alone were not identity proof. The checker now
   compares the ITK module metadata with the declared roots and reads exact
   3.0.10/1.14.3 version files.
4. `cmake --fresh` alone retained old objects. Canonical builds now use
   `--clean-first`.
5. Caller environment prefix/registries could widen discovery invisibly.
   Configure clears environment prefix paths and disables both package
   registries; cache checker verifies the result.
6. Generic `.gitignore` rules excluded the formal wrappers. Narrow exceptions
   now keep only the three canonical entries versioned.

## Automated evidence command

```bat
cmd.exe /d /c XQ\verify_canonical_shell_a.bat
```

Exit: `0`.

Evidence directory:

```text
.trellis/workspace/ocean/shell-a-canonical-logs/20260712-023910
```

Results:

| Evidence | Result |
| --- | --- |
| Source contract | PASS-EVIDENCE |
| Flow ON clean-first build | 306/306 targets |
| Flow ON dependency graph / recursive PE | clean / 92 binaries |
| Flow ON focused | 8/8 |
| Flow ON full | 93/93 |
| Flow OFF clean-first build | 280/280 targets |
| Flow OFF dependency graph / recursive PE | clean / 92 binaries |
| Flow OFF focused | 8/8 |
| Flow OFF full | 84/84 |
| Negative package/version matrix | 7/7 |

Both caches record:

- Release, isolated QtBase 6.7.0, exact VTK 9.3/ITK 5.4/tinyxml2 roots;
- six-entry narrow prefix in the expected order, including GDCM 3.0.10 and
  HDF5 1.14.3;
- user/system package registries `FALSE`;
- locked configure-only Python 3.11 executable/include/library;
- TetGen/MMG `OFF`;
- Flow `ON` in `build_shell_a_on` and `OFF` in `build_shell_a_off`;
- English MSVC `msvc_deps_prefix = Note: including file:`.

The configure log contains a non-blocking CMake warning that
`CMAKE_EXPORT_NO_PACKAGE_REGISTRY` is unused. The actual discovery controls are
the two verified `CMAKE_FIND_USE_*_PACKAGE_REGISTRY=FALSE` cache values; no
warning was hidden or treated as acceptance.

## Manual acceptance still required

The reviewer must run:

```bat
XQ\run_xq.bat "<real project path>"
```

Then record next to the evidence directory:

- review machine and project/sample ID;
- real project load (no demo or silent empty fallback);
- visible GUI behavior required by the `D:\XQ` Shell A review;
- screenshots and known limitations;
- explicit A1/A15 human decision.

Until that record exists, the only valid conclusion is: canonical entry is
implemented and its automated evidence is green; **manual acceptance pending**.
