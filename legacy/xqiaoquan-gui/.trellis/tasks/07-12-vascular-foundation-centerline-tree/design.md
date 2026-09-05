# Design: 自动中心线树

## Public Contract

`ICenterlineExtractor` consumes an XQ mask/surface descriptor and returns `XQCenterlineTreeV1`. No VTK/ITK/vmtk type appears in the interface.

`XQCenterlineTreeV1` owns graph nodes, branches and ordered samples with LPS-mm positions, arc length, radius, classifications, provenance and quality flags. Validators run before any Scene commit.

## Route A

Vendor only the locked minimal vtkvmtk C++ source closure and required notices. Generate endpoint candidates automatically from surface boundary/graph geometry, run the VTK 9.3-compatible centerline path, and materialize maximum-inscribed-sphere radius into the XQ tree.

## Route B

Reuse the locked 3D thinning adapter, ITK distance map and physical-space graph contract delivered by `07-12-shell-a-centerline-b`. Extend its validated single-path result into automatic endpoint discovery and a full adjacency tree, collapse degree-2 chains into branches, classify endpoints/bifurcations, prune with versioned physical rules, and preserve radius sampling from the shared distance map.

## Non-Duplication

Shell A centerline-B is the implementation owner for the minimal A13 thinning/distance single-Path fallback. This child is the v2 production extension and owns automatic endpoints, tree topology, real-data quality and A/B selection. Both use the same fallback adapter and kernel; this child must extend the contract rather than create a second thinning/distance stack.
