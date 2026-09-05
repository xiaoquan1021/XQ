# Selected Dataset: 3D-IRCADb-01 case 5

The first accepted real vascular case is `3Dircadb1.5` from the official
3D-IRCADb-01 dataset.

## What is actually available

- 139 enhanced liver CT DICOM slices.
- 16 DICOM mask categories with 139 slices each.
- Primary gold: `portalvein`, 61,774 foreground voxels.
- Secondary gold: `venoussystem`, 102,691 foreground voxels.
- Original patient and mask ZIPs whose full SHA-256 values match a fresh stream
  from the official IRCAD WebDAV.
- CC BY-NC-ND 4.0 license copies and the official anonymization statement.

External root:

```text
D:\XQ\data\vascular\3D-IRCADb-01
```

The repository contains no dataset bytes.

## Reader and identity decision

The original DICOM files omit FrameOfReferenceUID. The production reader now
reads their explicit LPS geometry while retaining an empty frame UID and
emitting `dicom.frame_uid_absent`; it does not invent an identity.

For the persisted XQ shell workflow, `xq_normalize_dicom_frame` writes a
local-only derived copy with one deterministic `2.25` frame UID derived from
the public case key and full LPS geometry. Production re-read proves identical
decoded pixels and geometry. The CT and both vessel masks receive the same UID.

## Accepted role

Case 5 is accepted for:

- real GDCM/ITK CT decode;
- real DICOM voxel-label decode into `XQSegmentationMask`;
- CT/gold physical alignment;
- first held-out image-to-mask/centerline/radius/mesh validation;
- XQ import, save, reopen, and lazy voxel recovery.

It is not training data in the current task and the gold masks are never a
production pipeline input.
