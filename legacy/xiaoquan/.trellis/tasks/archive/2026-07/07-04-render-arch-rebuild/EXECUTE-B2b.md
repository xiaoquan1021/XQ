# EXECUTE-B2b — seed/路径点拾取恢复 + 窗宽窗位数值框 + i18n 欠账(M2 收尾批)

> 活动任务:`07-04-render-arch-rebuild`。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD 12894e4 = B2d)。
> 纪律:工具调用必须真正以工具形式发出;完成报告是纯文本,不要以工具调用收尾。

## 目标(三块)

1. 切片窗左键单击拾取:发 `voxelPicked`→`seedPicked(i,j,k)`,走主窗口现有 pickMode_ 路由(Seed/PathPoint,XQMainWindow.cpp:309-340 现成不动);常驻种子标记小球;恢复 B1b 弱化的 seed 测试断言。
2. 窗宽窗位数值框:导航器加窗宽/窗位 spin,与共享 imageProperty 双向同步(拖图联动数值、输数值联动渲染)。
3. i18n 欠账:B1b/B2a/B2d 新增 tr() 串补进 `resources/i18n/xq_zh_CN.ts` 并重生成 .qm。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 修改 | `src/visualization/XQRenderScene.h`(仅追加步骤 1/2 列出的成员)/ `.cpp` |
| 修改 | `src/visualization/XQSliceViewWidget.{h,cpp}`、`XQMprWidget.{h,cpp}` |
| 修改 | `src/app/XQMainWindow.{h,cpp}` |
| 修改 | `resources/i18n/xq_zh_CN.ts`(+重生成 `i18n/xq_zh_CN.qm`)|
| 修改 | `tests/visualization/test_render_scene.cpp`、`tests/app/test_main_window.cpp` |

## 步骤 1:拾取(XQSliceViewWidget + XQRenderScene)

**源码现状(已核)**:`seedPickingEnabled_` 标志 B1b 已存(XQSliceViewWidget.cpp:176);`voxelPicked` 信号已声明未发射(.h:58);eventFilter 已有 press/move/release 状态机(B2a 拖线);XQMprWidget 已把 `seedPicked` 连到主窗口路由(转发链 `voxelPicked`→`seedPicked` 需在 XQMprWidget 内补 connect——grep 确认现状,缺则补)。

- **MouseButtonPress 分支扩展**(在现有"命中十字线则拖线"之后):`seedPickingEnabled_ && 未命中十字线 && 左键` → `displayToWorld(pos, world)`(B2a 现成)→ 世界→体素:XQRenderScene 追加公开成员
  ```cpp
  // World point -> nearest voxel index (rounded, clamped into the volume).
  // False when there is no volume. The out i/j/k follow the image index axes.
  bool worldToVoxelIndex(const double world[3], int* i, int* j, int* k) const;
  ```
  (实现:对三轴各用既有 worldToSliceIndex 的反解逻辑;本视图轴分量用当前 sliceIndex(axis_) 替代——点击发生在当前切片平面上)→ emit `voxelPicked(i,j,k)` → **consume 事件**(拾取模式下不给 VTK,避免误触窗宽窗位)。拾取未启用/未命中体积时行为不变。
- **种子标记**:XQRenderScene 追加
  ```cpp
  // Places / clears the resident seed marker (a small sphere at the voxel's
  // world position, shown in all four views). No volume -> no-op.
  void setSeedMarker(int i, int j, int k);
  void clearSeedMarker();
  bool seedMarkerVisible() const;   // probe
  ```
  实现:一组常驻 vtkSphereSource+actor(四视图各一,半径 = 2×平均 spacing,亮红 (0.9,0.2,0.2) 关照明),setSeedMarker 只 SetCenter+显示;clearVolume 时隐藏。XQSliceViewWidget/XQMprWidget 的 `clearSeedMarker()` stub 接到它;主窗口 seedPicked 路由 Seed 分支(:330-340)加 `renderScene_->setSeedMarker(i,j,k)` + renderAll(PathPoint 分支不加)。
- XQMprWidget:三个子件 `voxelPicked` → 自身 `seedPicked` 转发 connect(若缺)。

## 步骤 2:窗宽窗位数值框

