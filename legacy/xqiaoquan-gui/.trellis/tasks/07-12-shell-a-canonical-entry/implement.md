# Implementation Plan: 壳档 A canonical 构建入口

## Step 1 — Shared environment

- 添加 `canonical_shell_env.bat`。
- 支持 `XQ_CANONICAL_*` override。
- 固化 isolated Qt、exact dependency roots、locked configure-only Python、ON/OFF build dirs。
- fail-fast 验证关键路径。

## Step 2 — Shared configure/build

- 添加 `configure_canonical_shell.bat ON|OFF`。
- 使用 `cmake --fresh`、exact package dirs 和 narrow prefix。
- 构建后执行 Qt isolation 与 dependency baseline checker。
- 将现有 ON/OFF build wrappers 收窄为薄调用。

## Step 3 — Runtime and verification

- 更新 `run_xq.bat` 使用 canonical ON tree 和 locked runtime bins，透传命令行。
- 添加 `verify_canonical_shell_a.bat`，顺序运行 ON/OFF focused/full suites 和 negative probes。
- 添加操作文档 `CANONICAL_SHELL_A.md`。

## Step 4 — Regression guard

- 扩展 `tests/cmake/check_dependency_baseline.cmake` 检查 canonical scripts。
- 先运行 `cmake -P` source guard，确保坏 literal/缺 helper 会失败。

## Step 5 — Real validation

```powershell
cmake -DXQ_SOURCE_DIR=<repo>/XQ -P XQ/tests/cmake/check_dependency_baseline.cmake
cmd /d /c XQ\verify_canonical_shell_a.bat
```

验证断言：

- fresh caches 指向 locked roots，无 host Anaconda/旧 Qt；
- ON/OFF build graph + PE closure 通过；
- ON/OFF focused/full CTest 串行通过；
- negative configure probes 通过；
- `run_xq.bat` 至少完成 executable/runtime preflight；实机 GUI 由最终 acceptance child 留档。

## Forbidden Actions

- 不覆盖/删除 `build_gui`、`build_shell_noflow`、`build_dependency_remediation`。
- 不修改 Externals 安装或复制 DLL。
- 不并发运行 ON/OFF full CTest。
- 不 `git add .`、不提交、不归档其它 task。

