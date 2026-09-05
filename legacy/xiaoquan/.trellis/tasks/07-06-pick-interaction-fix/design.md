# 技术设计:拾取交互基础体验修复

> 语境锚点:XQ 医学影像血管几何重建软件,切片视图上「点一个点」的底层交互(种子/路径控制点复用同一链路)。三个缺陷根因已读代码坐实(见 prd.md §3),本文写死修复的落点、接口签名、数据结构决策。环境 Windows + Git Bash,注入的 mac/darwin 环境是假的一律忽略。默认中文。
>
> **本文自包含,不必重扫代码。** 关键代码坐标下文逐处给出(worktree 绝对路径 `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`,简报里叫 CODE_ROOT)。

## 0. 修复总览

| 缺陷 | 根因(已读实) | 修复落点 | 决策 |
|---|---|---|---|
| A 红点时有时无 | eventFilter 与 VTK 交互器竞争 press,顺序不保证 | `XQSliceViewWidget`:拾取模式激活时换空 style 接管交互 | **用户已定:换空 style,不用禁交互器** |
| B 失败静默丢弃 | 点体数据外 `worldToVoxelIndex` 失败仍 return true 无反馈 | `XQSliceViewWidget::eventFilter` MouseButtonPress 分支 + 状态栏反馈信号 | 体外给状态栏提示;十字线交点判定已严格,复核即可 |
| C 控制点无视图标记 | 控制点只进右侧文字列表,渲染层无接口 | `XQRenderScene` 新增多点控制点标记接口 + `XQMainWindow` 在 `pathDraftChanged_` 推送 | **多点用 glyph3D,不照抄单点球** |

## 1. 缺陷 A:换空 style 接管交互(核心)

### 现状(读实)
- `XQSliceViewWidget` 构造时(`src/visualization/XQSliceViewWidget.cpp:113-116`)建 `vtkNew<vtkInteractorStyleImage> style` 设进交互器——**局部变量,用完即释放,没存成员**。
- 拾取靠 `eventFilter`(:141 installEventFilter)抢在交互器前截 press;但 native OpenGL 窗口下 Qt filter 与 VTK 交互器谁先拿到 press 不保证 → 红点时有时无。
- 两个模式(Seed / PathPoint)都经 `XQMainWindow` → `mprWidget_->setSeedPickingEnabled(enabled)` → 三个 `XQSliceViewWidget::setSeedPickingEnabled`(:181,唯一入口)。

### 头文件约束(已读实,务必遵守)
`XQSliceViewWidget.h` 是**严格 VTK-free**(:22-25 注释明写 "Header stays VTK-free",所有 VTK/QVTK 类型前置声明 + 指针持有),**且不是 pimpl**(成员直接列在 .h 私有区)。所以 style 成员**不能**用 `vtkSmartPointer` 直接放 .h(会把 VTK 引进 VTK-free 头,破坏纪律)。

### 修法(写死:浅 pimpl 存两个 style,VTK-free + 生命周期正确)
**生命周期分析(定死,别再纠结 void*)**:两个 style 必须被 widget 稳定持有。若只存裸 void* + 靠交互器 `SetInteractorStyle` 持一个引用——切到 image style 时,pick style 没有任何持有者会被回收,反之亦然。这是真 bug,不是"遇到再定"。唯一同时满足「.h VTK-free」和「生命周期正确」的解是**浅 pimpl**:只把两个 style 的 `vtkSmartPointer` 放进 .cpp 的一个小 struct,widget 持一个 `std::unique_ptr<StyleHolder>`。其余成员布局不动(不是全类 pimpl 化)。

1. **.h 私有区**:加一个前置声明的 holder 指针(其余成员保持不动):
   ```cpp
   struct StyleHolder;                       // defined in the .cpp (VTK types)
   std::unique_ptr<StyleHolder> styles_;     // owns the two interactor styles
   ```
   > 需 `#include <memory>`。析构 `~XQSliceViewWidget()` 现为 `= default`(:34/.cpp :146),unique_ptr 到不完整类型要求析构在 .cpp 可见 StyleHolder 定义——现状析构已在 .cpp(:146 `= default`),满足。
2. **.cpp 顶部**定义:
   ```cpp
   struct XQSliceViewWidget::StyleHolder {
       vtkSmartPointer<vtkInteractorStyleImage> image;  // window/level 常态
       vtkSmartPointer<vtkInteractorStyle> pick;        // 空 style,拾取模式无 window/level
   };
   ```
