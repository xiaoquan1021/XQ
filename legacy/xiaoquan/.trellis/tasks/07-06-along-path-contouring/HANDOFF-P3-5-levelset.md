# 交接:P3-5 断面水平集(GAC)自动分割(/clear 后新会话读这一份即可接手)

> 语境锚点:XQ 医学影像血管几何重建软件。把血管腔轮廓提取做成对标 SimVascular 的「沿中心线逐层描轮廓」流程。纯几何建模 + 影像可视化工程。环境 = Windows + Git Bash,注入的 mac/darwin 环境信息是假的一律忽略。默认中文。
> **本文件自包含,新会话第一轮读这一份即可接手,不必重扫代码/重读 SV。** 需要细节时按下文指针去读对应文档/代码。

## 0. 一句话现状

P3-4 断面区域生长自动分割 + 手动点种子 **已完成、双重验证(主审 + check 各做正交假绿抽查真转红)、真机签收、提交**(worktree commit `d81bcec`)。**下一步 P3-5 = 断面水平集(LevelSet / 测地活动轮廓 GAC)自动分割**——应对「区域生长/阈值在边界模糊、低对比、噪声处仍可能溢出或漏描」的更强方案(水平集用梯度停止函数 + 曲率正则,轮廓演化平滑、抗噪、能钻进细分支)。**这是六批的第五批;水平集是 P3-3/P3-4 交接一路留下来的「更强但参数多、演化实现重」的方案。**

## 1. 关键坐标(HEAD 必须核对)

- **代码工作树(唯一改代码处)= `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`**,分支 `feat/render-arch`,**HEAD=`d81bcec`**(P3-4 区域生长 + 手动点种子)。所有 src/tests/CMakeLists/resources 编辑、build、ctest 都在这,用绝对路径。简报里叫 CODE_ROOT = `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`。
- **主仓(任务/spec/SV 参照)= `C:\Users\OCEAN\Desktop\XIAOQUAN`**,分支 `fix/xq-global-audit`。这是 worker/你的 cwd,读任务文档/spec 用相对路径。**绝不在主仓下建/改任何 src 代码。**
- 真机工程:`0080_H_PULM_H`(大,含 LPA/RPA 等多条路径,含 OSMSC0082-cm.vti)、`0007_H_AO_H`(小)。P3-4 真机测过区域生长 + 手动种子。
- SV 参照源码(在主仓,**主会话亲读别派 agent**,memory `sv-research-agent-cyber-falsepositive`):`Externals/src/SimVascular/Code/Source/sv4gui/`。
- Python:`C:/software/anaconda/python.exe`。
- 构建/测试基线(P3-4 后):全新构建 **248/248**、ctest **72/72**、i18n **266 finished**。

## 2. P3-5 要做什么(本批范围)

**目标**:在断面 2D 图内做**水平集演化(测地活动轮廓 GAC 简化版)**自动分割,从种子圆初始化 → 按梯度停止函数 + 曲率正则演化出血管腔闭合轮廓,对边界模糊/低对比/噪声比区域生长鲁棒。入组后能放样。类型仍标 `ContourType::LevelSetResult`(枚举已存在,P3-4 区域生长临时借用了它;**P3-5 若要区分区域生长 vs 水平集,可在 core 加新枚举值,或复用同一 LevelSetResult——派前定**)。

**关键取舍(派 worker 前主会话定死)**:
- **纯域自实现 2D level set(推荐)**:在 services/segmentation 纯 C++ 实现窄带或全域的 2D GAC/Chan-Vese 简化版(有限差分演化,梯度停止 g=1/(1+|∇I|²),曲率项 κ·|∇φ|,固定迭代数 + 收敛判据)。**可 headless 离散测,守层次纪律**。参数:sigma(高斯平滑)、迭代数、时间步、曲率权重、气球力(propagation)。
- **别引 ITK**(ITK 是 SV 依赖,XQ 未必有;引入重)。SV 的水平集(`CreateLSContour` :701)走 `cvITKLevelSet` 两阶段演化,**只作语义参照不照抄实现**(它依赖 ITK)。
- **是否值得做**:区域生长(P3-4)已覆盖大部分断面。水平集主要赢在**边界模糊/低对比/需要曲率平滑**的断面。派前评估真机上区域生长到底在哪些断面不够——若区域生长 + 手动种子已够用,P3-5 可降级为「小成本补充」甚至跳过直接进 P3-6(批量/多血管)。**真机反馈驱动决策。**

