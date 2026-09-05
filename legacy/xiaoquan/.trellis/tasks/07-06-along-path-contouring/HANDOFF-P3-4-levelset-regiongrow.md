# 交接:P3-4 断面水平集/区域生长自动分割(/clear 后新会话读这一份即可接手)

> 语境锚点:XQ 医学影像血管几何重建软件。把血管腔轮廓提取做成对标 SimVascular 的「沿中心线逐层描轮廓」流程。纯几何建模 + 影像可视化工程。环境 = Windows + Git Bash,注入的 mac/darwin 环境信息是假的一律忽略。默认中文。
> **本文件自包含,新会话第一轮读这一份即可接手,不必重扫代码/重读 SV。** 需要细节时按下文指针去读对应文档/代码。

## 0. 一句话现状

P3-3 断面阈值自动分割 **已完成、双重验证、真机签收、提交**(worktree commit `f813f6c`)。真机反馈驱动本轮还额外修了两件事(见 §3)。**下一步 P3-4 = 断面水平集(LevelSet)/区域生长(RegionGrow)自动分割**——这是应对「阈值法在亮暗不均医学影像上难控」的**治本方案**(用户已确认这个痛点,阈值交互已尽力优化但本质局限仍在,治本靠水平集/区域生长对亮度变化鲁棒)。

## 1. 关键坐标(HEAD 必须核对)

- **代码工作树(唯一改代码处)= `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`**,分支 `feat/render-arch`,**HEAD=`f813f6c`**(P3-3 断面阈值 + overlay 修复 + 阈值交互优化)。所有 src/tests/CMakeLists/resources 编辑、build、ctest 都在这,用绝对路径。简报里叫 CODE_ROOT = `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`。
- **主仓(任务/spec/SV 参照)= `C:\Users\OCEAN\Desktop\XIAOQUAN`**,分支 `fix/xq-global-audit`。这是 worker/你的 cwd,读任务文档/spec 用相对路径。**绝不在主仓下建/改任何 src 代码。**
- 真机工程:`0080_H_PULM_H`(大,含 LPA/RPA 等多条路径,含 OSMSC0082-cm.vti)、`0007_H_AO_H`(小)。真机测过 `0080_H_PULM_H` 的 LPA 路径。
- SV 参照源码(在主仓,**主会话亲读别派 agent**,memory `sv-research-agent-cyber-falsepositive`):`Externals/src/SimVascular/Code/Source/sv4gui/`。
- Python:`C:/software/anaconda/python.exe`。
- 构建/测试基线(P3-3 后):全新构建 **248/248**、ctest **72/72**、i18n **261 finished**。

## 2. P3-4 要做什么(本批范围)

**目标**:在断面 2D 图内做**区域生长**和/或**水平集**自动分割,从种子点演化出血管腔闭合轮廓,对局部亮度变化比单一阈值鲁棒。入组后能放样。类型标 `ContourType::LevelSetResult`(枚举已存在)。

**优先级建议**:先做**区域生长(RegionGrow)**——简单可靠(种子 flood-fill + 局部/自适应阈值 + 边界追踪),对亮暗不均已比全局阈值好很多,且复用 P3-3 已有的 marching squares 边界提取基建;水平集(LevelSet)更强但参数多、演化实现重,可本批顺带或留下一子批。**派 worker 前和用户定死做哪个/两个都做。**

### SV 参照算法(派 worker 前主会话亲读,别派 agent)

