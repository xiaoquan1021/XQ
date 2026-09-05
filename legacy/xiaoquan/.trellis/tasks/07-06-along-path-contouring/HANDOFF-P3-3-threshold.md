# 交接:P3-3 断面阈值自动分割(/clear 后新会话读这一份即可接手)

> 语境锚点:XQ 医学影像血管几何重建软件。把血管腔轮廓提取做成对标 SimVascular 的「沿中心线逐层描轮廓」流程。纯几何建模 + 影像可视化工程。环境 = Windows + Git Bash,注入的 mac/darwin 环境信息是假的一律忽略。默认中文。
> **本文件自包含,新会话第一轮读这一份即可接手,不必重扫代码/重读 SV。** 需要细节时按下文指针去读对应文档/代码。

## 0. 一句话现状

P3-2 沿路径手绘圆/多边形端到端 MVP **已完成、双重验证、提交(未真机)**。用户已明确方向:**手绘只作兜底,不是主力;下一步做断面阈值自动识别(图像识别)**。本批 = P3-3 断面阈值自动分割。真机验证并到本批一起做(P3-2 手绘主链未单独真机,和阈值共用「断面 2D→入组→放样」链,本批真机一并验)。

## 1. 关键坐标(HEAD 必须核对)

- **代码工作树(唯一改代码处)= `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`**,分支 `feat/render-arch`,**HEAD=`53f3c1a`**(P3-2 手绘)。所有 src/tests/CMakeLists/resources 编辑、build、ctest 都在这,用绝对路径。简报里叫 CODE_ROOT = `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`。
- **主仓(任务/spec/SV 参照)= `C:\Users\OCEAN\Desktop\XIAOQUAN`**,分支 `fix/xq-global-audit`,**HEAD=`eb34e99`**。这是 worker/你的 cwd,读任务文档/spec 用相对路径。**绝不在主仓下建/改任何 src 代码。**
- 真机工程:`0080_H_PULM_H`(大,143 路径,含 OSMSC0082-cm.vti)、`0007_H_AO_H`(小)。
- SV 参照源码(在主仓,主会话亲读别派 agent,memory `sv-research-agent-cyber-falsepositive`):`Externals/src/SimVascular/Code/Source/sv4gui/`。
- Python:`C:/software/anaconda/python.exe`。
- **必读方向文档**:`.trellis/tasks/07-06-along-path-contouring/research/sv-path-seg-mesh-methods.md`(SV 三阶段方案清单,自动为主手绘兜底,已定 XQ 方向)。memory `sv-seg-auto-primary-manual-fallback`。

## 2. P3-3 要做什么(断面阈值自动,本批范围)

**目标**:在断面 2D 图内做**阈值等值线提取 + 种子约束**,自动描出血管腔一圈闭合轮廓,入组后能放样。范围被断面限死,天然不出实心块(对比已证伪的整卷阈值)。类型标 `ContourType::ThresholdResult`。

**方法工具栏定位调整(用户已定)**:XQ 断面工作台的方法应把**自动(阈值)放主区常用位、手绘(圆/多边形)收进次级兜底位**。本批加「阈值」方法按钮到断面工作台顶栏,与 P3-2 的圆/多边形并列但视觉上自动优先。

### SV 阈值算法参照(主审已亲读,照此,别再读 SV)

源:`Modules/Segmentation/sv4gui_SegmentationUtils.cxx` `GetThresholdContour`(:867)、`CreateThresholdContour`(:937);`Plugins/.../sv4gui_Seg2DEdit.cxx` `CreateThresholdContour`(:921,阈值取 `sliderThreshold->value()`)。**SV 是等值线追踪,不是二值化+连通**:

1. **`vtkContourFilter`**:`SetInputDataObject(断面2D vtkImageData)` + `SetValue(0, thresholdValue)` + Update → 提出断面上该强度的等值线(可能多条,polydata 线集)。
2. **`vtkPolyDataConnectivityFilter`**:`SetExtractionModeToClosestPointRegion()` + `SetClosestPoint(seedPoint)` → 从多条等值线里**选离种子点最近的那条连通分量**。**种子约束是不出板砖、选中血管腔那圈的关键。**
3. `GetOrderedPtIDs(lines, ifClosed)`:把线段排成有序点序列 + 判闭合。
4. 阈值默认从断面直方图估计(避免 0/255 无效默认;SV 有 preset)。

