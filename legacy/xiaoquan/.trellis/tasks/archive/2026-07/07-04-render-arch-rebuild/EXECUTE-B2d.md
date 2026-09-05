# EXECUTE-B2d — 3D 切片平面可见性开关 + 渲染侧上传锚点(遮挡裁决批)

> 活动任务:`07-04-render-arch-rebuild`。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD e31c349 = B2c)。
> 纪律:工具调用必须真正以工具形式发出;完成报告是纯文本,不要以工具调用收尾。

## 背景(主会话已诊断)

用户认为 3D 里血管分支"没加载出来";主会话离线解析 0090_0001.vtp 证明:169080 三角单一连通分量、四分支端帽俱全、payload 锚点断言绿——几何全部进了渲染器。最大嫌疑是**3D 视图三张不透明灰度切片平面遮挡**。本批:①给 3D 平面加可见性开关(产品功能,MITK 同有);②渲染层补上传锚点断言。真机开关一关即可裁决分歧。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 修改 | `src/visualization/XQRenderScene.h`(仅追加下述两个成员)|
| 修改 | `src/visualization/XQRenderScene.cpp` |
| 修改 | `src/app/XQMainWindow.{h,cpp}` |
| 修改 | `tests/visualization/test_render_scene.cpp`、`tests/app/test_main_window.cpp` |

## 步骤 1:XQRenderScene 平面开关

头文件追加(仅这两个):
```cpp
// Shows/hides the three grey-scale slice planes mounted in the 3D view (the 2D
// slice views are unaffected). Default true. Survives setVolume (a volume
// reload re-applies the current flag to the rebuilt planes).
void setImagePlanesVisible3d(bool on);
bool imagePlanesVisible3d() const;
```
实现:Impl 加 `bool imagePlanes3dVisible_ = true;`;setter 对 `slices3d_[0..2]` 各 `SetVisibility`;setVolume 重建 3D 平面后按当前 flag 应用(重载体数据不丢用户选择);2D 切片(slices2d_)与十字线不受影响。

## 步骤 2:XQMainWindow 视图菜单项

- 视图菜单(现有 crosshairAction_ 旁,构造处 grep `crosshairAction_` 找先例)加 checkable QAction「3D 切片平面」objectName `xqAction3dPlanes`,默认 checked;toggled → `renderScene_->setImagePlanesVisible3d(on)` + `mprWidget_->renderAll()`;
- i18n:用与相邻 action 相同的 tr 机制;新串缺翻译条目属已知欠账(B 批收口统一补),报告提一句即可。

## 步骤 3:测试

1. `test_render_scene.cpp`:默认 `imagePlanesVisible3d()==true`;setVolume 后 set false→读回 false;再 setVolume(重载)→仍 false;set false 后 2D 探针不受扰(sliceCount/sliceIndex 原值);无卷时 set/get 不崩。
2. `test_main_window.cpp`:
   - findChild `xqAction3dPlanes` 非空、checked、checkable;trigger 后 `renderScene` 探针读回 false(经窗口现有测试访问路径;若主窗口无 renderScene 公开访问,加最小 test-only getter 的方案**不允许**——改为断言 action 状态本身 + 依赖 test_render_scene 覆盖内核逻辑,报告说明);
   - **渲染侧上传锚点**:在 B2c 数据锚点断言后追加——经现有场景遍历拿到 SurfaceModel 节点 id,断言窗口渲染场景 `uploadedPointCount(id)==84542`。主窗口如无探针通路:XQMainWindow 已有哪些 test 可见接口先 grep(如 test_main_window 现有访问手法),实在没有就把该断言放进 test_render_scene 用真实 .vtp 路径(XQ_TEST_DATA_ROOT 宏可用,grep CMakeLists `XQ_TEST_DATA_ROOT` 确认传参方式)经 MDLModelReader::read + upsertNode 直接锚,**二选一,报告写明选了哪条与原因**。

## 步骤 4:验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
cmd //c build_gui_wt.bat && cmd //c ctest_merge.bat   # 全量必须全绿
cmd //c run_xq.bat   # 冒烟不崩
```
假绿抽查(必做,附证据+exe 时间戳):篡改 setImagePlanesVisible3d(空实现)→ 相关断言转红 → 还原绿。

## 禁做

白名单外文件;XQRenderScene.h 超出两个成员的改动;不动截线/十字线/模型装配;不 commit;报告纯文本收尾。冲突事实停下等裁决。

## 完成报告格式

1. 文件清单;2. 构建+ctest 总结行原文;3. 假绿抽查证据;4. 上传锚点断言落点选择与原因;5. 偏离与存疑。
