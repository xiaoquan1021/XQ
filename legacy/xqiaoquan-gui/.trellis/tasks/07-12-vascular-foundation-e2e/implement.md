# Implementation Plan: 真实数据端到端

1. Freeze workflow request/result, persistence and atomic command contracts.
2. Compose the production services in dependency order with cancellation and typed stage diagnostics.
3. Add owner-thread controller capture, worker preparation, freshness check and single commit.
4. Replace the Segmentation page's normal threshold/seed UI with the automatic workflow controls and real result previews.
5. Add save/release/reopen handling for every new XQ-owned result and lineage edge.
6. Execute the accepted real case through headless and GUI paths and compare fingerprints/results.
7. Exercise named failure stages and prove no half-state/duplicate nodes.
8. Run existing compatibility regressions only after the product workflow is observable and correct.

Do not keep the old primitive workflow as an equal tab/mode. Do not add a test-only shortcut around expensive stages.
