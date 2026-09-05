# Design: 壳档 A ITK 中心线 B

## 1. Scope and Reuse Boundary

本 child 只关闭 A13 的最小单路径缺口：

```text
XQ SegmentationMask / IVoxelSource
  -> ItkCenterlineSkeletonizer3D
       -> ITKThickness3D BinaryThinningImageFilter3D
       -> ITK SignedMaurer physical distance map
       -> XQ-owned skeleton + radius arrays
  -> CenterlineBGraph (26-neighbor, mm spur pruning, main path)
  -> CenterlineBService (XQPath + segmentation-derived VesselProfileV1)
  -> existing VesselPathSnapshotService / validator / geometry smoke
  -> ProjectNodeBundleCommand (Path + Profile + Assets)
  -> existing Modules page / PathValidate
```

直接复用：

- ITKThickness3D `v5.3.0`, commit
  `36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb`，继续作为现有 ITK 5.4
  上的 header-only filter，不引入第二套 ITK。
- ITK 5.4 `SignedMaurerDistanceMapImageFilter`，显式
  `UseImageSpacing=true`、非平方距离、内部为正。
- `IVoxelSource` 一次 whole-volume lease、`XQPath`、`VesselProfileV1`、
  `VesselPathSnapshotService`、`VesselPathValidator`、
  `ShellGeometrySmokeService`、`ProjectNodeBundleCommand` 和 writer 的现有
  persistence/lineage 机制。

不复用开发探针中的病例/器官策略、MinimalPath、gold evaluator 或 ROI 参数；
探针只作为 26 邻域 endpoint 识别的已验证实现参考。不得引入 Frangi/Sato、
自动分割、vtkvmtk、完整树、Flow、Python 或肝脏特例。

## 2. External Adapter Contract

在 `core/path/ICenterlineSkeletonizer3D` 定义零第三方类型的稳定契约：

- 输入：`ImageGeometry` + `const IVoxelSource&`；只接受 LPS、有限且非奇异的
  direction、正 spacing、匹配 dimensions 的单分量 UInt8 二值体。
- 每次运行只调用一次 `acquire_whole()`；值域严格为 `0/1`。
- 输出：XQ-owned `CenterlineSkeletonV1`，包含完整 geometry、与 mask 等长的
  skeleton UInt8 数组、spacing-aware radius float 数组、前景/骨架计数和已锁定
  thinning/distance backend identity。
- 空 mask、空 skeleton、非法 source/geometry、非二值值、非有限/非正 skeleton
  radius 或 ITK 异常均返回 typed status，且无 output。

`ItkCenterlineSkeletonizer3D` 是唯一 ITK 实现。所有 `itk::` 类型和 filter pipeline
只存在于 adapter `.cpp`；core/service/controller/public header 不出现 ITK。

## 3. Physical Graph and Main Path

`CenterlineBGraph` 是纯 C++ service，可被后续 v2 tree child 原样复用：

1. 按 x-fastest flat index 建立节点，使用完整
   `origin + direction * (spacing * index)` 得到 LPS-mm position。
2. 枚举固定顺序的 26 邻域；邻接表和节点均按 flat index 排序。
3. 原始图必须单连通；断裂图返回 `DisconnectedSkeleton`。
4. 反复从 endpoint 沿 degree-2 chain 走到 junction；长度小于版本化
   `shortSpurLengthMm` 的 endpoint-to-junction 分支被删除，junction 保留。
5. 剪枝后必须至少两个 endpoint。无 endpoint 的环或退化拓扑返回
   `AmbiguousTopology`，不猜端点。
6. 对 endpoint 对运行 spacing/direction-aware weighted shortest path；选择最长
   endpoint geodesic。等长时按 `(startFlat,endFlat)` 字典序，Dijkstra 等距前驱也按
   flat index，确保重复运行得到相同方向与点列。
7. 主路径每点 radius 必须 finite/positive；arc length 从 0 开始按物理边长严格递增。

