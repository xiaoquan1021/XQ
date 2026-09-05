# EXECUTE P3-1:断面重采样视图 + 沿路径滑条

> 语境锚点:XQ 医学影像软件血管几何重建。本批做「沿血管中心线的断面重采样显示」——用 vtkImageReslice 沿路径法向切出 2D 断面图并挂进 GUI,加一条沿路径的定位滑条。纯影像可视化工程,对标 SimVascular。**本批只做断面显示 + 滑条,不做任何轮廓生成**(轮廓是 P3-2)。

## 活动任务

- 任务:`07-06-along-path-contouring`(in_progress)。
- 本批 = P3-1(六批第一批,技术核心)。
- **两个不同的 git 工作树,别混**:
  - **代码工作树(唯一改代码处)= `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`**,分支 `feat/render-arch`,HEAD=`85c357c`。本简报下文所有 `CODE_ROOT/...` 路径都指这里。所有 src/tests/CMakeLists 编辑、build、ctest 都在这个工作树,**用绝对路径**。
  - 任务文档/spec/agent 定义在主仓 `C:\Users\OCEAN\Desktop\XIAOQUAN`(= 你的 cwd,读 prd/design/implement/spec 用相对路径)。
- **凡本简报写 `CODE_ROOT`,替换为 `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`**。绝不在主仓 XIAOQUAN 下建/改任何 src 代码文件。

## 目标(本批范围,严格不越界)

vtkImageReslice 按 PathSamplePoint 坐标系出 2D 断面 → 专用断面工作台显示 → 沿路径滑条驱动断面切换。**做到:选一条路径,拖滑条,断面视图实时显示垂直于路径法向的断面图,不卡顿。**

**本批不做**:任何 2D 轮廓生成/手绘/自动方法/绑路径对话框/放样(全是后续批)。只到「断面能显示 + 滑条能滑」为止。

## 现状(已由主审亲读,worker 照此)

- 体数据:`XQRenderScene::Impl::image_`(`vtkSmartPointer<vtkImageData>`,src/visualization/XQRenderScene.cpp:2289),`setVolume`→`build_image` 构建。**XQRenderScene.h 目前不暴露内部 vtkImageData**——本批需新增 opaque handle。
- 路径采样:`core/XQPath.h` `PathSamplePoint{ position, tangent, normal, binormal, arcLength }`;`samplePoints()`、`frameAtArcLength`、`framesForAllSamples`。
- 断面坐标系:`core/XQContourGroup.h` `ContourFrame{ origin, normal, xAxis, yAxis }` + `unprojectFromFrame(frame,u,v)`(P3-1 不用,但 reslice frame 必须与它对齐——见验收命门)。
- 切片视图参照:`visualization/XQSliceViewWidget.{h,cpp}`(QVTKOpenGLNativeWidget + 挂 XQRenderScene 的 opaque renderer handle + VTK-free 头 + 浮层)。新断面视图照此结构。
- 四视图容器:`visualization/XQMprWidget.{h,cpp}`(QGridLayout + frame 显隐,有 LayoutMode Quad/Single)。
- 阶段接线:`app/XQMainWindow.{h,cpp}`;轮廓提取页 UI 在 `ui/panels/XQStageWidgets.{h,cpp}`。

## 实现(接口已定死,worker 不自由发挥)

### 1. XQRenderScene 暴露常驻体数据(opaque handle,头保持 VTK-free)
在 `CODE_ROOT/src/visualization/XQRenderScene.h` 加:
```cpp
// Opaque vtkImageData* of the resident volume (null when no volume). Lets the
// cross-section resampler reslice the same resident image without a second copy.
void* vtkImageDataHandle() const;
```
.cpp 返回 `impl_->image_.GetPointer()`(无 volume 时 null)。**不要**在头里 include 任何 VTK。

### 2. XQCrossSectionResampler(新增 visualization 层,封装 vtkImageReslice)
新增 `CODE_ROOT/src/visualization/XQCrossSectionResampler.{h,cpp}`。头 VTK-free(pimpl 或只暴露 opaque handle + POD 参数)。职责:
- 持有对 resident vtkImageData 的引用(经 `void* image` 传入,内部 reinterpret 回 vtkImageData*)。
- 输入一个采样位姿(POD:`double origin[3]; double tangent[3]; double normal[3]; double binormal[3];`)+ 输出断面参数(mm 尺寸 + 每 px mm)。
- 设 vtkImageReslice:
  - `SetResliceAxesOrigin(origin)`;
  - ResliceAxes 方向:**x 轴 = normal,y 轴 = binormal,z 轴 = tangent**(断面法向 = 路径切向)。用 `SetResliceAxesDirectionCosines(normal, binormal, tangent)`。
  - `SetInterpolationModeToCubic()`(对标 SV RESLICE_CUBIC);
  - `SetOutputDimensionality(2)`;
  - 设 `SetOutputSpacing/SetOutputExtent/SetOutputOrigin` 使输出为居中的 2D 断面(如默认 40mm×40mm,0.2mm/px → 200×200)。
- 输出:opaque `void* outputImageHandle()`(2D 断面 vtkImageData,给视图挂 vtkImageSlice)。
- 暴露一个探针:返回本次 reslice 对应的断面 frame（origin/normal=tangent/xAxis=normal/yAxis=binormal）用 POD 结构,供离散测试核对一致性。

