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
2. **Qt 子组件同时需要显式 `Qt6_DIR` 与窄 `CMAKE_PREFIX_PATH`**:只传
   `Qt6_DIR` 可能使 Widgets 等组件不可见；但 prefix 只能列隔离 Qt 和每个精确
   依赖 root，禁止加入 `Externals/install/windows-x64` 总根。
3. **ctest 跑 Qt/VTK 测试要 `set QT_QPA_PLATFORM=offscreen`**,否则无显示环境下 GUI 测试失败。
4. **git-bash 里用 `cmd //c <bat>` 调用**(双斜杠避免路径转义)。
5. **Trellis 脚本不要裸跑 `python`**：本机 `python` 解析到
   `C:\Users\OCEAN\AppData\Local\Microsoft\WindowsApps\python.exe` 商店 stub
   (`0.0.0.0`)，可能无诊断直接失败。使用
   `C:\software\anaconda\python.exe -X utf8 .\.trellis\scripts\<script>.py`。

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

## Scenario: Preserve explicit Qt translation contexts

### 1. Scope / Trigger

- Trigger: adding or changing user-facing strings in free-function UI builders
  that call the local `xqTr` helper, or regenerating `xq_zh_CN.ts/.qm`.
- Purpose: runtime lookup uses the literal `XQStageWidgets` context. A catalog
  entry extracted under the enclosing C++ namespace is present but unreachable.

### 2. Signatures

```cpp
QString xqTr(const char* source)
{
    return QCoreApplication::translate("XQStageWidgets", source);
}
```

```bat
lrelease.exe resources\i18n\xq_zh_CN.ts ^
  -qm resources\i18n\xq_zh_CN.qm
```

### 3. Contracts

- Every `xqTr("...")` runtime key belongs to the exact TS context
  `XQStageWidgets`. Do not register `xqTr` as a `tr` alias with lupdate: calls in
  this file are free functions inside namespace `xq`, so alias extraction assigns
  the wrong `xq` context.
- Maintain these TS messages explicitly under `XQStageWidgets` unless the source
  is first changed to an extraction form that carries an explicit context. Never
  move existing entries merely to match lupdate's inferred namespace.
- After editing TS, regenerate the committed QM with `lrelease`. The catalog must
  have zero unfinished messages, stay UTF-8 without BOM with LF line endings,
  and contain every source key used by the touched UI.
- Runtime tests must query the same context and source text used by production;
  checking only that the QM loads is insufficient.

### 4. Validation & Error Matrix

- `XQStageWidgets` message moved to context `xq` -> runtime returns the English
  source string even though lrelease succeeds.
- TS changed without regenerating QM -> resource test/executable observes stale
  translations.
- missing source key or `type="unfinished"` -> translation gate fails; do not
  accept fallback English as green.
- duplicate source in unrelated contexts -> allowed only when production really
  uses both contexts; it does not repair a missing `XQStageWidgets` entry.

### 5. Good/Base/Bad Cases

- Good: `translator.translate("XQStageWidgets", "Build VesselProfile")`
  returns the committed Chinese text from the rebuilt QM.
- Base: a QObject member using ordinary `tr()` remains in its generated class
  context, such as `xq::XQMainWindow`.
- Bad: run lupdate with `-tr-function-alias tr+=xqTr`, accept the generated `xq`
  context, and report success because the TS/QM files are non-empty.

### 6. Tests Required

- Run `lrelease` and assert its summary reports zero unfinished messages.
- Compare touched source keys with TS entries and assert no key is missing from
  its production context.
- Run `test_i18n_resources`; include direct `XQStageWidgets` lookups for new
  actions and Flow-OFF status text, then run the full Release CTest suite.

### 7. Wrong vs Correct

#### Wrong

```bat
lupdate.exe src -tr-function-alias tr+=xqTr -ts xq_zh_CN.ts
rem xqTr calls are classified under namespace context "xq".
```

#### Correct

```xml
<context>
  <name>XQStageWidgets</name>
  <message>
    <source>Build VesselProfile</source>
    <translation>构建血管剖面（VesselProfile）</translation>
  </message>
</context>
```

