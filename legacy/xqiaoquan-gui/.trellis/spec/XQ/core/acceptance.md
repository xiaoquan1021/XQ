# 验收原则(替代旧治理层)

> 来源:`plan/README.md`、`plan/00-architecture.md`。本计划已废弃旧版 harness / ledger /
> contract / task-pack 治理层,**不再有 gate / review / projection**。自动验证有两条必要门槛，
> 但功能实现与用户实机验收才是工作目标；自动全绿不构成完成证明。

---

## 两条自动验证门槛

1. **`ctest` 全绿**:每块能力以单测 + 真实样例集成测为准；失败必须处理，但全绿只说明
   已声明的自动检查没有发现问题。
2. **`0007_H_AO_H` 端到端跑通**:真实样例项目从打开到主线各阶段结果,端到端走通。

两条都通过仍不等于用户要求的功能已经实现。功能缺失、工作流错误、旧入口仍在、只改测试、
或用户明确要求的实机审查尚未完成时，任务必须保持未完成状态。

## 「数据进入 XQ 自有对象才算数」

经过 import shell、兼容 facade、插件壳、MITK/BlueBerry/CTK wrapper、或外部库对象图的"读取"
**不算**验收。数据必须落进 XQ 自有 payload / scene 节点,且建立正确的 source/derived 关系。

## 验收纪律(沉淀经验)

- **别 mock 掉测试、别跳过测试当"通过"。**
- **先实现功能，再验证功能。** 不得为了让测试通过而保留、隐藏、伪造或改名一个本应被取代
  的旧功能，也不得用修改断言代替产品行为。
- 每次写或改测试前，先写清它保护的真实工作流：输入、用户操作、可观察结果和失败行为。
  说不清实际功能时，不得为了覆盖率或绿色数字新增测试。
- 汇报顺序固定为：实际功能变成了什么、用户如何操作、还缺什么、最后才是自动证据。
  禁止以测试数量、构建日志或文件列表冒充功能说明。
- 验收必须 Release + 全量 `ctest`;有副作用的调用(如 reader::read)别包进 `assert`——
  `/DNDEBUG` 会删掉 assert,导致假绿后段错误。
- 没真正验证过,别说"完成""通过""可提交"。

## Scenario: 功能目标先于测试

### 1. Scope / Trigger

- Trigger: 实现或替换用户可见功能、修复行为缺陷、增删测试、汇报任务状态。
- Purpose: 防止把工程工作退化成“让测试变绿”，或用自动证据掩盖真实功能尚未实现。

### 2. Signatures

每项实现都必须能用下面四元组描述，不要求新增代码类型：

```text
FeatureBehavior = Input + UserAction + ObservableResult + FailureBehavior
VerificationEvidence = FeatureBehavior + ExactCheck + Result
```

### 3. Contracts

- `FeatureBehavior` 来自用户要求、权威规划或当前 Trellis PRD；测试不是需求来源。
- 产品代码必须在没有测试进程参与时完成相同工作流；测试不得注入隐藏成功路径。
- 替换语义意味着旧入口、旧控件和旧行为退出产品面，除非需求明确要求兼容共存。
- 自动测试只证明它实际执行并断言的行为；未覆盖、未实机观察或仍由用户验收的部分必须明确
  标为 pending。
- 用户要求实机审查时，自动检查全部通过后任务仍保持 `in_progress`，不得 finish/archive。
- 状态汇报先描述真实输入、操作与输出；测试总数只作为次要证据。

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| 只修改测试或断言，产品行为未改变 | 不得声称实现；返回产品代码继续处理 |
| 测试全绿，但目标入口不可见或不可操作 | 功能失败；不能以绿色结果关闭任务 |
| 新入口存在，但旧入口仍隐藏/禁用保留 | 若要求是替换，则判定未实现 |
| GUI 显示成功，但没有真实 service/controller/domain 输出 | 判定为壳或 stub，不算功能完成 |
| 自动链路通过，用户实机验收待定 | 记录自动证据，任务保持 `in_progress` |
| 功能、错误路径和实机操作均符合要求 | 才可由用户决定是否验收完成 |

### 5. Good/Base/Bad Cases

- Good: 用户导入真实 DICOM，准备 Path 输入，运行 PathValidate，看到来源/几何/dump，保存重开
  后再次运行；产品代码完成全链，测试只复现并检查该链路。
- Base: 一个纯 core 服务无 GUI，但真实调用者传入契约对象即可得到确定结果；单测验证同一公开 API。
- Bad: 为了绿色数字修改测试去接受当前实现，保留需求要求删除的旧入口，或汇报“98/98”却不说
  用户到底能做什么。

### 6. Tests Required

