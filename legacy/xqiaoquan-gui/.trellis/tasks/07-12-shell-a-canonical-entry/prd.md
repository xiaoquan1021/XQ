# 壳档 A：canonical 构建入口

> Status: in_progress
>
> Parent: `07-12-shell-a-v1-alignment`

## Goal

将 dependency remediation 已验证的隔离 QtBase、精确 package roots、配置期 Python 和依赖闭包检查传播到开发者实际使用的 Flow ON、Flow OFF 和 GUI run 入口，关闭父任务 A1 并为 A15 提供固定复现脚本。

## Dependencies

- `.trellis/tasks/archive/2026-07/07-11-vascular-foundation-dependency-remediation/research/remediation-report.md`
- 隔离 QtBase prefix 和当前 XQ package/target 收口变更保持可用。
- 本 child 不重新做依赖版本/许可审计，只消费审计结论。

## Requirements

- `XQ/build_gui_wt.bat`、`XQ/build_shell_noflow_wt.bat`、`XQ/run_xq.bat` 使用同一 canonical dependency configuration，禁止三份路径继续漂移。
- Qt 指向 `windows-x64-vascular/qt-6.7.0`；VTK/ITK/tinyxml2/GDCM 指向锁定的精确 roots。
- `CMAKE_PREFIX_PATH` 不包含过宽 `install/windows-x64` 总根。
- 安装包元数据触发的 Python 只解析到锁定 Externals 3.11，并且只存在于 configure cache；host Anaconda、Python link/runtime closure 均为零。
- 使用 fresh canonical ON/OFF build trees，不覆盖旧 build trees、失败树或整改证据树。
- Flow ON/OFF full Release CTest 串行运行；同时执行 dependency-focused、负向 package、build graph 和 recursive PE 检查。
- GUI run wrapper 使用与 canonical ON build 相同的 runtime roots，并能接受可配置 project path。
- 所有脚本失败均返回非零，不能在错误 package path 后静默 fallback。

## Acceptance Criteria

- [ ] AC1：三条正式入口不再引用旧 Qt、过宽 prefix 或 host Anaconda。
- [ ] AC2：fresh ON/OFF cache 的 package/version/root identity 与整改锁一致。
- [ ] AC3：ON/OFF Release build 和 full CTest 顺序通过；focused dependency tests 通过。
- [ ] AC4：build graph、recursive PE closure 和负向 package probes 通过，禁止 runtime/link 命中为零。
- [ ] AC5：GUI 从 canonical ON tree 启动，Qt plugin 和全部非系统 DLL 从锁定 roots 解析。
- [ ] AC6：复现文档记录命令、目录、日志位置和失败诊断；未修改旧安装/旧 build evidence。

## Out of Scope

- Path/module/centerline 业务能力。
- 重建 VTK/ITK/GDCM 或解决 TetGen 分发许可。
- 删除旧 build trees、清理用户工作区或提交其它 task 的变更。
