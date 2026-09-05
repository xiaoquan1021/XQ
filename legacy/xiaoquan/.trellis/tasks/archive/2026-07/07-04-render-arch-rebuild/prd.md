# 可视化层推倒重写:常驻 GPU 渲染管线 + 可见性驱动场景合成

> 任务: `07-04-render-arch-rebuild`。branch `feat/render-arch`(基于 feat/gui-v2,HEAD a3715e5,ctest 66/66 绿——注意:全绿但产品不合格,见下)。
> worktree: `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`(构建 `cmd //c build_gui_wt.bat`,测试 `cmd //c ctest_merge.bat`,真机 `cmd //c run_xq.bat`)。
> 状态: planning。
> **本任务的存在本身就是一次事故复盘的产物,新接手者先读完"为什么开这个任务"再动手。**

## 为什么开这个任务(事故经过,必读)

07-03 任务(GUI v2)六批全部执行完:ctest 66/66 绿、先红后绿、假绿抽查、主审亲读代码修缺——**全部流程走完,用户真机一开,产品不合格**:

- 比重构前(依赖 MITK 的旧 XQ)**卡顿严重得多**;
- 功能差距巨大:打开工程后 3D 视图空白、切片上没有轮廓叠加、勾选框不控制显隐、切片不能缩放/平移/调窗宽窗位、矢状/冠状位经常整格黑。

用户对照截图(见 research/visual-baseline.md)给出的结论:这种差距不用对比,推倒重来。

## 存在的问题(根因,三条,全部架构级)

### 1. MPR 是"假"的:离屏渲染贴 QLabel
`XQMprView` 的每个切片格是一个 QLabel。渲染路径:
```
滑块动 → renderAxis → XQSceneRenderer::addImageSlice
  → build_image: 把整卷体数据逐体素(经虚调用 scalarAt)拷进新建 vtkImageData   ← O(整卷)×3面板
  → renderOffscreenToRgba: 每帧新建 vtkRenderWindow(离屏 GL 上下文!)
    → Render → 整帧回读 CPU → QImage.copy() → QPixmap → QLabel.setPixmap      ← 每帧建/销 GL 上下文
```
切一张 2D 切片本该 O(切片),现在是 O(512³)×3 + 每帧几十 ms 的 GL 上下文创建。
QLabel 贴图**结构性地做不了**缩放/平移/窗宽窗位/流畅十字线。

### 2. 渲染模型错:只渲染"当前选中的单个节点"
`XQMainWindow::onSceneSelectionChanged` 是唯一渲染入口:clear() 后只 add 选中节点。
旧 XQ(MITK)的模型:**场景内所有可见节点合成渲染**——图像上叠分割轮廓、
3D 格里三平面+路径+模型+网格共存、数据管理器勾选框控显隐。
现在 XQSceneModel 的可见性列(显示 false)根本没接到渲染。

### 3. 测试脚手架当产品出货了
"离屏→RGBA→断言像素/结构"这套是为 headless ctest 可测而设计的,M1 起就是产品的
唯一渲染路径。后续所有批次(07-02 审计、07-03 六批)都在这个错误地基上盖楼——
**每一批的 diff 单看都是对的,合起来是错的**。ctest 测的是"结构存在/字节对/节点数守恒",
"卡""难用""功能不对"一行都测不出。

## 为什么要做这样的事(目标)

用纯 VTK(不回 MITK)重建正确的渲染架构,拿到"去 MITK"本来要拿的性能收益:

- 体数据**上传 GPU 一次**,切片是 GPU 上的 reslice,滑块响应 O(切片);
- 四个**常驻** `QVTKOpenGLNativeWidget`(3 切片 + 1 交互 3D),没有每帧上下文创建;
- **可见性驱动的场景合成**:勾选框→actor 显隐,切片叠轮廓,3D 合成显示所有可见节点;
- 交互补齐:缩放/平移/窗宽窗位/十字线联动/拾取(seed + path 控制点)。

对照基线 = 用户提供的旧 XQ 截图(research/visual-baseline.md):功能上至少做到
图像+轮廓叠加、3D 合成、勾选显隐、导航联动;流畅度必须明显优于 MITK 版。

## 大体怎么做(模块拆分,依赖序)

| # | 模块 | 内容 | 依赖 | 验收 |
|---|---|---|---|---|
| M1 | 渲染内核 | 常驻 vtkRenderer 场景合成器:体数据一次上传、vtkImageReslice(或 vtkImageSliceMapper 常驻输入)切片、payload→actor 构建器(可从旧 XQSceneRenderer 抄 mapper 设置)、节点 add/remove/setVisible | 无 | **真机给用户过目,不过目不盖 M2** |
| M2 | 四视图 MPR | 4 常驻 QVTKOpenGLNativeWidget(3 切片共享体数据 + 1 个 3D 共享场景)、十字线联动、滚轮切片、窗宽窗位/缩放/平移、拾取(seed + path 点,拾取路由现有 pickMode_ 逻辑可参考) | M1 | 真机交互流畅度 |
| M3 | 可见性场景树 | 数据管理器勾选框接 M1 显隐;Opacity/Color 做实(07-03 B2 删掉了,是错误决策,要做回来);图标/类型列保留 | M1 | 勾选即显隐 |
| M4 | 窗口瘦身重接 | XQMainWindow(2243 行上帝类)拆出 AppState/WorkflowSession(scene+stack+controllers+taskRunner+providers 打包);StageWidgets 的 12 个 std::function 尾参换成收 session 对象;导航器/状态栏坐标/加载路径接新视图 | M2,M3 | ctest + 真机回归 |
| M5 | 收口 | i18n(注意 xqTr 走 translate,lupdate 扫不到,手工加 ts 块)、忙态矩阵复核、真机逐项对照基线截图验收 | M4 | 用户过目签收 |

