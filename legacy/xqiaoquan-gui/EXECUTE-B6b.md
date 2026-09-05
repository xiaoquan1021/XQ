# EXECUTE-B6b — 分步引导条 + tooltip 补齐 + 阈值估计 + Ctrl+A/Esc/模式浮层

> 活动任务:`07-05-workflow-usability`(B6b 批,P2 引导)。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD = 6809589,B6a 已提交)。
> 纪律:工具调用必须真正以工具形式发出;完成报告纯文本收尾。
> **已知坑**:①本批改 XQMainWindow.h 与 XQSliceViewWidget.h(Q_OBJECT)→ 最终 `rm -rf build_gui` 全新构建;②.bat 绝对路径 `cmd //c "C:\...\build_gui_wt.bat"`;③LNK1104 先 taskkill xq_app;④ts 用 Edit 工具手工改、保持 UTF-8 无 BOM+LF、勿跑 lupdate;⑤新注释英文;⑥测试断言必须可证伪(B6a 教训:combo 默认选 index 0 掩蔽预选缺失——断言前先把状态 park 到反面)。

## 背景

用户反馈「工具指引性太弱、操作复杂、不会用」。B6a 已消灭 NodeId 手填;本批补引导层:每页分步引导、参数 tooltip、阈值估计、快捷键与模式可视化。SV 对照见任务 research/sv-workflow-comparison.md §2(tooltip 密度、Estimate 按钮、Ctrl+A、变蓝模式指示的教训——XQ 要做得更显式)。

## 源码事实(行号以 6809589 为准;个别行号因 B6a 有 ±10 漂移,以 grep 为准)

- **hint 机制**:`makeHintBar(page, name, message)` XQStageWidgets.cpp(B6a 前 :128-137,匿名 ns):QLabel 琥珀样式,objectName `xqStageHint_<name>`,构建期一次性文本,全文件无 setText 更新。有 hint 的页:Path/Seg/Modeling/Flow;**Meshing/AI 无**。
- **status 标签**:`makeStatusLabel`(:105-114,`xqStageStatus_` 前缀)每页都有,run/done 回调 setText——与 hint 两套,别混。
- **tooltip**:全文件 setToolTip 数 = 0。控件清单(B6a 后名字):
  - Path:nameEdit、pickToggle、pointList、spacingSpin;
  - Seg:thresholdRadio、regionGrowRadio、nameEdit、lowerSpin、upperSpin、pickSeed;
  - Modeling:nameEdit、sourceCombo、capEnds;
  - Meshing:nameEdit、sourceCombo、surfaceRadio、volumeRadio;
  - Flow:nameEdit、sourceCombo;
  - AI:nameEdit、sourceCombo、muSpin、ffrSpin、refPressureSpin。
- **阈值**:Seg 页 `lowerSpin`/`upperSpin`(QDoubleSpinBox,range ±100000,默认 0/255,无估计逻辑);`imageProvider`(无参 `ActiveImage()`,XQStageWidgets.h:47)在 run 时取活动图像。ActiveImage 结构先读 h(有 image 指针/buffer 与 valid 标志——照 Seg run lambda 的现用法)。体素访问方式照 XQMainWindow.cpp 里 seed/直方图类现有用法 grep(`XQDemoVolume`/`XQImageVolume` 的强度读取,如 window/level 初始化处),**先读再写,别猜 API**。
- **pick 模式**:`PickMode {None,Seed,PathPoint}` XQMainWindow.h:403-404;进入/退出:`seedPickingSetter`(cpp:752-769)/`pathPickingSetter`(cpp:786-803),互斥已做;voxelPicked 路由 cpp:322-359(PathPoint→pathDraftPoints_ push + pathDraftChanged_();其余落 seed 分支)。**现状零视觉指示、零快捷键**(仅 PageUp/Down cpp:410-413 先例)。
- **十字线位置**:`renderScene_->sliceIndex(axis)`(axis 0=x/Sagittal,1=y/Coronal,2=z/Axial;无体数据返回 -1)。voxel(i,j,k) 与 axis 的对应:i=x,j=y,k=z(照 voxelPicked 分支现有用法)。
- **浮层挂点**:XQSliceViewWidget 已有角标 overlay(corner info QLabel,objectName 见 infoObjectName,cpp:46 附近)——模式浮层照同款布局方式加第二个 QLabel。XQMprWidget 持三个 XQSliceViewWidget(mount 结构先读 XQMprWidget.cpp)。
- **stage 页取控件**:测试用 findChild(objectName);引导条更新函数在页内闭包直接持有指针,不需 findChild。
- **i18n context**:面板串 xqTr→"XQStageWidgets";XQMainWindow 串 tr→"xq::XQMainWindow";XQSliceViewWidget 若加 tr 串→"xq::XQSliceViewWidget"(新 context,ts 里新建);模式浮层文案若由 XQMainWindow 组好传入(推荐,少一个 context)则全落 xq::XQMainWindow。

