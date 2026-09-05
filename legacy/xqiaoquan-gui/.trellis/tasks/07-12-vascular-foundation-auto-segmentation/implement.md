# Implementation Plan: 自动三维分割

1. Freeze profile and result contracts from the selected dataset semantics.
2. Implement ITK threshold, morphology, component labeling/relabeling/statistics behind one adapter.
3. Implement XQ component-policy scoring over ITK-produced component records; retain multiple justified branches.
4. Add optional ITK level-set refinement only after the deterministic baseline is observable.
5. Add gold-separated evaluator and failure accounting using the frozen metric specification.
6. Run all frozen cases with one profile and record failures and metrics before any threshold change.
7. Prove the automatic service call graph does not reach `SegmentationService::threshold`, `regionGrow` or `keepLargestConnectedComponent`.

No GUI redesign or centerline logic belongs in this child.
