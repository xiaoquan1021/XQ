# Design: 完整复用壳 v2 总交付

## 1. Architecture Decision

采用“现有 XQ 宿主 + 外部成熟 kernel adapters”的单一产品链。宿主不重写，标准影像/几何算法不再由 XQ 重造。

```text
XQ GUI / headless intent
  -> VascularWorkflowService
       -> GDCM/ITK IO adapter
       -> ITK preprocessing adapter
       -> ITK automatic segmentation adapter
       -> ITK-VTK bridge adapter
       -> ICenterlineExtractor
       -> surface/mesh adapters
  -> validated XQ-owned result bundle
  -> one atomic Project command
  -> Scene / renderer / persistence
```

## 2. Ownership Boundary

| Capability | Reuse owner | XQ owns |
| --- | --- | --- |
| DICOM and image IO | GDCM + ITK IO | source selection, fingerprint, XQ Volume |
| 3D denoise/vesselness | ITK | parameter profile, diagnostics, provenance |
| segmentation/morphology/components | ITK | automatic orchestration and component policy |
| image-to-geometry bridge | ITKVtkGlue | LPS/mm contract and lifetime checks |
| surface/rendering | VTK | XQ surface handle and Scene integration |
| centerline/radius | vtkvmtk C++ or approved thinning source | stable XQ tree IDs, validation and backend selection |
| volume mesh | TetGen/MMG or approved backend | license gate, quality gate, XQ mesh contract |
| application shell | existing XQ | Project/Scene/commands/GUI/save/reopen |

## 3. Product Cutover

The existing threshold/seed UI is not extended. The final E2E child replaces its primary page action with an automatic vascular workflow. Legacy calls may remain behind an explicitly non-production diagnostic route until removal is safe, but the normal user path and acceptance runner cannot reach them.

## 4. Data and Evaluation Separation

`ProductionRunner(image, profile)` has no gold parameter. `Evaluator(prediction, gold, frozenMetricSpec)` runs only after outputs are frozen. Fingerprints prove that gold data did not influence segmentation, centerline or mesh inputs.

## 5. Task Dependency Graph

```text
dependency-remediation ----+
                           +-> itk-preprocess -> auto-segmentation --+
real-CTA-data-gate --------+                                      |
dependency-remediation --------> itk-vtk-bridge ------------------+-> centerline-tree --+
shell-a-centerline-b ----------------------------------------------+                  |
                                                                  +-> real-meshing -----+-> vascular-e2e
shell-a-v1-alignment --------------------------------------------------------------+-----> final-acceptance
vascular-e2e ----------------------------------------------------------------------+ 
```

`07-12-shell-a-centerline-b` must be completed on the Shell A v1 track as the minimal A13 fallback before the v2 tree child starts. `vascular-foundation-centerline-tree` consumes that implementation and extends it for v2; it does not create a second thinning/distance stack. No v2 child blocks Shell A v1 completion.

## 6. Failure Semantics

Each stage returns XQ-owned status, diagnostics, backend/version and no partial Scene mutation. The workflow prepares every output off-Scene, validates the complete bundle, then commits once. Cancellation or failure leaves the prior project state unchanged.

## 7. Compatibility

Existing projects, Source metadata, `VesselProfileV1`, Path modules and Flow-off behavior remain readable. The production vascular workflow may add new persisted XQ-owned results only through an explicit versioned schema plan in its owning child; it must not create a second authority for existing data.
