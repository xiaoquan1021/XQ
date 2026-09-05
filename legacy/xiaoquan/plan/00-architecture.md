# 00 架构

## 所有权

- `XQProject` 拥有项目生命周期、根标识、诊断信息、修改状态,以及唯一一个权威的 `XQScene`。
- `XQScene` 拥有所有领域节点及其关系。UI 模型、viewer、reader、算法只**观察或变换** scene
  数据,**不持有平行的业务对象图**。
- `XQDataNode` 拥有稳定节点标识、显示元数据、领域类型、来源(provenance)、诊断,以及一个
  XQ 自有的 payload。

## 依赖方向

```
app/workbench -> services -> io / adapters -> core
```

- `core` 不得依赖 Qt widgets、VTK renderer、ITK image、GDCM dataset、OCCT shape、MMG mesh、
  ONNX session,也不得依赖任何插件注册表或 SWIG 生成对象。
- `services`(path/segmentation/modeling/meshing/flow/ai)是纯算法/领域逻辑,**不依赖 Qt**;
  需要外部 kernel 时,经 `adapters` 调用,公开签名只用 XQ 自有类型。
- `app` 只调用 services、读 scene、把 UI 事件转成 service 输入,不实现领域算法。

## 外部库规则

外部库只能是 **kernel / codec / 渲染后端**,出现在 adapter 或 service 的私有实现里,或作为
一次性的可视化产物(disposable visualization product)。**不得出现在公开业务 API 中。**

| 库 | 角色 | 出现位置 |
|---|---|---|
| Qt6 | 桌面 UI 框架 | 仅 `app/`(及 `services` 之上的 UI 适配) |
| VTK | 渲染 + 几何 + vtp/vtu/vti 读写 | `adapters/vtk`、`app` 渲染;不进 core |
| ITK | 影像处理/重采样/形态学 | `adapters/itk`、分割服务私有实现 |
| GDCM | DICOM 读取 | `adapters/gdcm`、影像 io 私有实现 |
| ONNX Runtime | AI 推理后端 | `adapters/onnx`、`services/ai` 私有实现 |
| OCCT / MMG | CAD / 重网格(暂留) | 仅在引入后置于 `adapters`;见 `09-dependencies.md` |

公开 API 暴露 XQ 自有的 value / handle / payload 类型(如 `XQImageVolume`、
`XQTriangleSurfaceGeometryHandle`、`XQMesh`)。外部原生对象只能在私有实现或 backend adapter
内部持有。

## 坐标系与单位(冻结)

> 来源:SimVascular / ITK 惯例。XQ1 用 MITK/ITK 影像 IO 与几何,无显式 RAS 转换。

- **影像 index 坐标**:整数体素索引,按存储的影像轴序。
- **physical 坐标**:由 origin、spacing、direction 导出的连续坐标。
- **world 坐标**:XQ scene 坐标,viewer 与领域关系使用。
- `coordinate_system = LPS`:physical 与 world 坐标采用 DICOM/ITK 患者空间约定。
- `spatial_unit = mm`:长度、spacing、origin、path、contour、model、mesh 及几何派生量统一用 mm。
- 影像 payload 必须存:origin、spacing、direction、scalar type、component count、extent。
- 压力、流量、时间等非空间单位必须在各 reader / 领域 payload 中显式声明。
- reader 必须保留已声明的单位;单位缺失或歧义时发诊断。

## 路径与 contour 语义

- path 点带显式坐标空间。
- contour 平面必须引用一个 path frame 或 image/world frame。
- path frame 与 contour 平面在同一 LPS world 坐标系下解释,距离单位 mm。slice 局部 contour
  坐标(mm)通过引用的 path-frame 基底抬升到 LPS world 坐标。

## 变换语义

- 影像、模型、网格的变换必须显式。
- 当文件元数据已证明非恒等变换时,任何 reader 不得默默假定恒等变换。

## 领域关系

- **所有权关系**:一个节点属于且仅属于一个 `XQScene` 分组。
- **来源关系(source)**:节点由另一节点加载或派生而来。
- **派生关系(derived)**:算法输出依赖一个或多个来源节点,可能变陈旧(stale)。
- **stale 传播**:来源节点变更后,沿 derived 关系向下游传播 stale 标记。

主线本身就是一条 source/derived 链:

```
image --源--> path --源--> contour group --派生--> surface model
      --派生--> segmentation mask --派生--> surface model
surface model --派生--> mesh --派生--> simulation case / flow result --派生--> ai analysis
```

## 标识与来源

- 节点 ID 在 save/reopen 之间必须稳定(除非迁移策略另有规定)。
- payload 版本与 provenance 必须随项目保存,保证可复现。
- copy/move/share 语义需要显式的所有权与 ID 规则。

## 线程与错误

- core 对象默认非线程安全(除非契约明确)。
- 公开 API 返回显式的 result/diagnostic,而不是穿过 UI 代码路径抛异常。
- 诊断信息不得包含私有患者/机构字段。

## 命令与 undo

- 所有对 scene 的变更经 `XQCommandStack` 提交。
- service 计算"该怎么改",返回 XQ 自有的 command;command 在 execute 时改 scene,undo 时复原。
- service **不直接改 `XQScene`**;UI 只收集意图、提交 command。

## 验收原则(替代旧治理层)

- 数据进入 XQ 自有对象才算数:经过 import shell、兼容 facade、插件壳、MITK/BlueBerry/CTK
  wrapper、或外部库对象图的"读取"**不算**验收。
- 每块能力以单测 + 真实样例(`0007_H_AO_H`)集成测为准,`ctest` 全绿才算完成。
