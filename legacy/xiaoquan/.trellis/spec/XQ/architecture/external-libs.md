# 外部库规则与依赖基线(external libs)

> 来源:`plan/00-architecture.md`、`plan/09-dependencies.md`。版本以 `Externals/externals.manifest` 为准。

---

## 外部库只能是 kernel / codec / 渲染后端

外部库出现在 **adapter 或 service 的私有实现**里,或作为一次性的可视化产物
(disposable visualization product)。**不得出现在公开业务 API 中。**

公开 API 暴露 XQ 自有的 value / handle / payload 类型(如 `XQImageVolume`、
`XQTriangleSurfaceGeometryHandle`、`XQMesh`)。外部原生对象只能在私有实现或 backend adapter 内部持有。

| 库 | 角色 | 出现位置 |
|---|---|---|
| Qt6 | 桌面 UI 框架 | 仅 `app/`(及 services 之上的 UI 适配) |
| VTK | 渲染 + 几何 + vtp/vtu/vti 读写 | `adapters/vtk`、`app` 渲染;不进 core |
| ITK | 影像处理/重采样/形态学 | `adapters/itk`、分割服务私有实现 |
| GDCM | DICOM 读取 | `adapters/gdcm`、影像 io 私有实现 |
| ONNX Runtime | AI 推理后端 | `adapters/onnx`、`services/ai` 私有实现;**无 Python 依赖** |
| OCCT / MMG | CAD / 重网格(暂留) | 引入后置于 `adapters`;首版不引入 |

## 依赖基线版本(Externals 锁定)

| 依赖 | 版本 | 角色 |
|---|---|---|
| Qt6 | 6.7.0 | 桌面 UI |
| VTK | 9.3.0 | 渲染 + 几何 + vtp/vtu/vti |
| ITK | 5.4.0 | 影像处理/重采样/形态学 |
| GDCM | 3.0.10 | DICOM 读取(不引入 DCMTK) |
| TinyXML2 | 8.0.0 | XML 解析(.svproj/.pth/.ctgr/.mdl/原生存档) |
| HDF5 | 1.14.3 | 大数据/网格存储(按需) |
| FreeType | 2.13.0 | 字体(VTK/Qt 传递依赖) |
| ONNX Runtime | 待定(CPU 版起步) | AI 推理;**Externals 当前没有,需新增到 manifest** |

## 暂留 / 弃用

- **暂留(首版不引入)**:OCCT 7.6.0(建模)、MMG 5.3.9(网格)——首版用 VTK / XQ 自有三角化;
  需 CAD 级精度或高质量重网格时再引入 `adapters/occt`、`adapters/mmg`。
  - MMG adapter 依赖 TetGen adapter(两段式:TetGen 填充 → MMG 优化),属 adapters 层内的合法有向依赖。
- **明确弃用**:MITK 作为插件 workbench 架构弃用(其库能力语义仍是参考来源,见 `reference-sources.md`)、
  BlueBerry / CTK、SWIG / Python(首版不做 Python 绑定,AI 走 C++ 内置 ONNX)。

## 规则

- manifest 是版本权威;实现期精确锁(`dependencies.lock.json`)在实现工程内另行创建。
- 引入"暂留/新增"依赖前,先确认它只进 `adapters` / 私有实现,不污染公开 API。
- 依赖变更会使下游服务与测试过期,变更时同步更新相关 plan 文档与测试。
