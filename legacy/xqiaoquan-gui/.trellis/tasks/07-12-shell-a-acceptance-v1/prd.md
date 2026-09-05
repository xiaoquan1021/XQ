# 壳档 A：A1–A15 最终验收

> Status: planning
>
> Parent: `07-12-shell-a-v1-alignment`

## Goal

以 `D:\XQ` A1–A15 原文执行最终放行，不新增算法范围；把 canonical build、真实 DICOM、Path、geometry smoke、模块、ScaleSlot、TetGen、中心线 B、GUI 和诚实限制收敛为同一份可复现证据包。

## Dependencies

- canonical-entry、path-module-contract、centerline-b 三个 child 已完成 check 并归档。
- 旧 Shell A、dependency remediation 和 DICOM provenance 证据仍可读取。

## Requirements

- 建立逐项 A1–A15 matrix，每项列真实命令、build tree、test、样例/fixture、日志和人工证据。
- 从 fresh canonical ON/OFF setup 顺序运行全部要求，不复用“上次绿”替代最终门。
- 运行现有真实 LIDC DICOM gate并明确它只证明 A4 IO。
- 归档合法 Path dump、实际来源、geometry smoke、Noop/PathValidate registry/run 和 vmtk OFF thinning 证据。
- 验证 ScaleSlot 薄枚举没有被过度表述为语义多尺度。
- 验证 TetGen adapter/smoke/OFF gate；许可结论仍为 1.5 research-only。
- 按书面操作清单在用户物理机器启动 GUI，完成导入/获取或装入 Path/验证/模块/显示/保存重开，并记录结果。
- 维护非声称/已知限制页；任何未执行或失败门闩都保持父任务未完成。
- 最终证据不包含 PHI、外部受限数据、自由文本 DICOM tags 或本机敏感信息。

## Acceptance Criteria

- [ ] AC1：A1–A15 全部 PASS；无 partial、skip-as-green、未验证或以另一个门替代。
- [ ] AC2：fresh ON/OFF full Release CTest、focused tests、dependency/PE checks 全绿且串行执行。
- [ ] AC3：真实 DICOM、Path dump、geometry smoke、module registry、centerline B 和 persistence 证据齐全。
- [ ] AC4：用户实机 GUI checklist 完成并记录日期、配置、结果与限制；offscreen 不冒充。
- [ ] AC5：发布措辞只声明“稳平台 + 真数据 + 几何契约 + 可挂模块 + 薄尺度位”。
- [ ] AC6：父任务 baseline matrix 更新为全 PASS，所有 child 已归档，最终 `git diff --check` 和 Trellis check 通过。

## Out of Scope

- 为了让验收变绿而修改 A1–A15、降低 validator、使用 mock/Noop 替代真实 required path。
- 完整 CTA 自动分割、可信 1D、生产网格、全身多尺度或数字孪生声明。

