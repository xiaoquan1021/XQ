# 执行计划:沿血管中心线逐层轮廓提取

> 语境锚点:XQ 医学影像软件血管几何重建。本文件定分批、验证命令、复审门禁、回滚点。对标 SimVascular sv4gui 分割工作流。

## 0. 执行纪律(每批通用,违反=返工)

1. **channel worker 流程**:主审写 EXECUTE-P3-X.md 简报 → `trellis channel spawn --agent implement` → `--agent check` 复核 → **主审亲读最终代码 + `rm -rf build_gui` 全新构建复跑 ctest + 假绿抽查** → commit → 真机(memory `subagent-mainreview-must-read-code`、`gui-task-green-tests-not-done`)。
2. **done 事件 ≠ 工作树干净**:认账前必 `git diff` 核 worker 残留篡改(memory `worker-handedit-ts-bom-crlf`)。worker 常在验证中途因回合中断提前结束。
3. **改 Q_OBJECT 头必 `rm -rf build_gui` 全新构建**(memory `ninja-stale-moc`)。新增带 Q_OBJECT 的 widget = 改头。
4. **假绿抽查**:每个新断言篡改被测逻辑必须真转红(核 exe 时间戳变)后还原绿。断言可证伪(先 park 反面状态)。
5. **新源文件注释写英文**(MSVC GBK 坑,memory `msvc-gbk`)。
6. **i18n 手工编辑 .ts,绝不 lupdate**(memory `xqtr-translate-lupdate-blind`);lrelease 报 0 unfinished。
7. **worker 名额上限 6**:批完 `trellis channel rm <name>` / `prune --idle 30m --yes`。
8. **精确 git add**,绝不 `-A`/`.`(memory `commit-check-gitignore`)。
9. **架构先验收再盖楼**:P3-1 断面重采样是新渲染能力,真机验基线体验对了再往下,别让 headless 可测性反向决定架构(memory `render-architecture-must-be-validated-first`)。
10. 每批只在 worktree `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`(分支 `feat/render-arch`)改代码。

## 1. 验证命令(固定)

- 全新构建:`cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"`(改 Q_OBJECT 头先 `rm -rf build_gui`)。
- ctest:`cmd //c "C:\...\XQ\ctest_merge.bat"`(基线 68 全绿;新增测试后基线上调,记进 EXECUTE 简报)。
- 冒烟/真机:`cmd //c "C:\...\XQ\run_xq.bat"` 打开真实工程目视。
- i18n:改 .ts 后 `lrelease xq_zh_CN.ts -qm xq_zh_CN.qm` 应报 N finished / 0 unfinished。
- Python:`C:/software/anaconda/python.exe`。

## 2. 分批(依赖顺序,每批独立 implement→check→主审→真机→commit)

### P3-1 断面重采样视图 + 沿路径滑条(技术核心 · 先验基线)

**目标**:vtkImageReslice 按 PathSamplePoint 坐标系出 2D 断面,专用断面工作台显示,滑条沿路径驱动。**这批只要断面能正确显示 + 滑条能滑,不含轮廓生成。**

编辑范围:
- 新增 `visualization/XQCrossSectionResampler.{h,cpp}`(vtkImageReslice 封装,VTK-free 头 pimpl 或 opaque)。
- 新增 `visualization/XQCrossSectionViewWidget.{h,cpp}`(挂 reslice slice + 断面 vtkRenderer;本批只显示断面 + 窗位联动,无绘制交互)。
- 断面工作台容器(QStackedWidget 或主视图容器)+ 沿路径滑条(app 或 ui/panels)。
- app 接线:阶段进入/退出切换工作台;滑条值 → resampler → 视图刷新。

验收(离散 + 真机):
- 离散(命门):`test_cross_section_resampler` —— 已知 PathSamplePoint → 输出 frame == 预期 ContourFrame;断面中心像素反投影回世界 == path position(容差)。可证伪:篡改 ResliceAxes 轴序必转红。
- **真机(第一优先)**:真实工程 `0080_H_PULM_H` 选一条路径 → 滑条移动 → 断面视图实时显示垂直断面,不卡顿。**这批真机不过关不往下。**

回滚点:P3-1 是纯新增 + 阶段切换,不改现有四视图渲染路径;回滚 = revert 本批 commit。

