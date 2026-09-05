# Offline ROI environment audit

Recorded: 2026-07-13

- No TotalSegmentator ROI asset is currently present under `D:\XQ\data`.
- XQ has no maintained NIfTI mask reader or ROI-prior production contract.
- The locked ITK 5.4 install contains `ITKIONIFTI.cmake` and `ITKNIFTI.cmake`;
  the module is available but not requested by `XQ/CMakeLists.txt`.
- PyPI reports TotalSegmentator `2.15.0`, Apache-2.0, Python `>=3.9` on the audit
  date. The task pins this exact tool version for offline data generation.
- Docker 28.0.4 is running and exposes the NVIDIA runtime. The workstation has
  an RTX 3060 Laptop GPU with 6144 MiB and approximately 400 GB free on D:.
- Recommended generation route: an isolated environment under
  `D:\XQ\research`, never the XQ product/runtime environment. Generated masks,
  version lock, command log and hashes belong beside the external data bundle,
  not in Git.
