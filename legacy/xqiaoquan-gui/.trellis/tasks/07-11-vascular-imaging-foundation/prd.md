# 血管影像外部依赖地基

> Trellis phase: Phase 2 / Execute
>
> Task status: `in_progress`
>
> 所有生产 children 已建立规划；当前依赖整改已形成报告，真实数据门仍未关闭。任何 child 完成均不等于父任务或完整壳交付。

## Goal

用成熟的 C++ 医学影像与几何外部依赖，建立一条可复现、可替换、可在真实数据上量化验收的生产链：

```text
真实增强 CT / CTA DICOM
  -> XQ 自有 Volume / IVoxelSource（LPS, mm, 完整 direction）
  -> ITK 3D 去噪 + 多尺度 vesselness
  -> 全自动传统血管分割 + ITK 后处理
  -> ITK <-> VTK 守恒桥
  -> 自动中心线 + 半径 + 树拓扑
  -> 血管表面 / 体网格
  -> XQ Scene / GUI
```

当前阶段的产品价值是把“影像处理做不好、几何前端不可信”这个根问题解决掉，为后续 1D 血流提供真实患者血管几何。当前阶段不是持久化壳、Flow smoke、可信 1D、多尺度调度或肿瘤转移模拟。

## Scope Correction

旧任务 `07-10-google-earth-shell-a` 交付了有价值的内部宿主与数据脊柱，包括 DICOM 读取、XQ 自有对象、Source、持久化、lineage、GUI 编排和 Flow 可关闭能力。它不等于本任务所定义的“外部成熟依赖地基”，原因是旧链仍依赖人工 Path/Contour，缺少 ITK 3D vesselness、自动三维分割、自动中心线/半径/拓扑和真实 CTA 金标准验收。

因此：

- 保留旧任务及其测试结果作为历史事实，不重写历史。
- `92/92`、`83/83` 或 LIDC DICOM IO 通过，只证明回归与 IO，不证明血管几何地基完成。
- 旧任务不得归档或对外表述为“外部依赖壳完成”。
- 本任务是新的完成定义与后续执行入口。

## Confirmed Current State

| 能力 | 当前真实状态 | 本任务判断 |
| --- | --- | --- |
| VTK / ITK / GDCM / MMG 安装 | VTK 9.3.0、ITK 5.4.0、GDCM 3.0.10、MMG 5.3.9 已安装 | 仅“已安装”，仍需 ABI、模块、许可和真实 ON 构建审计 |
| DICOM | 已有生产 GDCM/ITK series reader，显式 UID、LPS/mm、direction、rescale、fingerprint 可用 | 可复用的数据入口 |
| 当前真实 DICOM | `D:\XQ` 下为 TCIA LIDC-IDRI 胸部 CT | 只能验 DICOM IO；无血管金标准，不得验收分割 |
| ITK 分割 | 已有 SimVascular ITK 二维截面 level-set | 不是三维自动血管分割 |
| 三维传统分割 | 主要是 XQ 自写阈值、6 连通区域生长、最大连通域 | 必须由 ITK 生产链替换/升格 |
| ITK 3D 去噪 / vesselness | 未实现 | 本任务 P0 |
| ITK 形态学 /可靠连通域 | 未实现为生产管线 | 本任务 P0 |
| ITK<->VTK 官方桥 | 已装 ITKVtkGlue，但项目未实际使用官方 bridge | 本任务 P0 |
| Path / centerline | 当前 Path 由人工控制点创建 | 不得冒充自动中心线 |
| vtkvmtk | 当前 Externals 未发现 vtkvmtk 安装或 adapter | 条件接入，先做时间盒探针 |
| 3D thinning fallback | 已装 ITK 未发现 `BinaryThinningImageFilter3D` / Thickness3D 模块 | fallback 也需真实依赖工作，不能预先算已有 |
| TetGen / MMG | TetGen 1.5.1、MMG adapter、TetGen->MMG 两阶段内核已有，默认 OFF | 现有验证主要是程序生成弯管；需真实血管生产验证 |
| GUI / Scene / Source / persistence | 已有较完整宿主 | 复用，不推倒重写 |

## Product Requirements

### R1 - 外部依赖、ABI、许可与可复现构建先过门

- 建立单一依赖清单，锁定 VTK 9.3.0、ITK 5.4.0、GDCM 3.0.10、MMG 5.3.9、TetGen 实际版本和中心线后端来源。
- 核对 x64、MSVC toolset、C++ 标准、`/MD`、Release/Debug、运行时 DLL、ITK/VTK 模块集合，禁止混链。
- ITK 必须按实际所需 COMPONENTS 配置并在目标中真实链接；“头文件存在”不等于生产能力已接通。
- vtkvmtk 必须只抽取所需 C++ 模块，不使用 vmtk SuperBuild，不引入 Python 运行时；VTK 9.3 已知问题必须通过最小真实探针暴露。
- TetGen 的头文件版本与仓内 LICENSE 存在需要正式核对的许可风险；未形成书面结论前，不得宣称可分发的生产体网格后端。
- 外部类型只允许存在于 adapters 或 service 私有实现；公开业务 API 只暴露 XQ 自有类型。

