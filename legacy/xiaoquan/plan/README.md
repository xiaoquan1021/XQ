# XQ 重建计划(精简版)

血管影像建模与血流分析的**轻量化桌面应用**:核心是一组无 UI、无框架依赖的 C++ 函数库,
Qt 只做薄壳。主线一条:**路径 → 分割 → 建模 → 网格 → 流体 → AI 分析**。

> 本计划已废弃旧版的 harness / ledger / contract / task-pack 治理层。验收方式就是
> `ctest` 全绿 + 真实样例项目(`0007_H_AO_H`)端到端跑通,不再有 gate / review / projection。

## 三条铁律

1. **库优先**:每条主线能力(path / segmentation / modeling / meshing / flow / ai)都是
   可单测、可被 CLI 或 GUI 复用的纯 C++ 服务库。UI 后挂,可整体替换。
2. **无治理层**:不写 harness/ledger/contract/task-pack。一个文档对应一块能力,改完跑测试。
3. **外部库当 kernel,不当架构**:VTK / ITK / GDCM / ONNX Runtime 只出现在 adapter / service
   的私有实现里;公开业务 API 只暴露 XQ 自有类型(`XQProject` / `XQScene` / `XQDataNode` / 各 payload)。

## 分层与依赖方向

```
app/        Qt 精简桌面壳(主窗口 / 项目树 / MPR / 3D / 工具面板)   ← 薄,可整体替换
   │  只调用 services + 读 scene
services/   path · segmentation · modeling · meshing · flow · ai     ← 重心
   │
adapters/   vtk · itk · gdcm · onnx   (kernel 封装,不出现在公开 API)
   │
io/         .svproj/.pth/.ctgr/.mdl/.vtp/.vtu 读 + 原生存档
   │
core/       XQProject/Scene/DataNode + 各 payload + 命令栈/undo         ← 零外部依赖
```

依赖只能自上而下。`core` 不依赖 Qt / VTK / ITK / GDCM。

## 里程碑

| 里程碑 | 内容 | 验收 |
|---|---|---|
| **M0 骨架** | 全新工程 + CMake;从 `XQrebuild` 搬入干净的 core/io/adapter;补命令栈 | `ctest` 全绿;能把 `0007` 读成 scene |
| **M1 路径** | `PathService`:控制点增删改、重采样、中心线局部标架;命令/undo | 单测 + 读 `0007/Paths/*.pth` |
| **M2 分割** | `SegmentationService`:阈值/区域生长/连通域 + **AI 分割接口**(ONNX);掩膜 payload | 单测 + `0007` 影像上跑通 |
| **M3 建模** | `ModelingService`:contour 放样成面 + 封口 + 面元元数据 | 单测 + 读 `0007/Models/*.mdl,*.vtp` |
| **M4 网格** | `MeshService`:表面网格 + 体网格(先 VTK/自有三角化;MMG/OCCT 暂留) | 单测 + 读 `0007/Meshes/*.msh,*.vtu` |
| **M5 流体** | `FlowService`:**内置 1D/降阶血流求解**(中心线+截面积→流量/压力);边界条件 | 单测(合成算例数值校验) |
| **M6 AI 分析** | `AiService` 三类:① 几何/影像分割识别 ② 流体指标/风险(FFR/WSS…) ③ 代理模型预测(几何/网格→流场,跳过求解);统一 ONNX 后端 | 单测(mock/小模型);接口契约固定 |
| **M7 前端** | 精简 Qt 壳串起整条主线 | `0007` 端到端走一遍 |

每个里程碑都「库先行、UI 后挂」:M1~M6 产出可单测的服务库,M7 才接进 Qt。

## 文档索引

- [00-architecture.md](00-architecture.md) — 分层 / 依赖 / 外部库规则 / 坐标语义
- [01-core-and-io.md](01-core-and-io.md) — core 数据模型 + io + 命令栈
- [02-path.md](02-path.md) — 路径
- [03-segmentation.md](03-segmentation.md) — 分割(传统 + AI)
- [04-modeling.md](04-modeling.md) — 建模(放样/封口)
- [05-meshing.md](05-meshing.md) — 网格(表面/体)
- [06-flow.md](06-flow.md) — 流体(1D/降阶血流)
- [07-ai-analysis.md](07-ai-analysis.md) — AI 分析(分割/指标/代理)
- [08-frontend.md](08-frontend.md) — 精简 Qt 前端
- [09-dependencies.md](09-dependencies.md) — 依赖基线

## 工作区

- `XQrebuild/` — 上一轮尝试。**code 起点:全新工程,从这里按需搬入干净的 core/io/adapter。**
- `XQ1/` — 老 SimVascular/MITK 风格实现,**仅作行为参考,不作为迁移源码**。
- `0007_H_AO_H/`、`0080_H_PULM_H/` — 真实样例项目,集成测试与端到端验收用。
