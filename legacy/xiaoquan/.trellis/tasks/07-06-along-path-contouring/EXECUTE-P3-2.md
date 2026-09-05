# EXECUTE P3-2:轮廓组绑路径 + 手绘圆/多边形(端到端 MVP)

> 语境锚点:XQ 医学影像软件血管几何重建。本批打通「选路径建组 → 沿路径滑条定位断面 → 在断面上手绘圆/多边形轮廓 → 入组 → 手动放样出血管管」。纯几何建模 + 影像可视化工程,对标 SimVascular sv4gui 分割工作流。**本批只做圆 + 多边形两种手绘 + 绑路径下拉 + 放样打通;不做椭圆/样条/断面阈值/水平集/批量/多血管**(那些是 P3-3~P3-5)。

## 活动任务

- 任务:`07-06-along-path-contouring`(in_progress)。本批 = P3-2(六批第二批,**主里程碑**)。
- **两个不同 git 工作树,绝不混**:
  - **代码工作树(唯一改代码处)= `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`**,分支 `feat/render-arch`,HEAD=`0b45424`。本简报所有 `CODE_ROOT/...` 都指这里。src/tests/CMakeLists/resources 编辑、build、ctest 全在这个工作树,**用绝对路径**。
  - 任务文档/spec/SV 参照在主仓 `C:\Users\OCEAN\Desktop\XIAOQUAN`(= 你的 cwd,读 prd/design/implement/spec 用相对路径)。
- **凡本简报写 `CODE_ROOT`,替换为 `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`**。**绝不在主仓 XIAOQUAN 下建/改任何 src 代码文件。**

## 目标(本批范围,严格不越界)

1. 轮廓组创建时能从下拉**选一条路径绑定**(修 P3-1 遗留的「硬编码取场景第一条 Path」)。
2. `ContourExtractionService`(services/segmentation,**纯域无 VTK/Qt**)提供圆/多边形的「控制点 → 2D 轮廓点」几何。
3. 断面视图 `XQCrossSectionViewWidget` 加**绘制交互**:鼠标点/拖生成控制点 → 预览 → 断面 2D 点经 `unprojectFromFrame` 回世界坐标 → 入组成 `XQContour`。
4. 断面工作台加**方法工具栏(圆/多边形)+ 进入/退出编辑按钮**。
5. 端到端:沿路径放若干轮廓 → 现有建模页 Loft → 出血管表面(数据链验证)。

**本批不做**:椭圆/样条几何、断面阈值/水平集/区域生长自动、批量/多血管、放样实时预览(放样仍走现有「建模页选轮廓组点 Loft」手动链)。

## 现状(已由主审亲读源码,worker 照此,别重扫)

### 数据结构(core,已就位不改)
- `CODE_ROOT/src/core/XQContourGroup.h`:
  - `ContourFrame{ Point3 origin; Vec3 normal; Vec3 xAxis; Vec3 yAxis; }`。
  - `XQContour{ ContourId contourId; double pathArcLength; ContourFrame frame; ContourType type; std::vector<Point3> points; bool closed; }`。
  - `ContourType` 已含 `Circle` / `Manual`(多边形用 `Manual`)。
  - `XQContourGroup`:`setSourcePathNode/hasSourcePathNode/sourcePathNode`、`addContour`、`orderedByPathPosition`(按 pathArcLength stable_sort)、静态 `projectToFrame` / `unprojectFromFrame`。
  - **`unprojectFromFrame(frame,u,v) = origin + u·xAxis + v·yAxis`**(已核实实现)。这是断面 2D → world 的唯一通道。
- `CODE_ROOT/src/core/XQPath.h`:`PathSamplePoint{ position, tangent, normal, binormal, arcLength }`;`samplePoints()`。
- `CODE_ROOT/src/core/XQContourGroupPayload.h`:`XQContourGroupPayload`(包 `XQContourGroup`,domainType()==ContourGroup)。已有 `group()` 访问器(核对头文件确认)。