## 保留 / 重写边界(定论,不要重新争论)

**保留不动(~70%,去 MITK 的真正收益):**
- `src/core` `src/services` `src/io` `src/adapters`:分割/TetGen+MMG 体网格/1D 求解/
  工程 IO/惰性几何(MappedGeometrySource+GeometryResourceManager LRU+pin+预算)全部有效;
- `src/ui/controllers` + 命令栈 + prepare*/commitPrepared 拆分 + `src/app/XQTaskRunner`
  (常驻工作线程,双跳投递):与渲染无关,直接复用;
- `src/ui/panels/XQStageWidgets` 表单逻辑保留(M4 只改参数传递方式);
- 忙态矩阵、工作区打开/保存(save-as 复制惰性 blob)、i18n 机制。

**推倒重写(~4500 行,15~20%):**
- `XQMprView`(905 行)全部——QLabel 贴图模型零保留;
- `XQSceneRenderer`(1155 行)交互路径——`renderOffscreenToRgba`/`build_image` 整卷拷贝
  路线废弃;各 payload→actor 的 mapper 构建代码**可抄**(约省该文件一半工作量);
- `XQRenderWidget`(129 行)——改为共享场景挂载;
- `XQMainWindow` 渲染/选中/导航联动约 1/3(onSceneSelectionChanged 的"选中单节点渲染"
  整段换成可见性合成模型)。

**离屏渲染代码的去留**:`renderOffscreenToRgba` 若仍被库级测试(test_scene_renderer 等)
引用,可留作 test-only 路径,但**产品运行时不得经过它**;交互路径与测试路径必须分开。

## 硬约束(违反即返工)

1. **架构验收先于批次执行**:M1 完成后先真机跑给用户看,用户认可地基再继续。
2. **headless 可测性不得反向决定产品架构**——测不了的交互/流畅度用真机目视验收,
   不许为了可测再造离屏假路径。
3. ctest 全绿只是不回归的下限,**不作为任何模块的达标依据**。
4. services/core 不得引 Qt/VTK(既有边界,check_arch_boundaries.cmake 在管)。
5. 不参考 XQ1(失败产物);MITK 只作为**行为参照**(截图/交互对比),不引依赖。
6. 每模块真机目视是第一优先,不是收尾待办。

## 新接手者需要知道的其他事实(过滤好的)

- 构建配方:vcvars64 + CMAKE_PREFIX_PATH 多前缀(非单 Qt6_DIR),worktree 里
  `build_gui_wt.bat`/`ctest_merge.bat`/`run_xq.bat` 三个脚本现成,勿另造。
- 单目标增量构建有假绿坑(链接静默跳过跑旧 exe),一律完整 `cmake --build build_gui`。
- Qt 静态库 qrc 需 main 里 Q_INIT_RESOURCE(已做,别删)。
- renderer 从 IGeometrySource 取数据必须 copy-on-upload(VTK 惰性管线+长持 actor,
  零拷贝借用必悬垂)——M9a 已验证的定论。
- LOD/渐进上传(SurfaceLodBuilder/addSurfaceProgressive/ChunkPlan)是为大表面模型做的,
  新场景合成器要保留等价能力,但接法可以重新设计(它们在旧 SceneRenderer 里)。
- 真实测试数据:`C:/Users/OCEAN/Desktop/XIAOQUAN/0007_H_AO_H`(CMake XQ_TEST_DATA_ROOT
  已配),旧 XQ 截图打开的就是这个工程(28 节点,含 path/contour/surface/mesh/simulation)。
- 07-03 的执行文档(EXECUTE-B1~B6)对本任务**只有参考价值没有执行价值**——它们建立在
  错误地基上;其中 B3(TaskRunner/忙态)和 B2(工作区打开保存)的成果保留在用。
- memory 已有相关教训条目:render-architecture-must-be-validated-first、
  gui-task-green-tests-not-done、renderer-source-copy-on-upload-not-borrow、
  lod-cuts-upload-not-host-peak、xqtr-translate-lupdate-blind。

## Non-goals

- 不回 MITK,不引新第三方依赖;
- 不动 services/core/io/adapters 的任何接口(Source 1.0 签名不破坏);
- 不做体渲染(volume rendering)、不做 4D/时间轴(旧 XQ 有 Time 控件但无数据驱动,
  等有真实 4D 数据再立项);
- AI 阶段 UI 不扩展;
- 不追求像素级复刻旧 XQ 外观,追求功能对齐+流畅度反超。
