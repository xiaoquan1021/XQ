# Current State Audit

Date: 2026-07-11

Purpose: record what is actually present before replanning. This is evidence, not a completion claim.

## Repository / branch

- Repository: `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui`
- Branch: `feat/google-earth-shell-a`
- HEAD at audit: `746cd66`
- Working tree already contains user-owned untracked files; they are out of scope and must be preserved.

## Installed dependencies

Observed under `C:\Users\OCEAN\Desktop\XIAOQUAN\Externals\install\windows-x64`:

| Dependency | Installed version | Important observation |
| --- | --- | --- |
| VTK | 9.3.0 | Current XQ build points to this exact CMake package |
| ITK | 5.4.0 | Includes ITKVtkGlue, ITKReview, ITKIOGDCM, ITKRegionGrowing and GrowCut headers |
| GDCM | 3.0.10 | Used by production DICOM adapter |
| MMG | 5.3.9 | Installed; XQ adapter is optional and default OFF |
| TetGen | vendored header reports 1.5 / CMake says 1.5.1 | Bundled LICENSE is AGPLv3; requires explicit compatibility decision |
| vtkvmtk | not found | No installed vtkvmtk tree or existing XQ adapter was found |

The ITK include tree contains anisotropic diffusion, Hessian objectness, multi-scale Hessian, connected/relabel components, SignedMaurer distance map and ITK-VTK bridge headers. It does not contain `BinaryThinningImageFilter3D` or an installed Thickness3D module. The available `BinaryThinningImageFilter` is not evidence of a true 3D fallback.

## Existing production capabilities

### DICOM data spine

- `XQ/CMakeLists.txt:172-187` builds `xq_adapter_dicom` and links ITKIOGDCM + GDCM.
- Existing service/import/resolver tests cover UID selection, LPS geometry, atomic import, persistence and voxel-source reacquisition.
- This is reusable and should not be rewritten.

### ITK usage is currently two-dimensional

- `XQ/src/adapters/itk/ItkVascularSegmenter.h:9-12` states the adapter consumes a section grayscale image in index/pixel space.
- `XQ/src/adapters/itk/ItkVascularSegmenter.cpp:20` uses `itk::Image<float, 2>`.
- It is a SimVascular two-phase level-set contour tool for one resampled cross-section, not a 3D volume segmentation pipeline.

### Current 3D segmentation is XQ-owned basic code

- `XQ/src/services/segmentation/SegmentationService.h:42` explicitly says the traditional algorithms are XQ code with zero ITK/VTK dependencies.
- `XQ/src/services/segmentation/SegmentationService.h:83-97` exposes threshold, seeded 6-connected region growing and largest 6-connected component.
- These are valid basic/manual tools but do not satisfy the requested mature ITK vessel pipeline.

### Current Path is manually constructed

- `XQ/src/services/path/PathService.h:50-55` creates a Path from caller-provided `PathControlPoint` values.
- The main window contains MPR control-point picking workflow.
- No automatic mask-to-centerline/radius/tree production service exists.

### ITK-VTK bridge is not in the application chain

- ITK installation provides `itkImageToVTKImageFilter` and `itkVTKImageToImageFilter`.
- XQ source search found no use of those filters.
- `VtkImageAdapter` reads VTI independently; that is not an ITK-VTK bridge.

### TetGen / MMG are real adapters but not yet real-data accepted

- `XQ/CMakeLists.txt:919` and `:958` define `XQ_ENABLE_TETGEN` and `XQ_ENABLE_MMG`, both default OFF.
- `XQ/CMakeLists.txt:953-981` requires TetGen as the fill stage before MMG.
- `XQ/tests/adapters/test_tetgen_volume_mesh.cpp:132-199` and `test_mmg_volume_mesh.cpp:129-200` generate a synthetic curved tube programmatically.
- Those tests are valuable kernel regressions, but they are not a real CTA-derived vessel production gate.

## Existing real DICOM data

`D:\XQ\data\dicom\tcia_lidc_idri_0957_ct\SOURCE.md` records:

- TCIA LIDC-IDRI, subject `LIDC-IDRI-0957`.
- CT / CHEST, 65 slices.
- Public source/license/de-identification/hash evidence.
- XQ DICOM reader/import/save/reopen gate passed.

It does not provide a reference vascular mask and is not selected for contrast-enhanced vascular segmentation. It can remain the DICOM IO oracle only.

## Honest capability matrix

| Work package | Present | Production real-data accepted |
| --- | ---: | ---: |
| DICOM read into XQ types | yes | yes, for IO semantics |
| 3D ITK denoising | no | no |
| Frangi/Sato vesselness | no | no |
| Automatic 3D vessel segmentation | no | no |
| ITK morphology/component pipeline | no | no |
| Actual ITK-VTK bridge | no | no |
| Automatic centerline | no | no |
| Radius extraction | no | no |
| Tree topology extraction | no | no |
| vtkvmtk adapter | no | no |
| True 3D skeleton fallback | no | no |
| TetGen/MMG adapters | yes, optional | no, only synthetic geometry evidence |
| Real CTA + reference label gate | no | no |
| Full automatic CTA-to-GUI chain | no | no |

## Conclusion

The repository has a strong host/data foundation and some real external kernels, but the requested medical image/vascular geometry foundation is at its beginning. The next honest stage is external dependency/data gating followed by the 3D ITK pipeline—not more Flow smoke or artificial Path acceptance.
