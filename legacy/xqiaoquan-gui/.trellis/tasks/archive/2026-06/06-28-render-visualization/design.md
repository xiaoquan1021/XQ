# Design — 渲染层做厚并接入主窗口

> 配合 `prd.md` 读。本文件讲技术方案:库结构、VTK 管线、与 core 的边界、接入主窗口的方式。
> 不替代 prd 的验收标准。

## 0. 设计原则

- **pimpl 藏 VTK**:沿用 `XQImageViewer` 已有的做法——公开 `.h` 零 VTK,所有 vtk* 类型只进 `.cpp` 的 `Impl`。
  这是"core/visualization 公开 API 不暴露 kernel"铁律在渲染层的落地方式。
- **XQ 几何 → vtkPolyData/vtkImageData 的转换器集中一处**:写一个内部 `VtkConverters`(仅 .cpp/匿名命名空间),
  把 `XQTriangleSurfaceGeometryHandle` / `XQTetVolumeMeshHandle` / `XQImageVolume` 转成 VTK 数据对象。
  渲染器只调转换器,不散落转换逻辑。
- **一个场景渲染器,多种 actor**:不是每类产物一个独立窗口,而是一个 `XQSceneRenderer` 持有一个
  `vtkRenderer`,按 payload 类型装配不同 actor,统一 render。主窗口选中节点 → 渲染器换 actor。

## 1. 库结构(`src/visualization/`)

```
src/visualization/
  XQImageViewer.{h,cpp}        # 现有,保留不破坏(offscreen 影像→RGBA)
  XQSceneRenderer.{h,cpp}      # 新增:核心场景渲染器,持 vtkRenderer,装配各类 actor
  XQRenderWidget.{h,cpp}       # 新增:QVTKOpenGLNativeWidget 封装(交互 3D 视图);若 GUISupportQt 不可用则本文件缓做
  (internal, in .cpp only) VtkConverters   # XQ 几何 → vtkPolyData/vtkImageData
```

### 1.1 `XQSceneRenderer` 公开接口(草案,零 VTK 类型)

```cpp
namespace xq {
class XQImageVolume;
class XQTriangleSurfaceGeometryHandle;
class XQTetVolumeMeshHandle;
class XQPathPayload;
class XQFlowResultPayload;

struct RenderStats { bool ok; int actorCount; long long pointCount; };

class XQSceneRenderer {
public:
    XQSceneRenderer();
    ~XQSceneRenderer();
    XQSceneRenderer(const XQSceneRenderer&) = delete;
    XQSceneRenderer& operator=(const XQSceneRenderer&) = delete;

    void clear();                                            // 清空所有 actor
    RenderStats addImageSlice(const XQImageVolume& img, int axis, int slice);
    RenderStats addPath(const XQPathPayload& path);
    RenderStats addSurface(const XQTriangleSurfaceGeometryHandle& surf);   // 按 faceId 着色 + 法线自定向
    RenderStats addVolumeMesh(const XQTetVolumeMeshHandle& mesh);          // 边界面 / 线框
    RenderStats addFlowResult(const XQFlowResultPayload& flow);            // 按标量着色

    // offscreen 渲染当前场景到 RGBA(测试用,无需窗口)
    RenderStats renderOffscreenToRgba(int width, int height, std::vector<unsigned char>* outRgba);

    // 供 XQRenderWidget 取内部 vtkRenderer 挂到 render window(仅同库 .cpp 间用,
    // 不暴露 vtk 类型:用 void* 句柄或 friend,二选一,见 §3)。
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace xq
```

> 注意:`addImageSlice` 的 `axis/slice` 是切片选择;影像 2D 切片渲染用 `vtkImageSliceMapper` + `vtkImageSlice`。

## 2. VTK 渲染管线(各类产物)

| 产物 | XQ 源 | 转换 | VTK 管线 |
|---|---|---|---|
| 影像切片 | `XQImageVolume`+buffer | → `vtkImageData`(scalar 拷进,spacing/origin/direction 来自 geometry) | `vtkImageSliceMapper`(SetSliceNumber) → `vtkImageSlice`,window/level 用 image 的 center/width |
| 路径 | `XQPathPayload` | 点序列 → `vtkPolyData`(vtkPolyLine) | `vtkTubeFilter`(可选管状) → `vtkPolyDataMapper` → `vtkActor` |
| 分割掩膜 | `XQSegmentationMaskPayload` | mask → `vtkImageData`,用 `vtkLookupTable` 上色 + alpha | 叠加在影像 slice 上(同一 renderer,半透明) |
| 表面模型 | `XQTriangleSurfaceGeometryHandle` | points+triangles → `vtkPolyData`;faceId → cell scalar | `vtkPolyDataNormals`(**AutoOrientNormalsOn / ConsistencyOn**,见铁律3) → mapper(faceId 走 LUT 着色) → actor |
| 体网格 | `XQTetVolumeMeshHandle` | tets → `vtkUnstructuredGrid`;抽边界面或 `vtkExtractEdges` 线框 | `vtkDataSetMapper` 或 `vtkGeometryFilter`+edges → actor(线框模式) |
| 流场结果 | `XQFlowResultPayload` | 表面/线 + 标量数组 → `vtkPolyData` point/cell scalar | `vtkPolyDataMapper`(SetScalarRange + LUT) → actor + `vtkScalarBarActor` |

### 2.1 表面法线自定向(铁律 3 的落地)

