# Design: Google Earth 式数字孪生壳档 A

## 1. 总体形态

壳 A 不是“先画一个全身地球”，而是先冻结一条可扩展的数据与执行脊柱：

```text
DICOM directory + explicit Series UID
  → ITK/GDCM private adapter
  → XQImageVolume metadata + ResidentVoxelSource
  → ExternalSource Image Asset ↔ typed Image Scene Node [ScaleSlot]
  → XQPath [navigation/frame]
  → XQContourGroup [measurement evidence]
  → VesselProfileV1 [single solver geometry authority]
  → FlowInputAssembler [mm/mm² → CGS + uniform stations]
  → FlowSolver1D fixed L0 smoke
  → XQFlowResult

Every derived edge
  → input revision + DerivationStamp + stale propagation + undo + persistence
```

未来 Whole-body atlas、Darcy、CTC、PhysiCell 等只允许通过新的 typed payload、窄 service/adapter 和明确 lineage 接入，不改变这条宿主脊柱。

## 2. 复用边界

| 现有能力 | 壳 A 用法 | 不做的重建 |
| --- | --- | --- |
| Project/Scene/DataNode/Payload | 权威对象图、稳定 NodeId、typed payload | 不建第二套数字孪生对象图 |
| CommandStack + stale | 原子提交、undo/redo、传递失效 | 不允许 UI/service 直接 mutate Scene |
| AssetRegistry/BlobStore/source/mmap | 外部来源身份、运行时只读 Source、未来 managed 路径 | 不为 DICOM 重造体素接口 |
| Qt/VTK/MPR/renderer/LOD | 显示同一 Scene 对象与 runtime source | 不用 render LOD 冒充 ScaleSlot |
| ITK 5.4 + GDCM 3.0.10 | 私有 DICOM series kernel | 不引 DCMTK、不泄漏外部类型 |
| Path/Contour workflow | 导航标架与测量证据 | 不把半径塞进 Path |
| FlowSolver1D/RCR | 固定参数真实消费者 | 不宣称 M5 科学可信度 |
| WorkflowSession/controllers/TaskRunner | 薄 UI 意图、后台任务与能力开关 | 不建通用插件注册/operation framework |

## 3. 领域契约

### 3.1 ScaleSlot

- 在 `XQDataNode` 上增加可选 `ScaleSlot`，枚举仅含 `Organ/Micro/Cell`；“未指定”由 optional 表达，避免给旧项目静默赋予错误科学语义。
- writer 在新 schema 中持久化；reader 对旧 schema 保持 absent，并产生至多一次信息诊断。
- 属性模型或结构化日志读取它；渲染 `LodOptions` 不引用、也不修改它。

### 3.2 Image payload 与运行时 Source

- 新增 metadata-only typed Image payload，拥有 XQ 自有 `XQImageVolume` 值语义；不持有 ITK image、GDCM dataset 或项目绝对生命周期对象。
- 体素驻留仍由 `IVoxelSource` 表达。首次读取以 `XQMemoryImageBufferHandle + ResidentVoxelSource` 提供；重开时由窄 `ImageSourceResolver` 根据 Image asset category 选择 external DICOM loader 或未来 managed voxel source。
- `activeImage_` 可暂留为 GUI residency cache，但任何重开、headless 或来源查询都必须能从 node→asset→resolver 重建。

### 3.3 DICOM source contract

- 公开 adapter API 只出现 XQ 类型：series descriptor、typed status/diagnostic、`XQImageVolume`、buffer handle。
- API 分两阶段：`enumerateSeries(directory)` 与 `readSeries(directory, seriesUid)`。
- 排序使用 IOP 法向与 IPP 投影；不以文件名/InstanceNumber 为权威。壳 A 只接受可由一个 affine LPS grid 无损表达的规则堆栈。
- canonical buffer 保存 modality rescale 后的值；XQ 待应用 rescale 固定为 1/0，原始 slope/intercept 只作 provenance，防止 double-rescale。
- 外部 asset 保存相对/绝对 locator、Study/Series/Frame UID、几何、像素元数据和版本化 fingerprint。V1 fingerprint 对按 IOP/IPP 验证后顺序排列的每个实例计算 `SOPInstanceUID + file-content SHA-256`，再对有序清单求摘要，既检测换序/缺片，也检测 UID 不变但像素或元数据被修改。
- 导入通过单个 command/transaction 原子创建 Asset + typed node + binding；失败或 undo 不留下孤儿 asset。

