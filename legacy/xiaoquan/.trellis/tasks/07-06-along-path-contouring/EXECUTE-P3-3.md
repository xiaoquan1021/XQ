# EXECUTE P3-3:断面阈值自动分割(等值线追踪 + 种子约束)

> 语境锚点:XQ 医学影像软件血管几何重建。本批打通「沿路径滑条定位断面 → 点『阈值』按钮 → 断面内按强度阈值自动描出血管腔一圈闭合轮廓(种子约束,不出实心块)→ 入组 → 手动放样出血管」。纯几何建模 + 影像可视化工程,对标 SimVascular sv4gui 分割工作流的 Threshold 方法。**本批只做断面阈值一种自动方法 + 阈值参数控件 + 把自动方法放主区常用位;不做水平集/区域生长/ML/批量/多血管**(P3-4/P3-5)。

## 活动任务

- 任务:`07-06-along-path-contouring`(in_progress)。本批 = P3-3(六批第三批,**自动分割首个落地**)。
- **两个不同 git 工作树,绝不混**:
  - **代码工作树(唯一改代码处)= `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`**,分支 `feat/render-arch`,HEAD=`53f3c1a`。本简报所有 `CODE_ROOT/...` 都指这里。src/tests/CMakeLists/resources 编辑、build、ctest 全在这个工作树,**用绝对路径**。
  - 任务文档/spec/SV 参照在主仓 `C:\Users\OCEAN\Desktop\XIAOQUAN`(= 你的 cwd,读 prd/design/implement/spec 用相对路径)。
- **凡本简报写 `CODE_ROOT`,替换为 `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`**。**绝不在主仓 XIAOQUAN 下建/改任何 src 代码文件。**

## 目标(本批范围,严格不越界)

1. `ContourExtractionService`(services/segmentation,**纯域无 VTK/Qt**)新增两个静态方法:`thresholdContour`(断面灰度 + 阈值 + 种子 → 有序闭合 2D 轮廓点,2D marching squares 等值线追踪 + 种子最近连通选择)、`estimateThreshold`(断面灰度直方图 → 血管高信号带默认阈值)。
2. `XQCrossSectionViewWidget`(visualization)暴露一个 **VTK-free 接口取当前断面灰度像素**(实现读 `outputImageHandle()` 的 vtkImageData,VTK 只在 .cpp),供 app 喂给 service。
3. 断面工作台顶栏加**「阈值」自动方法按钮(放主区常用位,视觉优先于手绘)+ 阈值滑条**;点阈值按钮 → 取当前断面像素 + 阈值 + 断面中心种子 → `thresholdContour` → 入组成 `XQContour{type=ThresholdResult}`。
4. 阈值方法**收敛复用 P3-2 入组后半段**(frame → unprojectFromFrame → XQContour → addContour → refresh),只换「2D 点怎么来」的前半段。

**本批不做**:水平集/区域生长/ML 自动方法、批量/多血管、放样实时预览、手动点种子(第一版种子固定=断面中心 (0,0);后续批再加手动点)。

## 现状(已由主审亲读源码,worker 照此,别重扫)

### 数据结构(core,已就位,本批不改 core)
- `CODE_ROOT/src/core/XQContourGroup.h`:
  - `ContourType` 枚举**已含 `ThresholdResult`**(第 21 行)——**本批不改 core 头,直接用**。也已有 `LevelSetResult`(P3-4 用)。
  - `XQContour{ ContourId contourId; double pathArcLength; ContourFrame frame; ContourType type; std::vector<Point3> points; bool closed; }`。
  - `ContourFrame{ Point3 origin; Vec3 normal; Vec3 xAxis; Vec3 yAxis; }`。
  - `unprojectFromFrame(frame,u,v) = origin + u·xAxis + v·yAxis`(断面 2D → world 唯一通道)。
- `CODE_ROOT/src/services/segmentation/ContourExtractionService.h`:已有 `struct ContourPoint2D{ double u; double v; }`(**复用,别另定义**)+ `circle`/`polygon` 静态方法。阈值方法加进这个类。

