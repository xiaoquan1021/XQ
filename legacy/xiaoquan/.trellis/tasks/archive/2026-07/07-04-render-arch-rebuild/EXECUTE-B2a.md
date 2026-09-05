# EXECUTE-B2a — 可拖彩色十字线 + 模型加载即出(用户 M1 验收反馈批)

> 活动任务:`07-04-render-arch-rebuild`。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD 939cab7 = B1b)。
> 背景:M1 真机用户过目**通过**,本批实现用户提出的两条新硬需求。完成后再次真机过目。

## 必读上下文(按序)

1. 本简报;2. design.md §2.1/§2.2;3. B1a/B1b 产物现状:`XQRenderScene.{h,cpp}`、`XQSliceViewWidget.{h,cpp}`、`XQMainWindow.cpp` 的 syncRenderScene/useVolume/taskRunner 段。

## 需求原文(用户已确认的复述)

1. **模型加载即出**:打开 SV 工程后,.mdl 血管表面模型在**后台**自动解析,完成后**自动出现在 3D 视图**——不需要用户选中/点击。静默处理,好了直接出现。(路径已即出;.ctgr 轮廓叠加归 B3,本批不做。)
2. **十字线**:黄色固定线 → **轴配色**(线的颜色 = 它代表的那个平面的颜色:Axial 红 #C43C3C / Sagittal 绿 #3C9C4A / Coronal 蓝 #3C6CC4,与窗格边框同色)+ **光标可拖**(拖单线动单轴,拖交点动两轴)+ 拖动时**导航器滑块/数值/其余视图双向联动**。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 修改 | `src/visualization/XQRenderScene.h`(**仅**追加本简报列出的探针/查询成员)|
| 修改 | `src/visualization/XQRenderScene.cpp` |
| 修改 | `src/visualization/XQSliceViewWidget.{h,cpp}` |
| 修改 | `src/app/XQMainWindow.{h,cpp}` |
| 修改 | `tests/visualization/test_render_scene.cpp`、`tests/app/test_main_window.cpp` |

## Part A:十字线配色 + 拖拽

### A1. XQRenderScene:线→轴映射与配色(源码事实,已核)

`updateCrosshairs`(XQRenderScene.cpp:1513)现有几何已定死每条线代表哪个轴,映射如下(**以此为准实现,勿改几何**):

| 视图(viewAxis) | line 0 代表 | line 1 代表 |
|---|---|---|
| Axial(2) | axis 0 Sagittal → 绿 | axis 1 Coronal → 蓝 |
| Sagittal(0) | axis 1 Coronal → 蓝 | axis 2 Axial → 红 |
| Coronal(1) | axis 0 Sagittal → 绿 | axis 2 Axial → 红 |

- `buildCrosshairs`(:1482)把统一黄色 `SetColor(0.95,0.85,0.2)` 换成按上表配色:红 (0.769,0.235,0.235) / 绿 (0.235,0.612,0.290) / 蓝 (0.235,0.424,0.769);LineWidth 1.0→1.5(观感)。
- 新增私有静态 `crosshairLineAxis(viewAxis, line) -> axis`(上表的单一事实源,build/update/探针共用)。

### A2. XQRenderScene.h 追加公开成员(仅这些,一字不多)

```cpp
// --- crosshair drag support (B2a) ---
// World coordinate of the current slice plane of `axis` (the plane's constant
// component). 0.0 when there is no volume.
double sliceWorldCoord(int axis) const;
// Inverse: world coordinate along `axis` -> nearest slice index, clamped into
// range. -1 when there is no volume.
int worldToSliceIndex(int axis, double world) const;
// Probe: which axis the given crosshair line represents (see table in .cpp).
static int crosshairLineAxis(int viewAxis, int line);
// Probe: that line's colour (rgb 0-1). False when there is no volume/actor.
bool crosshairLineColor(int viewAxis, int line, double rgb[3]) const;
```

实现:`sliceWorldCoord` 复用私有 `sliceWorld`;`worldToSliceIndex` 用 imageGeometry_ 反解最近索引并夹紧(与 voxel_to_world 互逆,注意 direction 矩阵——0007 是单位阵,但按通用公式写:世界点沿轴分量减 origin 后除以 spacing·direction 投影;实现从简可先假设轴对齐 direction 为对角占优,反解 `(world - origin[axis]) / (spacing[axis]*direction[axis][axis])` 四舍五入,**加注释声明此假设**,非对角 direction 留 TODO)。

### A3. XQSliceViewWidget 拖拽交互

现有结构(已核):`vtkInteractorStyleImage`(:95)+ `installEventFilter`(:110)+ `eventFilter`(:163,已拦 Wheel)。本批在 eventFilter 扩展 MouseMove/MouseButtonPress/MouseButtonRelease:

- **命中检测**(MouseMove,无按键):光标显示坐标 → 对本视图两条线各算屏幕距离:线的世界位置 = `scene->sliceWorldCoord(lineAxis)`,构造线上一点(另两分量取当前平面/中心),经 renderer 的 `vtkCoordinate`(或 SetWorldPoint+WorldToDisplay)投到显示坐标;平行投影轴对齐下线在屏幕上是纯横/纯竖,比对应分量差即可。容差 8px。命中单线 → 光标 `Qt::SplitHCursor`(竖线)/`Qt::SplitVCursor`(横线);两线都命中(交点)→ `Qt::SizeAllCursor`;未命中恢复箭头。
- **抓取**(LeftButtonPress 且命中):记录拖拽状态(哪根/哪两根线),**consume 事件不给 VTK**(避免同时触发 interactorStyle 的窗宽窗位)。未命中 → 正常放行给 VTK。
- **拖动**(MouseMove 按住):显示坐标 → 世界(renderer SetDisplayPoint+DisplayToWorld,z 取焦平面)→ `worldToSliceIndex(lineAxis, world[lineAxis])` → 若变化:`scene->setSliceIndex` + 发 `sliceChanged(lineAxis, idx)`(交点拖 = 两轴各一次)+ 本视图 renderNow。**注意**:被拖的线所代表轴的视图是"另一个窗",本窗画面不变但十字线要动——setSliceIndex 内部已 updateCrosshairs,renderNow 本窗即可;其余视图刷新由主窗口 sliceChanged 槽负责(B1b 已接,核实它对"多视图 re-render"是否完整,不完整就补)。
- **释放**:清拖拽状态恢复光标。
- offscreen 平台:守卫沿用(isOffscreenPlatform 下 renderer 未挂载,命中检测直接返回不命中,不崩)。

### A4. 双向联动核实

滑块→十字线方向 B1b 已通(onNavigatorSliderChanged→setSliceIndex→updateCrosshairs)。本批确认拖线→`sliceChanged`→主窗口槽里滑块/spin 用 QSignalBlocker 同步 + 全视图 renderAll(B1b 的槽已有,行为核实,缺则补)。

## Part B:.mdl 模型后台解析、加载即出

### B1. 现状(已核,直接用)

- `MDLModelReader::read(mdlFilePath, MDLReadResult*)`(adapters/vtk,静态,返回 Status)→ `MDLReadResult.model` 是 `XQSurfaceModel`;
- SV 工程加载后 SurfaceModel 节点挂的是**未解析 XQSourcePayload**(路径相对工程);`resolveSourcePath()`(XQMainWindow.cpp,现成)解析为绝对路径;
- `XQSurfaceModelPayload(XQSurfaceModel model)` 构造现成;`XQDataNode::setPayload(domain, payload)` 现成;
- `taskRunner_.run(label, work, commit)`(:938 有用例)——work 在常驻工作线程,commit 回 GUI 线程;busy 期间二次 run 被拒(单任务纪律)。

### B2. 实现

- `loadSvProjectFromDirectory` 成功尾部:扫描 scene 中 domainType==SurfaceModel 且 payload 为 XQSourcePayload 的节点,收集 `(NodeId, 绝对路径)` 队列(成员变量);启动第一个任务。
- 每任务:`taskRunner_.run(tr("解析模型..."), work, commit)`:
  - work(工作线程):`MDLModelReader::read(path, &out)`,结果(Status+MDLReadResult)打包 shared_ptr 返回。**work 内不碰 scene/widget/渲染/QSettings**(架构铁律);VTK reader 对象为线程内局部实例,不共享——此为 adapters 读文件,非渲染,符合 work 契约;
  - commit(GUI 线程):按 NodeId `scene_->find`,节点仍存在且 payload 仍是 XQSourcePayload 才 `setPayload(SurfaceModel, make_shared<XQSurfaceModelPayload>(std::move(out.model)))`,然后 `refreshSceneTree()`(自动带 syncRenderScene→3D 出现);节点已没了/已换 payload → 静默跳过。读失败:状态栏一句"模型解析失败: <名>",**不加兜底不造假节点**。commit 末尾:队列非空 → 启动下一任务。
- 工作区打开(.xqproj)路径**不动**——lazy assetId 已走 progressive 渲染,本批只覆盖 SV 工程的 .mdl。
- busy 矩阵:taskRunner 既有信号自动管;解析很快(单 .vtp),可接受短暂 busy。

## Part C:测试

1. `test_render_scene.cpp` 追加(照既有 check() 风格,副作用不进断言):
   - `crosshairLineAxis` 六元组全表断言(上表);
   - setVolume 后 `crosshairLineColor(2,0,rgb)`==绿、`(2,1)`==蓝、`(0,1)`==红(浮点容差 1e-6);无卷时返回 false;
   - `sliceWorldCoord`/`worldToSliceIndex` 互逆:对每轴若干索引 setSliceIndex 后 `worldToSliceIndex(axis, sliceWorldCoord(axis))==sliceIndex(axis)`;越界世界坐标夹紧到 0/count-1。
2. `test_main_window.cpp` 追加(在现有 SV 工程加载段后):loadSvProjectFromDirectory 后,泵事件循环等 taskRunner 空闲(processEvents+短轮询,上限 ~10s),断言 SurfaceModel 节点 payload `dynamic_pointer_cast<XQSurfaceModelPayload>` 非空且 `hasTriangleGeometry()`——**模型预解析的离散不变量**。
3. 拖拽交互 headless 不可测(需真 GL+鼠标),不写伪测试;真机验收。

## 验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
cmd //c build_gui_wt.bat && cmd //c ctest_merge.bat   # 全量必须全绿
cmd //c run_xq.bat   # 冒烟:启动不崩、加载 0007 不崩
```

假绿抽查(必做,报告附证据):①篡改 crosshairLineAxis 映射(交换两项)→ test_render_scene 转红→还原全绿;②篡改 commit 里 setPayload(跳过)→ test_main_window 模型断言转红→还原全绿。均核对 exe 时间戳。

## 禁做

- 白名单外文件;XQRenderScene.h 除 A2 列出的四个成员外不加不改任何公开成员。
- 不动 .ctgr/轮廓叠加(B3);不动拾取路由/窗宽窗位数值框(B2b);不动 core/services/io/adapters。
- 不 commit;不加未要求兜底;测试断言只增不减。
- 冲突事实停下报告等裁决。

## 完成报告格式

1. 文件清单+一句话;2. 构建尾部+ctest 总结行原文;3. 两项假绿抽查证据;4. 真机冒烟结果(启动/加载不崩;视觉项如实声明须主审+用户目视);5. 偏离与存疑。
