# Decision Log: Google Earth 壳档 A

## Frozen decisions

1. **Baseline**: implementation will branch from `feat/render-arch` in the `XQIAOQUAN-gui` worktree after planning review.
2. **Scope**: deliver a local patient-space model host, not a completed whole-body multiscale twin.
3. **Geometry authority**:
   - `XQPath` = navigation, resampling and section frame.
   - `XQContourGroup` = cross-sectional measurement evidence.
   - `VesselProfileV1` = the only solver-facing physical vessel geometry.
4. **First real consumer**: shell A includes a fixed-parameter `flow_geometry_smoke` through a pure C++ `VesselProfile -> FlowSolver1D::SolverInput` assembler. It does not satisfy M5 credibility.
5. **Units**: XQ patient geometry remains LPS/mm; conversion to solver CGS is centralized in the assembler and never performed ad hoc in MainWindow.
6. **Capability model**: reuse typed services/controllers/commands and add only thin capability injection sufficient to disable Flow. No dynamic plugins, DLL discovery, hot loading or universal Context.
7. **Operation catalog**: deferred until real Flow/Darcy work exposes duplicated execution/provenance/result-submission logic. Noop registration is not an acceptance gate.
8. **Scale identity**: `ScaleSlot { Organ, Micro, Cell }` must be stored on a node/asset, serialized and read by at least one UI/log consumer. It is distinct from render LOD.
9. **Data provenance**: Image -> Path -> Contour -> VesselProfile -> SmokeResult lineage and stale propagation are mandatory, including algorithm/version/parameters/inputs/frame/units/seed where applicable.
10. **Stop line**: after DICOM, VesselProfile, real consumer, optional Flow, lineage/provenance, persistent ScaleSlot and dual acceptance are green, shell A stops.
11. **DICOM persistence**: shell A uses `ExternalSource + explicit SeriesInstanceUID + fingerprint` and lazily rereads the same series after reopen. `ManagedCanonical` is a later explicit copy/freeze operation, not a shell-A gate or silent fallback.
12. **DICOM intensity**: the canonical runtime buffer contains modality-rescaled values; XQ's pending slope/intercept for that buffer are `1/0`, while original DICOM rescale tags are provenance only.
13. **Scale migration**: `ScaleSlot` has exactly `Organ/Micro/Cell`, stored as an optional node field. Legacy projects with no field remain unspecified rather than being silently classified.
14. **Flow-off proof**: runtime capability injection is the app boundary, and an independent `XQ_ENABLE_FLOW=OFF` configure/build/test is also required. Neither mechanism grows into a plugin registry.
15. **Freshness ownership**: pure assemblers never query Scene stale state. Controller prepare captures payload/revision after a fresh check; commit rechecks existence/revision/stale after background work and discards outdated results.
16. **Project atomicity**: authoritative node, optional asset, node↔asset binding, Scene multi-parent relations and available Asset lineage commit through one narrow project-level batch command with reverse rollback and undo. Runtime caches remain rebuildable derivatives.
17. **Smoke station policy**: `FlowSmokeProtocolV1` uses exactly 11 uniform endpoint-inclusive stations, length 50–500 mm, source/resampled area 50–2000 mm², dt `1e-4 s`, 200 steps and 2 cycles. It never retries or silently changes parameters.
18. **Imported-gold evidence**: every gold Profile references an existing Path and a versioned external evidence id/fingerprint; a gold Asset, when present, becomes an asset-lineage parent. No fake Contour node is created.
19. **Execution scheduling**: the shared worktree runs T1→T2→T3→T4→T5→T6 sequentially. Each child owns a contiguous, non-interleaved commit series and an independent review/revert boundary.

## Explicitly out of scope

- Full-body atlas ingestion or registration.
- ROI registry, ScaleNode tree, semantic zoom or zoom-triggered simulation.
- Complete vascular tree/network extraction.
- Automatic-centerline SOTA or vtkvmtk production integration.
- M5-grade 1D validation, patient-specific BC inference, Darcy, CTC or tumor biology.
- Generic ModuleArtifact property bags, dynamic plugin markets or Python runtime.

## Future extension map (not shell A deliverables)

- M5 1D credibility: `VesselProfileV1`, then `VascularNetwork` when tree work begins.
- Darcy: typed `SpatialField` and coupling contracts.
- CTC: typed `TrajectorySet` and `EventSet`.
- Tissue/tumor: typed `AgentSnapshotSeries`, with PhysiCell/BioFVM hidden behind an adapter/typed port.
- Whole body: atlas base map + sparse circulation template + patient-specific local replacement.