### 断面坐标映射(命门,已亲读 resampler.cpp 核实)
断面输出 vtkImageData 的坐标系:reslice 设 `SetResliceAxesDirectionCosines(normal, binormal, tangent)`,`SetResliceAxesOrigin(pose.origin)`,`SetOutputOrigin(-half,-half,0)`。**结论:断面输出的局部平面坐标 (lx, ly) mm 就是断面 2D 坐标 (u, v)**——局部 (0,0)=pose.origin=断面中心,x 轴=normal,y 轴=binormal。
- `XQCrossSectionResampler::lastFrame()` 返回 `XQCrossSectionFrame{ origin, normal, xAxis, yAxis }`,字段与 core `ContourFrame` **一一对应**(origin=pose.origin,normal=tangent,xAxis=normal,yAxis=binormal)。
- 手绘 2D 点 (u,v) mm → world:构造 `ContourFrame`(从 `lastFrame()` 拷字段)→ `XQContourGroup::unprojectFromFrame(frame, u, v)`。**这条链 P3-1 已用离散测试锁死一致性。**

### 断面视图现状(visualization,本批要在此加交互)
- `CODE_ROOT/src/visualization/XQCrossSectionViewWidget.{h,cpp}`:Q_OBJECT widget,VTK-free 头。内部 `Impl` 持 `XQCrossSectionResampler resampler_` + `vtkRenderer renderer_`(ParallelProjection)+ `vtkImageSliceMapper` + `vtkImageSlice slice_`。`vtkWidget_`=`QVTKOpenGLNativeWidget`。offscreen 平台不挂 render window(headless 用)。已有 `setSampleFrame(pose)` / `lastFrame()` / `syncWindowLevel()` / `renderNow()`。
- 交互器 style 目前是 `vtkInteractorStyleImage`(默认平移缩放)。**绘制模式要接管鼠标**(见实现 §3)。

### app/ui 接线现状(已亲读)
- `CODE_ROOT/src/app/XQMainWindow.cpp`:
  - `buildCrossSectionWorkbench()`(:2905):建 `crossSectionWorkbench_`(QWidget),顶部 `alongPathSlider_`(QSlider)+ `alongPathLabel_`,中间 `crossSectionView_`。滑条 `valueChanged` → `updateCrossSectionAtSample(value)`。
  - `bindCrossSectionPath()`(:2949):**P3-2 头号要修**——硬编码 `scene->visit_nodes` 取第一条 Path 的 samplePoints 存进 `sectionPathSamples_`。要改为读「当前轮廓组绑定的 sourcePathNode」。
  - `updateCrossSectionAtSample(index)`(:2999):取 `sectionPathSamples_[index]` 组 `XQCrossSectionPose` → `crossSectionView_->setSampleFrame(pose)`。
  - `showStagePage(index)`(:2802):`index==1` 切 `crossSectionWorkbench_` 并 `bindCrossSectionPath()`;其余切 `mprWidget_`。轮廓提取阶段 = stage index 1。
  - 现有 `ContourGroupProvider contourGroupProvider`(:856)从 combo id 拿 `ActiveContourGroup{valid,nodeId,contourGroup}`。
  - 建组入组参照::2586-2631 有把解析出的 `XQContourGroup` 塞进 `XQContourGroupPayload` 挂节点的范式(`node->setPayload(XQDomainType::ContourGroup, std::make_shared<XQContourGroupPayload>(...))` + `refreshSceneTree()`)。
