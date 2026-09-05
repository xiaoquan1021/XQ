# 血管地基：真实 CTA 数据门

> Parent: `07-11-vascular-imaging-foundation`
>
> Status: `completed`
>
> 本 child 交付真实数据证据和预先冻结的评价协议，不交付算法好结果。没有实际取得并核验数据时不得完成。

## Goal

取得至少一套可用于首个生产竖井的真实增强 CT/CTA 与参考血管标注，确认来源、许可、去标识、完整性和物理空间，并在任何最终算法结果产生前冻结分割、中心线、半径、拓扑和网格评价协议。

## Confirmed Baseline

- 当前 `D:\XQ` 只有 TCIA LIDC-IDRI 胸部 CT 数据门，来源/license/hash 完整，适合 DICOM IO 验证。
- 当前 LIDC bundle 没有本任务需要的参考血管 mask，也未被选为增强 CTA 血管目标。
- `D:\XQ` 规划文档建议优先增强 CT/CTA，并列出 3D-IRCADb-01 作为门/肝静脉验证候选；该描述不是已下载/已授权/已核验事实。
- 生产运行时保持纯 C++；数据门的图像/标注读取与空间核验最终应由 XQ/ITK C++ 路径证明。

## Requirements

### R1 - 候选数据集必须来自官方来源

- 对候选记录官方 landing/download URL、collection/version/case identifiers、modality/body site、contrast phase、image/label format、reference-label provenance 和维护状态。
- 聚合站、论文二手链接或别人转换后的网盘包不能作为唯一来源。
- 优先能形成单一真实竖井的数据：增强 CT/CTA + 目标血管 voxel mask + 可解释 label semantics。

### R2 - 许可、伦理与去标识证据

- 保存实际下载包对应的 LICENSE/terms、引用要求、非商用限制和再分发限制。
- 保存官方去标识/隐私说明；禁止尝试重识别或在日志输出自由文本患者字段。
- 数据不能合法提交 Git 时，使用外部目录和明确配置；不得把受限样本混进测试 fixture。

### R3 - 完整性和可追溯性

- 原始下载包和抽取文件建立 SHA-256 manifest；记录下载时间、官方标识和抽取规则。
- 检查 archive path safety、文件数量、DICOM preamble/series UID、重复/缺片、损坏文件和 label 文件完整性。
- 保留原始包只读；任何转换产物单独目录并带来源 hash/转换版本。

### R4 - 影像与参考标注空间核验

- 使用实际 XQ/GDCM/ITK C++ 路径读取影像，记录 dimensions、spacing、origin、direction、Series/Frame UID 和体素值范围。
- 读取 reference mask 并证明其与影像在同一 physical space；若需变换/重采样，必须有官方/确定性 transform 和最近邻 label 规则。
- 至少抽查多个角点、中心点和前景边界的 index<->LPS 映射。
- 任何靠肉眼看起来重合、忽略 direction 或默认 identity 的数据包不得通过。

### R5 - Gold 与生产输入严格隔离

- 生产分割只接收 image + versioned parameter profile。
- Gold mask 仅由独立 evaluator 在生产输出冻结后读取。
- 不允许用 gold 生成 seed、ROI、阈值、Path、Contour、中心线或 mesh 输入。
- 最终 held-out case 在冻结指标后才运行；若数据量不足以独立 held-out，必须公开 limitation 和替代交叉验证规则。

### R6 - 预先冻结量化协议

- 在最终算法验证前写入 `validation-baseline.md`：case split、排除规则、参数 profile、metric definitions、numeric thresholds、失败计数和报告格式。
- 分割至少包含 Dice、边界距离指标和 centerline-aware/连通指标。
- 中心线至少包含覆盖/连续性，半径包含物理 mm 误差，拓扑包含适合 label 语义的 branch/endpoint 规则。
- 网格至少包含零翻转/非正体积/域外单元和质量统计。
- 阈值必须结合实际 voxel spacing、label semantics 和标注误差设定；禁止看到最终结果后降线。

### R7 - 当前 LIDC 的边界

- 保留 LIDC 为 DICOM IO/regression gate。
- 文档、CTest 名称和完成记录不得把 LIDC 表述为血管分割/中心线/网格金标准。

## Acceptance Criteria

- [x] AC1：至少一套真实增强 CT/CTA + reference vessel mask 已实际取得，官方 source/version/case ID 可追溯；不是候选清单或空目录。
- [x] AC2：实际 LICENSE/terms、引用、再分发/非商用限制和去标识依据已保存并审阅。
- [x] AC3：原始包与抽取文件 hash manifest 完整，文件数量/series/损坏/重复/缺片检查通过。
- [x] AC4：XQ/GDCM/ITK C++ reader 实际读取影像；reference mask reader 实际读取标注。
- [x] AC5：影像与标注 dimensions/spacing/origin/direction/transform 和 index<->LPS 抽查通过；任何重采样都有确定性记录。
- [x] AC6：生产入口 API/命令不接受 gold mask；独立 evaluator 在生产结果冻结后评分，fingerprint 证明输入分离。
- [x] AC7：`validation-baseline.md` 在最终算法运行前冻结 case split、metrics 和 numeric thresholds。
- [x] AC8：至少定义一个 held-out 或诚实替代协议，失败/排除样本不能静默删除。
- [x] AC9：数据通过外部 root 配置使用，Git status/fixture 扫描证明受限数据未进入仓库，日志无 PHI。
- [x] AC10：LIDC 的角色被固定为 IO-only；本 child 不声称分割或父任务完成。

## Out of Scope

- 优化或实现最终 vesselness/segmentation/centerline 算法。
- 使用深度学习或 Python runtime 生成生产结果。
- 临床数据采集、患者招募、重新标注大型数据集。
- 可信 1D、Darcy、CTC、Flow 或多尺度验证。

## Candidate Decision Rule

优先验证 `3D-IRCADb-01` 是否满足首个门/肝静脉竖井，因为其规划价值最高；但只有实际官方包、license、label semantics 和 physical alignment 都通过时才选择。若下载受限、许可不符或标签空间不可审计，转向下一套公开 CTA/增强 CT 血管标注数据，而不是用 LIDC 或预制 VTP 顶替。
