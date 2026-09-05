# 验收原则(替代旧治理层)

> 来源:`plan/README.md`、`plan/00-architecture.md`。本计划已废弃旧版 harness / ledger /
> contract / task-pack 治理层,**不再有 gate / review / projection**。验收只有两条标准。

---

## 两条验收标准

1. **`ctest` 全绿**:每块能力以单测 + 真实样例集成测为准,`ctest` 全部通过才算完成。
2. **`0007_H_AO_H` 端到端跑通**:真实样例项目从打开到主线各阶段结果,端到端走通。

## 「数据进入 XQ 自有对象才算数」

经过 import shell、兼容 facade、插件壳、MITK/BlueBerry/CTK wrapper、或外部库对象图的"读取"
**不算**验收。数据必须落进 XQ 自有 payload / scene 节点,且建立正确的 source/derived 关系。

## 验收纪律(沉淀经验)

- **别 mock 掉测试、别跳过测试当"通过"。**
- 验收必须 Release + 全量 `ctest`;有副作用的调用(如 reader::read)别包进 `assert`——
  `/DNDEBUG` 会删掉 assert,导致假绿后段错误。
- 没真正验证过,别说"完成""通过""可提交"。

## 样例项目

- `0007_H_AO_H`:主线端到端验收的权威样例。
- `0080_H_PULM_H`:备用真实样例。
- 集成测对照物:节点数、分组、关系符合 `expected-scene.json`;已有产物文件(`*.pth/*.ctgr/*.mdl/*.vtp/*.msh/*.vtu`)可读入对照。
