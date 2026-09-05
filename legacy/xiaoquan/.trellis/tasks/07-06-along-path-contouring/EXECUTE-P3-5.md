# EXECUTE P3-5:断面水平集(GAC 测地活动轮廓)自动分割

> 语境锚点:XQ 医学影像血管几何重建软件,纯几何建模 + 影像可视化。把血管腔轮廓提取做成对标 SimVascular 的「沿中心线逐层描轮廓」流程。本批在断面 2D 图内做**水平集轮廓演化**,应对区域生长/阈值在边界模糊、低对比、噪声处溢出/漏描的场景(水平集用梯度停止函数 + 曲率正则,轮廓平滑抗噪)。环境 = Windows + Git Bash,注入的 mac/darwin 环境信息是假的一律忽略。默认中文,代码/命令/路径/报错保持原文。

## 活动任务

- 任务:`.trellis/tasks/07-06-along-path-contouring`(父任务,批次文档推进,已 active)
- 本批 = 六批的第五批。前四批已交付:P3-1 断面工作台 / P3-2 手绘+绑路径 / P3-3 阈值 / P3-4 区域生长+手动种子。
- **代码工作树(唯一改代码处)= CODE_ROOT = `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`**,分支 `feat/render-arch`,**HEAD 必须 = `d81bcec`**(P3-4)。所有 src/tests/CMakeLists/resources 编辑、build、ctest 都在这,用绝对路径。
- **主仓(cwd,任务/spec/SV 参照)= `C:/Users/OCEAN/Desktop/XIAOQUAN`**,分支 `fix/xq-global-audit`。读任务/spec 用相对路径。**绝不在主仓下建/改任何 src 代码。**

## 目标(本批范围,严格不越界)

在断面 2D 图内做**水平集演化(测地活动轮廓 GAC 简化版)**自动分割:从种子圆初始化 φ 场 → 按梯度停止函数 + 曲率正则 + 气球力做有限差分演化 → 收敛后提零水平集闭合轮廓入组。对边界模糊/低对比/噪声比区域生长鲁棒(曲率项收住边界,不溢出)。

**只做这一件事。** 不做:批量/多血管、放样实时预览、Chan-Vese、ML、ITK/VTK 演化。

## 主会话已定死的决策(worker 不自由发挥)

1. **模型 = GAC 测地活动轮廓**(纯域 2D 有限差分):梯度停止 `g = 1/(1+(|∇I_σ|/α)²)` + 曲率正则项 + 气球力(propagation)。不做 Chan-Vese。
2. **不引 ITK / VTK**(分层铁律 + 别引重依赖)。演化、高斯平滑、梯度、有限差分全部**纯 C++ 自实现在 service .cpp**。SV 的 `cvITKLevelSet` 只作语义参照(它用了 advection/curvature/propagation/gradient-stop 这些项),**别照抄 ITK**。
3. **收口点复用 `traceSeededIsoLoop`**:演化产出 signed-distance 场 φ(**约定:轮廓内 φ<0、轮廓外 φ>0**),收敛后 `traceSeededIsoLoop(phi, W, H, px, 0.0, seed)` 提**零水平集**闭合环选种子环。三法(阈值 iso=threshold / 区域生长 iso=0.5 / 水平集 iso=0.0)完全同一收口。
4. **不改 core**:`ContourType::LevelSetResult` 已存在,水平集入组用它。**已知事实:P3-4 区域生长临时也用了 `LevelSetResult`**(见 `runRegionGrowOnCurrentSection` :3766)。本批**接受水平集与区域生长共用同一 `LevelSetResult` 标签**(不影响放样,放样只看点;区分两法非本批范围)。**别为区分它俩去动 core 加枚举。**
5. **预览性能:滑条 released 才演化**(不同于阈值/区域生长的 `valueChanged` 实时预览)。水平集单次演化 = 迭代数 × 全域差分,慢,拖动时不重算,`QSlider::sliderReleased` 才跑一次完整演化画预览。
6. **参数 UI = 单滑条**(演化强度/迭代数)。其余参数(sigma 高斯平滑、时间步 dt、曲率权重、气球力权重、梯度停止 α)**固定为 service 内部合理默认**,不暴露 UI。

## 现状(已由主审亲读源码,worker 照此,别重扫;别派 agent 读 SV,memory `sv-research-agent-cyber-falsepositive`)