Then rebuild `xq_zh_CN.qm` with `lrelease` and verify the exact runtime lookup.

## Scenario: MSVC showIncludes language must match Ninja

### 1. Scope / Trigger

- Trigger: configuring or building the Ninja/MSVC tree, changing a public C++
  header or class layout, or diagnosing a crash that appears only after an
  incremental build.
- Purpose: Ninja parses `/showIncludes` using the prefix detected by CMake. A
  locale mismatch silently records zero header dependencies and can link objects
  compiled against different class layouts.

### 2. Signatures

- Required environment before configure **and** build: `VSLANG=1033`.
- Canonical configure recovery: `cmake --fresh -S . -B build_gui ...`.
- Generated contract in `build_gui/CMakeFiles/rules.ninja`:
  `msvc_deps_prefix = Note: including file:`.
- Dependency audit:
  `ninja -C build_gui -t deps <object-path>`.

### 3. Contracts

- Set `VSLANG=1033` after `vcvars64.bat` and before CMake compiler detection,
  configure, and every MSVC build. The detected prefix and actual `cl
  /showIncludes` output must use the same language.
- Changing `VSLANG` in a build script does not repair an existing CMake cache.
  If `rules.ninja` contains a localized prefix, run one `cmake --fresh`
  configure with the canonical environment.
- After repairing a mismatched tree, run a clean-first Release build so every
  object repopulates `.ninja_deps`; do not trust a green test run that may still
  contain ABI-mixed objects from the broken dependency database.
- At least one transitive consumer and the defining object must report `VALID`
  with `#deps > 0`. For `XQProject` layout changes, audit
  `XQAppStartup.cpp.obj`, `XQProject.cpp.obj`, and affected controller/test
  objects and confirm `XQProject.h` appears in consumer dependencies.
- Keep the configured test-data root and existing dependency prefixes unchanged
  while refreshing the cache. A fresh configure is dependency repair, not a
  license to change project schema, fixtures, or external versions.

### 4. Validation & Error Matrix

- `rules.ninja` expects Chinese `注意: 包含文件:` while `cl` prints English
  `Note: including file:` -> objects show `#deps 0`; incremental results invalid.
- build script sets `VSLANG=1033` but old rules remain localized -> cache was not
  refreshed; run `cmake --fresh`.
- English rules but audited objects are missing/zero/invalid in `ninja -t deps`
  -> clean-first rebuild before testing.
- header layout changed and only its own `.cpp` rebuilt -> possible ABI mix;
  rebuild all transitive consumers and rerun startup plus full tests.
- correct prefix and nonzero valid deps -> ordinary incremental builds may be
  used again.

### 5. Good/Base/Bad Cases

- Good: fresh English configure, clean Release build, `XQAppStartup.cpp.obj`
  lists `XQProject.h`, focused tests and full CTest pass.
- Base: an already-correct tree keeps nonzero valid deps through an incremental
  source edit.
- Bad: accept `test_app_startup` crash as a product bug while Ninja shows zero
  deps, or accept a clean test pass without repairing future incremental builds.

### 6. Tests Required

- Inspect `msvc_deps_prefix` after configure.
- Run `ninja -t deps` for the changed definition and at least one transitive
  consumer; assert `VALID`, nonzero dependency count, and the public header.
- After class-layout changes, run focused lifecycle/controller tests,
  `test_app_startup`, and full Release `ctest --output-on-failure`.
- If the build directory was cleared and the executable is used interactively,
  rerun the documented `windeployqt` command before GUI handoff.

### 7. Wrong vs Correct

#### Wrong

```bat
call vcvars64.bat
cmake -S . -B build_gui -G Ninja
cmake --build build_gui
rem rules may be localized while cl output later changes language
```

#### Correct

```bat
call vcvars64.bat
set VSLANG=1033
cmake --fresh -S . -B build_gui -G Ninja -DCMAKE_BUILD_TYPE=Release ...
cmake --build build_gui --config Release --clean-first
ninja -C build_gui -t deps CMakeFiles/xq_app_shell.dir/src/app/XQAppStartup.cpp.obj
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

使用仓库版本化的 canonical 入口；git-bash 里用 `cmd //c`：

