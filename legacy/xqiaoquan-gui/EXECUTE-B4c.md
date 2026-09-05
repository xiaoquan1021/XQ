# EXECUTE-B4c — 真机反馈四项:路径变细 + 切片满窗 + 十字线中心拖拽 + 大工程加载提速

> 活动任务:`07-04-render-arch-rebuild`(用户 2026-07-05 真机反馈插批)。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD = a7431c7,B4b 已提交)。
> 纪律:工具调用必须真正以工具形式发出;完成报告纯文本收尾。
> **已知坑**:①改 XQMainWindow.h(Q_OBJECT)→ 最终 `rm -rf build_gui` 全新构建;②.bat 绝对路径包 `cmd //c`;③LNK1104 先 taskkill xq_app;④测试参考工程 0007(5 contour);用户实测工程 0080_H_PULM_H(143 path+143 contour,290 节点)只做真机参考,不进测试。

## 四项需求与根因(已核,行号以 a7431c7 为准)

### 1. 路径太粗(比血管模型还粗)
`XQRenderScene.cpp:1361 addPath`:polyline 过 `vtkTubeFilter`(半径 = 包围盒对角线 2%,`estimate_tube_radius` :463——长路径半径巨大)。
**改法**:删 tube 管线,polyline 直接进 mapper,actor `SetLineWidth(2.0)` 细线(文件内 :1487 已有同款先例);`estimate_tube_radius` 若无其它消费者一并删。颜色 (0.95,0.75,0.2) 保留。副收益:143 条路径的 actor 构建省掉 143 次管状化。

### 2. 切片窗口利用率低(图像只占中间一条)
`XQRenderScene.cpp:696 setupSliceCameras`:`view->ResetCamera()`(:727)按整卷包围盒取景+默认边距,宽窗口里竖长图像只占中间窄条。
**改法**:ResetCamera 后手动收紧 ParallelScale——按该视图 up 方向的图像世界尺寸设 `camera->SetParallelScale(0.5 * upExtent)`(纵向精确满窗,无边距;横向由窗口宽高比自然决定,略裁边可接受,SV 同款观感)。三轴的 upExtent 分别取:Axial(up=y)=y 尺寸、Sagittal/Coronal(up=z)=z 尺寸(从 `image_->GetBounds()` 算)。**探针**:`vtkRendererHandle` 已公开(:121),测试可取 GetActiveCamera()->GetParallelScale() 断言。

### 3. 十字线拖拽:只拖中心交点 + 光标不碍事
现状 `XQSliceViewWidget.cpp`:`hitLine`(:337)按单线 8px 容差命中(mask 1/2/3);`applyHitCursor`(:416)mask==3 给 SizeAllCursor(用户嫌大)、单线给 SplitH/SplitV;拖单线动单轴。
**改法**(SV 式,只拖中心):
- `hitLine` 改为**仅当两条线都在容差内**(即靠近交点)返回命中(返回 3),否则 0;单线不再可抓(函数签名/返回语义简化,消费者跟改);
- `applyHitCursor` 大幅简化:命中交点 → `Qt::CrossCursor`(小十字,不碍事;**不用 SizeAllCursor**),未命中 → unsetCursor;SplitH/SplitV 与投影方向判定代码(:426-483)整段删除;
- 拖拽路径 `dragTo`(:381)已支持 mask=3 双轴同动,保留;`draggingLines_` 只会是 0/3;
- seed/path 拾取的 CrossCursor 逻辑(:177/:256)不动。

### 4. 大工程(0080:143 contour)加载比 SV 慢
**根因**(结构性,已核):`startNextParse` 一次一个文件,143 个 .ctgr 串行跑 143 个 taskRunner 任务;**每个 commit 都调 refreshSceneTree()**(:2346)= `sceneModel_->refresh()`(beginResetModel 整树重建,290 节点)+ expandAll + 状态栏 + 全场景 reconcile ——143 轮全量 UI 重建;每任务还翻转一次忙态(光标/动作闪烁 143 次)。SV 是同步一次读完+一次刷新,所以显得快。
**改法(批量合并,机制不换)**:
- `PendingParse` 队列消费改**批量**:`startNextParse` 一次取走**队列里全部同 kind 的连续任务**(Image 仍单独;Model/Contour 各自成批),一个 taskRunner 任务在 worker 线程循环解析整批文件(仍只读文件),commit 在 GUI 线程循环 setPayload 换整批 payload,**整批只调一次 refreshSceneTree()**,然后 kickNext 处理剩余队列;
- 任务标签带计数:`tr("Parsing contours (%1)...").arg(n)`(i18n 新串;Model 同理);
- 单文件失败不废整批:worker 逐文件记录 status,commit 只换成功的,失败逐个状态栏报(或汇总一条,报告写清选择);
- refreshSceneTree 本身不动(B4b 增量 sync 已控制渲染端成本;这批砍的是次数)。
**预期效果**:0080 从 143 轮任务+143 轮全树重建 → ~2 个批任务+2 次刷新。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 修改 | `src/visualization/XQRenderScene.cpp`(§1 §2;不改 .h,无接口变化)|
| 修改 | `src/visualization/XQSliceViewWidget.{h,cpp}`(§3)|
| 修改 | `src/app/XQMainWindow.{h,cpp}`(§4)|
| 修改 | `resources/i18n/xq_zh_CN.ts` + lrelease `.qm` |
| 修改 | `tests/visualization/test_render_scene.cpp`、`tests/app/test_main_window.cpp`(受影响断言先 grep 再适配+新增)|

## 测试

先 grep 现有断言受影响面(test_render_scene 的 path/uploadedPointCount 断言、十字线相关、test_main_window 的 contour 解析等待/taskStarted 依赖),逐条适配申报。新增:
1. **相机满窗**(test_render_scene):setVolume 后对三视图断言 `GetParallelScale() == 0.5*该视图 up 方向图像尺寸`(容差 1e-6;合成卷 8×6×4 尺寸已知);
2. **路径细线**(test_render_scene):既有 path upsert 断言应仍绿(uploadedPointCount 语义不变);若有 tube 特有断言(actor 面片数等)按细线语义改写申报;
3. **批量解析**(test_main_window):连接 `taskRunner taskStarted` 计数——加载 0007 工程(1 image+1 model+5 contour)后,解析任务总数 ≤ 3(image 1 + model 批 1 + contour 批 1;现状是 7)且全部 contour 照旧 resolve(既有 10s 等待断言保持绿);
4. **十字线中心命中**(若现有测试覆盖 hitLine 则适配;offscreen 下 hitLine 早退 return 0,大概率无既有覆盖——报告确认)。

## 验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rm -rf build_gui
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"   # 全量 68 全绿
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\run_xq.bat"        # 冒烟
```

假绿抽查两项(必做,exe 时间戳核对):
1. 篡改 ParallelScale 收紧逻辑(不设,保留 ResetCamera 默认)→ 相机断言转红 → 还原绿;
2. 篡改批量消费(退回一次一个)→ taskStarted 计数断言转红 → 还原绿。

## 禁做

白名单外文件;不动 core/services/io;不改 XQRenderScene.h/XQTaskRunner/XQWorkflowSession;不做"可配置线宽/半径"这类未要求的扩展;既有断言只做申报过的语义等价改写;不 commit;报告纯文本收尾;冲突/不确定停下等裁决。

## 完成报告格式

1. 文件清单;2. 全新构建+全量 ctest 总结行原文;3. 两项假绿抽查证据;4. 断言适配申报;5. i18n;6. 偏离与存疑(尤其:§4 失败聚合策略、§2 三轴 upExtent 的实际取值)。
