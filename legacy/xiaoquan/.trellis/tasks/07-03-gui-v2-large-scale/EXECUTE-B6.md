# EXECUTE-B6 — S6 视觉打磨 + i18n 收口 + 收尾验收

> **执行者须知**:本文档基于 `feat/gui-v2` 真实源码核实(qss 794 行、色值分布、MPR 四格构建、i18n 管线均已 rg/Read 过)。B1~B5 全部完成后才能开工(`git log` 核对;缺任何一批停下报告)。照做,不做本文之外的设计决策。
> **本批性质**:纯视觉/文案/收口,**不改任何算法与数据流**;唯一的新交互元素是空场景提示 QLabel。

---

## 0. 环境与固定纪律

- 工作树:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/gui-v2`(Git Bash:`/c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`)。
- 完整构建:`cmd //c build_gui_wt.bat`;全量测试:`cmd //c ctest_merge.bat`。
- 真机启动(目视验收用):`cmd //c run_xq.bat`(已核实脚本存在,负责 Qt/VTK/tinyxml2/MMG PATH)。
- **绝不 `cmake --build --target <单目标>`**;绝不 `git add -A`。
- 全局约束:只做本批;**不引新依赖、不换主题库、不写像素断言**(假绿:15% 容差吞亚像素差异,划分类特性验收在源头不在像素——memory 坑);不覆盖未提交改动;services/core 零 Qt/VTK;不破坏 Source 1.0;不参考 XQ1(**视觉资产自研,不从 XQ1 抄**);没验证过不写"完成/通过"。
- 基线:全量 ctest **66/66**(B5 收尾基线);不是 66 或有红 → 停下报告。

本批 3 个 commit:C1 qss token 归一 + 视图身份色 + 工具栏/状态栏细化;C2 空场景提示;C3 i18n 收口 + ts 全量补翻。真机目视贯穿最后。

---