### XQ 实现要点(层次归属见 §4)

- **算法放 services/segmentation**(纯域,可 headless 测):`ContourExtractionService` 加 `thresholdContour(断面2D像素+阈值+种子(u,v)) → 有序闭合 2D 轮廓点`。**但纯域层不能引 VTK**(vtkContourFilter 是 VTK)——两个选择,派前和用户/design 定:
  - **选项 A(推荐,守分层)**:services 层自己实现 2D marching squares(等值线追踪)+ 种子连通选择(纯 C++,可 headless 测,不引 VTK)。算法不复杂(断面是规则网格)。
  - **选项 B**:等值线提取放 visualization 层(用 vtkContourFilter/vtkPolyDataConnectivityFilter,照 SV),services 只留参数估计。破一点分层,但直接复用 SV 同款 VTK 滤镜。
  - 主审倾向 A(headless 可测 + 守分层铁律;marching squares 在规则断面网格上是初等算法)。**派 worker 前定死选哪个。**
- **种子来源**:断面中心 (u,v)=(0,0) = pose.origin = 沿路径采样时路径穿过的血管腔中心,天然是好种子。第一版用断面中心当种子(无需用户点)。后续可加手动点种子。
- **阈值估计**:断面像素直方图,默认取血管高信号带(照 B6c 经验 P90..max,memory `xq-threshold-seg-fullblock-bug`);给滑条让用户调。
- **入组链完全复用 P3-2**:阈值产出的断面 2D 轮廓点 → `unprojectFromFrame(lastFrame, u, v)` → world → `XQContour{type=ThresholdResult}` → `addContour`。**这条链 P3-2 已打通验证过,阈值只是换了「2D 点怎么来」的前半段。**

### 验收(离散 + 真机)

- **离散**(命门,可证伪):构造合成断面(中心高信号圆斑 + 背景噪声)→ thresholdContour 提取轮廓应**闭合**且**面积≈圆斑**(不吞背景=不出板砖)。**可证伪:篡改成全断面阈值/去掉种子约束 → 面积暴涨必转红。**照 design.md §5.2 + §9。
- **真机(本任务主里程碑,含 P3-2 主链一并验)**:真实工程沿路径滑动 → 点「阈值」→ 断面上自动描出贴合血管腔一圈(不出实心块)→ 沿路径放若干 → Loft 出血管管。**这是 P3-2+P3-3 合并的真机门禁**,GUI ctest 全绿≠达标(memory `gui-task-green-tests-not-done`)。

## 3. P3-2 已交付、本批直接复用的地基(commit 53f3c1a)

- **断面工作台**:`XQCrossSectionViewWidget`(断面视图,vtkImageReslice 常驻管线,ParallelProjection)、`XQCrossSectionResampler`(x=normal/y=binormal/z=tangent,40mm@0.2mm/px=200×200)。断面输出局部 (lx,ly) mm = 断面 2D (u,v),中心=(0,0)=pose.origin。`lastFrame()` 探针给断面 frame。
- **断面 2D→world 链**:`core/XQContourGroup.h` `unprojectFromFrame(frame,u,v)=origin+u·xAxis+v·yAxis`。P3-2 的 `XQMainWindow::addDrawnContour`(:3329)已是范本:service 产 2D 点 → unproject → XQContour 入组 → 放样。**阈值入组照抄这个函数,只换 `pts2d` 的来源(circle/polygon → thresholdContour)。**
- **绘制交互框架**:`XQCrossSectionViewWidget` 的 eventFilter + 空 style 接管(`engageDrawStyle`)、`displayToSection`(像素→(u,v))、常驻 line actor 预览/叠加(`setDisplayedContours`)、`contourDrawn` 信号。阈值是「点一下自动描」,交互比手绘简单(可能只需一个「在此断面执行阈值」按钮,或点种子)。
- **纯域 service**:`services/segmentation/ContourExtractionService.{h,cpp}` 已存在(圆/多边形),阈值方法加进去。`ContourPoint2D{u,v}` 已定义。
- **放样链**:`ContourLoftInputBuilder` → `ModelingService::loftSurface`。`contourGroupProvider`(XQMainWindow.cpp:858)已改为识别内存组 `XQContourGroupPayload` 直接放样(P3-2 修的),阈值组同样能喂。
- **活动轮廓组**:`XQMainWindow::activeContourGroup_` + `createContourGroupFromPicker`(选路径建组)+ `bindCrossSectionPath`(读活动组绑定路径)。已就位。

