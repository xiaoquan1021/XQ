# ABI and Runtime Report

Audit date: 2026-07-11

## Result

The explicitly scoped VTK/ITK/GDCM/MMG/TetGen/vtkvmtk probes share one x64
MSVC Release ABI and execute successfully. The current product build is not yet
a clean reproducible dependency baseline because of two independent host
ingress paths:

1. Installed Qt 6.7.0 was built against Anaconda zstd 1.5.6 and does not carry
   `zstd.dll` in its locked prefix.
2. Broad product `find_package(ITK 5.4)` causes unused VTK Python import targets
   and `C:\software\anaconda\libs\python312.lib` to appear on the link command.

The linker removes the unused Python imports, so the produced `xq_app.exe` has
no Python runtime dependency. This distinction is important: runtime isolation
passes, while build isolation remains `blocked`.

## Toolchain and configuration

| Property | Audited value |
| --- | --- |
| Generator | Ninja |
| CMake | 3.30.5 MSVC toolset build |
| C/C++ compiler | MSVC 19.43.34810.0, tool root 14.43.34808 |
| Compiler path | `.../VC/Tools/MSVC/14.43.34808/bin/Hostx64/x64/cl.exe` |
| Architecture | x64, 8-byte pointers, PE machine `8664` |
| XQ/probe standard | `/std:c++17`, extensions disabled in probes |
| Release runtime | `/MD`, dynamic UCRT/MSVC runtime |
| Release mode | `/O2 /Ob2 /DNDEBUG`; default Release iterator debug level |
| Linker | MSVC linker 14.43, `/machine:x64`, non-incremental Release links |

The same compiler version, pointer size, generator, and Release mode were read
from the XQ ON/ON, probe, VTK, ITK, GDCM, and MMG build trees. Representative
producer compile commands show:

- XQ and the probes: `/std:c++17 -MD`.
- ITK: `/std:c++17 -MD`.
- VTK: `-MD` with no explicit `/std` flag (MSVC default language mode).
- GDCM and MMG: `/MD`; MMG's linked API is C.

The VTK producer/consumer language-mode difference is recorded rather than
called identical. The C++17 consumer probes instantiate public VTK headers,
link the Release DLLs, and run successfully; no standard-library ABI failure
was observed.

## Release imported locations

- VTK: `VTK-targets-release.cmake` maps each selected `VTK::` target to
  `${prefix}/lib/<name>-9.3.lib` and `${prefix}/bin/<name>-9.3.dll`.
- ITK: `ITKTargets-release.cmake` maps compiled targets such as `ITKCommon`,
  `ITKSmoothing`, `ITKImageFeature`, `ITKIOGDCM`, and `ITKVTK` to the matching
  5.4 import library/DLL. Header-only modules remain interface dependencies.
- GDCM: `GDCMTargets-release.cmake` maps `gdcmMSFF` and its closure to the
  matching import libraries and DLLs under one 3.0.10 prefix.
- MMG: the old package has no installed CMake config. XQ now accepts only an
  explicit-prefix `include/mmg/libmmg.h`, `lib/mmg3d.lib`, and
  `bin/mmg3d.dll` trio and requires all three to share one real prefix.
- TetGen, ITKThickness3D, and vtkvmtk are local static/header-only targets in
  the audit builds and introduce no separate runtime DLL.

No Debug import location appears in the Release link graphs. `dumpbin` shows
MSVCP140/VCRUNTIME140/UCRT imports, not static CRT linkage, for the executable
and representative VTK, ITK, GDCM, MMG, and Qt DLLs.

## Actual ON/ON link selection

`XQ/build_dependency_audit/CMakeCache.txt` records:

```text
XQ_ENABLE_TETGEN:BOOL=ON
XQ_ENABLE_MMG:BOOL=ON
```

Generated `xq_app_shell` objects carry both `XQ_ENABLE_TETGEN` and
`XQ_ENABLE_MMG`. The final link graph contains `xq_adapter_mmg.lib`,
`xq_adapter_tetgen.lib`, `tetgen.lib`, and the locked `mmg3d.lib`. This proves
the enabled application branch does not silently compile the legacy star
fallback instead.

## Runtime dependency closure

MSVC `dumpbin /dependents` was followed recursively through the locked package
directories.

