# 壳 A：DICOM 来源、夹具与验收政策

## 1. 冻结决策

壳 A 的默认影像资产策略固定为：

```text
DICOM 目录 + 显式 SeriesInstanceUID
→ ITK/GDCM 读取
→ XQImageVolume + XQMemoryImageBufferHandle
→ ResidentVoxelSource
→ ExternalSource Image Asset + Image Scene Node
→ 保存外部定位、UID、几何与影像元数据
→ 重开后按同一 UID 懒重读
```

- 原始 DICOM 不复制进项目资产目录；项目保存的是可解析的外部来源描述。
- `sourceRelPath` 可用时优先按 `.xqproj` 所在目录解析，`sourceAbsPath` 仅作后备。
- `StudyInstanceUID`、`SeriesInstanceUID`、`FrameOfReferenceUID` 是允许持久化的技术身份；重开不得靠“目录里的第一个 series”猜测。
- 首次读取与重开读取必须得到相同的 patient-mm 几何、标量语义和选定 series 身份。
- `activeImage_` 可继续作为 GUI 驻留缓存，但 Project/Scene/Asset 才是权威来源。
- 外部目录缺失、内容漂移或指定 UID 不存在时，返回结构化诊断；不得伪造空影像、自动改选其他 series，亦不得留下半个 node/asset。

### 1.1 ManagedCanonical 的边界

`ManagedCanonical` 不属于壳 A 门槛，不得因“以后可能需要便携项目”而阻塞当前实现。

未来只有在用户显式选择“复制/固化进项目”时才增加：

```text
ExternalSource DICOM Asset
→ ManagedCanonical voxel Asset（content-addressed blob）
```

届时必须保留 source→canonical 血缘、像素语义和完整性摘要；不能用 canonical asset 覆盖或冒充原始 DICOM source。壳 A 不实现自动复制、离线打包或外部源丢失时的静默 canonical fallback。

## 2. 自动化 DICOM 夹具政策

默认 CTest/CI 必须使用仓库内稳定、可再分发、完全去标识的小型静态 DICOM 夹具，不依赖网络、开发者绝对路径、`Externals/build` 临时文件或真实患者目录。

### 2.1 准入条件

每个拟提交 fixture 必须同时满足：

1. 有明确的再分发许可及来源记录；无法确认许可的文件不得提交。
2. 已完成 DICOM 元数据 allowlist 检查；除测试所需技术标签外，其余标签删除或替换为固定合成值。
3. 不含 PatientName、PatientID、IssuerOfPatientID、PatientBirthDate、PatientAddress、PatientTelephoneNumbers、AccessionNumber、ReferringPhysicianName、InstitutionName、StationName、OperatorsName、StudyID、自由文本备注以及任何未审计 private tag。
4. 文件名、目录名和像素中不得包含患者、机构或检查号信息。
5. UID 使用专门为测试生成的固定 UID，不沿用临床系统 UID。
6. fixture 内容、预期 metadata 与预期体素值固定；测试不得在运行时重写黄金输入。
7. 体积保持最小，只覆盖契约，不追求临床尺寸。

允许在项目中持久化的真实数据元信息仅限技术 allowlist，例如：Study/Series/Frame UID、SOP Class、Rows/Columns、PixelSpacing、ImagePositionPatient、ImageOrientationPatient、BitsAllocated、PixelRepresentation、Modality、RescaleSlope/Intercept。新增标签进入项目、日志或快照前必须重新审计。

### 2.2 最小夹具集合

自动化夹具至少包含：

- `regular-oblique/`：一个 3–5 slice 的单分量 CT series，固定非单位 direction、已知 origin/spacing、已知像素值。
- `multi-series/`：同一目录至少两个不同 `SeriesInstanceUID`，用于证明枚举与显式选择；两个 series 应有可区分的尺寸或像素哨兵值。
- `rescale/`：已知 stored pixel、非平凡 slope/intercept 的 CT series，用于锁定 HU/强度语义。
- `invalid/`：至少一个非 DICOM 或截断文件，用于失败原子性。
- 如需验证非均匀层距、混合方向或 enhanced multi-frame，可放极小负向夹具；若许可或大小不合适，可由独立测试资源包提供，但测试预期仍必须固定。

