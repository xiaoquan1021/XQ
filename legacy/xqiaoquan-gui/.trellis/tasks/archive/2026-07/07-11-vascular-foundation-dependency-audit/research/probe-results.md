# Probe Results

Audit date: 2026-07-11

## Reproducible positive commands

Standalone dependency probes:

```bat
XQ\probes\vascular_foundation\run_audit_probes.bat
```

TetGen/MMG ON/ON XQ build and tests:

```bat
XQ\probes\vascular_foundation\run_mesh_on_audit.bat
```

Both scripts call `vcvars64.bat`, set `VSLANG=1033`, use Ninja Release, and
pass explicit source/install roots. The probe script pins exact ITK/VTK package
directories and rejects a second VTK package.

Probe source-set aggregate SHA-256, over sorted `filename|file-SHA256` lines:
`284A2AB703B5F4BC774A9BEBEA3B9D5B9AB66503FF309AC5E497B8135E3DC4D6`.

## Standalone probes

CTest result: 4/4 passed in 2.00 seconds from the final fresh scripted run and
in 1.75 seconds on the subsequent verbose run.

### ITK vascular filters

Capabilities executed in one real 3D template pipeline:

- curvature anisotropic diffusion;
- multi-scale Hessian objectness/vesselness;
- binary morphology and connected/relabel components;
- signed Maurer distance in physical spacing.

Output:

```text
PASS itk_vascular_filters_probe vesselness_max=0.83499 foreground=547 components=1 distance_max_mm=1.14017
```

Decision: the explicit ITK 5.4 component set is linkable and executable. This
synthetic tube proves dependency capability, not CTA segmentation quality.

### ITK-VTK oblique bridge

The input uses non-identity 3x3 direction, non-unit spacing, and non-zero
origin. The probe performs ITK-to-VTK and VTK-to-ITK conversion, then compares
dimensions, spacing, origin, direction, voxel value, and selected
index-to-physical-point round trip.

Output:

```text
PASS itk_vtk_bridge_probe itk=5.4.0 vtk=9.3.0 selected_physical=12.6119,-5.16706,52.2 value=432.25
```

The configure check also proves ITKVtkGlue selected the exact audited VTK
9.3.0 package directory.

### ITKThickness3D

The true 3D filter reduces a three-dimensional tubular foreground while
preserving axial connectivity.

Output:

```text
PASS itk_thickness3d_probe input=931 skeleton=15 z_span=14 radial_max=0
```

Decision: ITKThickness3D `v5.3.0` is an accepted fallback candidate. It has not
been compared with vtkvmtk on a CTA mask and is not selected as the production
route by this probe alone.

### vtkvmtk centerline

Input:

- `0007_H_AO_H/Models/0090_0001.vtp`
- 6,313,154 bytes
- SHA-256
  `7FA57F163319D43348EDF5B8C52FA0F47231A37A16845FD4FEE3B36592AD372A`

The probe triangulates and topology-preserving decimates the real closed
surface, chooses endpoint seeds from the geometry, executes the eight-source
vtkvmtk centerline closure, and checks finite positive
maximum-inscribed-sphere radii.

Output:

```text
PASS vtkvmtk_centerline_probe input_points=84542 probe_points=6765 centerline_points=18 centerline_lines=1 radius_range=0.902857,1.01548 cap_distance=0.118633,0.231281
```

Decision: vtkvmtk is an `accepted` technical candidate against VTK 9.3.0. The
small 18-point result is only stability/capability evidence; it is not a
centerline-quality acceptance result.

## TetGen/MMG ON/ON build

Configuration:

```text
CMAKE_BUILD_TYPE=Release
XQ_ENABLE_TETGEN=ON
XQ_ENABLE_MMG=ON
CMAKE_CXX_COMPILER_VERSION=19.43.34810.0
CMAKE_CXX_SIZEOF_DATA_PTR=8
```

Results:

- all 318 Ninja targets built;
- focused CTest: 3/3 passed in 2.66 seconds
  (`test_tetgen_volume_mesh`, `test_mmg_volume_mesh`,
  `test_arch_boundaries`);
- full Release CTest: 94/94 passed in 18.57 seconds;
- generated app-shell compile definitions include `XQ_ENABLE_TETGEN` and
  `XQ_ENABLE_MMG`;
- final app link includes both adapters, `tetgen.lib`, and `mmg3d.lib`;
- the MMG CTest PATH fix prevents the prior `0xc0000135` process-load failure.

The installed MMG executable reports:

```text
-- MMG3d, Release 5.3.8 (Apr. 10, 2017)
```

This matches the stale macros inside the official `v5.3.9` tag. Build/install
DLL hashes are identical, so it is not evidence of a mixed install.

TetGen remains `accepted-research-only` because successful execution does not
resolve its AGPL/commercial distribution decision.

## Runtime and public-boundary probes

- Recursive `dumpbin /dependents` closure:
  - ON/ON app: 96 binaries scanned, zero Python/MITK/Slicer/CTK/BlueBerry/vmtk
    runtime imports; unresolved external `zstd.dll` from stale Qt.
  - five probe/test entries: 67 total per-entry closure visits, zero forbidden
    imports and zero unresolved non-system dependencies.
- PE headers: x64 machine `8664`; linker 14.43.
- CRT imports: dynamic MSVCP140/VCRUNTIME140/UCRT family; no `/MT` evidence.
- Public lower-layer header scan: 102 core/service/io/adapter headers, no
  external type/include match.
- A negative fixture produced exactly three violations for a quoted ITK
  include, `OrtApi`, and `QUrl`, proving the guard covers include and type
  leakage without corrupting semicolon-terminated diagnostics.
- Permanent `test_arch_boundaries` passes with the expanded MMG/TetGen/type
  rules.

## Negative configure probes

| Probe | Before fix | Final result |
| --- | --- | --- |
| Missing vtkvmtk source | N/A | exit 1, explicit required-variable diagnostic |
| Invalid expected VTK package directory | N/A | exit 1 before package discovery |
| MMG ON without TetGen | N/A | exit 1, explicit dependency diagnostic |
| MMG prefix omitted | N/A | exit 1, no fallback backend |
| Forced bogus MMG include/library pair | exit 0 (defect reproduced) | exit 1 after validation fix |
| Generic MMG header/lib/DLL without the MMG3D API header | N/A | exit 1 before target generation |
| MMG header/library from different prefixes | N/A | exit 1 with both prefixes printed |
| MMG header/import library without DLL | N/A | exit 1 with expected DLL path printed |

The negative trees and fake incomplete MMG prefix live under the ignored
`.trellis/workspace/ocean/` audit workspace. No external install was modified.

## Known blockers and limits

1. Qt 6.7.0 must be rebuilt from the corrected external recipe so QtCore no
   longer depends on host Anaconda zstd.
2. Product ITK discovery must be narrowed to explicit components so the build
   link line no longer names host Python or unused VTK Python targets.
3. TetGen distribution requires a commercial license, an AGPL-compatible
   distribution decision, or a replacement fill backend.
4. Real CTA segmentation, centerline quality, topology, radius accuracy, and
   production vessel mesh quality are not tested here.
5. Passing 94 XQ regressions and these dependency probes does not complete the
   parent vascular imaging foundation task.
