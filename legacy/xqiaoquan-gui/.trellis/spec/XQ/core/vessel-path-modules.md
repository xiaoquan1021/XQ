# VesselPath snapshot and Path-only module boundary

`VesselProfileV1` remains the only persisted physical vessel geometry authority.
`VesselPathV1` is a rebuilt, immutable-at-consumption snapshot for Shell-A
geometry inspection and static modules; it is not a Scene payload or project
schema.

## Scenario: Rebuild and execute a Path-only module from VesselProfile

### 1. Scope / Trigger

- Trigger: changes to `VesselPathV1`, Profile-to-Path conversion, stable Path
  dumps, `ShellGeometrySmokeService`, `IPathModule`, `PathModuleRegistry`,
  `PathModuleController`, or the shell's replacement Modules-page wiring.
- Purpose: keep radius/source semantics explicit without creating a second
  writable geometry authority or coupling modules to Scene, Contour, Flow, Qt,
  ITK, or VTK.

### 2. Signatures

```cpp
VesselPathValidationResult VesselPathValidator::validate(
    const VesselPathV1& path);
VesselPathSnapshotService::Result VesselPathSnapshotService::build(
    const DerivationInputStamp& sourceProfile,
    const VesselProfileV1& profile);
VesselPathDump::Result VesselPathDump::format(const VesselPathV1& path);
ShellGeometrySmokeService::Result ShellGeometrySmokeService::run(
    const VesselPathV1& path);

virtual PathModuleExecutionResult IPathModule::run(
    const VesselPathV1& path) const = 0;
PathModuleRegistry::RunResult PathModuleRegistry::run(
    const std::string& id,
    const VesselPathV1& path) const;
PathModuleController::Result PathModuleController::run(
    const PathModuleController::Intent& intent) const;
```

### 3. Contracts

- `VesselPathV1` is contract version 1, LPS/mm, equivalent-circular radius,
  non-empty frame of reference, one Profile derivation input, and at least two
  uniquely identified stations with finite position/radius/arc values,
  `radius > 0`, and non-decreasing arc length.
- A derivation input Asset is all-or-nothing: an optional `assetId` must itself
  be valid and must have a non-empty fingerprint; without an AssetId the
  fingerprint must be empty. Checking only `optional.has_value()` is invalid.
- Snapshot construction validates the Profile first, copies station identity,
  position, and arc length, and derives `radiusMm = sqrt(areaMm2 / pi)`.
- Source claims are minimal: all `SegmentationDerived` becomes
  `AutomaticCenterlineB`; all `ImportedGold` becomes `GoldFile`; measured or
  mixed evidence becomes `SemiAutomatic`.
- The stable dump uses classic locale and `max_digits10`. It may contain fixed
  contract fields, frame UID, source Profile stamp, fingerprint, and station
  values; it must not emit external evidence free text, Profile parameter
  summaries, patient names, or free-text DICOM tags.
- Geometry smoke validates first and returns no populated summary on failure.
  Mean radius uses an online mean, not a raw `double`/MSVC `long double` sum,
  so multiple finite near-maximum radii do not overflow the mean.
- The module ABI is exactly `const VesselPathV1&`. The architecture source
  guard rejects Scene, Contour, Flow, Qt, ITK, or VTK types in
  `PathModuleRegistry.h`.
- `Noop` and `PathValidate` are deterministic built-ins. Registry order is
  stable; duplicate/invalid IDs, unknown IDs, invalid Path, and module-declared
  failure return typed status plus stable diagnostics.
- Sources, services, controller, and tests are registered outside
  `XQ_ENABLE_FLOW`. Flow OFF disables solver execution only; Path modules and
  their GUI source/summary/dump remain available.
- The six-stage shell's fifth page is `Modules` in both Flow ON and OFF. It
  replaces the old Flow smoke UI instead of being inserted beside it. Retired
  Flow source/protocol/run controls and capability-state wiring must be absent.
- The existing `VesselProfileController` path is permitted only as a compatibility
  bridge that prepares the persisted geometry source inside the same Modules
  workflow. It must not appear as a separate Flow feature or a third parallel
  execution section.
- Native save auto-derives an Asset for payload-bearing nodes that have none.
  Before binding that Asset, `XQProjectWriter` must set a deterministic
  `xq-payload-v1:sha256:<64-hex>` content fingerprint over the canonical payload
  block plus blob-reference records. A reopened Path input must therefore keep
  strict provenance validation; controllers must not special-case an empty
  writer-generated fingerprint.
- GUI code formats controller output only. It must not recompute radius, source
  kind, validation, or geometry summaries.
- After Path input preparation commits, the Modules source picker selects that
  exact new Profile node. It must not preserve an older source merely because
  the picker was already populated.
- Changing the selected Path input or module invalidates the displayed source,
  geometry summary, and dump immediately. The copy action remains disabled
  until a new successful run produces the currently displayed canonical dump.