- XQRenderScene 已有 `setWindowLevel/windowLevel`(公开,现成)。追加通知能力**不做**——用简单轮询绑定:主窗口在 sliceChanged/渲染刷新时同步数值框会漏"纯拖窗宽窗位"场景,故改为:XQSliceViewWidget 在 eventFilter 的 MouseButtonRelease(左键、非拖线、非拾取,即经 VTK 窗宽窗位交互后)发新信号 `windowLevelChanged()`(无参,收方自己读 scene);XQMprWidget 转发。
- 主窗口导航器面板(:1200-1240 dock 构建区)加两行:`窗宽`/`窗位` QDoubleSpinBox(objectName `xqNavWindowSpin`/`xqNavLevelSpin`,range 1..65535 / -32768..32767,decimals 1),useVolume 时初始化为当前值并 enable;`valueChanged` → `renderScene_->setWindowLevel` + renderAll(QSignalBlocker 防回环);收 `windowLevelChanged` → 读 `renderScene_->windowLevel` 回填两 spin(blocker)。
- retranslateUi 加两个 label 的 tr("Window")/tr("Level")。

## 步骤 3:i18n 欠账(xq_zh_CN.ts,手工加条目,勿跑 lupdate)

- 先 grep 汇总 B1b/B2a/B2d 引入的新 tr() 串(至少:`Show 3D Slice &Planes`、`Parsing model...`、`Model parse failed: %1`、切片角标 Axial/Sagittal/Coronal(XQMprWidget context)、`Nothing to render`(XQVolumeViewWidget)、本批新增 `Window`/`Level`、`Seed voxel  i:%1  j:%2  k:%3` 若缺)。
- 照现有 ts 文件里对应 context 的 message 块格式**手工追加**(context name = 类的完整命名空间名,对照文件内先例);中译:显示 3D 切片平面 / 正在解析模型... / 模型解析失败: %1 / 轴位·矢状位·冠状位 / 无可渲染内容 / 窗宽·窗位 / 种子体素 i:%1 j:%2 k:%3。
- 重生成 .qm:找 lrelease(Qt bin 目录,grep build_gui CMakeCache `Qt6_DIR` 定位)跑 `lrelease resources/i18n/xq_zh_CN.ts`;.qm 在 qrc 里(resources/xq_resources.qrc:26),重编后生效。

## 步骤 4:测试

1. `test_render_scene.cpp`:worldToVoxelIndex 正反(setVolume 后已知世界点→体素;越界夹紧;无卷 false);setSeedMarker 后 seedMarkerVisible true、clearSeedMarker/clearVolume 后 false。
2. `test_main_window.cpp`:**恢复 B1b 弱化块**——现状是"断言 B1b 不发射"(:238 附近 TODO(B2)),改回行为断言:构造 QMouseEvent 直接 `QApplication::sendEvent` 给 axial 子件的 vtkWidget(offscreen 下 renderer 未挂,displayToWorld 返回 false→不发射——**如实处理**:offscreen 测不了真点击换算,改为直接调用 `mprWidget->...` 无法到达 private;两难时的落点:断言 `setSeedPickingEnabled(true)` 后 `seedPickingEnabled()==true` + `voxelPicked` 信号可 connect + **XQRenderScene 层直接测 worldToVoxelIndex 数学**(1 已覆盖)+ 保留 TODO 注明"真点击链路真机验",报告申报);窗宽窗位:findChild 两 spin,setValue 后经 action/探针可读的部分断言(窗口无 renderScene 探针——断言 spin 值本身与 enable 态,内核 setWindowLevel 读回由 test_render_scene 既有测试覆盖)。
3. ts:test_i18n_resources 既有测试继续绿(新增条目不破坏);若它断言条目数/特定串,按需更新(先读该测试)。

## 步骤 5:验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
cmd //c build_gui_wt.bat && cmd //c ctest_merge.bat   # 全量必须全绿
cmd //c run_xq.bat   # 冒烟不崩
```
假绿抽查(必做,附证据+exe 时间戳):①篡改 worldToVoxelIndex(恒返回 0,0,0)→ test_render_scene 转红→还原绿;②篡改 setSeedMarker(空实现)→ seedMarkerVisible 断言转红→还原绿。

## 禁做

白名单外文件;不动 pickMode_ 路由逻辑本体;不做色阶条/.ctgr/勾选框/Opacity(B3);不跑 lupdate;不 commit;报告纯文本收尾;冲突停下等裁决。

## 完成报告格式

1. 文件清单;2. 构建+ctest 总结行原文;3. 两项假绿抽查证据;4. 测试恢复/弱化的如实申报;5. i18n 条目清单;6. 偏离与存疑。