### 断面像素来源(命门,已亲读核实)
- 断面重采样输出:`XQCrossSectionResampler::outputImageHandle()` 返回不透明 `void*`,实为 **2D 单通道 `vtkImageData*`**(vtkImageReslice `SetOutputDimensionality(2)` 输出)。`XQCrossSectionViewWidget::setSampleFrame`(:193)已 `static_cast<vtkImageData*>(output)` 用它;widget .cpp 已 `#include <vtkImageData.h>`(:23)。
- 断面尺寸:`resampler.outputSpec()` = `XQCrossSectionOutputSpec{ sizeMm, pixelSizeMm }`,默认 40mm / 0.2mm/px → **200×200 像素**。像素 (px,py) 的断面局部坐标 (u,v) mm 关系(与 P3-1/P3-2 锁定的一致):**断面中心 (0,0) 在图像中心**,`u = (px - (W-1)/2) * pixelSizeMm`,`v = (py - (H-1)/2) * pixelSizeMm`(reslice `SetOutputOrigin(-half,-half,0)` 使图像局部原点在断面左下,中心即 (0,0);worker 用下面的自检锁死这条映射)。
- 取灰度数组:`vtkImageData::GetDimensions(int[3])` 拿 W×H×1;`GetScalarPointer()` 拿标量指针;标量类型 reslice 输出通常是 `double` 或源体的 `short`——**必须用 `GetScalarType()` 判类型**或用 `GetScalarComponentAsDouble(px,py,0,0)` 逐点取(简单稳妥,200×200 无性能问题)。**行主序**:VTK vtkImageData 按 (x 快变, y 慢变) 排布,`idx = py*W + px`。

### 入组接线现状(app,已亲读,阈值复用后半段)
- `CODE_ROOT/src/app/XQMainWindow.cpp` `addDrawnContour(DrawMethod, points)`(:3329):P3-2 入组范本。后半段(:3372-3402)= **取 `crossSectionView_->lastFrame()` 拷 core `ContourFrame` → 取当前 arcLength(`sectionPathSamples_[slider.value()].arcLength`)→ 组 `XQContour`(contourId=`allocateSceneNodeId()`,points=对每个 (u,v) `unprojectFromFrame`)→ `payload->group().addContour(contour)` → `refreshSceneTree()` + `refreshSectionContourOverlay()`**。阈值入组**照抄这后半段**,只是 `pts2d` 来自 `thresholdContour`、`contour.type = ContourType::ThresholdResult`。
- **当前活动轮廓组**:成员 `activeContourGroup_`(NodeId);`scene->find(activeContourGroup_)` → `dynamic_pointer_cast<XQContourGroupPayload>(node->payload())` → `payload->group()`。阈值同样从这里拿组。
- 顶栏方法工具栏 `buildCrossSectionWorkbench()`(:2918):`toolRow`(:2990)现有 `[Method:][Circle][Polygon][Draw]`(objectName `xqContourMethodCircle`/`xqContourMethodPolygon`/`xqContourEditToggle`,QButtonGroup 互斥)。成员声明 XQMainWindow.h:386-390。

### SV 阈值算法参照(主审已亲读,照此语义,worker 别再读 SV)
源:`Modules/Segmentation/sv4gui_SegmentationUtils.cxx` `GetThresholdContour`(:867)/`CreateThresholdContour`(:937);`Plugins/.../sv4gui_Seg2DEdit.cxx`(阈值取 `sliderThreshold->value()`)。SV 是**等值线追踪 + 种子最近连通**(不是二值化填充):
1. `vtkContourFilter` 对断面 2D 图 `SetValue(0, threshold)` → 提该强度的等值线(可能多条,线集)。
2. `vtkPolyDataConnectivityFilter` `SetExtractionModeToClosestPointRegion()` + `SetClosestPoint(seed)` → **从多条等值线里选离种子最近的那条连通分量**。**种子约束 = 不出板砖、选中血管腔那圈的关键。**
3. 排成有序点序列 + 判闭合。阈值默认从断面直方图估计(避免 0/255 无效默认)。
**XQ 选项 A(用户已定):在 services 纯域自实现 2D marching squares 等值线追踪 + 种子最近环选择**,不引 VTK,可 headless 离散测。语义对齐 SV(同样是「等值线 + 种子选环」),实现是纯 C++ 初等算法(规则断面网格上)。

