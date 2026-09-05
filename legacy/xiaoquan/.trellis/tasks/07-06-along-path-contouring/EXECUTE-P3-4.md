# EXECUTE P3-4:断面区域生长自动分割 + 手动点种子

> 语境锚点:XQ 医学影像软件血管几何重建。本批打通「沿路径滑条定位断面 → 点『区域生长』按钮 → 从种子像素按局部上下界做 8-连通生长 → 复用 P3-3 等值线追踪把连通区边界描成闭合轮廓 → 入组 → 手动放样出血管」,并加「断面上手动点种子」。区域生长的纳入判据锚在**局部种子值**(不是全局阈值),对亮暗不均影像比单一阈值鲁棒——这是治本应对「阈值法在亮暗不均影像上难控」。纯几何建模 + 影像可视化工程,对标 SimVascular sv4gui 分割工作流(3D 版 ConnectedThreshold 用 [lower,upper] 上下界 + 种子连通)。**本批只做区域生长一种自动方法 + 带宽/容差参数控件 + 手动点种子;不做水平集/ML/批量/多血管**(水平集留 P3-5)。

## 活动任务

- 任务:`07-06-along-path-contouring`(in_progress)。本批 = P3-4(六批第四批,自动分割第二种方法 + 手动点种子)。
- **两个不同 git 工作树,绝不混**:
  - **代码工作树(唯一改代码处)= `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`**,分支 `feat/render-arch`,HEAD=`f813f6c`。本简报所有 `CODE_ROOT/...` 都指这里。src/tests/CMakeLists/resources 编辑、build、ctest 全在这个工作树,**用绝对路径**。
  - 任务文档/spec/SV 参照在主仓 `C:\Users\OCEAN\Desktop\XIAOQUAN`(= 你的 cwd,读 prd/design/implement/spec 用相对路径)。
- **凡本简报写 `CODE_ROOT`,替换为 `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`**。**绝不在主仓 XIAOQUAN 下建/改任何 src 代码文件。**

## 目标(本批范围,严格不越界)

1. `ContourExtractionService`(services/segmentation,**纯域无 VTK/Qt**)新增一个静态方法:`regionGrowContour`(断面灰度 + 种子 + 上下界容差 → 有序闭合 2D 轮廓点。8-连通 flood-fill 得连通 mask → **复用 P3-3 已有的 marching squares + 选种子环基建**在 mask 上提边界)。
2. `XQCrossSectionViewWidget`(visualization)扩展 draw 交互加**手动点种子**:`DrawMethod` 加 `SeedPick`,press 时经 `displayToSection` 得 (u,v),emit 新信号 `seedPicked(u,v)`(复用空 style 接管 + eventFilter,**不抢事件时序**,memory `xq-seed-pick-eventfilter-vtk-race`)。
3. 断面工作台顶栏加**「区域生长」自动方法按钮(与「阈值」并列常用位)+ 带宽/容差滑条(实时预览,照 P3-3 阈值滑条范式)**;加**「点种子」按钮**(进 SeedPick 模式)。
4. 区域生长入组**完全复用 P3-3 已抽的 `addContourFromSection2D`**;种子从「app 存的当前种子 `sectionSeed_`」取(手动点过则用点的,否则退化断面中心 (0,0))。**阈值方法也改成用 `sectionSeed_`**(顺带让手动种子对阈值也生效,一处改两处受益;P3-3 阈值原本硬编码 (0,0) 种子)。

**本批不做**:水平集/GAC、ML 自动方法、批量/多血管、放样实时预览。区域生长的判据用「种子值 ± 容差」上下界(SV 3D ConnectedThreshold 同款语义),不做多尺度/自适应直方图(留后续)。

## 现状(已由主审亲读源码,worker 照此,别重扫;别派 agent 读 SV,memory `sv-research-agent-cyber-falsepositive`)

### P3-3 已交付、本批直接复用的基建(HEAD f813f6c,主审已亲读)

