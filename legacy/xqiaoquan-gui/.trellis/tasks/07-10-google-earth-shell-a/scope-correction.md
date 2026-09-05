# Scope Correction: 2026-07-11

This note preserves the historical work while correcting its completion meaning.

## What this task actually delivered

`07-10-google-earth-shell-a` and its children delivered a valuable internal host/data-spine vertical slice:

- XQ-owned domain/persistence contracts.
- Production GDCM/ITK DICOM series IO.
- Source/resource, lineage/stale and save/reopen behavior.
- Manual Path/Contour -> VesselProfile -> Flow geometry smoke integration.
- GUI/headless orchestration and Flow ON/OFF regression coverage.

Those commits and test results remain valid evidence for those capabilities.

## What this task did not deliver

It did not deliver the external-dependency vascular imaging foundation requested by the user:

- no ITK 3D denoising;
- no Frangi/Sato vesselness;
- no automatic 3D vessel segmentation and mature ITK post-processing;
- no actual ITK-VTK bridge in the production chain;
- no automatic centerline, radius or tree topology;
- no vtkvmtk adapter or validated 3D skeleton fallback;
- no real enhanced CT/CTA + reference vessel mask validation;
- no real CTA-derived TetGen/MMG production mesh gate.

The existing LIDC-IDRI sample validates DICOM IO and persistence only. The existing automatic tests and `92/92` / `83/83` totals are regression evidence, not proof of the missing vascular geometry capabilities.

## Status rule

- Do not rewrite or delete the historical task.
- Do not archive it as “the external-dependency shell is complete”.
- Do not use physical GUI acceptance of the old workflow to close the missing medical-image foundation.
- New authoritative planning/execution task: `../07-11-vascular-imaging-foundation/`.

The old task may later be closed only under a precise name such as “internal host/data-spine vertical slice”, with this correction retained.
