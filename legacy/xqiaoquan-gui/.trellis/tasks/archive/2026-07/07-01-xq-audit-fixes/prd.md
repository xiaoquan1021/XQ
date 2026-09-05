# 修复 XQ 全程序审计缺陷

## Goal

承接 2026-07-01 全程序代码审计结果，将确认缺陷按真实风险拆成独立可验收的 Trellis 子任务，并按风险顺序完成修复、验证和任务收尾。

父任务不直接承载源码改动；源码改动必须落到对应子任务中。父任务负责范围控制、优先级、跨任务依赖、验证总账和最终收口。

## Confirmed Facts

- 当前包是 `XQ`，主要风险面覆盖 `core`、`io`、`services/resource`、`visualization`、`app`、`tests`、`CMakeLists.txt`。
- 当前父任务状态是 `planning`，包含 10 个子任务。
- 当前没有 active task；此前创建任务时没有运行 `task.py start`。
- 审计中已验证 `ctest --test-dir XQ/build_m9be --output-on-failure` 在当时 60/60 通过，但测试未覆盖关键失败路径，不能作为放行依据。
- 当前工作树已有用户/历史未提交内容，执行修复时不得回滚无关改动。

## Requirements

### R1. 按风险顺序执行

执行顺序以审计风险为准，不以修改难度为准：

1. `07-01-fix-command-stack`
2. `07-01-fix-blob-schema-validation`
3. `07-01-fix-geometry-resource-lifetime`
4. `07-01-fix-renderer-connectivity-validation`
5. `07-01-fix-stale-propagation`
6. `07-01-fix-real-app-entry`
7. `07-01-fix-lazy-geometry-gui-path`
8. `07-01-fix-project-close-state-reset`
9. `07-01-fix-portable-test-data-root`
10. `07-01-fix-geometry-sidecar-resolver`

### R2. 每个子任务独立规划、实现和验收

每个子任务进入实现前必须至少具备：

- `prd.md`：问题、触发条件、后果、验收标准。
- 复杂或跨层任务必须有 `design.md` 和 `implement.md`。
- `implement.jsonl` 与 `check.jsonl` 必须包含真实的 spec/research 条目，不允许只保留 seed 行。

### R3. 修复必须保持 XQ 架构约束

- `core` 不引入 Qt/VTK/外部 kernel 依赖。
- scene 变更必须通过 command stack，失败路径不能被当成成功路径。
- source/derived/stale、Source lease、renderer copy-on-upload 等既有契约不能被弱化。
- UI/app 只做调度和展示，不实现领域算法。

### R4. 测试必须覆盖失败路径

每个确认缺陷的修复都必须新增或修改测试覆盖其触发条件，不允许只验证 happy path 或“没有抛异常”。

### R5. 验证必须可复现

每个子任务完成时至少运行对应 targeted tests；影响公共行为、持久化、渲染或资源生命周期时，还需运行相关 build 目录下的扩展测试。父任务收口前必须运行一个明确的全量验证命令并记录结果。

## Acceptance Criteria

- [ ] 10 个子任务均已完成或明确转出范围，父任务 `children` 中没有未处理的 P0/P1 风险。
- [ ] 每个完成的子任务都有对应源码修复、失败路径测试和验证命令记录。
- [ ] 命令栈失败入栈、blob 元数据错误恢复、资源句柄悬空、非法 connectivity 越界这四类 P0 风险均被测试证明已修复。
- [ ] 父任务最终记录全量验证结果，包括命令、build 目录、通过/失败情况。
- [ ] 没有回滚或覆盖用户已有无关改动。

## Out of Scope

- 不重写 XQ 整体架构。
- 不把审计中标为“待验证风险”的事项直接当作确认缺陷修复；需要先在对应子任务中验证。
- 不在父任务中进行大范围源码改动。

## Open Questions

无阻塞问题。当前用户已要求按 Trellis 模式执行。