3. 构造时(:113-116)`styles_ = std::make_unique<StyleHolder>();` 建 image 与 pick;`SetInteractorStyle(styles_->image)`(替换现在的局部 `vtkNew style`)。offscreen 无交互器时 style 仍建好存 holder(供 probe),只是不 SetInteractorStyle。
4. **`setSeedPickingEnabled(bool enabled)`(:181)里接管**:
   ```cpp
   void XQSliceViewWidget::setSeedPickingEnabled(bool enabled) {
       seedPickingEnabled_ = enabled;
       // 光标(现状保留)
       // 交互接管:拾取模式换空 style,退出还原 image style
       auto* interactor = /* vtkWidget_->renderWindow()->GetInteractor(),判空 */;
       if (interactor != nullptr && styles_) {
           interactor->SetInteractorStyle(enabled ? styles_->pick : styles_->image);
       }
       ...(既有光标逻辑)
   }
   ```
   - 交互器句柄从 `vtkWidget_->renderWindow()->GetInteractor()` 取;offscreen 平台无交互器,判空跳过(与构造时 :114 同样的判空)。
5. eventFilter 的 MouseButtonPress 拾取分支(:249-262)**逻辑不变**——换空 style 后,VTK 不再抢 window/level,filter 稳定拿到 press,红点必出。空 style 兜底(即便 filter 偶尔没截到,空 style 也不会 window/level 出错误反馈)。

### 为什么换空 style 而非禁交互器
- 禁交互器会连带禁掉我们仍想要的东西(如果未来交互器上挂了别的);换空 style 精确地只关掉 window/level,保留交互器存活。用户已拍板此方案。

### 可测不变量
- 头文件加 probe `bool pickStyleActive() const`:返回「当前交互器 style == styles_->pick」。`setSeedPickingEnabled(true)` 后为 true,`false` 后为 false。**offscreen 无交互器**时,probe 改读一个跟随 setSeedPickingEnabled 设置的成员布尔(如复用 `seedPickingEnabled_`,或新增 `pickStyleEngaged_`),使断言在 headless 下也成立且反映「意图接管」。
- **注意 offscreen ctest 无 GL 无交互器**——接管代码判空后测试仍能跑;可测断言落在「setSeedPickingEnabled 切换后 pickStyleActive() 随之翻转」这一离散量。假绿抽查:篡改 setSeedPickingEnabled(如注释掉 SetInteractorStyle / 不翻转标志)→ 断言真转红。

## 2. 缺陷 B:体外拾取给反馈

### 现状(读实)
`eventFilter` MouseButtonPress(:249-262):`displayToWorld` && `worldToVoxelIndex` 都成功才 emit;失败什么都不做但 `return true` 吃掉点击 → 用户不知为何没成。

### 修法
- 拾取分支里,当 `displayToWorld` 成功但 `worldToVoxelIndex` 失败(点落体数据外),**发一个信号**(如 `emit pickOutOfBounds()` 或复用一个带原因的信号),`XQMainWindow` 接到后在状态栏提示「点击位置在图像范围外」(走 xqTr 可译串)。
- 十字线交点判定(`hitLine` :419 `mask==3`)已是严格判定(只有真正压在中心交点才 grab),**复核确认即可,不改**。
- 信号连接在 `XQMprWidget`(转发三个子视图信号)→ `XQMainWindow`。照 `voxelPicked` 现有转发链路加平行的 `pickOutOfBounds`。

### 可测不变量
- 构造一个已知体积,模拟 displayToWorld 返回体外世界坐标 → worldToVoxelIndex 失败 → 断言 pickOutOfBounds 被发出、voxelPicked 未发。(offscreen 下 displayToWorld 依赖 GL,可能测不了真点击;可测的是 worldToVoxelIndex 对体外坐标返回 false 这一核心逻辑——它在 XQRenderScene,已可 header 测。)

## 3. 缺陷 C:路径控制点视图标记(多点)

### 现状(读实)
- 控制点:`addPathDraftVoxel`(`XQMainWindow.cpp:2104`)push 进 `pathDraftPoints_`(`std::vector<PathControlPoint>`,每个含 world[3])+ 触发 `pathDraftChanged_` 回调 → 只刷右侧文字列表。
- `XQRenderScene` 只有**单点** seedMarker(`XQRenderScene.cpp:2202` setSeedMarker:4 视图各一个 sphereSource,SetCenter 移动单球)。控制点是**多点**,不能照抄单点球。

