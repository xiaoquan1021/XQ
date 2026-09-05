# EXECUTE-B5 — S4 大规模内存 GUI 接通(预算 / LOD 默认开 / 状态栏真实内存)

> **执行者须知**:本文档每处 before 代码均逐字对照过 `feat/gui-v2` 真实源码(B2 完成后 XQMainWindow 行号漂移,**符号为准,改前 rg 定位**)。照做,不做本文之外的设计决策;before 对不上内容时停下报告。
> **依赖(固定,不可变通):B1~B4 全部完成后才能开工。** 开工前核查(任一不满足 → 停下报告,不许跳过、不许"部分执行"):
> ```bash
> cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
> rg -n 'maxRecordedFrames' src/services/flow/FlowSolver1D.h   # B1:必须有命中
> rg -n 'assetRootDir_' src/app/XQMainWindow.h                 # B2:必须有命中
> rg -n 'taskRunner_' src/app/XQMainWindow.h                   # B3:必须有命中
> rg -n 'xqPathRunButton' src/ui/panels/XQStageWidgets.cpp     # B4:必须有命中
> ```

---

## 0. 环境与固定纪律

- 工作树:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/gui-v2`(Git Bash:`/c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`)。
- 完整构建:`cmd //c build_gui_wt.bat`;全量测试:`cmd //c ctest_merge.bat`;单测:
  ```bash
  cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui && \
  "C:/software/Visual Studio/Visual Studio2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/ctest.exe" \
    -R '^test_app_startup$' --output-on-failure
  ```
- **绝不 `cmake --build --target <单目标>`**;副作用先存变量再 CHECK;绝不 `git add -A`。
- 全局约束:只做本批;不加未要求兜底/降级;不覆盖未提交改动;**services 零 Qt/VTK**(GeometryResourceManager 加访问器纯 std;test_arch_boundaries 守着);不破坏 Source 1.0 签名;不参考 XQ1;没验证过不写"完成/通过"。
- 本批完成后按 implement.md 要求**另跑 TETGEN+MMG ON 档**(主仓 build_audit_p01.bat 配方另起 build 目录)——只跑不改,红了停下报告。
- 基线:开工前全量 ctest 全绿。依赖已固定为 B1~B4 全部完成,基线应为 **66/66**(65 基线 − test_image_viewer + test_task_runner + test_path_stage);不是 66 或有红 → 停下报告。

本批 3 个 commit:C1 预算(manager 访问器 + QSettings + Preferences)→ C2 LOD 默认开 → C3 状态栏真实内存。

---

