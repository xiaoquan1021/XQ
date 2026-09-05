# 血管地基：真实血管网格

> Parent: `07-11-vascular-imaging-foundation`
>
> Depends on: automatic segmentation, ITK-VTK bridge, dependency/license gate and real data gate.

## Goal

复用 VTK 表面提取/清理/平滑/简化，以及现有 `ITetMesher` 后的 TetGen 1.5 + MMG 路径，在自动真实血管 mask 上生成可视表面和通过质量门的体网格。不得用程序弯管或 star-fan fallback 冒充生产成功。

## Requirements

- surface 通过 VTK 官方 filters 生成，保持 LPS/mm/direction、闭合性、orientation 和 foreground boundary 语义。
- smoothing/decimation 参数版本化，并报告点/面数、拓扑变化、边界和体积变化。
- TetGen 只在 `XQ_ENABLE_TETGEN=ON` + research acknowledgement 下使用；MMG 负责质量改善，不冒充 volume fill。
- 若目标分发方式不允许 TetGen，必须选择并实现合规 fill backend 或保持本 child 未完成；不能静默切已知退化算法。
- 体网格检查覆盖 inverted/non-positive/outside/degenerate cells、quality distribution、boundary markers 和参数敏感性。
- 生产输入必须来自自动 mask/bridge；人工 VTP 和合成弯管只作回归。
- 输出为 XQ-owned surface/mesh contract，原子提交前全部验证。

## Acceptance Criteria

- [ ] AC1：至少一个真实弯曲/分叉 case 由自动 mask 生成闭合、方向一致的 surface，并在 GUI 可见。
- [ ] AC2：ON 构建实际执行 TetGen->MMG 或经批准替代后端；日志包含后端/version/profile/fingerprint。
- [ ] AC3：真实体网格 inverted/non-positive/outside/obvious-degenerate 均为零，质量分布达到数据门冻结阈值。
- [ ] AC4：boundary marker 和物理坐标在 surface->volume mesh 后守恒。
- [ ] AC5：后端失败、开口/非流形 surface、质量不达标均失败且不回退到 star fan。
- [ ] AC6：许可边界在构建、GUI/报告和最终交付声明中明确；未解决许可会阻止相应分发声明。

## Out of Scope

CFD 求解、1D 可信化、商业许可采购和只对玩具几何调参。
