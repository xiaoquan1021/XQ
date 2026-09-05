# 坐标系与单位(冻结)+ path/contour 语义

> 来源:`plan/00-architecture.md`。**已冻结**,不得擅自变更。
> 来源惯例:SimVascular / ITK(见 `reference-sources.md`)。

---

## 坐标系(冻结)

- **影像 index 坐标**:整数体素索引,按存储的影像轴序。
- **physical 坐标**:由 origin、spacing、direction 导出的连续坐标。
- **world 坐标**:XQ scene 坐标,viewer 与领域关系使用。
- `coordinate_system = LPS`:physical 与 world 坐标采用 DICOM/ITK 患者空间约定。

## 单位(冻结)

- `spatial_unit = mm`:长度、spacing、origin、path、contour、model、mesh 及几何派生量统一用 **mm**。
- 影像 payload 必须存:origin、spacing、direction、scalar type、component count、extent。
- 压力、流量、时间等**非空间单位必须在各 reader / 领域 payload 中显式声明**。
- reader 必须保留已声明的单位;单位缺失或歧义时**发诊断**(不静默假定)。

## path / contour 语义

- path 点带显式坐标空间。
- contour 平面必须引用一个 path frame 或 image/world frame。
- path frame 与 contour 平面在同一 LPS world 坐标系下解释,距离单位 mm。
- slice 局部 contour 坐标(mm)通过引用的 path-frame 基底**抬升**到 LPS world 坐标。

## 变换语义

- 影像、模型、网格的变换必须**显式**。
- 当文件元数据已证明非恒等变换时,任何 reader **不得默默假定恒等变换**。
