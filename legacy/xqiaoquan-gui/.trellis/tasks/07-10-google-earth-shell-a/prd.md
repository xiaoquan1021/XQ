# Google Earth 式全身多尺度数字孪生：壳档 A

## Goal

在现有 XQ 工程上交付一个可继续装入“芯”的最小稳定壳：真实 DICOM 影像进入 XQ 自有 Project/Scene/Asset/Source 数据脊柱，沿 `Image → Path → Contour → VesselProfileV1 → flow_geometry_smoke` 形成可保存、可重开、可追溯、可失效、可撤销的纵向切片，并以持久化 `ScaleSlot` 和可关闭的 Flow 能力证明后续器官、微观、细胞尺度模型可以在不重建宿主的前提下接入。

本任务不声称已经完成全身数字孪生、肿瘤转移模拟或可信患者血流；它只完成支撑这些目标的壳档 A。

## Product Requirements

### R1 — 最大化复用现有宿主

- 复用现有 `XQProject`、`XQScene`、typed payload、`AssetRegistry`、Blob/Source/资源管理、命令栈、lineage/stale、TaskRunner、Qt/VTK 视图和项目生命周期。
- 复用现有 `XQPath` 作为导航/重采样/截面标架，复用 `XQContourGroup` 作为截面测量证据，复用现有 `FlowSolver1D` 作为第一个真实消费者。
- ITK/GDCM 只作为 DICOM kernel/codec，不能进入 core 或公开 service API。
- 不引入 Slicer/MITK/CTK 宿主、Cesium、嵌入式 Python、动态 DLL 插件市场或万能 Context/OperationRegistry。

### R2 — 真实 DICOM 数据脊柱

- 提供“枚举目录内 series”与“按显式 `SeriesInstanceUID` 读取”两阶段能力；不得默认读取目录中的第一组 series。
- 读取结果必须进入 XQ 自有 `XQImageVolume`/image payload、`IVoxelSource`、ExternalSource Image Asset 和 Image Scene Node，而不是只存于 `MainWindow::activeImage_`。
- patient physical/world 坐标固定为 LPS，空间单位固定为 mm；dimensions、spacing、origin、完整 direction、scalar/component 语义及 Study/Series/Frame UID 必须保留。
- 壳 A 默认采用 `ExternalSource + UID + fingerprint`，保存重开后按同一 UID 懒重读；不把整卷 DICOM 自动复制进项目。ManagedCanonical 仅作为后续显式“固化进项目”能力。
- 源目录缺失、内容漂移、UID 不存在、不规则堆栈或暂不支持的 DICOM 类型必须返回结构化诊断，且不得提交半个 node/asset。

### R3 — 薄多尺度身份

- 引入 first-class `ScaleSlot { Organ, Micro, Cell }`，以显式可选字段随 Scene node 持久化；旧项目缺失时保持未指定，不能把视图缩放或渲染 LOD 当成生物/物理尺度。
- 至少一个属性面板或结构化日志必须读取并显示 ScaleSlot；保存重开后值不丢失。
- 壳 A 不实现 ScaleNode 树、ROI registry、语义缩放或按相机 zoom 自动触发求解。

### R4 — 单一求解几何真源

- 保持职责分离：`XQPath` 只负责导航/标架，`XQContourGroup` 只负责测量证据，`VesselProfileV1` 是 1D/未来 CTC/血管网络的唯一物理血管几何。
- `VesselProfileV1` 必须显式声明 contract version、frame、LPS/mm/mm²、来源、算法/参数/revision，并保存稳定 sample ID、弧长、位置、单位切向、正面积、证据类型和质量。
- Profile 只保存权威面积；等效半径可派生但不能成为第二真源。
- pure Profile validator 必须逐项拒绝错误版本/单位/frame、非有限值、重复 ID、非递增弧长、非正面积和坏切向；assembler/controller 的 context validation 负责 source node 存在、Path/Contour 绑定与标架一致。两层均返回 typed diagnostics，禁止静默跳过坏截面。

### R5 — Profile 装配与失效

- 提供纯 C++ `VesselProfileAssembler`，支持 `Path + Contour` 与显式 imported-gold profile 两条输入路径。
- DICOM/path canonical 输入使用 mm/mm²；legacy SimVascular cm 数据必须由调用者显式声明并在边界转换，禁止按文件名猜单位。
- Profile 与其所有父来源建立原子多父关系；Path 或 Contour 的 semantic edit 必须递增 content revision，并传递 stale 到 Profile 和后续结果。
- 后台惰性解析/materialize 不是 semantic edit，不能制造假 stale；undo/redo 必须恢复 payload、revision 和编辑前 stale 集。

### R6 — 第一个真实消费者

- workflow prepare/commit 层负责在 Scene 中拒绝 stale 或 revision 改变的 Profile；纯 C++ `FlowInputAssembler` 接受其 immutable value snapshot，集中完成契约验证、mm→cm、mm²→cm²，并把非均匀 Profile 重采样为现有 solver 所需的均匀 stations。
- 通过冻结的非患者 waveform/RCR/fluid/time 参数执行 `VesselProfileV1 → FlowInputAssembler → FlowSolver1D → XQFlowResult`。
- 结果必须 finite、consistent、可持久化，并记录 Profile/Case revision、assembler/solver 版本和固定参数标识。
- UI、日志和测试必须明确标为 `L0 geometry smoke`；不得称为 M5 可信血流或患者特异预测。

### R7 — Flow 可关闭

