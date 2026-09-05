# Implementation Plan: XQ 审计缺陷修复总任务

## Phase 1: Parent Planning

- [x] 读取父任务和子任务状态。
- [x] 写入父任务 PRD、设计、执行计划和审计摘要。
- [ ] 为父任务和第一个子任务填充 `implement.jsonl` / `check.jsonl`。

## Phase 2: P0 子任务

1. Start `07-01-fix-command-stack`
   - 补齐子任务 PRD/design/implement。
   - 修复 command result / stack recording / relation rollback / controller propagation。
   - 测试：`test_command_stack`、workflow/controller 相关测试。

2. Start `07-01-fix-blob-schema-validation`
   - 补齐 blob role schema、checked arithmetic、relPath 边界。
   - 测试：`test_blob_store`、project reader/roundtrip/lazy geometry 相关测试。

3. Start `07-01-fix-geometry-resource-lifetime`
   - 修复 handle/source ownership。
   - 测试：`test_geometry_resource_manager`、`test_geometry_source_resolver`。

4. Start `07-01-fix-renderer-connectivity-validation`
   - reader 和 renderer 双层校验非法 connectivity。
   - 测试：visualization progressive/scene renderer 相关测试。

## Phase 3: P1/P2 子任务

继续按父任务 PRD 顺序执行 P1/P2 子任务。每个子任务必须独立 start、实现、验证、finish。

## Parent Closeout

- 运行全量验证命令。
- 确认全部子任务状态。
- 更新必要 spec。
- 提交任务 artifacts 和源码修复。