- **水平集**:`Modules/Segmentation/sv4gui_SegmentationUtils.cxx` `CreateLSContour`(:701,已亲读概要)——圆形种子 `vtkGenerateCircle(radius, center, 50)` 初始化 → ITK `cvITKLevelSet` **两阶段演化**(`ComputePhaseOneLevelSet(kc, expFactorRising, expFactorFalling)` → 用 front1 当种子做 `ComputePhaseTwoLevelSet(kupp, klow)`)→ 取 front2 → merge pts。参数:kc/expFactor/sigmaFeat/sigmaAdv/maxIter/maxErr。**XQ 若做水平集,纯域实现 2D 测地活动轮廓(GAC)简化版**,别引 ITK(ITK 是 SV 依赖,XQ 未必有);或评估引入轻量 2D level set。派前主会话亲读 `sv3_ITKLevelSet.h` + `cvITKLevelSet` 定实现范围。
- **区域生长**:SV 的 3D 版在 `sv4gui_Seg3DUtils.cxx`(CollidingFronts/ConnectedThreshold),2D 断面区域生长 SV 没独立函数(靠水平集覆盖)。**XQ 2D 区域生长自实现**:种子 (u,v)=断面中心 → 8-连通 flood-fill,纳入条件 = 像素值在 [种子值±带宽] 或 > 局部自适应阈值 → 得连通区 → 复用 P3-3 marching squares 提该区边界为闭合轮廓。**这是纯域纯 C++,可 headless 测,推荐先落地。**

### XQ 实现要点(层次归属)

- **算法放 services/segmentation**(纯域无 VTK/Qt):`ContourExtractionService` 加 `regionGrowContour(gray, W, H, pixelSizeMm, seed, 带宽/容差参数)` 和/或 `levelSetContour(...)`。**复用 P3-3 已有基建**:marching squares 边界提取(已抽在 .cpp 匿名命名空间,区域生长得到二值 mask 后可复用同套 case 表/接环/选环提边界)、`ContourPoint2D`、pixelToSection 映射。参数默认从断面直方图/种子邻域估计。
- **种子**:第一版仍用断面中心 (0,0)=pose.origin。**本批也可顺带加「断面上手动点种子」**(P3-3 真机发现路径采样点与真实管腔中心有偏移,固定中心种子偶尔偏——手动点种子能解决;用户 P3-3 时选了「留 P3-4」)。断面交互点种子照 memory `xq-seed-pick-eventfilter-vtk-race`(空 style 显式接管,别抢事件时序);widget 已有 eventFilter + displayToSection(像素→(u,v))基建。
- **入组链完全复用**:产出断面 2D 轮廓点 → `addContourFromSection2D(pts2d, ContourType::LevelSetResult)`(P3-3 已抽的公共入组尾,自动 unproject+入组+refresh)。**这条链已验证,只换「2D 点怎么来」。**
- **UI**:断面工作台顶栏方法区已有[阈值分割][阈值滑条][圆][多边形][绘制](自动优先布局)。加[区域生长]/[水平集]按钮(自动方法,与阈值并列常用位)+ 对应参数控件(带宽/容差滑条,照阈值滑条实时预览范式——P3-3 已建 `setPreviewContour` 预览层 + `previewThresholdContour`/`updateThresholdPreview` 范式可照抄)。stage panel 右侧页有旧「区域生长」radio(见 §4)可复用文案。

### 验收(离散 + 真机)

- **离散**(命门,可证伪):构造合成断面(中心血管斑 + **渐变/不均背景**,专门制造「全局阈值描不准但区域生长/水平集能描准」的场景)→ regionGrowContour/levelSetContour 从中心种子提取应**闭合**且**面积≈血管斑**(不吞不均背景)。**可证伪:去掉种子约束/连通约束 → 面积暴涨必转红。** 照 P3-3 test_contour_extraction 追加(同 test exe,已链 xq_services)。
- **真机**(本批门禁,GUI ctest 全绿≠达标 memory `gui-task-green-tests-not-done`):真实工程沿路径 → 点区域生长/水平集 → 在**阈值描不好的那些不均断面**上描出贴合血管腔一圈(证明比阈值鲁棒)→ 放若干 → Loft 出血管管。**主审读不出本地截图(memory `harness-cannot-read-local-images`),真机判定靠用户,派用户前必核 exe 时间戳新过所有改动源(memory `gui-realmachine-test-verify-exe-timestamp`)。**

## 3. P3-3 已交付、本批直接复用的地基(commit f813f6c)