- **`CODE_ROOT/src/services/segmentation/ContourExtractionService.{h,cpp}`**:
  - 已有 `struct ContourPoint2D{ double u; double v; }`(**复用,别另定义**)。
  - 已有 `circle`/`polygon`/`estimateThreshold`/`thresholdContour` 四个静态方法。
  - **.cpp 匿名命名空间里的 helper 全部可复用**(区域生长提边界的关键):`pixelToSection(x,y,W,H,px)`(像素→(u,v) mm,中心对齐)、`EdgeKey`/`IsoSegment`、`crossingT`、`edgePoint`、`signedArea`、`centroidOf`、`pointInLoop`(射线交叉 point-in-polygon)。
  - **`thresholdContour` 的两大段(marching squares 提环 + 选种子环)就是区域生长要复用的**:区域生长 = 先 flood-fill 得二值 mask(值 1/0),再在 mask 上跑同一套 marching squares(阈值取 0.5)提边界环,再用同一套 `pointInLoop`/`signedArea` 选含种子的最小面积环。**强烈建议把 `thresholdContour` 里「从一个 gray 缓冲 + 一个 threshold 生成闭合环并选种子环」的整段抽成 .cpp 匿名命名空间内部 helper**,例如:
    ```cpp
    // (anonymous namespace, .cpp) Traces closed iso-loops of `field` at `iso`
    // via marching squares, then returns the loop enclosing `seed` (smallest
    // enclosing area) or, if none, the loop nearest the seed; empty if none.
    std::vector<ContourPoint2D> traceSeededIsoLoop(
        const std::vector<double>& field, int width, int height,
        double pixelSizeMm, double iso, const ContourPoint2D& seed);
    ```
    `thresholdContour` 改成 `return traceSeededIsoLoop(gray, W, H, px, threshold, seed);`(**行为逐字不变**,现有 4 条阈值离散断言必须仍全绿),`regionGrowContour` flood-fill 出 0/1 mask 后 `return traceSeededIsoLoop(mask, W, H, px, 0.5, seed);`。**若抽,thresholdContour 语义必须一字不差**(worker 报告贴抽取前后 test_contour_extraction 阈值 4 条断言均绿证明)。不抽也行(复制那两段),但**绝不允许两份实现语义漂移**。

- **`CODE_ROOT/src/visualization/XQCrossSectionViewWidget.{h,cpp}`**:
  - `enum class DrawMethod { None, Circle, Polygon };`(头 :26)——**本批加 `SeedPick`**。POD 枚举,VTK-free,随 signal 走。
  - `setDrawMethod(DrawMethod)`(:358):设 drawMethod_、清 draft、`engageDrawStyle(method != None)`(**空 style 接管交互器**,memory `xq-seed-pick-eventfilter-vtk-race` 要求的正是这个)、切光标。**SeedPick 也走它**(`engageDrawStyle(true)` 让 eventFilter 抢到 press)。
  - `bool displayToSection(const QPoint&, double* u, double* v) const`(:451):Qt 鼠标位置 → 断面 (u,v) mm。**点种子复用它**。
  - `eventFilter`(:559):draw 模式下截鼠标事件。press 分支(:571-)现按 Circle/Polygon 放控制点。**加 SeedPick 分支**:press 时 `displayToSection` 得 (u,v) → `emit seedPicked(u, v)` → **不进 draft、不 finishContour**(点种子是一次性,不画环)。
  - `void contourDrawn(DrawMethod, const QVector<QPointF>&)` signal(:119):画完轮廓发。**加新 signal `void seedPicked(double u, double v);`**。
  - `sectionPixels(gray, W, H, pixelSizeMm)`(:399):读断面灰度(已就位,区域生长照调,别改)。

- **`CODE_ROOT/src/app/XQMainWindow.{h,cpp}`**(P3-3 接线范式,区域生长镜像照抄):
  - `addContourFromSection2D(pts2d, ContourType)`(.cpp :3431):**公共入组尾,已抽好**——lastFrame() → ContourFrame → 当前 arcLength → unprojectFromFrame 每点 → XQContour → addContour → refreshSceneTree + refreshSectionContourOverlay。**区域生长入组直接调它,type=`ContourType::LevelSetResult`**(枚举已存在;本批区域生长复用 LevelSetResult 标类型,与阈值 ThresholdResult 区分)。
  - `previewThresholdContour(pts2d, usedThreshold)`(.cpp :3485):抽的公共助手——读断面像素、滑条(0..1000)线性映射断面**真实 [min,max] 强度范围**、种子**当前硬编码 `{0.0, 0.0}`(:3531)**、调 thresholdContour。**本批把种子改成读 `sectionSeed_`**。
  - `updateThresholdPreview()`(.cpp :3537):拖滑条槽,实时重描画到 `setPreviewContour`(**独立橙色预览层**),标签显真实强度。**区域生长的 `updateRegionGrowPreview` 照这个镜像做。**
  - `runThresholdOnCurrentSection()`(.cpp :3575):点阈值按钮入组(previewThresholdContour → addContourFromSection2D(ThresholdResult) → 清预览)。**区域生长的 `runRegionGrowOnCurrentSection` 照这个镜像做。**
  - `buildCrossSectionWorkbench()`(.cpp :2918)的 `toolRow`(:2992):现有 `[Method:][Threshold][阈值滑条+标签]  (spacer)  [Circle][Polygon][Draw]`。阈值按钮 = 非 checkable QToolButton,点击执行(不进绘制态);滑条 `valueChanged → updateThresholdPreview()`。**区域生长按钮 + 带宽滑条并列插在阈值之后、手绘之前**(自动方法都在前)。**「点种子」按钮**是 checkable(进 SeedPick 模式,类似 Draw 但独立)。
  - 成员声明:XQMainWindow.h :428-430 有 `contourMethodThresholdBtn_ / thresholdSlider_ / thresholdLabel_`。**区域生长成员挨着加**。
  - `updateContourWorkbenchState()`(.cpp,启用/禁用逻辑):阈值控件按「有绑路径 + 有活动组」启用。**区域生长控件 + 点种子按钮同样纳入。**
  - `updateCrossSectionAtSample(index)`(.cpp :3200):换断面时 `setPreviewContour(空)` 清预览。**换断面也应重置 `sectionSeed_` 回 (0,0)**(种子是断面局部的,滑走作废;否则上个断面点的种子用到新断面上错位)。

