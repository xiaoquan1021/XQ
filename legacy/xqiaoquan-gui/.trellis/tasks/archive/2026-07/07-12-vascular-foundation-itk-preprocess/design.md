# Design: ITK 三维预处理

## Contract

`ItkVascularPreprocessor::run(const XQImageVolume&, const IVoxelSource&, const VascularPreprocessProfile&) -> Result` returns an XQ-owned scalar volume plus provenance. The adapter owns all ITK pipeline objects.

## ITK Pipeline

1. Validate XQ image geometry and acquire canonical rescaled scalar buffer.
2. Construct `itk::Image<float,3>` with exact spacing/origin/direction.
3. Apply ITK anisotropic diffusion.
4. Apply ITK multi-scale Hessian filter and Hessian-to-objectness measure configured for bright tubular structures.
5. Materialize the result into an XQ-owned immutable buffer before returning.

The adapter does not normalize away physical intensity semantics silently. Any normalization is an explicit profile stage and appears in provenance.

## Types

- `VascularPreprocessProfileV1`: diffusion and vesselness settings, physical units and profile ID.
- `XQVesselnessVolume`: XQ geometry + float buffer + source/profile fingerprints.
- `VascularPreprocessDiagnostic`: stable status, stage, non-PHI message key and numeric context.

## Memory and Cancellation

The first implementation may be whole-volume synchronous in the service worker, but allocation is overflow checked and cancellation is checked between materialized stages. GUI code never touches ITK objects.