### 直接复用的地基(HEAD d81bcec,主审已亲读)

**services/segmentation(纯域无 VTK/Qt)—— `ContourExtractionService.{h,cpp}`:**
- `struct ContourPoint2D { double u; double v; }`(section-local mm)。
- **匿名命名空间 helper `traceSeededIsoLoop(const std::vector<double>& field, int W, int H, double px, double iso, const ContourPoint2D& seed) → std::vector<ContourPoint2D>`**(.cpp :148):marching squares 提所有闭合环 → 选**含种子最小面积环**(`pointInLoop` 判含种子,`signedArea` 比面积;无环含种子退化选质心最近环)。**水平集提零水平集直接调它(iso=0.0)。**
- 匿名命名空间还有:`pixelToSection(x,y,W,H,px)`、`sampleAt(gray,W,x,y)`(row-major idx=y*W+x)、`signedArea`、`centroidOf`、`pointInLoop`、`crossingT`、`edgePoint`。**水平集实现可复用这些(同一匿名 ns 内)。**
- 已有公共方法:`circle`/`polygon`/`estimateThreshold`/`thresholdContour`/`regionGrowContour`。**本批新增 `levelSetContour`(见下)。**
- 断面像素约定:section-local (u,v) mm,section CENTER 在 (0,0):`u=(x-(W-1)/2)*px, v=(y-(H-1)/2)*px`;种子像素反算 `sx=lround(u/px+(W-1)/2)`(clamp),照 `regionGrowContour` :474-481。

**app/XQMainWindow.{h,cpp} —— 预览三件套范式(阈值/区域生长对称,水平集照抄):**
- `previewXxxContour(pts2d out, usedParam out) → bool`:读像素(`crossSectionView_->sectionPixels(gray,W,H,px)`)+ 映射滑条参数 + 调 service(用 `sectionSeed_`)。返回 false = 无断面。
- `updateXxxPreview()`:槽,调 preview + 画到共用橙色 `crossSectionView_->setPreviewContour(preview)` 预览层 + 更新参数标签。不入组。
- `runXxxOnCurrentSection()`:点按钮才 `addContourFromSection2D(pts2d, ContourType::...)` 入组 + 清预览。空/无环给状态栏提示。
- `sectionSeed_`(`ContourPoint2D` 成员,默认 {0,0}):手动点种子基建(P3-4),阈值/区域生长/水平集都锚它。换断面 `updateCrossSectionAtSample` 重置回 {0,0}。水平集初始 φ = 以 `sectionSeed_` 为圆心的小圆的 signed distance。
- **头文件方法声明区**在 :339-367(`previewThresholdContour`/`updateThresholdPreview`/`runRegionGrowOnCurrentSection`/`previewRegionGrowContour`/`updateRegionGrowPreview`/`onSeedPicked` 一带),水平集三方法**紧跟 `updateRegionGrowPreview()` 之后声明**(:363 后)。
- **头文件成员声明区**在 :448-462(`contourMethodThresholdBtn_`/`thresholdSlider_`/`contourMethodRegionGrowBtn_`/`regionGrowBandSlider_`/`regionGrowBandLabel_`/`contourSeedPickBtn_`/`sectionSeed_`),水平集按钮/滑条/标签**紧跟 `contourSeedPickBtn_`(:458)之后、`sectionSeed_`(:462)之前**声明。
- **顶栏方法区** `buildCrossSectionWorkbench` 的 `toolRow`(:2992-3084):现次序 `[Method:] [Threshold][thresholdLabel][thresholdSlider] [Region grow][bandLabel][bandSlider][Seed] |spacer| [Circle][Polygon][Draw]`。水平集按钮 + 标签 + 滑条**插在 `contourSeedPickBtn_`(:3079)之后、`toolRow->addSpacing(16)`(:3080)之前**(仍在自动方法区,手绘工具之前)。
- **信号连接区**在 :3145-3174(阈值/区域生长/seed 的 connect)。水平集 connect 加在 `regionGrowBandSlider_` connect(:3155-3156)之后:按钮 `clicked → runLevelSetOnCurrentSection()`;滑条 **`sliderReleased`(不是 `valueChanged`)→ updateLevelSetPreview()**(released 才演化)。
- `updateContourWorkbenchState()`(:3183 调用处;定义处 worker 用 `rg` 找)按组是否 active 启用/禁用方法控件——水平集按钮/滑条须一并纳入启用/禁用(照区域生长控件的写法)。

