# Acceptance Matrix

| Capability | Fast/unit evidence | Real-data evidence | GUI evidence | Cannot count as pass |
| --- | --- | --- | --- | --- |
| Dependency/ABI | compile/link probes | actual backend executes on accepted CTA case | runtime DLL load on target machine | install directory or header presence |
| DICOM | synthetic/curated DICOM edge cases | accepted CTA series read by explicit UID | series selection/import | VTI/NIfTI bypass or wrong first series |
| 3D preprocessing | analytic/synthetic tube response | real CTA vesselness output + frozen parameters | preview optional | per-slice 2D processing |
| Automatic segmentation | degenerate/empty/threshold tests | held-out gold evaluation | run without manual seed | gold mask, Path or Contour as input |
| ITK-VTK bridge | oblique direction round-trip | real image/mask physical-point audit | correct MPR/3D overlay | visual flip/transpose workaround |
| Centerline/radius/tree | synthetic bifurcation and graph validator | real gold-derived metrics | tree/radius display | manual control points or “non-empty output” |
| Surface | closed synthetic topology checks | CTA mask-derived surface quality | rendered surface | imported prebuilt VTP only |
| Volume mesh | synthetic curved tube regression | real CTA-derived surface with zero invalid cells and quality report | mesh inspection | star fan, OFF build or skipped backend |
| Persistence | small deterministic round-trip | real pipeline save/reopen equality | reopen project | keeping results only in MainWindow cache |
| E2E | focused integration tests | CTA -> mask -> tree -> surface/mesh using production chain | user physical-machine acceptance | mock/Noop/test-only orchestrator |

## Required negative matrix

| Input/failure | Expected behavior |
| --- | --- |
| directory has multiple series but no explicit selection | reject/require selection; never choose arbitrary first series |
| missing/corrupt DICOM instance | structured diagnostic; no partial Scene mutation |
| image/label geometry mismatch | data gate fails before algorithm scoring |
| vesselness is empty/non-finite | segmentation fails with diagnostics |
| automatic segmentation produces invalid/empty mask | no tree/mesh commit |
| skeleton has cycles/spurs/disconnected fragments beyond policy | validator fails or marks explicit quality failure |
| centerline backend crashes/throws | backend failure isolated; no fabricated tree |
| surface is open/non-manifold outside repair policy | mesher not invoked |
| mesher emits non-positive/inverted/domain-external cells | mesh rejected |
| source revision/fingerprint changes | derived objects marked stale; no silent reuse |
| project save/reopen loses provenance/geometry | E2E fails |

## Completion claim rules

- Unit green + real gate red = incomplete.
- Full CTest green + real gate not run = unverified, not complete.
- Offscreen GUI green + no physical-machine acceptance = physical GUI pending.
- One backend compiled + another backend actually ran = only the executed backend can be claimed.
- Partial work packages are reported individually; parent completion requires every parent AC.