```bat
XQ\build_gui_wt.bat
XQ\build_shell_noflow_wt.bat
```

## 标准配方(全量 ctest)

```bat
XQ\verify_canonical_shell_a.bat
```

该脚本生成自动证据，不替代 `XQ\run_xq.bat <project>` 的实机 GUI 验收。

## Scenario: Separate build-tree CTest runs are sequential

### 1. Scope / Trigger

- Trigger: validating two configured XQ build trees, such as the default Flow-ON
  tree and an `XQ_ENABLE_FLOW=OFF` tree.
- Purpose: several IO/project tests use stable filenames under the shared Windows
  temporary directory. The same named test running from two build trees can
  delete or overwrite the other process's fixture.

### 2. Signatures

```bat
ctest --test-dir XQ\build_gui -C Release --output-on-failure
ctest --test-dir XQ\build_shell_noflow -C Release --output-on-failure
```

### 3. Contracts

- Run the two full suites one after the other. A completed ON suite is followed
  by the OFF suite (or vice versa); do not launch both commands concurrently.
- A concurrent failure involving temporary project save/reopen, BlobStore copy,
  or asset directories is not accepted as green and is not immediately a
  product regression. Stop both runners, confirm no test processes remain, then
  rerun each full suite sequentially.
- Do not hide the collision with retries or selective test removal. If parallel
  cross-build validation becomes a requirement, first make every temporary path
  include a per-process/build-tree identity and add a concurrency regression.

### 4. Validation & Error Matrix

- Two build-tree suites run concurrently -> possible `XQProjectWriter::save`
  failures, reopen failures, missing BlobStore bytes, or junction-copy failures.
- Same binaries rerun sequentially -> all affected tests must pass; otherwise
  investigate a real product/test defect.
- Stale `ctest.exe` or `test_*.exe` remains -> terminate/wait for the old run
  before trusting the sequential rerun.

### 5. Good/Base/Bad Cases

- Good: Flow ON full CTest completes, then Flow OFF full CTest completes.
- Base: run only one configured tree's full suite.
- Bad: use `Promise.all`, two terminals, or background processes to run both
  full suites against the shared `%TEMP%`, then report the collision as a code
  failure or ignore it as flaky.

### 6. Tests Required

- Before release, run full Release CTest once in every required build tree,
  sequentially, with `--output-on-failure`.
- Record the independent pass totals for each tree.
- When diagnosing a prior concurrent failure, rerun every failed test through
  its normal full suite rather than replacing it with a mocked or edited test.

### 7. Wrong vs Correct

#### Wrong

```powershell
$on = Start-Job { ctest --test-dir XQ/build_gui }
$off = Start-Job { ctest --test-dir XQ/build_shell_noflow }
Wait-Job $on, $off
```

#### Correct

```powershell
ctest --test-dir XQ/build_gui -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir XQ/build_shell_noflow -C Release --output-on-failure
```

## Scenario: Canonical Shell A Windows entry boundary

### 1. Scope / Trigger

- Trigger: 修改正式 ON/OFF build wrapper、GUI run wrapper、Windows package
  roots、依赖 checker 或 A15 复现脚本。
- Purpose: 正式入口必须消费同一份可审计配方；自动全绿只是机器证据，不能替代
  A1/A15 的源码与实机审查。

### 2. Signatures

```text
XQ/probes/vascular_foundation/canonical_shell_env.bat
XQ/probes/vascular_foundation/configure_canonical_shell.bat ON|OFF
XQ/build_gui_wt.bat
XQ/build_shell_noflow_wt.bat
XQ/run_xq.bat [project-path]
XQ/verify_canonical_shell_a.bat
```

### 3. Contracts

- shared env 只定义 `XQ_CANONICAL_*` 值；configure preflight 才允许检查
  configure-only Python、样例、工具、旧 Qt witness；runtime preflight 不得依赖
  这些对象。
