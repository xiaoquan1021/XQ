# 壳子审查记忆：完成定义、证据边界与补齐顺序

日期：2026-07-12

用途：沉淀本轮对 `D:\XQ` 壳子要求、旧 Shell A、依赖整改和血管影像地基任务的交叉审查。未来 agent 在判断“壳是否完成”、继续依赖整改或拆分血管地基实现任务前，必须先读本文件；不要只凭测试总数或旧任务名称下结论。

## 1. 先固定完成定义，禁止静默漂移

存在两个不同层级的完成定义：

1. `D:\XQ\规划-GoogleEarth全路线与壳子阶段.md` 的“壳档 A”以 A1–A15 为 v1 唯一验收真源，并明确写着修改门控必须升版本号。
2. `.trellis/tasks/07-11-vascular-imaging-foundation/prd.md` 定义的是更完整的外部依赖血管影像/几何地基：真实 CTA、ITK 3D vesselness、自动分割、ITK/VTK 桥、自动中心线/半径/树、真实血管网格和 GUI。

二者不能在汇报时混为一个隐式变化的“壳”。若以后决定用血管影像地基 PRD 替代旧 A1–A15，必须把它明确命名为壳完成定义 v2，并给出逐项迁移/废弃映射；在此之前：

- 旧 Shell A 只能按 v1 条文逐项判断；
- 血管影像地基按自己的 PRD/AC1–AC12 判断；
- 不能用较宽松的旧门槛关闭新任务，也不能用新任务范围反向抹掉旧任务已经完成的有效工程事实。

## 2. 当前诚实状态

### 旧 Shell A

`07-10-google-earth-shell-a` 实际交付的是内部 host/data-spine 竖井：XQ 自有对象、真实 DICOM IO、Source、持久化、lineage/stale、人工 Path/Contour 到 `VesselProfileV1`、Flow geometry smoke、GUI 编排和 Flow ON/OFF。

- 自动回归证据：Flow ON 92/92，Flow OFF 83/83；外部真实 DICOM gate 2/2。
- 任务仍为 `in_progress`，用户实机 GUI 验收未完成。
- 这些结果证明内部宿主、IO 和 smoke 回归，不证明自动血管前处理/中心线/网格地基完成。

### 依赖生产基线整改

`07-11-vascular-foundation-dependency-remediation` 已有技术绿色证据：

- 默认产品 Release 93/93；
- TetGen/MMG research ON/ON 95/95；
- 隔离 QtBase、精确 package roots、build graph 和 PE closure 检查通过；
- TetGen 仍为 `accepted-research-only`。

但 child 仍是 `in_progress`，变更尚未提交/归档；而且整改配方尚未传播到 canonical Shell 构建/运行脚本，不能把“独立整改树通过”表述成“所有壳入口已经迁移”。

### 血管影像父任务

父任务当前只登记三个 child：

- dependency audit：已完成并归档；
- data gate：`planning`，尚无真实 CTA + reference vessel mask；
- dependency remediation：技术绿色但仍 `in_progress`。

父 `implement.md` 规划的 ITK preprocess、auto segmentation、ITK/VTK bridge、centerline tree、real meshing、E2E 六个实现 child 尚未创建。因此当前没有正在交付 `CTA -> vesselness -> mask -> centerline/radius/tree -> surface/mesh` 的实现任务。

## 3. 本轮踩坑与以后必须遵守的判断规则

### 3.1 任务名和测试总数不能代替验收条文

- “Shell A”这个名字不等于 A1–A15 已过。
- Release 全量 CTest 绿是必要回归门，不是范围完成的充分证据。
- 每个完成声明必须写明：完成定义版本、对应条文、实际数据、实际 backend、未验证项。

### 3.2 LIDC-IDRI 只证明 DICOM IO

当前 `D:\XQ\data\dicom\tcia_lidc_idri_0957_ct` 是真实、可追溯的 LIDC-IDRI 胸部 CT，可证明 reader/import/save/reopen/lazy byte recovery。它不是已选定的增强 CTA，也没有参考血管 mask；不能用于证明 vesselness、分割 Dice、中心线、半径、拓扑或网格质量。

### 3.3 旧 A5 与当前几何权威不完全同构

旧 A5 要求 `Path` 本身包含有序点、正半径、弧长和版本校验。当前 `XQPath` 不拥有正半径；solver-facing 的物理几何权威已演化为 `VesselProfileV1`。

- 不能直接声称当前实现严格满足旧 A5。
- 合法做法只有两种：补回符合 A5 的 Path 契约；或把 `Path + evidence -> VesselProfileV1` 正式写入 v2 完成定义并给出迁移说明。

### 3.4 当前 capability gate 不是旧 A8 的模块注册系统

`WorkflowCapabilities` 解决的是编译/运行时 Flow 可用性边界：Flow OFF 不创建执行 controller，同时保留历史对象可读。它不是 `Noop + PathValidate` 的通用模块 registry，也不应冒充一个插件系统。

- 若保留旧 A8 原文，仍需实现/证明等价的“可注册运行且只消费 Path”的模块边界。
- 若当前静态 capability/session 设计就是最终选择，必须在 v2 中明确替代 A8，而不是口头称“等价”。

### 3.5 旧 A13 尚未通过

当前没有生产 ITK 3D thinning、mask-to-centerline、物理距离图半径、图提取/剪枝和树拓扑。vtkvmtk 与 ITKThickness3D 目前只是锁定的 source-only candidates；“源码下载/版本锁定”不等于 adapter 已集成或真实链已执行。

