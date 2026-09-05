# design.md — 常驻 GPU 渲染管线 + 可见性驱动场景合成

> 任务 `07-04-render-arch-rebuild`。前置阅读:prd.md(事故根因)、research/render-code-inventory.md(废弃/可抄清单)、research/visual-baseline.md(验收基线)。
> 本文写「新架构长什么样、为什么这样定」;批次执行细节在 implement.md。
> 行号/接口均已对 feat/gui-v2 @ a3715e5 源码核实(2026-07-04)。

## 1. 设计总则

- **两个常驻层**:数据常驻(体数据/几何 → VTK 数据对象只建一次)+ 视图常驻(4 个 QVTKOpenGLNativeWidget + 4 个 vtkRenderer 与主窗口同生命周期)。此后一切交互 = 改参数 + Render(),不再有 clear-and-rebuild、不再有离屏回读、不再有每帧 GL 上下文。
- **渲染模型 = 可见性驱动场景合成**:场景里每个可渲染节点对应一组常驻 actor,勾选框控显隐;切片视图 = 图像 + 派生物叠加;3D 视图 = 所有可见节点合成。选中只做高亮,不触发重建。
- **分层不破**:新渲染核心进 `xq_visualization`(VTK 私有、无 Qt),新窗口部件进 `xq_render_widget`(Qt+VTK);core/services/io/adapters 接口零改动,可见性/透明度/颜色是**呈现属性,只存 UI 层**,不进 core。

## 2. 组件划分

```
xq_visualization(无 Qt)
  XQRenderScene        ← 新,渲染内核/场景合成器(M1 核心)
  SurfaceLodBuilder    ← 保留复用(LOD 契约不变)
  ChunkPlan.h          ← 保留复用(分块不变量测试在管)
  XQSceneRenderer      ← 降级 test-only(离屏路径仅供旧库测试;产品运行时不得触达)

xq_render_widget(Qt+VTK)
  XQMprWidget          ← 新,2×2 四视图容器(替 XQMprView,后者删除)
  XQSliceViewWidget    ← 新,单切片视图(QVTKOpenGLNativeWidget + 2D 交互)
  XQVolumeViewWidget   ← 新,3D 视图(替 XQRenderWidget,后者删除)

xq_app_shell
  XQMainWindow         ← 渲染/选中/导航联动段重写(约 1/3);M4 拆出 WorkflowSession
  XQSceneModel(xq_ui) ← 加可见性勾选列(CheckStateRole),NodeId→bool 存模型内
```

### 2.1 XQRenderScene(渲染内核,M1)

公开头 VTK-free(pimpl,沿用 XQSceneRenderer 的手法)。持有 4 个常驻 vtkRenderer(Axial/Sagittal/Coronal/Volume3D),widget 层经不透明句柄挂载:

```cpp
enum class ViewId { Axial, Sagittal, Coronal, Volume3D };

class XQRenderScene {
    // ---- 体数据(一次上传) ----
    // 从 volume+buffer 构建常驻 vtkImageData(一次,memcpy 快路径见 §3.1),
    // 三个切片视图各挂一个 vtkImageSlice(vtkImageSliceMapper,共享同一 vtkImageData),
    // 3D 视图挂三张正交切片平面(MITK 风格)。共享一个 vtkImageProperty(窗宽窗位联动)。
    bool setVolume(const XQImageVolume& img, const XQMemoryImageBufferHandle* buffer);
    void clearVolume();

    // ---- 切片/窗宽窗位(改参数,O(切片)) ----
    void setSliceIndex(int axis, int index);      // 只改 mapper SliceNumber + 十字线
    int  sliceIndex(int axis) const;
    int  sliceCount(int axis) const;
    void setWindowLevel(double window, double level);   // 共享 vtkImageProperty
    void windowLevel(double* window, double* level) const;
    void setCrosshairIndex(int i, int j, int k);  // 十字线 + 3D 三平面同步
    void setCrosshairVisible(bool on);

    // ---- 节点(常驻 actor + 显隐) ----
    // upsert:节点首次出现构建 actor 组(payload→actor 构建器抄旧 SceneRenderer),
    // 已存在且 payload 未变则复用;进 3D 视图,切片叠加见 §3.3。
    RenderStats upsertNode(const NodeId& id, const XQDataNode& node);
    RenderStats upsertNodeProgressive(const NodeId& id, const IGeometrySource& src,
                                      GeoKind kind, const ChunkUploadSpec& spec,
                                      const ChunkProgressFn& onChunk = {});
    void removeNode(const NodeId& id);
    void clearNodes();
    void setNodeVisible(const NodeId& id, bool on);   // actor->SetVisibility,全视图
    void setNodeOpacity(const NodeId& id, double a);
    void setNodeColor(const NodeId& id, double r, double g, double b);

    // ---- 离散不变量探针(headless 测试用,无像素断言) ----
    int  actorCount(ViewId v) const;
    bool nodeVisible(const NodeId& id) const;
    bool hasNode(const NodeId& id) const;
    long long uploadedPointCount(const NodeId& id) const;

    void* vtkRendererHandle(ViewId v) const;   // widget 挂载用,同旧契约
};
```

