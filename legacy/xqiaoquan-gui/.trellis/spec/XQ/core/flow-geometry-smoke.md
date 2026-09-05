# L0 flow geometry smoke

The flow geometry smoke is an engineering connectivity check. It proves that a
validated `VesselProfileV1` can cross the solver boundary, execute the real
transient `FlowSolver1D`, and persist one traceable Case/Result bundle. It is not
a patient-specific boundary-condition workflow or an M5 validation claim.

## Scenario: Run and persist one fixed geometry smoke

### 1. Scope / Trigger

- Trigger: code that converts `VesselProfileV1` to solver geometry, runs the
  backend flow smoke, publishes its outputs, or reads and writes its provenance.
- The pure service owns validation, resampling, unit conversion, and solver
  execution. The controller owns project freshness. Shell-A does not expose this
  controller as a GUI entry; later flow modules may call it through their own
  reviewed interface.

### 2. Signatures

```cpp
FlowSmokeProtocolV1 FlowSmokeProtocolV1::engineeringSmoke();
FlowInputAssembler::Result FlowInputAssembler::assemble(
    const VesselProfileV1&, ScaleSlot, const FlowSmokeProtocolV1&);
FlowGeometrySmokeService::Result FlowGeometrySmokeService::run(
    const Request&);

FlowSmokeController::CapturedInput FlowSmokeController::capture(
    const SmokeIntent&) const;
static FlowSmokeController::PreparedCommand FlowSmokeController::compute(
    CapturedInput);
FlowSmokeController::Status FlowSmokeController::commitPrepared(
    PreparedCommand) const;
```

Persistent core fields are `RomSettings::vesselProfileNode`,
`FlowSmokeCaseProvenance`, and `FlowSmokeResultProvenance`. The provenance owns
the protocol stamp, source Profile node/revision, conversion record, station
mapping, and assembler/solver identity; service-layer types are not serialized.

### 3. Contracts

- Input is a valid V1 Profile in patient `LPS`, `mm`, and `mm2`, with explicit
  `ScaleSlot::Organ`. Missing, Micro, or Cell scale is unsupported for this
  smoke and is never guessed.
- `FlowInputAssembler` is the only `mm -> cm` and `mm2 -> cm2` boundary. It
  normalizes the first arc length to zero and linearly resamples to exactly 11
  uniform, endpoint-inclusive stations while recording source sample ids and
  interpolation weights.
- `engineering-smoke-v1` is immutable: length 50-500 mm, source and resampled
  area 50-2000 mm2, 11 stations, `dt=1e-4 s`, 200 steps, 2 cycles, and the
  versioned waveform/fluid/RCR values returned by `engineeringSmoke()`.
- Out-of-envelope input or a solver CFL failure returns a stable failure. No UI,
  controller, or service retries with a different `dt`, step count, station
  count, waveform, or RCR.
- `FlowGeometrySmokeService` calls the production `FlowSolver1D::solve`. Success
  requires 10 segments, non-empty times, consistent finite Q/P/A matrices,
  positive area, convergence, and a finite CFL value.
- Capture runs on the project owner thread and records the exact `XQProject*`,
  lifecycle epoch, Profile payload identity, revision, stale state, ScaleSlot,
  and bound Asset fingerprint. Compute reads only captured values. Commit
  rechecks every guard on the owner thread.
- Case and Result publish through one `ProjectNodeBundleCommand`. Scene parents
  are `Profile -> Case`, `Profile -> Result`, and `Case -> Result`; available
  bound Assets receive the same lineage. Failure and undo remove the complete
  bundle in reverse order.
- Project IO writes smoke provenance only when present and treats it as optional
  when reading legacy projects. It does not change the project schema version
  merely to add optional typed payload fields.
- Persistent case provenance is structurally complete only when
  `protocol.stationCount == stationMap.size()`. The reader rejects a protocol
  header that declares 11 stations while the mapping payload is missing or has
  another count; tests that construct historical smoke data must write all 11
  mappings rather than a protocol-only placeholder.
- Persistent node/provenance labels say `L0 geometry smoke`; the protocol id is
  `engineering-smoke-v1`. Neither may be presented as patient-specific flow.
- `XQStageWidgets` and `StagePanelContext` must not depend on this service or
  controller. The shell's fifth page is Path/Modules; keeping a disabled or
  hidden smoke button is a contract violation, not compatibility.

### 4. Validation & Error Matrix

- wrong contract version/frame/unit, invalid sample/area/arc, or fewer than
  three samples -> `InvalidProfile`; solver is not called and project is unchanged.
- absent/Micro/Cell ScaleSlot -> `UnsupportedScaleForSmoke` or
  `UnsupportedScale`; no output command.
- length or area outside the inclusive envelope -> `GeometryOutOfEnvelope`;
  no parameter changes or retry.
- any protocol field different from V1 -> `InvalidProtocol`.
- solver non-Ok or inconsistent/non-finite/empty result -> `SolverFailed` or
  `InvalidResult`; no Case or Result publication.
- missing/stale/wrong-domain Profile or invalid bound Asset at capture -> the
  corresponding controller status; no worker publication state.
- project pointer/epoch, payload identity, revision, stale state, ScaleSlot,
  source Asset, target id, or target Asset changes before commit ->
  `SourceChanged`; no node, relation, Asset, or undo entry.
- any later bundle spec failure -> reverse rollback and `CommitRejected`.
- persisted station-map count differs from the protocol station count -> project
  load fails explicitly; do not silently synthesize mappings in the reader.

### 5. Good/Base/Bad Cases

- Good: a non-uniform Organ Profile passed through the backend controller becomes
  11 uniform CGS stations, the real transient solver runs, and one undoable
  Case/Result bundle survives save/reopen without adding a Shell-A GUI entry.
- Base: scene-only outputs are valid when no output Assets are requested; all
  three Scene relations and typed provenance are still required.
- Bad: MainWindow exposes a smoke button, computes contour shoelace areas, reads
  an inflow file, creates RCR values, fills `SolverInput`, or silently increases
  steps after failure.

### 6. Tests Required

- `test_flow_input_assembler`: exact conversions, endpoints/uniform spacing,
  interpolation mapping, immutable input, invalid contract matrix, scale matrix,
  and inclusive envelope edges.
- `test_flow_geometry_smoke`: run the real solver at both minimum and maximum
  geometry corners; assert status Ok, 10 segments, 200 time samples, finite
  positive consistent series, provenance, and CFL gate.
- `test_flow_smoke_controller`: atomic Scene/Asset lineage, undo/redo,
  save/reopen, transitive stale, payload/revision/Asset drift, target collision,
  cross-project, close/reopen, assignment ABA, and reverse rollback.
- `test_main_window`: assert the backend controller may still exist in a Flow-ON
  session while the old Flow page/source/protocol/run controls are absent.
- `test_flow_smoke_controller`: remains the production proof for capture,
  compute, guarded commit, provenance and undo; do not move that proof back into
  the Shell-A GUI.
- Run focused controller/project/startup regressions and the full Release
  `ctest` before committing.

### 7. Wrong vs Correct

#### Wrong

```cpp
solverInput.area0 = areasFromContourGroup(group);
solverInput.inletWaveform = parseFlowFile(path);
if (FlowSolver1D::solve(solverInput).status == Status::CflViolation) {
    solverInput.dt /= 10.0;
    solveAgain(solverInput);
}
```

#### Correct

```cpp
auto captured = controller.capture(intent);              // owner thread
auto prepared = FlowSmokeController::compute(captured);  // worker, real solver
auto status = controller.commitPrepared(                 // owner, guarded bundle
    std::move(prepared));
```