## 1. rg 定位

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rg -n 'setBudgetBytes|budgetBytes_' src/services/resource/GeometryResourceManager.{h,cpp}
rg -n 'attachGeometryResources' src/app tests
rg -n 'addSurface\(\*|addSurfaceProgressive|addVolumeMeshProgressive' src/app/XQMainWindow.cpp
rg -n 'kSingleViewKey|openSettings' src/app/XQPreferencesDialog.cpp
rg -n 'statusMemLabel_' src/app
rg -n 'Mem: -- MB' src/app/XQMainWindow.cpp
rg -n 'psapi|GetProcessMemoryInfo' src CMakeLists.txt probe
```

已核实的事实:
- `GeometryResourceManager::setBudgetBytes`(.h:131,.cpp:143-148);`budgetBytes_` 默认 `(std::numeric_limits<std::size_t>::max)()`(.cpp:137)= 无界;**无读取访问器**(residentBytes/blockCount/mapCount/hitCount/evictCount 有,budget 没有,.h:154-158)。
- `attachGeometryResources`(XQMainWindow.cpp:412-420):registry 非空且 rootDir 非空才 new manager。
- `onSceneSelectionChanged` 两处 eager `addSurface(*...)`:SurfaceModel 分支 `renderer.addSurface(*model.triangleGeometry())`(:1938)与 Mesh 分支 `renderer.addSurface(*mesh.surfaceTriangles())`(:1957);progressive 三处:`addSurfaceProgressive(lazy.source())`(:1946/:1976)、`addVolumeMeshProgressive(lazy.source())`(:1967)。
- `LodOptions{enabled,fixedLevel,budgetTriangles,interactive}`(XQSceneRenderer.h:39-49);`addSurface(surf, const LodOptions& = {})`(:103-104);`ChunkUploadSpec{maxCellsPerChunk, LodOptions lod}`(:56-59);progressive 签名带 `const ChunkUploadSpec& spec = {}`(:113-115/:123-125)。
- QSettings 统一 org "XQ" app "XQ"(XQPreferencesDialog.cpp:16-18;XQMainWindow save/restoreWindowState 同款)。
- Preferences 对话框模式:常量键名 + loadFromSettings/saveToSettings + 静态 getter(XQPreferencesDialog.cpp 全文已核实,控件 objectName xqPref* 风格)。
- 状态栏:`statusMemLabel_` 建于 buildStatusBar(:1020-1022),retranslateUi 写死 `tr("Mem: -- MB")`(:1398-1400)。
- psapi 链接先例:`target_link_libraries(scale_probe PRIVATE ... psapi)`(CMakeLists.txt:692);include 先例 `#define WIN32_LEAN_AND_MEAN` + `<windows.h>` + `<psapi.h>`(probe/main.cpp:24-26);`GetProcessMemoryInfo(GetCurrentProcess(), ...)` 用法(probe/main.cpp:45-54)。全仓 src/ 下无 NOMINMAX 定义(rg 已核实)——XQMainWindow.cpp 用了 `std::max/std::min`,**include windows.h 前必须 NOMINMAX**。
- QTimer 已被 XQMainWindow.cpp include 并使用(singleShot,:406/:1181)。
- `XQAppStartupState.assetRootDir` 传入 `attachGeometryResources`(XQAppStartup.cpp:67-70)。
- flow 页 SolverInput 组装在**窗口** flowInputProvider(XQMainWindow.cpp:642-651,`solverInput.numTimeSteps = 2000` 等),不在 XQStageWidgets.cpp(design 写的 :545-619 是页面 run 处,已纠正)——**maxRecordedFrames=2000 的设置点在窗口 flowInputProvider**,定位锚:`rg -n 'solverInput.numTimeSteps = 2000' src/app/XQMainWindow.cpp`。B1 的 `maxRecordedFrames` 字段存在性已由文档头部依赖核查保证。

---

## 2. Commit 1 — 几何驻留预算

### 2.1 GeometryResourceManager 只读访问器(纯追加,零 Qt)

`src/services/resource/GeometryResourceManager.h`,`void setBudgetBytes(std::size_t budget);` 之后追加:
```cpp
    // Current resident-bytes ceiling (read-only; for assertions / status UI).
    std::size_t budgetBytes() const;
```
`.cpp`,setBudgetBytes 实现之后追加:
```cpp
std::size_t GeometryResourceManager::budgetBytes() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return budgetBytes_;
}
```

### 2.2 Preferences 增设置项

`src/app/XQPreferencesDialog.h` 静态 getter 区追加:
```cpp
    static int  geometryBudgetMiB();      // key "memory/geometryBudgetMiB", default 2048 (256..65536)
```
`.cpp`:
- 常量区追加:
  ```cpp
  constexpr auto kGeometryBudgetKey = "memory/geometryBudgetMiB";
  constexpr int kGeometryBudgetDefault = 2048;
  constexpr int kGeometryBudgetMin = 256;
  constexpr int kGeometryBudgetMax = 65536;

  int clampBudget(int value) {
      if (value < kGeometryBudgetMin) return kGeometryBudgetMin;
      if (value > kGeometryBudgetMax) return kGeometryBudgetMax;
      return value;
  }
  ```
- 构造函数 statusGroup 之后新组(照 Viewing 组风格):
  ```cpp
      // "Memory" group: geometry residency budget for lazy-loaded assets.
      auto* memoryGroup = new QGroupBox("Memory", this);
      auto* memoryForm = new QFormLayout(memoryGroup);

      auto* geometryBudget = new QSpinBox(memoryGroup);
      geometryBudget->setObjectName("xqPrefGeometryBudget");
      geometryBudget->setRange(kGeometryBudgetMin, kGeometryBudgetMax);
      geometryBudget->setSingleStep(256);
      geometryBudget->setSuffix(" MiB");
      memoryForm->addRow("Geometry budget", geometryBudget);

      layout->addWidget(memoryGroup);
  ```
  (插在 `layout->addWidget(statusGroup);` 与按钮盒之间;RestoreDefaults 的 lambda 捕获列表加 `geometryBudget` 并加 `geometryBudget->setValue(kGeometryBudgetDefault);`。)
