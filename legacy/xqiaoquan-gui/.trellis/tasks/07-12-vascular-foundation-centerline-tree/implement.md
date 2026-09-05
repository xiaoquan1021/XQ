# Implementation Plan: 自动中心线树

1. Verify the completed Shell A centerline-B adapter/contract and freeze which thinning, distance-map and physical-graph components are reused unchanged.
2. Freeze and validate `XQCenterlineTreeV1` and `ICenterlineExtractor` as compatible extensions of the v1 Path view.
3. Run the vtkvmtk A time-box against the isolated VTK 9.3 build and at least one real surface; record ABI, patch and license results.
4. If A passes, implement the production adapter and automatic endpoint selection. If A fails, record the failure and extend the reused Shell A B path into full tree topology without reimplementing thinning or distance-map kernels.
5. Implement shared graph validation, stable IDs, provenance and Path-view derivation.
6. Evaluate real cases against frozen centerline/radius/topology metrics; do not weaken gates for B.
7. Add persistence-ready round-trip fixtures without using artificial Path as the accepted input.

`07-12-shell-a-centerline-b` is a required v1 predecessor, not a disabled duplicate. Do not start this v2 child until its reusable fallback boundary is available, and do not introduce a second thinning/distance implementation here.
