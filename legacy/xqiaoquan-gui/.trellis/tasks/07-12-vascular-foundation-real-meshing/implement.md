# Implementation Plan: 真实血管网格

1. Reuse/extend the private VTK surface adapter and define XQ-owned surface provenance.
2. Add closed/manifold/orientation and topology-change validation around smoothing/decimation.
3. Route the real surface through existing `ITetMesher`, TetGen acknowledgement and MMG adapter.
4. Add exact cell validity, outside-cell, marker and quality distribution checks.
5. Execute on accepted real cases in the isolated ON build; keep toy curves only as focused regressions.
6. Reject quality failures without invoking star-fan or test-only fallback.

Do not claim distributable production licensing beyond the recorded research-only boundary.