- loadFromSettings/saveToSettings 各加一段(照 xqPrefSliceStep 模式,clampBudget 包裹);静态 getter:
  ```cpp
  int XQPreferencesDialog::geometryBudgetMiB() {
      QSettings settings = openSettings();
      return clampBudget(
          settings.value(kGeometryBudgetKey, kGeometryBudgetDefault).toInt());
  }
  ```

### 2.3 attachGeometryResources 应用预算

`src/app/XQMainWindow.cpp`,before(逐字,:412-420):
```cpp
void XQMainWindow::attachGeometryResources(const AssetRegistry* registry,
                                           const std::string& assetRootDir)
{
    geometryRegistry_ = registry;
    geometryResources_.reset();
    if (registry != nullptr && !assetRootDir.empty()) {
        geometryResources_.reset(new GeometryResourceManager(registry, assetRootDir));
    }
}
```
after(**B2 已在此函数加过 `assetRootDir_ = assetRootDir;` 一行,保留它**;以下 diff 只加预算两行):
```cpp
void XQMainWindow::attachGeometryResources(const AssetRegistry* registry,
                                           const std::string& assetRootDir)
{
    geometryRegistry_ = registry;
    assetRootDir_ = assetRootDir;
    geometryResources_.reset();
    if (registry != nullptr && !assetRootDir.empty()) {
        geometryResources_.reset(new GeometryResourceManager(registry, assetRootDir));
        geometryResources_->setBudgetBytes(
            static_cast<std::size_t>(XQPreferencesDialog::geometryBudgetMiB()) << 20);
    }
}
```
(XQPreferencesDialog.h 已被本文件 include,:40。)

### 2.4 applyPreferences 热应用

`applyPreferences()`(:1700-1718)末尾追加:
```cpp
    if (geometryResources_ != nullptr) {
        geometryResources_->setBudgetBytes(
            static_cast<std::size_t>(XQPreferencesDialog::geometryBudgetMiB()) << 20);
    }
```

### 2.5 测试(先红)

`tests/app/test_app_startup.cpp` 新增用例(main() 挂上;include 补 `#include <app/XQPreferencesDialog.h>`、`#include <services/resource/GeometryResourceManager.h>`、`#include <QSettings>`):

断言需要经窗口拿 manager,窗口现状不暴露——**新增最小只读访问器(定形)**:XQMainWindow.h public 加:
```cpp
    // Test/UI-readonly view of the geometry residency manager (null until
    // attachGeometryResources with a non-empty asset root).
    const GeometryResourceManager* geometryResourceManager() const;
```
.cpp:
```cpp
const GeometryResourceManager* XQMainWindow::geometryResourceManager() const
{
    return geometryResources_.get();
}
```
用例:
```cpp
int test_geometry_budget_flows_from_settings()
{
    // Pin the setting to a known value, then attach and assert传导.
    QSettings settings(QStringLiteral("XQ"), QStringLiteral("XQ"));
    const QVariant saved = settings.value(QStringLiteral("memory/geometryBudgetMiB"));
    settings.setValue(QStringLiteral("memory/geometryBudgetMiB"), 512);

    const xq::NodeId surfaceId(701);
    const xq::NodeId meshId(702);
    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    add_lazy_geometry_fixture_nodes(&project, surfaceId, meshId);
    TempProjectFiles files("xq_budget_flow");
    CHECK(xq::XQProjectWriter::save(project, files.path.string())
          == xq::XQProjectWriter::Status::Ok);

    xq::XQAppStartupConfig config;
    config.projectPath = files.path.string();
    xq::XQAppStartupState state;
    CHECK(xq::initializeAppStartup(config, &state) == xq::XQAppStartupStatus::Ok);

    xq::XQMainWindow window;
    xq::attachMainWindowWorkflow(&window, &state);

    const xq::GeometryResourceManager* manager = window.geometryResourceManager();
    CHECK(manager != nullptr);
    CHECK(manager->budgetBytes() == (std::size_t(512) << 20));

    // Restore the user's persisted value (don't pollute the real settings).
    if (saved.isValid()) {
        settings.setValue(QStringLiteral("memory/geometryBudgetMiB"), saved);
    } else {
        settings.remove(QStringLiteral("memory/geometryBudgetMiB"));
    }
    return 0;
}
```
**先红做法**:访问器 + 用例先写,`attachGeometryResources` 的 setBudgetBytes 行**后写** → 构建 → 红在 `budgetBytes() == 512MiB`(默认 SIZE_MAX)→ 补 2.3 两行 → 绿。红证据留档。
库层预算驱逐已有 `test_geometry_resource_manager` 覆盖,**不重复写**。

