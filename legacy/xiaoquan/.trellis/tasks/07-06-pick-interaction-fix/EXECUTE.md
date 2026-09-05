# EXECUTE:拾取交互基础体验修复(种子/控制点确定性 + 可视反馈)

> 语境锚点:XQ 医学影像软件血管几何重建。本批修「在切片图像上点一个点」这一底层交互(路径控制点拾取、分割种子拾取复用同一链路)的可靠性与反馈。纯桌面 GUI 交互工程,不碰任何分割/建模算法。父任务 07-06-along-path-contouring。

## 活动任务

- 任务:`07-06-pick-interaction-fix`(in_progress)。
- **两个不同的 git 工作树,别混**:
  - **代码工作树(唯一改代码处)= `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`**,分支 `feat/render-arch`,HEAD=`0aef6b8`(P3-1)。本简报下文所有 `CODE_ROOT/...` 都指这里,所有 src/tests/CMakeLists 编辑、build、ctest 全在这个工作树,**用绝对路径**。
  - 任务文档/spec/agent 定义在主仓 `C:\Users\OCEAN\Desktop\XIAOQUAN`(= 你的 cwd,读 prd/design/implement 用相对路径)。
- **凡本简报写 `CODE_ROOT`,替换为 `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`**。绝不在主仓 XIAOQUAN 下建/改任何 src 代码。

## 第一步:先读这三份任务文档(根因/签名/落点已全写死)

用 Read 工具依次读(主仓相对路径,cwd 已是主仓):
1. `.trellis/tasks/07-06-pick-interaction-fix/prd.md` —— 三个缺陷的代码级根因(§3)、需求 R1/R2/R3、验收标准。
2. `.trellis/tasks/07-06-pick-interaction-fix/design.md` —— 每个缺陷的**修复落点、接口签名、数据结构决策**(已写死,不自由发挥)。**尤其 §1 的浅 pimpl StyleHolder 方案是定死的,别改回 void*(pick style 会被提前回收)。**
3. `.trellis/tasks/07-06-pick-interaction-fix/implement.md` —— 改动顺序(块 A/B/C)、测试、验证命令、回滚点。

**本简报只给流程约束与验收命门;具体怎么改看 design.md/implement.md。三份文档冲突时以 design.md 的签名为准。**

## 目标(本批范围,严格不越界)

修既有拾取交互的三个真实缺陷,让「在切片上点一个点」确定成功、有即时可视反馈:
- **A(核心)**:拾取模式激活时**换空 style 接管 VTK 交互**(vtkInteractorStyleImage → 空 vtkInteractorStyle),使点击有唯一确定处理路径,红点/标记必出;退出还原。根治「红点时有时无」。
- **B**:点在体数据外不再静默,发信号 → 状态栏提示。
- **C**:路径控制点拾取后在切片视图画出**可见标记**(多点 glyph3D,`XQRenderScene` 新接口),随增删实时更新。

**本批不做**:选路径下拉(归 P3-2)、断面轮廓手绘(P3-2)、不动整卷阈值/区域生长算法本身、不重构整个事件系统。

## 关键实现约束(design.md 已详,这里只提命门)

### 缺陷 A —— 换空 style,浅 pimpl 持有(定死)
- `XQSliceViewWidget.h` 是**严格 VTK-free 且非 pimpl**(:22-25 注释)。style **不能**用 vtkSmartPointer 直接进头。
- **定死方案**:浅 pimpl —— `.h` 加 `struct StyleHolder;` 前置声明 + `std::unique_ptr<StyleHolder> styles_;`(需 `#include <memory>`);`.cpp` 顶部定义 `struct XQSliceViewWidget::StyleHolder { vtkSmartPointer<vtkInteractorStyleImage> image; vtkSmartPointer<vtkInteractorStyle> pick; };`。**别用 void* 裸指针**(切 style 时另一个会被回收=真 bug)。
- 接管点:`setSeedPickingEnabled`(:181,种子/控制点唯一入口)—— `interactor->SetInteractorStyle(enabled ? styles_->pick : styles_->image)`,交互器从 `vtkWidget_->renderWindow()->GetInteractor()` 取并判空(offscreen 无交互器,与构造 :114 同判空)。
- 构造(:113-116):建 holder，`SetInteractorStyle(styles_->image)` 替换局部 `vtkNew style`。
- probe `bool pickStyleActive() const`:有交互器读实际 style==pick,headless 读跟随开关的成员布尔(如 `pickStyleEngaged_`)。

### 缺陷 B —— 体外拾取发信号
- `XQSliceViewWidget` signals 加 `void pickOutOfBounds()`;eventFilter 拾取分支(:249-262)`displayToWorld` 成功但 `worldToVoxelIndex` 失败时 emit(仍 return true)。
- `XQMprWidget` 照 `voxelPicked` 现有转发链路(:230 附近)平行加 `pickOutOfBounds` 转发。
- `XQMainWindow` 接 → `statusBar()->showMessage(xqTr("Click point is outside the image bounds"), ...)`。连接点在 mprWidget voxelPicked 接线处平行加。
- 十字线 `hitLine` :419 `mask==3` 已严格,复核**不改**。