本机已有的 DCMQI `ct-3slice` 只能作为研究/候选种子。许可与去标识复核完成前，不得直接复制入仓库；自动测试也不得引用其 `Externals/build/...` 路径。

## 3. 手工真实数据政策

真实临床规模数据通过 CMake cache 变量提供：

```text
XQ_DICOM_TEST_DATA_ROOT=<本机去标识 DICOM 根目录>
```

- 默认值为空；默认自动 CTest 不访问任何本机临床目录。
- 变量非空时才注册或启用真实数据测试；一旦设置，路径错误、无 series 或读取失败必须显式失败，不得 skip 成绿色。
- 完成壳 A 前，开发机必须至少对一套经过授权且去标识的真实 CT/MR series 跑一次端到端读取、保存、重开和体素复核。
- 真实数据、复制品、截图、主档、测试输出和失败日志都不得提交到 Git。
- 测试报告只记录非敏感汇总：series 数量、尺寸、spacing 范围、方向行列式、标量类型、选定体素摘要/哈希和状态码。
- 控制台、CTest 日志和诊断不得打印 PatientName/PatientID等标签，也不得无条件打印含身份信息的完整目录；必要时只显示脱敏 basename 或路径哈希。

建议的手工门：

```powershell
cmake -S . -B build_gui -DXQ_DICOM_TEST_DATA_ROOT="D:/deidentified-study"
cmake --build build_gui --config Release --target test_dicom_real_series
ctest --test-dir build_gui -C Release --output-on-failure -R "^test_dicom_real_series$"
```

## 4. 像素与几何语义

### 4.1 Rescale 决策

XQ 的 canonical 内存 buffer 保存 ITK/GDCM 完成 DICOM modality rescale 后的数值；CT 即可直接按 HU 解释。为避免重复换算：

- 写入 `XQMemoryImageBufferHandle` 的是已 rescale 数值。
- `XQImageVolume` 对该 buffer 的待应用 rescale 固定为 slope=`1`、intercept=`0`。
- 原始 DICOM slope/intercept 如需审计，仅作为 provenance metadata 保留，不得再次作用于体素。
- 自动测试必须用已知 stored pixel、slope/intercept 验证 `scalarAt()` 等于预期物理强度，并验证不存在 double-rescale。
- 若某 SOP/series 无法形成全卷一致的强度语义，读取失败并报告 `UnsupportedRescale` 或等价结构化状态，不得逐 slice 静默混用。

### 4.2 Multi-series 决策

- 读取分为 `enumerateSeries(directory)` 与 `readSeries(directory, seriesUid)` 两步。
- `readSeries` 的 UID 必填；即使目录只有一个 series，调用者也应传入枚举所得 UID。
- 多 series 且 UID 为空：`AmbiguousSeries`。
- UID 不存在：`SeriesNotFound`。
- UI 可以在仅有一个 series 时预选，但提交读取时仍传明确 UID。
- 保存重开使用持久化 UID，绝不按枚举顺序、描述文本或文件名重新匹配。

### 4.3 Slice 排序与规则体契约

- 排序依据 `ImageOrientationPatient` 定义的法向与 `ImagePositionPatient` 在法向上的投影。
- 禁止使用文件名或 `InstanceNumber` 作为权威排序。
- 同一 series 必须校验 Rows/Columns、标量类型、分量数、方向、in-plane spacing 与 FrameOfReference 一致。
- 重复位置、缺失必要定位标签、方向混合、非单调位置或非均匀层距必须返回结构化诊断。
- 可由单个 affine `origin + direction × spacing × index` 无损表达的规则斜位数据应保留原始 LPS direction。
- 需要剪切、重采样或逐 slice transform 才能表达的 gantry tilt/不规则堆栈不在壳 A 内处理，返回 `UnsupportedGeometry`；不得静默正交化或等距化。

## 5. Unsupported 与诊断政策

壳 A 至少区分以下失败类别；具体枚举名可在设计阶段确定，但对调用者必须可判别：