## 实现(接口已定死,worker 不自由发挥)

### 1. ContourExtractionService 加 thresholdContour + estimateThreshold(services/segmentation,纯域无 VTK/Qt)
在 `CODE_ROOT/src/services/segmentation/ContourExtractionService.{h,cpp}` 追加。**头/实现注释一律英文**。复用已有 `ContourPoint2D`。

头新增(接口签名定死):
```cpp
    // Estimates a default threshold from the section's grayscale histogram,
    // biased to the high-signal band where contrast-filled vessel lumen sits
    // (percentile `pct` of the value range, default 0.90 -> P90). `gray` is a
    // row-major W*H sample buffer (idx = y*W + x). Returns the midpoint of the
    // min/max range when the buffer is empty or flat. NOT a hard 0/255 default.
    static double estimateThreshold(const std::vector<double>& gray,
                                    int width, int height, double pct = 0.90);

    // Extracts one closed 2D contour of the vessel lumen from a section by
    // tracing the `threshold` iso-line and keeping the connected iso-loop
    // nearest the seed (SV: vtkContourFilter + ClosestPointRegion). `gray` is a
    // row-major W*H grayscale buffer (idx = y*W + x). `pixelSizeMm` maps pixels
    // to section-local millimetres; the returned points are in section-local
    // (u, v) mm with the section CENTER at (0, 0):
    //   u = (x - (W-1)/2) * pixelSizeMm,  v = (y - (H-1)/2) * pixelSizeMm.
    // `seed` is the section-local (u,v) mm seed (the section center (0,0) is the
    // path lumen center -- a good default seed). Marching-squares tracing on the
    // regular grid; among the closed iso-loops, the one whose interior contains
    // the seed is chosen (fall back to the loop nearest the seed). Returns an
    // ordered, closed loop (last point != first). Returns empty when no closed
    // loop encloses/near the seed (caller rejects -> no contour added).
    static std::vector<ContourPoint2D> thresholdContour(const std::vector<double>& gray,
                                                        int width, int height,
                                                        double pixelSizeMm,
                                                        double threshold,
                                                        const ContourPoint2D& seed);
```

实现要点(.cpp,纯 C++):
- **estimateThreshold**:扫 gray 求 min/max;若 `size()==0` 或 `max<=min` 返回 `(min+max)/2`;否则取值域百分位 `min + pct*(max-min)`(简单线性,不必真直方图分桶——P90 高端锚定血管高信号,照 memory `xq-threshold-seg-fullblock-bug`)。
- **thresholdContour**:
  1. `width<=1 || height<=1 || gray.size() != (size_t)width*height` → 返回空。
  2. **2D marching squares**:对每个 cell(4 邻像素,阈值二分类)按 16 种 case 生成等值线段(线性插值边上交点,得亚像素精度)。交点坐标先算像素坐标再转 mm(用上面注释的中心对齐公式)。
  3. **把线段接成闭合环**:按端点邻接(容差匹配)把线段串成多条 polyline;闭合的成环。
  4. **选种子环**:对每条闭合环,判 seed 是否在环内(射线交叉/绕数 point-in-polygon,纯 2D);选**包含 seed 的环**;若无环含 seed,退化为**离 seed 质心最近**的闭合环。选中环即结果(有序闭合,末点≠首点)。
  5. 无任何闭合环 → 返回空。
- **算法归属**:全在 service .cpp,纯 C++,不引 VTK/Qt/core。marching squares case 表 + 交点插值 + polyline 接环 + point-in-polygon 都是初等几何,别引第三方。

### 2. XQCrossSectionViewWidget 暴露断面灰度像素(visualization,VTK-free 头)
在 `CODE_ROOT/src/visualization/XQCrossSectionViewWidget.{h,cpp}` 加**只读取像素**接口。头保持 VTK-free(POD out 参数)。

