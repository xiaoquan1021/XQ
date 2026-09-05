# EXECUTE-B6c — 阈值分割板砖修复:Estimate 改高端百分位 + 阈值路径接最大连通域

> 活动任务:`07-05-workflow-usability`(B6c 补丁批,用户真机反馈 P0 bug)。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD = 21a9f3f,B6b 已提交)。
> 纪律:工具调用必须真正以工具形式发出;完成报告纯文本收尾。
> **已知坑**:①本批不改 Q_OBJECT 头(只改 .cpp),但最终仍 `rm -rf build_gui` 全新构建保险;②.bat 绝对路径 `cmd //c`;③ts 用 Edit 工具改、UTF-8 无 BOM+LF、勿跑 lupdate;④新注释英文;⑤断言可证伪(park 反面再断言,B6a/B6b 教训)。

## 背景(用户真机反馈 + 三方定位)

用户对 0080_H_PULM_H 跑「阈值分割」,输出一整块实心矩形板砖(3D 视图),不是血管。两个独立 bug-hunt agent + 定量实测(真实图像 OSMSC0082,100×512×512)+ 主会话亲读 SV 源码,根因铁定:

1. **主因**:B6b 的 Estimate 按钮用**全图 P25/P75**当阈值。造影血管是**高信号**,P25/P75 覆盖的是中间强度=软组织。实测该图 P25=28/P75=236(=用户 UI 的 [28,236]),选中**全卷 50.3%(1318 万体素)**=那块板砖;真血管在 P95(715)以上只占 5%、P99 占 1%。**血管阈值该取分布高端**。
2. **加剧因**:`thresholdMask`(SegmentationService.cpp:126-131)全卷逐体素无连通约束,即使阈值对,细血管掩膜仍混入全卷散点。
   SV 对照:`sv4gui_Seg3DUtils::collidingFronts` 阈值只是预处理,选区靠 CollidingFronts+两组种子(种子约束+连通),结构上不出板砖。

本批修 P0(Estimate 百分位)+ P1(阈值路径接最大连通域)。P3 沿路径 reslice 分割仍单独立任务。

## 源码事实(已核,行号以 21a9f3f 为准)

- **Estimate lambda**:`src/ui/panels/XQStageWidgets.cpp:668-706`。现状 :697-702 取 `p25=size/4`、`p75=size*3/4`,nth_element 求值,`lowerSpin->setValue(lower); upperSpin->setValue(upper)`。采样是全卷跨步(:684-690,stride 保 ≤1M 样本)。
- **阈值控制器**:`src/ui/controllers/SegmentationController.cpp:33-62 prepareThreshold`。:46-47 调 `SegmentationService::thresholdMask(...)` 拿 result.mask,:52 直接 createMaskNodeCommand 建节点。**中间没有连通域后处理。**
- **现成接口**:`SegmentationService::keepLargestConnectedComponent(const XQSegmentationMask&)`(SegmentationService.h:97,返回 Result{status,mask};src/services/segmentation/SegmentationService.cpp:221 已实现,6 连通取最大分量,保留 geometry/source/labels)。
- **阈值参数**:`XQSegmentationThresholdParameters{lower,upper,foregroundLabel}`(SegmentationService.h:23)。lower/upper 是 double 原始强度(scalarAt 无归一化)。
- **现有 Estimate 测试**:`tests/app/test_main_window.cpp:894-920`,断言 Estimate 后 `lower<upper` 且偏离 0/255 默认。

## 改法

### P0 — Estimate 改高端百分位(XQStageWidgets.cpp:695-704)
把 P25/P75 换成**血管高信号带**:
- `lower` = 高分位(建议 **P90**,`p90 = samples.size() * 9 / 10`);
- `upper` = 采样最大值(`*std::max_element` 或排序后末元素;即"取高于 P90 的全部亮体素")。
- nth_element 求 p90 后取 `samples[p90]` 为 lower;upper 取整段最大(单独 max_element,别复用被 nth_element 破坏顺序的容器——先 max 再 nth_element,或用两次 nth_element)。
- 注释改成英文事实说明:"contrast vessels are high-signal; seed the lower bound at the 90th percentile and the upper bound at the max so Estimate targets the bright lumen, not mid-intensity tissue"。
- status 文案沿用 "Estimated from image intensities." 或更明确 "Estimated vessel band (P90..max)."(新串则加 ts)。
- **不做**可配置百分位(不加 UI);P90 写死(报告注明依据:实测 P95 选 5%、P99 选 1%,P90 给一点余量,是"亮血管带"量级,远小于 P25/P75 的 50%)。

