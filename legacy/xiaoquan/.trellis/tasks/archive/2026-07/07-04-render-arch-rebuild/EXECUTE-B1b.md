# EXECUTE-B1b — 四视图部件 + 主窗口接线(M1 完成,真机门禁批)

> 活动任务:`07-04-render-arch-rebuild`。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(B1a 已提交:XQRenderScene 内核 + test_render_scene,67/67 绿)。
> 本批结束后主审真机给用户过目——**这是 M1 的产品门禁**,你的产出要能让四格出正确图像。

## 必读上下文(按序)

1. 本简报全文;2. design.md §2.2/§2.3/§3.1/§3.2;3. render-code-inventory.md(概念遗产清单);4. B1a 产物:`src/visualization/XQRenderScene.h`(接口契约,**本批只允许按 §步骤1 增改其 .cpp 的相机段,公开头不动**)。

## 目标

三个新 Qt 部件(XQSliceViewWidget/XQVolumeViewWidget/XQMprWidget)挂载 XQRenderScene 的 4 个 renderer;XQMainWindow 渲染入口从"选中单节点"换成"场景同步";旧 XQMprView/XQRenderWidget 不再被实例化(代码留到 B5 删)。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 新建 | `src/visualization/XQSliceViewWidget.{h,cpp}`、`XQVolumeViewWidget.{h,cpp}`、`XQMprWidget.{h,cpp}` |
| 修改 | `src/visualization/XQRenderScene.cpp`(仅步骤 1 相机段)|
| 修改 | `src/app/XQMainWindow.{h,cpp}`、`src/app/main.cpp` |
| 修改 | `CMakeLists.txt`(xq_render_widget 源列表)|
| 修改 | `tests/app/test_main_window.cpp`、`tests/app/test_app_startup.cpp`、`tests/app/test_path_stage.cpp`(仅按步骤 6 迁移)|

## 步骤 1:XQRenderScene.cpp 补切片相机(B1a 遗漏,本批必补)

B1a 的 `setVolume` 对 4 个 renderer 只做了 `ResetCamera()`(XQRenderScene.cpp:563-565)。改为:三个切片 renderer 各自设置轴对齐平行投影相机,**再** ResetCamera:

- 各轴取向(LPS;真机对照基线后允许微调,先按此):
  - Axial(axis2,看 z 切片):相机位于体中心 −Z 方向,view-up = (0,−1,0);
  - Sagittal(axis0,看 x 切片):相机位于 +X 方向,view-up = (0,0,1);
  - Coronal(axis1,看 y 切片):相机位于 −Y 方向,view-up = (0,0,1);
  - 三者 `ParallelProjectionOn()`;Volume3D 保持默认透视 + ResetCamera。
- 实现放 Impl 私有辅助(如 `setupSliceCameras()`),setVolume 末尾调用。公开头零改动。

## 步骤 2:XQSliceViewWidget(Qt+VTK,进 xq_render_widget)

- 持一个 `QVTKOpenGLNativeWidget`,构造收 `XQRenderScene*` + axis(0/1/2),经 `vtkRendererHandle(viewForAxis)` 把 renderer 挂进自己的 renderWindow(挂法照抄旧 XQRenderWidget.cpp 的 AddRenderer 段)。
- 交互器:`vtkInteractorStyleImage`(现成,左键拖=窗宽窗位、滚轮=缩放,B2 再细调)。
- **滚轮 → 切片**:eventFilter 拦 vtkWidget 的 QEvent::Wheel,自己消费(不给交互器):`scene->setSliceIndex(axis, cur ± delta)` 后发 `sliceChanged(axis,int)` 信号;主窗口负责各视图 re-render 与滑块同步。
- 角标 QLabel 叠加(右下,"idx/count",旧 XQMprView 的 info label 概念),objectName 沿用 `xqMprInfoAxial/xqMprInfoSagittal/xqMprInfoCoronal`。
- `renderNow()` 公开方法:renderWindow->Render()。
- `setSeedPickingEnabled(bool)`/`clearSeedMarker()`:**B1b 只存状态位 + 注释 TODO(B2)**,不实现拾取(拾取是 M2 范围;信号 `voxelPicked(int,int,int)` 先声明不发射)。