头新增:
```cpp
    // Copies the current resliced section's grayscale into `gray` (row-major,
    // idx = y*W + x) and reports its dimensions + pixel size (mm). Returns false
    // (outputs untouched) when there is no resliced section (no volume / no pose
    // yet). Used by the app to run threshold segmentation on the section. Reads
    // the resampler's output vtkImageData; the VTK access lives in the .cpp so
    // the header stays VTK-free.
    bool sectionPixels(std::vector<double>& gray, int& width, int& height,
                       double& pixelSizeMm) const;
```
.cpp 实现:取 `impl_->resampler_.outputImageHandle()`;null → 返回 false;`static_cast<vtkImageData*>`;`GetDimensions(dims)`;`pixelSizeMm = impl_->resampler_.outputSpec().pixelSizeMm`;逐点 `GetScalarComponentAsDouble(x,y,0,0)` 填 `gray[y*W+x]`(200×200 逐点无性能问题,稳过类型判断)。**只读,不改渲染管线。**

### 3. 断面工作台顶栏:阈值方法按钮(主区常用位)+ 阈值滑条(app/XQMainWindow)
在 `buildCrossSectionWorkbench()` 的 `toolRow`(:2990)加自动方法,**视觉上放在手绘按钮之前(自动优先)**:
- 新成员(XQMainWindow.h,挨着现有按钮成员 386-390):
  ```cpp
  QToolButton* contourMethodThresholdBtn_ = nullptr;   // objectName xqContourMethodThreshold
  QSlider* thresholdSlider_ = nullptr;                  // objectName xqThresholdSlider
  QLabel* thresholdLabel_ = nullptr;
  ```
- 布局:toolRow 从 `[Method:]` 开始改为 `[Method:][Threshold][阈值滑条 + 值标签]  |  [Circle][Polygon][Draw]`。阈值按钮 = 主区常用位(第一个方法);手绘三件收在其后(用一个细分隔或 addSpacing 拉开视觉层级即可,不必大改)。阈值按钮**不进 methodGroup 互斥的手绘绘制态**——它是「点一下执行」不是「进绘制模式」。
- 阈值滑条 `thresholdSlider_`:范围先给 `[0,1000]`(映射到断面值域的百分位或直接值;第一版最简:滑条值 = 断面值域百分位 0..100%,`threshold = min + (slider/1000)*(max-min)`,进入断面时用 `estimateThreshold` 反推初值定滑条位置)。滑条 `valueChanged` 只更新阈值预览标签,不自动重算(避免每拖一次跑一遍);**点阈值按钮才执行**。
- 阈值按钮 `clicked` → `runThresholdOnCurrentSection()`(新 app 方法)。
- **状态**:未绑路径 / 无 `activeContourGroup_` / 无断面像素时,阈值按钮 + 滑条禁用(照现有 `updateContourWorkbenchState()` 加这两个控件的启用逻辑)。

### 4. runThresholdOnCurrentSection(app/XQMainWindow,复用入组后半段)
新增 app 方法(声明 XQMainWindow.h 挨着 `addDrawnContour`:314):
```cpp
    void runThresholdOnCurrentSection();
```
逻辑:
```
1. scene / crossSectionView_ / activeContourGroup_ 有效性检查(照 addDrawnContour:3331-3344)。
2. std::vector<double> gray; int W,H; double px;
   if (!crossSectionView_->sectionPixels(gray, W, H, px)) return;   // 无断面
3. double threshold = 从 thresholdSlider_ 当前值映射(见 §3;滑条空时用 estimateThreshold(gray,W,H))。
4. seed = { 0.0, 0.0 };   // 断面中心 = pose.origin = 血管腔中心(第一版固定种子)
5. std::vector<ContourPoint2D> pts2d =
       ContourExtractionService::thresholdContour(gray, W, H, px, threshold, seed);
   if (pts2d.size() < 3) { /* 状态栏提示"该阈值未描出闭合轮廓" */ return; }
6. --- 以下与 addDrawnContour 后半段(:3372-3402)完全相同 ---
   frame = lastFrame() 拷 core ContourFrame;
   arcLength = sectionPathSamples_[slider.value()].arcLength;
   XQContour contour{ contourId=allocateSceneNodeId(), pathArcLength=arc, frame,
                      type=ContourType::ThresholdResult, closed=true,
                      points=每个(u,v)经 unprojectFromFrame };
   payload->group().addContour(contour);
   refreshSceneTree(); refreshSectionContourOverlay();
```
> **抽公共入组尾**(可选,推荐):把 addDrawnContour 的 :3372-3402 抽成 `addContourFromSection2D(const std::vector<ContourPoint2D>& pts2d, ContourType type)`,`addDrawnContour` 和 `runThresholdOnCurrentSection` 都调它。**若抽,addDrawnContour 行为必须逐字不变**(圆→Circle、多边形→Manual)。不抽就照抄,别引入行为差异。worker 报告说明采用哪种。