### 3.6 offscreen GUI 不能替代实机 GUI

offscreen 自动测试可以证明 action wiring、worker/commit、对象和回归行为；不能证明真实显示、交互、显卡/驱动和用户操作体验。涉及 GUI 的最终完成声明必须保留一次用户实机验收记录。

### 3.7 canonical Shell 入口仍使用旧依赖基线

以下入口仍指向旧 `install/windows-x64/qt-6.7.0` 和过宽的 `install/windows-x64` prefix：

- `XQ/build_gui_wt.bat`
- `XQ/build_shell_noflow_wt.bat`
- `XQ/run_xq.bat`（运行入口仍需一起核对/迁移）

现有 `XQ/build_gui/CMakeCache.txt` 与 `XQ/build_shell_noflow/CMakeCache.txt` 还记录旧 Qt 和 host `C:\software\anaconda` Python 3.12。整改只在独立 `build_dependency_remediation`/隔离 QtBase 路径被证明；下一步需要把 canonical 配方迁移后 fresh configure，再重跑 ON/OFF 全量测试和 GUI 启动门。

### 3.8 完整 Qt 超集失败不等于壳失败

产品实际只需要 QtBase 闭包。完整 Qt 超集在非必需模块失败时，应保留失败证据，同时在净化环境中构建最小 QtBase；不能为了“完整”引入宿主 zstd/Anaconda 污染，也不能把非产品模块失败夸大成壳不可行。

### 3.9 `ITKVtkGlue` 有二次发现副作用

安装版 `ITKVtkGlue.cmake` 会再次 componentless 查找 VTK、覆盖 `VTK_LIBRARIES`，并间接触发 Python 查找。产品构建必须：

- 在 ITK discovery 前冻结显式 VTK C++ target set，之后恢复；
- 将 Python 限定为锁定的配置期依赖；
- 保证 Python/VTK Python targets 不进入 link graph 或 PE runtime closure。

详细可执行契约已在 `.trellis/spec/XQ/architecture/external-libs.md`，本文件不重复替代该 spec。

### 3.10 TetGen 技术通过不等于许可通过

当前源码自证版本为 1.5，许可边界为 AGPL/commercial。ON/ON 技术测试只允许写 `accepted-research-only`；未取得兼容许可或选定替代 fill backend 前，不得声称可用于专有产品分发。

### 3.11 多 build tree 全量 CTest 必须顺序运行

不同 build tree 的同名测试会共享固定 `%TEMP%` 路径并互删文件。Flow ON/OFF、默认整改树和 research mesh 树的 full CTest 必须顺序执行，先确认旧进程退出；并发失败不能直接归因于代码回归。

### 3.12 计划表不等于任务已经被拆出来执行

父 `implement.md` 中写了 Step 3–8，不代表这些 child 已存在或已开始。状态汇报必须同时检查 `task.json.children`、child `status`、实际 artifact 和真实执行证据。

## 4. 前人工作流的实际复用结论（只谈壳）

已经成功落地/复用：

- GDCM/ITK 的真实 DICOM series IO；
- ITK/VTK/GDCM/MMG/TetGen 的 adapter 隔离思路与 XQ 自有公共类型边界；
- XQ 现有 GUI、Scene、Asset、Source、持久化和 command/lineage 宿主；
- 最小 QtBase、精确依赖发现和构建闭包审计；
- Flow 作为可关闭运行时能力的静态/session 边界。

尚未成功用于生产竖井：

- ITK 3D 去噪 + Frangi/Sato vesselness；
- 自动传统 3D 分割与成熟 ITK 后处理；
- 生产 ITK/VTK 守恒桥；
- vtkvmtk 中心线或 ITK 3D thinning fallback；
- 自动正半径、树拓扑和真实 CTA-derived surface/volume mesh；
- 通用 `Noop + PathValidate` 模块注册边界（若 v1 A8 继续有效）。

## 5. 推荐补齐顺序

1. 先由用户确认：继续以 A1–A15 为壳 v1，还是批准血管影像地基为壳 v2；若是 v2，先写迁移矩阵。
2. 将隔离 QtBase、精确 package dirs、锁定配置期 Python 和 dependency checker 传播到 canonical ON/OFF build/run 入口，fresh configure 后顺序验证。
3. 完成真实 CTA + reference mask 数据门，冻结指标；LIDC 继续只做 IO oracle。
4. 按父计划创建并执行 preprocess、segmentation、bridge、centerline、meshing、E2E children；不能跳到父任务完成声明。
5. 最后分别完成自动证据和用户实机 GUI 验收；两者不可互相替代。

## 6. 主要证据位置

- `D:\XQ\规划-GoogleEarth全路线与壳子阶段.md`
- `D:\XQ\可复用工作打包说明.md`
- `.trellis/tasks/07-10-google-earth-shell-a/scope-correction.md`
- `.trellis/tasks/07-11-vascular-imaging-foundation/prd.md`
- `.trellis/tasks/07-11-vascular-imaging-foundation/implement.md`
- `.trellis/tasks/07-11-vascular-imaging-foundation/research/current-state-audit.md`
- `.trellis/tasks/archive/2026-07/07-11-vascular-foundation-dependency-remediation/research/remediation-report.md`
- `.trellis/spec/XQ/architecture/external-libs.md`
- `.trellis/spec/XQ/architecture/flow-capabilities.md`
- `.trellis/spec/XQ/core/vessel-profile.md`
- `.trellis/spec/XQ/core/build-and-test.md`
