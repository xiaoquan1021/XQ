# M7 前端:精简 Qt 壳串起主线 + 0007 端到端

## Goal

精简 Qt 桌面壳,把整条主线(路径→分割→建模→网格→流体→AI)串成可用工作流。**壳薄、可替换**:
只调用 services、读 scene、把 UI 事件转成 service 输入并通过命令栈提交;**不实现任何领域算法**。

## 依赖

- 前置:**M1~M6 全部**(每个工具面板调用对应 service)、M0(scene / 命令栈 / 原生存档)。
- 这是端到端验收里程碑。

## Requirements

### 复用 XQrebuild 已有 UI(精简,不重写)

```
src/ui/XQSceneModel              QAbstractItemModel:XQScene -> 项目树(display name / domain / stale 列)
src/visualization/XQImageViewer  VTK 离屏渲染影像 -> RGBA
src/app/XQMainWindow             主窗口:dock + 项目树 + 影像视图
```

在此基础上精简并补齐,而非从零搭。

### 布局(首版)

- 左:项目树(`XQSceneModel`,按 scene 分组、选中、stale 标记)。
- 中:视图区 —— MPR(轴/冠/矢)+ 3D 场景视图(VTK)。首版可先一个 MPR + 一个 3D。
- 右:随工作流阶段切换的工具面板(路径/分割/建模/网格/流体/AI)。
- 工具面板只收集意图,调用对应 service 拿命令,经 `XQCommandStack` 提交;Edit 菜单统一 undo/redo。

### 工作流串联

```
路径面板  -> PathService           (MPR 点击取世界坐标 -> 控制点)
分割面板  -> SegmentationService   (阈值/区域生长 + AI 分割)
建模面板  -> ModelingService       (选 contour group -> 放样/封口)
网格面板  -> MeshService           (表面/体网格 + 质量摘要)
流体面板  -> BoundaryConditionService + FlowSolver1D  (设边界 -> 求解 -> 结果)
AI 面板   -> AiService             (分割/识别、流体指标/风险、代理预测)
```

### VTK / Qt 边界

- VTK 仅在 `visualization` 与 `app` 渲染路径使用,作为一次性可视化产物。
- viewer 通过 adapter 消费 scene 节点 payload,**不持有平行业务对象图**;UI 不绕过 service 改 scene。

## 约束

- 公开给 service 的仍只是 XQ 自有类型;不在 UI 实现领域算法。
- 参考来源:整条工作流 UI **不参考 XQ1**(全面作废);仅在 XQrebuild 已有 UI 上精简。

## Acceptance Criteria(plan「M7 验收」+ 08「验收」)

- [ ] `0007_H_AO_H` 端到端走一遍:打开项目 → 看到树与影像 → 路径 → 分割 → 建模 → 网格 → 流体求解 → AI 指标,各步结果出现在项目树并可 undo。
- [ ] 关闭/重开项目后 scene 状态一致(配合 M0 原生存档)。
- [ ] `XQSceneModel`:scene 变更后树正确刷新(已有 `test_scene_model`)。
- [ ] `XQMainWindow`:offscreen 平台下可构造、可显示影像(已有 `test_main_window`,`QT_QPA_PLATFORM=offscreen`)。
- [ ] 工具面板:无头方式验证"点击→service→命令→scene 变更"链路(不依赖真实渲染窗口)。

## Notes

- 复杂 task,进 Phase 2 前补 design.md / implement.md。