- `CODE_ROOT/src/ui/panels/XQStageWidgets.{h,cpp}`:
  - `buildModelingPage`(:832)用 `makeNodeCombo(group, "xqModelingSourceCombo", nodeLister, XQDomainType::ContourGroup)` 建下拉 —— **这就是绑路径下拉可复用的模式**(改成 `XQDomainType::Path`)。`makeNodeCombo(QWidget* parent, const char* objectName, SceneNodeLister lister, XQDomainType domain)` 返回 `NodeComboBox*`,有 `repopulate()`;`idFromCombo(combo)` 取选中 NodeId。
  - **stage panel 页序已核实**:index 0=`buildPathPage`,**index 1=`buildSegmentationPage`(旧整卷阈值 UI)**,2=`buildModelingPage`... `showStagePage(index==1)` 时中央视图切到断面工作台(`crossSectionWorkbench_`),但 stage panel 右侧那一页仍是旧 `buildSegmentationPage`。**`buildSegmentationPage` 当前签名不含 `nodeLister`**(选路径下拉要用它)。
- **决策(定死,别改 buildSegmentationPage 签名):** P3-2 的「选路径下拉 + 新建轮廓组 + 方法工具栏(圆/多边形)+ 进入/退出编辑」**全部放断面工作台顶栏**(app 层 `buildCrossSectionWorkbench`,能直接拿 scene/nodeLister,不用动 buildSegmentationPage 及其调用点)。让它们贴着断面视图形成一条连贯操作带:选路径 → 新建组 → 滑条定位 → 方法 → 编辑绘制。**不改 `buildSegmentationPage`。**

### SV 参照算法(主审已亲读,直接照抄语义,worker 别再读 SV)
- **圆 Circle**(`sv4gui_ContourCircle.cxx`):2 控制点 c0=圆心、c1=边界点;`radius = dist(c0,c1)`;采样 `interNumber=36` 点:第 i 点 `(c0.u + r·cos(α), c0.v + r·sin(α))`,`α = i·2π/36`,i=0..35;closed=true。
- **多边形 Polygon**(`sv4gui_ContourPolygon.cxx`):SV 前 2 控制点是内部 center/scaling,实际顶点从 index 2 起,段间线性插值。**XQ 简化**:用户点击的 N 个顶点(N≥3)**直接作为多边形顶点**(不搞 SV 的 center/scaling 前 2 点约定,XQ 数据模型存世界坐标点不存控制点)。段间线性插值:相邻顶点 pt_i → pt_{i+1} 之间插 `k` 个点(见实现 §2 给定公式);闭合时最后一个顶点连回第一个。closed=true。

## 实现(接口已定死,worker 不自由发挥)

### 1. ContourExtractionService(新增 services/segmentation,纯域无 VTK/Qt)
新增 `CODE_ROOT/src/services/segmentation/ContourExtractionService.{h,cpp}`。纯 C++,只依赖 `core/GeometryTypes.h`(用 2D 点)。**头/实现注释一律英文**。

设计 2D 点用简单结构(别引 VTK/Qt):
```cpp
struct ContourPoint2D { double u; double v; };
```
接口(静态方法,无 throw):
```cpp
class ContourExtractionService {
public:
    // Circle from 2 control points (center, boundary). radius = dist(center,
    // boundary). Samples `sampleCount` points (default 36, min 8) evenly:
    // p_i = center + r*(cos a_i, sin a_i), a_i = i*2pi/sampleCount. Closed loop.
    // Returns empty when radius <= 0.
    static std::vector<ContourPoint2D> circle(const ContourPoint2D& center,
                                              const ContourPoint2D& boundary,
                                              int sampleCount = 36);

    // Polygon from >=3 vertices. Each edge v_i -> v_{i+1} (wrapping the last
    // back to the first, closed) is linearly subdivided into `perEdge`
    // segments (perEdge>=1); vertex points are included, the duplicated closing
    // vertex is not. Returns empty when fewer than 3 vertices.
    static std::vector<ContourPoint2D> polygon(const std::vector<ContourPoint2D>& vertices,
                                               int perEdge = 8);
};
```
- circle:`sampleCount` clamp 到 >=8;radius=hypot(dU,dV);radius<=0 返回空。
- polygon:vertices.size()<3 返回空;perEdge clamp 到 >=1;对每条边 push 起点 + (perEdge-1) 个内插点(内插点 = start + t·(end-start),t=k/perEdge,k=1..perEdge-1);闭合边(最后一个顶点→第一个顶点)同样处理但**不重复 push 第一个顶点**。结果是有序闭合 2D 环。

