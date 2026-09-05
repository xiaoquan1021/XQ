# VesselProfile assembly and publication

`VesselProfileV1` is the only solver-facing vessel geometry. `XQPath`
provides navigation and section frames; `XQContourGroup` provides measured
section evidence. Neither source is a second authority for physical area.

## Scenario: Build or import one authoritative VesselProfile

### 1. Scope / Trigger

- Trigger: code that assembles a Profile from Path + ContourGroup, imports a
  controlled gold Profile, publishes a Profile node/asset, or consumes vessel
  geometry for a solver.
- The pure service layer owns geometry/unit validation. The controller owns
  Scene freshness capture and recheck. `ProjectNodeBatchCommand` owns atomic
  publication.

### 2. Signatures

```cpp
VesselProfileAssembler::Result VesselProfileAssembler::assemble(
    const Input&, const Options&);
VesselProfileImporter::Result VesselProfileImporter::importProfile(
    const Request&);

VesselProfileController::CapturedContourInput captureFromContours(
    const ContourIntent&) const;
static PreparedCommand computeFromContours(CapturedContourInput);
VesselProfileController::Status commitPrepared(PreparedCommand) const;
```

The imported-gold path has the same capture/compute/commit split through
`captureImportedGold` and `computeImportedGold`.

### 3. Contracts

- Canonical output is `VesselProfileV1`, patient `LPS`, length `mm`, and area
  `mm2`. It has at least three samples, strictly increasing arc lengths, unique
  stable sample ids, finite Path positions/unit tangents, and positive areas.
- Contour assembly is fail-closed. The Path must be resampled and the
  ContourGroup must declare the same source Path node. Every contour must have
  a valid globally stable id, unique arc position, closed finite planar loop,
  valid frame, and no duplicate point, reversal overlap, self-intersection, or
  degenerate area.
- Project the contour into its own `(xAxis, yAxis)` frame and compute shoelace
  area there. Position and tangent always come from `XQPath::frameAtArcLength`;
  a contour centroid never replaces the centerline. A repeated final point that
  exactly equals the first may be removed from the local copy only.
- Segment collinearity tolerance is a distance tolerance times the tested edge
  length. Do not compare an `mm2` cross product directly with an unscaled `mm`
  tolerance, especially for short edges.
- Samples are ordered by arc length and use `VesselSampleId(contourId.value())`.
  Provenance records algorithm id/version, classic-locale parameter summary,
  and exact Path/Contour `DerivationInputStamp` values.
- Imported gold is a typed boundary, not a second file parser. Accepted unit
  pairs are exactly `mm + mm2` or `cm + cm2`; the latter converts position and
  arc length by `10` and area by `100`. The request must explicitly carry an
  existing Path stamp plus non-empty external evidence id/fingerprint. An
  optional evidence Asset must have the same fingerprint.
- Controller capture runs on the owner thread, rejects missing/type-invalid or
  stale sources, clones immutable XQ payload values, and records source
  revision, payload identity, ScaleSlot, asset id/kind/fingerprint, originating
  `XQProject*`, and `lifecycleEpoch()`.
- Pure compute receives only captured values and never reads Scene or Registry.
  Commit rechecks project pointer/epoch, project-open state, source identity,
  revision/stale/ScaleSlot/asset state, and an existing target Asset's id,
  category, kind, and fingerprint.
- Publication is one `ProjectNodeBatchCommand`: optional Profile Asset
  registration, Profile node, binding, every Scene parent, and available Asset
  lineage succeed together or all roll back. Invalid output ScaleSlot is
  rejected by the batch command as a final defensive check.
- Explicit output ScaleSlot wins. Otherwise it propagates only when every
  captured Scene source has the same explicit valid value; absent or conflicting
  sources produce an absent Profile ScaleSlot.
- New solver code consumes `VesselProfileV1`; it must not reconstruct physical
  area directly from ContourGroup or add authoritative area/radius to Path.

### 4. Validation & Error Matrix

- invalid options/source stamps, unresampled Path, wrong ContourGroup binding,
  fewer than three contours, bad frame/plane, duplicate id/arc/point,
  self-intersection, or degenerate area -> non-Ok assembler result and no Profile.
- unsupported gold version, non-LPS frame, mixed/unknown unit pair, invalid Path
  stamp, missing evidence identity, or invalid sample -> specific importer status
  or shared validator issues; do not collapse all failures to a generic status.
- null/closed project, bad output intent, missing/stale/wrong-domain source, or
  invalid source Asset -> capture fails with no command.
- source/target Asset mutation, semantic source edit, project close/reopen,
  project assignment, or commit through a different project object after
  capture -> `SourceChanged`, no node/asset/relation, and no undo entry.
- command collision or downstream batch validation failure -> `CommitRejected`;
  Scene, Registry, relations, and prior redo state remain atomic.

### 5. Good/Base/Bad Cases

- Good: three measured contours produce deterministic LPS/mm/mm2 samples and
  one Profile linked to both Path and ContourGroup, with exact undo/redo.
- Base: imported gold in explicit centimeters converts once and publishes a
  Path-derived Profile without inventing a fake Contour node.
- Bad: a worker reads live Scene, silently drops a bad contour, uses contour
  centroid as Profile position, guesses units/ScaleSlot, or publishes parents in
  separate commands.

