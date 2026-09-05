# 修复命令栈失败入栈导致数据删除

## Goal

修复 `XQCommandStack` 和 scene command 的失败路径：命令只有在实际成功修改 scene 后才允许进入 undo 栈；undo/redo 只能回滚本命令确实做过的变更；控制器不得在命令执行失败后继续返回成功。

## Problem

当前 `XQCommand::execute()` 返回 `void`，`XQCommandStack::push()` 在 `command->execute()` 后无条件把命令压入 undo 栈并清空 redo 栈。

确认缺陷：

- `AddNodeCommand::execute()` 忽略 `XQScene::insert()` 的 `DuplicateNodeId`。
- `AddNodeCommand::undo()` 无条件 `scene_->remove(id_)`。
- `AddNodeWithSourceRelationCommand::execute()` 忽略 `insert()` 和 `link_derived()` 的失败。
- 多个 controller 在 `stack_->push(...)` 后直接 `return Status::Ok`。

## Trigger Scenarios

1. 场景已有 `NodeId(1)`。
2. 调用 `stack.push(new AddNodeCommand(... NodeId(1) ...))`。
3. `insert()` 返回 `DuplicateNodeId`，但命令仍进入 undo 栈。
4. 用户执行 undo。
5. 原本存在的节点被删除。

关系命令的等价触发场景：

- source 节点不存在。
- derived id 冲突。
- relation 自环或重复。
- link 失败但已插入 derived 节点。

## Requirements

### R1. 命令执行结果必须显式返回

`XQCommand::execute()` 必须返回可检查结果。`XQCommandStack::push()` 和 `redo()` 必须根据结果决定是否记录命令。

### R2. 失败命令不得污染 undo/redo 栈

- `push(nullptr)` 保持 no-op。
- `push(failing command)` 不改变 scene，不增加 undo 栈，不清空 redo 栈。
- `redo(failing command)` 不应把失败命令重新压回 undo 栈。

### R3. AddNodeCommand 只回滚自己的成功插入

重复 id 或空 scene 不得导致后续 undo 删除既有节点。

### R4. AddNodeWithSourceRelationCommand 必须原子化

插入和建立 source relation 必须同时成功才算成功。若 link 失败，必须回滚已插入的新节点。

### R5. ReplacePayloadCommand / RemoveNodeCommand 失败路径不得入栈

目标节点不存在、scene 为空等 no-op 情况必须被报告为失败或 no-op，不得进入 undo 栈制造虚假可撤销操作。

### R6. Controller 必须传播命令执行失败

`PathController`、`SegmentationController`、`ModelingController`、`MeshingController`、`FlowController`、`AiController` 等调用 `stack_->push()` 的位置必须根据返回结果决定 `Status`。

## Acceptance Criteria

- [ ] 重复 id 的 `AddNodeCommand` 不增加 `undo_count()`，undo 不删除原节点。
- [ ] 缺失 source 的 `AddNodeWithSourceRelationCommand` 不留下未关联 derived 节点。
- [ ] relation 建立失败时，已插入节点被回滚，scene 与执行前一致。
- [ ] `ReplacePayloadCommand` 目标不存在时不入栈。
- [ ] `RemoveNodeCommand` 目标不存在时不入栈。
- [ ] controller 在 command push 失败时返回失败状态，不返回 `Status::Ok`。
- [ ] 现有成功路径 undo/redo 行为保持。
- [ ] 相关 tests 通过，至少包含 `test_command_stack` 和 workflow/controller 相关测试。

## Out of Scope

- 不在本任务中修复 stale 传递传播；该问题属于 `07-01-fix-stale-propagation`。
- 不重写 scene 关系模型。
- 不改变 service 计算逻辑，只修复命令提交和错误传播契约。
