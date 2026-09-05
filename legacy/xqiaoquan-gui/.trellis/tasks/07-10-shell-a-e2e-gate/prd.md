# 壳 A：端到端验收门

## Goal

用同一批生产契约和服务完成壳档 A 最终双重验收：真实 DICOM 进入项目数据脊柱，金/测量 Path+Contour 生成 VesselProfile，固定 Flow smoke 运行并持久化；headless 与 GUI 结果一致。通过后立即停止壳 A 扩张。

## Dependencies

- Parent: `07-10-google-earth-shell-a`。
- Hard dependencies：`shell-a-domain-contracts`、`shell-a-dicom-spine`、`shell-a-profile-assembly`、`shell-a-flow-smoke`、`shell-a-flow-capability` 全部完成并独立检查通过。

## Requirements

- 固定成功链：`DICOM Image -> Path -> ContourGroup -> VesselProfile -> SmokeCase -> FlowResult`。
- 自动 headless gate 使用去标识 fixture 与同 frame 的金 Path/Contour；不通过 MainWindow 私有数据或测试专用假结果绕过生产 service/controller。
- GUI gate 能选择 series、显示影像/Path/Profile、触发 smoke、保存、退出并从 `.xqproj` 重开。
- 每个节点保留稳定 id、Organ ScaleSlot、source/derived relation、算法/版本/参数/frame/units；重开后一致。
- 修改 Path 或 Contour 后，Profile、Case、Result 一次调用即传递 stale。
- 负向 gate 覆盖坏 DICOM、多 series 未选、source missing、坏 Profile、错误单位、solver failure 和 Flow OFF。
- CI 不联网、不含 PHI；另用 `XQ_DICOM_TEST_DATA_ROOT` 做授权且去标识真实 series 手工 gate（公开数据优先）。真实数据门只记录脱敏汇总；截图只能使用仓库内去标识 fixture。

## Acceptance Criteria

- [ ] headless 成功链完整运行，保存重开后 geometry、voxel checksum、Profile、Flow series、relations、ScaleSlot 一致。
- [ ] GUI 使用同一链完成可重复演示，Path/Profile 在 DICOM patient space 中位置正确。
- [ ] 所有负向场景明确失败、无部分 scene/asset/undo 污染。
- [ ] Flow OFF build 仍可导入/重开影像、浏览 Path/Contour/Profile 和历史结果，不能执行新 smoke。
- [ ] 现有 `0007_H_AO_H`、VTI、project、render、workflow 回归及 Release full ctest 全绿。
- [ ] 验收记录明确“工程壳完成，不代表全身孪生或肿瘤转移模型完成”。

## Out of Scope / Stop Line

- 不再加入 atlas、ROI registry、ScaleNode、语义 zoom、血管树、Darcy、CTC、PhysiCell、患者化 BC 或通用插件系统。
- 本任务通过后父任务进入 finish；新增科学能力必须另开后续任务。
