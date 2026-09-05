# Implementation Plan: 依赖 ABI 与许可审计

## Steps

1. Snapshot manifest, source/install directories, CMake cache and current link configuration.
2. Build a dependency inventory table and flag version/license/ABI contradictions.
3. Inspect ITK module configs and define the minimal vascular component set.
4. Create/run the ITK vascular filters probe in a new isolated Release build tree.
5. Create/run the ITK-VTK oblique geometry bridge probe.
6. Resolve a true 3D skeletonization fallback candidate and compile its minimal probe.
7. Obtain/pin the minimal vtkvmtk C++ source set, verify license/patch provenance, then compile/run its bounded probe.
8. Reconfigure XQ with TetGen/MMG ON in a new build tree; verify actual adapter selection and run kernel/full regressions sequentially.
9. Inspect runtime/link dependencies for forbidden Python/MITK/Slicer/CTK ingress and third-party public-header leakage.
10. Write accepted/research-only/rejected/blocked decisions and exact reproducible commands.

## Validation Commands

Exact paths are frozen in the audit report. Initial commands include:

```powershell
cmake -S XQ -B XQ/build_dependency_audit -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH=<locked-prefixes> `
  -DXQ_ENABLE_TETGEN=ON `
  -DXQ_ENABLE_MMG=ON
cmake --build XQ/build_dependency_audit --config Release
ctest --test-dir XQ/build_dependency_audit -C Release --output-on-failure `
  -R "itk|dicom|tetgen|mmg|arch_boundaries"
ctest --test-dir XQ/build_dependency_audit -C Release --output-on-failure
```

Probe target names are created only after reading the current CMake/spec rules. A command is evidence only when its real output is saved in `research/probe-results.md`.

## Files/Areas at Risk

- `XQ/CMakeLists.txt`
- XQ build scripts and runtime PATH setup
- `Externals/externals.manifest` or a new lock file
- isolated probe sources/CMake
- third-party license/source records

Do not edit application services/GUI/segmentation algorithms in this child.

## Stop/Failure Rules

- Do not overwrite installed VTK/ITK/GDCM/MMG trees.
- Do not download/build vmtk SuperBuild or Python runtime.
- Do not mark a backend accepted on compile-only evidence.
- Do not weaken default tests to make an ON build green.
- If license provenance cannot be established, mark `blocked`/`rejected` and continue auditing alternative paths.

## Completion Record Requirements

- Exact commands and exit status.
- Build tree and cache fingerprint.
- Probe inputs and numerical outputs.
- Accepted/rejected backend table.
- Unresolved risks explicitly listed.
- Clear statement that real CTA algorithm quality is not covered by this child.
