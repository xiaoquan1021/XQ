# Candidate Matrix

## Accepted primary candidate

| Field | 3D-IRCADb-01 |
| --- | --- |
| Official source | IRCAD landing page and public WebDAV, live-verified 2026-07-12 |
| Image | Contrast-enhanced liver CT, DICOM |
| Gold | Per-category DICOM voxel masks; `portalvein` primary and `venoussystem` secondary |
| Space | Complete per-slice LPS geometry; numerical CT/mask equality verified |
| Identity caveat | Original series omit FrameOfReferenceUID; deterministic local derived copy documented and verified |
| License | CC BY-NC-ND 4.0, non-commercial, no derived redistribution |
| Privacy | Official page states patient images are anonymized |
| Integrity | Full official archive SHA-256, extraction manifest, DICOM preamble and sequence checks |
| Scale | 20 cases exist; case 5 is the first fully accepted case |
| Decision | Accepted for the first real vascular validation vertical slice |

## Researched fallback

Medical Segmentation Decathlon Task08 `HepaticVessel` was considered as a
fallback because it supplies contrast-enhanced liver CT volumes and hepatic
vessel voxel labels in NIfTI. It was not selected for this first gate because
IRCAD provides separate portal/venous labels, DICOM input coverage, and an
official current per-case source already verified byte-for-byte. Task08 remains
a future cross-dataset validation candidate only after its exact official
version, archive digest, license copy, and label semantics are independently
frozen under the same rules.

LIDC-IDRI remains DICOM IO-only and is not a vascular gold candidate.