### 2. XQCrossSectionViewWidget 绘制交互(visualization,改现有 widget)
在 `CODE_ROOT/src/visualization/XQCrossSectionViewWidget.{h,cpp}` 加绘制层。头保持 VTK-free(新加的都是 Qt / POD)。

**绘制方法枚举**(头里,POD):
```cpp
enum class DrawMethod { None, Circle, Polygon };
```
**公有接口**:
```cpp
void setDrawMethod(DrawMethod method);   // None = 退出编辑(默认)
DrawMethod drawMethod() const;
signals:
    // Emitted when the user finishes a contour. `points` are section-local 2D
    // (u,v) mm coordinates; the app maps them through unprojectFromFrame using
    // the widget's lastFrame() and adds an XQContour at the current arc length.
    void contourDrawn(xq::DrawMethod method, const QVector<QPointF>& points);
```
> 用 `QPointF` 传 (u,v),避免头里引入自定义结构;`DrawMethod` 放 `xq` 命名空间。信号发的是**断面局部 2D (u,v) mm**,不是屏幕像素——像素→(u,v) 的映射在 widget 内做完(见下)。

**交互实现(.cpp)**:
- `setDrawMethod` 非 None 时进入绘制:**换掉交互器 style**——用 `vtkInteractorStyleUser`(或自定义空 style)接管,别让默认平移缩放吃掉 press(参照 memory `xq-seed-pick-eventfilter-vtk-race`:显式接管交互,别抢事件时序)。None 时还原 `vtkInteractorStyleImage`。offscreen 平台无 interactor,跳过(不崩)。
- 装 vtkCallback(或 QVTK 的鼠标事件)监听 LeftButtonPress / MouseMove / LeftButtonRelease / 双击。
- **屏幕像素 → 断面局部 (u,v) mm 映射**:断面渲染是 ParallelProjection,slice 在 z=0 平面,输出图像局部坐标 (lx,ly) 就是 (u,v)。用 VTK 世界坐标反投影:
  - `vtkRenderer::SetDisplayPoint(x, y, 0); WorldToDisplay/DisplayToWorld` → 得世界坐标 (wx,wy,wz);因为 slice 在 z=0 局部平面且相机正交对齐,**(wx, wy) 即 (u, v) mm**(section 局部坐标系)。核对:reslice 输出的 vtkImageData 局部原点在 (-half,-half),但 vtkImageSlice 把它摆在 renderer 世界坐标同一位置——用 `DisplayToWorld` 拿到的世界点减去可能的偏移。**worker 必须用 P3-1 已锁的映射验证:断面中心像素反投影 world ≈ (0,0)。** 若拿到的世界坐标有 (-half) 偏移,补正到以断面中心为 (0,0)。
  - Qt 的鼠标 y 轴与 VTK display y 轴相反(Qt 顶为 0,VTK 底为 0),换算 `vtkY = height - qtY - 1`。
- **圆**:press 记圆心(第 1 点),drag/release 记边界点(第 2 点);release 时 emit `contourDrawn(Circle, {center, boundary})`(发这两个控制点即可,app 端调 service 生成 36 点;**或** widget 内直接调 service 生成后发完整环——二选一,推荐 widget 只发 2 控制点,几何生成在 app 调 service,保持 widget 薄)。**统一约定:widget 发控制点,app 调 ContourExtractionService 生成轮廓点。**
- **多边形**:每次 LeftButtonPress 加一个顶点;双击 或 Enter 闭合 → emit `contourDrawn(Polygon, {v0,v1,...})`;Esc 取消当前绘制(清空控制点,不 emit)。
- **预览**:绘制中用一个 vtkActor(vtkPolyData + line)画当前控制点/预览环,鼠标移动实时更新。预览 actor 独立于已入组轮廓的显示。**预览只 render,不重建 renderer/GL 上下文**(memory `render-architecture-must-be-validated-first`)。
- 绘制层的 VTK 全在 .cpp;头只有 Qt 信号 + POD 枚举。

