# 壳档 A v1 对齐

> Trellis phase: Phase 2 / in_progress
>
> Authoritative acceptance source: `D:\XQ\规划-GoogleEarth全路线与壳子阶段.md` §4.2 A1–A15 and §5.3–5.4.

## Goal

在不重写已有 XQ 宿主、不偷换完成定义、也不等待完整血管影像 v2 管线的前提下，使当前仓库逐项满足 `D:\XQ` 壳档 A v1 的 A1–A15：

```text
纯 C++ 可复现构建
  + 真实 DICOM Volume（patient LPS/mm）
  + 带正半径的稳定 Path 契约
  + geometry-only smoke
  + Path-only 静态模块挂载
  + vmtk OFF 可用的 ITK 3D thinning 中心线 B
  + ScaleSlot 薄尺度位
  + GUI 与复现档案
```

只有 A1–A15 全部为 PASS，父任务才能完成并使用“壳档 A 已完成”的表述。

## Source-of-Truth Boundary

- A1–A15 原文是本任务唯一完成定义；不以旧 Trellis 任务名、CTest 总数或演示效果替代。
- `07-10-google-earth-shell-a` 是已有 host/data-spine、DICOM、Profile、Flow ON/OFF 与 GUI 自动化证据，不重写历史、不重新归属。
- `07-11-vascular-foundation-dependency-remediation` 是隔离 QtBase、精确 package roots 和依赖闭包证据，不重新归属。
- `07-11-vascular-imaging-foundation` 的真实 CTA、vesselness、自动分割、树拓扑和生产网格是更高阶段，不作为壳档 A v1 的前置门闩。
- `07-12-shell-a-centerline-b` 独立拥有 A13 的最小 vmtk-OFF thinning + 物理距离图单 Path fallback；后续 v2 centerline-tree 只能复用并扩展它，不得反向阻塞壳档 A 或重复实现 thinning/distance kernel。
- 若未来修改 A1–A15 的语义，必须显式升版本并另建迁移矩阵；本任务不做静默 v2。

## Confirmed Reuse

- A2：外部库 adapter/private boundary 与 public-header guard 已存在。
- A3：`XQImageVolume`/voxel source 已保存 spacing、origin、direction、LPS/mm。
- A4：LIDC-IDRI-0957 的 65 层真实 DICOM 已经 GDCM/ITK 生产路径读入、保存、重开和 lazy recovery。
- A9：`ScaleSlot{Organ,Micro,Cell}` 及持久化已经存在。
- A11：现有 VTK/Scene/GUI 具备几何显示基础；该项本身不是硬否决门。
- A12：TetGen adapter 和极简 tetra smoke 已存在且可 OFF；只能表述为 research-only。
- 旧 Shell A 自动回归、lineage、stale、undo/redo、typed persistence 与 Flow OFF 证据继续作为回归资产。

## Requirements

### A1 — 构建与铁律

- canonical ON/OFF build 和 GUI run 入口必须使用已验证的隔离 QtBase 与精确 package roots。
- 产品 link graph/PE closure 中无 Python runtime、MITK、Slicer、CTK、BlueBerry、Cesium、vmtk SuperBuild 或 zstd 污染。
- Windows 路径由固定脚本/配置显式提供，不依赖 host registry、Anaconda 或手工复制 DLL。

### A2 — adapters 边界

- ITK/VTK/GDCM/Thinning 外部类型只存在于 adapter 或私有实现。
- core/public service/module API 只暴露 XQ 自有值类型。

### A3 — Volume patient-mm

- Volume 保留 dimensions、spacing、origin、完整 direction、LPS/mm 与 voxel/world round-trip。

### A4 — M1b 真实 DICOM 通路

- 至少一套真实 DICOM series 经生产 GDCM/ITK 入口成功读入。
- 坏目录、坏 series/geometry/source drift 返回稳定诊断且无半状态。
- LIDC 样例只证明 IO，不被描述为 CTA 或血管算法金标准。

### A5 — Path 契约

- 壳级 Path module input 必须具有版本、LPS/mm frame、至少两个有序站点、finite position、正半径和非降弧长。
- 半径语义必须明确为局部等效半径；若由面积派生，使用 `sqrt(areaMm2 / pi)`，禁止第二套可写几何权威。
- validator 必须 fail-closed，并支持确定性 Path dump 作为验收证据。

### A6 — Path 来源诚实

- 来源明确映射为 `AutomaticCenterlineB`、`SemiAutomatic` 或 `GoldFile`。
- GUI、dump、日志与文档显示实际来源；半自动/金文件不得表述为全自动前处理。