## 4. 层次纪律(P3-3 关键)

- `core`(无 VTK/Qt):XQPath/XQContourGroup/ContourFrame。不动。
- `services/segmentation`(**无 VTK/Qt**):`ContourExtractionService` 加 `thresholdContour`。若走选项 A(marching squares 纯 C++)就放这;选项 B 的 VTK 等值线部分不能放这。
- `visualization`(VTK 私有):XQCrossSectionViewWidget。若选项 B,vtkContourFilter 逻辑放这。头保持 VTK-free。
- `app`(接线):XQMainWindow 阈值方法按钮、执行阈值命令(取断面像素→service→入组,照 addDrawnContour)、阈值滑条。
- 写代码前先读对应 layer 的 spec:`.trellis/spec/XQ/{core,services,visualization}/index.md`。断面阈值算法参照 design.md §5.2。

## 5. channel worker 流程(踩过的坑,务必遵守)

- **spawn 绝不用 `--file`/`--jsonl` 挂文件**——会让 worker 首轮卡死超时(memory `channel-spawn-file-jsonl-hangs-worker`)。spawn 只 `--agent implement --as impl --cwd C:/Users/OCEAN/Desktop/XIAOQUAN --timeout 90m`;然后 `channel send --to impl --as main` 用 `--text-file <简报路径>` 把简报+文档绝对路径给 worker 让它 Read。`send` 无 `--kind`/`--text`(那是 wait 的);正文是位置参数或 `--text-file`。**注意 worker 回报可能用 `SendMessage` 工具或直接正文回复,主会话读 channel messages --raw 拿报告。**
- worker cwd 设**主仓**(能找 agent 定义与 spec);代码文件用 **worktree 绝对路径 CODE_ROOT**。两个不同 git 工作树别混。
- **判 worker 死活**:`channel list --all` 看 WORKERS(`1/1`活/`0/1`死)+ EVENTS 增长;别信 `channel wait` 空退=完工,先 cat wait 输出确认 `turn_finished`(不是 turn_ended)。wait 参数:`--as main --from impl --kind turn_finished`。channel messages **不吃 `--as`**,用 `--raw --last N`。
- **worker turn 可能在构建没跑完时就 done**。**done≠验证完成≠工作树干净**。主审(你)必接管全部验证:`rm -rf build_gui` 全新构建 + 全量 ctest + **亲手假绿抽查**(篡改被测逻辑真转红,别只信 worker 报告)+ 亲读最终代码(memory `subagent-mainreview-must-read-code`)+ git diff 核残留(worker/check 抽查篡改可能留在树上)。
- 派 check worker 复验(implement→check→spec→commit);check 也做独立假绿抽查(选与主审不同的命门)。check 抽查会短暂改代码再还原,**认账前 git diff 核它还原干净**。
- 完工后 `channel rm <chan>` 清理(名额上限 6)。清理临时简报文件。
- P3-2 实测:两个 worker 都表现好,报告诚实。impl ~47min,check ~12min。

## 6. 构建/测试/提交(硬规范)

