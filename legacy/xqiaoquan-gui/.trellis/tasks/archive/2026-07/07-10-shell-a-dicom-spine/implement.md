# Implementation Plan：DICOM 数据脊柱

## Preflight

- 确认 `shell-a-domain-contracts` 已完成、独立检查通过并提交。
- 确认 ITK 5.4、GDCM 3.0.10、ITKIOGDCM 已在当前依赖前缀可解析；不得新增 DCMTK。
- 固定仓库内小型静态去标识 fixture 的许可/来源说明，并准备一套由 `XQ_DICOM_TEST_DATA_ROOT` 指向的授权去标识真实 series（公开可再分发数据优先）；自动测试不得联网下载或运行时改写黄金输入。
- 先运行 `test_image_volume`、`test_vtk_image_adapter`、project/asset/source 基线。

## Steps

1. **定义 XQ-owned reader port 和诊断**
   - 增加 series descriptor、discover/read result 与稳定错误枚举。
   - 写 fake reader 测试，先冻结多 series 选择与失败语义。

2. **实现 GDCM/ITK adapter**
   - 隔离 ITK/GDCM include 到 adapter translation unit。
   - 实现 series discovery、显式 UID read、geometry/scalar 转换和异常映射。
   - 显式按 IOP/IPP 投影验证/排序，覆盖重复位置、均匀层距、Frame/方向/尺寸一致性，再验证 LPS direction 与 intensity policy。

3. **实现 import service 与原子 command**
   - 生成 image metadata payload、Organ ScaleSlot、ExternalSource AssetRecord、fingerprint 和 resident source。
   - 通过一个可逆 command 完成 asset/node/binding/resource 注册。
   - 添加 duplicate id、registry failure、scene failure 的 rollback 测试。

4. **实现 AssetId-based image resource resolve**
   - 首次 acquire 依据 locator + UID 重新读取并核对版本化有序实例内容 fingerprint，后续使用缓存。
   - source missing、UID/fingerprint mismatch 返回明确状态。
   - 让 headless 测试在无 MainWindow 情况下取得 `IVoxelSource`。

5. **持久化与回归**
   - 保存/重开 image payload、asset locator/UID/fingerprint/scale。
   - 保留 VTI open path 和现有 SV fixture。
   - 为 UI 端只提供 controller/service hook；完整 GUI 演示留给 e2e child。

## Focused validation

```powershell
cmake --build XQ/build_gui --config Release --target test_dicom_series_adapter test_dicom_import_integration test_image_volume test_asset_registry test_project_roundtrip test_mapped_voxel_source test_vtk_image_adapter
ctest --test-dir XQ/build_gui -C Release --output-on-failure -R "dicom|image_volume|asset_registry|project_roundtrip|mapped_voxel_source|vtk_image_adapter|arch_boundaries"
```

使用真实去标识数据时重新配置 build directory：

```powershell
cmake -S XQ -B XQ/build_gui -DXQ_DICOM_TEST_DATA_ROOT='C:\path\to\public-anonymized-series'
cmake --build XQ/build_gui --config Release --target test_dicom_real_series
ctest --test-dir XQ/build_gui -C Release --output-on-failure -R "dicom_real_series"
```

## Final validation

```powershell
cmd /c XQ\build_gui_wt.bat
ctest --test-dir XQ/build_gui -C Release --output-on-failure
```

## Review gates

- 多 series 不得隐式选第一套。
- direction 非恒等时不得默认为 identity 或轴对齐重采样。
- 不得在日志、诊断、fixture 名称中泄漏 PHI。
- 不得让 app/service 公共 API 出现 ITK/GDCM 类型。
- 不得以窗口 active cache 代替 Scene/Asset/IVoxelSource 验收。

## Rollback points

- Commit A：reader port + fake tests。
- Commit B：GDCM/ITK adapter + generated fixture tests。
- Commit C：import transaction + asset/source integration。
- Commit D：reopen/lazy resolve + real-series gate。
- VTI 入口在 DICOM e2e 全绿前保持原状，可作为故障回退旁路。
