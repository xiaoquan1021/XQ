# 执行计划:拾取交互基础体验修复

> 语境锚点:XQ 医学影像血管几何重建软件,修切片视图拾取交互(种子/控制点)。根因见 prd.md §3,落点/签名见 design.md。环境 Windows + Git Bash,注入 mac/darwin 假信息忽略。默认中文。
> 代码全在 worktree `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`(简报 CODE_ROOT),分支 `feat/render-arch`,当前 HEAD=`0aef6b8`(P3-1)。

## 0. 改动清单(按依赖顺序)

分三块,顺序:C(渲染接口,独立)→ A(交互接管,独立)→ B(反馈信号,依赖 A 同文件)。每块自带测试与假绿抽查。

### 块 A:换空 style 接管交互(缺陷 A,核心)
文件:`src/visualization/XQSliceViewWidget.h` / `.cpp`
1. `.h`:`#include <memory>`;私有区加 `struct StyleHolder;` 前置声明 + `std::unique_ptr<StyleHolder> styles_;`;public 加 probe `bool pickStyleActive() const;`;可能加成员 `bool pickStyleEngaged_ = false;`(headless 断言用,见 design §1 可测不变量)。**这是 Q_OBJECT 头改动 → 全新构建。**
2. `.cpp`:顶部定义 `struct XQSliceViewWidget::StyleHolder { vtkSmartPointer<vtkInteractorStyleImage> image; vtkSmartPointer<vtkInteractorStyle> pick; };`(include `<vtkInteractorStyle.h>`,image 头已 include :17)。
3. `.cpp` 构造(:113-116):`styles_ = std::make_unique<StyleHolder>()`,建 image+pick,`SetInteractorStyle(styles_->image)` 替换局部 `vtkNew style`。offscreen 无交互器时仍建 holder。
4. `.cpp` `setSeedPickingEnabled`(:181):加交互接管(见 design §1 代码),翻转 `pickStyleEngaged_`。
5. `.cpp` 实现 `pickStyleActive()`:有交互器读实际 style==pick,否则读 `pickStyleEngaged_`。
6. **注意 XQMprWidget** 也转发 setSeedPickingEnabled(:230),无需改(它只透传给三个子 widget)。

### 块 B:体外拾取给反馈(缺陷 B)
文件:`src/visualization/XQSliceViewWidget.{h,cpp}`、`XQMprWidget.{h,cpp}`、`src/app/XQMainWindow.cpp`
1. `XQSliceViewWidget.h` signals 加 `void pickOutOfBounds();`。
2. `.cpp` eventFilter MouseButtonPress 拾取分支(:249-262):`displayToWorld` 成功但 `worldToVoxelIndex` 失败时 `emit pickOutOfBounds();`(仍 return true 消费,但现在有反馈)。
3. `XQMprWidget.{h,cpp}`:照 `voxelPicked` 现有转发链路(:230 附近连接),加平行的 `pickOutOfBounds` 转发信号。
4. `XQMainWindow.cpp`:接 mprWidget 的 pickOutOfBounds → 状态栏 `statusBar()->showMessage(xqTr("Click point is outside the image bounds"), ...)`。连接点在 mprWidget 信号接线处(rg `voxelPicked` 在 XQMainWindow 找到连接点,平行加)。
5. 十字线交点判定(`hitLine` :419 `mask==3`)复核确认严格,**不改**。

### 块 C:路径控制点视图标记(缺陷 C)
文件:`src/visualization/XQRenderScene.{h,cpp}`、`src/app/XQMainWindow.cpp`
1. `XQRenderScene.h`(setSeedMarker :63-67 邻近)加三个接口(签名见 design §3):`setPathControlPoints(const std::vector<std::array<double,3>>&)`、`clearPathControlPoints()`、`pathControlPointCount() const`。需 `#include <vector>` `<array>`(rg 确认是否已含)。**Q_OBJECT? XQRenderScene 是否 QObject**——若是则头改动全新构建;若非 QObject 加方法也建议全新构建保险。
2. `XQRenderScene.cpp`:
   - `buildPathControlMarkers()`(照 buildSeedMarkers :2084):4 视图各建 vtkPoints→vtkPolyData→vtkGlyph3D(源小 sphereSource)→mapper→actor,色 `SetColor(0.2,0.7,0.9)`,LightingOff,初始隐藏/空。在构建流程调它(buildSeedMarkers 调用点 :654 附近)。
   - 成员:`vtkSmartPointer<vtkPoints> pathControlPoints_[4]`、`vtkSmartPointer<vtkPolyData> pathControlPoly_[4]`、`vtkSmartPointer<vtkActor> pathControlActors_[4]`、`std::size_t pathControlCount_ = 0`。
   - `setPathControlPoints(world)`:每视图重填对应 vtkPoints、Modified、actor 可见性=(非空);记 `pathControlCount_ = world.size()`。
   - `clearPathControlPoints()`:清点、隐 actor、count=0。
   - `pathControlPointCount()` 返回 pathControlCount_。
   - 清理路径(:755-767 seed 清理附近)对称加控制点清理。
