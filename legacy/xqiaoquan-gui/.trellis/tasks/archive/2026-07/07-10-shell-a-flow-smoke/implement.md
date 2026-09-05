# Implementation Plan：1D 血流几何冒烟

## Preflight

- 确认 domain-contracts 与 profile-assembly 已完成并提交。
- 运行现有 `test_flow_solver_1d`、`test_flow_integration`、`test_boundary_condition_service` 基线。
- 从现有稳定 transient 测试提取 fixed protocol 数值，不自行发明未经测试参数。
- 搜索并记录 MainWindow 旧 `arcLength/area0` assembly，作为本任务删除/替换清单。

## Steps

1. **实现 FlowSmokeProtocolV1 与 assembler**
   - 定义 protocol、station mapping、conversion record 和 status。
   - 集中验证、非均匀→均匀 station 线性重采样，并转换 mm/mm² 到 CGS。
   - 添加 exact conversion、显式 Organ/absent/Micro/Cell、固定 N=11/端点/等间距/插值映射、envelope 边界、invalid matrix 和不修改输入测试。

2. **实现 FlowGeometrySmokeService**
   - 调用真实 `FlowSolver1D::solve`。
   - 校验 result consistency/finite/segment mapping。
   - 在 core typed payload 中增加 protocol/conversion/station-map/source-revision DTO，生成 SimulationCase、FlowResult 和 provenance bundle；service-only 类型不得成为持久格式。

3. **实现 atomic bundle command/controller**
   - worker prepare，main-thread commit。
   - 插入 Case/Result 和 Profile→Case、Profile→Result、Case→Result relations；完整 rollback/undo/redo。
   - 添加 solver fail、duplicate id、relation fail 的零副作用测试。

4. **持久化与 stale**
   - round-trip protocol/source/mapping/result series。
   - Path/Contour 改动后断言 Profile、Case、Result 一次性 transitive stale。

5. **替换 GUI 旧路径**
   - UI 改为选择 Profile 并调用 controller。
   - 删除或封闭 MainWindow 内 contour area 和 solver unit assembly。
   - 明确显示固定 smoke 协议，不冒充正式 Flow run。

## Focused validation

```powershell
cmake --build XQ/build_gui --config Release --target test_flow_input_assembler test_flow_geometry_smoke test_flow_solver_1d test_flow_integration test_scene_relations test_project_roundtrip test_workflow_controllers test_main_window
ctest --test-dir XQ/build_gui -C Release --output-on-failure -R "flow_input_assembler|flow_geometry_smoke|flow_solver_1d|flow_integration|scene_relations|project_roundtrip|workflow_controllers|main_window"
```

## Final validation

```powershell
cmd /c XQ\build_gui_wt.bat
ctest --test-dir XQ/build_gui -C Release --output-on-failure
```

## Review gates

- solver 必须真实执行，不能用 Noop/metrics 替代。
- mm/cm 和 mm²/cm² 只能在 assembler 出现一次。
- fixed protocol 必须版本化，UI/结果标为 `L0 geometry smoke`。
- 非均匀 Profile 必须输出严格等间距 solver stations 及可追溯插值映射。
- V1 的 N=11、50–500 mm、50–2000 mm²、dt/steps/cycles 边界必须由真实 solver 测试锁定；不得自动 retry。
- result 必须能映射回 profile stable sample/station。
- 失败、undo、redo 和 stale 必须验证整条 bundle 原子性。

## Rollback points

- Commit A：protocol + FlowInputAssembler。
- Commit B：smoke service + solver tests。
- Commit C：bundle command/controller + lineage。
- Commit D：GUI replacement + persistence/stale regressions。
- 若 GUI 集成出现问题，保留通过检查的 headless chain，回滚 UI commit；不得恢复重复科学逻辑作为永久方案。