| 场景 | 预期诊断/行为 |
|---|---|
| 路径不存在或不可读 | `SourceNotFound` / `SourceUnreadable`；零状态提交 |
| 目录无 DICOM series | `NoSeries` |
| 多 series 未指定 UID | `AmbiguousSeries` |
| 指定 UID 不存在 | `SeriesNotFound` |
| 文件截断、解码失败 | `ReadFailed`，指出 slice 序号而非PHI标签 |
| 混合尺寸、方向、像素类型或 frame | `InconsistentSeries` |
| 非均匀层距、剪切/不可表达堆栈 | `UnsupportedGeometry` |
| RGB、palette、非标量或暂不支持的像素格式 | `UnsupportedPixelFormat` |
| enhanced multi-frame（壳 A 若未实现） | `UnsupportedMultiFrame`，不得当普通单 slice |
| rescale 无法形成一致语义 | `UnsupportedRescale` |
| 重开时 locator 可达但 series 内容改变 | `SourceChanged`，不得自动接受新内容 |

所有失败必须满足：无新 Scene node、无新 AssetRecord、无 `activeImage_` 替换、无半写项目文件。

## 6. 验收数据矩阵

| 数据集/场景 | 自动或手工 | 必须断言 | 壳 A gate |
|---|---|---|---|
| 去标识 regular-oblique 单 series | 自动 | UID、dims、spacing、origin、完整 direction、LPS/mm、voxel↔world round-trip、哨兵体素 | 必须 |
| 同目录双 series | 自动 | 枚举稳定；空 UID 拒绝；分别按 UID 读到正确哨兵值 | 必须 |
| 非平凡 slope/intercept CT | 自动 | buffer 为预期 HU/物理强度；XQ pending rescale 为 1/0；无 double-rescale | 必须 |
| `ResidentVoxelSource` 包装 | 自动 | meta 与 volume/buffer 一致；whole/slab/region 字节与体素一致 | 必须 |
| 非 DICOM/损坏文件 | 自动 | 结构化失败；Scene/Asset/UI 状态零变化 | 必须 |
| 未知 UID | 自动 | `SeriesNotFound`；不回退其他 series | 必须 |
| 混合方向或非均匀层距 | 自动或小型负向资源 | `InconsistentSeries`/`UnsupportedGeometry`；不静默修正 | 必须 |
| enhanced multi-frame | 自动或资源包 | 支持则验证帧顺序；未支持则稳定返回 `UnsupportedMultiFrame` | 必须明确行为 |
| ExternalSource 保存→重开 | 自动 | node↔asset、locator、UID、geometry、scalar metadata round-trip；按同 UID 懒重读；体素摘要一致 | 必须 |
| 保存后源目录删除/移动 | 自动 | `SourceNotFound`；项目结构仍可打开但影像不可驻留；无伪造体素 | 必须 |
| 外部内容漂移 | 自动 | 版本化 fingerprint（有序 `SOPInstanceUID + 每实例 file-content SHA-256` 清单摘要）不符时报 `SourceChanged`，包括 UID 未变但内容修改 | 必须 |
| 本机授权且去标识真实 CT/MR（公开可再分发数据优先，但非强制） | 手工，`XQ_DICOM_TEST_DATA_ROOT` | 完整读取、显示、保存、退出、重开、按 UID 恢复、抽样体素/摘要一致；只记录脱敏汇总，不保存或提交真实数据截图 | 完成前必须执行一次 |
| 未审计含PHI数据 | 禁止 | 不运行、不提交、不截图、不记录元数据 | 硬禁令 |

## 7. 完成定义

只有同时满足以下条件，DICOM 数据入口才可计入壳 A：

1. 默认自动夹具全绿，且不依赖开发机临时目录或网络。
2. 一套本机授权、去标识真实 series 通过手工门。
3. ExternalSource 保存重开按持久化 UID 恢复同一数据。
4. PHI allowlist/denylist 检查有测试或可重复审计脚本证据。
5. rescale、multi-series、slice排序和所有 unsupported 行为均有明确断言。
6. Release 全量 `ctest` 通过。