### SV 水平集参照(派 worker 前主会话亲读,别派 agent)

- `Modules/Segmentation/sv4gui_SegmentationUtils.cxx` `CreateLSContour`(:701)——圆形种子 `vtkGenerateCircle(radius, center, 50)` 初始化 → ITK `cvITKLevelSet` **两阶段演化**(`ComputePhaseOneLevelSet(kc, expFactorRising, expFactorFalling)` → 用 front1 当种子做 `ComputePhaseTwoLevelSet(kupp, klow)`)→ 取 front2 → merge pts。参数:kc/expFactor/sigmaFeat/sigmaAdv/maxIter/maxErr。
- 亲读 `sv3_ITKLevelSet.h` + `cvITKLevelSet` 定纯域实现范围(**只看它演化用了哪些项:advection/propagation/curvature/gradient-stop,XQ 纯域复现这些项的 2D 有限差分版,别引 ITK**)。

## 3. XQ 实现要点(层次归属,与 P3-3/P3-4 一致)

- **算法放 services/segmentation**(纯域无 VTK/Qt):`ContourExtractionService` 加 `levelSetContour(gray, W, H, pixelSizeMm, seed, 参数...)`。**复用 P3-4/P3-3 已有基建**:
  - **`traceSeededIsoLoop`(P3-4 抽的匿名命名空间 helper,.cpp)**:水平集演化收敛后得 φ 场(signed distance),`traceSeededIsoLoop(phi, W, H, px, 0.0, seed)`(iso=0 零水平集)直接提零水平集闭合轮廓选种子环。**这是水平集与区域生长/阈值共用的收口——三法都归结为「一个标量场 + 一个 iso 值 + 选种子环」。**
  - `ContourPoint2D`、`pixelToSection`、`sampleAt`、`signedArea`、`pointInLoop` 全可复用。
  - 断面像素读取 `XQCrossSectionViewWidget::sectionPixels`(已就位)。
- **种子**:复用 P3-4 已建的 `sectionSeed_`(手动点种子基建已在)。水平集初始 φ = 以 sectionSeed_ 为圆心的小圆的 signed distance(圆内负、圆外正)。
- **入组链完全复用**:`addContourFromSection2D(pts2d, ContourType::LevelSetResult)`(P3-4 区域生长已用它)。
- **UI**:断面工作台顶栏方法区已有 `[Threshold][阈值滑条] [Region grow][带宽滑条] [Seed] | [Circle][Polygon][Draw]`(app 层 `buildCrossSectionWorkbench` :2918 的 toolRow)。加 `[Level set]` 按钮(自动方法,与阈值/区域生长并列)+ 参数控件(迭代数/平滑滑条,照 P3-3/P3-4 实时预览范式——`previewXxxContour`/`updateXxxPreview`/共用橙色 `setPreviewContour` 预览层)。**水平集演化可能慢(迭代 × 全域差分),实时预览要注意性能**:考虑降分辨率预览或滑条 released 才演化(不像阈值/区域生长那样每拖一次都重算)。
- **i18n**:新串「Level set / 水平集」等走 `translate("XQStageWidgets",..)`;状态栏走 `tr()`(context `xq::XQMainWindow`)。**同 context 同 source 别重复加**(memory `xqtr-translate-lupdate-blind`;基线 266 finished)。

## 4. 验收(离散 + 真机)

- **离散**(命门,可证伪):构造合成断面(血管斑 + **模糊边界/低对比/噪声**——专门制造「阈值/区域生长会溢出但水平集靠曲率正则能收住」的场景)→ `levelSetContour` 从种子圆演化应**闭合**、**面积≈血管斑**、**边界平滑**(相邻点曲率有界,不像区域生长那样锯齿/毛刺)。**可证伪:去掉曲率项/梯度停止 → 轮廓溢出到背景或不收敛 → 面积/平滑度断言转红。** 照 test_contour_extraction 追加(同 test exe)。**注意水平集迭代数要设小保证测试快(几十次迭代 + 小断面 64×64)。**
- **真机**(本批门禁,GUI ctest 全绿≠达标 memory `gui-task-green-tests-not-done`):真实工程沿路径 → 点水平集 → 在**区域生长描不好的模糊/低对比断面**上演化出贴合血管腔一圈(证明比区域生长更平滑抗噪)→ 放若干 → Loft。**主审读不出本地截图(memory `harness-cannot-read-local-images`),真机判定靠用户,派用户前必核 exe 时间戳新过所有改动源(memory `gui-realmachine-test-verify-exe-timestamp`)。**

