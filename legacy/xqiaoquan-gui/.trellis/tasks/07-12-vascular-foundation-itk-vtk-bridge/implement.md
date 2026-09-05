# Implementation Plan: ITK-VTK 生产桥

1. Define XQ-owned bridge metadata and geometry validation helpers.
2. Implement the official ITKVtkGlue connection in a private adapter.
3. Make direction propagation explicit and add index/LPS/world numerical probes.
4. Freeze a safe lifetime policy and test adapter output after all local ITK objects are destroyed.
5. Integrate private VTK surface extraction entry without exposing VTK through services/core.
6. Run oblique synthetic geometry and accepted real case alignment checks; record both numeric and GUI-observable results.

Do not create a second image reader or compensate coordinate errors with renderer-only transforms.