### 2.6 假绿抽查

把 2.3 的 `<< 20` 篡改为 `<< 10` → 构建 → 本用例**必红**;还原,构建,绿。

### 2.7 Commit 1

```
feat: 几何驻留预算接通 — QSettings 默认 2048MiB + Preferences 设置项 + attach 时传导
```
文件:
- `src/services/resource/GeometryResourceManager.{h,cpp}`
- `src/app/XQPreferencesDialog.{h,cpp}`
- `src/app/XQMainWindow.{h,cpp}`
- `tests/app/test_app_startup.cpp`

---

## 3. Commit 2 — 选中渲染 LOD 默认开

### 3.1 默认 LodOptions 常量

`XQMainWindow.cpp` 匿名 namespace(kScreenMargin 之后)追加:
```cpp
// Default LOD for selection-driven surface rendering (R4): interactive
// vtkLODActor levels with a 2M-triangle budget for deterministic paths. The
// renderer's own default stays off -- library-level tests pass options
// explicitly and are unaffected.
const LodOptions kDefaultLodOptions = [] {
    LodOptions options;
    options.enabled = true;
    options.interactive = true;
    options.budgetTriangles = 2'000'000;
    return options;
}();
```
(LodOptions 可见性已核实:XQMainWindow.cpp include 了 `visualization/XQRenderWidget.h`,该头第 4 行 include XQSceneRenderer.h,LodOptions 直接可用,**无需新增 include**。)

### 3.2 五个调用点改传

`onSceneSelectionChanged`:
1. before `stats = renderer.addSurface(*model.triangleGeometry());`(SurfaceModel 分支)
   after `stats = renderer.addSurface(*model.triangleGeometry(), kDefaultLodOptions);`
2. before `stats = renderer.addSurfaceProgressive(lazy.source());`(SurfaceModel lazy 分支)
   after:
   ```cpp
                ChunkUploadSpec spec;
                spec.lod = kDefaultLodOptions;
                stats = renderer.addSurfaceProgressive(lazy.source(), spec);
   ```
3. before `stats = renderer.addSurface(*mesh.surfaceTriangles());`(Mesh 分支)
   after `stats = renderer.addSurface(*mesh.surfaceTriangles(), kDefaultLodOptions);`
4. Mesh lazy 的 `addVolumeMeshProgressive(lazy.source())`:**不动**。依据(已核实):`spec.lod` 全仓唯一消费点在 surface chunk 装配(XQSceneRenderer.cpp:706,`spec.lod.interactive`),volume progressive 路径不消费 lod;LOD 是面片 decimation,对 tet 线框无意义。
5. Mesh 分支 fallback 的 `addSurfaceProgressive(lazy.source())`(:1976)同 2 处理(spec.lod = kDefaultLodOptions)。

`addVolumeMesh(*mesh.volumeTets())`(:1955)无 LOD 参数,不动。

### 3.3 测试与验证

- 库层测试零影响(渲染器默认 `LodOptions{}` 未动;已核实测试全部显式传参)。
- **本 commit 不加新窗口测试(定形)**:窗口层断言 uploadedPointCount≤pointCount 需要真实大表面 + offscreen 渲染,而 interactive 模式 vtkLODActor 无法 headless 度量(XQSceneRenderer.h:44-48 注释已言明)。验收落在:①全量 ctest 全绿;②B6 真机目视(交互旋转大模型不卡);③代码评审 diff 五点核对。AC4 的 offscreen "uploadedPointCount≤源点数" 断言由既有 `test_surface_lod`(库层,显式传参)覆盖。
- 完整构建 + 全量 ctest 全绿。

