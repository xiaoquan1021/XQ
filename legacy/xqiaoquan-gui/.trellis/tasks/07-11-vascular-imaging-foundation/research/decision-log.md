# Decision Log

## D1 - Current stage

Decision: current product stage is the external-dependency medical image/vascular geometry foundation.

Consequence: work starts with dependency/data gates, then ITK 3D preprocessing/segmentation, bridge, centerline/radius/topology and real meshing. Flow smoke is not the acceptance target.

## D2 - Runtime language and algorithm family

Decision: pure C++ runtime; no embedded Python; no deep-learning segmentation in this stage.

Consequence: TotalSegmentator/nnU-Net may not become runtime dependencies. The accepted automatic path is traditional ITK-based processing.

## D3 - Host applications

Decision: do not introduce 3D Slicer, MITK or CTK as the application shell.

Consequence: reference their behavior/semantics where useful, but keep XQ's existing GUI and architecture.

## D4 - External library boundary

Decision: external libraries are kernels, not architecture.

Consequence: all public contracts are XQ-owned; third-party objects remain private to adapters/visualization.

## D5 - Real-data requirement

Decision: final acceptance requires real enhanced CT/CTA plus reference vessel annotation.

Consequence: synthetic fixtures remain unit tests; current LIDC series remains DICOM IO evidence only.

## D6 - Automation requirement

Decision: production E2E must be fully automatic after series/profile selection.

Consequence: manual Path, Contour, per-case seed and gold-mask input cannot pass the success gate.

## D7 - Centerline strategy

Decision: attempt minimal vtkvmtk C++ integration under a bounded probe, with a real 3D skeleton/distance-map fallback.

Consequence: neither route is pre-declared complete. The installed ITK lacks the planned 3D thinning module, so fallback dependency work is explicit.

## D8 - Old Shell A work

Decision: preserve it as an internal host/data-spine vertical slice, not the completed external-dependency shell.

Consequence: its tests and commits remain valid evidence for what they actually cover; they no longer define completion of the current stage.

## D9 - Downstream science

Decision: credible 1D, Darcy, CTC, ONNX and real multiscale scheduling are out of scope until the geometry foundation passes.

Consequence: downstream consumer smokes may remain regressions but cannot satisfy this parent task.

## D10 - Honesty gate

Decision: no mocks, Noop providers, fake results, skipped real data, after-the-fact thresholds or simulated physical-machine acceptance.

Consequence: unresolved dependency, data, metric or real-machine gates leave the task incomplete and are reported as such.
