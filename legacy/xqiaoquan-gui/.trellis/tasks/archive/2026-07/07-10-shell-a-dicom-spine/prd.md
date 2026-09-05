# 壳 A：DICOM 数据脊柱

## Goal

复用已安装的 GDCM/ITKIOGDCM、现有 XQ image/asset/source 契约和后台任务基础，把真实 DICOM series 作为一等项目数据导入：影像必须进入 Scene/Asset，可被 headless service 与 GUI 共用，并在保存重开后由同一资产重新取得体素。

## Dependencies

- Parent: `07-10-google-earth-shell-a`。
- Hard dependency: `07-10-shell-a-domain-contracts` 完成并通过 schema/image payload/ScaleSlot gate。
- 本任务完成后才允许端到端 gate 声称“真实 DICOM 数据脊柱”成立。

## Requirements

### R1. 窄型 DICOM reader port

- 公共 reader API 只暴露 XQ 自有 series descriptor、诊断、`XQImageVolume` 和 `XQMemoryImageBufferHandle`/`IVoxelSource`。
- GDCM、ITK image、dictionary、tag 和 exception 只存在于 adapter 私有实现。
- 主程序无 Python 运行时，不调用外部 DICOM 转换脚本。

### R2. Series discovery 与显式选择

- 枚举目录内全部可读 series，至少返回 SeriesInstanceUID、StudyInstanceUID、FrameOfReferenceUID、modality 与切片数。默认安全显示名只能由 modality、尺寸、slice 数和缩略 UID 组成；不得直接显示 `SeriesDescription`、`StudyDescription` 或任意自由文本标签。
- 多 series 目录不得静默选择“第一套”；调用方始终必须传明确 series id。UI 可在单 series 时预选，但 read 调用仍携带 UID。
- 空目录、非 DICOM、混合坏文件、重复/不一致实例产生结构化诊断。

### R3. 几何与像素语义

- 使用 GDCM/ITK 的 series 排序和方向语义，输出 LPS/mm 的 dimensions、spacing、origin、direction。
- 保留 study/series/frame UIDs；检测同一 series 内 frame、尺寸、component、方向或 slice spacing 不一致。
- 明确像素 rescale 策略，保证 buffer 与 `rescaleSlope/rescaleIntercept` 不会被下游重复应用。
- 支持壳 A 真实样例所需的常见单分量 CT/MR scalar；未支持的多帧、压缩或彩色格式返回明确 Unsupported，而非错误解码。

### R4. Project/Scene/Asset 落地

- import service/controller 生成 `ScaleSlot::Organ` 的 image node、image metadata payload 和 `AssetKind::Image` 记录。
- DICOM 原始 series 作为 `ExternalSource` 资产登记，保留可重定位 locator、UID 和 content fingerprint；不得把患者姓名、患者 ID 等 PHI 写进诊断或 provenance 摘要。
- Scene 与 AssetRegistry 的提交必须原子、可 undo；失败不得留下孤儿 node、asset 或缓存。
- 运行时像素通过 AssetId 解析到 `IVoxelSource`，不得以 `XQMainWindow::activeImage_` 作为唯一数据真源。

### R5. 保存、重开与失效

- 保存项目后重开，若原始 series 可访问且 UID/fingerprint 匹配，headless consumer 可由 node/asset 获取同一 geometry 和 voxel content。
- source 缺失、UID 改变或 fingerprint 不匹配时，项目仍能报告 metadata，但 voxel acquire 明确失败并给出可重定位诊断；不得静默绑定别的 series。
- VTI 入口保留为兼容旁路，不得因新增 DICOM 删除既有样例能力。

### R6. 测试数据政策

- 自动测试使用仓库内固定、许可确认、allowlist 去标识的最小 DICOM fixture；允许离线生成脚本，但 CTest 运行时不得生成或改写黄金输入，也不依赖网络。
- 手工/验收测试使用通过 `XQ_DICOM_TEST_DATA_ROOT` 配置的授权、去标识、多切片真实 series；公开可再分发数据优先，但不是硬前提。
- 不提交私人患者数据或机构专有 tag dump。

## Acceptance Criteria

- [ ] 单 series 和多 series 枚举结果确定；多 series 未选择时被拒绝。
- [ ] 真实多切片 series 的 LPS geometry、UID、scalar buffer 和 voxel/world round-trip 正确。
- [ ] 导入后 Scene node、AssetRecord、ScaleSlot、payload 和 `IVoxelSource` 可由 headless API 取得。
- [ ] 保存重开后在原 source 可用时重新取得体素；source 缺失/替换时失败明确且不误绑。
- [ ] 坏目录、混合 series、不一致 geometry、unsupported pixel type 均有非 PHI 诊断且零副作用。
- [ ] VTI 回归、project round-trip、asset registry、source interface、GUI build 和 Release full ctest 通过。

## Out of Scope

- DICOMweb、PACS、网络上传或院内认证。
- 全部 transfer syntax、多帧增强 DICOM、彩色/波形/RT 对象。
- 患者图谱配准、自动分割或全身重建。
- 把外部 DICOM 永久复制为 managed canonical archive；本任务先完成可靠 ExternalSource 重开语义。