### A7 — smoke_geometry_only

- 新增纯几何 smoke，只消费已验证 Path，不调用 Flow solver、不读取 Contour/ITK 容器。
- 同一输入重复运行结果稳定；坏 Path 返回诊断且零副作用。

### A8 — 模块可挂

- 提供最小静态 Path module interface/registry，至少注册并运行 `Noop` 与 `PathValidate`。
- 模块只消费壳级 Path 快照；不得读取 Scene、Contour、ITK/VTK native object 或 Flow schema。
- Flow OFF 时模块、Path 和几何浏览仍可运行；不要求动态 DLL/热加载。

### A9 — 尺度位

- 保留 `ScaleSlot{Organ,Micro,Cell}` 薄枚举与文档。
- 不增加 ROI registry、层-数据绑定表、自动内容切换或语义多尺度宣称。

### A10 — GUI 贯通

- 不依赖外部 Python 完成真实 DICOM 导入、Path 获取/装入、验证、模块运行、显示、保存重开与诊断查看。
- GUI 只调用共享 service/module path，不实现算法分支。

### A11 — 网格（推荐非锁死）

- 若表面可见，记录实际来源与限制；若本轮没有表面，不因此否决，但必须写入限制清单。

### A12 — TetGen

- adapter 可编译、极简 tetra smoke 可运行、开关可 OFF。
- TetGen 1.5 继续标记 AGPL/commercial research-only；质量与产品分发不是本任务通过声明。

### A13 — 中心线默认 B

- 接入审计锁定的真实 3D thinning 实现，vmtk 完全关闭时仍能从 XQ-owned binary mask 产生合法档 A Path。
- 半径来自 spacing-aware 物理距离图；输出经过图提取、短毛刺剪枝、主路径选择和 Path validator。
- 本门只证明壳级可用单路径，不声称 Dice、解剖树质量或自动分割可信。

### A14 — 诚实文档

- 维护 A1–A15 矩阵、非声称事项、样例 ID、Path 来源、TetGen/数据/中心线限制。
- 只能表述为稳定局部竖井与薄尺度位，不宣称完整 Google Earth、可信 1D、全自动 CTA 或数字孪生 Level 2–3。

### A15 — 复现仪式

- 在主操机器使用固定脚本和操作清单完成 fresh build、真实样例、Path dump、geometry smoke、模块运行、GUI、保存重开。
- 归档实际命令、日志、样例 ID、截图/人工验收记录和失败项；未执行项写 `未验证`。

## Acceptance Criteria

- [ ] AC1：`research/a1-a15-baseline.md` 中 A1–A15 全部从当前基线推进为 PASS，且每项有可定位证据。
- [ ] AC2：canonical Flow ON/OFF build/run 入口使用隔离依赖基线；fresh Release build、顺序 full CTest、build graph、PE closure 与负向 package probes 通过。
- [ ] AC3：壳级版本化 Path 契约、来源、validator、dump 和纯 geometry smoke 完成；坏输入 fail-closed。
- [ ] AC4：静态 module registry 的 Noop/PathValidate 真注册真运行，且只消费 Path；Flow OFF 保持可用。
- [ ] AC5：ITK 3D thinning B 在 vmtk OFF 构建中真实执行，输出 LPS/mm、正半径、非降弧长且 validator 通过。
- [ ] AC6：真实 DICOM、ScaleSlot、TetGen smoke、持久化、lineage 和现有 Shell 回归无退化；LIDC/TetGen/Flow 表述不越界。
- [ ] AC7：固定完成仪式和用户实机 GUI 验收记录存在；offscreen 自动测试不替代实机记录。
- [ ] AC8：所有 child 独立通过 check 并归档后，父级最终复核无 `partial`、`unverified` 或跳过门闩。

## Out of Scope

- 真实 CTA + reference mask 数据门、Frangi/Sato vesselness、自动三维血管分割和 Dice/clDice/HD95。
- 完整中心线树、分支拓扑定量、vtkvmtk 生产化或 vmtk 对比报告。
- CFD 级体网格质量、TetGen 商业许可解决、可信 1D/Boileau/openBF。
- Darcy、CTC、ONNX、全身内容切换、ROI registry、动态插件、临床或数字孪生能力声明。

## Execution Status

- 父任务当前为 `in_progress`；canonical-entry 与 Path/module-contract child 已进入实施。
- centerline-B 与最终验收仍按各自 task 状态独立启动；v2 工作不得接管或取消 A13。
- 每个复杂 child 仍须在各自 PRD/design/implement 与 context 通过审核后单独实施和验收。
