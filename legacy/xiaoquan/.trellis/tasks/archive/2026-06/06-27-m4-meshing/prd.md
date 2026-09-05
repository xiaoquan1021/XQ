# M4 网格:MeshService 表面网格 + 体网格 + 边界面元

## Goal

由表面模型生成表面网格,再由闭合表面生成体网格(四面体),保留边界面元数据供仿真设置使用。
首版用 VTK / XQ 自有三角化;MMG / TetGen / OCCT 暂不引入。

## 依赖

- 前置:**M3**(表面模型 + ModelFace id / cap id)。
- 后续:M5 流体(网格边界面表是边界条件的权威面列表)。

## Requirements

### 所有权

```
src/core/XQMesh.h / .cpp                              网格 payload(XQrebuild 已有,扩展)
src/services/meshing/SurfaceMeshService.h / .cpp
src/services/meshing/VolumeMeshService.h / .cpp
tests/services/meshing/SurfaceMeshServiceTest.cpp
tests/services/meshing/VolumeMeshServiceTest.cpp
```

### 公开 API

```cpp
buildSurfaceMeshCommand(model, params)         -> AddNodeWithSourceRelationCommand
buildVolumeMeshCommand(surfaceOrModel, params) -> AddNodeWithSourceRelationCommand
transferBoundaryFaces(meshA, meshB)            // face/cap id 传递
```

- 表面网格:输入 `XQSurfaceModel` + `XQTriangleSurfaceGeometryHandle` + 面元元数据 + 参数;
  输出表面 `XQMesh` / XQ 自有三角面网格 handle、保留 face id 与 cell id 的边界面记录、质量摘要。
- 体网格:输入闭合三角表面 + 参数;输出体 `XQMesh`(非结构网格 + 边界表面 + face 映射)、
  四面体 handle、边界面记录(保留 face id / cap id / boundary cell id)、质量摘要。

### 边界面元数据(关键)

- 表面/体网格必须保留来自建模(M3)的 `ModelFace` id 与 cap id。
- 这是仿真边界条件(M5)绑定进出口的依据,**全链路必须可追踪**。

### 算法 kernel 边界

- 首版:VTK 几何 + XQ 自有三角化/四面体化(或 VTK 内置),够跑通主线即可。
- MMG(重网格)/ TetGen(体网格)作为后续 kernel 候选,引入时置于 `adapters`,不进公开 API。

### 校验

- 体网格输入表面必须闭合;非闭合应发诊断而非默默生成无效网格。
- 输出网格附基本质量摘要(单元数、最差质量指标)。

## 约束

- 公开 API 不暴露 VTK/MMG/TetGen 对象;不直接改 scene。
- 参考来源:网格生成语义参考 **SimVascular/MITK**(及底层 VTK);**XQ1 不参考**。

## Acceptance Criteria(plan「M4 验收」+ 05「验收」)

- [ ] 由 `0007` 表面模型生成表面网格与体网格;`0007/Meshes/*.msh,*.vtu` 可读入对照。
- [ ] 边界 face id / cap id 端到端保留。
- [ ] 单测覆盖闭合性校验、面元传递、质量摘要、undo。

## Notes

- 复杂 task,进 Phase 2 前补 design.md / implement.md。