### SV 区域生长参照(主审已亲读,worker 别再读 SV)

- SV 3D 区域生长 = ITK `ConnectedThreshold`,判据是 **[lowerThreshold, upperThreshold] 上下界 + 种子连通**(`Modules/Segmentation/sv4gui_Seg3DUtils.cxx` `ThresholdImage` :67 `thresh->ThresholdOutside(lower, upper)`;3D CollidingFronts/ConnectedThreshold 同族)。**关键语义:纳入判据是上下界区间 + 种子连通,不是全局单阈值——这就是对亮暗不均鲁棒的原因(判据锚在种子局部值)。**
- SV 2D 断面无独立区域生长函数(2D 靠阈值等值线 `GetThresholdContour` = vtkContourFilter + `ClosestPointRegion` 种子最近连通,P3-3 已纯域复现)。**XQ 2D 区域生长自实现**:种子像素值 `seedVal` → 上下界 `[seedVal - lowerTol, seedVal + upperTol]` → 8-连通 flood-fill → 得连通 mask → 复用 P3-3 marching squares 提 mask 边界 → 选含种子环。第一版 `lowerTol == upperTol == band`(对称带宽,一个滑条控)即可,语义等价 SV 的上下界。

## 实现(接口已定死,worker 不自由发挥)

### 1. ContourExtractionService 加 regionGrowContour(services/segmentation,纯域无 VTK/Qt)

在 `CODE_ROOT/src/services/segmentation/ContourExtractionService.{h,cpp}` 追加。**头/实现注释一律英文**(MSVC GBK 坑,memory `msvc-gbk-chinese-comment-syntax-error`)。复用已有 `ContourPoint2D` + .cpp 匿名命名空间 helper。

头新增(接口签名定死):
```cpp
    // Extracts one closed 2D contour of the vessel lumen from a section by region
    // growing from the seed and tracing the grown region's boundary. The seed's
    // pixel value defines an inclusion band [seedValue - band, seedValue + band];
    // an 8-connected flood fill from the seed pixel keeps pixels whose value falls
    // in that band, yielding one connected mask (SV 3D ConnectedThreshold uses the
    // same lower/upper-bound + seed-connectivity semantics; anchoring the band on
    // the local seed value is what makes this robust to non-uniform brightness
    // where a single global threshold fails). The connected region's boundary is
    // then traced as a closed loop (reusing the marching-squares iso-tracer at
    // iso = 0.5 on the 0/1 mask) and the loop enclosing the seed is returned.
    // `gray` is a row-major W*H buffer (idx = y*W + x). `pixelSizeMm` maps pixels
    // to section-local mm; returned points are section-local (u, v) mm with the
    // section CENTER at (0, 0), same convention as thresholdContour. `seed` is the
    // section-local (u, v) mm seed (section center (0, 0) is a good default).
    // `band` is the inclusion half-width in intensity units (>= 0). Returns an
    // ordered, closed loop (last point != first); empty when the seed pixel is out
    // of range, the region is empty, or no closed boundary loop is found (caller
    // rejects -> no contour added).
    static std::vector<ContourPoint2D> regionGrowContour(const std::vector<double>& gray,
                                                         int width, int height,
                                                         double pixelSizeMm,
                                                         const ContourPoint2D& seed,
                                                         double band);
```

