# Implementation Plan：Flow 能力可关

## Steps

1. 增加 `WorkflowCapabilities`、集中 factory/查询和 runtime OFF session tests。
2. 让 Flow stage/action 只读取 capability 投影并呈现 unavailable。
3. 增加默认 ON 的 `XQ_ENABLE_FLOW`，集中调整 target sources/link/tests，并新增带完整已验证工具链/prefix 的 `build_shell_noflow_wt.bat`。
4. 建立独立 no-flow Release build，验证项目加载、影像/Profile、渲染、保存和历史 FlowResult。
5. 回归默认 ON smoke 与全量 ctest。

## Validation

```powershell
cmake --build XQ/build_gui --config Release --target test_workflow_capabilities test_workflow_session test_main_window test_flow_geometry_smoke
ctest --test-dir XQ/build_gui -C Release --output-on-failure -R "workflow_capabilities|workflow_session|main_window|flow_geometry_smoke"
cmd /c XQ\build_shell_noflow_wt.bat
ctest --test-dir XQ/build_shell_noflow -C Release --output-on-failure -R "workflow_capabilities|app_startup|project_roundtrip|main_window|arch_boundaries"
cmd /c XQ\build_gui_wt.bat
ctest --test-dir XQ/build_gui -C Release --output-on-failure
```

## Review and rollback

- Gate：OFF build 不能靠跳过 app/project tests 变绿；ON 默认行为不能回归。
- Commit A：runtime capability；Commit B：UI unavailable；Commit C：CMake OFF gate。
- 若 OFF target 切分失败，可保留已验证 runtime OFF 作为中间 checkpoint，但本 child 与 parent 必须保持未完成/blocked，直到独立 no-flow build gate 修复；不得以 runtime OFF 替代硬门，也不得引入 Noop registry 规避。