### 3. 断面工作台顶栏:选路径 + 新建组 + 方法工具栏 + 进入/退出编辑(app/XQMainWindow)
全部加在 `buildCrossSectionWorkbench()`(app 层能直接拿 scene/nodeLister)。建议顶栏从上到下/从左到右布局:**[选路径下拉][新建轮廓组按钮] / [沿路径滑条(已有)] / [圆|多边形 方法按钮][进入/退出编辑]**。
- **选路径下拉**:`makeNodeCombo(topBar, "xqContourPathCombo", nodeLister, XQDomainType::Path)`(nodeLister 从 XQMainWindow 现有场景枚举拿;若成员里已有 `nodeLister` 用它,否则照 modeling 页传入的 `context.nodeLister` 同源构造)。
- **新建轮廓组按钮**(objectName `xqContourGroupCreateBtn`):点击 → 取下拉选中 path id → 见 §4 建组绑路径流程 → 设为 `activeContourGroup_` → 触发 `bindCrossSectionPath()`。
- **进入/退出编辑**按钮(QToolButton checkable,objectName `xqContourEditToggle`):checked=进入绘制(默认方法 Circle),unchecked=退出(`crossSectionView_->setDrawMethod(None)`)。
- **方法工具栏**:圆 / 多边形两个互斥按钮(QToolButton checkable,QButtonGroup;objectName `xqContourMethodCircle` / `xqContourMethodPolygon`)。选中 → `crossSectionView_->setDrawMethod(Circle/Polygon)`(且自动 check 进入编辑态)。
- 未绑路径 / 无 `activeContourGroup_` 时,方法工具栏 + 编辑按钮禁用;空态引导已有(P3-1 的 emptyState)。

**入组接线**(app,连 `contourDrawn` 信号):
```
连 crossSectionView_->contourDrawn:
  1. 取当前 arc length = sectionPathSamples_[currentSliderIndex].arcLength。
  2. 从 crossSectionView_->lastFrame() 拷成 core ContourFrame(origin/normal/xAxis/yAxis)。
  3. 若 method==Circle: pts2d = ContourExtractionService::circle(ctrl[0], ctrl[1]);
     若 method==Polygon: pts2d = ContourExtractionService::polygon(ctrl);
  4. world points = 对每个 (u,v): XQContourGroup::unprojectFromFrame(frame,u,v)。
  5. 组 XQContour{ contourId=新id, pathArcLength=arc, frame, type=(Circle|Manual),
     points=world, closed=true }。
  6. 取当前轮廓组的 XQContourGroupPayload,group().addContour(contour),
     setPayload 回节点 + refreshSceneTree()(参照 :2586-2631 范式)。
  7. crossSectionView_ 叠加显示新轮廓(复用 XQRenderScene addContourGroup / 断面视图内画环)。
```
> **当前轮廓组**从哪来:本批需要有「当前活动轮廓组」概念。若无现成机制,用最简:进入轮廓提取阶段时,从选中节点或轮廓组下拉取当前轮廓组 id 存成成员 `activeContourGroup_`。worker 报告说明采用的机制。