- 测试名称和断言必须对应已写明的 `FeatureBehavior`，至少覆盖成功结果和一个真实失败行为。
- 跨层功能优先走真实输入与真实 controller/service，不以 mock 替代关键领域动作。
- 替换功能必须断言新工作流可操作，并断言旧产品入口确实不存在。
- 测试不能替代实机视觉、交互手感、外部设备或用户明确保留的人工验收。

### 7. Wrong vs Correct

#### Wrong

```text
改了测试，98/98 全绿，所以功能完成。
```

#### Correct

```text
功能：用户从 Modules 页准备 Path，运行 PathValidate，看到来源、几何摘要和稳定 dump；
保存重开后仍能再次运行，旧 Flow 壳入口不存在。
自动证据：对应真实链路的检查通过。
人工状态：用户实机验收 pending，任务保持 in_progress。
```

## 样例项目

- `0007_H_AO_H`:主线端到端验收的权威样例。
- `0080_H_PULM_H`:备用真实样例。
- 集成测对照物:节点数、分组、关系符合 `expected-scene.json`;已有产物文件(`*.pth/*.ctgr/*.mdl/*.vtp/*.msh/*.vtu`)可读入对照。

## Scenario: DICOM-first 壳与自动 Path 实机验收

### 1. Scope / Trigger

- Trigger: 声称 DICOM 医学影像处理壳、自动中心线或完整 Shell A 主路径可实机使用。
- Purpose: 固定首次输入、自动/手工边界和可声称范围，防止用预制项目、Mask 或手工 Path
  冒充自动 DICOM 主流程。

### 2. Signatures

```text
XQ\run_xq.bat
File -> Open DICOM Series -> <DICOM directory>
DICOM -> XQ Volume -> automatic processing -> SegmentationMask
      -> Centerline B -> Path + VesselProfile -> PathValidate
File -> Save Workspace -> <name>.xqproj
```

### 3. Contracts

- 首次输入是一个真实 DICOM Series 目录；`.xqproj` 只用于保存/恢复，不能作为首次输入前提。
- DICOM 必须经 GDCM/ITK 进入带 spacing/origin/direction 的 XQ Volume，并在 MPR/3D 可见。
- 自动 Path 声称要求产品流程真实产生 `SegmentationMask`，再自动生成 Path/Profile 并运行
  PathValidate。合成 Mask GUI test 只证明 child，不证明这条完整链。
- 用户要求“替代旧代码”时，手工点 Path/画轮廓不能作为完成路线；旧入口必须按 PRD 移除、
  降级或明确保留为 fallback，不能继续充当默认主流程。
- 固定器官 ROI/病例策略可以作为 disclosed 样例算法，但不得被描述成通用 DICOM 壳的产品身份。
- 每个阶段分别记录通过/pending；基础 DICOM 通过不得自动关闭自动 Path 验收。

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| 要求用户先提供含 Mask 的 `.xqproj` | 验收设计错误；回到 DICOM-first |
| DICOM 可显示，自动处理没有 Mask | 只通过 IO/显示，自动 Path pending |
| Mask->Path 合成测试通过，真实 DICOM 链未运行 | child 自动证据通过，完整壳 pending |
| 用户必须手工点 Path 才能继续 | 不满足自动替代要求 |
| 固定肝/门静脉 ROI 链成功 | 只证明该 disclosed 路线，不证明通用性 |
| 保存/重开后来源与结果恢复 | 持久化通过；仍需逐项确认自动来源诚实 |

### 5. Good/Base/Bad Cases

- Good: `run_xq.bat` 打开真实 DICOM，产品自动得到 Mask，Centerline B 自动输出
  Path/Profile，PathValidate 成功，保存重开后来源和结果仍有效。
- Base: 真实 DICOM 导入、MPR/3D、保存重开通过，明确记录自动处理尚未闭环。
- Bad: 用历史 `.xqproj`、合成 Mask 或手工 Path 完成演示后声称完整自动 DICOM 壳通过。

### 6. Tests Required

- 自动：真实 DICOM adapter/integration、真实 ITK Mask->Centerline B、PathValidate、持久化和
  lineage/stale/undo 回归。
- 实机：从 `run_xq.bat` 和 DICOM 目录开始，记录每个可见阶段、失败信息和保存重开。
- 替换要求：测试新自动主入口可操作，并验证旧手工入口符合 PRD 规定的移除/降级状态。

### 7. Wrong vs Correct

#### Wrong

```text
先打开一个已经含 SegmentationMask 的 .xqproj，再点 Build Centerline B；因此完整壳通过。
```

#### Correct

```text
从真实 DICOM 开始分别记录 Volume、自动 Mask、自动 Path/Profile、PathValidate 和重开结果；
任一阶段缺失就只报告前一阶段通过。
```
