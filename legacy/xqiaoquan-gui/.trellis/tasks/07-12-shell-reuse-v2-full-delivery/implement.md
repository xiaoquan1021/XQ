# Implementation Plan: 完整复用壳 v2 总交付

## Ordered Delivery

1. Close `07-11-vascular-foundation-dependency-remediation`; preserve its exact isolated Qt/VTK/ITK/GDCM baseline.
2. Complete `07-11-vascular-foundation-data-gate` with actual enhanced CT/CTA and reference mask bytes, not a candidate list.
3. Complete `07-12-shell-a-v1-alignment`, including its minimal A13 centerline-B fallback, without waiting for or claiming v2 tree quality.
4. Implement and check `07-12-vascular-foundation-itk-preprocess`.
5. Implement and check `07-12-vascular-foundation-auto-segmentation`; remove the old primitive chain from the production route.
6. Implement and check `07-12-vascular-foundation-itk-vtk-bridge`.
7. Implement and check `07-12-vascular-foundation-centerline-tree`; time-box vtkvmtk A, and if A fails extend the already delivered Shell A B fallback without duplicating its thinning/distance kernel.
8. Implement and check `07-12-vascular-foundation-real-meshing` on the same real cases.
9. Implement `07-12-vascular-foundation-e2e`: one service chain, GUI cutover, atomic Scene commit, save/reopen.
10. Run `07-12-shell-reuse-v2-final-acceptance`; wait for user physical-machine acceptance before completion.

## Planning Gates

- Every child has `prd.md`, `design.md`, `implement.md` and curated context before activation.
- Numeric quality thresholds are frozen by the data gate before final held-out output is inspected.
- Any replacement of a listed mature dependency needs a written technical or license failure report.
- Any schema change is owned by the E2E child and planned before code.
- No v2 child may block, supersede or silently broaden the Shell A A1–A15 completion definition.

## Verification Discipline

Verification is tied to a product question: real input enters, a real external backend executes, the expected XQ object appears, and failure leaves no half state. Focused tests are added only to protect those contracts. Full suites run at integration boundaries, not repeatedly for a green count.

## Forbidden Actions

- Do not use Claude, Claude provider or Claude channel workers.
- Do not use Mock/Noop/artificial Path/gold mask as the production success path.
- Do not retain threshold/seed segmentation as the default GUI workflow after cutover.
- Do not expose third-party types outside adapters/private implementations.
- Do not use Python runtime, MITK/Slicer/CTK whole applications or vmtk SuperBuild.
- Do not clean/reset the dirty workspace, stage broadly, commit or archive without explicit scope review.
