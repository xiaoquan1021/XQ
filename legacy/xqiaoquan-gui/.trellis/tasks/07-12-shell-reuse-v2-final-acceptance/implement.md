# Implementation Plan: 完整复用壳 v2 最终验收

1. Verify both parent dependency lines have no unresolved functional gaps.
2. Freeze executable, config, case/profile and evaluator fingerprints.
3. Run the written physical-machine script once without developer-only shortcuts.
4. Record observable functionality and all failed/partial cases.
5. Reproduce save/reopen and one failure/cancel path.
6. Run purpose-bound regression suites after the functional run, only to detect breakage.
7. Present the application to the user for final physical-machine judgment.
8. Complete the child and umbrella only after explicit acceptance.