## 改法

### 1. 分步引导条(XQStageWidgets.cpp)
- `makeHintBar` 保留;每页构建处把 hint 指针捕获进一个页内 `updateGuidance` lambda(std::function 存页内,或直接多处调用),按该页 run-enable 同源条件生成文本:
  - Path(3 步):无图像→"Step 1/3: Open an image (File > Open Image)";图像在、拾取关→"Step 2/3: Toggle point picking, then click in a slice view (Ctrl+A adds at the crosshair)";点 <2→"Step 2/3: Add at least 2 points (%1 so far)";≥2→"Step 3/3: Name the path and press Generate Path";
  - Seg(2 步):无图像→"Step 1/2: Open an image first";有→"Step 2/2: Set thresholds (try Estimate) or pick a seed, then run";
  - Modeling(2 步):combo 空→"Step 1/2: Create a contour group first (Segmentation stage)";有→"Step 2/2: Pick a contour group and press Loft";
  - Meshing(**补 makeHintBar**,2 步):combo 空→"Step 1/2: Create a surface model first (Modeling stage)";有→"Step 2/2: Pick a model, choose surface/volume, press Build Mesh";
  - Flow(2 步):同构(case 空/有);
  - AI(**补 makeHintBar**,2 步):同构(flow result 空/有)。
- 触发点 = 与 run-enable 更新完全同点位(pickToggle toggled、point list 刷新、imageProvider 状态变化处、combo currentIndexChanged、done 回调),**不加轮询/定时器**。combo 空判断用 `combo->count()==0`(构建时 repopulate 过;done 回调再调一次 updateGuidance 前先 repopulate,保证跨阶段推进)。
- 步骤文案全 xqTr;hint objectName 不变(测试锚点)。

### 2. tooltip 补齐(XQStageWidgets.cpp)
上面清单 ~24 控件逐个 setToolTip(xqTr,一句话:含义+单位+建议值;SV 文案密度)。新增控件(estimateBtn 等)一并带。报告贴 setToolTip 总数。

### 3. 阈值估计按钮(XQStageWidgets.cpp,Seg 页)
- 阈值行旁加 `QPushButton* estimateBtn`(xqTr("Estimate"),objectName `xqSegEstimateBtn`)。
- clicked:`imageProvider()` 无效→status "Open an image first.";有效→对体素**跨步采样**(stride = max(1, total/1'000'000))收集强度,nth_element 取 P25/P75(**先读现有强度访问 API 再定**;窗宽窗位初始化处有先例),`lowerSpin->setValue(p25); upperSpin->setValue(p75);` status 报 "Estimated from image intensities."。不锁用户后续修改;GUI 线程直算(≤1M 样本可承受,与 SV Estimate 同步先例一致)。
- 百分位选择写死 P25/P75(经验值,报告注明;别做成可配置)。

