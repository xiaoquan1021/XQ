# Current-State Evidence: Google Earth 壳档 A

## Repository and worktree

- Implementation baseline: `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui`, branch `feat/render-arch`, task base recorded by Trellis as `feat/render-arch`.
- The directory is a Git worktree of `C:\Users\OCEAN\Desktop\XIAOQUAN`, not a separate repository.
- Existing unrelated/untracked files (`CHECK-*.md`, `EXECUTE-*.md`, `XQ/xq_app_dist/`) must not be removed, rewritten, staged, or committed by this task.

## Reusable shell foundation already present

- Architecture is already split into `app -> services -> adapters/io -> core`; public business APIs use XQ-owned types and external kernels stay private (`.trellis/spec/XQ/architecture/index.md:8-35`).
- Patient-space image geometry already carries dimensions, spacing, origin, direction and LPS/RAS identity, plus DICOM UIDs and voxel/world transforms (`XQ/src/core/XQImageVolume.h:34-117`).
- Scene/project infrastructure already supplies stable nodes, payloads, source/derived relations, transitive stale propagation, command-based mutation and undo/redo (`.trellis/spec/XQ/core/command-and-scene.md`).
- Asset/blob/mmap/source infrastructure already exists and must be extended rather than replaced: `AssetRegistry`, `BlobStore`, `IVoxelSource`, `IGeometrySource`, mapped sources and resource managers.
- GUI/workflow infrastructure already supplies `XQWorkflowSession`, six typed controllers, `XQTaskRunner`, scene refresh, Qt/VTK views and project lifecycle (`XQ/src/app/XQWorkflowSession.h`).
- Rendering LOD and chunked/progressive upload already exist; they are rendering fidelity only and are not biological/physical scale (`.trellis/spec/XQ/visualization/lod-and-upload.md`).
- ITK 5.4 is already linked through `xq_adapter_itk`; GDCM 3.0.10 and `ITKIOGDCM` are installed and enabled in the existing ITK build (`Externals/externals.manifest:9-11`, installed `ITKConfig.cmake`).
- The existing `FlowSolver1D`, RCR boundary condition support and `XQFlowResult` are reusable as the first real consumer smoke, without claiming M5 solver credibility.

## Confirmed shell gaps

### Real DICOM data spine

- No DICOM series reader exists in `XQ/src`; current real voxel entry is `VtkImageAdapter::loadVti/loadVtiWithBuffer` (`XQ/src/adapters/vtk/VtkImageAdapter.h:21-31`).
- `DicomSeriesIdentity` exists only as a core contract (`XQ/src/core/XQImageVolume.h:65-69`).
- The active image is also held in window-private `activeImage_` state (`XQ/src/app/XQMainWindow.h:596-598`), so successful display alone does not prove the image is available through Scene/Asset to headless services after save/reopen.
- Existing sample trees `0007_H_AO_H` and `0080_H_PULM_H` contain VTI/SV artifacts but no DICOM image series.

### Solver geometry authority

- `XQPath` is a navigation/resampling/frame object; `PathSamplePoint` has position, tangent, normal, binormal and arc length, but no physical area/radius (`XQ/src/core/XQPath.h:31-37`).
- `XQContourGroup` stores measured contours bound to `pathArcLength` (`XQ/src/core/XQContourGroup.h:31-38`).
- Current flow assembly computes area from contours inside `XQMainWindow` and directly fills `solverInput.arcLength/area0` (`XQ/src/app/XQMainWindow.cpp:986-1051`).
- `FlowSolver1D` explicitly requires CGS: length in cm and area in cm^2 (`XQ/src/services/flow/FlowSolver1D.h:20-23`). The current MainWindow assembly path does not expose a centralized, auditable mm-to-CGS conversion boundary.
- Therefore adding a second authoritative radius directly to `XQPath` would risk Path/Contour drift. A separate canonical `VesselProfileV1` is required.

### Capability optionality and scale identity

- `XQWorkflowSession` owns concrete Path/Segmentation/Modeling/Meshing/Flow/AI controllers; Flow is not currently demonstrated as a disabled/absent capability.
- No `ScaleSlot { Organ, Micro, Cell }` was found in source. Existing LOD must remain separately named and must not satisfy the scale acceptance criterion.
- No generic plugin runtime is required for shell A. The existing typed-port pattern (`ITetMesher`, `ILevelSetSegmenter`) is the reuse model for future scientific capabilities.

## Acceptance baseline

- Project acceptance requires Release full `ctest` plus the real `0007_H_AO_H` end-to-end path, and data must enter XQ-owned objects (`.trellis/spec/XQ/core/acceptance.md`).
- New DICOM acceptance needs an additional configurable DICOM data root and a pinned, anonymized, legally usable public series. CI may use a compact fixture, but manual acceptance must also exercise a real multi-slice series.
- Both headless and GUI paths must use the same canonical objects and services; GUI-private state and VTI-only paths cannot substitute for DICOM acceptance.