## 步骤 3:XQVolumeViewWidget

- 同挂载手法,取 Volume3D renderer;默认 trackball 相机交互(QVTK 默认即是)。
- `static void configureDefaultSurfaceFormat()`:一行,照抄旧 XQRenderWidget(QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat()))——迁到这里,B5 删旧类时不断链。
- `renderNow()`。空场景提示标签(旧 setEmptyHintVisible 概念)可做可不做,不作验收项。

## 步骤 4:XQMprWidget(2×2 容器)

- 2×2 QGridLayout:左上 Axial(红 #C43C3C)/右上 Sagittal(绿 #3C9C4A)/左下 Coronal(蓝 #3C6CC4)/右下 3D(黄 #C4913C)——QFrame 配色边框照抄旧 XQMprView.cpp `styleSliceFrame/styleVolumeFrame`(:40-80),objectName 沿用 `xqMprAxialFrame/xqMprSagittalFrame/xqMprCoronalFrame/xqMprVolumeFrame`,自身 objectName `xqMprView`,四格内的视图部件 objectName `xqMprAxial/xqMprSagittal/xqMprCoronal/xqRenderWidget`(最后一个 test_app_startup:256 依赖)。
- 构造收 `XQRenderScene*`,自建 3 slice + 1 volume 子部件。
- API:`sliceCount(axis)`/`currentSlice(axis)`(转发 scene)、`setLayoutMode(Quad/Single)`+`layoutMode()`(Single=只显 Axial,照旧语义)、`renderAll()`(四个 renderNow)、`setCrosshairVisible(bool)`(转发 scene + renderAll)、`setSeedPickingEnabled`/`clearSeedMarker`(转发三个 slice 子件)、信号 `sliceChanged(axis,idx)`(转发子件)、`seedPicked(i,j,k)`(声明,B2 接)。
- changeEvent(LanguageChange)→ 角标轴名 retranslate(照旧 XQMprView 先例;若角标只有数字则本条自然为空实现)。

## 步骤 5:XQMainWindow 重接(核心)

成员变化:`mprView_`(XQMprView*)与 `renderWidget_`(XQRenderWidget*)删除,换 `renderScene_`(std::unique_ptr<XQRenderScene>)+ `mprWidget_`(XQMprWidget*)。逐触点迁移(行号基于 feat/render-arch 当前 HEAD):

1. 构造(:216-277):new XQRenderScene → new XQMprWidget(scene) 放进 centralStack_,布局/objectName 语义照旧。
2. `useVolume`(:1648-1680):`mprView_->setImage` → `renderScene_->setVolume(image,buffer)`;滑块 range/value 改从 `renderScene_->sliceCount/sliceIndex` 取;末尾 `mprWidget_->renderAll()`。
3. `onNavigatorSliderChanged`(:1846-1873):`mprView_->setSlice` → `renderScene_->setSliceIndex` ×3 + `mprWidget_->renderAll()`;状态栏文本逻辑不变。
4. 新增槽接 `mprWidget_->sliceChanged`:同步对应滑块/spin(QSignalBlocker 防回环)+ 其余视图 re-render。
5. **渲染模型替换**:`onSceneSelectionChanged`(:2104-2237)——Image 分支(加载 .vti → useVolume)**保留**;`renderer.clear()+add*` 整段**删除**(选中不再驱动几何渲染)。新增 `syncRenderScene()`:遍历 `scene_->visit_nodes`,内存 payload 直接 `renderScene_->upsertNode`;SurfaceModel/Mesh 无内存几何时走 `resolveLazyGeometrySource` + `upsertNodeProgressive`(lazy 解析链照抄旧 :2163-2210,LOD 用 kDefaultLodOptions,:142 常量保留);对 `renderScene_->hasNode` 但场景已无的 id 调 `removeNode`;末尾 `mprWidget_->renderAll()`。**挂进 `refreshSceneTree()`(:1992)**——它已覆盖工程加载/命令 push/undo/redo 全部 10 个调用点,单一集成点。
6. seedPicked 连接(:308-340):connect 源改 `mprWidget_`,lambda 体不动(B2 前不会发射,连接必须存在且编译)。
7. 杂项触点:clearImage 两处(:341,:603)→ `renderScene_->clearVolume()`(603 处还有 renderer().clear→`renderScene_->clearNodes()`)+ renderAll;crosshair(:997)、centralStack setCurrentWidget(:1087,useVolume 尾)、seg/path 页 setSeedPickingEnabled/clearSeedMarker(:656-705)→ 转发 mprWidget_;applyPreferences 的 crosshair(:1900 附近)同。彻底 grep `mprView_|renderWidget_` 清零。
8. `main.cpp:36`:`XQRenderWidget::configureDefaultSurfaceFormat()` → `XQVolumeViewWidget::configureDefaultSurfaceFormat()`。

## 步骤 6:测试迁移(先读后改,断言语义只增不减,唯一例外见 6c)

- a) `test_main_window.cpp`:`findChild<xq::XQMprView*>("xqMprView")` 三处 → `xq::XQMprWidget*`;`mpr->sliceCount/currentSlice` 调用签名不变(新 API 同名);四 frame 等尺寸断言、xqMprInfoAxial 等不动;include 与 :84 的 configureDefaultSurfaceFormat 改新类。
- b) `test_app_startup.cpp`:256 行 findChild QWidget "xqRenderWidget" 应继续通过(objectName 已沿用),不动;若有 include/format 调用同迁。
- c) **seed-pick 断言块**(:232-260,QLabel 点击 → seedPicked):拾取是 B2 范围,此块**替换**为等价结构断言(mprWidget setSeedPickingEnabled(true) 后 seedPickingEnabled()==true 之类)+ 显式 `// TODO(B2): seed-pick 行为断言随拾取实现迁回`。**报告里必须单独声明此项弱化**,主会话记账 B2 恢复。
- d) `test_path_stage.cpp`:include/format 调用迁新类;若有经 XQMprView 的路径拾取断言,同 c) 处理并申报。

