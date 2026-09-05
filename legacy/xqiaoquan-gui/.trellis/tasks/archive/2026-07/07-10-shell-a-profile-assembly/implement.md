# Implementation Plan：血管剖面装配

## Preflight

- 确认 `shell-a-domain-contracts` 完成并通过 Profile round-trip/validator gate。
- 搜索现有 contour area、Path frame、loft input 和多 source command 逻辑，优先复用已有工具。
- 运行 `test_path`、`test_contour_group`、`test_contour_group_path_binding`、`test_centerline_frame_service`、`test_scene_relations` 基线。

## Steps

1. **实现几何辅助与严格错误矩阵**
   - 复用 `projectToFrame` 和 Path frame API。
   - 实现或复用 simple polygon/area 检查，集中容差常量。
   - 单测 open、重复点、退化、自交、frame mismatch、越界弧长。

2. **实现 VesselProfileAssembler**
   - 定义 options/result/status，不依赖 scene/UI。
   - 生成稳定 sample id、position/tangent/area/provenance。
   - 调用共享 validator，保证输出只有全成功或全失败。

3. **实现 imported-gold typed path**
   - 增加显式 version/frame/unit/existing Path/external evidence/sample 输入转换，不新增用户文件格式。
   - 测试 canonical mm、legacy cm ×10/×100、未知版本、错误 units/count/finite、missing Path/evidence，以及 optional evidence asset lineage。

4. **实现 typed controller 并复用原子多 source command**
   - prepare 阶段在主线程检查 stale 并捕获 payload/revision snapshot；worker 只计算；commit 前重检 revision/stale，再用 project-level command 写 node/asset/binding/relation。
   - 添加 command failure rollback、undo/redo、stale propagation 测试。
   - 添加 ScaleSlot 显式指定、全来源一致传播、absent/冲突不猜测测试。

5. **消除新增重复路径**
   - 将可共享的面积/质量逻辑放在 service，不在 MainWindow 复制。
   - 记录旧 flow GUI assembly 为 flow-smoke child 的删除/替换目标。

## Focused validation

```powershell
cmake --build XQ/build_gui --config Release --target test_vessel_profile_assembler test_vessel_profile_import test_vessel_profile_controller test_project_lifecycle test_project_node_batch_command test_main_window test_app_startup test_path test_contour_group test_contour_group_path_binding test_centerline_frame_service test_scene_relations test_workflow_controllers
ctest --test-dir XQ/build_gui -C Release --output-on-failure -R "vessel_profile_(assembler|import|controller)|project_lifecycle|project_node_batch_command|main_window|app_startup|path$|contour_group|centerline_frame|scene_relations|workflow_controllers"
```

## Final validation

```powershell
cmd /c XQ\build_gui_wt.bat
ctest --test-dir XQ/build_gui -C Release --output-on-failure
```

## Review gates

- Path 不得新增权威 radius/area。
- assembler 不得依赖 MainWindow、Qt、VTK 或 solver。
- sample id 必须由稳定输入决定，不能每次运行随机重建。
- 默认不得静默跳过坏 contour。
- Profile 必须同时链接 Path 与 Contour 来源，并通过 transitive stale 测试。

## Rollback points

- Commit A：polygon/frame helper + tests。
- Commit B：assembler + validator integration。
- Commit C：imported-gold typed conversion。
- Commit D：controller/multi-source command/stale tests。
- 若 controller 集成失败，保留已验证 pure service，不把临时 scene mutation 放入 UI。
