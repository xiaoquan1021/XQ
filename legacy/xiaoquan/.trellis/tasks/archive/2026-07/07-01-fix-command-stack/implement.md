# Implementation Plan: fix-command-stack

## Steps

1. 更新 command API
   - `XQCommand::execute()` 改为 `bool execute()`。
   - `XQCommandStack::push()` 改为 `bool push(...)`。
   - `XQCommandStack::redo()` 只在 execute 成功后入 undo 栈。

2. 更新 scene command 子类
   - `AddNodeCommand` 增加成功插入状态。
   - `AddNodeWithSourceRelationCommand` 增加原子插入+link，link 失败回滚。
   - `ReplacePayloadCommand` 对 missing node 返回 false。
   - `RemoveNodeCommand` 对 missing node 返回 false。

3. 更新 controller 调用点
   - 搜索 `stack_->push`。
   - 根据返回值决定 `Status::Ok` 或失败状态。

4. 增加回归测试
   - `test_command_stack` 覆盖重复 id、missing source、missing replace/remove target。
   - controller 测试覆盖 push 失败时不返回 ok。

5. 验证
   - 先构建/运行 `test_command_stack`。
   - 再运行 `test_workflow_controllers` 和 `test_workflow_integration`。
   - 若构建系统受影响，运行 `ctest --test-dir XQ/build_m9be --output-on-failure -R "command_stack|workflow"`.

## Rollback Points

- 如果 API 改动影响范围超出 command/controller，先停止并记录调用点，不继续扩大到无关服务。
- 不修改 stale 传播或 scene relation 模型。
