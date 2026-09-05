# Implementation Plan: 真实 CTA 数据门

## Steps

1. Verify official source, access path, license and label description for 3D-IRCADb-01.
2. In parallel research at least one fallback public enhanced CT/CTA vessel-label dataset; do not rely on one inaccessible portal.
3. Select the first candidate that passes source/license/privacy/image/gold/space hard gates.
4. Download the original package to an explicit external directory; preserve original bytes and record SHA-256.
5. Safely inspect/extract the package; generate per-file manifest and case/series inventory.
6. Read image series with XQ's production DICOM path and record geometry/identity.
7. Read reference labels through an official C++/ITK adapter or an audited native format path.
8. Verify numerical image-label alignment and write per-case alignment reports.
9. Define gold-separated production/evaluator invocation and fingerprint checks.
10. Freeze case split, metric definitions, numeric thresholds and exclusion/failure reporting before downstream final validation.
11. Run repository/data-leak/PHI scans and document LIDC as IO-only.

## Validation Commands

Final commands depend on the accepted dataset format. They must include real invocations equivalent to:

```powershell
# Inventory and integrity (external root only)
Get-FileHash -Algorithm SHA256 <original-package>

# XQ production DICOM read/import gate
cmake -S XQ -B XQ/build_vascular_data -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DXQ_VASCULAR_TEST_DATA_ROOT=<accepted-dataset-root>
cmake --build XQ/build_vascular_data --config Release
ctest --test-dir XQ/build_vascular_data -C Release --output-on-failure `
  -R "vascular_data|dicom_real|label_alignment"

# Repository safety
git status --short
rg -n -i "patient.?name|patient.?id|institution" XQ/tests .trellis/tasks/07-11-vascular-foundation-data-gate
```

No command output counts unless the actual case IDs, data hashes and geometry report are recorded.

## Planned Deliverables

- `research/candidate-matrix.md`
- `research/selected-dataset.md`
- external `SOURCE.md` / LICENSE / hash manifest
- `research/alignment-report.md`
- `research/validation-baseline.md`
- exact external-root configuration and no-leak evidence

## Risk/Stop Rules

- Do not commit or stage dataset files.
- Do not use Python as the sole reader/converter or product-runtime dependency.
- Do not use gold mask to produce accepted prediction inputs.
- Do not download into `XQ/xq_app_dist/` or existing user data without an isolated directory.
- Do not lower metric thresholds after final output is seen.
- If user credentials/manual license acceptance are required, finish all public research/inventory first, then report the exact blocking step instead of fabricating acquisition.

## Activation/Completion Boundary

This child can start research and official-source verification immediately. It completes only after actual bytes, license, hashes, C++ read, spatial alignment and frozen metrics all exist.