- A successful module run enables an explicit copy action for the canonical
  dump and writes a PHI-free application log containing module id, honest Path
  source token, station count, and radius range. Failure clears all prior result
  fields and logs the typed status plus controller diagnostic.

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| bad version/frame/unit/source/station value | validator issue; no implicit repair |
| invalid optional AssetId or fingerprint parity | invalid stamp/input issue; no Path |
| invalid `VesselProfileV1` | `InvalidProfile` with original typed Profile issues |
| post-conversion Path validation fails | `PathValidationFailed`; no snapshot |
| invalid Path passed to dump/smoke/module | typed invalid status; empty dump/summary |
| duplicate/empty module descriptor | registration rejection; registry unchanged |
| unknown module ID | `UnknownModule` plus stable diagnostic |
| module-declared failure | `ModuleFailed`; never converted to success |
| Flow OFF build | same Path services/controller/modules compile and execute |
| Flow ON or OFF shell | page 5 is Modules; old Flow smoke controls absent |
| save unbound Path input | writer derives Asset + stable non-empty fingerprint |
| reopened derived Asset lacks fingerprint | reject as invalid provenance; do not relax controller |
| prepare a second Path input | new output becomes the selected module source |
| change source/module after a successful run | old source/summary/dump cleared; copy disabled |
| successful module run | current dump copyable; PHI-free source summary logged |

### 5. Good/Base/Bad Cases

- Good: a validated imported-gold Profile rebuilds a `GoldFile` Path, exposes a
  stable dump, and runs `path-validate` in a Flow-OFF shell without writing the
  Scene.
- Base: a mixed-evidence Profile is labelled `SemiAutomatic`; repeated builds
  produce byte-identical dumps.
- Base: an unbound Path input is saved, receives a deterministic derived-asset
  fingerprint on the writer's work copy, reopens, and runs PathValidate.
- Bad: persist a second radius payload, let a module accept `XQScene` or
  `XQContourGroup`, label mixed evidence automatic, accept `AssetId(0)`, or hide
  Path modules inside the Flow compile block. Also bad: leave the old Flow smoke
  page/controls disabled beside the new Modules workflow.

### 6. Tests Required

- `test_vessel_path`: complete version/frame/unit/source/stamp/station rejection
  matrix, including invalid optional AssetId.
- `test_vessel_path_snapshot`: exact radius/source/stamp mapping, deterministic
  dump, PHI/free-text exclusions, invalid Profile, and invalid source stamp.
- `test_shell_geometry_smoke`: deterministic counts/length/radius summary,
  zero output on invalid Path, and overflow-safe mean for maximum finite radii.
- `test_path_module_registry`: built-in order/run, invalid Path, duplicate/empty
  registration, unknown ID, and explicit module failure.
- `test_path_module_controller` and `test_main_window`: read-only project flow,
  typed Profile validation propagation, source/dump visibility, Modules page in
  Flow ON/OFF, and absence of all retired Flow smoke controls.
- `test_shell_a_gui`: DICOM -> Path input preparation -> PathValidate ->
  save/reopen -> PathValidate in both build modes, with no
  SimulationCase/FlowResult side effect. The reopened execution is the assertion
  that the writer-derived fingerprint remains acceptable to the strict controller.
  The same real workflow also prepares a second input, asserts the new node is
  selected, invalidates old output on module change, and copies only the current
  successful dump.
- Writer regressions: `test_shell_domain_persistence`,
  `test_asset_metadata_roundtrip`, and `test_payload_roundtrip` remain green
  after auto-derived fingerprints are introduced.
- `test_arch_boundaries`: Path module header rejects Scene/Contour/Flow and
  third-party type leakage.
- Final verification: incremental Release build followed by sequential full
  Flow-ON and Flow-OFF CTest; never run the two suites concurrently.

### 7. Wrong vs Correct

#### Wrong

```cpp
struct ModuleContext {
    XQScene* scene;
    XQContourGroup* contours;
    FlowController* flow;
};
double radius = areaMm2; // GUI/module reinvents the geometry contract.

panel->addWidget(buildFlowPage(...));
panel->addWidget(buildPathModuleSection(...)); // additive, duplicate shell entry

if (asset->contentFingerprint.empty()) {
    input.assetId.reset(); // hides broken persisted provenance
}
```

#### Correct

```cpp
PathModuleExecutionResult run(const VesselPathV1& path) const override;

const auto snapshot = VesselPathSnapshotService::build(profileStamp, profile);
if (!snapshot.ok()) {
    return typedFailure(snapshot.status, snapshot.profileValidation);
}

panel->addWidget(buildModulesPage(...)); // replacement page, same in ON/OFF

record->contentFingerprint = "xq-payload-v1:sha256:"
    + Sha256::hashHex(canonical.data(), canonical.size());
```