### SV 水平集参照(主审已亲读,worker 别再读 SV)

`sv4gui_SegmentationUtils.cxx` `CreateLSContour`(:701):圆种子 `vtkGenerateCircle(radius, center, 50)` 初始化 → `cvITKLevelSet` **两阶段演化**,每阶段设 `SetAdvectionScaling(1.0)` + `SetCurvatureScaling(1.0)` + `SetSigmaFeature/SetSigmaAdvection`,`ComputePhaseOneLevelSet(kc, expFactorRising, expFactorFalling)`(kc=propagation/气球力) → front1 当种子做 phase2 → 取 front2 → merge pts。**语义 = advection(梯度停止特征力)+ curvature(曲率正则)+ propagation(气球力 kc)。XQ 纯域复现这三项的 2D 有限差分单阶段版即可**(不做两阶段;单阶段 + 单滑条控迭代数已够治「区域生长溢出」)。

## 实现(接口已定死,worker 不自由发挥)

### 1. ContourExtractionService 加 levelSetContour(services/segmentation,纯域无 VTK/Qt)

**头文件**(`ContourExtractionService.h`,紧跟 `regionGrowContour` 声明 :85-89 之后加),签名定死:

```cpp
    // Extracts one closed 2D contour of the vessel lumen from a section by
    // geodesic active contour (GAC) level-set evolution, then tracing the zero
    // level set. Robust to blurred / low-contrast / noisy boundaries where
    // region-grow or a global threshold overflow or leak, because the curvature
    // term regularises (smooths) the front and the gradient-stop term halts it at
    // edges. `gray` is a row-major W*H buffer (idx = y*W + x). `pixelSizeMm` maps
    // pixels to section-local mm; returned points are section-local (u, v) mm with
    // the section CENTER at (0, 0), same convention as thresholdContour /
    // regionGrowContour. `seed` is the section-local (u, v) mm seed (section
    // center (0,0) is a good default): the signed-distance level set is
    // initialised as a small circle around the seed (inside negative, outside
    // positive). `iterations` is the evolution step count (>= 1; clamped up);
    // more iterations let the front travel further before the discrete stopping
    // criteria hold. Internal parameters (Gaussian sigma, time step, curvature /
    // balloon / gradient-stop weights) are fixed sensible defaults. After
    // convergence the zero level set is traced with the shared traceSeededIsoLoop
    // (iso = 0). Returns an ordered, closed loop (last point != first); empty when
    // the section is degenerate or no closed zero-level-set loop encloses/near the
    // seed (caller rejects -> no contour added).
    static std::vector<ContourPoint2D> levelSetContour(const std::vector<double>& gray,
                                                       int width, int height,
                                                       double pixelSizeMm,
                                                       const ContourPoint2D& seed,
                                                       int iterations = 120);
```

**实现要点**(`ContourExtractionService.cpp`,纯 C++,不引 VTK/ITK):

- 入口 guard 照 `regionGrowContour`:`width<=1||height<=1||gray.size()!=W*H → 返回空`;`iterations<1 → clamp 到某最小(如 1)`。
- **归一化灰度**到 [0,1](按 gray 的 min/max),避免不同断面强度量纲影响固定参数。flat 断面(hi<=lo)直接返回空。
- **高斯平滑** `I_σ`(sigma 固定默认,如 sigma≈1.0~1.5 px):自实现可分离高斯卷积(先横后纵),核半径按 sigma 截断(如 3σ),边界 clamp 复制。**不引 vtkImageGaussianSmooth。**
- **梯度停止函数** `g(x,y) = 1/(1 + (|∇I_σ|/α)²)`:`|∇I_σ|` 用中心差分(边界 forward/backward),α 固定默认(如 α = 灰度梯度尺度的某分位,或固定小常数,worker 定合理值并在报告说明)。g 在边界(高梯度)→0,平坦区→1。
- **初始 φ = signed distance of a seed circle**:圆心 = seed 像素,半径 r0 固定小值(如 min(W,H) 的一小比例,或 3~5 px;报告说明)。`φ(x,y) = dist((x,y),center) - r0`(圆内负、圆外正,满足「内负外正」约定)。
- **演化(显式有限差分,固定迭代数)**:每步更新 `φ ← φ + dt * (g·(κ + c)·|∇φ| + ∇g·∇φ)`,其中:
  - `κ = div(∇φ/|∇φ|)` 曲率(用二阶中心差分标准离散,`|∇φ|` 加小 ε 防除零)—— **曲率正则项,收住毛刺/防溢出**(命门靠它)。
  - `c` = 气球力(propagation,固定小正常数,推动前沿向外扩,让种子小圆长到血管腔壁)。
  - `g·(κ+c)·|∇φ|` = 曲率+气球力被梯度停止 g 调制(边界处 g→0 停止演化)。
  - `∇g·∇φ` = advection(梯度停止函数梯度把前沿吸向边界)。
  - `dt` 固定满足 CFL 的小步长(如 0.1~0.25;报告说明)。
  - 迭代 `iterations` 次(**不做重初始化/窄带即可,全域显式演化,64×64 + 一两百次迭代离散测试够快**;报告实测单次演化耗时量级)。