实现要点(.cpp,纯 C++):
1. 入参校验:`width<=1 || height<=1 || gray.size() != (size_t)width*height` → 返回空。`band < 0` → clamp 到 0。
2. **种子像素定位**:把种子 (u,v) mm 反算像素 `(sx, sy)`——`pixelToSection` 的逆:`sx = round(seed.u / pixelSizeMm + (W-1)/2.0)`,`sy = round(seed.v / pixelSizeMm + (H-1)/2.0)`。clamp 到 [0,W-1]×[0,H-1]。取 `seedValue = gray[sy*W + sx]`。
3. **8-连通 flood-fill**:`std::vector<unsigned char> mask(W*H, 0)`;BFS/DFS 从 (sx,sy),纳入条件 `std::abs(gray[idx] - seedValue) <= band`,8 邻域扩散,访问过标记。得 0/1 mask(纳入=1)。
4. **提边界 + 选种子环**:把 mask 转 `std::vector<double>`(1.0/0.0),复用 §现状建议抽的 `traceSeededIsoLoop(maskD, W, H, px, 0.5, seed)`(或直接内联 P3-3 那两段 marching squares + 选环)。iso=0.5 恰好在 0 与 1 之间,提出 mask 区域边界。选含 seed 的最小面积环。
5. 空 mask / 无闭合环 → 返回空。
- **算法归属**:全在 service .cpp,纯 C++,不引 VTK/Qt/core。flood-fill + marching squares + point-in-polygon 都是初等算法,别引第三方。**别用 vtkImageThreshold / vtk 连通滤镜**(分层铁律违反=返工)。

### 2. XQCrossSectionViewWidget 加手动点种子(visualization,VTK-free 头)

在 `CODE_ROOT/src/visualization/XQCrossSectionViewWidget.{h,cpp}`:
- 头 `enum class DrawMethod` 加 `SeedPick`(放 None 之后、Circle 之前或末尾均可,不影响现有值语义;**别改 None/Circle/Polygon 相对顺序以免动到依赖枚举底值的地方**——最稳:加在末尾 `{ None, Circle, Polygon, SeedPick }`)。
- 头 signals 区加:
  ```cpp
      // Emitted when the user clicks a seed point on the section while in the
      // SeedPick draw method. (u, v) is the section-local mm point clicked. The
      // app stores it as the current segmentation seed for threshold / region
      // grow; unlike contourDrawn this places no contour.
      void seedPicked(double u, double v);
  ```
- `.cpp` `eventFilter` 的 mouse-press 分支加 `SeedPick`:
  ```cpp
      // (in the ButtonPress handling, alongside Circle/Polygon)
      if (drawMethod_ == DrawMethod::SeedPick) {
          double u = 0.0, v = 0.0;
          if (displayToSection(mouse->pos(), &u, &v)) {
              emit seedPicked(u, v);
          }
          return true;   // consumed; do not fall through to drawing / view interaction
      }
  ```
  **SeedPick 不进 draftPoints_、不调 finishContour、不发 contourDrawn。** `setDrawMethod(SeedPick)` 现有实现已 `engageDrawStyle(true)` 接管交互器 + 切 CrossCursor,SeedPick 无需特判(照 Circle/Polygon 同路径)。**失败(displayToSection 返回 false)也 return true 消费掉**(memory `xq-seed-pick-eventfilter-vtk-race`:拾取模式显式接管,别静默漏给 VTK 交互器)。

### 3. 断面工作台顶栏:区域生长按钮 + 带宽滑条 + 点种子按钮(app/XQMainWindow)

在 `buildCrossSectionWorkbench()` 的 `toolRow`(.cpp :2992)加控件,**自动方法都排在手绘之前**。最终布局:
```
[Method:] [Threshold][阈值滑条+标签]  [Region grow][带宽滑条+标签]  [Seed]  (spacer)  [Circle][Polygon][Draw]
```
- 新成员(XQMainWindow.h,挨着 :428-430 阈值成员):
  ```cpp
      // Auto region-grow segmentation (P3-4): "Region grow" method sits after
      // Threshold in the automatic-first method row and runs on click. The band
      // slider controls the seed-value inclusion half-width; its label previews
      // the current band. The Seed button toggles the section into seed-pick mode.
      QToolButton* contourMethodRegionGrowBtn_ = nullptr;   // objectName xqContourMethodRegionGrow
      QSlider* regionGrowBandSlider_ = nullptr;             // objectName xqRegionGrowBandSlider
      QLabel* regionGrowBandLabel_ = nullptr;
      QToolButton* contourSeedPickBtn_ = nullptr;           // objectName xqContourSeedPick, checkable
      // Current segmentation seed in section-local (u, v) mm (section center (0,0)
      // by default; set by clicking in seed-pick mode). Used by both threshold and
      // region-grow. Reset to (0,0) when the section plane changes.
      ContourPoint2D sectionSeed_{0.0, 0.0};
  ```
  (若 XQMainWindow.h 未 include ContourExtractionService.h 拿 `ContourPoint2D`,已有的阈值路径用到它,应已可见;确认 include 就位。)