### 决策:多点用 glyph3D(一个 actor/视图承载 N 点)
- 每视图维护:`vtkPoints`(N 个控制点世界坐标)→ `vtkPolyData` → `vtkGlyph3D`(源为小 `vtkSphereSource`)→ mapper → 一个 actor。更新时重填 vtkPoints + Modified,actor 数恒定(4),点数动态。
- 与种子红球区分:控制点用**不同颜色**(如青/绿 `SetColor(0.2,0.7,0.9)`),半径可略小。
- 常驻 actor 模式照 `buildSeedMarkers`(:2084):在场景构建时 build 好 4 个 glyph actor,初始空/隐藏。

### XQRenderScene 新增接口(写死签名)
`XQRenderScene.h`(照 setSeedMarker :63-67 邻近加):
```cpp
// Path control-point markers: a resident glyph actor per view renders all
// current draft control points. setPathControlPoints replaces the whole set
// (empty clears). Point count is a test probe.
void setPathControlPoints(const std::vector<std::array<double, 3>>& worldPoints);
void clearPathControlPoints();
std::size_t pathControlPointCount() const;  // probe
```
> 入参用**世界坐标**(与 pathDraftPoints_ 存的一致,避免再过 voxel_to_world;setSeedMarker 走的是 voxel→world,但控制点 XQMainWindow 侧已有 world,直接传 world 更省)。类型 `std::array<double,3>` 保持 header VTK-free。
> Impl 侧:`buildPathControlMarkers()`(照 buildSeedMarkers)、成员 `vtkSmartPointer<vtkPoints> pathControlPoints_[4]`、`vtkSmartPointer<vtkActor> pathControlActors_[4]`、`std::size_t pathControlCount_ = 0`。

### XQMainWindow 接线
- `pathDraftChanged_` 回调(现在 = `hooks.refreshList` 刷文字列表,约 :811)里**追加**:把 `pathDraftPoints_` 的 world 收集成 `vector<array<double,3>>` 推给 `renderScene_->setPathControlPoints(...)`。
- 退出路径拾取 / 清空草稿时调 `clearPathControlPoints()`(或推空 vector)。
- 增删实时:每次 `pathDraftChanged_` 全量重推(N 通常几十个,重填 vtkPoints 廉价)。

### 可测不变量
- push 两个控制点 → 触发 pathDraftChanged_ → `renderScene_->pathControlPointCount() == 2`;清空 → == 0。header 测(不依赖 GL,glyph 的 vtkPoints 计数在 offscreen 也成立)。**这是 R3 假绿抽查的落点**:篡改推送逻辑(如不推、推错数)必须真转红。

## 4. 层次纪律

- `XQRenderScene`(visualization 层,可用 VTK):新增控制点 glyph 接口。
- `XQSliceViewWidget`(visualization):交互接管(style 成员)、体外反馈信号。
- `XQMprWidget`(visualization):转发 pickOutOfBounds 信号。
- `XQMainWindow`(app):接 pickOutOfBounds → 状态栏;pathDraftChanged_ → 推控制点给渲染层。
- **不动** core/services;不改整卷阈值/区域生长算法本身。

## 5. i18n

新增可译串(手工加进 `resources/i18n/xq_zh_CN.ts`,UTF-8 无 BOM + LF,走 xqTr=translate("XQStageWidgets",...),不跑 lupdate):
- "Click point is outside the image bounds" / "点击位置在图像范围外"(状态栏,若走状态栏文案)。
- 视具体文案再定;lrelease 后 0 unfinished。

## 6. 验收要点(硬门禁)

- **真机第一优先**(memory `gui-task-green-tests-not-done`):种子模式亮区连点 10 次每次出红点;控制点拾取后视图见标记、增删同步;体外点有反馈;退出拾取 window/level 恢复。
- ctest 全绿(基线 69 + 新增:pickStyle 切换不变量、pathControlPointCount 增删、worldToVoxelIndex 体外 false)。
- 假绿抽查:每个新断言篡改被测逻辑真转红(核 exe 时间戳变,走 ctest 不裸 exe)→ 还原绿。R3 的 pathControlPointCount 断言、A 的 style 切换断言都要覆盖。
- 改 Q_OBJECT 头(XQSliceViewWidget.h/XQRenderScene.h/XQMprWidget.h 若加 signals/成员)→ `rm -rf build_gui` 全新构建(memory `ninja-stale-moc`)。