- **提零水平集**:演化后 `return traceSeededIsoLoop(phi, W, H, pixelSizeMm, 0.0, seed);`(iso=0.0)。**不另写提环/选环。**
- **所有 helper 放同一匿名命名空间**(高斯/梯度/曲率/演化可作匿名 ns 自由函数或直接内联在 levelSetContour body),复用已有 `sampleAt`/`pixelToSection`。

### 2. app 新方法(XQMainWindow,镜像 threshold/regionGrow 范式)

**头文件**(`XQMainWindow.h`,紧跟 `updateRegionGrowPreview()` :363 之后,`onSeedPicked` :367 之前加三方法):

```cpp
    // Auto level-set (GAC) segmentation on the current section (P3-5): reads the
    // section grayscale, evolves a geodesic active contour from the current seed
    // with the slider's iteration count, traces the zero level set via
    // ContourExtractionService::levelSetContour, and adds it as a LevelSetResult
    // contour. A no-op (status hint) with no section / no group / no closed loop.
    void runLevelSetOnCurrentSection();
    // Computes (but does not add) the level-set contour for the current section at
    // the current iteration count. Fills `pts2d` and reports the iteration count
    // used. Returns false with no resliced section. Shared by the (release-driven)
    // preview and the commit path so both evolve identically.
    bool previewLevelSetContour(std::vector<ContourPoint2D>& pts2d, int& usedIterations);
    // Slider-RELEASE slot (not valueChanged -- one evolution is expensive): runs a
    // full level-set evolution once and shows the result as a preview overlay (not
    // added until the Level set button is clicked), and updates the iteration
    // label. Dragging the slider does NOT re-evolve; only release does.
    void updateLevelSetPreview();
```

**成员**(`XQMainWindow.h`,紧跟 `contourSeedPickBtn_` :458 之后、`sectionSeed_` :462 之前):

```cpp
    // Auto level-set segmentation (P3-5): "Level set" method sits after Seed in
    // the automatic-first method row and runs on click. The iteration slider
    // controls the GAC evolution step count; its label previews the count. Because
    // one evolution is expensive, the slider re-evolves on RELEASE only (not on
    // every drag), unlike the threshold / band sliders.
    QToolButton* contourMethodLevelSetBtn_ = nullptr;   // objectName xqContourMethodLevelSet
    QSlider* levelSetIterSlider_ = nullptr;             // objectName xqLevelSetIterSlider
    QLabel* levelSetIterLabel_ = nullptr;
```

**.cpp 实现**(镜像 `previewRegionGrowContour`/`updateRegionGrowPreview`/`runRegionGrowOnCurrentSection` :3668-3770):

- `previewLevelSetContour(pts2d, usedIterations)`:读像素 → 滑条值映射到迭代数(如滑条 0..1000 → 迭代 30..300,或直接滑条即迭代数区间;报告说明映射)→ `pts2d = ContourExtractionService::levelSetContour(gray,W,H,px,sectionSeed_,iterations)`。
- `updateLevelSetPreview()`:调 preview → 更新 `levelSetIterLabel_`(如 "Iterations: %1")→ 画 `setPreviewContour`(≥3 点才画)。**照区域生长,但由 `sliderReleased` 触发。**
- `runLevelSetOnCurrentSection()`:guard(session/scene/crossSectionView/`activeContourGroup_.is_valid()`)→ preview → `pts2d.size()<3` 给状态栏提示("No closed contour was found by the level set." / tr context `xq::XQMainWindow`)→ `addContourFromSection2D(pts2d, ContourType::LevelSetResult)` → 清预览。
- **onSeedPicked**(:3772)重设种子后现刷两个自动预览(threshold + regionGrow)。**水平集慢,不在 onSeedPicked 里自动演化**(避免点种子就卡)——只更新按钮态即可,用户点 Level set 按钮或拖滑条 release 才演化。worker 在报告说明这个刻意取舍(与阈值/区域生长的差别)。

