# Design: 真实血管网格

## Surface Pipeline

Private VTK geometry implementation: mask image -> surface extraction -> clean/triangle -> orientation/manifold checks -> windowed-sinc smoothing -> optional decimation -> XQ surface materialization.

Every transform is identity in the canonical LPS-mm space; renderer-only alignment fixes are forbidden.

## Volume Pipeline

Use the existing `ITetMesher` boundary. For the current research shell, TetGen 1.5 requires explicit acknowledgement, followed by MMG quality improvement. A future compliant backend can replace it behind the same interface.

## Quality Gate

Validation computes exact cell validity plus distribution metrics. The service rejects the whole result when hard validity or frozen quality gates fail. No mesh is committed merely because a backend returned success.
