# Design: ITK-VTK 生产桥

## Adapter Shape

`ItkVtkImageBridge` accepts a validated XQ scalar/mask object, constructs a private ITK image, runs `itk::ImageToVTKImageFilter`, applies the authoritative direction matrix to VTK 9.3, and materializes/retains ownership according to one documented policy.

Downstream VTK surface extraction remains in the same private geometry layer. Services receive only XQ metadata and surface handles/contracts.

## Numerical Oracle

For selected indices including corners and foreground samples:

```text
p_lps = origin + direction * (index .* spacing)
```

The VTK world point must match `p_lps` within the frozen tolerance. Identity-direction tests are insufficient.

## Lifetime

The adapter may deep-copy into `vtkImageData` or retain the complete ITK exporter/importer graph in a private owner. Borrowed buffers without an owner are forbidden.
