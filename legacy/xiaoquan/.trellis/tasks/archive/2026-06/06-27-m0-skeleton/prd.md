# M0 骨架:工程+CMake+core/io/adapter 搬入+命令栈

## Goal

建立零外部依赖的数据内核与项目读写,作为所有上层服务的地基。**起点是从 `XQrebuild`
按需搬入已实现的干净代码,而非从零写。**

## 依赖

- 前置:无(主线起点)。
- 后续:M1~M7 全部依赖 M0 的 core/io/命令栈。

## Requirements

### 从 XQrebuild 搬入(只读参考,搬干净的真实现)

```
src/core/  NodeId · Diagnostics · GeometryTypes · XQImageVolume · XQPath ·
           XQContourGroup · XQSegmentation · XQSurfaceModel · XQMesh ·
           XQSimulationCase · XQProject · XQScene · XQDataNode
src/io/project/  XQProjectReader/Writer · SvProjectReader · PTHPathReader · CTGRContourReader
src/adapters/vtk/  VtkImageAdapter · MDLModelReader · MSHMeshReader
tests/  对应的 ~24 个测试(全部纳入新工程的 ctest)
```

### 现状缺口(M0 必须补)

1. **统一 payload 机制**:`XQDataNode` 持有 `std::shared_ptr<XQPayload>`;`XQDomainType` 枚举
   (Image/Path/ContourGroup/SegmentationMask/SurfaceModel/Mesh/SimulationCase);`XQScene` 分组
   (Images/Paths/Segmentations/Models/Meshes/Simulations)+ `groupForDomain()`。
2. **命令栈**(XQrebuild 没有,M0 新建):
   - `src/core/command/XQCommand.h`:抽象 `execute() / undo() / label()`。
   - `src/core/command/XQCommandStack.h`:push+execute、undo、redo、清空。
   - 预置:`AddNodeCommand`、`AddNodeWithSourceRelationCommand`、`ReplacePayloadCommand`、`RemoveNodeCommand`。
3. **原生存档骨架**:`XQProjectReader/Writer` 先搭骨架,M1 起逐步 round-trip 各 payload。

### SvProjectReader(.svproj 兼容)

`SvProjectReader::load(projectDir)` → 一个 `XQProject` + 一个 `XQScene`,各文件读成对应 payload 节点
并建立 source/derived 关系。样例 `0007_H_AO_H` 结构:Images/*.vti、Paths/*.pth、Segmentations/*.ctgr、
Models/*.mdl,*.vtp、Meshes/*.msh,*.vtu、Simulations/*.sjb、flow-files/*.flow。

## 约束

- core 零外部依赖;遵循 spec/XQ/architecture 与 spec/XQ/core 全部规范。
- 参考来源:代码起点 XQrebuild;行为/算法参考 SimVascular/MITK;**XQ1 全面作废,不参考**。

## Acceptance Criteria(plan「M0 验收」)

- [ ] 全新 CMake 工程配置通过,VTK 9.3 / tinyxml2 / Qt6 可解析。
- [ ] 搬入的 ~24 个测试 + 新增命令栈/payload 测试全部 `ctest` 绿。
- [ ] `SvProjectReader` 能把 `0007_H_AO_H` 读成 scene(节点数、分组、关系符合 `expected-scene.json`)。
- [ ] 数据落进 XQ 自有对象(非外部库对象图的"读取")。

## Notes

- 这是基础设施类复杂 task;进 Phase 2 前应补 design.md / implement.md。