本任务只输出选中的单主路径；graph 不持久化为 tree。后续 v2 可以复用 skeletonizer
与 graph builder，再扩展 branch collapse/tree topology，不得复制 thinning/distance kernel。

## 4. Domain Assembly

`CenterlineBService` 接收 mask/source stamp、预分配的 Path/Profile NodeId 与 AssetId、
源 Image NodeId 和 frame UID：

- `XQPath` 使用主路径物理点作为 polyline control points，并以输入最小 spacing
  调用现有确定性 resampler；保存 source image identity。
- `VesselProfileV1` 的 sample 与选中图点一一对应；tangent 由相邻物理点计算；
  `areaMm2 = pi * radiusMm^2`；evidence 固定为 `SegmentationDerived`，证据节点固定为
  mask node。
- derivation 固定记录 algorithm/version、thinning/distance backend、26-neighbor、
  spur threshold 和 main-path policy。不得记录 DICOM free text 或患者信息。
- Path/Profile content fingerprint 对 canonical XQ values 做 SHA-256，供原子 Asset
  注册和重开 source identity 使用。
- 依次运行 `VesselProfileValidator`、`VesselPathSnapshotService`、
  `VesselPathValidator` 和 `ShellGeometrySmokeService`。任一失败不产生可提交 bundle。

`VesselProfileV1` 继续是唯一持久化物理面积权威；`VesselPathV1` 仍是重建的只读
snapshot，不新增 payload/domain/project schema。

## 5. Atomic Project Publication

新增 project-aware `CenterlineBController`，采用现有 capture/compute/commit 模式：

- owner thread 捕获 project epoch、mask/image payload identity、revision、stale、Asset
  fingerprint、ScaleSlot 和两个未占用 NodeId/AssetId；worker 只处理复制后的 XQ values。
- compute 通过 `ResidentVoxelSource` 调用 skeletonizer/service，准备两个
  `ProjectNodeBatchSpec`：`mask -> Path`，以及 `Path + mask -> VesselProfile`。
- commit 前重新核对 project epoch、source identity/revision/stale/Asset 和目标 ID；
  然后用一个 `ProjectNodeBundleCommand` 注册两个 derived Assets、插入两个节点和全部
  Scene/Asset lineage。
- 第二个 batch 失败时 bundle 回滚第一个；失败 push 不进入 undo，也不清 redo。
- undo/redo 一次撤销/恢复 Path、Profile、两个 Assets 和关系；mask/source 保留。

## 6. GUI and Flow-OFF Boundary

在现有 Modules 页的 Path 输入区新增：

- 一个 `SegmentationMask` 选择器；
- 一个“Build Centerline B”命令；
- 成功后自动选择新 Profile，并继续使用现有 `PathValidate`、source summary、geometry
  smoke 和 canonical dump；不增加第二个模块页或 Flow 入口。

GUI 不计算 thinning、radius、graph、tangent、source kind 或 validator 结论。长任务走
现有 `XQTaskRunner`；owner-thread commit 后刷新 Scene。该控制器、adapter 与页面在
Flow ON/OFF 均存在，完全不受 `XQ_ENABLE_FLOW` 条件控制。

## 7. Verification and Non-Claims

- 纯 graph tests：直线、弯折、分叉、短/长毛刺、断裂、环、非正 radius、确定性。
- 真实 adapter tests：各向异性 spacing + 倾斜 direction、一次 source acquire、空/坏
  mask，并真正执行 ITKThickness3D 与 SignedMaurer。
- controller/project tests：零半提交、Scene/Asset lineage、stale、undo/redo、保存重开
  后 source fingerprint 与 PathValidate。
- GUI tests：Modules 中可从 mask 运行 Centerline B，结果可选、可 PathValidate；旧入口
  不复活。
- canonical Flow ON/OFF Release full CTest 串行运行；自动证据不替代最终人工 GUI 验收。

结论只允许表述为“vmtk-OFF 的壳档 A 单路径 fallback 可装配”。不声称自动分割、
完整树、Dice/clDice、最大内切球或临床半径准确性。