## 步骤 7:CMake

`xq_render_widget` 源列表加三对新文件(AUTOMOC 已开);旧 XQMprView/XQRenderWidget **留在源列表**(B5 才删)。无新链接依赖。

## 步骤 8:验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
cmd //c build_gui_wt.bat && cmd //c ctest_merge.bat   # 67/67 必须全绿
cmd //c run_xq.bat    # 真机冒烟:自己先看一眼再交
```

真机自查清单(做不到的如实报告,不许糊弄):四格皆有内容(三切片图像 + 3D 三平面);Sagittal/Coronal 不是黑格;Axial 长宽比正常;导航滑块拖动切片跟手;滚轮切片可用;十字线可见;打开 0007_H_AO_H 后 3D 里能看到 surface/mesh/path 合成(syncRenderScene 生效)。
假绿抽查:篡改 syncRenderScene(跳过 upsert)→ 相关结构断言/真机 3D 空 → 还原全绿;核对 exe 时间戳。

## 禁做

- 白名单外文件;不动 XQRenderScene.h 公开头;不删旧 XQMprView/XQRenderWidget 源文件。
- 不实现拾取/窗宽窗位数值框/Opacity/Color/勾选框(B2/B3 范围);不 commit;不加未要求兜底。
- 测试断言只许按步骤 6 迁移,不许静默删除;弱化必须申报。
- 冲突事实停下报告等裁决。

## 完成报告格式

1. 文件清单+一句话;2. 构建尾部+ctest 总结行原文;3. 真机自查清单逐项结果(含做不到的);4. 假绿抽查证据;5. 测试弱化申报(6c/6d);6. 偏离与存疑。