### 3.4 假绿抽查

本 commit 无新测试,抽查落在**库层反向验证**:把 `kDefaultLodOptions` 的 `options.enabled = true;` 临时改 `false` → 构建 → 全量 ctest **仍全绿**(证明库层测试确实不受 GUI 默认影响——这是"隔离性"抽查,不是红绿抽查);再 `rg -n 'kDefaultLodOptions' src/app/XQMainWindow.cpp` 确认 4 处引用齐(1/2/3/5)。还原。

### 3.5 Commit 2

```
feat: 选中渲染 LOD 默认开 — interactive + 2M 三角预算,progressive 同配
```
文件:
- `src/app/XQMainWindow.cpp`

---

## 4. Commit 3 — 状态栏内存真实化

### 4.1 psapi include 与链接

`XQMainWindow.cpp` **文件尾部 include 区之后**(即所有 Qt/std include 之后、`namespace xq {` 之前)追加:
```cpp
// Windows process-memory query for the status bar (R4). Included last, guarded
// against min/max macro pollution (this file uses std::min/std::max).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
```
CMakeLists.txt,xq_app_shell 链接段 before(逐字,:195-203):
```cmake
target_link_libraries(xq_app_shell
    PUBLIC xq_core
    PUBLIC xq_ui
    PUBLIC xq_render_widget
    PUBLIC xq_controllers
    PUBLIC Qt6::Widgets
    PRIVATE xq_adapter_vtk
    PRIVATE xq_io
)
```
after(加一行):
```cmake
target_link_libraries(xq_app_shell
    PUBLIC xq_core
    PUBLIC xq_ui
    PUBLIC xq_render_widget
    PUBLIC xq_controllers
    PUBLIC Qt6::Widgets
    PRIVATE xq_adapter_vtk
    PRIVATE xq_io
    PRIVATE psapi
)
```

### 4.2 updateMemoryStatus

头文件 private 方法区加 `void updateMemoryStatus();`,成员区加 `QTimer* memoryTimer_ = nullptr;`,前向声明区(`class QToolBar;` 附近)补一行 `class QTimer;`(已核实 XQMainWindow.h 现状没有该前向声明)。

.cpp 实现:
```cpp
void XQMainWindow::updateMemoryStatus()
{
    if (statusMemLabel_ == nullptr) {
        return;
    }
    PROCESS_MEMORY_COUNTERS pmc = {};
    std::size_t workingSetMiB = 0;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        workingSetMiB = static_cast<std::size_t>(pmc.WorkingSetSize >> 20);
    }
    if (geometryResources_ != nullptr) {
        const std::size_t residentMiB = geometryResources_->residentBytes() >> 20;
        statusMemLabel_->setText(tr("Mem %1 MB | Geometry %2 MB")
                                     .arg(workingSetMiB)
                                     .arg(residentMiB));
    } else {
        statusMemLabel_->setText(tr("Mem %1 MB").arg(workingSetMiB));
    }
}
```
(文案用英文 source + ts 翻中文 `内存 %1 MB · 几何驻留 %2 MB` / `内存 %1 MB`,与全文件 tr() 习惯一致。)

buildStatusBar 末尾追加定时器:
```cpp
    memoryTimer_ = new QTimer(this);
    memoryTimer_->setInterval(2000);
    QObject::connect(memoryTimer_, &QTimer::timeout,
                     this, &XQMainWindow::updateMemoryStatus);
    memoryTimer_->start();
    updateMemoryStatus();
```

retranslateUi,before(逐字,:1398-1400):
```cpp
    if (statusMemLabel_ != nullptr) {
        statusMemLabel_->setText(tr("Mem: -- MB"));
    }
```
after(语言切换后立即用新语言重算,不再写死):
```cpp
    if (statusMemLabel_ != nullptr) {
        updateMemoryStatus();
    }
```

**taskFinished 后即刷**:构造函数里 taskRunner_ 的 taskFinished connect 的 lambda(B3 建立)末尾补一行 `updateMemoryStatus();`。