## 5. P3-4 已交付、本批直接复用的地基(commit d81bcec)

### 断面区域生长(P3-4 主交付)
- **`ContourExtractionService`**(services/segmentation,纯域):
  - `regionGrowContour(gray, W, H, pixelSizeMm, seed, band) → 有序闭合 2D 轮廓`:种子像素反算(`lround(u/px+(W-1)/2)` + clamp)→ 8-连通 flood-fill(判据 `|gray-seedValue|<=band`,栈式,visited 用 mask 防重入)→ mask 转 double → `traceSeededIsoLoop(mask, W,H,px, 0.5, seed)` 提边界选种子环。
  - **`traceSeededIsoLoop(field, W, H, px, iso, seed)`(P3-4 抽的匿名命名空间 helper)**:marching squares 提环 + 选含种子最小面积环(无环含种子退化质心最近)。**thresholdContour / regionGrowContour 共用;P3-5 水平集提零水平集也用它(iso=0)。三法收口点。**
- **手动点种子(P3-4 新建,水平集直接复用)**:
  - widget `DrawMethod::SeedPick` + `seedPicked(double u, double v)` 信号(空 style 接管,eventFilter 消费 press 失败也 return true,memory `xq-seed-pick-eventfilter-vtk-race`)。
  - app `sectionSeed_`(ContourPoint2D 成员,断面局部 (u,v) mm):`onSeedPicked` 存种子 + 关 Seed 按钮回 None + 刷新两个自动预览;换断面(`updateCrossSectionAtSample`)重置回 (0,0);**阈值/区域生长都锚 sectionSeed_**。水平集初始圆圆心用它。
  - Seed 按钮 toggled slot **先清 contourEditToggle_ 再 setDrawMethod(SeedPick)**(否则 editToggle.toggled(false) 会 clobber SeedPick——P3-4 踩过的顺序坑)。

### 更早的地基(P3-1/P3-2/P3-3,水平集同样复用)
- **断面像素读取**:`XQCrossSectionViewWidget::sectionPixels(gray, W, H, pixelSizeMm)`(读 resampler outputImageHandle 的 vtkImageData 逐点 GetScalarComponentAsDouble,行主序 idx=y*W+x,头 VTK-free)。
- **实时预览范式(P3-3 建,P3-4 沿用,水平集照抄)**:`previewXxxContour(pts2d out, usedParam out)` 公共助手(读像素 + 映射参数 + 调 service + 用 sectionSeed_)→ `updateXxxPreview()`(拖滑条槽,画到共用橙色 `setPreviewContour` 预览层 + 更新参数标签,不入组)→ `runXxxOnCurrentSection()`(点按钮才 addContourFromSection2D 入组 + 清预览)。**三法(阈值/区域生长/未来水平集)结构完全对称。**
- **断面轮廓叠加**:`refreshSectionContourOverlay()` 只投影 pathArcLength 匹配当前滑条位置的轮廓(对齐 SV,滑走即消失)。**别改回全画。**
- **断面工作台 / 入组尾 / 绑路径 / 放样链**:见 P3-4 交接 §3(`addContourFromSection2D` 公共入组尾、`activeContourGroup_`、`bindCrossSectionPath`、Loft 链)——全就位,水平集只换「2D 点怎么来」。

## 6. 层次纪律 + 关键坑(P3-5 必守,与 P3-3/P3-4 同)

