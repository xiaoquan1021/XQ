# Implement — 渲染层做厚并接入主窗口

> 执行计划。新对话接手时:先读 `prd.md` + `design.md` + 根目录 `PROGRESS.md` + `CLAUDE.md`,
> 再按下面步骤做。每步做完跑测试,绿了再进下一步。
> **构建/测试配方见 PROGRESS.md §构建/测试配方**(vcvars64 + CMAKE_PREFIX_PATH 四根 + offscreen ctest)。

## Step 0 — 接手核对(不写代码)

- [ ] `task.py current` 确认活动任务是 `render-visualization`(若不是:`task.py start render-visualization`)。
- [ ] `git status` 干净;读 `git log -5` 确认在 master、主线已归档(`1b31fc9`)。
- [ ] 读 `src/visualization/XQImageViewer.{h,cpp}` 全文(现有渲染范式 / pimpl 用法)。
- [ ] 读 `src/app/XQMainWindow.{h,cpp}` 全文(中央区 / showImage / attachWorkflow / 场景树)。
- [ ] 读以下 payload 头,确认渲染取数接口:`XQPathPayload.h` / `XQSurfaceModelPayload.h` /
      `XQMeshPayload.h` / `XQFlowResultPayload.h` / `XQSegmentationMaskPayload.h`,
      及句柄 `XQTriangleSurfaceGeometryHandle.h` / `XQTetVolumeMeshHandle.h`(后两个已确认接口,见 prd §5)。

## Step 1 — 探测 VTK GUISupportQt 是否可用(决定交互 widget 走不走)

- [ ] 临时在 CMakeLists 的 `find_package(VTK ...)` 加 `GUISupportQt`,configure 一次:
      - 可用 → 走交互 widget 路径(Step 5a)。
      - 不可用 → 走降级路径(Step 5b),交互 widget 记为后续任务,**不阻塞本任务完成**。
- [ ] 把结论写进本文件 Step 5 旁注。

## Step 2 — `VtkConverters`(内部,仅 .cpp)

- [ ] 在 `XQSceneRenderer.cpp` 的匿名命名空间里(或单独 `VtkConverters.cpp` 不导出头)实现:
      - `XQImageVolume` → `vtkImageData`(scalar 拷贝;spacing/origin/direction/scalarType 对齐 geometry)。
      - `XQTriangleSurfaceGeometryHandle` → `vtkPolyData`(points + triangle cells + faceId 作 cell scalar)。
      - `XQTetVolumeMeshHandle` → `vtkUnstructuredGrid`(points + VTK_TETRA cells)。
      - path 点序列 → `vtkPolyData`(polyline)。
- [ ] 单元自测点:转换后 `GetNumberOfPoints()` / `GetNumberOfCells()` 与 XQ 源一致。

## Step 3 — `XQSceneRenderer`(库核心,零 VTK 公开头)

- [ ] 写 `XQSceneRenderer.h`:按 design §1.1 草案,公开 API 零 vtk 类型(pimpl)。
- [ ] 写 `XQSceneRenderer.cpp`:`Impl` 持 `vtkRenderer` + actor 列表;实现 `clear` / `addImageSlice` /
      `addPath` / `addSurface` / `addVolumeMesh` / `addFlowResult` / `renderOffscreenToRgba`。
- [ ] **表面定向**(铁律3):`addSurface` 用 `vtkPolyDataNormals`(AutoOrientNormals+Consistency)
      或有符号体积翻正;代码注释标明选了哪种、为什么。
- [ ] `renderOffscreenToRgba`:复用 `XQImageViewer` 的 offscreen render window 套路
      (offscreen + `vtkWindowToImageFilter` → RGBA),保证无头可跑。

## Step 4 — 渲染测试(对应 AC4/AC5)

