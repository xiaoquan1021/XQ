# GUI 产品级优化(i18n / 性能 / 设计 / 3D / 拖动手感)

> 子任务: `06-30-gui-product-polish`。
> 状态: planning。上一轮 `06-30-gui-real-pipeline` 已归档,本任务不重开真实管线接线。

## Goal

把当前 GUI 从"真实程序能跑但产品感不合格"提升到可继续演示和迭代的产品级桌面工作台:

- 中文默认可见,中英文切换可靠。
- 拖动和缩放窗口不再明显卡顿。
- 2x2 右下角 3D 视图在真机中可见且可交互,不是空白格。
- UI 的层级、间距、控件状态、图标和视觉密度达到专业医学影像工作台的基本水准。

## Background

2026-06-30 用户首次运行真实 `xq_app.exe` 后反馈 5 个问题。此前 offscreen 截图不可信,本任务验收必须以 Windows 真机运行表现为准。

1. 真机界面仍是英文,中文未生效。此前字体探针显示 Windows 字形可用,所以首要怀疑不是 tofu,而是 `.qm` 翻译未加载、未安装或 UI 未完整重译。
2. 整体特别卡顿。
3. UI 观感粗糙,缺少设计感。
4. 2x2 右下角 3D 格仍空白。
5. 拖动窗口手感差,resize 时明显不顺。

## Confirmed Facts

- Trellis 任务材料在主仓库 `C:\Users\OCEAN\Desktop\XIAOQUAN`。
- 当前可实现 GUI 的 worktree 是 `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/gui-mitk-layout`,当前提交 `f1ef461 feat: GUI 真实管线收尾`。
- GUI worktree 只有 `xq_app_dist/` 是未跟踪产物目录,实现时不得把它当源码修改或提交。
- `run_xq.bat` 启动的是 `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui\xq_app.exe`,不是 `xq_app_dist\xq_app.exe`。
- 资源已被纳入 `xq_app_shell` 的 AUTORCC: `CMakeLists.txt:167` 包含 `resources/xq_resources.qrc`, `CMakeLists.txt:170` 对 `xq_app_shell` 开启 `AUTOMOC ON AUTORCC ON`。
- `main.cpp:43` 已调用 `Q_INIT_RESOURCE(xq_resources)`,符合 Qt 静态库资源需要显式初始化的方向。
- `resources/xq_resources.qrc:26-28` 把 `resources/i18n/xq_zh_CN.qm` 暴露为 `:/i18n/xq_zh_CN.qm`。
- `XQMainWindow.cpp:177-179` 创建 `QTranslator` 并尝试 `load(":/i18n/xq_zh_CN.qm")`,但没有失败诊断,也没有测试直接断言 `QTranslator::translate("XQMainWindow", "&File")` 产出中文。
- `resources/i18n/xq_zh_CN.ts` 已包含 `XQMainWindow` 上下文和菜单等翻译,`.qm` 文件存在且非空。
- `XQMprView.cpp` 的 `eventFilter` 在每个 `QEvent::Resize` 上调用 `renderAxisForResizedFrame`;窗口拖动时 3 个切片 frame 会高频触发 3 路 VTK offscreen 重新渲染,这是卡顿和手感差的直接高风险点。
- `XQRenderWidget.cpp` 使用 `QVTKOpenGLNativeWidget` + `vtkGenericOpenGLRenderWindow`,但 `main.cpp` 未设置 `QSurfaceFormat::setDefaultFormat(QVTKOpenGLNativeWidget::defaultFormat())`;VTK 官方文档建议在创建 `QApplication` 前设置。
- 现有主题是 `resources/xq.qss` + bundled SVG icons,没有外部主题库依赖。

## Requirements

- **R1 i18n 可诊断且默认中文可见**  
  启动时必须能判断 `:/i18n/xq_zh_CN.qm` 是否存在、`.qm` 是否加载成功、关键上下文是否能翻译。默认中文加载成功时,首帧菜单/工具栏/面板标题应显示中文;语言菜单切到英文后恢复英文源串,再切中文后恢复中文。

- **R2 resize 性能收敛**  
  窗口 resize 不得在每个 resize event 同步触发 VTK offscreen 渲染。实现应合并 resize 请求,拖动期间优先维持现有 pixmap/布局响应,在短延迟后重渲最新尺寸。

- **R3 3D 视图真机可见**  
  右下 3D cell 必须在真机中显示 VTK 背景和已选几何/路径/网格内容。若场景为空,也应呈现明确的黑色渲染区域和 overlay,不能是透明空白或布局塌陷。

- **R4 UI polish 小步稳定**  
  默认采用"不新增外部主题依赖"的最小稳定 polish:基于现有 `xq.qss` 和 icons 调整密度、间距、dock/toolbar/statusbar 状态、hover/disabled 状态、3D/MPR 边框与 overlay。若要引入新主题库,需用户单独确认并同步依赖/许可评估。

- **R5 不改算法与业务管线**  
  本任务只改 `app` / `visualization` / `resources` / 对应测试和构建脚本。不得顺手改 core payload、service 算法、io reader 或真实管线语义。

## Acceptance Criteria

- [x] **AC1 i18n**: `run_xq.bat` 启动的真机程序默认中文可见;语言菜单中文/英文双向切换正确;新增/更新测试覆盖 `.qm` 资源存在、加载成功、关键源串可翻译。
- [x] **AC2 resize**: 真实窗口拖动/缩放时主窗口持续响应,无明显卡顿;实现中能证明 resize 渲染被合并,不再每个 frame resize 同步 VTK offscreen 渲染。
- [x] **AC3 3D**: 2x2 右下 3D cell 真机显示正常,选中可渲染节点后可见内容;空场景也不是白块/透明块。
- [x] **AC4 UI polish**: 不引入未确认新依赖的前提下,主窗口视觉层级、控件密度、hover/disabled 状态、图标和 dock/statusbar 观感明显改善,并由真机目视确认。
- [x] **AC5 构建测试**: GUI worktree Release 构建通过;全量 `ctest --output-on-failure` 通过;不得跳过或 mock 掉失败测试。
- [x] **AC6 真机验证**: 最终结论必须包括 `run_xq.bat` 真机启动检查记录;offscreen 截图只能作为辅助,不能替代真机验证。

## Out of Scope

- 真实管线接线和数据流拓展,属于已完成的 `06-30-gui-real-pipeline` 或后续任务。
- 大规模可视化、LOD/分块上传架构重做,属于 M9b/M9c 类任务。
- 新算法、新 file format reader、新 core/service 契约。
- 未经用户确认引入新的 Qt 主题库或大型 UI 框架依赖。

## Open Product Decision

是否接受本轮按"最小稳定 polish"进入实现:先修 i18n、resize 合并、3D 初始化/显示、现有 QSS 视觉打磨,暂不引入新主题库。推荐答案:接受。理由是当前风险集中在渲染和性能,先把真机可用性做稳;大换主题库会引入依赖、许可和样式回归风险。