### 6. Tests Required

- `test_vessel_profile_assembler`: deterministic success plus every contour,
  frame, tolerance, ordering, provenance, and validator failure class.
- `test_vessel_profile_import`: mm/cm conversions and version/frame/unit/source/
  evidence/sample error matrix.
- `test_vessel_profile_controller`: capture/commit, target/source Asset guards,
  atomic failure, imported lineage, ScaleSlot, cross-project, close/reopen, and
  assignment epoch ABA rejection.
- `test_project_node_batch_command`: exact multi-parent publication, invalid
  ScaleSlot, rollback, undo/redo, and retryable failed redo.
- `test_main_window`: contour edits invalidate prepared Profile work and do not
  duplicate assembler/unit logic in the UI.
- Final gate: focused Profile/controller/core tests, `test_app_startup`, and full
  Release `ctest`.

### 7. Wrong vs Correct

#### Wrong

```cpp
group.addContour(contour);                 // in-place Scene payload mutation
scene.link_derived(pathId, profileId);     // partial publication
solver.area0.push_back(areaFrom(group));   // Contour becomes solver authority
```

#### Correct

```cpp
auto captured = controller.captureFromContours(intent); // owner thread
auto prepared = VesselProfileController::computeFromContours(
    std::move(captured));                              // worker-safe values
return controller.commitPrepared(std::move(prepared)); // guarded atomic batch
```

## Scenario: GUI-created ContourGroup preserves Path lineage

### 1. Scope / Trigger

- Trigger: a GUI/workbench action creates an in-memory `XQContourGroupPayload`
  from a selected `Path` instead of importing a pre-existing contour asset.
- The payload-level `sourcePathNode` and the Scene derivation graph are separate
  contracts. Both must be established so stale propagation, save/reopen, and
  `VesselProfileController` observe the same source identity.

### 2. Signatures

```cpp
void XQMainWindow::createContourGroupFromPicker();
void XQContourGroup::setSourcePathNode(const NodeId& node);
void XQDataNode::setScaleSlot(ScaleSlot slot);
AddNodeWithSourceRelationCommand(
    XQScene* scene, XQDataNode node, const NodeId& source,
    std::string label = "Add node with source");
```

### 3. Contracts

- The selected source must still exist and have domain `Path` when the command
  is prepared. The new node has domain `ContourGroup`, and its payload's group
  id and `sourcePathNode` match the new node id and selected Path id.
- Creation is submitted through the session command stack as one
  `AddNodeWithSourceRelationCommand`. A successful command inserts the node and
  the `Path -> ContourGroup` Scene relation together, creates one undo item, and
  clears incompatible redo history.
- If the Path has an explicit `ScaleSlot`, copy that exact slot to the new
  ContourGroup node. If the Path has no slot, leave the new node slot absent; do
  not guess `Organ` or another scale in the GUI.
- Update `activeContourGroup_` and refresh/bind workbench UI only after the
  command succeeds. Payload construction alone is never treated as publication.
- The Scene relation is authoritative for dependency traversal and transitive
  stale propagation. `XQContourGroup::sourcePathNode()` remains the typed payload
  binding required by the assembler; neither representation substitutes for the
  other.

### 4. Validation & Error Matrix

- null Scene/session, no picker selection, missing Path, or wrong source domain
  -> return without node, relation, undo entry, or active-group change.
- invalid/colliding output id -> command rejection; no partial Scene mutation.
- source disappears or relation insertion fails during command execution -> the
  inserted node is rolled back; no relation and no undo entry remain.
- Path has no `ScaleSlot` -> creation may succeed with an absent ContourGroup
  slot; silently inventing a slot is invalid.

### 5. Good/Base/Bad Cases

- Good: an Organ-scale Path creates an Organ-scale ContourGroup with matching
  payload binding, one Scene parent, and exact undo/redo behavior.
- Base: a legacy Path without a ScaleSlot creates a related ContourGroup whose
  slot is also absent.
- Bad: call `AddNodeCommand` or `scene.insert` after setting only
  `group.setSourcePathNode(pathId)`; the UI looks populated, but dependency and
  persistence consumers see an orphan ContourGroup.

### 6. Tests Required

- `test_main_window`: select a real Path node through the workbench control,
  click create, and assert payload source id, `Path -> ContourGroup` relation,
  inherited/absent ScaleSlot, active UI state, and redo clearing.
- `test_command_stack`: source missing or relation failure rolls back the node
  and creates no undo item; undo removes both node and relation.
- Profile/round-trip gates: a GUI-created ContourGroup remains a Path child after
  save/reopen and can be captured by `VesselProfileController` without repairing
  lineage in the test.

### 7. Wrong vs Correct

#### Wrong

```cpp
group.setSourcePathNode(pathId);
session.pushCommand(std::make_unique<AddNodeCommand>(scene, node));
// Payload says "Path", but Scene lineage is missing.
```

#### Correct

```cpp
group.setSourcePathNode(pathId);
if (pathNode->hasScaleSlot()) {
    node.setScaleSlot(pathNode->scaleSlot().value());
}
session.pushCommand(std::make_unique<AddNodeWithSourceRelationCommand>(
    scene, std::move(node), pathId, "Add contour group"));
```
