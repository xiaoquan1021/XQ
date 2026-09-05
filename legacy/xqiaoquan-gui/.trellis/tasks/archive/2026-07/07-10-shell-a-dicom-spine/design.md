# Design：DICOM 数据脊柱

## Data flow

```text
DICOM directory
  -> IDicomSeriesReader::discover()
  -> selected SeriesInstanceUID
  -> GdcmItkDicomSeriesReader::read()
  -> XQImageVolume + XQMemoryImageBufferHandle
  -> ResidentVoxelSource
  -> DicomImportService prepared transaction
  -> Image node + ExternalSource AssetRecord + resource binding
  -> save/reopen
  -> ImageResourceResolver(assetId)
  -> same reader/UID -> IVoxelSource
```

## Reader port

在 XQ-owned header 中定义窄型 port：

- `discover(directory) -> descriptors + diagnostics`；
- `read(directory, seriesUid) -> metadata + shared buffer + diagnostics`。

descriptor 不暴露 tag map；只包含选择所需的 UID、modality、slice count 和由技术字段合成的安全显示名（modality、尺寸、slice 数、缩略 UID），不传递 description/free-text。adapter 用 `itk::GDCMSeriesFileNames`、`itk::GDCMImageIO` 与 `itk::ImageSeriesReader`，并把 ITK exception 映射为稳定状态码。

## Geometry policy

- XQ 输出固定 LPS/mm，直接保留 ITK physical origin/spacing/direction。
- GDCM 可提供候选 file list，但 adapter 必须独立读取 IOP/IPP：以 IOP 定义法向、按 IPP 法向投影排序，并验证重复位置、单调性、均匀层距、方向、尺寸、像素类型和 Frame UID 一致；不能只信任 GDCM 返回顺序。
- direction 必须 finite、非奇异；倾斜 acquisition 保留非恒等 direction，不重采样为轴对齐。
- UID 只作为身份，不作为文件排序替代品。

## Intensity policy

选定一个不可双重应用的规范：buffer 保存 GDCM/ITK 已解码的患者强度值；metadata 中“剩余应用”的 rescale 为 identity。原始 slope/intercept 可记录在非 PHI provenance 字段中用于审计。若 adapter 无法证明解码语义，返回 Unsupported，不猜测。

## Import transaction

`DicomImportService` 只做纯准备，返回：

- image metadata payload；
- caller-supplied stable node/asset id 所需的 AssetRecord 内容；
- resident `IVoxelSource`；
- algorithm/version/source fingerprint 摘要。

GUI/headless controller 在主线程复用 domain child 的 project-level batch command：注册 asset、插入 image node、绑定 asset id。undo 反向移除这些权威项目条目。resident source/cache 是由已提交 asset 可重建的运行时状态，可在成功提交后安装；安装失败返回 residency warning，但不能留下孤儿 asset/node，也不能替代 Project 真源。

## ExternalSource persistence

- `sourceRelPath` 优先相对 `.xqproj`；必要时保留 `sourceAbsPath` 作为显式 fallback。
- 保存 descriptor 的 UID 与版本化 content fingerprint；V1 fingerprint 对验证后有序实例的 `SOPInstanceUID + file-content SHA-256` 清单再求摘要，重开 resolver 必须同时检查所选 series 和内容。
- project payload 保存 geometry/type/window/DICOM identity；asset 保存 locator/fingerprint。
- reader 不在 project load 时强制解码整套 DICOM；首次 voxel acquire 才解析，可在后台运行。

## Runtime resource ownership

新增或扩展 image resource service，使 AssetId 成为缓存 key，返回 `IVoxelSource` handle。它可复用 `ResidentVoxelSource`、现有 pin/lease 语义和 task runner，但不得把资源唯一所有权放回 MainWindow。窗口的 active image 只是一份可丢弃视图缓存。

## Diagnostics and privacy

诊断只包含目录级错误、series UID 的安全缩略、实例计数和技术字段；不打印 patient name/id、accession number、institution 或完整 tag dump。

## Test strategy

- 单元 fixture：仓库内固定、许可确认、allowlist 去标识的小型静态 DICOM，覆盖排序、direction、UID、rescale 和 multi-series；可由脚本离线生成一次并审计，但测试运行时不改写黄金输入。
- adapter integration：通过可配置 data root 读取授权且去标识真实 series（公开可再分发数据优先）。
- project integration：导入、保存、重开、lazy acquire、source missing/fingerprint mismatch。
- VTI 兼容测试保持不变。
