# Current Data Inventory

## Existing accepted IO sample

Path: `D:\XQ\data\dicom\tcia_lidc_idri_0957_ct`

- TCIA LIDC-IDRI / `LIDC-IDRI-0957`
- CT / CHEST
- 65 DICOM instances
- explicit official source, CC BY 3.0, de-identification statement and hashes
- production XQ DICOM read/import/save/reopen evidence

Accepted role: DICOM IO, LPS geometry, persistence and source integrity regression.

Rejected roles:

- vascular segmentation gold
- centerline/radius/topology gold
- real vessel mesh quality gate

Reason: no reference vessel mask and not selected as the enhanced vascular target.

## Other local assets

SimVascular sample projects such as `0007_H_AO_H` contain paths/contours/surfaces/meshes useful for compatibility and dependency probes. They are not a replacement for image + reference vessel segmentation data because their derived products may have been produced manually or by another pipeline.

## Required new data

At least one accepted bundle must add:

- enhanced CT/CTA image volume
- reference vessel voxel mask
- official label semantics
- physical alignment evidence
- source/license/privacy/hash evidence
- enough cases for a frozen validation protocol

## Accepted vascular case (2026-07-12)

`3D-IRCADb-01 / 3Dircadb1.5` is now the first accepted vascular case under:

```text
D:\XQ\data\vascular\3D-IRCADb-01
```

It provides 139 contrast-enhanced CT slices plus 139-slice `portalvein` and
`venoussystem` DICOM masks. Full official WebDAV archive SHA-256 values match
the local read-only packages. All 2,363 extracted DICOM files have a valid
preamble and all 17 series contain the complete `image_0` through `image_138`
sequence.

The original series omit FrameOfReferenceUID. The production reader now keeps
that absence explicit while reading pixels and LPS geometry. A separate
`xq-dicom-derived-frame-v1` copy inserts one deterministic frame UID into CT
and both vessel labels; production re-read proves identical decoded buffers and
geometry. This derived CT completes XQ import/save/reopen/lazy-recovery without
changing the original dataset.

See `selected-dataset.md`, `alignment-report.md`, and
`validation-baseline.md` for the executable contract.
