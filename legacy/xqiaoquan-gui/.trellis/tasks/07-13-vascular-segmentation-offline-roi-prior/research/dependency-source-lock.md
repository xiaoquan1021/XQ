# Reused dependency and source lock

Recorded: 2026-07-13

This file freezes the source boundary for the automatic ROI-guided vessel
segmentation task. It is an implementation input, not a permission to discover
or substitute another algorithm at build or runtime.

## Product toolchain baseline

| Item | Locked identity | Product use |
| --- | --- | --- |
| C++ | C++17 | `XQ/CMakeLists.txt` sets `CMAKE_CXX_STANDARD 17` |
| MSVC CRT | dynamic CRT (`/MD`, `/MDd` only for Debug) | `CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded...DLL`; no `/MT` objects |
| ITK | 5.4.0 | image import, threshold, region growing/reconstruction, level set, morphology, connected components, relabel, distance, thinning support and path module closure in `adapters/itk` |
| GDCM | 3.0.10 | DICOM decode through ITK `ITKIOGDCM`; direct GDCM consumers define `GDCM_BUILD_SHARED_LIBS` |
| VTK | 9.3.0 | existing shell/rendering and ITK bridge only; no VTK segmentation algorithm is introduced by this task |
| Qt | 6.7.0 | existing desktop shell only |
| TinyXML2 | 8.0.0 | existing XQ project persistence only |

Version authority is
`C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/externals.manifest`. The canonical
ON/OFF CMake caches resolve six narrow roots only: isolated Qt 6.7.0, GDCM
3.0.10, HDF5 1.14.3, ITK 5.4.0, TinyXML2 8.0.0 and VTK 9.3.0. Debug and Release
artifacts must not be mixed.

The installed `ITKVtkGlue` metadata requires Python 3.11 during CMake package
configuration. That interpreter is configure-only. Python targets, libraries,
executables and DLLs are not permitted in the product link graph, PE closure or
runtime preflight.

## Exact upstream source identities

| Source | Identity and archive | License lock | Schema-4 status |
| --- | --- | --- | --- |
| ITK | official 5.4.0 release from the Externals manifest | Apache-2.0; local `LICENSE` SHA-256 `AAC73B3148F6D1D7111DBCA32099F68D26C644C6813AE1E4F05F6579AA2663FE` | consumed through explicitly named ITK modules only |
| GDCM | official v3.0.10 release from the Externals manifest | BSD-style `Copyright.txt`; SHA-256 `31A64B1BC4F367401FDD689DAEE273AF8195C24EB5D8DCD5B694F201B06721ED` | consumed by the DICOM adapter and ITK IOGDCM |
| VTK | official 9.3.0 release from the Externals manifest | BSD-style `Copyright.txt`; SHA-256 `11232448BE82E0EA2C2C66219C2E36389F42249894070EE10C549FF182FC08B6` | existing shell/rendering dependency; not a schema-4 segmentation kernel |
| ITK-TubeTK | release 1.3.5; `D:/XQ/research/ITKTubeTK-1.3.5.tar.gz`; SHA-256 `26972916EB332275106A6AF47CAE3182B0D035ECA329EC56069D630A24747A4C` | Apache-2.0; `LICENSE` SHA-256 `AAC73B3148F6D1D7111DBCA32099F68D26C644C6813AE1E4F05F6579AA2663FE` | legacy research/artifact-v5 provenance only; not built, linked or loaded by schema 4 |
| ITK MinimalPathExtraction | commit `35dd8e83b7df2059876e6835a5741eb3d45973bf`; `D:/XQ/research/ITKMinimalPathExtraction-35dd8e83.tar.gz`; SHA-256 `A2EDCCA4BC07175487BE34E0A6C1B780CC176A67E6F9A1DE20C21D55911D4FB4` | Apache-2.0; `LICENSE` SHA-256 `AAC73B3148F6D1D7111DBCA32099F68D26C644C6813AE1E4F05F6579AA2663FE` | adopted optional schema-4 path kernel; exact consumed source is vendored privately and compiled against product ITK 5.4 |
| TotalSegmentator | 2.15.0; standard-resolution case-1 report under the external IRCAD derived-data bundle | Apache-2.0; installed `LICENSE` SHA-256 `C71D239DF91726FC519C6EB72D318EC65820627232B2F796219E87DCF35D0AB4` | offline data generator only; Python, torch, nnU-Net and weights never enter XQ |

The accepted standard-resolution run report records TotalSegmentator 2.15.0,
nnU-Net 2.8.1 and torch 2.13.0+cu126 and requests only `liver` and
`portal_vein_and_splenic_vein`. Its outputs and hashes remain external data.

## Actual schema-4 dependency decision

- Consume only the explicit ITK 5.4 module closures needed by the schema-4
  composition: the existing diffusion/Hessian/objectness and IO modules plus
  `ITKLevelSets`, `ITKImageGradient`, `ITKImageIntensity`,
  `ITKBinaryMathematicalMorphology`, `ITKConnectedComponents`,
  `ITKDistanceMap`, `ITKFastMarching`, `ITKOptimizers` and `ITKPath`. Never use
  package-wide `${ITK_LIBRARIES}` or `ITK_USE_FILE`.
- Remove the `xq_itk_tubetk_ridge` target and its include/link closure from the
  current product graph. The vendored files may remain untouched as legacy
  source evidence because artifact v5 records TubeTK identity.
- Vendor only the exact consumed MinimalPathExtraction source files under
  `XQ/third_party/itk_minimal_path`, preserve the upstream LICENSE/notices and
  compile them against the locked product ITK. Never link a research install
  prefix or a second ITK build.
- Do not link or include `D:/XQ/research/itk-tubetk-install`, vendored TubeTK,
  TotalSegmentator, Python, torch or nnU-Net in schema 4. Source-provenance
  strings and offline ROI files are not runtime code dependencies.
- XQ may implement the project-owned component/topology policy and choose the
  explicit physical endpoints passed to MinimalPath. XQ must record that policy
  as its own and must not implement component labeling, thinning, fast marching
  or path optimization in local voxel traversal code.
- If ITK or the locked upstream path source cannot express a required kernel,
  stop and revise the task instead of hand-writing a replacement.