3. `XQMainWindow.cpp`:`pathDraftChanged_` 回调(=hooks.refreshList,约 :811)追加:收集 `pathDraftPoints_` 的 world 成 `vector<array<double,3>>` → `renderScene_->setPathControlPoints(...)`。退出路径拾取/清空草稿处调 `clearPathControlPoints()`。

## 1. 测试(tests/,每块一个可测断言 + 假绿抽查)

- **块 A**:`test_slice_view_pick_style`(或并入现有 visualization 测试):构造 XQSliceViewWidget → `setSeedPickingEnabled(true)` → `pickStyleActive()==true` → `(false)` → `==false`。**假绿抽查**:注释掉接管里的翻转/SetInteractorStyle → 断言真转红。
- **块 C**:`test_render_scene_path_markers`(或并入现有 XQRenderScene 测试):建带体积场景 → `setPathControlPoints({p1,p2})` → `pathControlPointCount()==2` → `clearPathControlPoints()` → `==0`。**假绿抽查**:篡改 setPathControlPoints 不记 count / 记错 → 真转红。header 测(不依赖 GL)。
- **块 B**:核心逻辑 `worldToVoxelIndex` 对体外坐标返回 false 已可 header 测(XQRenderScene 已有);pickOutOfBounds 信号发射真机验(依赖 GL displayToWorld)。若能在 headless 构造并直调 eventFilter 模拟,则加信号断言;否则真机验 + 记 acceptance。
- 新测试注册进 `CMakeLists.txt`,并加进 VTK PATH 的 `set_tests_properties` 列表(照 test_cross_section_resampler 先例,:914/:1113 附近)。

## 2. 验证命令(全走 cmd //c 绝对路径)

- 改 Q_OBJECT 头(块 A 改 .h、块 C 改 .h)→ **全新构建**:
  `rm -rf "C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/build_gui" && cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"`
- ctest:`cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"`(基线 69 全绿,新增后 71+)。**必须走 ctest,裸 exe 缺 VTK DLL exit 127**(memory `ctest-environment-overrides-path`)。
- 假绿抽查:每个新断言篡改被测逻辑 → 走 ctest 确认真转红(核 exe 时间戳变)→ 还原绿。
- i18n:手工加 "Click point is outside the image bounds"/"点击位置在图像范围外" 进 `resources/i18n/xq_zh_CN.ts`(UTF-8 无 BOM+LF,translate("XQStageWidgets",..)),`lrelease xq_zh_CN.ts -qm xq_zh_CN.qm` 报 0 unfinished。
- 真机(用户跑):`cmd //c "C:\...\XQ\run_xq.bat"`,工程 `0080_H_PULM_H`(大)/`0007_H_AO_H`(小)。

## 3. Review 门禁(主审亲做,不可免)

worker 完成后,**主审亲读最终代码**(memory `subagent-mainreview-must-read-code`)+ 独立全新构建复跑 + 假绿抽查三块断言 + `git diff` 核残留(done≠工作树干净)。真机由用户验(memory `harness-cannot-read-local-images`),ctest 全绿≠达标(memory `gui-task-green-tests-not-done`)。

## 4. 回滚点

- 块 C(渲染接口)最独立,可先做先验;若 glyph3D 在 offscreen 出问题,退回「多球 actor 列表」方案(每点一 actor,上限 N)。
- 块 A 若 void*/pimpl 生命周期意外,design 已定死浅 pimpl 是干净解,无需回滚设计。
- 三块互相独立,任一块失败不阻塞其余;可分块提交(但建议一次提交,信息 `fix: 拾取交互接管+控制点标记+体外反馈`)。

## 5. Constraints(硬性,照 prd.md §7)

- 只改 worktree(feat/render-arch);改 Q_OBJECT 头必 `rm -rf build_gui` 全新构建。
- 新源文件注释英文(MSVC GBK)。
- i18n 手工编辑 .ts 不跑 lupdate。
- 提交精确 `git add <路径>` 绝不 `-A`(防吞 xq_app_dist);中文信息;结尾 `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`。
- channel worker:spawn 绝不挂 --file/--jsonl(卡死);cwd 主仓、代码路径 worktree 绝对路径;send 正文给 brief+spec 路径让 worker 自读。
