# Reuse and Replacement Matrix

Authority: `D:\XQ\可复用工作打包说明.md` and `D:\XQ\规划-GoogleEarth全路线与壳子阶段.md`.

| Shell capability | Predecessor work to reuse | Current XQ state | v2 action |
| --- | --- | --- | --- |
| DICOM | GDCM + ITK IOGDCM | Production reader exists | Keep; use as the sole image ingress |
| 3D denoise | ITK anisotropic diffusion | Missing | Add ITK adapter; no custom filter |
| Vessel enhancement | ITK multi-scale Hessian Frangi/Sato | Missing | Add profile-driven ITK adapter |
| Automatic segmentation | ITK threshold/level-set/morphology/components | Main UI uses custom threshold and seed grow | Replace default main path; legacy diagnostic only |
| Connected cleanup | ITK ConnectedComponent/RelabelComponent | Custom largest-component kernel | Replace kernel; XQ retains explicit branch-aware policy |
| Image/geometry bridge | ITKVtkGlue | Installed but unused in product | Add official bridge with LPS/direction checks |
| Surface | VTK extraction/smoothing/decimation | VTK host exists | Reuse VTK filters through private geometry adapter |
| Centerline/radius | vtkvmtk minimal C++ closure | Source candidate only | Time-box A under one interface |
| Centerline fallback | ITKThickness3D/3D thinning + distance map | Source candidate only | Use only if A fails; self-write graph/pruning, not thinning kernel |
| Volume mesh | TetGen + MMG | Adapters exist; research-only and toy validation | Run on real vessels under explicit license/quality gate |
| GUI/Scene/save | Existing XQ host | Exists | Keep and wire the production workflow into it |
| Flow/RCR | Existing XQ modules | Exists | Preserve as downstream; not part of this shell replacement |
| TotalSegmentator | Offline mask/data | Not runtime | Optional offline data/ROI only; never product dependency or gold shortcut |
| Scale identity | 3D Tiles/Physiome ideas | Thin ScaleSlot exists/planned | Keep thin identity; no Cesium runtime |
| Whole platforms | Slicer/MITK/CTK | Prohibited | Do not integrate; only boundary ideas are reused |

## Direct replacement points

- `XQ/src/services/segmentation/SegmentationService.*`: no longer production owner of threshold, region grow or component kernel.
- `XQ/src/ui/controllers/SegmentationController.*`: production intent changes from threshold/seed to automatic vascular profile.
- `XQ/src/ui/panels/XQStageWidgets.cpp`: Segmentation page replaces threshold/region-grow controls with automatic pipeline controls and diagnostics.
- `XQ/src/adapters/itk/ItkVascularSegmenter.*`: keep the separate 2D section-level-set editor only if still needed; do not misrepresent it as 3D automatic segmentation.

## Not replaced

Project/Scene/payload/command/lineage/persistence, renderer, GUI shell, module boundary, FlowSolver1D and RCR are retained. ITK does not own these concerns.