### 4. Ctrl+A / Esc / 模式浮层(XQMainWindow + XQSliceViewWidget/XQMprWidget)
- **Ctrl+A**(XQMainWindow 构造,照 PageUp 先例):`pickMode_==PathPoint` 且有体数据 → 取 `renderScene_->sliceIndex(0/1/2)` 为 (i,j,k),**复用 voxelPicked 的 PathPoint 分支**(提公共私有函数 `addPathDraftVoxel(int i,int j,int k)`,voxelPicked 与快捷键都调它);其它模式 no-op。
- **Esc**:`pickMode_!=None` → 调对应 setter(false)。注意 setter 是 buildStagePanel 里的 lambda——把「退出当前拾取态」提成私有方法或让快捷键直接复用 seedPickingSetter/pathPickingSetter 存下的 std::function(实现自选,报告写清);页面 toggle 按钮状态必须同步复位(setter 现状已做?先读——pathPickingSetter 里有对 toggle 的 setChecked 吗?若没有,通过存 toggle 指针或 findChild 同步,别让按钮残留 checked)。
- **模式浮层**:XQSliceViewWidget 加公有 `void setModeHint(const QString& text)`(空→hide;非空→show),实现为半透明底 QLabel(照角标 overlay 同款定位,放视图顶部居中或左上,styleSheet 半透明深底白字,自动 adjustSize);XQMprWidget 加同名转发(三切片格)。XQMainWindow 在两个 pickingSetter 里进入时 `mprWidget_->setModeHint(tr("Picking mode: click a slice to add a point (Ctrl+A adds at the crosshair, Esc exits)"))`(seed 版同构文案),退出时空串。
- objectName:浮层 QLabel `xqSliceModeHint`(测试锚点)。

### 5. i18n
新串:引导条各步、~24 tooltip、Estimate 相关、两条浮层文案、（若有）快捷键相关。context 归属见上。手工 ts + lrelease,报告 finished/unfinished。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 修改 | `src/ui/panels/XQStageWidgets.cpp`(§1 §2 §3;.h 仅当需要暴露新类型时,预计不用)|
| 修改 | `src/app/XQMainWindow.{h,cpp}`(§4 快捷键+浮层接线)|
| 修改 | `src/visualization/XQSliceViewWidget.{h,cpp}`、`src/visualization/XQMprWidget.{h,cpp}`(§4 浮层)|
| 修改 | `resources/i18n/xq_zh_CN.ts` + lrelease `.qm` |
| 修改 | `tests/app/test_main_window.cpp`、`tests/ui/test_path_stage.cpp` 等受影响测试(先 grep 申报)|

**不动**:core/services/io;XQRenderScene;controllers;XQWorkflowSession;B6a 的 combo 逻辑。

## 测试(新增,全部要可证伪)

1. **引导推进**(test_main_window 或 test_path_stage,选现有架构方便处):Path 页 hint 文本随状态变化——无图像时含 "Step 1"、加载图像后含 "Step 2"、appendPathDraftPointForTest ×2 后含 "Step 3"(断言前记录前值,确认文本**变了**再查内容,防一次性文本假绿);
2. **估计按钮**:合成图像(现有测试卷先例)→ 点击 estimateBtn(findChild)→ lowerSpin/upperSpin 值**偏离 0/255 默认**且 lower<upper 且落在图像强度范围内;
3. **Ctrl+A**:进入 path 拾取态(现有 pickToggle 测试先例)→ 模拟 Ctrl+A(QTest::keyClick 或直接调提出来的 addPathDraftVoxel——若快捷键离屏触发不可靠,调私有测试入口并报告说明)→ draft 点数 +1 且坐标 == 当前 sliceIndex 三元组;
4. **Esc**:拾取态下 Esc → pickMode 复位(探针:toggle unchecked / setModeHint 浮层 hidden);
5. **浮层**:进入拾取 → findChild `xqSliceModeHint` visible 且文本非空;退出 → hidden。

## 验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rm -rf build_gui
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"   # 全量全绿
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\run_xq.bat"        # 冒烟
```

假绿抽查两项(必做,exe 时间戳核对):
1. 篡改 updateGuidance(恒写 Step 1 文本)→ 引导推进断言转红 → 还原绿;
2. 篡改 estimateBtn(clicked 不 setValue)→ 估计断言转红 → 还原绿。

## 禁做

白名单外文件;不加轮询/定时器;不做可配置化(百分位/快捷键/文案);既有断言只做申报过的等价适配;强度 API 拿不准先读代码再写、仍不确定停下报告;不 commit;报告纯文本收尾;冲突/存疑停下等裁决。

## 完成报告格式

1. 文件清单;2. 全新构建+全量 ctest 总结行原文;3. 两项假绿抽查证据;4. 既有断言适配申报;5. tooltip 总数+引导条文案清单;6. 估计算法说明(采样/百分位/实测值);7. i18n 计数;8. 偏离与存疑。
