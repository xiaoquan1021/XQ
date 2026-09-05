# Design: 真实 CTA 数据门

## 1. Data Bundle Contract

数据位于 Git 外部目录，建议结构：

```text
<XQ_VASCULAR_TEST_DATA_ROOT>/
  dataset.json
  SOURCE.md
  LICENSE/
  original/
  extracted/
  labels/
  derived/              # optional deterministic conversions
  manifest.sha256
  validation-baseline.md
  reports/
```

`dataset.json` 只存公开/技术字段，不存患者自由文本：

- dataset/version/case IDs
- modality/site/contrast phase
- image series identifiers
- label definitions and file mapping
- coordinate/transform description
- source/archive hashes
- license class and use restrictions

## 2. Candidate Evaluation Matrix

每个候选按同一矩阵打分，任何硬门失败即拒绝：

| Dimension | Hard requirement |
| --- | --- |
| Source | official, stable, actually accessible |
| Image | 3D enhanced CT/CTA suitable for target vessels |
| Gold | voxel reference vessel mask with documented semantics |
| Space | recoverable physical geometry and deterministic alignment |
| License | research use allowed; restrictions recorded |
| Privacy | de-identified/public basis documented |
| Integrity | immutable package/file hashes possible |
| Scale | enough cases for tune/validation or honest limitation |

## 3. Reader Path

- DICOM images use the existing production `GdcmItkDicomSeriesReader` and XQ import/source path.
- Non-DICOM reference labels require a narrow C++ adapter using ITK official IO, returning `XQSegmentationMask` with exact geometry.
- If labels are DICOM SEG/RTSTRUCT or another format, select the appropriate official C++ reader; no Python conversion is accepted as the only geometry proof.
- Any optional offline conversion preserves the original package and is checked against the native reader/transform.

## 4. Spatial Alignment Report

For every accepted case, record:

- image and label dimensions
- spacing/origin/direction
- image Series/Frame IDs when available
- label transform/source metadata
- physical bounding boxes
- selected foreground voxel index -> LPS samples
- overlay slice checks as secondary visual evidence

Numerical mapping is authoritative; screenshots are supporting evidence only.

## 5. Gold Separation

```text
ProductionRunner(image_root, profile) -> prediction + fingerprint
Evaluator(prediction, gold_root, metric_spec) -> metrics
```

The production runner binary/API has no gold parameter. The evaluator checks prediction fingerprint, gold fingerprint and frozen metric-spec fingerprint. This prevents accidental gold leakage and after-the-fact metric changes.

## 6. Metric Baseline

Metric definitions are selected only after confirming label semantics:

- Segmentation: Dice plus HD95/ASSD and a centerline-aware connectivity metric.
- Centerline: coverage/overlap, disconnected fragment count, endpoint/branch evaluation where gold permits.
- Radius: distance-transform or annotated surface correspondence in mm, with resolution-aware tolerance.
- Topology: branch/endpoint graph score only if reference labels support stable topology extraction; otherwise report the limitation and freeze a reproducible proxy.
- Mesh: validity is exact (zero inverted/non-positive cells); quality threshold based on accepted real surface and downstream needs.

Numeric thresholds are written before final held-out execution, not in this design document.

## 7. Data Safety

- No patient names/free-text tags in logs.
- No re-identification attempts.
- No restricted data copied into repo/build artifacts/test result attachments.
- Derived reports identify public case IDs and hashes only.
- Removal/cleanup commands are scoped to the explicit external dataset directory and never the workspace root.

## 8. Failure Outcomes

| Failure | Outcome |
| --- | --- |
| official download unavailable | candidate rejected/blocked; try next candidate |
| license ambiguous | do not use; obtain official clarification or select another dataset |
| image/label space mismatch without authoritative transform | reject candidate |
| label semantics unsuitable for target vessels | may remain auxiliary data, not primary gate |
| too few cases | document limitation and freeze cross-validation/held-out alternative |
| only mesh/path available | not accepted as image-to-segmentation gold gate |