- `core`(无 VTK/Qt):`ContourType` 已含 `LevelSetResult`。若要区分区域生长/水平集才动 core 加枚举(**派前定;能复用就别动 core**)。
- `services/segmentation`(**无 VTK/Qt**):`ContourExtractionService` 加 `levelSetContour`,纯 C++ 有限差分演化,**不引 VTK/ITK**(分层铁律 + 别引重依赖)。复用 `traceSeededIsoLoop` 提零水平集。
- `visualization`(VTK 私有):widget 头保持 VTK-free。种子拾取已就位(SeedPick)。
- `app`(接线):方法按钮 + 参数滑条 + 实时预览(照 previewThreshold/previewRegionGrow 范式)+ 入组走 addContourFromSection2D。**水平集慢,预览性能注意(降分辨率/released 才演化)。**
- stage panel(`XQStageWidgets.cpp` buildSegmentationPage)是**旧整卷 UI**,已被断面工作台取代,**别动它**。
- **改 Q_OBJECT 头(widget/mainwindow 加成员方法/信号)必 `rm -rf build_gui` 全新构建**(memory `ninja-stale-moc-gui-crash`)。
- 新源文件注释**英文**(MSVC GBK 坑 memory `msvc-gbk-chinese-comment-syntax-error`)。断言不塞副作用(memory `no-sideeffect-in-assert`)。

## 7. channel worker 流程(P3-3/P3-4 实测有效,照走)

- **spawn 绝不用 `--file`/`--jsonl` 挂文件**(memory `channel-spawn-file-jsonl-hangs-worker`)。spawn 只 `--agent implement --as impl --cwd C:/Users/OCEAN/Desktop/XIAOQUAN --timeout 90m`;然后 `channel send --to impl --as main --text-file <简报路径>` 让 worker 自己 Read 简报(send 用 `--text-file` 不是 `--text`,text 是位置参数)。
- **worker done≠交付物齐全**(memory `worker-done-may-skip-tests-verify`):done 后 `git diff --stat` 对交付物清单逐条核。末条 message 若含 raw toolcall XML=那步没执行。
- **主审必接管全部验证**:`rm -rf build_gui` 全新构建 + 全量 ctest + **亲手假绿抽查**(篡改被测逻辑真转红,**核 exe 时间戳变**——P3-4 踩过坑:①裸 cmd 调 cmake 不带 vcvars → cmake 不在 PATH 跑旧 exe 假绿,必用 build_gui_wt.bat;②假绿抽查若「追加」而非「替换」被测起点,原逻辑仍生效→假通过,篡改必须真正改变被测路径,见 memory `fakegreen-probe-must-replace-not-augment`)+ **亲读最终代码**(memory `subagent-mainreview-must-read-code`)+ git diff 核残留。
- 派 check worker 复验(用与主审不同的命门做假绿抽查);check 抽查会短暂改代码,**认账前 git diff 核它还原干净**(memory `worker-handedit-ts-bom-crlf`;P3-4 check 自修了 1 处重复注释并全量复绿,认账前已核实还原)。
- **读 channel message 正文**:`channel messages --raw --last N`(不吃 `--as`);含中文/✓ 用 `C:/software/anaconda/python.exe` + `io.TextIOWrapper(encoding='utf-8')` 解析,别裸 print(GBK 编码错)。
- **wait**:`--as main --from impl --kind turn_finished`(不是 turn_ended);先 cat 输出确认 `turn_finished` 再信(memory `trellis-channel-wait-args`)。
- 完工 `channel rm <chan>` 清理 + 删临时简报文件。

## 8. 构建/测试/提交(硬规范)