- configure 清空调用者的 `CMAKE_PREFIX_PATH`，关闭 user/system package
  registry，再以命令行传入六个精确 roots、四个 Python 3.11 configure 输入和
  exact package dirs。
- ON/OFF 使用不同 tree；每次 `cmake --fresh` 后 `--clean-first`，且
  `VSLANG=1033`。验证脚本固定 ON focused/full 完成后才运行 OFF focused/full，
  `CTEST_PARALLEL_LEVEL=1`。
- `run_xq.bat` 只启动 canonical ON app，原样透传 `%*`，PATH 只含锁定 runtime
  bins + Windows system dirs，并清除 Python/Conda/configure-only 环境变量。
- 正式 wrapper 必须由 `.gitignore` 反向规则纳入版本控制；LF `.bat` 不得依赖
  `call :label`/`goto :label`，因为当前 `cmd.exe` 的 label scan 可能失败。
- log run-id 目录不可覆盖；最终文案必须保留 `pending manual acceptance`。

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| caller 带宽 `CMAKE_PREFIX_PATH`/package registry | 被清空/禁用；cache 仍只有六个锁定 roots |
| runtime 缺 Python、样例、旧 Qt build cache | GUI 入口不因此失败 |
| ON/OFF 指向同目录 | env 非零退出 |
| package/config/version/root 不一致 | configure/checker 非零退出，不 fallback |
| 二次复现 | clean-first 重编二进制，不复用旧 obj |
| evidence run-id 已存在 | 拒绝覆盖已有日志 |
| 自动 suite 全绿 | 只记 PASS-EVIDENCE；人工 GUI 验收仍 pending |

### 5. Good/Base/Bad Cases

- Good: 三个 public wrappers 只做薄调用；checker 同时审 cache、ITK 内嵌
  GDCM/HDF5 identity、build graph 与递归 PE。
- Base: 当前机器的完整 Externals 安装留在构建时路径，ON/OFF 顺序验证。
- Bad: 把 Python 文件存在性放进共享 env，使 GUI 启动依赖 Python；仅靠目录名
  声称 GDCM/HDF5 版本；或 `cmake --fresh` 后增量 build 旧 objects。

### 6. Tests Required

- 先运行 pure source guard；断言 wrapper versioning、exact flags、runtime 清理和
  验证顺序。
- 运行 `verify_canonical_shell_a.bat`；记录 ON/OFF cache identity、focused/full
  totals、Qt/build graph/PE closure 与负向矩阵日志。
- 最后由人工运行 `run_xq.bat <project>`，记录机器、样例/项目 ID、截图、真实
  project load 和已知限制；这一步未记录前不得关闭 A1/A15。

### 7. Wrong vs Correct

#### Wrong

```bat
set PREFIX=C:\deps\install\windows-x64
cmake --fresh -S . -B build -DCMAKE_PREFIX_PATH="%PREFIX%"
cmake --build build
rem run wrapper also checks Python and test-data paths
```

#### Correct

```bat
set "CMAKE_PREFIX_PATH="
XQ\probes\vascular_foundation\configure_canonical_shell.bat ON
XQ\probes\vascular_foundation\configure_canonical_shell.bat OFF
XQ\verify_canonical_shell_a.bat
rem Keep the real GUI/project review pending until a human records it.
```

## 依赖安装位置(Externals,已编译)

`Externals/install/windows-x64/` 下:`qt-6.7.0`、`vtk-9.3.0`、`tinyxml2-8.0.0`(及 ITK/GDCM 等)。
版本权威以 `Externals/externals.manifest` 为准。

> 验收纪律见 `acceptance.md`:Release + 全量 ctest 绿才算完成;副作用调用别进 assert。

## Scenario: PowerShell UTF-8 reads and interactive GUI launch

### 1. Scope / Trigger

- Trigger: 在 Windows PowerShell 中读取含中文的 Trellis/Markdown/JSON，或把正式 GUI
  交给用户实机运行。