- [ ] 新建 `tests/visualization/test_scene_renderer.cpp`(CHECK 宏式,副作用先取变量不进 assert)。
- [ ] 用例:小表面(一个四面体的 4 三角面)/ 小体网格 / 影像切片 / path,各 `add*` + offscreen 渲染,
      断言 `ok`、actorCount、pointCount 与输入一致、RGBA 非全黑。
- [ ] winding 不一致表面用例:渲染不崩 + 装配成功(走到定向逻辑)。
- [ ] CMake 加 `test_scene_renderer`(link `xq_visualization`)+ `add_test`。
- [ ] **构建 + 跑这个测试**(单测:`ctest -R test_scene_renderer`)绿。

## Step 5 — 接入 `XQMainWindow`

### 5a 交互 widget 路径(GUISupportQt 可用)
- [ ] 写 `XQRenderWidget.{h,cpp}`:封 `QVTKOpenGLNativeWidget`,持 `XQSceneRenderer`,
      把 renderer 挂到 widget 的 render window(friend 或不透明句柄,见 design §3)。
- [ ] `XQMainWindow`:加 `renderWidget_` 进中央区;场景树 `currentChanged` → 取选中节点 payload 类型 →
      `clear()` + 对应 `add*` + `render()`。**只做分发,不写渲染逻辑**。
- [ ] 保留 `setScene`/`showImage` 旧路径不破坏(AC7)。

### 5b 降级路径(GUISupportQt 不可用)
- [ ] 不建 `XQRenderWidget`;选中节点 → `XQSceneRenderer::renderOffscreenToRgba` → 贴现有 `imageLabel_`。
      支持切节点/切层,无 3D 交互;交互 widget 记后续任务。

- [ ] 更新 `test_main_window`(若接口有变)/ 确认其仍绿。

## Step 6 — 反向依赖审计(AC2,铁律1)

- [ ] `rg "#include.*(vtk|QVTK)" src/core src/services src/ui/controllers` → **必须空**。
- [ ] `rg "#include.*vtk" src/visualization/*.h` → **必须空**(公开头不泄漏 VTK;pimpl 生效)。

## Step 7 — 全量验收(主会话自己做,不信 worker)

- [ ] **全新 build 目录**:删 `build_verify` → `build_verify.bat`(配方见 PROGRESS.md),零错误。
- [ ] **全量 ctest**:`ctest_verify.bat`(offscreen),原 44 + 新增渲染测试 **全绿**。
- [ ] **假绿抽查**:篡改 `XQSceneRenderer::addSurface` 不 AddActor(或不做定向)→ `test_scene_renderer`
      Release **FAIL** → 恢复 → **PASS**。证明测试有效。
- [ ] AC1~AC7 逐条对勾。

## Step 8 — 收尾

- [ ] 更新 `PROGRESS.md`:渲染能力做厚 + 接入主窗口的状态;若交互 widget 降级,把"交互 widget"记进技术债。
- [ ] `task.py archive --no-commit`(自控 commit)。
- [ ] `git add -A` + 中文 commit(类型 `feat`):`feat: 渲染层做厚 — 场景渲染器 + 各产物 actor + 接入主窗口`。
- [ ] 若有新技术债(体绘制 / MPR / 交互 widget 降级 / 流场动画),回填父任务 prd 滚动清单或本任务 notes。

## 关键坑提醒(踩过的)

- **副作用不进 assert**:Release /DNDEBUG 删 assert → 假绿 segfault(memory `feedback-no-sideeffect-in-assert`)。
- **git-bash 调 .bat 用 `cmd //c "绝对路径"`**:相对路径会乱码失败。
- **offscreen**:ctest 跑 Qt/VTK 测试前 `set QT_QPA_PLATFORM=offscreen`。
- **表面 winding 别假设同向**:闭合 ≠ 法线一致,必自定向(memory `xq-surface-winding-not-consistent`)。
- **Python**:`C:/software/anaconda/python.exe`(本机 `python` 是 WindowsApps 假占位符)。
