# 血管地基：ITK 三维预处理

> Parent: `07-11-vascular-imaging-foundation`
>
> Depends on: `07-11-vascular-foundation-dependency-remediation`; final real-data acceptance also depends on `07-11-vascular-foundation-data-gate`.

## Goal

复用 ITK 5.4 的三维各向异性扩散与多尺度 Hessian objectness/vesselness，在保持 LPS/mm 完整几何的前提下，把 XQ `Volume/IVoxelSource` 转成可供自动分割消费的 XQ-owned vesselness 结果。不得自写 diffusion、Hessian 或 eigenvalue kernel。

## Requirements

- 生产实现位于 `adapters/itk` private implementation；public header 不出现 ITK/VTK/Qt 类型。
- 使用真正的 `itk::Image<...,3>`；禁止逐切片二维处理冒充三维。
- 去噪复用 ITK anisotropic diffusion；vesselness 复用 ITK multi-scale Hessian + objectness，Frangi 语义为首个生产基线。
- sigma 最小/最大/步进、diffusion timestep/iterations/conductance 均由版本化 profile 提供；所有空间尺度以 mm 解释。
- XQ->ITK 复制/导入必须保留 dimensions、spacing、origin、完整 direction 和 rescaled scalar semantics。
- 输出包含 XQ-owned float volume、输入 fingerprint、profile/version、ITK version、耗时、scalar range 和 typed diagnostics。
- 空体、非有限几何、非法尺度、内存不足或零 vesselness 必须稳定失败，不能提交半结果。
- 合成管仅保护数学/坐标回归；真实增强 CT/CTA 才能关闭本 child。

## Acceptance Criteria

- [x] AC1：产品 target 真正实例化并执行 ITK 3D diffusion 与 multi-scale Hessian objectness filters。
- [x] AC2：倾斜 direction 与各向异性 spacing 输入的 index<->LPS 及输出 geometry 完全守恒。
- [x] AC3：同一输入/profile 重复运行的 fingerprint、geometry 和数值结果在定义容差内稳定。
- [x] AC4：sigma/profile 以 mm 记录；不同 spacing 的等物理管径产生一致尺度响应趋势。
- [x] AC5：真实数据门中至少一个 case 运行并生成非空可诊断 vesselness；日志证明不是 custom/2D path。
- [x] AC6：失败路径零半状态，公共 API 和 architecture guard 无第三方类型泄漏。

## Out of Scope

自动 mask、gold 评分、中心线、表面/网格、GUI cutover、深度学习或 Python runtime。
