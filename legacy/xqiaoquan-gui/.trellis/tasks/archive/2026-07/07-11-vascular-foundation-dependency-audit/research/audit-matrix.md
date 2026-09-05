# Audit Matrix

Audit date: 2026-07-11

Decisions use only `accepted`, `accepted-research-only`, `rejected`, or
`blocked`. Dependency viability is kept separate from the current product build
recipe so a clean probe cannot hide host ingress in the application build.

| Dependency/capability | Installed/source locked | Manifest | XQ/product linked | Runtime executed | License resolved | Final decision | Evidence anchor / limit |
| --- | --- | --- | --- | --- | --- | --- | --- |
| VTK 9.3.0 explicit C++ targets | yes | yes | yes | yes, product tests and probes | BSD-3-Clause | `accepted` | `dependency-lock.md` VTK record; `probe-results.md` standalone and runtime probes |
| ITK 5.4.0 explicit components | yes | yes | yes | yes, product tests and probes | Apache-2.0 | `accepted` | `dependency-lock.md` ITK record; `probe-results.md` vascular-filter probe |
| ITK 3D vascular filters | yes, installed modules | via ITK | probe only | yes | Apache-2.0 via ITK | `accepted` | Vesselness `0.83499`, foreground `547`, one component, distance `1.14017 mm` |
| ITKVtkGlue oblique bridge | yes, installed module | via ITK | probe only | yes | Apache-2.0 plus VTK notice | `accepted` | Exact ITK 5.4.0/VTK 9.3.0 package check and oblique geometry round trip |
| GDCM 3.0.10 | yes | yes | yes | yes, production DICOM tests | BSD-3-Clause-style | `accepted` | `dependency-lock.md` GDCM record and imported-target hashes |
| ITKThickness3D v5.3.0 fallback | yes, clean pinned source | no | probe only | yes | Apache-2.0 | `accepted` | True-3D thinning `931 -> 15` voxels with z-span `14`; no second ITK |
| vtkvmtk at SimVascular `b8c30d7` | yes, clean pinned source | no | probe only | yes, real closed surface | VMTK BSD plus SimVascular/VTK notices | `accepted` | Real `0090_0001.vtp`: 18 centerline points, finite radius `0.902857..1.01548`; quality remains out of scope |
| TetGen self-identified 1.5 | yes, vendored and source-matched | no | yes in ON/ON audit build | yes | AGPL-3.0-or-later or commercial | `accepted-research-only` | Byte-identical SimVascular files; not approved for proprietary distribution |
| MMG official tag v5.3.9, runtime string 5.3.8 | yes | yes | yes in ON/ON audit build | yes | LGPL-3.0-or-later | `accepted` | All 265 official files match; header/lib/DLL share one prefix; two-stage production use still depends on an approved fill backend |
| MITK/BlueBerry/Slicer/CTK runtime | installed elsewhere | MITK only | no | absent from scanned closures | not applicable to adoption | `rejected` | Forbidden runtime boundary; zero imports in audited entries |
| Python runtime | installed elsewhere | Python 3.11 only | no surviving PE import | absent from scanned closures | not applicable to adoption | `rejected` | Forbidden runtime boundary; host Python still appears in the blocked broad link command |
| Host Anaconda zstd | host-only | no | indirect through installed QtCore | yes, resolved only through inherited PATH | not locked for XQ | `rejected` | `abi-report.md` Qt/zstd blocker; do not package the incidental DLL |
| Installed Qt 6.7.0 binary | yes | yes | yes | only with host zstd | license route known; binary closure not locked | `blocked` | Rebuild with the corrected `-no-feature-zstd` recipe before baseline acceptance |
| Current broad product ITK/VTK discovery | current source recipe | not a lock entry | names VTK Python targets and host `python312.lib` | linker removes final Python imports | transitive closure not controlled | `blocked` | Replace broad `${ITK_LIBRARIES}` discovery with the explicit component recipe in a later integration child |

## Overall decision

- The explicit Windows x64 C++ dependency probe baseline is `accepted`.
- The TetGen-to-MMG research build is technically reproducible, but TetGen is
  `accepted-research-only`; this audit does not approve its production
  distribution.
- The current product dependency baseline is `blocked` until Qt is rebuilt
  without host zstd and broad ITK/VTK discovery is narrowed.
- Runtime isolation passes for Python/MITK/Slicer/CTK/BlueBerry, while clean
  build isolation does not yet pass.
- These decisions do not claim real CTA segmentation, centerline quality,
  production mesh quality, or completion of the parent vascular task.