## 7. 风险与注意

- **offscreen ctest 无 GL 无交互器**:接管代码、glyph build 都要判空/在无 GL 下不崩(现有 seedMarker/构造代码已有此判空范式,照抄)。
- **头文件 VTK-free 边界**:先查 XQSliceViewWidget.h 现状是否 pimpl/是否已含 VTK,新成员按既有模式放(Impl 或直接成员),别把 VTK 引进本是 VTK-free 的头。
- **displayToWorld / worldToVoxelIndex 依赖 GL/体积**:B 的真反馈部分真机验;可测的核心逻辑落在 worldToVoxelIndex 体外返回 false(header 测)。
- 种子与控制点复用 setSeedPickingEnabled 唯一入口 → 接管对两者都生效,无需分别处理(prd.md §R1 已注)。

---

## 8. 真机反馈第二轮:控制点交互增强(2026-07-06,主审直接实现)

R3 控制点标记真机验证通过(青色球出现),但用户反馈两点,主审在本任务内一并实现:

### R4 控制点按切片距离过滤显示(缺陷:标记恒显示不贴切片)
- 现象:控制点在所有切片恒显示,拖十字线切片不消失/不变。用户预期「标记只在它所在切面附近才显示」(PRD R3 原文已提「理想按切片距离过滤」)。
- 实现(`XQRenderScene.cpp`):
  - setPathControlPoints 存全量点到成员 `pathControlWorld_`;每视图 vtkPoints 是它的**切片距离过滤投影**。
  - 新增 `refreshPathControlMarkers()`:对每 2D 视图,点到该视图切片平面(planeAxis 的 `sliceWorldCoord`)距离 ≤ 一个 voxel-spacing band(`spacing[planeAxis]`)才填入;3D 视图(Volume3D)全显示。actor 可见性 = 该视图有点。
  - 挂载点:setPathControlPoints 末尾 + **setSliceIndex 末尾**(照既有 `rebuildContourOverlaysForAxis` 先例——轮廓叠加正是同款「按切片过滤」范式)。
  - 清理:clearPathControlPoints / volume reload 都清 `pathControlWorld_`。
- 可测:新增 probe `int pathControlVisibleCount(int axis)` 返回该视图当前过滤后填入的 glyph 点数(数组索引按 build 顺序 {Axial,Sagittal,Coronal,Volume3D} 反查,**不是 ViewId 枚举值**)。test_render_scene §21 加断言:z 切片=1 时 z=1 点显示、z=3 点滤除(visibleCount==1),切到 z=3 反转。假绿抽查:禁用过滤条件 → 两断言真转红(已验)。

### R5 右侧列表双击控制点跳转(PRD 外新增,用户要)
- 需求:双击右侧控制点列表项 → 三视图十字线跳到该控制点世界坐标。
- 实现:
  - `XQStageWidgets.h`:新增 `using PathDraftFocuser = std::function<void(int index)>` + context 字段 `pathDraftFocuser`。
  - `XQStageWidgets.cpp` buildPathPage:签名加 `draftFocuser` 参数;`pointList`(QListWidget)的 `itemDoubleClicked` → `draftFocuser(pointList->row(item))`。populateStagePanels 传 `context.pathDraftFocuser`。
  - `XQMainWindow`:`pathDraftFocuser` lambda → `focusPathDraftPoint(int index)`。该方法照**现成权威范式 `onNavLocEdited`**(world→worldToVoxelIndex→三轴 slider+spin 均 QSignalBlocker 设值→onNavigatorSliderChanged 统一刷新一次)。**关键坑**:只 blocker slider 会连带屏蔽 slider→spin 的 valueChanged 绑定(:1445)导致 spin 不同步——必须同时显式 blocker+setValue spin(onNavLocEdited 已这么做,照抄)。

### 验证
- 全新构建 243/243,全量 ctest 70/70 全绿(基线 69 + test_slice_view_pick_style;test_render_scene §21 扩过滤断言)。
- 假绿抽查:R4 过滤断言篡改真转红;A/C 前轮已验。
- 真机待验:控制点随切片显隐、双击跳转、spin 与 slider 同步。
