# Dependency and Data Gates

## 1. Dependency truth levels

Every external capability is classified separately:

1. Installed: files exist on disk.
2. Configurable: CMake can find the intended exact version/modules.
3. Linkable: the target compiles/links with the approved ABI.
4. Executed: a test proves the external algorithm ran, not a fallback branch.
5. Production accepted: it runs on the real vascular dataset and passes quantitative gates.

Only level 5 counts as the corresponding parent-task capability complete.

## 2. ABI/build audit checklist

- [ ] Exact source/release revision and checksum for every dependency.
- [ ] x64 architecture.
- [ ] Same MSVC toolset.
- [ ] Same C++ language level.
- [ ] Same CRT (`/MD` for Release; no mixed runtime).
- [ ] No Debug/Release library mixing.
- [ ] VTK 9.3.0 exact package for XQ, ITKVtkGlue and vtkvmtk.
- [ ] ITK components are explicit and sufficient for GDCM IO, vesselness, morphology, distance map and bridge.
- [ ] Runtime DLL deployment and CTest PATH are deterministic.
- [ ] Optional backend build switches do not silently select the legacy fallback.
- [ ] Public headers contain no third-party types.

## 3. Current dependency-specific gates

### ITK

Use existing ITK 5.4.0 if the required modules build and execute. Required capability probes include:

- 3D anisotropic diffusion.
- Multi-scale Hessian objectness/vesselness.
- Connected/relabel components and morphology.
- Signed distance map.
- ITKVtkGlue bridge.
- A true 3D skeleton solution if fallback B is selected.

The last item is currently absent from the installed headers and must not be waved through.

### vtkvmtk

Current status: absent.

Gate:

- pin minimal C++ source revision and license;
- build against VTK 9.3.0 without Python/SuperBuild;
- apply/document the known VTK 9.3 centerline compatibility fix if applicable;
- execute on one real closed vascular surface;
- validate centerline/radius/tree output and memory stability.

### TetGen

Current status: vendored implementation exists; header reports 1.5, CMake calls it 1.5.1, bundled LICENSE is AGPLv3.

Gate:

- identify exact upstream source and local modifications;
- reconcile version/license statements;
- decide whether research use and intended distribution comply;
- keep build optional and backend replaceable;
- do not call the backend production-complete until a real surface gate passes.

### MMG

Current status: 5.3.9 installed and adapter exists.

Gate:

- confirm headers/library/runtime DLL belong to the same build;
- execute after the approved fill backend;
- verify parameter sensitivity and quality improvement/non-regression on real vessels.

## 4. Data gate requirements

Minimum accepted validation data:

- real human contrast-enhanced CT/CTA or appropriate venous-phase CT;
- 3D volume, not screenshots or pre-rendered mesh only;
- reference vascular segmentation with documented label meaning;
- sufficient physical metadata to align image and label in LPS mm;
- public/de-identified source, explicit research license and immutable hashes;
- at least one held-out case for final reporting where dataset size permits.

Candidate only, not yet accepted:

- `3D-IRCADb-01`: commonly described as 20 contrast-enhanced liver CT cases with portal/hepatic vessel labels and non-commercial research restrictions. Before adoption, the data-gate child must verify the official source, exact file formats, label definitions, license text, de-identification and physical transform on the actually downloaded package.

The candidate name is not completion evidence.

## 5. Gold separation protocol

To prevent leakage:

- production binary receives only image DICOM + versioned algorithm profile;
- reference mask path is passed only to a separate evaluator after production output is finalized;
- evaluator logs input/output fingerprints;
- code review checks that segmentation and centerline services have no gold-mask parameter;
- final E2E deletes/hides the gold path during production execution and evaluates afterward.

## 6. Metric freeze protocol

Before final implementation tuning, write and review:

- case split and any excluded cases with reasons;
- Dice plus one boundary metric (e.g. HD95/ASSD);
- a centerline-aware metric (e.g. clDice or explicit overlap/coverage);
- radius error definition and sampling correspondence;
- topology/branch metric appropriate to label semantics;
- runtime/memory reporting method;
- mesh validity and quality thresholds.

Do not invent universal numeric cutoffs before inspecting label resolution and semantics. Do not choose cutoffs after seeing final held-out results.

## 7. Why current LIDC data is insufficient

The current LIDC series has excellent provenance and proves DICOM IO semantics. It lacks the reference blood-vessel mask and is not selected as the contrast-enhanced vascular target. Therefore it cannot answer:

- vesselness sensitivity;
- segmentation Dice/boundary quality;
- centerline continuity or radius accuracy;
- tree topology accuracy;
- real vessel mesh quality.

Using it for those claims would be a false test.