RenderStats / LodOptions / ChunkUploadSpec 结构体**原样沿用**(`pointCount` 恒报源点数、`uploadedPointCount` 语义、chunk 计数——lod-and-upload.md 契约全部继承)。

### 2.2 XQMprWidget / XQSliceViewWidget / XQVolumeViewWidget(M1 骨架 + M2 交互)

- `XQMprWidget`:2×2 网格(Axial 红框 / Sagittal 绿框 / Coronal 蓝框 / 3D 黄框,轴配色对齐旧 XQ),LayoutMode Quad/Single 保留;objectName 沿用现有测试所依赖的名字(xqMprAxial 等,B1 时以 test_main_window 实际引用为准)。
- `XQSliceViewWidget`:QVTKOpenGLNativeWidget + `vtkInteractorStyleImage` 子类:
  - 滚轮 → 切片 ±1;左键拖 → 窗宽窗位;右键拖/滚轮+Ctrl → 缩放;中键拖 → 平移;
  - 左键单击 → 拾取(经 vtkCellPicker/世界坐标→体素索引),发 `voxelPicked(i,j,k)`;
  - 十字线两条线 actor 由 XQRenderScene 持有,widget 只发交互信号。
- `XQVolumeViewWidget`:trackball 相机,场景来自 XQRenderScene 的 Volume3D renderer。
- `configureDefaultSurfaceFormat()`(QApplication 前调)从旧 XQRenderWidget 原样搬。

### 2.3 主窗口接线(M1 最小 → M4 定形)

渲染入口从 `onSceneSelectionChanged`(XQMainWindow.cpp:2104-2237,整段废弃)换成**场景同步**:

- `syncRenderScene()`:遍历 `scene_->visit_nodes`,对每个可渲染节点 `upsertNode`;对已移除节点 `removeNode`。调用时机 = 工程加载后 / 命令栈 push/undo/redo 后(现有各 commit 点已集中,M4 收进 WorkflowSession)。
- 选中变化只做高亮/属性面板刷新,不再触碰渲染器。
- Image 节点加载路径保留(activeImage_/XQDemoVolume 容器 + VtkImageAdapter),加载成功 → `renderScene_->setVolume(...)`。

## 3. 关键技术决策(定论 + 理由)

### 3.1 切片方案:vtkImageSliceMapper 常驻输入(不用 vtkImageReslice 全功能路线)
- prd 允许二选一。选 vtkImageSliceMapper:正交 MPR 下改 SliceNumber 只重传单张切片纹理,O(切片),实现最简、风险最低;斜切/曲面重建是 Non-goal。
- vtkImageData 构建**每卷一次**:XQMemoryImageBufferHandle 若暴露连续 float span 则 memcpy(M8b-1 结论:消费者要整块连续视图),否则逐体素拷——但只发生在加载时,不在交互路径。origin/spacing/direction 的 apply_geometry 逻辑从旧 build_image 抄(盘点确认正确)。
- Sagittal/Coronal 黑屏与 Axial 比例异常的根因(离屏相机/贴图)随旧路线一起消失;相机按轴设 ParallelProjection + 正确 up 向量,B1 真机对照基线核。

### 3.2 actor 共享策略:数据共享、actor/mapper 每视图独立
4 个 widget 是 4 个 GL 上下文。重资产(vtkImageData/vtkPolyData)全场景共享一份;actor+mapper 每视图自建(轻)。避免跨上下文共享 mapper 的 VTK 资源管理边角坑,这也是 MITK 的做法。

### 3.3 切片叠加(M3)
- **轮廓(.ctgr)**:SvProjectReader 现只给 ContourGroup 节点挂 XQSourcePayload(SvProjectReader.cpp:397-401,未解析)——这就是"轮廓躺在列表里"的直接原因。M3 在 app 层首次需要时经 CTGRContourReader 解析并缓存到节点 payload;每切片视图上,距当前切片平面 < spacing/2 的 contour 画为常驻 polyline actor(切片变化只改可见集合)。
- **分割掩膜**:第二层 vtkImageSlice + 二值 LUT(0 透明/1 着色),共享掩膜 vtkImageData,SliceNumber 与主图联动——标准做法,零拷贝。
- **表面截线**(基线截图的绿色轮廓也可能来自 model):vtkPolyDataPlaneCutter 对可见 surface 节点截线,只在切片变化时重截。若真机测出大网格卡顿,此项降为可开关(默认开,门槛=真机流畅)。

