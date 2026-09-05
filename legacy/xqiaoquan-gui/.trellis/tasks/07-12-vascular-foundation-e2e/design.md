# Design: 真实数据端到端

## Orchestrator

`VascularWorkflowService::prepare(request, cancellation) -> PreparedVascularResult` runs the complete chain and returns immutable XQ-owned outputs. `VascularWorkflowController` captures owner-thread intent, runs preparation off-thread, checks freshness, then submits one atomic command.

## GUI Cutover

The current Segmentation page is replaced, not supplemented, for the normal workflow. Controls are image selection, versioned profile, run/cancel, stage progress, diagnostics and result visibility. Seed picking and threshold/region-grow mode controls are removed from that path.

## Persistence

Mask, centerline tree, surface and mesh each have XQ-owned payload/provenance. Schema changes, if required, are planned explicitly before implementation and preserve old project readability. Runtime ITK/VTK objects are never persisted.

## Atomicity

The command bundle validates node IDs, source revision and lineage before mutation. Failure rolls back the whole bundle. Re-run replaces or versions the prior derived bundle according to one documented rule.