ts 更新:
```bash
/c/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lupdate.exe \
  src -ts resources/i18n/xq_zh_CN.ts -no-obsolete
/c/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lrelease.exe \
  resources/i18n/xq_zh_CN.ts -qm resources/i18n/xq_zh_CN.qm
```
新词条补翻:`Mem %1 MB | Geometry %2 MB` → `内存 %1 MB · 几何驻留 %2 MB`;`Mem %1 MB` → `内存 %1 MB`;旧词条 `Mem: -- MB` 被 -no-obsolete 删除(预期)。

### 4.3 测试(先红)

`tests/app/test_main_window.cpp` 追加(放 workflowWindow 断言区;include 补 `#include <QRegularExpression>`):
```cpp
    // Status-bar memory is real (R4): matches "<zh>内存</zh> N MB" with N > 0,
    // never the old hard-coded "Mem: -- MB" placeholder.
    QLabel* memLabel = workflowWindow.findChild<QLabel*>("xqStatusMem");
    if (memLabel == nullptr) {
        return fail("status bar exposes the memory label");
    }
    const QString memText = memLabel->text();
    const QRegularExpression memPattern(
        QStringLiteral("\\d+ MB"));
    if (!memPattern.match(memText).hasMatch() || memText.contains(QStringLiteral("--"))) {
        return fail("status bar shows a real working-set figure");
    }
```
(中文环境下文案是 `内存 N MB`,正则只锚数字+MB,两种语言都过;`--` 排除旧占位。)
**先红做法**:此断言块先写(4.2 实现前)→ 构建 → 红在 "status bar shows a real working-set figure"(现状 `Mem: -- MB`)→ 做 4.1/4.2 → 绿。红证据留档。

### 4.4 假绿抽查

临时让 updateMemoryStatus 函数体第一行直接 `statusMemLabel_->setText(tr("Mem: -- MB")); return;`(还原旧占位行为)→ 完整构建 → `test_main_window` **必红**(memText contains "--");还原,构建,确认绿。(不要用"把 WorkingSetSize 改 0"当抽查——0 也匹配 `\d+ MB`,测试仍绿,不构成抽查。)

### 4.5 Commit 3

```
feat: 状态栏内存真实化 — WorkingSet + 几何驻留字节,2s 定时刷新
```
文件:
- `src/app/XQMainWindow.{h,cpp}`
- `CMakeLists.txt`
- `tests/app/test_main_window.cpp`
- `resources/i18n/xq_zh_CN.ts`
- `resources/i18n/xq_zh_CN.qm`

---

## 5. 附:GUI 侧 flow 抽帧接线(一行,归并进 Commit 3)

前提已由文档头部的依赖核查保证(B1 的 `maxRecordedFrames` 字段必须在,否则整批开工前就已停下)。

`XQMainWindow.cpp` flowInputProvider,before(逐字,:649-651):
```cpp
        solverInput.numCycles = 2;
        solverInput.numTimeSteps = 2000;
        solverInput.dt = solverInput.period / 2000.0;
```
after:
```cpp
        solverInput.numCycles = 2;
        solverInput.numTimeSteps = 2000;
        solverInput.dt = solverInput.period / 2000.0;
        solverInput.maxRecordedFrames = 2000;
```
(此行归并进 Commit 3;design S5c 定论:GUI 显式开 2000,库默认 0 不动。既有 20000 步重试分支(XQStageWidgets.cpp flow 页 run 里 `numTimeSteps = 20000`)因 intent 按值捕获自带 maxRecordedFrames=2000,20000 步时录 2000 帧封顶——这正是 R5 要的效果,无需另改。)

---

## 6. 批次收尾

1. 3 个 commit 后删 `build_gui` 全新重建 + 全量 ctest,**66/66 绿**(本批不增删测试)。
2. **TETGEN+MMG ON 档**:照主仓 `build_audit_p01.bat` 配方另起 build 目录完整构建 + 全量 ctest(memory 坑:可选 dll 的 GUI 测试 `0xc0000135` 优先怀疑 ctest ENVIRONMENT PATH,CMake 已注入 mmg bin,:1030-1036)。红了停下报告,不许自行修。
3. `git log --oneline -3` 对照;`git status` 干净。
4. 汇报:commit hash、2.5/4.3 红输出摘录、2.6/4.4 假绿抽查摘录、ON 档 ctest 计数。**没验证过不写"完成/通过"。**