- 使用薄 `WorkflowCapabilities`/factory injection 控制 Flow；关闭时不创建 FlowController，Flow UI 明确 unavailable，影像、Path、Contour、Profile、渲染和存档仍可用。
- 提供独立 `XQ_ENABLE_FLOW=OFF` 配置构建/测试，证明壳在未启用 Flow 执行能力时仍能编译、启动和打开包含历史 FlowResult 的项目。
- 不实现动态加载、热插拔、插件市场或通用 operation catalog。

### R8 — 保存、重开、血缘和诊断

- 完整链 `Image/Asset → Path → Contour → VesselProfile → SimulationCase → SmokeResult` 必须用 typed payload 保存重开；尤其补齐当前 ContourGroup round-trip/AssetKind 缺口。
- Scene 多父关系、Asset lineage、content revision、stale reason、ScaleSlot、DerivationStamp 和 node↔asset 绑定必须 round-trip。
- 项目格式变化必须有明确 schema 版本和旧格式兼容策略；未知新版本不得退化成 `Unknown` 后继续计算。
- 诊断和日志不得包含患者/机构身份字段。

### R9 — 同一条 headless/GUI 链

- GUI 只收集意图、显示进度/诊断并提交 service 返回的 command；不得在 `XQMainWindow` 内计算 contour area、做单位换算或挑选“第一个匹配对象”。
- Headless 与 GUI 必须复用相同 adapter/service/command 链；GUI-private cache 不能成为业务真源。
- 长任务沿用现有 TaskRunner/busy 防重复提交机制。

## Constraints

- 基线仓库：`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui`，基线提交 `f1f6230`，base branch `feat/render-arch`。
- 规划通过后统一在 `feat/google-earth-shell-a` 分支实现；六个 child task 分批提交，便于独立回滚。
- 不触碰、不清理、不提交已有未跟踪文件：`CHECK-*.md`、`EXECUTE-*.md`、`XQ/xq_app_dist/`。
- 保持依赖方向 `app → services → adapters/io → core`，core 零 Qt/VTK/ITK/GDCM。
- Release 副作用调用不得放入 `assert`；失败路径必须零状态提交。
- 自动 DICOM fixture 必须去标识、许可可再分发、离线稳定；真实数据只通过 `XQ_DICOM_TEST_DATA_ROOT` 使用，禁止提交 PHI。

## Out of Scope

- 全身图谱、患者全身配准、完整血管树和语义瓦片层级。
- ROI registry、ScaleNode、zoom-triggered simulation。
- 自动中心线 SOTA、生产级 vmtk、M5 血流可信度、患者 BC 反演。
- Darcy、CTC、PhysiCell/BioFVM、肿瘤生长或转移全过程模拟。
- ManagedCanonical 离线打包、云同步、法规归档、临床用途。

## Acceptance Criteria

- [ ] AC1：自动 DICOM fixture 可枚举并按显式 UID 读取；LPS/mm 几何、斜位 direction、rescale 后体素、UID 和 voxel↔world 往返正确。
- [ ] AC2：DICOM 导入原子建立 typed Image node、ExternalSource Image asset、node↔asset 绑定和 `IVoxelSource`；失败不留下半状态。
- [ ] AC3：保存退出重开后按持久化 UID 恢复相同 series 与体素摘要；源缺失、漂移或 UID 不存在返回稳定诊断，不改选其他 series。
- [ ] AC4：`ScaleSlot` 三值均能保存重开并被 UI/日志读取；渲染 LOD/相机缩放不改变 ScaleSlot。
- [ ] AC5：`VesselProfileV1` validator 的版本、单位、frame、sample、绑定和几何负路径全部覆盖；Profile round-trip 不丢字段。
- [ ] AC6：synthetic contour 与显式 cm fixture 分别正确组装为 canonical LPS-mm/mm² Profile；Path 不新增权威半径。
- [ ] AC7：`FlowInputAssembler` 用合法三点 Profile 精确验证 `[10,15,20] mm → [0,0.5,1] cm`、`100 mm² → 1 cm²`，并对非均匀输入产生均匀 stations。
- [ ] AC8：固定 `flow_geometry_smoke` 产生 finite/consistent `XQFlowResult`，保存重开数值和来源一致，且全程标为 `L0 geometry smoke`。
- [ ] AC9：Image/Path/Contour/Profile/Case/Result typed payload、双父关系、Asset lineage、revision、stale、DerivationStamp 和 ScaleSlot 全量 round-trip。
- [ ] AC10：编辑 Path 或 Contour 会传递 stale 到 Profile/Result；undo/redo 恢复 payload、revision 和 stale snapshot；惰性 materialize 不触发 stale。
- [ ] AC11：Flow runtime capability OFF 与 `XQ_ENABLE_FLOW=OFF` 构建均通过；非 Flow 壳能力和历史结果只读加载不受影响。
- [ ] AC12：MainWindow 不再拥有 contour→area/solver input 业务拼装或散落单位换算；GUI/headless 使用同一 service/command 链。
- [ ] AC13：旧 VTI/SV/1.2 项目读取回归通过，新 schema 完整 round-trip；ContourGroup 不再被错误映射为 Image asset。
- [ ] AC14：`0007_H_AO_H` 现有端到端链不回归；新增一套授权去标识真实 DICOM 通过 `XQ_DICOM_TEST_DATA_ROOT` 手工门。
- [ ] AC15：`cmd /c build_gui_wt.bat`、Release 全量 `ctest`、`cmd /c run_xq.bat` 及独立 no-flow 构建/测试均通过。
- [ ] AC16：达到以上门槛后停止扩大壳 A；全身图谱、Darcy、CTC、PhysiCell 和肿瘤转移进入后续“芯”任务。