- 控件构建照 threshold 范式(:2997-3011):`contourMethodRegionGrowBtn_` = 非 checkable QToolButton,文案 `translate("XQStageWidgets", "Region grow")`;`regionGrowBandSlider_` 范围 `[0,1000]`(映射断面值域,见下);`regionGrowBandLabel_` 显 `translate("XQStageWidgets","Band:")`。`contourSeedPickBtn_` = **checkable** QToolButton,文案 `translate("XQStageWidgets","Seed")`。
- 布局插入:在 `toolRow->addWidget(thresholdSlider_, 1);`(:3043)之后、`toolRow->addSpacing(16);`(:3044)之前,插区域生长三件 + Seed 按钮。
- 连接(照 :3109-3112 threshold 范式):
  ```cpp
      QObject::connect(contourMethodRegionGrowBtn_, &QToolButton::clicked, this,
                       [this]() { runRegionGrowOnCurrentSection(); });
      QObject::connect(regionGrowBandSlider_, &QSlider::valueChanged, this,
                       [this](int) { updateRegionGrowPreview(); });
      // Seed pick: toggling on puts the section view into SeedPick draw mode; off
      // returns to view-only. Mutually exclusive with the manual draw toggle is
      // NOT required (SeedPick is its own mode); but turning it on should clear the
      // manual method buttons' checked state for a clean single-active affordance.
      QObject::connect(contourSeedPickBtn_, &QToolButton::toggled, this, [this](bool on) {
          if (crossSectionView_ == nullptr) return;
          crossSectionView_->setDrawMethod(on ? DrawMethod::SeedPick : DrawMethod::None);
          if (on && contourEditToggle_ != nullptr) contourEditToggle_->setChecked(false);
      });
  ```
- **接种子信号**(照 :3115 contourDrawn 范式):
  ```cpp
      QObject::connect(crossSectionView_, &XQCrossSectionViewWidget::seedPicked, this,
                       [this](double u, double v) { onSeedPicked(u, v); });
  ```
- **状态**:`updateContourWorkbenchState()` 里区域生长按钮 + 带宽滑条 + Seed 按钮的启用逻辑照阈值控件(有绑路径 + 有活动组才启用)。

### 4. app 新方法(XQMainWindow,镜像 threshold 范式)

声明 XQMainWindow.h(挨着 threshold 那组 :331-347);实现 .cpp(挨着 :3575 那组)。

```cpp
    // Auto region-grow segmentation on the current section: reads the section
    // grayscale, region-grows from the current seed with the band slider's
    // half-width, traces the region boundary via
    // ContourExtractionService::regionGrowContour, and adds it as a LevelSetResult
    // contour. A no-op (status hint) with no section / no group / no closed loop.
    void runRegionGrowOnCurrentSection();
    // Computes (but does not add) the region-grow contour for the current section
    // at the current band. Fills `pts2d` (section-local 2D loop) and reports the
    // band actually used. Returns false with no resliced section. Shared by the
    // live preview and the commit path so both trace identically.
    bool previewRegionGrowContour(std::vector<ContourPoint2D>& pts2d, double& usedBand);
    // Slider-drag slot: re-traces the region-grow contour live and shows it as a
    // preview overlay (not added until the Region grow button is clicked), and
    // updates the band label. Lets the user scrub the band and watch the region
    // grow/shrink.
    void updateRegionGrowPreview();
    // Stores a clicked seed point (section-local (u,v) mm) as the current
    // segmentation seed and refreshes both auto-method previews so the user sees
    // the contour re-trace from the new seed. Leaves seed-pick mode.
    void onSeedPicked(double u, double v);
```