### 4. 建组绑路径流程 + 修 bindCrossSectionPath 硬编码(app/XQMainWindow)
- **新建轮廓组**(§3 的 `xqContourGroupCreateBtn` 点击):从 `xqContourPathCombo` 取选中 path id(`idFromCombo`)→ 建 `XQContourGroup`,`setId(新id)`,`setSourcePathNode(pathId)` → 挂成新场景节点(domainType ContourGroup,payload `XQContourGroupPayload`;节点名留空默认用 path 节点名)。挂节点用 XQMainWindow 现有的加节点范式(参照 :2586-2631 的 setPayload + refreshSceneTree,或现有 create 命令族)+ `refreshSceneTree()`。记 `activeContourGroup_ = 新组 nodeId`。然后调 `bindCrossSectionPath()`。
- **`bindCrossSectionPath()` 改为**(替换现在硬编码取第一条 path):若 `activeContourGroup_` 有效 → 取该组 payload 的 `sourcePathNode()` → 在场景找该 Path 节点(`XQPathPayload`)→ 取 `path().samplePoints()` 存 `sectionPathSamples_`。无活动组 / 组未绑路径 / 找不到 path → 清空 + 空态引导。**保留原有滑条 range/label 更新逻辑不变。**
- 保持 `updateCrossSectionAtSample` / 滑条 valueChanged 逻辑不变(它们已对 `sectionPathSamples_` 工作)。
- `activeContourGroup_` 是 `NodeId` 成员(XQMainWindow.h 加),默认 invalid;进入轮廓提取阶段(showStagePage index==1)时,若为 invalid 且场景已有轮廓组,可默认选第一个(可选;至少新建后要指向新组)。

## 测试(离散不变量,headless ctest)

新增两个纯域测试(照 `test_contour_group` 挂法:`add_executable` + `target_link_libraries(... xq_core)` / `xq_services`;bare main + `check(bool,const char*)` 风格,**副作用调用先执行存结果再 assert**,memory `no-sideeffect-in-assert`)。

### test_contour_extraction(链 service,纯域)
- **圆**:circle(center={0,0}, boundary={5,0}) → 点数==36;每点到 center 距离 ≈5(容差 1e-9);首尾闭合(第 0 点 ≈ (5,0));采样均匀(相邻点夹角 ≈2π/36)。**可证伪**:把 radius 算成 dist 的一半 → 距离断言转红。
- **多边形**:polygon({{0,0},{10,0},{10,10},{0,10}}, perEdge=4) → 结果含 4 个原始顶点(顶点 ∈ 轮廓点集,精确相等);点数 == 4*perEdge(闭合,每边 perEdge 点);有序(相邻点距离合理,无跳变)。**可证伪**:把闭合边漏掉(只连 N-1 条边)→ 点数断言转红。

### test_contour_group_path_binding(链 core)
- 建 `XQContourGroup`,`setSourcePathNode(pathId)` → `hasSourcePathNode()`==true 且 `sourcePathNode()`==pathId。
- addContour 三个不同 pathArcLength(乱序加,如 2.0 / 0.5 / 1.0)→ `orderedByPathPosition()` 返回 arcLength 单调递增(0.5,1.0,2.0)。**可证伪**:把 orderedByPathPosition 的比较反号(此测试断言必转红——但那是改被测代码,抽查时做)。
- **端到端放样打通**:合成一组沿路径圆轮廓(用 circle() 生成 2D 点 → 3 个不同 arcLength 的 frame 经 unprojectFromFrame 得 world → 3 个 XQContour 入组)→ `ContourLoftInputBuilder::buildLoftInput(group, {})` 返回 `Status::Ok` 且 `input.rings.size()>=2`、`pointsPerContour>=3`(**几何守恒断言,别只测非空**)。**可证伪**:只入 1 个轮廓 → Status 应是 NotEnoughContours,断言转红。

> 若 `ContourLoftInputBuilder` 已有测试覆盖放样,本测试聚焦「手绘轮廓入组后能喂进放样链」的衔接即可,别重复它的内部不变量。

## 验证命令(worker 必跑,报告贴输出)

1. 全新构建(**本批改 Q_OBJECT 头**——XQCrossSectionViewWidget 加信号/成员 = 改头,**必 rm -rf build_gui**,memory `ninja-stale-moc-gui-crash`):
   ```
   rm -rf "C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui" && cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
   ```