### 3.4 VesselProfileV1

```text
contractVersion = 1
frameOfReferenceId
coordinateSystem = LPS
lengthUnit = mm
areaUnit = mm2
sourcePathNode
sourceEvidenceNodes[]
derivationStamp
samples[] {
  VesselSampleId
  arcLengthMm
  positionMm
  unitTangent
  areaMm2
  EvidenceKind
  QualityFlag
  sourceEvidenceId?
}
```

- frame ID 对 DICOM 使用 FrameOfReferenceUID；非 DICOM 必须显式 local frame ID。
- sample ID 独立于 NodeId；从 contour 构建时可由稳定 contour ID 确定性映射。
- 只保存 area；radius 为 `sqrt(area/pi)` 派生视图。
- validator 返回 typed diagnostics，不抛穿 UI 的异常，也不 silent continue。
- 所有 Profile 必须引用现存 Path node。`MeasuredContour` 要求一个或多个现存 Contour evidence nodes；`ImportedGold` 允许 `sourceEvidenceNodes` 为空，但必须在 DerivationStamp 中有非空、版本化 external evidence id/fingerprint。若 gold 已登记为 Asset，则同时建立 asset lineage；不能伪造 Scene evidence node。

### 3.5 DerivationStamp 与 revision

- `XQDataNode` 增加持久化 `contentRevision`；只有 semantic edit 递增，reader/materialize/residency 改变不递增。
- `DerivationStamp` 记录算法 ID/version、参数摘要、可选 seed，以及每个输入的 node ID + content revision；可用时同时记录 asset ID/fingerprint。
- 新增窄型 project-level batch command，持有 `XQProject*` 与 prepared additions（node、optional asset、node↔asset binding、source relations）。它先预验证所有 NodeId/AssetId/source，再原子注册 assets、插入 nodes、建立 Scene relations 和可用的 Asset lineage；任一步失败逆序回滚。AssetRegistry 增加最小可逆 unregister/remove-relation API，自动 id 计数仍单调。
- Profile measured 路径有 Path+Contour 多父；ImportedGold 至少有 Path scene parent，并在 evidence asset 存在时有 asset parent；FlowResult 有 Profile+Case 多父。writer 在 work copy 中只做幂等 lineage reconciliation，不能成为运行时原子性的替代品。
- semantic replace command 保存 stale snapshot，execute 标记下游，undo 恢复旧 payload/revision/stale；保留现有普通 replace 用于非语义 materialize。

## 4. 服务边界

### 4.1 VesselProfileAssembler

- 纯 C++、无 Qt/VTK/ITK/GDCM。
- 输入 Path、ContourGroup、显式 source unit/frame/provenance；检查 sourcePath 绑定、闭合轮廓、唯一 ID、平面与 path frame 一致。
- contour area 在 service 内统一计算；position/tangent 来自 `path.frameAtArcLength`。
- imported-gold 走同一 validator；legacy cm 必须显式 unit，转换为 canonical mm/mm²。

### 4.2 FlowInputAssembler

- workflow prepare 在 Scene 主线程捕获 node id、payload clone、content revision 和 stale 状态；stale 则拒绝。纯 assembler 接受该 immutable value snapshot 与 case/run 参数，不直接查询 Scene。后台完成后 controller 在提交前再次核对 node 存在、revision 未变且仍非 stale，否则丢弃结果。
- assembler 对 snapshot 内 Profile 做 contract validation，输出 `Result<FlowSolver1D::SolverInput, diagnostics>`。
- 唯一完成 mm→cm(×0.1)、mm²→cm²(×0.01)；弧长先减首站，再转 CGS。
- 现有 solver 假设均匀 dx，因此 station policy 由 typed protocol 显式提供；`FlowSmokeProtocolV1` 固定 N=11、包含首末端并线性插值。BC/material/waveform 不回写 Profile。
- V1 L0 smoke 仅接受 total length 50–500 mm、resampled area 50–2000 mm²，固定 dt=1e-4 s、200 steps、2 cycles，并复用现有稳定 waveform/RCR/fluid；越界或 CFL violation 失败且不自动改参。
- 失败零副作用，不提交部分 input/result。