## 测试(离散不变量,headless ctest)

在 `CODE_ROOT/tests/services/segmentation/test_contour_extraction.cpp` **追加**阈值用例(同一 test exe,已链 xq_services;bare main + `check(bool,const char*)` 风格,**副作用调用先执行存结果再 assert**,memory `no-sideeffect-in-assert`)。

**合成断面**(纯域构造,不碰 VTK):W=H=64,pixelSizeMm=0.5,中心一个半径 r=8px 的高信号圆斑(圆内值=1000,圆外背景=0,可加轻噪声但保持圆内外分离)。种子 seed={0,0}(断面中心)。

新增断言(命门,可证伪):
- **闭合**:`thresholdContour(gray,64,64,0.5, 500, {0,0})` 返回非空且形成闭合环(首末点距离在容差内闭合;点数 ≥ 一个下限如 8)。
- **面积≈圆斑**(核心命门,不吞背景=不出板砖):对返回环用 shoelace 算面积,`area ≈ π*(r*pixelSizeMm)^2`,即 `π*(8*0.5)^2 = π*16 ≈ 50.27 mm²`,容差放宽(marching squares 亚像素 + 阈值位置,±15% 内)。**可证伪:去掉种子约束改成「取所有环并集」或把阈值设为 -1(全断面过阈)→ 面积暴涨到接近整断面 `(64*0.5)^2=1024 mm²` → 断言必转红。**
- **estimateThreshold 高端**:`estimateThreshold(gray,64,64,0.90)` 返回值 > 背景(0)且 ≤ 圆斑值(1000),落在高信号带(> min + 0.5*(max-min),证明取的是高端不是中位)。**可证伪:把 pct 用成 0.10 或把 P90 写成 P10 → 返回值落到低端 → 断言转红。**
- **种子选环**(证种子约束有效):构造**两个**分离圆斑(一个在中心含 seed、一个在角落不含 seed)→ `thresholdContour` 只返回含 seed 的那个环(面积≈单个圆斑,不是两个之和)。**可证伪:把「选含 seed 环」改成「选最大环」或「取第一个环」,当角落斑更大/更靠前时结果错 → 面积断言转红。**

> 阈值离散测试全在合成灰度数组上跑,**不需要 vtkImageData**(service 纯域)。widget 的 `sectionPixels` 无独立离散测试(它只是读 vtkImageData 的薄封装,靠真机 + P3-1 已锁的断面映射覆盖);若 worker 想加,可在 GUI 测试里造断面后 assert `sectionPixels` 尺寸==200×200,但非必需。

## 验证命令(worker 必跑,报告贴输出)

1. 全新构建(**本批改 Q_OBJECT 头**——XQCrossSectionViewWidget 加成员方法 + XQMainWindow 加成员/槽 = 改头,**必 rm -rf build_gui**,memory `ninja-stale-moc-gui-crash`):
   ```
   rm -rf "C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui" && cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
   ```
   **当前基线全新构建 248/248。** vcvars64 冷启动偶挂死 → 换 DevShell(memory `vcvars64-coldstart-hangs-use-devshell`)。
2. ctest(基线 72/72;本批新增断言追加进 **test_contour_extraction**,不新增 test exe,期望仍 **72/72 全绿**;若为阈值另开 test exe 则 73):
   ```
   cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"
   ```
   **必走 ctest,裸跑 test exe 缺 VTK DLL exit 127**(memory `ctest-environment-overrides-path`)。
3. 假绿抽查:对**面积≈圆斑**和**种子选环**两条命门,分别篡改被测逻辑(阈值设 -1 全断面过阈 / 选环改成「选最大环」)→ 走 ctest 该测试真 FAIL(**核 exe 时间戳变**,memory `ninja-target-incremental-fakegreen-trap`)→ 还原 → 复绿。报告写明篡改点 + 前后结果。