### 缺陷 C —— 控制点 glyph3D 标记(多点)
- 控制点是**多点**,**不能照抄单点球 seedMarker**。定死用 glyph3D:每视图 vtkPoints→vtkPolyData→vtkGlyph3D(源小 sphere)→一个 actor 承载 N 点。
- `XQRenderScene.h`(setSeedMarker :63-67 邻近)加(签名照 design §3):
  ```cpp
  void setPathControlPoints(const std::vector<std::array<double, 3>>& worldPoints);
  void clearPathControlPoints();
  std::size_t pathControlPointCount() const;  // probe
  ```
- `.cpp`:`buildPathControlMarkers()` 照 `buildSeedMarkers`(:2084),色 `SetColor(0.2,0.7,0.9)` 区分红球,LightingOff,初始隐;在构建流程(buildSeedMarkers 调用点 :654 附近)调用;清理路径(:755-767 附近)对称清控制点。
- `XQMainWindow` `pathDraftChanged_` 回调(约 :811)追加:收集 `pathDraftPoints_` 的 world → `renderScene_->setPathControlPoints(...)`;退出路径拾取/清空处 `clearPathControlPoints()`。

## 测试(离散不变量,headless ctest;假绿抽查必做)

- **块 A**:构造 XQSliceViewWidget → `setSeedPickingEnabled(true)`→`pickStyleActive()==true`→`(false)`→`==false`。假绿:注释掉接管翻转 → 真转红。
- **块 C**:带体积场景 → `setPathControlPoints({p1,p2})`→`pathControlPointCount()==2`→`clearPathControlPoints()`→`==0`。假绿:篡改 setPathControlPoints 不记/记错 count → 真转红。header 测不依赖 GL。
- **块 B**:核心逻辑 `worldToVoxelIndex` 体外返回 false 已可 header 测;pickOutOfBounds 信号真机验(依赖 GL)。
- 新测试注册进 `CODE_ROOT/CMakeLists.txt` 并加进 VTK PATH 的 `set_tests_properties` 列表(照 test_cross_section_resampler 先例,:914/:1113 附近)。断言必须可证伪:先 park 反面确认红,再改对确认绿。

## 验证命令(worker 必跑,报告贴输出)

1. 改 Q_OBJECT 头(块 A 改 .h、块 C 改 .h)→ **全新构建**:
   ```
   rm -rf "C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui" && cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
   ```
2. ctest(基线 69 + 新增 → 期望 71+ 全绿):
   ```
   cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"
   ```
3. 假绿抽查:块 A、块 C 各篡改被测逻辑 → 走 ctest 该测试必 FAIL(核 exe 时间戳变)→ 还原 → 复绿。报告写明篡改点 + 前后结果。
4. i18n:手工加 "Click point is outside the image bounds"/"点击位置在图像范围外" 进 `CODE_ROOT/resources/i18n/xq_zh_CN.ts`(UTF-8 无 BOM+LF,translate("XQStageWidgets",..)),`lrelease xq_zh_CN.ts -qm xq_zh_CN.qm` 报 0 unfinished。**不跑 lupdate。**

## 禁止(违反=返工)

- 不做选路径下拉/断面手绘/放样(超范围,归 P3-2)。
- 不动整卷阈值/区域生长算法本身;不重构整个事件系统。
- 头文件不 include VTK(XQSliceViewWidget.h / XQRenderScene.h 保持 VTK-free,VTK 只在 .cpp)。style 用浅 pimpl 不用 void*。
- 新源文件注释一律英文(MSVC GBK 坑)。
- 不 git commit(worker 无提交权;主审复核后提交)。
- 不 `git add -A`(防吞 xq_app_dist 构建产物)。
- 不跑 lupdate(手工编辑 .ts)。

## 交付物(全部在 CODE_ROOT = C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ)

- 改动:src/visualization/XQSliceViewWidget.{h,cpp}(A 接管 + B 信号)、src/visualization/XQMprWidget.{h,cpp}(B 转发)、src/visualization/XQRenderScene.{h,cpp}(C 控制点 glyph 接口)、src/app/XQMainWindow.cpp(B 状态栏 + C 推控制点)、resources/i18n/xq_zh_CN.ts+.qm(B 文案)、CMakeLists.txt(挂新测试)。
- 新增:tests 下块 A / 块 C 的测试(可并入现有 visualization 测试文件或新建)。
- 报告:构建输出、ctest 结果(N/N)、块 A+C 假绿抽查前后结果、`git diff --stat`、遗留问题(尤其真机待验项:红点连点、控制点标记、体外反馈、退出还原)。