实现要点:
- **`previewRegionGrowContour`**(镜像 `previewThresholdContour` :3485):
  ```
  1. crossSectionView_ null → false。
  2. sectionPixels(gray, W, H, px) 失败 → false。
  3. band: regionGrowBandSlider_ 值(0..1000)映射断面值域跨度的一部分。
     求 gray 的 [min,max];band = (slider/1000) * (max - min)。
     (band=0 → 只种子同值像素;band=满 → 整个值域,退化成吞全断面——正是可证伪命门。)
     usedBand = band。
  4. pts2d = ContourExtractionService::regionGrowContour(gray, W, H, px, sectionSeed_, band);
  5. return true。
  ```
- **`updateRegionGrowPreview`**(镜像 `updateThresholdPreview` :3537):算 pts2d + band;`regionGrowBandLabel_` 显 `translate("XQStageWidgets","Band: %1").arg(band,0,'f',0)`(无断面时显 `"Band:"`);pts2d≥3 时 `setPreviewContour(单环)` 否则清空。**用同一个 `setPreviewContour` 橙色预览层**(与阈值预览共用;两个自动方法不会同时预览,后触发的覆盖前一个,符合直觉)。
- **`runRegionGrowOnCurrentSection`**(镜像 `runThresholdOnCurrentSection` :3575):有效性检查 → `previewRegionGrowContour` → pts2d<3 时状态栏提示 `tr("No connected region was grown from this seed.")` → `addContourFromSection2D(pts2d, ContourType::LevelSetResult)` → `setPreviewContour(空)`。
- **`onSeedPicked`**:`sectionSeed_ = {u, v};` → 关 Seed 按钮 checked(`contourSeedPickBtn_->setChecked(false)`,其 toggled 会把 draw mode 切回 None)→ **同时刷新两个自动预览**(`updateThresholdPreview(); updateRegionGrowPreview();`)让用户立刻看到从新种子重描 → 可选状态栏提示 `tr("Seed set at (%1, %2) mm.").arg(u,0,'f',1).arg(v,0,'f',1)`。
- **改 `previewThresholdContour` 的种子**(:3531):把硬编码 `const ContourPoint2D seed{0.0, 0.0};` 改成 `const ContourPoint2D seed = sectionSeed_;`(手动种子对阈值也生效,一处改两处受益)。
- **`updateCrossSectionAtSample`**(:3200)换断面时:`sectionSeed_ = {0.0, 0.0};`(种子是断面局部,滑走作废回中心默认)。放在现有 `setPreviewContour(空)`(:3226)附近。

## 测试(离散不变量,headless ctest)

在 `CODE_ROOT/tests/services/segmentation/test_contour_extraction.cpp` **追加**区域生长用例(同一 test exe,已链 xq_services;bare main + `CHECK(cond)` 宏风格,**副作用调用先执行存结果再 CHECK**,memory `no-sideeffect-in-assert`)。现有 `diskSection` / `loopArea` 辅助复用。

**核心命门 = 亮暗不均背景**(专门制造「全局阈值描不准但区域生长能描准」的场景,证伪性最强):

1. **不均背景下区域生长只描血管斑、不吞渐变背景**(命门):
   - 造 64×64 断面:中心一个高信号血管斑(半径 8px,值=1000),背景是**从一侧到另一侧线性渐变**(如 `bg(x,y) = 200 + 400*(x/(W-1))`,即左侧 200 → 右侧 600,**渐变背景峰值 600 高过若用全局阈值想框住血管斑必须设的低阈**,制造「全局阈值两难」:阈值设低吞右侧亮背景、设高漏左侧暗处)。血管斑值 1000 明显高于任何背景。
   - 种子 = 断面中心 {0,0}(落在血管斑内,seedValue≈1000)。band 取一个把血管斑纳入、把渐变背景排除的值(如 band=300 → 纳入 [700,1300],背景 200..600 全部排除)。
   - `regionGrowContour(gray, 64, 64, 0.5, {0,0}, 300)` 返回**非空闭合环**,面积 ≈ 血管斑 `π*(8*0.5)^2 ≈ 50.27 mm²`(±20%),且 `< 0.25 * 断面面积`(不吞渐变背景)。
   - **可证伪**:①把 band 放到极大(如 band=100000 → 纳入全值域)→ 8-连通从种子吞掉整个断面 → 面积暴涨到接近 1024 mm² → 断言必转红。②去掉连通/种子约束(改成「值在 band 内的所有像素」不做 flood-fill 的种子起点)→ 若背景某处也落进 band 会并入 → 面积/形状错 → 转红。worker 报告写明这两个篡改点前后结果。