- 全新构建:`rm -rf "CODE_ROOT/build_gui"` 然后 `cmd //c "...\build_gui_wt.bat"`。vcvars64 冷启动偶挂死→换 DevShell(memory `vcvars64-coldstart-hangs-use-devshell`)。
- ctest:`cmd //c "...\ctest_merge.bat"`(基线 72/72;新增测试后上调)。**必走 ctest,裸跑 test exe 缺 VTK DLL exit 127**(memory `ctest-environment-overrides-path`)。
- lrelease:`<Qt>/bin/lrelease.exe xq_zh_CN.ts -qm xq_zh_CN.qm`,Qt = `C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0`。基线 266 finished。**手工编辑 .ts(UTF-8 无 BOM+LF)不跑 lupdate**(memory `xqtr-translate-lupdate-blind`);面板串走 `translate("XQStageWidgets",..)`,主窗口串走 `tr()`。**同 context 同 source 别重复加**(会被 lrelease 合并报 Duplicate)。
- **真机**:`cmd //c "...\run_xq.bat"`,用户 `! run_xq.bat` 自己开;真机第一门禁,真机过再 commit。
- 提交:精确 `git add <路径>` **绝不 `-A`**(worktree 有 `xq_app_dist/` + 上级历史 md 不能进库,memory `commit-check-gitignore-present`);中文信息「类型: 描述」;结尾 `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。worktree 代码提 `feat/render-arch`;spec/任务文档提主仓 `fix/xq-global-audit`。

## 9. 新会话接手动作

1. 读本文件(已读)。
2. 核对坐标(§1):worktree HEAD `d81bcec`、主仓分支 `fix/xq-global-audit`、真机工程存在。
3. **决策点**:P3-5 做水平集吗?先看真机上区域生长(P3-4)到底在哪些断面不够——**若区域生长 + 手动种子已够用,可降级/跳过水平集直接进 P3-6(批量/多血管)**。做水平集则:纯域 2D GAC 简化版(别引 ITK);是否要在 core 加枚举区分区域生长 vs 水平集(能复用 LevelSetResult 就别动 core)。**派 worker 前问用户定死。**
4. 需要时读 P3-4 交付代码(§5 列的 `regionGrowContour`/`traceSeededIsoLoop`/`sectionSeed_`/`onSeedPicked`/预览范式)。
5. **SV 参照主会话亲读**(§2 指针,别派 agent):水平集读 `CreateLSContour`(:701)+ `sv3_ITKLevelSet.h`/`cvITKLevelSet` 定纯域实现范围(只看演化用哪些项,别照抄 ITK)。
6. 写 EXECUTE-P3-5 简报(照 EXECUTE-P3-4.md 格式,自包含,接口定死/离散测试(含「模糊/低对比可证伪」命门)/验证/禁止/交付物)→ 派 channel worker → 主审接管验证(全新构建+全量 ctest+亲手假绿抽查+亲读代码+git diff 核残留)→ check worker → **真机(重点验区域生长描不好的模糊/低对比断面上水平集能描准且平滑)** → commit → 写 P3-6 交接。

## 10. 本任务链已沉淀的 memory(新会话已自动加载索引)

- `worker-done-may-skip-tests-verify` — worker done≠交付物齐全,done 后 git diff --stat 逐条核。
- `fakegreen-probe-must-replace-not-augment`(P3-4 新增)— 假绿抽查篡改必须「替换」被测路径而非「追加」,否则原逻辑仍生效→假通过;裸 cmd 调 cmake 不带 vcvars 会跑旧 exe 假绿,必用 build_gui_wt.bat。
- `sv-seg-auto-primary-manual-fallback` — SV 三阶段自动为主手绘兜底,分割优先级 水平集/阈值/ML>手绘。
- `sv-research-agent-cyber-falsepositive` — 读 SV 分割源码主会话亲读别派 agent。
- `xq-threshold-seg-fullblock-bug` — 整卷阈值出板砖=缺种子约束;断面靠种子选环。
- `gui-task-green-tests-not-done` / `render-architecture-must-be-validated-first` — GUI ctest 全绿≠达标,真机第一门禁。
- `channel-spawn-file-jsonl-hangs-worker` / `subagent-mainreview-must-read-code` / `gui-realmachine-test-verify-exe-timestamp` / `worker-handedit-ts-bom-crlf` / `trellis-channel-wait-args` — worker 流程 + 主审验证纪律。
- `xq-seed-pick-eventfilter-vtk-race` — 断面点种子交互空 style 显式接管。
- `ninja-stale-moc-gui-crash` / `ninja-target-incremental-fakegreen-trap` / `ctest-environment-overrides-path` / `msvc-gbk-chinese-comment-syntax-error` / `xqtr-translate-lupdate-blind` / `commit-check-gitignore-present` / `vcvars64-coldstart-hangs-use-devshell` — 构建/测试/提交坑。

## 11. P3-4 真机遗留 / P3-5 或后续可处理

- **区域生长溢出**:边界模糊/低对比/噪声断面上,区域生长的 band 判据可能溢出到背景(无曲率正则)。这正是 P3-5 水平集要治的(曲率项收住边界)。
- **手动种子已解决固定中心偏移**:P3-3 遗留的「固定中心种子偶偏」P3-4 已靠手动点种子解决(真机验过)。
- **六批进度**:P3-1 断面工作台 → P3-2 手绘 + 绑路径 → P3-3 阈值 → P3-4 区域生长 + 手动种子(本次)→ **P3-5 水平集(下一批,可选/可降级)** → P3-6 批量/多血管。放样链全程已通。