### P3-2 轮廓组绑路径 + 手绘圆/多边形(端到端 MVP)

**目标**:打通「选路径建组 → 滑条定位 → 断面手绘圆/多边形 → 入组 → 手动放样出血管」。

编辑范围:
- 轮廓组创建对话框加「选择路径」下拉,`setSourcePathNode`(ui/panels + app)。
- 新增 `services/segmentation/ContourExtractionService.{h,cpp}` 的手绘几何(圆/多边形控制点 → 2D 轮廓点,照 SV Circle/Polygon)。纯域。
- XQCrossSectionViewWidget 绘制交互:点/拖生成控制点 → 预览 → 2D 点 unprojectFromFrame → XQContour 入组命令。
- 方法工具栏(圆/多边形)+ 进入/退出编辑按钮(ui/panels)。

验收:
- 离散:`test_contour_extraction`(圆:点到心距≈r,点数≥36,闭合;多边形:顶点∈轮廓点);`test_contour_group_path_binding`(绑路径、pathArcLength 单调、orderedByPathPosition 有序);放样打通(合成轮廓 → loftSurface 非空,几何守恒)。
- **真机**:真实工程沿一段路径手绘 3~5 个圈 → 建模 Loft → 出可见血管管。**MVP 真机过关是本任务主里程碑。**

### P3-3 手绘椭圆/样条 + 断面阈值自动

编辑范围:ContourExtractionService 加椭圆/样条几何(读 SV Ellipse/SplinePolygon)+ `thresholdContour`(2D 断面阈值 + 连通 + 边界追踪,读 SV ContourModelThresholdInteractor)。断面视图加对应交互 + 阈值参数控件(默认按断面直方图估计)。

验收:离散(椭圆/样条点数闭合;阈值:合成断面圆斑 → 轮廓面积≈圆斑,可证伪:改成全断面阈值必转红);真机(断面阈值描出贴合血管腔一圈,不出实心块)。

### P3-4 断面水平集/区域生长自动

编辑范围:ContourExtractionService 加 `regionGrowContour` + `levelSetContour`(读 SV LevelSet2D)。断面视图加种子拾取 + 参数控件。

验收:离散(合成断面从种子生长出闭合轮廓);真机(从种子演化出闭合轮廓)。

### P3-5 批量 + 多血管 + 放样打磨

编辑范围:app 层批量循环(路径子集 × 当前自动方法 → 逐点入组,进度反馈);多血管(多路径遍历);放样入口顺手化(选轮廓组直达)。

验收:离散(批量对合成路径生成 N 个轮廓,arcLength 覆盖预期采样位);真机(整条路径批量一串轮廓;多路径批量;放样出多段血管)。

### P3-6 显式易用性打磨 + 真机收口

编辑范围:断面视图浮层(方法提示/Esc)、空态引导、进入/退出显式态打磨、i18n 补串。

验收:真机全流程走查(prd §7 全部勾)。ctest 全绿。真机对 `0080_H_PULM_H` + `0007_H_AO_H` 双工程验收。

## 3. 全局回滚

- 每批 commit 独立,可逐批 revert。
- P3-1 是新增 + 阶段切换隔离;P3-2 起改现有对话框/建模链入口,回滚需连带 revert 依赖它的后续批。
- 整任务失败兜底:B6c 止血(整卷高端百分位 + 最大连通域)保留在 HEAD,轮廓提取阶段至少不出实心块。

## 4. 里程碑判据

- **主里程碑 = P3-2 真机**:真实工程沿路径手绘几个圈放样出一段血管。到这里「沿路径描轮廓」方向被真机证实成立。
- P3-3~P3-6 是在成立方向上补齐 SV 全套能力。
- 收口 = prd §7 全部真机勾 + ctest 全绿 + spec 更新 + archive。

## 5. spec 更新(P3 收口时,step 3.3)

- `.trellis/spec/XQ/visualization/`:新增断面重采样契约(XQCrossSectionResampler 的 reslice frame ↔ ContourFrame 一致性、断面工作台落位)。
- `.trellis/spec/XQ/`:轮廓提取工作流从整卷阈值改为沿路径描轮廓(更新分割阶段描述)。