M3 表面 winding 全局不一致。两种处理,**择一并在代码注释标明**:
- **首选**:`vtkPolyDataNormals`,`SetAutoOrientNormals(true)` + `SetConsistency(true)`,让 VTK 统一定向。
- **备选**:转换时按有符号体积(对每个三角相对网格质心)翻正,与 M4 VolumeMeshService 的做法一致
  (PROGRESS.md / memory `xq-surface-winding-not-consistent`)。

不允许直接把 raw 三角喂给 mapper 不做定向——否则光照大面积翻面。AC5 会查。

## 3. 边界设计:`XQSceneRenderer` ↔ `XQRenderWidget` 不泄漏 VTK

交互 widget(`QVTKOpenGLNativeWidget`)需要把 renderer 挂到它的 `vtkGenericOpenGLRenderWindow`。
但 `XQSceneRenderer` 公开头不能暴露 `vtkRenderer*`。方案:

- `XQRenderWidget.cpp` 与 `XQSceneRenderer.cpp` **同库**,可用 `friend class XQRenderWidget;` 让 widget
  访问 `XQSceneRenderer::Impl` 拿 `vtkRenderer`;friend 声明在 `XQSceneRenderer.h` 里(前向声明 widget),
  不引入 vtk include。
- 或者 `XQSceneRenderer` 提供 `void attachToRenderWindow(void* nativeRenderWindow)` 之类的不透明句柄接口。

> 首选 friend(同库、类型安全)。两者都不让 vtk 类型进 .h。

## 4. 接入 `XQMainWindow`

现状:中央靠 `QLabel imageLabel_` 贴 offscreen RGBA。改造:

1. 新增成员 `XQRenderWidget* renderWidget_`(若 GUISupportQt 可用),放进中央区(`setCentralWidget` 或
   与现有布局并存)。持有一个 `XQSceneRenderer`。
2. **场景树选中 → 渲染**:`sceneTreeView_` 的 selectionModel 的 `currentChanged` 信号 → 槽里取选中 `XQDataNode`
   的 payload 类型 → 调 `renderer.clear()` + 对应 `add*` + `renderWidget_->render()`。
   - 这层接线在 `app_shell`(`XQMainWindow.cpp`),属界面层,允许碰 Qt;但**只做分发,不做渲染逻辑**。
3. **保留 `setScene`/`showImage` 旧路径**:`test_main_window` 依赖它们,不破坏(AC7)。
   新交互路径与旧 offscreen 路径并存。
4. 降级路径(GUISupportQt 不可用):不建 `XQRenderWidget`,改成选中节点 → `XQSceneRenderer::renderOffscreenToRgba`
   → 贴 `imageLabel_`(复用现有机制),支持切节点 / 切层但无 3D 交互。

## 5. CMake 改动

```cmake
# 1. 验证交互 widget 模块(执行者第一步先单独 find_package 试):
find_package(VTK 9 REQUIRED COMPONENTS ... GUISupportQt RenderingUI)   # 若可用

# 2. xq_visualization 增源,link 不变(仍 PUBLIC xq_core + PRIVATE ${VTK_LIBRARIES}):
add_library(xq_visualization STATIC
    src/visualization/XQImageViewer.cpp
    src/visualization/XQSceneRenderer.cpp
    src/visualization/XQRenderWidget.cpp   # GUISupportQt 可用才加
    ...)
# 若 XQRenderWidget 用 Qt,需 AUTOMOC ON + link Qt6::Widgets(此时 visualization 会引入 Qt;
# 可接受——visualization 本就是 sink 层;但 core/services 仍零 VTK/Qt,铁律不破)。

# 3. 新增测试:
add_executable(test_scene_renderer tests/visualization/test_scene_renderer.cpp)
target_link_libraries(test_scene_renderer PRIVATE xq_visualization)
add_test(NAME test_scene_renderer COMMAND test_scene_renderer)
```

> 注意:若 `XQRenderWidget` 让 `xq_visualization` link Qt6::Widgets,**确认这不违反铁律**——
> 铁律管的是 core/services/controllers 零 VTK/Qt;visualization 是渲染 sink,link Qt 渲染 widget 合理。
> 但要保证 `xq_visualization` 公开头不强迫下游引入 Qt(pimpl)。

## 6. 测试设计(对应 AC4/AC5/AC6)

`tests/visualization/test_scene_renderer.cpp`(offscreen,QT_QPA_PLATFORM=offscreen):

- 构造一个小 `XQTriangleSurfaceGeometryHandle`(如一个四面体的 4 个三角面),`addSurface` →
  `renderOffscreenToRgba` → 断言 `ok && actorCount==1 && pointCount==4`,RGBA 非全黑。
- 同理一个小 `XQTetVolumeMeshHandle`、一个 `XQImageVolume` 切片、一条 path。
- **法线自定向断言**:构造一个 winding 故意不一致的表面,渲染后不崩 + actor 装配成功(定向逻辑被走到)。
- **CHECK 宏式**(铁律):`if(!cond){fprintf(stderr,...);return 1;}`;**副作用调用先取变量再判断,绝不进 assert**。
- 假绿抽查点(执行者验收时用,勿写死进测试):在 `addSurface` 里临时"不 AddActor" → 测试应 FAIL。

## 7. 不做 / 后续(防 scope 蔓延)

- 体绘制、MPR 联动、剪切/测量、流场动画 → 后续任务。
- 大网格抽稀 / LOD → 后续。
- 若 GUISupportQt 不可用,交互 widget 整体降级,记为后续任务,本任务以 offscreen + 切节点为完成线。