### 3.4 可见性/透明度/颜色的归属
core 不加字段(Non-goal:不动 core 接口)。XQSceneModel 内部持 `NodeId→bool` 勾选态(CheckStateRole 列),变化发信号 → 主窗口 → `renderScene_->setNodeVisible`。Opacity/Color 同路(M3 做回,07-03 B2 删除是错误决策的回滚)。默认全部可见(对齐旧 XQ 打开即所见)。工作区持久化可见性不在本任务范围。

### 3.5 LOD / 渐进上传的继承
SurfaceLodBuilder、ChunkPlan、copy-on-upload、faceId 全局 LUT range、连通性 bounds gate——**契约全部原样继承**进 XQRenderScene 的 surface/mesh 构建器(lod-and-upload.md 是既有规范,不重开讨论)。交互视图挂 vtkLODActor(interactive=true),CPU 侧 selectLodLevel 仍是唯一可测选级源。app 层默认 LodOptions 沿用 kDefaultLodOptions(enabled+interactive+2M budget)。

### 3.6 旧代码处置
- `XQMprView`(905 行)、`XQRenderWidget`(129 行):**删除**(M5 收口时移出构建;概念性遗产——axis 映射、seedPicked 信号形状、LayoutMode、changeEvent retranslate——已吸收进新部件)。
- `XQSceneRenderer`:**保留为 test-only**。test_scene_renderer / test_scene_renderer_progressive / test_surface_lod 仍链它跑离屏断言(库级契约有效);CMake 加注释声明 test-only,产品侧唯一消费者 XQRenderWidget 删除后自然无产品引用。renderOffscreenToRgba 不搬进 XQRenderScene。
- 新增测试原则(盘点定论):验收点=离散不变量(upsert N 可见节点→actorCount(Volume3D)==N;setSliceIndex→探针读回==idx;setNodeVisible(false)→nodeVisible false),不做像素断言;流畅度/观感只认真机。

### 3.7 线程纪律
VTK 全部 GUI 线程(架构规范既有铁律)。XQTaskRunner 双跳投递不变;SurfaceLodBuilder buildAsync 的"深拷贝输入 + 主线程回填"契约不变。

## 4. 模块依赖与 CMake 变更

```
xq_visualization  += XQRenderScene.{h,cpp}           (无 Qt,链 VTK PRIVATE)
xq_render_widget  += XQMprWidget/XQSliceViewWidget/XQVolumeViewWidget
                  -= XQMprView/XQRenderWidget(M5 移除)
xq_ui             XQSceneModel 加列(无新依赖)
xq_app_shell      主窗口重接(依赖不变)
新测试            test_render_scene(链 xq_visualization,offscreen 探针断言)
```

check_arch_boundaries.cmake 继续在管:io/core 永无 vtk*,services 无 Qt。

## 5. 模块→批次映射与真机门禁

| 模块 | 批次(implement.md) | 真机门禁 |
|---|---|---|
| M1 渲染内核 | B1a(XQRenderScene+测试)→ B1b(三部件+主窗口最小接线) | **用户过目,不过不盖 M2** |
| M2 四视图交互 | B2 | 交互流畅度用户过目 |
| M3 可见性场景树 | B3 | 勾选即显隐 + 叠加对照基线 |
| M4 窗口瘦身 | B4 | ctest + 真机回归 |
| M5 收口 | B5 | 逐项对照 visual-baseline 签收 |

## 6. 风险与对策

| 风险 | 对策 |
|---|---|
| vtkImageSliceMapper 非轴对齐 direction 体数据显示异常 | 0007_H_AO_H 实测为准;真出现再评估 vtkImageResliceMapper,不预防性加分支 |
| 大表面 vtkPolyDataPlaneCutter 截线卡 | 只在切片变化时重截 + 真机测;不达标降为开关项 |
| 28 vs 14 节点差异根因未知 | M3 内查 SvProjectReader 覆盖面,先查清再改,不猜 |
| 四上下文显存翻倍(切片纹理每视图一份) | 切片纹理是 O(切片)量级,可忽略;3D 三平面共享 vtkImageData host 侧仅一份 |
| 新部件破坏 test_main_window 结构断言 | B1b 先读该测试引用的 objectName 清单,新部件沿用 |
