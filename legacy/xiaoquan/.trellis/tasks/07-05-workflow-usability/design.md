# design.md — 工作流可用性(B6):技术设计

> 事实基线:worktree `feat/render-arch` @ ead692c(B5)。行号来自两份只读盘点(2026-07-05)。
> 分层铁律:不动 core/services/io(节点枚举用现有 `XQScene::visit_nodes` 自筛,core/XQScene.h:65-73);VTK 只在 visualization。

## 1. 消灭 NodeId 手填(P1)

### 1.1 现状(盘点事实)
四页 QSpinBox 手填 NodeId(range 0..1000000,`idFromSpin` cpp:61-64 转 NodeId):
- 建模 `sourceSpin` XQStageWidgets.cpp:636,label "Contour group NodeId":642,消费 :665-668 → `contourGroupProvider(id)`;
- 网格 `sourceSpin` :732,"Model source NodeId":740,消费 :759-762 → intent.modelNode/sourceNode;
- 流场 `sourceSpin` :833,"Case source NodeId":837,消费 :859-862 → `flowInputProvider(id)`;
- AI `sourceSpin` :934,"Flow source NodeId":950,消费 :970-974 → intent.flowNode。
StagePanelContext(XQStageWidgets.h:107-125)全部 provider 是「传 id 反查」型,**无枚举接口**。

### 1.2 设计
- **新枚举 provider**:`StagePanelContext` 加一个字段
  `std::function<std::vector<SceneNodeOption>(XQDomainType)> nodeLister;`
  `struct SceneNodeOption { NodeId id; QString name; };`(声明在 XQStageWidgets.h)。
  XQMainWindow 接线:lambda 内 `session_->scene()->visit_nodes` 按 domainType 筛(现成写法先例 XQMainWindow.cpp:837-841)。
- **下拉控件**:XQStageWidgets.cpp 内部小类 `NodeComboBox : QComboBox`,重写 `showPopup()` 时调 nodeLister 重填(条目文本 `"%1 (#%2)"` name+id,userData=id 数值)——**打开即新鲜**,免去场景变化通知管道(与「面板一次性构建」现状 cpp:40-44 一致)。四页 `sourceSpin` 全部替换;`idFromSpin` 改为 `idFromCombo`(userData 取值;空列表/未选中返回无效 id,run 报错路径沿用现状)。
- **objectName**:`xqModelingSourceCombo` / `xqMeshingSourceCombo` / `xqFlowSourceCombo` / `xqAiSourceCombo`(结构测试 findChild 锚点)。
- **选中自动带入**:`XQMainWindow::onSceneSelectionChanged` 里按选中节点 domainType 找对应 combo(findChild by objectName),若列表里有该 id 则 setCurrentIndex(先重填一次)。不新增 context 注册管道(避免接口膨胀)。
- 四页替换同一机制一次做完(建模/网格是 PRD 主项,流场/AI 是同机制顺带,一致性)。

### 1.3 右键直达
`onSceneContextMenu`(XQMainWindow.cpp:2635-2664,现仅 Delete)扩展:读 `node->domainType()`,
- ContourGroup → action "Model from this"(用它建模):切建模页 + 预选该 id;
- Model → action "Mesh from this"(用它建网格):切网格页 + 预选。
切页复用工具栏 stage 切换的既有入口(worker 查 buildStagePanel/toolbar action 的切页函数);预选 = 上面 findChild+重填+setCurrentIndex 同一助手函数。Delete 行为不变;忙态门禁(:2640)沿用。

## 2. addPath 绕过 session 回调(既有缺陷)

盘点结论:GUI 生产路径(asyncRunner 恒非空,XQMainWindow.cpp:1003-1023)commit 走 `session_->pushCommand` **会**触发 refreshSceneTree;真正绕过的是:
- `PathController::addPath`(PathController.cpp:37-44)直推 `stack_->push`——查 GUI 是否还有调用点,若仅测试用,标注 headless-only 注释;
- XQStageWidgets 六页的**同步回退分支**(:408/:607/:704/:804/:907)`commitPrepared` 直推——headless 无 runner 时走。
修法(最小):同步回退分支 commit 成功后补调 `stageChanged`(该回调在 XQMainWindow.cpp:1000 已接 refreshSceneTree);若页面 done 回调已调 stageChanged 则确认即可,不重复加。验收:任一 stage 命令成功提交(async 或同步回退)恰好一次 refreshSceneTree。

## 3. 分步引导条(P2)

