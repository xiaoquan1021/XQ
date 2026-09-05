# Baseline Inventory

This is the pre-probe snapshot. Final reconciliations and decisions are in
`dependency-lock.md`, `abi-report.md`, `license-report.md`, and
`probe-results.md`.

## Manifest

The external manifest at `C:\Users\OCEAN\Desktop\XIAOQUAN\Externals\externals.manifest` records:

- VTK 9.3.0
- ITK 5.4.0
- GDCM 3.0.10
- MMG 5.3.9
- MITK 2024.06 and Python 3.11.0 are installed external packages but forbidden as XQ product runtime dependencies
- no vtkvmtk entry
- no TetGen entry

## XQ configuration

- `XQ/CMakeLists.txt:5-6`: C++17 required.
- `XQ/CMakeLists.txt:99`: VTK 9 requested with explicit components.
- `XQ/CMakeLists.txt:138-139`: broad ITK package + legacy `ITK_USE_FILE`.
- `XQ/CMakeLists.txt:184-187`: production DICOM adapter links ITKIOGDCM and gdcmMSFF.
- `XQ/CMakeLists.txt:919`: TetGen optional, default OFF.
- `XQ/CMakeLists.txt:958`: MMG optional, default OFF and requires TetGen.
- Current `XQ/build_gui/CMakeCache.txt` points to VTK 9.3.0 and ITK 5.4.0; both mesh options are OFF.

## Installed ITK capability clues

Present headers/modules include:

- curvature/gradient anisotropic diffusion
- Hessian objectness and multiscale Hessian measure
- connected/relabel components
- SignedMaurer distance map
- ITKVtkGlue / image-to-VTK and VTK-to-image bridge
- ITKReview / GrowCut related installation artifacts

Not found:

- `BinaryThinningImageFilter3D`
- Thickness3D installed module

Header presence was not accepted as capability evidence; the completed runtime
probes are recorded in `probe-results.md`.

## Mesh dependency contradiction

- `XQ/third_party/tetgen/tetgen.h` reports Version 1.5.
- XQ CMake comments call the backend TetGen 1.5.1 plus an SV patch.
- `XQ/third_party/tetgen/LICENSE` contains GNU Affero GPL v3 text.

The final audit resolved this as byte-identical SimVascular TetGen 1.5 under an
AGPL-3.0-or-later/commercial choice, classified `accepted-research-only`.