- Purpose: 防止无 BOM UTF-8 被 PowerShell 5.1 按 ANSI/OEM 解码后产生乱码，也防止用户
  直接运行 build-tree EXE 而丢失 VTK/ITK/Qt DLL 搜索路径。

### 2. Signatures

```powershell
Get-Content -Raw -Encoding UTF8 <path>
$utf8 = [System.Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = $utf8
[Console]::OutputEncoding = $utf8
$OutputEncoding = $utf8
& 'C:\software\anaconda\python.exe' -X utf8 .\.trellis\scripts\get_context.py
```

```bat
XQ\run_xq.bat [project-path]
```

### 3. Contracts

- 读取任何已知 UTF-8 中文文本时必须显式传 `-Encoding UTF8`；不得依赖 Windows
  PowerShell 5.1 的默认编码。
- console/`$OutputEncoding` 只影响终端和 native pipe，不能修复已经被 `Get-Content`
  错误解码的字符串。出现乱码后必须重新读取，不能据乱码编辑文件。
- 本机裸 `python` 是 WindowsApps stub；Trellis Python 脚本必须调用已验证的 Anaconda
  解释器并带 `-X utf8`。这只控制 Python I/O，不替代 PowerShell 的 `-Encoding UTF8`。
- 手工编辑继续使用 `apply_patch`；不得用 `Set-Content` 重写 UTF-8 文件并引入 BOM、换行或
  全文件 churn。
- 用户运行正式 GUI 必须用 `XQ\run_xq.bat`。build-tree 的 `xq_app.exe` 不是可直接双击的
  已部署程序；wrapper 才负责锁定 Qt/VTK/ITK/GDCM/HDF5/tinyxml2 runtime PATH。
- `build_gui_wt.bat` 负责构建，`verify_canonical_shell_a.bat` 负责自动验证，二者都不是日常
  GUI 启动入口。

### 4. Validation & Error Matrix

| Condition | Required result |
| --- | --- |
| 中文标题显示为 mojibake | 用 `Get-Content -Encoding UTF8` 重读；丢弃错误解码结果 |
| 只设置 console encoding 后复用乱码变量 | 禁止；从文件重新解码 |
| `python .trellis\scripts\get_context.py` 无输出失败 | 检查 `Get-Command python`；改用锁定 Anaconda Python + `-X utf8` |
| 直接双击 build-tree `xq_app.exe` 缺 DLL | 该启动无效；改用 `run_xq.bat` |
| `run_xq.bat` preflight 报依赖缺失 | 明确失败并修复锁定 runtime，不从主机 PATH fallback |
| wrapper 启动且真实 DICOM 可打开 | 才进入实机功能验收 |

### 5. Good/Base/Bad Cases

- Good: 显式 UTF-8 读取 `prd.md`，用锁定 Python 获取 Trellis context，再用
  `run_xq.bat` 启动真实 GUI。
- Base: 只读 ASCII 文件时编码无可见差异，但仍可显式 UTF-8 保持命令一致。
- Bad: 把乱码 PRD 当作真实内容继续规划，或让用户双击
  `build_shell_a_on\xq_app.exe` 后把缺 DLL 归因于系统安装损坏。

### 6. Tests Required

- 读取含中文的当前任务 `prd.md`，断言标题可识别且无替换字符/乱码片段。
- 运行锁定 Anaconda Python 的 `get_context.py`，断言能输出 current task；不得接受
  WindowsApps stub 的空失败。
- 修改 Trellis 文档后运行 `git diff --check` 并审查 diff 只包含预期行。
- 实机验收从 `run_xq.bat` 启动；记录直接 EXE 不是受支持入口。

### 7. Wrong vs Correct

#### Wrong

```powershell
Get-Content -Raw .trellis\tasks\<task>\prd.md
python .\.trellis\scripts\get_context.py
& .\XQ\build_shell_a_on\xq_app.exe
```

#### Correct

```powershell
Get-Content -Raw -Encoding UTF8 .trellis\tasks\<task>\prd.md
& 'C:\software\anaconda\python.exe' -X utf8 .\.trellis\scripts\get_context.py
& .\XQ\run_xq.bat
```