## i18n(有新可译串就加,memory `xqtr-translate-lupdate-blind`)

- 新串:阈值按钮「Threshold / 阈值」、阈值标签「Threshold: / 阈值:」等。面板/工作台顶栏串走 `QCoreApplication::translate("XQStageWidgets", ...)`(与现有 toolRow 一致);状态栏提示走 `tr()`(context `xq::XQMainWindow`)。
- 手工编辑 `CODE_ROOT/resources/i18n/xq_zh_CN.ts`(UTF-8 无 BOM + LF),**照现有 message 块格式加**,**绝不跑 lupdate**。改完:
  ```
  lrelease xq_zh_CN.ts -qm xq_zh_CN.qm    # 应报 N finished / 0 unfinished
  ```
  **当前基线 258 finished。** 报告列出新增每条串 + lrelease 输出。

## 禁止(违反=返工)

- 不做水平集/区域生长/ML/批量/多血管/放样实时预览/手动点种子(全超范围,后续批)。第一版种子固定 = 断面中心 (0,0)。
- **services 层不引 VTK/Qt**:ContourExtractionService.{h,cpp} 纯 core 依赖(连 core 都尽量不引,只用 std + ContourPoint2D)。阈值等值线算法(marching squares + 选环 + point-in-polygon)必须**纯 C++ 自实现在 service .cpp**,不引 vtkContourFilter/vtkPolyDataConnectivityFilter。分层铁律违反=返工。
- 头文件不 include VTK:ContourExtractionService.h、XQCrossSectionViewWidget.h(VTK-free,`sectionPixels` 用 std::vector/POD out 参数)。VTK 只在 .cpp。
- **不改 core**(ContourType 已有 ThresholdResult;不动 XQContourGroup.h)。
- 若抽 `addContourFromSection2D` 公共尾,`addDrawnContour` 行为(Circle→Circle、Polygon→Manual)必须逐字不变。
- 断面像素读取只读,不改断面渲染管线 / 不重建 GL 上下文。
- 新源文件注释一律**英文**(MSVC GBK 坑,memory `msvc-gbk-chinese-comment-syntax-error`)。
- 断言不塞副作用调用(memory `no-sideeffect-in-assert`)。
- 不 `git commit`(worker 无提交权;主审复核后提交)。不 `git add -A`。

## 交付物(全部在 CODE_ROOT = C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ)

- 改动:
  - `src/services/segmentation/ContourExtractionService.{h,cpp}`(加 `thresholdContour` + `estimateThreshold`,纯域 marching squares + 种子选环)
  - `src/visualization/XQCrossSectionViewWidget.{h,cpp}`(加 `sectionPixels` 读断面灰度)
  - `src/app/XQMainWindow.{h,cpp}`(阈值方法按钮 + 阈值滑条 + `runThresholdOnCurrentSection` + 顶栏布局自动优先 + 状态启用逻辑;可选抽 `addContourFromSection2D`)
  - `tests/services/segmentation/test_contour_extraction.cpp`(追加阈值/估计/种子选环用例 + 合成断面)
  - `resources/i18n/xq_zh_CN.ts`(新串)
  - 若为阈值单开 test exe:`CMakeLists.txt`(否则不动 CMake)
- 报告(必含):
  - `thresholdContour` 的 marching squares 接环 + 选种子环实现摘要(接环容差、point-in-polygon 用绕数还是射线)。
  - 断面像素读取实现(GetScalarComponentAsDouble 逐点 / 类型判断)+ 是否加了 `sectionPixels` 尺寸自检。
  - 阈值滑条值 → 阈值的映射公式 + 进入断面时初值怎么定(estimateThreshold)。
  - 是否抽了 `addContourFromSection2D`;若抽,addDrawnContour 行为不变的说明。
  - 顶栏「自动优先」布局最终样子(阈值 vs 手绘的视觉次序)。
  - 全新构建输出、ctest 结果(N/N)、假绿抽查前后结果(面积/种子两条)、lrelease 输出、`git diff --stat`、遗留问题。