### 断面阈值分割(P3-3 主交付)
- **`ContourExtractionService`**(services/segmentation,纯域):
  - `thresholdContour(gray, W, H, pixelSizeMm, threshold, seed) → 有序闭合 2D 轮廓`:2D marching squares(16 case 表 + saddle 5/10 cell-mean 消歧 + EdgeKey 整数键保证共享边节点一致 + 节点邻接接环)→ 绕数 point-in-polygon **选含种子的最小面积环**(无环含种子退化到质心最近环)。**这套 marching squares + 选环基建 P3-4 区域生长提边界可直接复用**(匿名命名空间的 helper 在 .cpp 顶部)。
  - `estimateThreshold(gray, W, H, pct=0.90) → 直方图高端值`(血管高信号带,memory `xq-threshold-seg-fullblock-bug`)。
- **断面像素读取**:`XQCrossSectionViewWidget::sectionPixels(gray, W, H, pixelSizeMm)`——从 resampler `outputImageHandle()`(vtkImageData)逐点 `GetScalarComponentAsDouble` 读灰度(行主序 idx=y*W+x)。VTK 只在 .cpp,头 VTK-free。**区域生长/水平集拿断面像素照调它。**
- **阈值交互(P3-3 真机优化,照抄范式给 P3-4)**:
  - `previewThresholdContour(pts2d out, usedThreshold out)`:抽的公共助手,滑条(0..1000)线性映射断面**真实 [min,max] 强度范围**(SV 用图像 range 非百分位),算 pts2d。预览和入组共用它保证同强度。
  - `updateThresholdPreview()`:拖滑条槽,实时重描候选轮廓画到 `setPreviewContour`(**独立预览层**,橙色,区别已入组青色/手绘黄色)+ 标签显示真实强度值。**不入组**,点按钮才 `addContourFromSection2D` 入组。**区域生长/水平集的带宽/容差滑条照这个实时预览范式做,手感最好。**
  - `crossSectionView_->setPreviewContour(loops)`:widget 预览层接口(第三个 actor thresholdPreviewActor_,橙色)。换断面(`updateCrossSectionAtSample`)/入组后清空。

### 断面轮廓叠加(P3-3 真机修的 bug,本批注意别回退)
- `refreshSectionContourOverlay()`(XQMainWindow.cpp)**只投影 pathArcLength 匹配当前滑条位置(容差 1e-3mm)的轮廓** → `setDisplayedContours`。**对齐 SV:断面只叠加显示「当前路径点位置」的那个轮廓,滑走即消失。** 原实现画整组所有轮廓不过滤(堆屏、滑走不消失),已修。**P3-4 入组新轮廓后同样走这条,别改回全画。**

### 更早的地基(P3-1/P3-2)
- **断面工作台**:`XQCrossSectionViewWidget`(vtkImageReslice 常驻管线,ParallelProjection)、`XQCrossSectionResampler`(x=normal/y=binormal/z=tangent,40mm@0.2mm/px=200×200,`outputSpec()`/`outputImageHandle()`/`lastFrame()`)。断面局部 (u,v) mm 中心=(0,0)=pose.origin。
- **断面 2D→world**:`XQContourGroup::unprojectFromFrame(frame,u,v)=origin+u·xAxis+v·yAxis`。`projectToFrame` 反向。
- **入组公共尾**:`XQMainWindow::addContourFromSection2D(pts2d, ContourType)`——取 lastFrame() 拷 ContourFrame、当前 arcLength、unproject 每点、组 XQContour、addContour、refreshSceneTree+refreshSectionContourOverlay。**所有自动/手绘方法入组都走它。**
- **活动组/绑路径**:`activeContourGroup_`(NodeId)、`createContourGroupFromPicker`(选路径建组)、`bindCrossSectionPath`(读活动组 sourcePathNode 的 samplePoints 存 sectionPathSamples_)、`updateCrossSectionAtSample(index)`(滑条驱动 reslice + 换断面清预览)。
- **放样链**:选轮廓组 → 建模页 Loft(`ContourLoftInputBuilder::buildLoftInput` → `ModelingService::loftSurface`)。`XQContourGroupPayload` 内存组直接可喂。

## 4. 层次纪律 + 关键坑(P3-4 必守)