### 4.3 Flow capability

- `WorkflowCapabilities` 由构建配置/factory 注入 `XQWorkflowSession`。
- Flow ON：构造现有 FlowController，provider 从 SimulationCase 的显式 profile node 取数。
- Flow OFF：controller 为空，stage 显示 unavailable；reader 仍认识 SimulationCase/FlowResult 以便只读历史数据。
- `XQ_ENABLE_FLOW=OFF` 只做必要的 target/source/UI guard，不扩展成插件 ABI。

## 5. 持久化策略

- 项目 schema 计划从 1.2 写为 1.3，reader 继续读取 1.2；最终版本号以实现前当前 writer 常量复核为准。
- 新 schema 覆盖 metadata Image payload、ScaleSlot、VesselProfileV1、contentRevision、DerivationStamp、ContourGroup typed payload 和多父/asset lineage。
- ExternalSource 是壳 A 的 DICOM 真源；ManagedCanonical 不在本任务实现。
- writer 在所有 node→asset binding 确定后镜像 Scene lineage 到 AssetRegistry；不能把 ContourGroup 误映射成 Image。
- 未知 Profile contract version 或非法单位不能按 Unknown payload 继续求解。

## 6. UI 接缝

- 新增独立“Open DICOM Series”目录入口；enumerate 后即便只有一组也以明确 UID 提交 read。
- MainWindow 不组装 Image asset、不算 contour area、不做 CGS 转换、不以名称/顺序猜输入。
- 属性面板显示 ScaleSlot、frame/unit、Profile version/来源/revision、stale 和 `L0 geometry smoke` 标签。
- 复用 TaskRunner/busy/progress/diagnostic；重复点击不产生重复提交。

## 7. DICOM fixture 与 PHI

- 自动测试使用仓库内小型、许可确认、allowlist 去标识 fixture：regular-oblique、multi-series、rescale、invalid。
- 真实数据由可配置 `XQ_DICOM_TEST_DATA_ROOT` 提供；默认不访问本机临床目录，设置后失败不能被 skip 成绿。
- 日志和项目禁止 PatientName/PatientID/机构/自由文本/private tags；只保存允许的技术身份与脱敏汇总。可提交截图只能使用仓库内审计过的去标识 fixture，真实数据人工门不截图、不提交路径或主档。

## 8. 依赖顺序与停止线

```text
domain-contracts
      ↓
dicom-spine
      ↓
profile-assembly
      ↓
flow-smoke
      ↓
flow-capability
      ↓
e2e-gate
```

逻辑上 profile assembly 只依赖 domain contracts，但本仓库采用单 worktree/channel 集成，DICOM 与 Profile 都会修改共享 CMake/controller/app 接缝，因此实施顺序冻结为 T1→T2→T3→T4→T5→T6；不得在同一工作树并发编辑 T2/T3。

达到 parent PRD 的 AC 后立即停止。Operation catalog、atlas、Darcy、CTC、PhysiCell 和肿瘤转移作为独立后续任务，不能顺手塞进壳 A。

## 9. 回滚设计

- 每个 child task 是一个独立 review/revert 单元，可由一段连续、不得与其它 child 交错的 focused commit series 组成；task 记录最终 commit，并在 implement 记录该 child 的 commit range。后续 child 只依赖已通过 gate 的完整 series。
- schema/领域契约是第一硬门；旧 1.2 读取与新格式 round-trip 未通过，不进入真实 DICOM/Flow UI 接线。
- DICOM adapter、Profile assembler、Flow smoke、capability UI 各有独立接缝，可分别回滚；不得用删除用户未跟踪文件或 hard reset 回滚。