2. **区域生长 vs 全局阈值对照**(证「比阈值鲁棒」的量化命门,可选但强烈推荐):
   - 同上不均断面,**用 P3-3 的 `thresholdContour` 在同一断面上跑**:任选一个能框住血管斑的全局阈值(如 700),因右侧渐变背景 600 接近、marching squares 在血管斑外无闭合环或环被背景污染 → thresholdContour 结果面积偏差大 / 非血管斑面积。而 `regionGrowContour` 描准(命门 1 已证)。**断言:regionGrow 面积比 threshold 面积更接近真值**(`|regionArea - true| < |thrArea - true|`),或至少 regionGrow 命中真值而 threshold 明显偏。若构造上 threshold 在该断面恰好也能描准,调整渐变斜率/斑值让 threshold 确实失手再断言。**这条是「治本」主张的离散证据。** 若 worker 发现构造不出稳定「阈值失手」场景,报告说明,退回只保命门 1。

3. **种子选连通区**(证种子约束):造两个分离血管斑(中心含种子、角落不含),`regionGrowContour` 从中心种子只生长出中心斑(8-连通到不了角落斑)→ 面积≈单个中心斑,不是两个之和。**可证伪:把 flood-fill 起点从种子像素改成「全图扫第一个 in-band 像素」→ 可能先命中角落斑 → 面积错 → 转红。**

> 区域生长离散测试全在合成灰度数组上跑,**不需要 vtkImageData**(service 纯域)。手动点种子(widget seedPicked 信号 + app onSeedPicked)无独立离散测试(交互 + Qt 信号,靠真机覆盖);**但 `previewThresholdContour` 改用 sectionSeed_ 后,现有 4 条阈值离散断言必须仍全绿**(种子默认 (0,0) 与原硬编码等价)——worker 确认阈值断言不回归。

## 验证命令(worker 必跑,报告贴输出)

1. 全新构建(**本批改 Q_OBJECT 头**——XQCrossSectionViewWidget 加 signal + XQMainWindow 加成员/槽 = 改头,**必 rm -rf build_gui 全新构建**,memory `ninja-stale-moc-gui-crash`):
   ```
   rm -rf "C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui" && cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
   ```
   **当前基线全新构建 248/248。** vcvars64 冷启动偶挂死 → 换 DevShell(memory `vcvars64-coldstart-hangs-use-devshell`;「注入+configure+build」写进同一 .ps1 一次跑完)。
2. ctest(基线 72/72;本批新增断言追加进 **test_contour_extraction**,不新增 test exe,期望仍 **72/72 全绿**):
   ```
   cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"
   ```
   **必走 ctest,裸跑 test exe 缺 VTK DLL exit 127**(memory `ctest-environment-overrides-path`)。
3. 假绿抽查:对**命门 1(不均背景不吞)**和**命门 3(种子选连通区)**,分别篡改被测逻辑(band 放极大 / flood-fill 起点改成全图第一个 in-band)→ 走 ctest 该测试真 FAIL(**核 exe 时间戳变**,memory `ninja-target-incremental-fakegreen-trap`)→ 还原 → 复绿。报告写明篡改点 + 前后结果。**若抽了 `traceSeededIsoLoop`,额外确认 thresholdContour 4 条阈值断言仍绿**(抽取无语义漂移)。

## i18n(有新可译串就加,memory `xqtr-translate-lupdate-blind`)

- 新串:区域生长按钮「Region grow / 区域生长」、带宽标签「Band: / 带宽:」「Band: %1」、Seed 按钮「Seed / 种子」、状态栏「No connected region was grown from this seed. / 未从该种子生长出连通区域。」「Seed set at (%1, %2) mm. / 种子设于 (%1, %2) mm。」
- **重复串警示(memory `xqtr-translate-lupdate-blind`,P3-3 踩过 Duplicate)**:`resources/i18n/xq_zh_CN.ts` 里 **`XQStageWidgets` context 已有 `<source>Region grow</source>`(译「区域生长」,旧 stage panel 的,:534-535)**。断面工作台的「Region grow」按钮**若也走 `translate("XQStageWidgets", "Region grow")`,同 context 同 source 已存在,直接复用现有译文,别再加第二个 message 块**(lrelease 会报 Duplicate)。只有 context+source 组合是**新**的才加 message 块。逐条核对:Band/Seed 系列串在 XQStageWidgets 应为新;「Region grow」复用旧;状态栏串走 `tr()`(context `xq::XQMainWindow`)应为新。
- 手工编辑 `CODE_ROOT/resources/i18n/xq_zh_CN.ts`(UTF-8 无 BOM + LF),**照现有 message 块格式加**,**绝不跑 lupdate**。改完:
  ```
  "C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lrelease.exe" xq_zh_CN.ts -qm xq_zh_CN.qm
  ```
  **当前基线 261 finished / 0 unfinished。** 新增 N 条应 `261+N finished / 0 unfinished`。报告列出新增每条串(标明哪条是复用旧的没新增)+ lrelease 输出。

