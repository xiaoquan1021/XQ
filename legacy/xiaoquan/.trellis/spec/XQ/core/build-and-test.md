# 构建与测试环境(已实证)

> 这台机器上 XQ 工程的构建/测试配方,已由 baseline 24 测试实测 24/24 PASS 验证。
> 后续所有里程碑(M0~M7)的构建与验收都照此执行。**别重新摸索环境。**

---

## 工具链(实测可用)

- **CMake**:`C:/software/Visual Studio/Visual Studio2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe`(3.30.5)
- **编译器**:MSVC 14.43(VS2022 Community)`cl.exe`
- **Generator**:Ninja(VS 自带:`.../CMake/Ninja/ninja.exe`)
- **ctest**:同 CMake bin 目录下 `ctest.exe`
- **Python**:`C:/software/anaconda/python.exe`

## 关键坑(排障后才弄清)

1. **必须先 `call vcvars64.bat`**:MSVC + Ninja 依赖 VS 开发者环境;裸跑 cmake 找不到 `cl`/`ninja`。
   `vcvars64.bat`:`C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat`
2. **Qt 子组件靠 `CMAKE_PREFIX_PATH`,不是单个 `Qt6_DIR`**:只传 `Qt6_DIR` 会报
   `Failed to find required Qt component "Widgets"`(Qt6WidgetsConfig.cmake 找得到却 NOT FOUND)。
   正确做法:`CMAKE_PREFIX_PATH` 指向 `Externals/install/windows-x64` 及各依赖根。
3. **ctest 跑 Qt/VTK 测试要 `set QT_QPA_PLATFORM=offscreen`**,否则无显示环境下 GUI 测试失败。
4. **git-bash 里用 `cmd //c <bat>` 调用**(双斜杠避免路径转义)。cmd 的初始工作目录可能不是 bash 的 `pwd`,自包含 bat(内部 `cd /d`)要传**绝对路径**给 cmd:`cmd //c "C:/.../build_gui_wt.bat"`,相对名会「找不到」。
5. **i18n 加新可译串:手工编辑 `.ts`,不要跑 `lupdate`**。GUI 面板串多数走自定义 `xqTr(const char*)` = `QCoreApplication::translate("XQStageWidgets", ...)`,`lupdate` 只静态扫标准 `tr()`,扫不到这些串;`lupdate ... -no-obsolete` 会把整个 `XQStageWidgets` context 判为 obsolete **删掉在用串**(实测删 77~90 条),lrelease 后 .qm 丢串、i18n 断言转红。正确做法:在 `resources/i18n/xq_zh_CN.ts` 对应 context 手工加 `<message><source/><translation/></message>` 块(沿用无 `<location>` 的风格),再 `lrelease xq_zh_CN.ts -qm xq_zh_CN.qm`(应报 `N finished, 0 unfinished`),ts + qm 一并提交。context 归属:`tr()` → 所在类(如 `xq::XQMainWindow`),`xqTr()` → 固定 `XQStageWidgets`。

## Scenario: CTest 里的 Qt GUI 测试运行环境

### 1. Scope / Trigger

- Trigger: 新增或修改任何链接 Qt (`Qt6::Core` / `Qt6::Gui` / `Qt6::Widgets`) 的 `add_test` 目标。
- 原因: Windows 上 `ctest` 给每个 test 单独启动进程,不会自动继承构建时找到的 Qt DLL / platform plugin 路径。

### 2. Signatures

- CMake test property:
  ```cmake
  set_tests_properties(<qt_test> PROPERTIES
      ENVIRONMENT "PATH=${_xq_qt_bin}\\;${_xq_test_path};QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_xq_qt_plugin_path}"
  )
  ```

### 3. Contracts

- `PATH` 必须包含 Qt `bin` 目录,否则测试进程可能 `0xc0000135`。
- GUI/字体/窗口类测试必须设置 `QT_QPA_PLATFORM=offscreen`。
- 如果 Qt `plugins` 目录存在,同时设置 `QT_PLUGIN_PATH`,让 offscreen/windows platform plugin 可定位。
- 链接 app shell 的测试还要把其间接 DLL 放进 `PATH`(例如 `tinyxml2`、VTK,以及可选 MMG)。

### 4. Validation & Error Matrix

- 缺 Qt DLL -> `Exit code 0xc0000135`。
- 缺 platform plugin / 没设 offscreen -> headless 环境下 Qt 初始化失败或 GUI 测试挂起。
- 缺 app shell 间接 DLL -> 测试启动前即失败,不会进入断言。

### 5. Good/Base/Bad Cases

- Good: `test_main_window` 同时配置 Qt、VTK、tinyxml2、可选 MMG 和 `QT_QPA_PLATFORM=offscreen`。
- Base: 纯 `Qt6::Core` model 测试至少配置 Qt `PATH`。
- Bad: 只在 shell 里 `set PATH=...` 后跑 `ctest`,但没有给新增 test 设置 `set_tests_properties`;CTest 固定环境时会丢 DLL。

### 6. Tests Required