### R2 - 建立真实增强 CT/CTA + 参考标注数据门

- 至少选定一套公开、去标识、来源和许可明确的增强 CT/CTA 血管数据，包含可用于量化的参考血管 mask；优先选择一套可形成单部位完整竖井的数据。
- 数据包必须记录官方来源、下载标识、许可、去标识依据、文件清单、SHA-256 和本地目录规则。
- 参考 mask 只作为评价 oracle，不得作为生产自动分割的 seed、ROI 或输入捷径。
- 数据集的影像与标注必须完成同一物理空间核验：dimensions、spacing、origin、direction、frame/transform 和体素到 LPS 映射。
- 当前 LIDC-IDRI 样本继续保留为 DICOM IO gate，但明确排除出血管分割、中心线和网格最终验收。
- 受限许可数据不得提交进 Git；通过显式 `XQ_VASCULAR_TEST_DATA_ROOT` 或同等配置使用，CI 不联网、不含 PHI。

### R3 - ITK 三维预处理与 vesselness

- 使用 `itk::Image<...,3>` 建立真实三维管线，而不是逐切片二维伪三维。
- 至少包含各向异性扩散去噪和多尺度 Hessian objectness/vesselness；Frangi 作为主基线，Sato 可作为同接口对照。
- 所有尺度参数按物理 mm 解释，不能把 voxel index 当 mm；方向、spacing、origin 全程保留。
- 输出参数、算法版本、输入 fingerprint、耗时和诊断，能够复现同一结果。
- 合成直管/弯管可做单元测试，但不能作为本要求最终通过证据。

### R4 - 全自动传统三维血管分割与后处理

- 生产成功路径不得要求用户逐例提供 seed、Path、Contour 或金 mask。
- 基线采用可解释的传统链：vesselness + 影像强度约束 + 自动阈值/滞后连接 + 形态学 + 连通域/重连策略；必要时可由自动初始 mask 驱动 level-set 细化。
- 参数允许按 modality / vascular site 使用版本化 profile，但禁止验收时针对每个病例手工调参后只报最好结果。
- 最大连通域不能成为唯一规则；必须处理细支断裂、分叉抑制和错误孤岛，并输出可诊断原因。
- 输出进入 XQ 自有 `XQSegmentationMask`、Scene、lineage 和持久化链，失败时零半状态提交。

### R5 - ITK<->VTK 桥与空间守恒

- 实际使用 ITKVtkGlue 官方桥或等价的官方 ITK/VTK 连接方式，不能只做两个独立 reader 后声称“桥已接”。
- 桥接前后必须守恒 dimensions、spacing、origin、完整 3x3 direction、LPS/mm、scalar/component 语义和 voxel/world round-trip。
- VTK 表面提取和 GUI 显示使用同一物理空间，不允许以转置、翻轴或隐式 identity 修补视觉位置。
- 外部对象生命周期留在 adapter/visualization 私有实现，不能泄漏进 core/public service API。

### R6 - 自动中心线、半径和树拓扑

- 定义 XQ 自有的中心线树结果：稳定节点/分支 ID、LPS-mm 点列、弧长、正半径、父子连接、端点/分叉类型、算法 provenance 和质量标记。
- 路线 A：时间盒接入 `vtkvmtkPolyDataCenterlines` 等必要 C++ 模块，验证 VTK 9.3 兼容补丁、ABI 和许可。
- 路线 B：若 A 未通过，接入真实三维 thinning/skeletonization + 物理距离图半径 + 图提取/剪枝；当前 ITK 安装没有所需 3D thinning filter，必须补齐依赖或选择另一可审计 C++ 实现。
- A/B 都必须通过相同真实数据门；fallback 不是“有输出就算”，必须验证连续性、半径、分叉和拓扑。
- 最终 E2E 不得使用人工 `XQPath` 控制点代替自动中心线。

### R7 - 真实血管表面与体网格生产验证

- 从自动 mask 生成闭合、方向一致的真实血管表面，记录清理、平滑、简化参数及拓扑变化。
- TetGen/MMG 必须在真实弯曲/分叉血管表面上以 ON 构建执行；现有程序生成弯管仅保留为单元回归。
- 体网格验收至少覆盖零翻转、零非正体积、零明显域外单元、退化统计、质量分布、边界 marker 守恒和参数敏感性。
- 若 TetGen 许可不满足目标分发方式，任务保持未完成，必须明确选择合规后端或限定研究构建；不得静默回退到已知会产生域外单元的 star fan 后声称完成。