现状:`makeHintBar`(cpp:128-137)构建期一次性文本,琥珀样式;网格/AI 页无 hint。
设计:hint QLabel 保留(样式/objectName 不变),升级为**运行时状态机文本**:
- 每页新增内部函数 `updateGuidance()`,按现有 enable 逻辑的同源条件计算当前步骤文本(如路径页:无图像→"Step 1/3: Open an image";拾取关→"Step 2/3: Toggle picking and click points";<2 点→同上带点数;可执行→"Step 3/3: Generate Path");
- 在既有信号处调用(pickToggle toggled、pointList 变化、radio 切换、combo currentIndexChanged、run done 回调——与 run enable 更新同点位,不新增轮询);
- 网格/AI 页补 makeHintBar。
文案全部 xqTr(英文源串+中文翻译)。

## 4. tooltip 补齐(P2)

盘点:全文 0 个 setToolTip。六页全部参数控件补(盘点第 4 节清单为准:路径 4、分割 6、建模 3、网格 4、流场 2、AI 5,约 24 处)+ 新增控件(combo/估计按钮)自带。文案照 SV 密度:一句话说明含义+单位/默认值;xqTr。

## 5. 阈值估计按钮(P2)

现状:lowerSpin/upperSpin 默认 0/255 写死(cpp:441-446),无图像强度逻辑。
设计:分割页阈值行加 `QPushButton* estimateBtn`("Estimate"),clicked → `imageProvider()` 取活动图像,**跨步采样**(stride 取样 ≤1M 体素,GUI 线程可承受)算 P25/P99 百分位设为 lower/upper 建议值(setValue,不锁定用户后续修改);无图像 → status 标签报 "Open an image first."。不动 core(算法就地写在 XQStageWidgets.cpp 匿名 namespace,或 ui/ 内部 helper)。

## 6. Ctrl+A 加点 + 模式指示浮层 + Esc(P2)

现状:pick 模式零视觉指示(仅 seed 分支状态栏文本),零快捷键承接;XQSliceViewWidget 无 keyPress(rg 零命中)。
设计(全部在 app + visualization widget 层):
- **Ctrl+A**:XQMainWindow 构造函数加 `QShortcut(QKeySequence("Ctrl+A"), this)`(先例 :410-413 PageUp/Down):`pickMode_==PathPoint` 时取 `renderScene_->sliceIndex(0/1/2)` 拼当前十字线体素 → 复用 voxelPicked 的 PathPoint 分支逻辑(voxelToWorld → pathDraftPoints_ push → pathDraftChanged_());其它模式 no-op。
- **模式浮层**:XQSliceViewWidget 加 `void setModeHint(const QString&)`(空串隐藏):角落半透明 QLabel(与既有角标 overlay 同布局方式);XQMprWidget 转发到三个切片格。XQMainWindow 在 seedPickingSetter(:752-769)/pathPickingSetter(:786-803)进入/退出时设文案:"拾取模式:点击切片加点(Ctrl+A 加十字线处,Esc 退出)"/种子同构。
- **Esc 退出**:QShortcut(Qt::Key_Esc) → pickMode_ 非 None 时调对应 setter(false)+ 同步页面 toggle 按钮 setChecked(false)(setter 现状已互斥,沿用)。
- 阶段页 pickToggle 本来就是 checkable(checked 态保留,PRD 第 7 项)。

## 7. 测试与 i18n

- 结构测试:test_main_window 加 combo 锚点(findChild + showPopup 后 count 与场景 ContourGroup 数一致、userData 正确)、右键 action 存在性与触发后 stage 页切换、Ctrl+A 在 PathPoint 模式下 draft 点数 +1、Esc 清模式;test_path_stage 适配 spin→combo(先 grep 现有断言)。
- 估计按钮:合成已知直方图卷 → 点击后 lower/upper 落在预期百分位(容差)。
- 引导条:状态推进断言(hint 文本随 toggle/点数变化)。
- 假绿抽查(每批 ≥2 项,红→绿+时间戳核对)。
- i18n:新串全部手工 ts(XQStageWidgets/xq::XQMainWindow/xq::XQSliceViewWidget 各归其 context)+ lrelease 0 unfinished;**勿跑 lupdate**。

## 8. 批次拆分

同一 worktree 严格串行(memory: gui-v2-batches-serial-worktree):
- **B6a = P1 + addPath 修复**(§1 + §2):XQStageWidgets.{h,cpp} + XQMainWindow.{h,cpp} + controllers 注释/微调 + ts + 测试;
- **B6b = P2 全部**(§3~§6):XQStageWidgets.cpp + XQMainWindow.{h,cpp} + XQSliceViewWidget/XQMprWidget + ts + 测试。
每批:implement worker → check worker → 主审亲读+全新构建复跑 → commit;B6b 后真机交用户走四阶段链签收。