- `core`(无 VTK/Qt):`ContourType` 已含 `LevelSetResult`/`ThresholdResult`,**不改 core**。
- `services/segmentation`(**无 VTK/Qt**):`ContourExtractionService` 加 regionGrow/levelSet,纯 C++,**不引 VTK**(别用 vtkImageThreshold/vtk 连通滤镜——分层铁律,违反=返工)。复用 P3-3 marching squares helper。
- `visualization`(VTK 私有):widget 头保持 VTK-free。种子拾取交互空 style 接管(memory `xq-seed-pick-eventfilter-vtk-race`)。
- `app`(接线):方法按钮 + 参数滑条 + 实时预览(照 P3-3 previewThreshold 范式)+ 入组走 addContourFromSection2D。
- stage panel(`XQStageWidgets.cpp` buildSegmentationPage)是**旧整卷阈值 UI**,已被断面工作台取代,**别动它**(P3-2 决策:所有断面操作放 app 层 buildCrossSectionWorkbench 顶栏,不改 buildSegmentationPage 签名)。其 .ts 已有「区域生长」「Region grow」串可复用。
- **改 Q_OBJECT 头(widget/mainwindow 加成员方法)必 `rm -rf build_gui` 全新构建**(memory `ninja-stale-moc-gui-crash`)。
- 新源文件注释**英文**(MSVC GBK 坑 memory `msvc-gbk-chinese-comment-syntax-error`)。断言不塞副作用(memory `no-sideeffect-in-assert`)。

## 5. channel worker 流程(P3-3 实测有效,照走)

- **spawn 绝不用 `--file`/`--jsonl` 挂文件**(memory `channel-spawn-file-jsonl-hangs-worker`)。spawn 只 `--agent implement --as impl --cwd C:/Users/OCEAN/Desktop/XIAOQUAN --timeout 90m`;然后 `channel send --to impl --as main --text-file <简报路径>` 让 worker 自己 Read 简报。
- **worker done≠交付物齐全**(memory `worker-done-may-skip-tests-verify`,P3-3 实测):P3-3 impl worker 8min 就 done,源码质量高但**漏了离散测试+i18n+自验证**(回合耗尽前没走到)。**done 后立刻 `git diff --stat` 对交付物清单逐条核**,缺测试/i18n 主审直接自己补最高效;末条 message 若含 raw toolcall XML(`<invoke name=...>`)=那步没执行,去工作树核。
- **主审必接管全部验证**:`rm -rf build_gui` 全新构建 + 全量 ctest + **亲手假绿抽查**(篡改被测逻辑真转红,核 exe 时间戳变 memory `ninja-target-incremental-fakegreen-trap`)+ **亲读最终代码**(memory `subagent-mainreview-must-read-code`)+ git diff 核残留。
- 派 check worker 复验(用与主审不同的命门做假绿抽查);check 抽查会短暂改代码,**认账前 git diff 核它还原干净**(memory `worker-handedit-ts-bom-crlf`)。
- **wait**:`--as main --from impl --kind turn_finished`(不是 turn_ended);先 cat 输出确认 `turn_finished` 再信(memory `trellis-channel-wait-args`)。channel messages `--raw --last N` 不吃 `--as`。判死活看 `channel list --all` WORKERS `1/1`活 + EVENTS 增长。
- 完工 `channel rm <chan>` 清理(名额上限 6)+ 删临时简报文件。

## 6. 构建/测试/提交(硬规范)