- 新增 Qt test 后必须跑:
  ```bat
  ctest --output-on-failure -R <qt_test>
  ctest --output-on-failure
  ```
- 断言点:目标 test 单跑通过,全量 ctest 不出现 `0xc0000135`。

### 7. Wrong vs Correct

#### Wrong

```cmake
add_test(NAME test_ui_font_glyphs COMMAND test_ui_font_glyphs)
```

#### Correct

```cmake
add_test(NAME test_ui_font_glyphs COMMAND test_ui_font_glyphs)
set_tests_properties(test_ui_font_glyphs PROPERTIES
    ENVIRONMENT "PATH=${_xq_qt_bin}\\;${_xq_qt_test_path};QT_QPA_PLATFORM=offscreen"
)
```

## Scenario: Portable real sample data root

### 1. Scope / Trigger
- Trigger: adding or modifying any test target that reads the real
  `0007_H_AO_H` sample dataset.
- Purpose: real-data tests must work from a different checkout directory,
  drive, or developer account without editing source-controlled CMake files.

### 2. Signatures
- CMake cache variable:
  ```cmake
  set(XQ_TEST_DATA_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../0007_H_AO_H"
      CACHE PATH "Root directory for XQ real sample test data")
  ```
- Derived internal variables:
  ```cmake
  _xq_ctgr_dir
  _xq_flow_dir
  _xq_vti_path
  _xq_models_dir
  _xq_meshes_dir
  _xq_pth_dir
  _xq_svproject_dir
  ```

### 3. Contracts
- All compile definitions that point at real sample data must derive from
  `XQ_TEST_DATA_ROOT`.
- `XQ_TEST_DATA_ROOT` is configurable at configure time with
  `-DXQ_TEST_DATA_ROOT=<path>`.
- The default remains repository-local:
  `${CMAKE_CURRENT_SOURCE_DIR}/../0007_H_AO_H`.
- Do not add fallback directories. A wrong configured root must make the
  affected real-data test fail visibly.

### 4. Validation & Error Matrix
- Developer-machine absolute sample path in `XQ/CMakeLists.txt` ->
  `test_portable_test_data_root` fails.
- Missing `XQ_TEST_DATA_ROOT` cache variable -> `test_portable_test_data_root`
  fails.
- Incorrect configured root -> real-data integration tests fail while trying to
  read their input files.

### 5. Good/Base/Bad Cases
- Good: `XQ_CTGR_DIR="${_xq_ctgr_dir}"` after `_xq_ctgr_dir` is derived from
  `XQ_TEST_DATA_ROOT`.
- Base: no override; the checked-out sibling `../0007_H_AO_H` is used.
- Bad: `XQ_CTGR_DIR="C:/Users/<developer>/.../0007_H_AO_H/Segmentations"` in a
  source-controlled `target_compile_definitions` block.

### 6. Tests Required
- `test_portable_test_data_root` must fail if the local absolute sample-data
  path is reintroduced into `XQ/CMakeLists.txt`.
- After changing real-data CMake definitions, run:
  ```bat
  ctest --output-on-failure -R "^(test_portable_test_data_root|test_modeling_integration|test_meshing_integration|test_flow_integration|test_ai_integration|test_workflow_integration|test_segmentation_integration|test_vtk_image_adapter|test_mdl_reader|test_msh_reader|test_pth_reader|test_ctgr_reader|test_svproject_reader)$"
  ctest --output-on-failure
  ```
- Also search `XQ/CMakeLists.txt`, `XQ/tests`, and `XQ/src` for the removed
  developer-machine sample path.

### 7. Wrong vs Correct
#### Wrong
```cmake
target_compile_definitions(test_ctgr_reader PRIVATE
    XQ_CTGR_DIR="C:/Users/<developer>/workspace/0007_H_AO_H/Segmentations"
)
```

#### Correct
```cmake
target_compile_definitions(test_ctgr_reader PRIVATE
    XQ_CTGR_DIR="${_xq_ctgr_dir}"
)
```

## 标准配方(配置 + 构建)

把以下写成 `.bat`,git-bash 里 `cmd //c build.bat`:

```bat
@echo off
call "C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "C:\Users\OCEAN\Desktop\XIAOQUAN\XQ"
set CM="C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set PREFIX=C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64;C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/tinyxml2-8.0.0;C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0;C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/vtk-9.3.0
%CM% -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%PREFIX%"
%CM% --build build --config Release
```

## 标准配方(全量 ctest)

```bat
@echo off
call "C:\software\Visual Studio\Visual Studio2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "C:\Users\OCEAN\Desktop\XIAOQUAN\XQ\build"
set QT_QPA_PLATFORM=offscreen
"C:\software\Visual Studio\Visual Studio2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" -C Release --output-on-failure
```

## 依赖安装位置(Externals,已编译)

`Externals/install/windows-x64/` 下:`qt-6.7.0`、`vtk-9.3.0`、`tinyxml2-8.0.0`(及 ITK/GDCM 等)。
版本权威以 `Externals/externals.manifest` 为准。

> 验收纪律见 `acceptance.md`:Release + 全量 ctest 绿才算完成;副作用调用别进 assert。
