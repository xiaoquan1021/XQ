# 01 Core 与 IO(M0 / M1 基础)

## 目标

建立零外部依赖的数据内核与项目读写,作为所有上层服务的地基。**起点是从 `XQrebuild`
按需搬入已实现的干净代码,而非从零写。**

## 从 XQrebuild 搬入(已是真实现,直接复用)

```
src/core/  NodeId · Diagnostics · GeometryTypes · XQImageVolume · XQPath ·
           XQContourGroup · XQSegmentation · XQSurfaceModel · XQMesh ·
           XQSimulationCase · XQProject · XQScene · XQDataNode
src/io/project/  XQProjectReader/Writer · SvProjectReader · PTHPathReader · CTGRContourReader
src/adapters/vtk/  VtkImageAdapter · MDLModelReader · MSHMeshReader
tests/  对应的 ~24 个测试(全部纳入新工程的 ctest)
```

`XQ1/` 仅作行为参考,不搬源码。

## 现状缺口(M0/M1 必须补)

1. **`XQDataNode` 不承载 payload**。当前只有 `id / domain_type / display_name`,各 payload
   (image/path/contour/...)还没挂进节点。需要引入统一的 payload 机制:

   ```
   XQDomainType  枚举(Image/Path/ContourGroup/SegmentationMask/SurfaceModel/Mesh/SimulationCase)
   XQPayload     payload 基类/handle;XQDataNode 持有一个 std::shared_ptr<XQPayload>
   XQScene 分组  XQSceneGroup(Images/Paths/Segmentations/Models/Meshes/Simulations)
   ```
   `groupForDomain(XQDomainType)` 把领域类型映射到分组。

2. **命令栈缺失**。旧领域文档反复引用 `XQCommandStack`,但 XQrebuild 里没有。M0 补:

   ```
   src/core/command/XQCommand.h        抽象:execute() / undo() / label()
   src/core/command/XQCommandStack.h   push+execute、undo、redo、清空
   ```
   预置通用命令:`AddNodeCommand`、`AddNodeWithSourceRelationCommand`、
   `ReplacePayloadCommand`、`RemoveNodeCommand`。所有服务通过返回这些命令来改 scene。

3. **原生存档要覆盖新 payload**。`XQProjectReader/Writer` 当前是骨架,M1 起逐步让它
   round-trip 各 payload(配合后续里程碑增量)。

## 现有 core API(搬入后即有,供上层直接用)

- `XQProject`:`scene()`、`open()/close()/reopen()`、`LifecycleState`。
- `XQScene`:`insert/find/remove`、`link_derived(source,derived)`、`mark_source_changed`、
  `is_stale/stale_reason`、`visit_nodes/visit_derived_relations/visit_stale_nodes`。
- `XQImageVolume`:几何(origin/spacing/direction/extent)、scalar type、modality、DICOM 标识、
  窗宽窗位、`voxelToWorld/worldToVoxel`。
- `XQPath`:控制点、`resample(spacing)`、`frameAtArcLength`、`sourceImageNode`、样本点(含标架)。
- `NodeId`:稳定 id,`serialize/parse`(供存档)。
- `Diagnostic / Provenance`:诊断与来源,带 stale 证据计数。

## 项目读取主线(SimVascular `.svproj` 兼容)

样例 `0007_H_AO_H` 结构:

```
.svproj                 项目清单
Images/*.vti            影像
Paths/*.pth             中心线/路径
Segmentations/*.ctgr    contour group
Models/*.mdl,*.vtp      表面模型
Meshes/*.msh,*.vtu      网格
Simulations/*.sjb       仿真设置
flow-files/*.flow       inflow 波形
```

`SvProjectReader::load(projectDir)` → 一个 `XQProject` + 一个 `XQScene`,各文件读成对应 payload
节点并建立 source/derived 关系。读取产物进入 XQ 自有对象才算验收(见 `00-architecture.md`)。

## M0 验收

- 全新 CMake 工程配置通过,VTK 9.3 / tinyxml2 / Qt6 可解析。
- 搬入的 ~24 个测试 + 新增命令栈/payload 测试全部 `ctest` 绿。
- `SvProjectReader` 能把 `0007_H_AO_H` 读成 scene(节点数、分组、关系符合 `expected-scene.json`)。

## M1 验收

- `XQDataNode` 可承载各 payload;`XQScene` 按分组组织。
- `XQCommandStack` 支持所有 scene 变更的 execute/undo/redo。
- `0007/Paths/*.pth` 读成 `XQPath` 节点并挂到 scene。