- 全新构建:`rm -rf "CODE_ROOT/build_gui"` 然后 `cmd //c "...\build_gui_wt.bat"`。vcvars64 冷启动偶挂死→换 DevShell(memory `vcvars64-coldstart-hangs-use-devshell`)。
- ctest:`cmd //c "...\ctest_merge.bat"`(基线 72/72;新增测试后上调)。**必走 ctest,裸跑 test exe 缺 VTK DLL exit 127**(memory `ctest-environment-overrides-path`)。
- lrelease:`<Qt>/bin/lrelease.exe xq_zh_CN.ts -qm xq_zh_CN.qm`,Qt = `C:/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0`。基线 261 finished。**手工编辑 .ts(UTF-8 无 BOM+LF)不跑 lupdate**(memory `xqtr-translate-lupdate-blind`);面板串走 `translate("XQStageWidgets",..)`,主窗口串走 `tr()`(context xq::XQMainWindow)。**同 context 同 source 别重复加**(P3-3 踩过:XQStageWidgets 已有「Threshold」译「阈值分割」,重复加会被 lrelease 合并报 Duplicate)。
- **真机**:`cmd //c "...\run_xq.bat"`,用户 `! run_xq.bat` 自己开;真机第一门禁,真机过再 commit。
- 提交:精确 `git add <路径>` **绝不 `-A`**(worktree 有 `xq_app_dist/` + 上级历史 md 不能进库,memory `commit-check-gitignore-present`);中文信息「类型: 描述」;结尾 `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。worktree 代码提 `feat/render-arch`;spec/任务文档提主仓 `fix/xq-global-audit`。

## 7. 新会话接手动作

1. 读本文件(已读)。
2. 核对坐标(§1):worktree HEAD `f813f6c`、主仓分支、真机工程存在。
3. **决策点**:P3-4 做区域生长 / 水平集 / 两个都做?(主审倾向先区域生长——纯域可靠、复用 P3-3 基建;水平集参数多留后)。是否本批顺带加「断面手动点种子」(P3-3 真机发现固定中心种子偶偏,用户当时选留 P3-4)?**派 worker 前问用户定死。**
4. 需要时读 P3-3 交付代码(§3 列的 `ContourExtractionService`/`addContourFromSection2D`/`previewThresholdContour`/`refreshSectionContourOverlay`)。
5. **SV 参照主会话亲读**(§2 指针,别派 agent):水平集读 `CreateLSContour`(:701)+ `sv3_ITKLevelSet.h`/`cvITKLevelSet` 定纯域实现范围;区域生长 2D 自设计(SV 无独立 2D 函数)。
6. 写 EXECUTE-P3-4 简报(照 EXECUTE-P3-3.md 格式,自包含,接口定死/离散测试(含「不均背景」可证伪命门)/验证/禁止/交付物)→ 派 channel worker → 主审接管验证(全新构建+全量 ctest+亲手假绿抽查+亲读代码+git diff 核残留)→ check worker → **真机(重点验阈值描不好的不均断面上区域生长/水平集能描准)** → commit → 写 P3-5 交接。

## 8. 本任务链已沉淀的 memory(新会话已自动加载索引)

- `worker-done-may-skip-tests-verify`(P3-3 新增)— worker done≠交付物齐全,漏测试/i18n/自验证;done 后 git diff --stat 对清单逐条核。
- `sv-seg-auto-primary-manual-fallback` — SV 三阶段自动为主手绘兜底,分割优先级 水平集/阈值/ML>手绘。
- `sv-research-agent-cyber-falsepositive` — 读 SV 分割源码主会话亲读别派 agent。
- `xq-threshold-seg-fullblock-bug` — 整卷阈值出板砖=缺种子约束;断面靠种子选环。
- `gui-task-green-tests-not-done` / `render-architecture-must-be-validated-first` — GUI ctest 全绿≠达标,真机第一门禁。
- `channel-spawn-file-jsonl-hangs-worker` / `subagent-mainreview-must-read-code` / `gui-realmachine-test-verify-exe-timestamp` / `worker-handedit-ts-bom-crlf` — worker 流程 + 主审验证纪律。
- `xq-seed-pick-eventfilter-vtk-race` — 断面点种子交互空 style 显式接管。
- `ninja-stale-moc-gui-crash` / `ninja-target-incremental-fakegreen-trap` / `ctest-environment-overrides-path` / `msvc-gbk-chinese-comment-syntax-error` / `xqtr-translate-lupdate-blind` / `commit-check-gitignore-present` / `vcvars64-coldstart-hangs-use-devshell` — 构建/测试/提交坑。

## 9. P3-3 真机遗留(P3-4 或后续可处理)

- **固定中心种子偶偏**:LPA 路径采样点与真实管腔中心有偏移,固定 (0,0) 种子偶尔选不到最佳环(靠「无环含种子退化质心最近」兜住,轮廓仍描在血管上)。根治=断面手动点种子(P3-4 候选)或路径居中优化(路径阶段)。
- **阈值法本质局限**:亮暗不均断面单一阈值仍难全描准。P3-3 已把交互优化到位(真实强度范围滑条 + 实时预览),治本靠本批 P3-4 水平集/区域生长。