## 禁止(违反=返工)

- 不做水平集/GAC/ML/批量/多血管/放样实时预览(全超范围,后续批)。区域生长判据只做「种子值 ± band 对称带宽」,不做自适应/多尺度。
- **services 层不引 VTK/Qt**:ContourExtractionService.{h,cpp} 纯 core 依赖(只用 std + ContourPoint2D)。flood-fill + marching squares + point-in-polygon 必须**纯 C++ 自实现在 service .cpp**,不引 vtkImageThreshold/vtk 连通滤镜。分层铁律违反=返工。
- 头文件不 include VTK:ContourExtractionService.h、XQCrossSectionViewWidget.h(VTK-free,`seedPicked` 用 double 标量参数)。VTK 只在 .cpp。
- **不改 core**(ContourType 已有 LevelSetResult;不动 XQContourGroup.h)。
- 若抽 `traceSeededIsoLoop`,`thresholdContour` 行为(阈值 4 条离散断言)必须逐字不变。**绝不允许 threshold 与 regionGrow 两份提环/选环实现语义漂移。**
- 手动点种子交互**空 style 显式接管**(setDrawMethod(SeedPick) 已走 engageDrawStyle(true)),eventFilter SeedPick 分支**失败也 return true 消费**,别静默漏给 VTK 交互器(memory `xq-seed-pick-eventfilter-vtk-race`)。
- 断面像素读取只读,不改断面渲染管线 / 不重建 GL 上下文。
- 新源代码注释一律**英文**(MSVC GBK 坑,memory `msvc-gbk-chinese-comment-syntax-error`)。
- 断言不塞副作用调用(memory `no-sideeffect-in-assert`)。
- i18n 同 context 同 source **别重复加**(memory `xqtr-translate-lupdate-blind`;「Region grow」已存在于 XQStageWidgets)。
- 不 `git commit`(worker 无提交权;主审复核后提交)。不 `git add -A`。

## 交付物(全部在 CODE_ROOT = C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ)

- 改动:
  - `src/services/segmentation/ContourExtractionService.{h,cpp}`(加 `regionGrowContour`,flood-fill + 复用 marching squares 选种子环;建议抽 `traceSeededIsoLoop`)
  - `src/visualization/XQCrossSectionViewWidget.{h,cpp}`(DrawMethod 加 SeedPick + seedPicked signal + eventFilter SeedPick 分支)
  - `src/app/XQMainWindow.{h,cpp}`(区域生长按钮 + 带宽滑条 + Seed 按钮 + `runRegionGrowOnCurrentSection`/`previewRegionGrowContour`/`updateRegionGrowPreview`/`onSeedPicked` + `sectionSeed_` 成员 + previewThresholdContour 改用 sectionSeed_ + updateCrossSectionAtSample 重置种子 + 顶栏布局 + 状态启用)
  - `tests/services/segmentation/test_contour_extraction.cpp`(追加区域生长不均背景/对照/种子连通用例)
  - `resources/i18n/xq_zh_CN.ts`(新串,「Region grow」复用旧)
- 报告(必含):
  - `regionGrowContour` 实现摘要:种子像素反算公式、flood-fill 连通性(8-邻域)、band 判据、是否抽了 `traceSeededIsoLoop`(抽则贴 thresholdContour 阈值 4 条断言仍绿证明)。
  - 手动点种子接法:DrawMethod::SeedPick 走 setDrawMethod 空 style 接管、eventFilter SeedPick 分支消费 press、seedPicked→onSeedPicked→sectionSeed_ 链;换断面重置种子。
  - 带宽滑条值 → band 的映射公式;两个自动方法共用橙色预览层的行为。
  - 顶栏「自动优先」最终布局(Threshold / Region grow / Seed / 手绘的视觉次序)。
  - 离散测试:不均背景构造参数、命门 1/2/3 断言、命门 2 是否成立(区域生长比阈值更接近真值;若构造不出阈值失手场景说明退回)。
  - 全新构建输出、ctest 结果(N/N)、假绿抽查前后结果(命门 1 + 命门 3 两条 + 抽取无漂移)、lrelease 输出(标明复用旧串)、`git diff --stat`、遗留问题。
