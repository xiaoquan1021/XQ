# Design: 壳档 A canonical 构建入口

## 1. Boundary

本 child 只把已经验证的 dependency-remediation 配方传播到开发者实际入口，不改变 XQ 产品依赖版本或业务代码。

```text
canonical_shell_env.bat
  -> configure_canonical_shell.bat ON/OFF
       -> build_gui_wt.bat / build_shell_noflow_wt.bat
  -> run_xq.bat
  -> verify_canonical_shell_a.bat
```

## 2. Shared Environment Contract

新增 `XQ/probes/vascular_foundation/canonical_shell_env.bat`，集中产生：

- repo/Externals/test-data roots；
- isolated QtBase、VTK、ITK、GDCM、HDF5、tinyxml2、configure-only Python roots；
- exact CMake package dirs 与 narrow prefix；
- canonical ON/OFF build dirs；
- CMake/CTest/vcvars/dumpbin paths。

所有值允许通过 `XQ_CANONICAL_*` 环境变量覆盖；默认值从脚本位置和现有安装布局推导。脚本必须 fail-fast 检查关键文件/目录。

## 3. Configure/Build Contract

新增 `configure_canonical_shell.bat <ON|OFF>` 作为唯一 configure/build 实现：

- `VSLANG=1033`、Ninja、Release、`cmake --fresh`；
- explicit `Qt6_DIR/VTK_DIR/ITK_DIR/tinyxml2_DIR`；
- locked `Python3_ROOT_DIR/EXECUTABLE/INCLUDE_DIR/LIBRARY`；
- `XQ_ENABLE_TETGEN=OFF`、`XQ_ENABLE_MMG=OFF`；
- ON/OFF 只改变 `XQ_ENABLE_FLOW` 与 build dir；
- configure/build 后运行 isolated Qt checker 和 dependency graph/PE checker。

`build_gui_wt.bat` 与 `build_shell_noflow_wt.bat` 变为薄包装，消除重复路径和参数。

## 4. Build Directories

- ON：`XQ/build_shell_a_on`
- OFF：`XQ/build_shell_a_off`

使用新目录保留旧 `build_gui`、`build_shell_noflow` 与整改树证据。环境变量可覆盖，但 ON/OFF 不得指向同一目录。

## 5. Runtime Contract

`run_xq.bat` 调用共享环境，运行 ON tree 的 `xq_app.exe`：

- runtime PATH 只加入 locked ITK/GDCM/HDF5/tinyxml2/Qt/VTK bins；
- 不加入 Python、旧 Qt、MMG 或 host Anaconda；
- `QT_PLUGIN_PATH` 指向 isolated Qt；
- 透传 `%*`，支持 project path；
- executable 缺失时非零退出并提示先构建。

## 6. Verification Entry

新增 `verify_canonical_shell_a.bat`，严格串行：

1. ON configure/build/baseline check；
2. ON focused + full CTest；
3. OFF configure/build/baseline check；
4. OFF focused + full CTest；
5. dependency negative probes。

任何一步失败立即非零退出。

## 7. Source Regression Guard

扩展现有 `test_dependency_baseline` CMake script，检查：

- 三个公开入口调用共享 helper；
- helper 使用 isolated Qt 和 narrow roots；
- configure helper 包含 exact dirs、locked Python、`--fresh` 和 distinct ON/OFF trees；
- run wrapper 使用 canonical ON executable 并透传参数；
- 源脚本不存在旧 Qt literal、broad prefix 或 Anaconda。

## 8. Rollback

- 公开 wrapper 的改动可独立回退；新 helper 不改 CMake product semantics。
- 新 build dirs 可保留为证据；不删除旧树。
- 不修改 Externals 源码/安装、不清理用户未提交文件。