**默认断面尺寸/分辨率**:40mm × 40mm,0.2mm/px(200×200)。写成可配置常量,真机可调。

### 3. XQCrossSectionViewWidget(新增 visualization 层,参照 XQSliceViewWidget)
新增 `CODE_ROOT/src/visualization/XQCrossSectionViewWidget.{h,cpp}`。Q_OBJECT widget,VTK-free 头(QVTKOpenGLNativeWidget 前向声明 + 指针)。职责(本批只显示):
- 内部持有一个 XQCrossSectionResampler + 一个 vtkRenderer + vtkImageSlice(挂 resampler 输出)。
- `setSampleFrame(POD pose)` → 驱动 resampler 重采样 → 更新 slice mapper input → 单次 render。**pipeline 常驻,切帧只改 ResliceAxes 参数 + render,绝不每帧重建 GL 上下文/renderer**(memory `render-architecture-must-be-validated-first`:重蹈 GUI v2 卡顿覆辙是本批头号风险)。
- 窗位与主场景联动(共享 window/level 值即可,本批可简单同步)。
- 空态占位提示(无 volume/无路径时显示引导文本)。

### 4. 断面工作台 + 沿路径滑条(app/ui 层接线)
- 主视图区用 QStackedWidget(或等效)承载两页:page0=现有 XQMprWidget 四视图;page1=断面工作台(顶部沿路径滑条 + 中间 XQCrossSectionViewWidget)。
- 进入「轮廓提取」阶段 → 切 page1;退出 → 切 page0。**切换只是 setCurrentWidget,不销毁重建。**
- 滑条:范围 = 当前路径 samplePoints() 索引 [0, M-1];值变 → 取该 PathSamplePoint 组 POD pose → XQCrossSectionViewWidget::setSampleFrame → 断面刷新;显示「第 N/M 点 · 弧长 X mm」。
- 本批路径来源:若阶段已有「选中路径」机制则复用;否则临时取场景内第一条 path 驱动(P3-2 再做正式绑路径下拉)。以能真机验证断面+滑条为准,别硬造复杂绑定。

## 测试(离散不变量,headless ctest)

新增 `CODE_ROOT/tests/visualization/test_cross_section_resampler.cpp`,顶层 `CODE_ROOT/CMakeLists.txt` 照 `test_render_scene` 挂法加(add_executable + `target_link_libraries(... xq_visualization ${VTK_LIBRARIES})` + `vtk_module_autoinit` + `add_test` + offscreen `set_tests_properties` 那组)。

**命门断言(P3-1 正确性核心)**:
1. **frame 一致性**:构造已知体数据 + 已知 PathSamplePoint(origin/tangent/normal/binormal)→ resampler 输出 frame 的 origin==pose.origin、normal==tangent、xAxis==normal、yAxis==binormal(容差 1e-6)。可证伪:把 SetResliceAxesDirectionCosines 轴序改错必转红。
2. **中心像素反投影**:断面输出的中心像素世界坐标 == pose.origin(容差,取决于 output origin/spacing 设置)。可证伪:output origin 偏移必转红。
3. **无 volume**:image handle null → resampler 输出 handle null,不崩。

断言必须可证伪:先 park 反面(错轴序/错 origin)确认红,再改对确认绿。

## 验证命令(worker 必跑,报告贴输出)

1. 全新构建(本批新增带 Q_OBJECT 的 widget = 改头,**必 rm -rf build_gui**):
   ```
   rm -rf build_gui && cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
   ```
2. ctest(基线 68 + 新增 test_cross_section_resampler → 期望 69 全绿):
   ```
   cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"
   ```
3. 假绿抽查:把 test_cross_section_resampler 的轴序断言对应的 resampler 代码篡改(如交换 normal/binormal)→ 重跑该测试必 FAIL(核 exe 时间戳变)→ 还原 → 复绿。报告写明篡改点 + 前后结果。

## 禁止(违反=返工)

- 不做任何 2D 轮廓生成/手绘/自动/绑路径对话框/放样(超范围)。
- 头文件不 include VTK(XQRenderScene.h / XQCrossSectionResampler.h / XQCrossSectionViewWidget.h 保持 VTK-free,VTK 只在 .cpp)。
- 新源文件注释一律英文(MSVC GBK 坑)。
- 断面视图不得每帧重建 renderer/GL 上下文(pipeline 常驻)。
- 不改现有四视图渲染路径(P3-1 是纯新增 + 阶段切换)。
- 不 git commit(worker 无提交权;主审复核后提交)。
- 不跑 lupdate(本批若无新可译串则不碰 .ts;有则手工编辑,报告说明)。
- 不 `git add -A`。

## 交付物(全部在 CODE_ROOT = C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ)

- 新增:src/visualization/XQCrossSectionResampler.{h,cpp}、src/visualization/XQCrossSectionViewWidget.{h,cpp}、tests/visualization/test_cross_section_resampler.cpp。
- 改动:src/visualization/XQRenderScene.{h,cpp}(加 image handle)、src/visualization/XQMprWidget 或主视图容器(工作台切换)、src/ui/panels/XQStageWidgets.{h,cpp} 或 src/app/XQMainWindow.{h,cpp}(滑条 + 阶段切换接线)、CMakeLists.txt(挂测试)。
- 报告:构建输出、ctest 结果(N/N)、假绿抽查前后结果、git diff --stat、遗留问题。