### 3. 顶栏 UI + 信号(app/XQMainWindow,`buildCrossSectionWorkbench`)

- **控件创建**(在 `contourSeedPickBtn_` 创建 :3037-3041 之后加):`contourMethodLevelSetBtn_`(QToolButton,非 checkable,text `translate("XQStageWidgets","Level set")`,objectName `xqContourMethodLevelSet`)+ `levelSetIterLabel_`(QLabel,minWidth 96,text `translate("XQStageWidgets","Iterations:")`)+ `levelSetIterSlider_`(QSlider Horizontal,range 0..1000,默认值 worker 定,如 400)。
- **布局**(在 `toolRow->addWidget(contourSeedPickBtn_,0)` :3079 之后、`toolRow->addSpacing(16)` :3080 之前):`addWidget(contourMethodLevelSetBtn_,0)` + `addWidget(levelSetIterLabel_,0)` + `addWidget(levelSetIterSlider_,1)`。**次序:… Region grow / band / Seed / Level set / iter |spacer| 手绘。**
- **信号**(在 regionGrow connect :3153-3156 之后):
  - `contourMethodLevelSetBtn_ clicked → runLevelSetOnCurrentSection()`。
  - **`levelSetIterSlider_` `QSlider::sliderReleased`(不是 valueChanged)→ updateLevelSetPreview()`**。可另接 `valueChanged → 只更新 label 文本`(让拖动时标签跟着走但不演化;可选,worker 定,报告说明)。
- **启用/禁用**:`updateContourWorkbenchState()` 把 `contourMethodLevelSetBtn_` / `levelSetIterSlider_` / `levelSetIterLabel_` 纳入(照区域生长控件启用/禁用写法)。

## 测试(离散不变量,headless ctest)

在 `CODE_ROOT/tests/services/segmentation/test_contour_extraction.cpp` **追加**水平集用例(同一 test exe,已链 xq_services;bare main + `CHECK(cond)` 宏风格,**副作用调用先执行存结果再 CHECK**,memory `no-sideeffect-in-assert`)。现有 `diskSection`/`loopArea` 辅助复用。**迭代数设小保证快(64×64 断面 + 几十~一百多次迭代)。**

**核心命门 = 模糊/低对比/噪声边界**(专门制造「区域生长/阈值会溢出但水平集靠曲率正则+梯度停止能收住」的场景,证伪性最强):

1. **模糊边界不溢出(命门)**:
   - 造 64×64 断面:中心血管斑(半径 8px,内部高信号≈1.0),但**边界是宽渐变过渡带**(不是硬阶跃,如斑到背景在半径 6..12px 间线性从 1.0 降到 0.2,背景 0.2),再叠**轻噪声**(可确定性伪随机,别用 `Math.random`/`rand()` 不可复现——用固定式子如 `0.02*sin(12*x)*cos(7*y)`)。
   - 种子 = 中心 {0,0}(斑内)。`levelSetContour(gray,64,64,0.5,{0,0},iterations)` 返回**非空闭合环**,面积落在血管斑量级(如真斑 `π*(8*0.5)²≈50 mm²` 的合理带,允许因过渡带前沿停在中段而偏小/偏大,取宽容差如 ±40%,worker 按实际演化定稳定容差并说明),且 `< 0.5 * 断面面积`(**没溢出到整个断面**)。
   - **边界平滑**:相邻轮廓点局部曲率有界(离散曲率或转角 < 某阈,证明比区域生长的锯齿平滑);或至少断言点数合理且无自交。worker 选一个可稳定断言的平滑度量。
   - **可证伪(必须两个都真转红)**:①**去掉曲率项**(κ 项系数置 0)→ 前沿无正则,在模糊+噪声边界处溢出/长毛刺 → 面积暴涨/超过断面面积一半 或 平滑度断言破 → 转红。②**去掉梯度停止**(g 恒 =1)→ 气球力无阻挡把前沿推满断面 → 面积→接近断面面积 → 转红。worker 报告写明这两个篡改点前后面积/平滑数值。

2. **区域生长 vs 水平集对照(证「更平滑/不溢出」的量化命门,强烈推荐)**:
   - 同上模糊+噪声断面,用 P3-4 `regionGrowContour` 在同断面跑(band 取一个「够纳入斑但会被噪声/渐变带拉出去」的值):区域生长因无曲率正则,在渐变带+噪声处 flood-fill 溢出 → 面积偏大/边界锯齿。水平集描准且平滑(命门 1 已证)。**断言:levelSet 面积比 regionGrow 更接近真斑面积**(`|lsArea-true| < |rgArea-true|`),或 levelSet 平滑度明显优于 regionGrow(相邻点曲率方差更小)。若构造上区域生长恰好也稳,调渐变带宽度/噪声幅度让区域生长确实溢出再断言;构造不出稳定「区域生长失手」场景则报告说明,退回只保命门 1。**这条是「治本(比区域生长鲁棒)」的离散证据。**

3. **清晰边界回归(证不破已有能力)**:造硬边界血管斑(P3-4 命门 1 那种阶跃斑),`levelSetContour` 也应描出≈斑面积的闭合环(水平集在清晰边界上不比区域生长差)。防止「只对模糊断面 work、对清晰断面反而收不住」。

> 水平集离散测试全在合成灰度数组上跑,**不需要 vtkImageData**(service 纯域)。app 层水平集方法(preview/run/UI/release 触发)无独立离散测试(交互 + Qt,靠真机覆盖)。**现有阈值(4 条)+ 区域生长离散断言必须仍全绿**(本批只加不改 `traceSeededIsoLoop` / `thresholdContour` / `regionGrowContour`)——worker 确认无回归。

## 验证命令(worker 必跑,报告贴输出)

1. 全新构建(**本批改 Q_OBJECT 头**——XQMainWindow 加成员/槽 = 改头,**必 rm -rf build_gui 全新构建**,memory `ninja-stale-moc-gui-crash`):
   ```
   rm -rf "C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui" && cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
   ```
   **当前基线全新构建 248/248。** vcvars64 冷启动偶挂死 → 换 DevShell(memory `vcvars64-coldstart-hangs-use-devshell`;「注入+configure+build」写进同一 .ps1 一次跑完)。
2. ctest(基线 72/72;本批新增断言追加进 **test_contour_extraction**,不新增 test exe,期望仍 **72/72 全绿**):
   ```
   cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"
   ```
   **必走 ctest,裸跑 test exe 缺 VTK DLL exit 127**(memory `ctest-environment-overrides-path`)。
3. 假绿抽查:对**命门 1** 的两个篡改点(①曲率项系数置 0 ②梯度停止 g 恒=1),分别改被测逻辑 → 走 ctest 该测试真 FAIL(**核 exe 时间戳变**,memory `ninja-target-incremental-fakegreen-trap`;裸 cmd 调 cmake 不带 vcvars 跑旧 exe 假绿,必用 build_gui_wt.bat,memory `fakegreen-probe-must-replace-not-augment`——篡改必须真正切断被测路径而非追加)→ 还原 → 复绿。报告写明篡改点 + 前后面积/平滑数值。

## i18n(有新可译串就加,memory `xqtr-translate-lupdate-blind`)

- 新串(面板走 `translate("XQStageWidgets",..)`):按钮「Level set / 水平集」、标签「Iterations: / 迭代:」「Iterations: %1」。状态栏走 `tr()`(context `xq::XQMainWindow`):「No closed contour was found by the level set. / 水平集未描出闭合轮廓。」
- **重复串警示(memory `xqtr-translate-lupdate-blind`)**:改前用 `rg` 在 `resources/i18n/xq_zh_CN.ts` 逐条核对每个 (context, source) 组合是否**已存在**。已存在的**别再加 message 块**(lrelease 报 Duplicate)。只有 context+source **新**组合才加。「Level set」「Iterations:」预计为新,但仍须核。
- 手工编辑 `CODE_ROOT/resources/i18n/xq_zh_CN.ts`(UTF-8 无 BOM + LF),**照现有 message 块格式加**,**绝不跑 lupdate**。改完:
  ```
  "C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lrelease.exe" xq_zh_CN.ts -qm xq_zh_CN.qm
  ```
  **当前基线 266 finished / 0 unfinished。** 新增 N 条应 `266+N finished / 0 unfinished`。报告列出新增每条串(标明哪条是复用旧的没新增)+ lrelease 输出。

## 禁止(违反=返工)

- 不做批量/多血管/放样实时预览/Chan-Vese/ML(全超范围)。GAC 只做单阶段(不做 SV 那样的两阶段)。
- **services 层不引 VTK/Qt/ITK**:`ContourExtractionService.{h,cpp}` 纯 core 依赖(只用 std + ContourPoint2D)。高斯平滑 + 梯度 + 曲率 + 有限差分演化必须**纯 C++ 自实现在 service .cpp**,不引 vtkImageGaussianSmooth / ITK LevelSet / 任何 vtk 滤镜。分层铁律违反=返工。
- 头文件不 include VTK / ITK:`ContourExtractionService.h` 纯域。
- **不改 core**(`ContourType` 已有 `LevelSetResult`,水平集用它;不动 `XQContourGroup.h` / 不加枚举)。
- **收口点复用**:提零水平集必须调既有 `traceSeededIsoLoop`(iso=0.0),**绝不第二份提环/选环实现**。`traceSeededIsoLoop` / `thresholdContour` / `regionGrowContour` 行为**逐字不变**(阈值 4 条 + 区域生长离散断言全绿)。
- **随机性不可用不可复现的 RNG**:测试噪声用确定性式子(如三角函数组合),别用 `rand()` / `Math.random`(memory:Workflow 脚本禁 `Math.random`,测试也要可复现)。
- 预览滑条**必须 `sliderReleased` 触发演化**(不是 valueChanged),否则拖动卡死。onSeedPicked 不自动跑水平集演化。
- 新源代码注释一律**英文**(MSVC GBK 坑,memory `msvc-gbk-chinese-comment-syntax-error`)。断言不塞副作用调用(memory `no-sideeffect-in-assert`)。
- i18n 同 context 同 source **别重复加**(memory `xqtr-translate-lupdate-blind`)。
- 不 `git commit`(worker 无提交权;主审复核后提交)。不 `git add -A`。

## 交付物(全部在 CODE_ROOT = C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ)

- 改动:
  - `src/services/segmentation/ContourExtractionService.{h,cpp}`(加 `levelSetContour`:归一化 + 自实现高斯 + 梯度停止 g + 初始 φ 种子圆 + GAC 有限差分演化 + 复用 `traceSeededIsoLoop` 提零水平集)
  - `src/app/XQMainWindow.{h,cpp}`(Level set 按钮 + 迭代滑条 + `runLevelSetOnCurrentSection`/`previewLevelSetContour`/`updateLevelSetPreview` + 三成员 + 顶栏布局 + sliderReleased 连接 + updateContourWorkbenchState 启用)
  - `tests/services/segmentation/test_contour_extraction.cpp`(追加水平集模糊边界/对照/清晰边界回归用例)
  - `resources/i18n/xq_zh_CN.ts`(新串,逐条核重复)
- 报告(必含):
  - `levelSetContour` 实现摘要:归一化方式、高斯 sigma / 核半径、梯度停止 α、初始圆半径 r0、演化更新式各项(曲率/气球力/advection/梯度停止调制)、dt / 迭代映射、单次演化耗时量级、提零水平集调 `traceSeededIsoLoop(...,0.0,seed)`。
  - φ 约定(内负外正)与种子圆初始化如何保证零水平集环含种子(与 `pointInLoop` 选环兼容)。
  - app 接法:preview/run/update 三件套 + **滑条 `sliderReleased` 才演化**(与阈值/区域生长 `valueChanged` 的差别,及为何)+ onSeedPicked 不自动演化的取舍。
  - 顶栏最终布局(Threshold / Region grow / Seed / **Level set** / 手绘的视觉次序)。
  - 命门 1 两个假绿篡改点(曲率置 0 / g 恒 1)前后面积+平滑数值;命门 2 对照数值(若保留);阈值+区域生长既有断言无回归确认。
  - 全新构建 count、ctest count、lrelease finished count(逐项贴输出)。