2. ctest(基线 70 + 新增 test_contour_extraction + test_contour_group_path_binding → 期望 72 全绿):
   ```
   cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"
   ```
   **必走 ctest,裸跑 test exe 缺 VTK DLL exit 127**(memory `ctest-environment-overrides-path`)。
3. 假绿抽查:每个新测试选一条命门断言,篡改被测逻辑(circle 半径减半 / polygon 漏闭合边 / orderedByPathPosition 比较反号)→ 走 ctest 该测试真 FAIL(**核 exe 时间戳变**,memory `ninja-target-incremental-fakegreen-trap`)→ 还原 → 复绿。报告写明篡改点 + 前后结果。

## i18n(有新可译串就加,memory `xqtr-translate-lupdate-blind`)

- 面板串(工具栏按钮「圆」「多边形」「编辑」「新建轮廓组」「选择路径」等)走 `xqTr` = `translate("XQStageWidgets", ...)`。主窗口串走 `tr()`(context `xq::XQMainWindow`)。
- 手工编辑 `CODE_ROOT/resources/i18n/xq_zh_CN.ts`(UTF-8 无 BOM + LF),**照现有 message 块格式加**,**绝不跑 lupdate**。改完:
  ```
  lrelease xq_zh_CN.ts -qm xq_zh_CN.qm    # 应报 N finished / 0 unfinished
  ```
- 报告列出新增的每条串 + lrelease 输出。

## 禁止(违反=返工)

- 不做椭圆/样条/断面阈值/水平集/区域生长/批量/多血管/放样实时预览(全超范围,后续批)。
- 头文件不 include VTK:ContourExtractionService.h(纯域,连 Qt 都不引)、XQCrossSectionViewWidget.h(VTK-free,只 Qt 信号 + POD 枚举)。VTK 只在 .cpp。
- **services 层不引 VTK/Qt**(ContourExtractionService 纯 core 依赖)。分层铁律违反=返工。
- 断面绘制预览不得每帧重建 renderer/GL 上下文(pipeline 常驻,只 render)。
- 新源文件注释一律**英文**(MSVC GBK 坑,memory `msvc-gbk-chinese-comment-syntax-error`)。
- 断言不塞副作用调用(memory `no-sideeffect-in-assert`)。
- 不 `git commit`(worker 无提交权;主审复核后提交)。不 `git add -A`。
- 不改现有四视图渲染路径 / 现有放样链内部(只在建模页外围接线调用它)。

## 交付物(全部在 CODE_ROOT = C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ)

- 新增:
  - `src/services/segmentation/ContourExtractionService.{h,cpp}`
  - `tests/services/segmentation/test_contour_extraction.cpp`(或 tests/services/segmentation/ 下,照现有命名)
  - `tests/core/test_contour_group_path_binding.cpp`
- 改动:
  - `src/visualization/XQCrossSectionViewWidget.{h,cpp}`(绘制交互 + contourDrawn 信号 + DrawMethod)
  - `src/app/XQMainWindow.{h,cpp}`(方法工具栏、进入/退出编辑、contourDrawn 入组接线、bindCrossSectionPath 改绑活动组路径、activeContourGroup_ 成员)
  - `src/ui/panels/XQStageWidgets.{h,cpp}`(轮廓提取页选路径下拉 + 新建轮廓组入口)
  - `CMakeLists.txt`(挂 ContourExtractionService 进对应 lib + 两个新测试)
  - `resources/i18n/xq_zh_CN.ts`(新串,若有)
- 报告(必含):
  - stage index 1 对应哪个 build 函数、采用的「当前活动轮廓组」机制。
  - 屏幕像素 → 断面 (u,v) mm 映射的实现方式 + 断面中心反投影 ≈(0,0) 的自检结果。
  - 全新构建输出、ctest 结果(N/N)、假绿抽查前后结果、lrelease 输出、`git diff --stat`、遗留问题。