### R8 - 真数据端到端与 GUI

- 同一生产服务链完成：`CTA -> vesselness -> mask -> centerline/radius/tree -> surface -> volume mesh -> Scene/GUI`。
- Headless 与 GUI 调用同一 adapters/services；GUI 只收集意图、显示进度/诊断和提交 command，不实现算法分支。
- 成功路径保存、释放运行时资源、重开项目后，mask、centerline tree、surface/mesh、坐标、provenance 和 lineage 一致。
- 至少完成一次用户实机 GUI 验收；offscreen 自动测试不能替代实机显示与交互检查。

### R9 - 防假完成与证据纪律

- Mock、Noop、伪造结果、测试专用算法路径、金 Path/Contour 旁路不得通过生产门。
- 合成数据只做单元/病态回归，不得作为最终真实数据验收。
- 每个量化阈值必须在查看最终结果前由数据门任务冻结；禁止跑完后调低门槛。
- Release 全量 CTest 绿是必要回归条件，不是充分完成条件。
- 完成记录必须逐项列出真实命令、构建配置、数据 ID/hash、指标、失败样本和已知限制；没跑过的项必须写“未验证”。

## Acceptance Criteria

- [ ] AC1：依赖/ABI/许可报告完成；所选生产配置可从干净 Release build 复现，日志证明 ITK/GDCM/VTK、中心线后端和网格后端在对应真实链中实际执行。
- [ ] AC2：真实增强 CT/CTA + 参考 mask 数据门完成，来源/许可/去标识/hash/空间对齐均有证据；LIDC 不被用于血管算法验收。
- [ ] AC3：ITK 3D 去噪 + 多尺度 vesselness 在真实数据上运行；参数以 mm 表示，输出可复现且无逐切片伪三维。
- [ ] AC4：无需人工 seed/Path/Contour 的传统自动分割在冻结验证集上运行；Dice、clDice/中心线重合、HD95 或等价边界指标、连通性和分支保留指标全部报告，并达到数据门预先冻结的阈值。
- [ ] AC5：ITK<->VTK 桥的 dimensions/spacing/origin/direction/LPS/scalar 守恒和 voxel/world round-trip 通过倾斜 direction 的自动测试及真实数据抽查。
- [ ] AC6：自动中心线结果在真实数据上连续，半径全部 finite/positive，树图无悬空引用/重复边；中心线、半径和拓扑指标达到预先冻结阈值。
- [ ] AC7：真实血管表面和生产体网格通过翻转、非正体积、域外、退化、质量分布和边界 marker 检查；对应后端在 ON 构建中真实执行。
- [ ] AC8：真实数据 headless E2E 从 CTA 自动运行到 mask/tree/surface/mesh，保存重开后结果、坐标、provenance、lineage 一致；成功路径不读取金标作为输入。
- [ ] AC9：GUI 使用同一生产链完成导入、运行、查看、保存、重开；用户实机验收记录存在，offscreen 测试不冒充实机验收。
- [ ] AC10：坏 DICOM、错 frame、空 vesselness、分割失败、断裂 skeleton、开口表面、中心线后端失败、网格后端失败均返回稳定诊断且不提交半状态。
- [ ] AC11：默认 Release 全量 CTest、相关 ON 构建全量 CTest 和真实数据门全部通过；没有 skip/mock/Noop 代替生产执行。
- [ ] AC12：最终完成记录明确声明本任务只完成血管影像/几何地基，不声称可信 1D、Darcy、CTC、ONNX、多尺度孪生或临床能力完成。

## Out of Scope

- FlowSolver1D 可信化、Boileau/openBF 对标、患者边界条件反演。
- Darcy 微循环、1D-3D 耦合、CTC/粒子追踪、肿瘤转移预测。
- ONNX、深度学习分割、Python 运行时、TotalSegmentator 运行时。
- 3D Slicer/MITK/CTK 整机、vmtk SuperBuild、Cesium。
- 全身稠密重建、真正多尺度调度、语义缩放和 Level 4 数字孪生。
- 临床诊断、治疗决策、法规或商用分发声明。

## Planning Decisions Still Resolved Inside This Task

以下不是让实现者边做边猜，而是前两个 execution child 必须在算法编码前关闭的门：

1. 最终采用的公开增强 CT/CTA + 金标准数据集及其许可。
2. 基于该数据标注语义、spacing 和样本量预先冻结的量化阈值。
3. vtkvmtk 是否通过 VTK 9.3/ABI/许可探针；若不通过，三维 skeletonization fallback 的具体来源。
4. TetGen 当前 AGPL 许可与目标研究/分发方式是否兼容；不兼容时采用哪个合规体网格路径。

这些门没有关闭前，可以完成审计、数据准备和最小探针，但不得开始“跑出一个结果再定义通过线”的生产实现。
