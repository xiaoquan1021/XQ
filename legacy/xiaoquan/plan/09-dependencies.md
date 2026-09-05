# 09 依赖基线

依赖只能作为 kernel / codec / 渲染后端,出现在 `adapters` 或 service 私有实现里(见 `00-architecture.md`)。
版本以 `Externals/externals.manifest` 为准。

## 基线(保留)

| 依赖 | Externals 锁定版本 | 角色 | 出现位置 |
|---|---|---|---|
| Qt6 | 6.7.0 | 桌面 UI | `app/` 及 UI 适配 |
| VTK | 9.3.0 | 渲染 + 几何 + vtp/vtu/vti 读写 | `adapters/vtk`、`app` 渲染 |
| ITK | 5.4.0 | 影像处理/重采样/形态学 | `adapters/itk`、分割私有实现 |
| GDCM | 3.0.10 | DICOM 读取 | `adapters/gdcm`、影像 io 私有��现 |
| TinyXML2 | 8.0.0 | XML 解析(.svproj/.pth/.ctgr/.mdl/原生存档) | `io` 私有实现 |
| HDF5 | 1.14.3 | 大数据/网格存储(按需) | io 私有实现 |
| FreeType | 2.13.0 | 字体(VTK/Qt 传递依赖) | 渲染 |

> DICOM 走 GDCM(经/不经 ITK 均可),与 SimVascular/MITK 对齐,不引入 DCMTK。

## 新增

| 依赖 | 建议版本 | 角色 | 出现位置 | 说明 |
|---|---|---|---|---|
| ONNX Runtime | 待定(取稳定 release,CPU 版起步) | AI 推理后端 | `adapters/onnx`、`services/ai` 私有实现 | **Externals 当前没有,需新增到 manifest**。无 Python 依赖。 |

## 暂留待评估(首版不引入)

| 依赖 | Externals 锁定版本 | 现状 | 决策点 |
|---|---|---|---|
| OCCT (OpenCascade) | 7.6.0 | 建模(04)首版用 VTK/自有三角化,不引入 | 评估"AI/VTK 是否足以替代 CAD 建模";需要 CAD 级精度时再引入 `adapters/occt` |
| MMG | 5.3.9 | 网格(05)首版用 VTK/自有三角化,不引入 | 需要高质量重网格时再引入 `adapters/mmg` |

## 明确弃用

| 依赖 | manifest 中 | 理由 |
|---|---|---|
| MITK | 2024.06 | 架构不再以插件 workbench 为所有者(见 `00-architecture.md`);仅 `XQ1/` 作行为参考 |
| BlueBerry / CTK | 随 MITK | 同上 |
| SWIG / Python | 3.0.12 / 3.11.0 | 首版不做 Python 绑定;AI 走 C++ 内置 ONNX,不需 Python 运行时。未来绑定需另行决策,且不得持有业务状态或制造第二套对象图 |

## 规则

- manifest 是版本权威;实现期的精确锁(`dependencies.lock.json`)在实现工程内另行创建。
- 引入"暂留"或"新增"依赖前,先确认它只进 `adapters` / 私有实现,不污染公开 API。
- 依赖变更会使下游服务与测试过期,变更时同步更新相关 plan 文档与测试。
