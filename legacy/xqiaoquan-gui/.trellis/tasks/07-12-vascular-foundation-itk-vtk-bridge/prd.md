# 血管地基：ITK-VTK 生产桥

> Parent: `07-11-vascular-imaging-foundation`
>
> Depends on: `07-11-vascular-foundation-dependency-remediation`; consumes the automatic mask contract.

## Goal

实际使用 ITKVtkGlue 官方桥把 ITK 三维 mask/scalar 数据交给 VTK 几何链，并数值证明 dimensions、spacing、origin、完整 direction、LPS/mm、scalar/component 和生命周期守恒。不得用两个独立 reader 或视觉对齐冒充 bridge。

## Requirements

- bridge 实现关在 adapter/private geometry implementation，公共 API 不暴露 `itk::`/`vtk::`。
- 使用 ITKVtkGlue 官方连接方式；若桥本身不传播 direction，必须从权威 ITK geometry 显式设置 VTK 9.3 direction matrix并验证。
- adapter 返回前完成明确 ownership 转移或 materialization；ITK pipeline 销毁后 VTK consumer 仍安全。
- mask scalar 类型、extent、voxel order 和 foreground semantics 不改变。
- 提供 XQ-owned surface/bridge result metadata，记录源 mask fingerprint、transform 和 backend versions。
- 任何 geometry mismatch、unsupported scalar、dangling pipeline 或 empty output 稳定失败。

## Acceptance Criteria

- [ ] AC1：产品 target 链接并执行 ITKVtkGlue，而非 test-only probe。
- [ ] AC2：oblique direction + anisotropic spacing 的 index->physical->VTK world 抽样 round-trip 在冻结容差内。
- [ ] AC3：dimensions/extent/origin/spacing/direction/scalar/foreground count 桥前后守恒。
- [ ] AC4：销毁 ITK 输入和 pipeline 后，返回的 VTK/private geometry 仍可完成 surface extraction，无 UAF/悬挂 buffer。
- [ ] AC5：真实 case 的 mask、surface 与原 CT 在 GUI 中同一 LPS 位置叠加，数值报告为权威证据。
- [ ] AC6：公共 API/architecture guard 无第三方类型泄漏，失败时无半状态。

## Out of Scope

中心线算法、mesh quality、GUI workflow、分割优化和新的独立 VTK reader。
