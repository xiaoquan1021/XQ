# Design: XQ 审计缺陷修复总任务

## Task Model

父任务 `07-01-xq-audit-fixes` 是 orchestration task。它不直接改源码，负责：

- 保存审计结论和风险顺序。
- 确认每个子任务的范围、依赖和验收。
- 防止跨子任务修复互相覆盖。
- 在全部子任务完成后做全量验证和收口。

源码改动必须落在子任务中。每个子任务可以单独 start、实现、check、finish。

## Execution Strategy

执行策略是 P0 严格优先：

1. 先修会造成数据删除、数据损坏、UAF、越界访问的 P0。
2. 再修会造成状态错误或产品入口不可用的 P1。
3. 最后修测试可复现性和性能策略不一致的 P2。

如果某个子任务发现根因跨越多个子任务，只允许在当前子任务修复其直接依赖的最小公共契约；其余问题记录回父任务或对应子任务，避免无边界扩张。

## Cross-Task Dependencies

- `fix-command-stack` 会改变 command 执行结果协议，可能影响 controllers 和 service tests。它必须先于其他 workflow 修复。
- `fix-blob-schema-validation` 与 `fix-renderer-connectivity-validation` 都涉及非法 geometry。reader 应该尽早拒绝损坏数据，renderer 仍需防御外部 source。
- `fix-geometry-resource-lifetime` 可能影响 lazy geometry GUI path，因此应先于 `fix-lazy-geometry-gui-path`。
- `fix-portable-test-data-root` 最好后置，避免测试路径调整干扰前面缺陷验证。

## Validation Design

每个子任务验证分三层：

- Unit/targeted：直接覆盖缺陷触发条件。
- Adjacent：运行相关模块现有测试，确认契约变化没有破坏调用链。
- Parent closeout：所有子任务完成后运行一次全量 `ctest`。

## Rollback

如果某个子任务实现导致范围失控：

- 保留任务 artifacts 和失败证据。
- 回退该子任务内的源码改动，不回退其他用户/历史改动。
- 在父任务 research 中追加阻塞说明，再重新拆分。