- 全新构建(改 Q_OBJECT 头必须,memory `ninja-stale-moc-gui-crash`):`rm -rf "CODE_ROOT/build_gui"` 然后 `cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"`。增量:直接跑不删目录。**当前基线全新构建 248/248。** vcvars64 冷启动偶挂死→换 DevShell(memory `vcvars64-coldstart-hangs-use-devshell`)。
- ctest:`cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"`(**当前基线 72/72 全绿**,新增阈值离散测试后基线上调)。**必走 ctest,裸跑 test exe 缺 VTK DLL exit 127**(memory `ctest-environment-overrides-path`)。
- **假绿抽查**:每个新断言篡改被测逻辑→走 ctest 真转红(**核 exe 时间戳变**,memory `ninja-target-incremental-fakegreen-trap`)→还原复绿。
- **真机**:`cmd //c "...\run_xq.bat"`,用户用 `! run_xq.bat` 自己开;**主会话读不出本地截图**(memory `harness-cannot-read-local-images`),真机判定靠用户。**派用户真机前必核 exe 时间戳新过所有改动源**(memory `gui-realmachine-test-verify-exe-timestamp`)。
- i18n:手工编辑 `resources/i18n/xq_zh_CN.ts`(UTF-8 无 BOM+LF)不跑 lupdate(memory `xqtr-translate-lupdate-blind`);面板串走 `xqTr`=translate("XQStageWidgets",..);主窗口串走 `tr()`(context xq::XQMainWindow)。`lrelease xq_zh_CN.ts -qm xq_zh_CN.qm` 报 0 unfinished。**当前基线 258 finished。**
- 新源文件注释一律**英文**(MSVC 无 /utf-8 无 BOM,中文注释按 GBK 误读=假语法错,memory `msvc-gbk-chinese-comment-syntax-error`)。
- 提交:精确 `git add <路径>` **绝不 `-A`**(worktree 有 `xq_app_dist/` + 上级一堆历史 md 不能进库,memory `commit-check-gitignore-present`);中文信息「类型: 描述」;结尾 `Co-Authored-By: Claude Fable 5 <noreply@anthropic.com>`。worktree 代码提 `feat/render-arch`;spec/任务文档提主仓 `fix/xq-global-audit`。**真机验证过再 commit(GUI 任务真机第一门禁)。**

## 7. 新会话接手动作

1. 读本文件(已读)。
2. 读方向文档 `.trellis/tasks/07-06-along-path-contouring/research/sv-path-seg-mesh-methods.md`(SV 方案清单,已定方向)+ 父任务 `design.md §5.2`(断面阈值设计)+ `implement.md`(P3-3 原计划——注意原 P3-3 含椭圆/样条,按新方向椭圆/样条降级或砍,阈值提为主线)。
3. **决策点**:阈值算法走选项 A(services 纯域 marching squares)还是选项 B(visualization vtkContourFilter)——见 §2,主审倾向 A。派 worker 前定死。可问用户或按 A 走。
4. 需要时读 P3-2 交付代码(§3 列的文件,尤其 `addDrawnContour`/`ContourExtractionService`)。
5. SV 阈值算法参照 §2 已给全(vtkContourFilter + ClosestPointRegion 种子约束),不用再读 SV。若要区域生长/水平集(本批可能顺带或留下批),再亲读 `sv4gui_SegmentationUtils.cxx` CreateLSContour(:701)。
6. 写 EXECUTE-P3-3 简报(照 EXECUTE-P3-2.md 格式,自包含,含选定算法方案/接口/离散测试/验证/禁止/交付物)→ 派 channel worker → 主审接管验证 → check worker → spec → **真机(P3-2+P3-3 主链合并验)** → commit。

## 8. 本任务链已沉淀的 memory(新会话已自动加载索引)

- `sv-seg-auto-primary-manual-fallback`(本轮新增)— SV 三阶段自动为主手绘兜底,分割优先级 水平集/阈值/ML>手绘,XQ 优先落地断面阈值。
- `sv-research-agent-cyber-falsepositive` — 读 SV 分割源码主会话亲读别派 agent。
- `xq-threshold-seg-fullblock-bug` — 整卷阈值出板砖=缺种子约束;断面阈值靠种子(ClosestPointRegion)选血管腔那圈。
- `gui-task-green-tests-not-done` / `render-architecture-must-be-validated-first` — GUI ctest 全绿≠达标,真机第一门禁。
- `channel-spawn-file-jsonl-hangs-worker` / `subagent-mainreview-must-read-code` / `gui-realmachine-test-verify-exe-timestamp` — worker 流程 + 主审验证纪律。
- `xq-seed-pick-eventfilter-vtk-race` — 断面交互空 style 显式接管(P3-2 已用,阈值点种子若需交互照此)。
