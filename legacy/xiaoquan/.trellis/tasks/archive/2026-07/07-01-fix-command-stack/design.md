# Design: Command 执行结果与失败路径

## Contract Change

把 `XQCommand::execute()` 从 `void` 改为返回 `bool`：

- `true`：命令完成了可撤销的 scene 变更，可以进入 undo 栈。
- `false`：命令没有完成可撤销变更，不得进入 undo 栈。

本任务采用 `bool` 而不是复杂 `CommandResult`，因为当前 controllers 只有 `Status::Ok/InvalidInput/ServiceUnavailable` 等粗粒度状态，且本任务目标是阻止失败命令污染栈。更细的诊断可以后续扩展。

## Stack Semantics

`XQCommandStack::push()` 改为返回 `bool`：

- `false`：空命令或执行失败；scene/redo 栈不应发生额外变化。
- `true`：执行成功，命令进入 undo 栈，redo 栈清空。

`redo()` 仍返回 `bool`：

- 从 redo 栈取出命令后重新执行。
- 重新执行成功才进入 undo 栈并返回 true。
- 重新执行失败时不进入 undo 栈；该命令也不应继续留在 redo 栈中，因为它已经无法重放当前状态。

## Scene Commands

### AddNodeCommand

- `execute()` 调用 `scene_->insert(node_)`。
- 只有返回 `Inserted` 才成功。
- `undo()` 只在命令曾成功插入时删除 `id_`。
- 每次 successful redo 后保持可 undo。

### AddNodeWithSourceRelationCommand

- 先插入节点。
- 插入失败直接返回 false。
- 再建立 relation。
- relation 失败时删除刚插入的节点，返回 false。
- undo 只删除本命令成功插入的节点。

### ReplacePayloadCommand

- scene 为空或节点不存在返回 false。
- 首次成功执行时捕获旧 payload/domain。
- 成功替换后返回 true。

### RemoveNodeCommand

- scene 为空或节点不存在返回 false。
- 只有成功删除节点时返回 true。
- undo 只在有 snapshot 时恢复。

## Controller Propagation

所有 `stack_->push(std::move(command))` 调用改为检查返回值：

- push 成功：返回 `Status::Ok`。
- push 失败：返回 `Status::InvalidInput`，因为 service 已经产出 command，但 scene 状态拒绝该变更。

## Compatibility

- 现有 `undo()` / `redo()` bool API 保持。
- `push()` 返回值新增后，忽略返回值的旧调用仍可编译，但本任务会修复生产 controller 调用点。
- tests 可以直接断言 `push()` 返回值和栈计数。

## Risks

- 改 `execute()` 签名会影响所有 command 子类，必须全量搜索 `void execute() override`。
- redo 失败语义需要测试；本任务只确保不会制造数据损坏。