### P1 — 阈值路径接最大连通域(SegmentationController.cpp prepareThreshold)
- :46-47 拿到 `result`(thresholdMask)后、:52 建节点前,插一步:
  ```cpp
  SegmentationService::Result largest =
      SegmentationService::keepLargestConnectedComponent(*result.mask);
  if (!largest.ok() || largest.mask == nullptr) { prepared.status = Status::Rejected; return prepared; }
  ```
  然后用 `largest.mask` 建节点(createMaskNodeCommand 的第 4 参从 `result.mask` 换 `largest.mask`)。
- **只改阈值路径**(prepareThreshold);regionGrow 路径本就有种子+连通,不动;aiSegment 不动。
- 注释:英文,说明"threshold selects every in-band voxel volume-wide; keep only the largest connected component so a vessel threshold yields the vessel, not scattered tissue speckle"。
- 边界:keepLargestConnectedComponent 对空前景返回空 mask(源实现 :292 注释),空 mask 建节点行为不变(现状 thresholdMask 空前景也建空 mask 节点;保持一致,不额外报错)。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 修改 | `src/ui/panels/XQStageWidgets.cpp`(P0 Estimate 百分位)|
| 修改 | `src/ui/controllers/SegmentationController.cpp`(P1 prepareThreshold 接连通域)|
| 修改 | `resources/i18n/xq_zh_CN.ts` + lrelease `.qm`(仅当新增 status 串)|
| 修改 | `tests/app/test_main_window.cpp`(Estimate 断言强化)、`tests/ui/controllers/WorkflowControllerTest.cpp` 或 `tests/services/segmentation/SegmentationServiceTest.cpp`(阈值+连通域,先 grep 现有阈值测试落点)|

**不动**:SegmentationService 的算法本体(thresholdMask/keepLargestConnectedComponent 都已存在且正确,只在 controller 里串联);regionGrow/aiSegment 路径;渲染层;core。

## 测试(可证伪)

1. **Estimate 高端**(test_main_window,强化现有 :894-920):合成一个**已知高信号小区 + 大片中低背景**的图像(或用现有 SV 工程图像),点 Estimate 后断言:①`lower > upper` 为假(lower<upper 仍成立);②**lower 落在分布高端**——构造已知直方图卷时,断言 lower ≥ 某个已知高分位值,且**前景占比远小于 50%**(可经 controller 跑一次 threshold 数 labelAt!=0 占比 < 某阈值如 20%,证明不再是板砖);park 对照:若把百分位篡改回 P25/P75,前景占比断言必须转红。
2. **阈值接连通域**(service/controller 测试):构造一个含**两个不连通前景块**(大块+小块)的合成图像 → prepareThreshold → 断言结果 mask 只含大块(小块被 keepLargestConnectedComponent 去掉);篡改(不接连通域)→ 断言转红。先 grep 现有阈值测试(SegmentationServiceTest 里应有 thresholdMask 直测),照其合成图像先例加。
3. 既有断言适配逐条申报。

## 验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rm -rf build_gui
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"   # 全量全绿
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\run_xq.bat"        # 冒烟
```

假绿抽查两项(必做,exe 时间戳核对):
1. 篡改 Estimate(退回 P25/P75)→ 前景占比/高端断言转红 → 还原绿;
2. 篡改 prepareThreshold(不接 keepLargestConnectedComponent)→ 两块合成图断言(只留大块)转红 → 还原绿。

## 禁做

白名单外文件;不改 thresholdMask/keepLargestConnectedComponent 算法本体;不动 regionGrow/ai 路径;不做可配置百分位/UI 扩展;既有断言只做申报过的等价强化;不 commit;报告纯文本收尾;冲突/存疑停下等裁决。

## 完成报告格式

1. 文件清单;2. 全新构建+全量 ctest 总结行原文;3. 两项假绿抽查证据;4. 既有断言适配申报;5. P0 百分位实测值(合成图/SV 工程图 Estimate 后 lower/upper 与前景占比);6. i18n(若有);7. 偏离与存疑。
