# Implementation Plan: ITK 三维预处理

1. Define XQ-owned profile/result/diagnostic contracts and validators.
2. Add target-specific ITK adapter source using the already locked explicit ITK components.
3. Implement XQ->ITK geometry-preserving import and materialized XQ output.
4. Wire ITK anisotropic diffusion and multi-scale Hessian objectness; record exact parameter provenance.
5. Add focused checks for oblique direction, anisotropic spacing, physical sigma, determinism, cancellation and bad input.
6. Run the adapter on the accepted real data case and record visible vesselness ranges/diagnostics before claiming completion.

Do not add segmentation heuristics or GUI controls in this child. Do not use a custom Hessian/eigen solver as a fallback.
