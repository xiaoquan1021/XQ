# Task Topology

## Existing work retained

- `07-12-shell-a-v1-alignment`: host/canonical/Path-module/scale baseline.
- `07-12-shell-a-centerline-b`: minimal A13 thinning + physical-distance single-Path fallback owned by and to be delivered on the Shell A v1 track.
- `07-11-vascular-imaging-foundation`: external imaging and geometry production foundation.
- Historical completed/active children remain in place; no reparenting or rewritten history.

## New production children under vascular foundation

1. `07-12-vascular-foundation-itk-preprocess`
2. `07-12-vascular-foundation-auto-segmentation`
3. `07-12-vascular-foundation-itk-vtk-bridge`
4. `07-12-vascular-foundation-centerline-tree`
5. `07-12-vascular-foundation-real-meshing`
6. `07-12-vascular-foundation-e2e`

## Non-duplication rule

`07-12-shell-a-centerline-b` owns the reusable v1 fallback implementation. `07-12-vascular-foundation-centerline-tree` depends on it and owns the post-Shell-A production extension: automatic endpoints, full tree topology, real-data quality and A/B backend selection. It must reuse the fallback adapter/kernel rather than create a second thinning/distance implementation.

## Final gate

`07-12-shell-reuse-v2-final-acceptance` depends on both existing parent lines and the vascular E2E. It is the only task allowed to claim complete v2 shell delivery, and only after user physical-machine acceptance. This post-Shell-A gate cannot delay or redefine Shell A v1 acceptance.