| Entry | Binaries scanned | Forbidden imports | Unresolved external imports |
| --- | ---: | ---: | --- |
| `xq_app.exe` (ON/ON) | 96 | 0 | `zstd.dll` from `Qt6Core.dll` |
| `test_mmg_volume_mesh.exe` | 6 | 0 | 0 |
| `itk_vascular_filters_probe.exe` | 7 | 0 | 0 |
| `itk_vtk_bridge_probe.exe` | 25 | 0 | 0 |
| `itk_thickness3d_probe.exe` | 5 | 0 | 0 |
| `vtkvmtk_centerline_probe.exe` | 24 | 0 | 0 |

Forbidden patterns were Python, MITK, Slicer, CTK, BlueBerry, and vmtk shared
runtime products. The vtkvmtk probe's selected sources are statically linked.
Windows system DLLs such as `AUTHZ.dll` and `DWrite.dll` are not external
package gaps.

### Qt/zstd blocker

- Installed `Qt6Core.dll` hash:
  `35DEE962158C90E7958F3540070C3E6013A140401D2479D05378097FE11DB629`.
- External Qt build cache: `FEATURE_zstd=ON`,
  `zstd_DIR=C:/software/anaconda/Library/lib/cmake/zstd`, version 1.5.6.
- The locked Qt prefix has no `bin/zstd.dll`.
- The inherited test PATH resolves Anaconda `zstd.dll` 1.5.6, hash
  `BE5A011E36657A266E96BF287CF4FF806DCEC43B5B720BB1E80D10A28F91F294`.
- The old, unrelated `XQ/xq_app_dist` contains zstd 1.5.7 instead and is not the
  audited ON/ON executable. It also contains unused VTK Python wrapper DLLs.

The current Externals recipe contains `-no-feature-zstd` and a regression test
named `test_qt_windows_no_host_zstd.ps1`; the install tree predates that fix.
Rebuild the external Qt prefix from that clean recipe before accepting the
runtime baseline.

### Product broad-link blocker

`XQ/CMakeLists.txt` currently uses broad `find_package(ITK 5.4 REQUIRED)` and
links `${ITK_LIBRARIES}` for the old section segmenter. The generated product
link line includes all installed ITK modules, VTK Python targets, and absolute
host `C:\software\anaconda\libs\python312.lib`. The final PE import table does
not include Python because no Python symbols survive linking.

The explicit-component probe link graph contains none of `python*.lib`, VTK
Python targets, MITK, Slicer, CTK, or BlueBerry. A later integration child must
adopt that component recipe before the build can be called reproducible.

## Public header boundary

The permanent architecture check now covers Qt, VTK, ITK, GDCM, ONNX, MMG,
TetGen, and common external type tokens across public core/service/io/adapter
headers. It ignores explanatory `//` comments and fails on code. The direct
scan covered 102 core/service/io/adapter headers and found no third-party
public type or include.

The check command is:

```powershell
cmake -DXQ_SOURCE_DIR=<repo>/XQ -P XQ/tests/cmake/check_arch_boundaries.cmake
```

Result: `Architecture boundaries OK: no forbidden includes found.`

## Negative configure evidence

Each case used a fresh isolated tree under `.trellis/workspace/ocean/`.

| Scenario | Exit | Deterministic diagnostic |
| --- | ---: | --- |
| Missing vtkvmtk source | 1 | `XQ_AUDIT_VMTK_SOURCE_DIR must be provided explicitly` |
| Wrong expected VTK package dir | 1 | `XQ_AUDIT_EXPECTED_VTK_DIR is not a VTK package directory` |
| MMG ON, TetGen OFF | 1 | `XQ_ENABLE_MMG requires XQ_ENABLE_TETGEN=ON` |
| MMG prefix omitted | 1 | `XQ_ENABLE_MMG=ON but MMG not found` |
| Forced bogus MMG header/library | 1 after fix | `XQ_ENABLE_MMG=ON but MMG not found` |
| Generic MMG header/lib/DLL without `libmmg3d.h` | 1 | `XQ_ENABLE_MMG=ON but MMG not found` |
| Header and library from different prefixes | 1 | `MMG headers and library must come from one install prefix` |
| Header/import library present, DLL absent | 1 | `MMG import library has no matching runtime DLL` |

Before the CMake validation change, the forced bogus MMG pair configured with
exit 0. That reproduced defect is why the stronger explicit-prefix and file
checks are part of this task.

## ABI decision

- Explicit vascular probe ABI: `accepted`.
- TetGen/MMG ON/ON technical ABI: `accepted`.
- Product runtime forbidden-import boundary: `accepted`, with Qt/zstd unresolved.
- Current clean/reproducible product dependency baseline: `blocked` until the
  stale Qt install is rebuilt and product ITK/VTK discovery is narrowed.