## 1. rg 定位与现状清单(已核实)

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rg -o '#[0-9A-Fa-f]{6}\b' resources/xq.qss | sort | uniq -c | sort -rn
rg -n 'styleSliceFrame|styleVolumeFrame|makeAxisLabel' src/visualization/XQMprView.cpp
rg -n 'addSeparator' src/app/XQMainWindow.cpp
rg -n 'xqStatusMem|xqStatusNodeCount|xqStatusPosition' resources/xq.qss src/app
rg -n 'xqEmptySceneHint' src   # 应零命中
rg -n 'stats.ok' src/app/XQMainWindow.cpp
```

现状事实(全部已核实):
- `resources/xq.qss` 794 行,头部注释是风格说明(无 token 表)。色值分布主力:`#D5DCE5`(30)边框、`#1F2937`(24)文字、`#2563C5`(18)强调、`#FFFFFF`(16)面板、`#3B82D0`(16)第二强调、`#EEF2F6`(15)工具栏/状态栏底、`#EDF3FA`(10)hover、`#B5BDC8`(10)禁用、`#E8F1FC`(8)dock 标题底、`#A8B2BF`(8)、`#DCEAF9`(7)选中、`#C4CBD4`(6)、`#F5F7FA`(5)背景、`#E6EAF0`(5)、`#5F6B7A`(5)次级文字、`#FDECEC`(3)警示底、`#DCE8T6→#DCE8F6`(3)pressed、`#D96B6B`(3)警示、`#000000`(2)、`#AAB3BF`(1)、`#9F2D2D`(1)、`#8A96A6`(1)、`#354052`(1)。
- **design.md 色板表(#2563EB 等)与 qss 实际主力色(#2563C5/#3B82D0)不一致**——定论见 2.1:**以 qss 现值为 token 基准**,归一的是"离群近似色",不是全局换色(避免真机全 UI 变调,超出"微调"边界)。
- MPR 四格边框:C++ 内联 setStyleSheet(styleSliceFrame/styleVolumeFrame,XQMprView.cpp:50-85),现色 Axial `#FF0000`/Sagittal `#00C000`/Coronal `#008BFF`/3D `#D6D600`(:606/:626/:647/:668)。角标签 makeAxisLabel:`rgba(0,0,0,0.4)` 底白字 11px(:100-110)。
- 工具栏分隔:buildMainToolBar 已有 2 处 addSeparator(:787/:795——Open/Save‖Undo/Redo‖后段);**阶段钮组前没有分隔**(:795 后直接 tbImageAction_)。
- QToolButton 三态:hover/pressed/checked/disabled 四态**已全**(qss:119-145);`QToolBar#mainActionsToolBar QToolButton` 已有 min-width/min-height/padding(:155-159),icon 22px(:151)。
- 状态栏:`QLabel#xqStatusMem, QLabel#xqStatusNodeCount { border-left: 1px solid #D5DCE5; }`(qss:649-652)已是分段线;字号未单独设(全局 13px)。
- Dock 标题:`QDockWidget::title` padding 8px 10px、font-weight 600(qss:168-177)。
- `onSceneSelectionChanged` 尾部 `if (stats.ok) { centralStack_->setCurrentWidget(mprView_); renderWidget_->render(); }`(XQMainWindow.cpp:2006-2011)。
- glyph 测试:`tests/app/test_ui_font_glyphs.cpp`(字体覆盖断言);i18n 测试:`tests/app/test_i18n_resources.cpp`。
- lupdate/lrelease:`/c/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/{lupdate,lrelease}.exe`(已核实存在)。
- ts 结构:4 个 context(xq::XQMainWindow / XQStageWidgets / xq::XQSceneModel / xq::XQMprView),149 词条(B3~B5 后会更多)。

---

## 2. Commit 1 — qss token 归一 + 四视图身份色 + 工具栏/状态栏细化

### 2.1 token 表与归一(**基准 = qss 现值**)

qss 头部注释块(`:1-8`)替换为带 token 表的版本:
```css
/*
 * XQ desktop theme
 *
 * Calm, dense medical/engineering desktop style:
 * - Cold neutral surfaces, thin 1px borders, restrained blue accents.
 * - 4px control radius, compact 12-13px system fonts.
 * - No gradients, glass, heavy shadows, large cards, or saturated decoration.
 *
 * Color tokens (single source of truth -- do not introduce near-duplicates):
 *   window background   #F5F7FA
 *   panel background    #FFFFFF
 *   toolbar/status bg   #EEF2F6
 *   border              #D5DCE5
 *   border/handle hover #A8B2BF
 *   text                #1F2937
 *   text secondary      #5F6B7A
 *   accent              #2563C5
 *   accent secondary    #3B82D0
 *   accent hover bg     #EDF3FA
 *   accent selected bg  #DCEAF9
 *   accent pressed bg   #DCE8F6
 *   dock title bg       #E8F1FC
 *   disabled text       #B5BDC8
 *   danger              #D96B6B  (bg #FDECEC, deep text #9F2D2D)
 *   alt row / groove    #E6EAF0, scrollbar handle #C4CBD4
 *   hint amber          border #E4B65B / bg #FFF7E6 / text #8A5A00 (C++ inline)
 *
 * View identity borders (MPR cells, set in XQMprView.cpp):
 *   Axial #C43C3C / Sagittal #3C9C4A / Coronal #3C6CC4 / 3D #C4913C
 */
```
**离群色替换清单(固定,恰好 3 处,不多不少)**——每个源值的全部选择器用途已逐处核实,以下是唯一要执行的替换:

| 行号锚(执行时 rg 重定位) | 选择器 | 替换 |
|---|---|---|
| :144 | `QToolButton:disabled { color: ... }` | `#AAB3BF` → `#B5BDC8`(禁用文字统一 token) |
| :408 | `QLineEdit { placeholder-text-color: ... }` | `#8A96A6` → `#5F6B7A`(占位文字并入次级文字) |
| :121 | `QToolButton { color: ... }` | `#354052` → `#1F2937`(正文统一 token) |

**明确保留、不许替换的值**(已核实各有独立职责,已收进上方 token 表):`#A8B2BF` 全部 8 处(hover 加深的边框/滚动条把手/分隔条,与禁用文字 #B5BDC8 职责不同,并掉会杀 hover 对比);`#9F2D2D`(dangerButton 深文字,:348);`#C4CBD4`/`#E6EAF0`(滚动条把手/交替行灰阶)。
替换后核查:`rg -c '#AAB3BF|#8A96A6|#354052' resources/xq.qss` 必须零命中(rg 退出码 1)。
**不改任何选择器结构、不增删规则块。**
C++ 内联样式:已核实 XQStageWidgets.cpp 的 `#5F6B7A`(subtitle,:93)已是 token,不动;hint bar 琥珀三件套 `#E4B65B/#FFF7E6/#8A5A00`(:129)是功能色,值不动,已收进上方 token 表(hint amber 行)。**本节对 C++ 文件零改动。**

### 2.2 四视图身份色(改 C++ 内联常量,4 处字符串)

`src/visualization/XQMprView.cpp`(改 C++ 内联常量是定形方案:边框色在 C++ setStyleSheet 拼接,qss 里没有这些选择器,qss 路不通):
1. before(:606)`styleSliceFrame(impl_->axial.frame, "xqMprAxialFrame", "#FF0000");`
   after `styleSliceFrame(impl_->axial.frame, "xqMprAxialFrame", "#C43C3C");`
2. before(:626)`... "xqMprSagittalFrame", "#00C000");` after `... "#3C9C4A");`
3. before(:647)`... "xqMprCoronalFrame", "#008BFF");` after `... "#3C6CC4");`
4. before(:668)`styleVolumeFrame(volumeFrame, "xqMprVolumeFrame", "#D6D600");` after `... "#C4913C");`
(降饱和:纯 RGB 荧光 → 与浅色主题同灰度的柔和身份色。)

角标签统一半透明深底,before(:104-110,makeAxisLabel 内):
```cpp
    label->setStyleSheet(
        "QLabel { color: #FFFFFF; background: rgba(0, 0, 0, 0.4); "
        "border: none; border-radius: 3px; padding: 1px 4px; "
        "font-size: 11px; }");
```
after:
```cpp
    label->setStyleSheet(
        "QLabel { color: #FFFFFF; background: rgba(31, 41, 55, 0.72); "
        "border: none; border-radius: 3px; padding: 1px 4px; "
        "font-size: 11px; }");
```
makeInfoLabel(:117-121)的 `rgba(0, 0, 0, 0.4)` 同步改 `rgba(31, 41, 55, 0.72)`(右下角切片信息与角标签同底,视觉一致)。

### 2.3 工具栏分组分隔

`buildMainToolBar`:`toolBar->addSeparator();`(:795,Undo/Redo 之后)已有;阶段钮组内部再加一刀——`tbImageAction_` 归"数据"、`tbPathAction_` 起是管线阶段。before(逐字,:809-821 一带):
```cpp
    tbImageAction_ = toolBar->addAction(themeIcon("tool-image.svg"), QString());
    tbImageAction_->setObjectName(QStringLiteral("xqToolbarImageAction"));
    QObject::connect(tbImageAction_, &QAction::triggered, this, [this]() {
        if (mprView_ != nullptr) {
            centralStack_->setCurrentWidget(mprView_);
        }
        if (stageDock_ != nullptr) {
            stageDock_->hide();
        }
    });
    tbPathAction_ = toolBar->addAction(themeIcon("tool-path.svg"), QString());
```
after(中间插一行):
```cpp
    tbImageAction_ = toolBar->addAction(themeIcon("tool-image.svg"), QString());
    tbImageAction_->setObjectName(QStringLiteral("xqToolbarImageAction"));
    QObject::connect(tbImageAction_, &QAction::triggered, this, [this]() {
        if (mprView_ != nullptr) {
            centralStack_->setCurrentWidget(mprView_);
        }
        if (stageDock_ != nullptr) {
            stageDock_->hide();
        }
    });
    toolBar->addSeparator();
    tbPathAction_ = toolBar->addAction(themeIcon("tool-path.svg"), QString());
```
QToolButton padding/icon:现状 icon 22px、padding 6px 8px(qss:151/:158)已达标(design 写 6px/20px 是下限)——**不动**,在 result 注明"已满足"。

### 2.4 状态栏与 Dock 细化(qss)

- `QStatusBar QLabel`(:642-646)加一行 `font-size: 12px;`(段间线已有,:649-652 border-left)。
- `QDockWidget::title`(:168-177)padding 8px→改 `padding: 6px 10px;`,加 `font-size: 13px;`(font-weight 600 已有=半粗)。
- 三态完整性核查(不改,只验):`rg -n ':hover|:pressed|:disabled' resources/xq.qss | wc -l` 输出记进 result;QPushButton/QToolButton/QComboBox 等主控件族已核实三态齐。

### 2.5 验证与假绿抽查

- 完整构建(qrc 重编)+ 全量 ctest 全绿(样式不入断言,绿是"没砸功能"的门禁)。
- **真机目视**:`cmd //c run_xq.bat` → 检查:四格边框新色、角标签深底、工具栏三段分隔、状态栏 12px、无一处荧光旧色残留。截图留档给用户过目(附 result)。
- 假绿抽查(qss 生效链路,防 Q_INIT_RESOURCE 类坑):把 xq.qss 的 `QToolBar {` 块 background 临时改 `#FF00FF` → 构建 → 真机看工具栏变品红(证明 qss 确实从 qrc 加载生效)→ 还原,构建,真机复查正常。**这一步是目视抽查,记录看到/没看到**。

### 2.6 Commit 1

```
style: qss token 归一 + MPR 四格身份色降饱和 + 工具栏分组/状态栏字号细化
```
文件:
- `resources/xq.qss`
- `src/visualization/XQMprView.cpp`
- `src/app/XQMainWindow.cpp`

---

## 3. Commit 2 — 空场景 3D 提示

### 3.1 实现

`src/visualization/XQRenderWidget.h`:public 加:
```cpp
    // Shows/hides the centered "nothing to render" hint over the 3D view. The
    // main window drives this from selection rendering (stats.ok).
    void setEmptyHintVisible(bool visible);
```
private 成员加 `QLabel* emptyHint_ = nullptr;`(前向声明 `class QLabel;` 在 Qt include 区补)。

`XQRenderWidget.cpp` 构造函数末尾(vtkWidget_ 装配后):
```cpp
    // Empty-scene hint: hidden by default; the shell toggles it per selection.
    emptyHint_ = new QLabel(tr("Nothing to render"), this);
    emptyHint_->setObjectName(QStringLiteral("xqEmptySceneHint"));
    emptyHint_->setAlignment(Qt::AlignCenter);
    emptyHint_->setStyleSheet(QStringLiteral(
        "QLabel { color: #5F6B7A; background: transparent; font-size: 13px; }"));
    emptyHint_->hide();
```
`resizeEvent` 里补一行让它铺满(在既有逻辑后):
```cpp
    if (emptyHint_ != nullptr) {
        emptyHint_->setGeometry(rect());
    }
```
```cpp
void XQRenderWidget::setEmptyHintVisible(bool visible)
{
    if (emptyHint_ != nullptr) {
        emptyHint_->setVisible(visible);
        if (visible) {
            emptyHint_->raise();
        }
    }
}
```
(include 补 `<QLabel>`。**注意**:xq_render_widget 库有独立 AUTOMOC 与 Qt 链接,`tr()` 在 QWidget 子类内可用。)

`XQMainWindow.cpp` `onSceneSelectionChanged` 尾部,before(逐字,:2006-2011):
```cpp
    if (stats.ok) {
        // Geometry renders into the MPR's 3D cell, so raise the MPR workspace
        // (renderWidget_ is nested inside it, not a direct stack page).
        centralStack_->setCurrentWidget(mprView_);
        renderWidget_->render();
    }
```
after:
```cpp
    renderWidget_->setEmptyHintVisible(!stats.ok);
    if (stats.ok) {
        // Geometry renders into the MPR's 3D cell, so raise the MPR workspace
        // (renderWidget_ is nested inside it, not a direct stack page).
        centralStack_->setCurrentWidget(mprView_);
        renderWidget_->render();
    }
```
(Image 分支与 early-return 路径不碰提示——只有"选中了该渲染却渲染不出"才提示,选中 Image/无 payload 不算。)

### 3.2 测试(先红)

`tests/app/test_app_startup.cpp` 惰性选中用例尾部追加(该用例已选中 Lazy Surface 成功渲染):
```cpp
    // Empty-scene hint: hidden after a successful geometry selection.
    QLabel* emptyHint = window.findChild<QLabel*>("xqEmptySceneHint");
    CHECK(emptyHint != nullptr);
    CHECK(!emptyHint->isVisibleTo(renderWidget->parentWidget()));
```
(include 补 `<QLabel>`。)
**先红**:断言先写,`xqEmptySceneHint` 未创建 → `emptyHint != nullptr` 红 → 实现 → 绿。
i18n:`Nothing to render` 进 ts(context `xq::XQRenderWidget`),补翻 `无可渲染节点`,与 Commit 3 的 lupdate 合并跑。

### 3.3 假绿抽查

把 `onSceneSelectionChanged` 的 `setEmptyHintVisible(!stats.ok)` 临时改 `setEmptyHintVisible(true)` → 构建 → 上述断言**必红**(成功选中后仍可见);还原,构建,绿。

### 3.4 Commit 2

```
feat: 3D 视图空场景提示 — 选中不可渲染时显示居中提示
```
文件:
- `src/visualization/XQRenderWidget.{h,cpp}`
- `src/app/XQMainWindow.cpp`
- `tests/app/test_app_startup.cpp`

---

## 4. Commit 3 — i18n 收口 + ts 全量补翻

### 4.1 lupdate 全量扫描 + 补翻

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
/c/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lupdate.exe \
  src -ts resources/i18n/xq_zh_CN.ts -no-obsolete
```
然后逐个处理 ts 里所有 `type="unfinished"` 词条(B1~B6 累计新串,含 XQRenderWidget 新 context):
```bash
rg -n 'unfinished' resources/i18n/xq_zh_CN.ts
```
全部补上中文翻译(删除 `type="unfinished"` 属性),要求:与既有词条用语一致(如 "工作区/节点/阶段" 沿用现译法)。**一个 unfinished 都不许剩**:
```bash
rg -c 'unfinished' resources/i18n/xq_zh_CN.ts   # 必须 0 命中(rg 无命中退出码 1 = 通过)
```
再 lrelease:
```bash
/c/Users/OCEAN/Desktop/XIAOQUAN/Externals/install/windows-x64/qt-6.7.0/bin/lrelease.exe \
  resources/i18n/xq_zh_CN.ts -qm resources/i18n/xq_zh_CN.qm
```
lrelease 输出的 `Generated N translation(s) (N finished and 0 unfinished)` 里 **unfinished 必须为 0**,输出留档。

### 4.2 i18n 测试挂钩(抽 2 条新 key)

`tests/app/test_i18n_resources.cpp` 追加(B4 已加过 2 条 XQStageWidgets 断言;本批再抽 2 条本批新串,防"漏翻以英文显形但不红"):
```cpp
    const QString emptyHint =
        translator.translate("xq::XQRenderWidget", "Nothing to render");
    if (emptyHint.isEmpty() || emptyHint == QStringLiteral("Nothing to render")) {
        return fail("XQRenderWidget empty-scene hint translates to Chinese");
    }
    const QString memFmt =
        translator.translate("xq::XQMainWindow", "Mem %1 MB | Geometry %2 MB");
    if (memFmt.isEmpty()
        || memFmt == QStringLiteral("Mem %1 MB | Geometry %2 MB")) {
        return fail("XQMainWindow memory status translates to Chinese");
    }
```
(第二条的串由 B5 引入;本批依赖 B1~B5 全部完成,该串必然存在——`rg -n 'Mem %1 MB' src/app/XQMainWindow.cpp` 无命中即依赖没满足,停下报告。)
**先红(固定顺序,不可变通)**:开发顺序 = 先写上面两条断言 → 在 4.1 补翻/lrelease **之前**完整构建并跑 `test_i18n_resources` → **红**(旧 .qm 无 `Nothing to render` 新串)→ 红输出留档 → 再做 4.1 补翻 + lrelease → 构建 → 绿。

### 4.3 Commit 3

```
chore: i18n 收口 — 全量 lupdate 补翻,新串零 unfinished,抽查断言挂钩
```
文件:
- `resources/i18n/xq_zh_CN.ts`
- `resources/i18n/xq_zh_CN.qm`
- `tests/app/test_i18n_resources.cpp`

---

## 5. 收尾验收(任务级,AC 对账)

1. **全新目录完整重建 + 全量 ctest,66/66 绿**(删 build_gui 重来;本批不增删测试)。
2. **TETGEN+MMG ON 档**再跑一轮全量(无条件跑,不因"B5 跑过/本批只动视觉"省略——qss/qrc 变更走全链路)。
3. **真机验收清单**(`cmd //c run_xq.bat`,逐项记录"看到/没看到"):
   - [ ] 启动即中文、字体正常无 tofu;
   - [ ] 四格身份色边框 + 深底角标签;
   - [ ] 工具栏三段分隔(Open/Save ‖ Undo/Redo ‖ Image ‖ 阶段钮);hover/pressed/checked 三态肉眼可辨;
   - [ ] 状态栏:内存数字在走(2s 刷新)、坐标/节点数分段线;
   - [ ] 菜单/工具栏**无一个点击无响应项**(逐个点:File 全部、Edit、View、Tools、Window、Help、工具栏全部)——R1 验收;
   - [ ] 打开工作区 → 树出节点 → 选中大表面 → 拖动旋转流畅(LOD interactive);
   - [ ] MPR 拾取 3 点 → 生成路径 → 树出 Path 节点 → 3D 可见 → undo/redo 对称(AC2 真机半);
   - [ ] 跑 512³ 区域生长时窗口可拖动、不白屏,忙态禁用可见(AC3 真机半);
   - [ ] 选中无几何节点 → 3D 格出现「无可渲染节点」;
   - [ ] 语言切换 zh↔en 全 UI 跟随(含新增串)。
   截图(主界面/四格/忙态/路径)留给用户过目。
4. `git log --oneline` 对照 B1~B6 全部 commit;`git status` 干净。
5. 汇报:各 commit hash、3.2/4.2 红绿证据、2.5 目视抽查记录、真机清单逐项结果、ON 档计数。**任何一项没做就写"没做",不许写"通过"。**
6. **不要动 task.json / implement.md 勾选**——收口汇报后由主审统一处理;spec 回填(core/command-and-scene.md redo 语义、visualization/lod-and-upload.md GUI 默认 LOD、app 层线程纪律)也归主审,不在本批。
