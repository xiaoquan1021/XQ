# Alignment Report: 3Dircadb1.5

## Production C++ path

```text
GdcmItkDicomSeriesReader
  -> ITK ImageSeriesReader<float,3>
  -> XQImageVolume + XQMemoryImageBufferHandle

GdcmItkDicomLabelReader
  -> same production DICOM reader
  -> frozen 0/255 binary interpretation
  -> XQSegmentationMask
```

There is no second DICOM enumerator or decoder in the label path.

## Normalized shared identity

```text
FrameOfReferenceUID: 2.25.195508161051123547813769883276794292485
CT SeriesInstanceUID:
1.2.826.0.1.3680043.2.1125.3714849404296577478106961719918593568
portalvein SeriesInstanceUID:
1.2.826.0.1.3680043.2.1125.1946307859025252973366516009933640145
venoussystem SeriesInstanceUID:
1.2.826.0.1.3680043.2.1125.8014296657898619511795927277332923706
```

## Geometry

CT, portal vein, and venous system are equal under
`sameImageGeometry`:

```text
dimensions: 512, 512, 139
spacing mm: 0.78200000524520896, 0.78200000524520896, 1.600000023841855
origin LPS mm: 0, 0, 0
direction:
  1, 0, 0
  0, 1, 0
  0, 0, 1
bounds LPS mm:
  x [0, 399.60200268030178]
  y [0, 399.60200268030178]
  z [0, 220.80000329017599]
```

Index-to-LPS samples round-trip exactly:

| Index | LPS mm |
| --- | --- |
| `(0, 0, 0)` | `(0, 0, 0)` |
| `(255.5, 255.5, 69)` | `(199.80100134015089, 199.80100134015089, 110.400001645088)` |
| `(511, 511, 138)` | `(399.60200268030178, 399.60200268030178, 220.80000329017599)` |

No resampling or transform is used.

## Content evidence

```text
CT float buffer SHA-256:
73feff65c1bf9b41a938d09d4c8a55667f98b89a0204ad2f62e15792b0197e60

portalvein XQ binary mask SHA-256:
ac2f83f2e0da67a5126649606024efbc0a9b5830defe0c3d5d888ef2dc075944
foreground voxels: 61,774

venoussystem XQ binary mask SHA-256:
278f815ae3bce7d6211054c48109b815359c46ac5e68cfaa724aeddfa6fa73fa
foreground voxels: 102,691
```

The original and normalized decoded CT/mask buffers are identical. Only the
derived DICOM container bytes and series fingerprints change because tag
`(0020,0052)` is inserted.
