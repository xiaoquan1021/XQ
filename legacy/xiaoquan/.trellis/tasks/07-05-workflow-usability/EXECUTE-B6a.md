# EXECUTE-B6a — 消灭 NodeId 手填(四页下拉)+ 右键直达 + 同步回退回调修复

> 活动任务:`07-05-workflow-usability`(B6a 批)。你是 implement worker,只做本简报。
> worktree(唯一可改代码处):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(HEAD = ead692c,B5 已提交)。
> 纪律:工具调用必须真正以工具形式发出;完成报告纯文本收尾。
> **已知坑**:①本批改 XQMainWindow.h(Q_OBJECT)→ 最终 `rm -rf build_gui` 全新构建;②.bat 绝对路径 `cmd //c "C:\...\build_gui_wt.bat"`;③LNK1104 先 taskkill xq_app;④ts 手工编辑勿跑 lupdate,且**保持 UTF-8 无 BOM + LF**(用 Edit 工具改,别用 PowerShell 重写整文件);⑤新注释一律英文。

## 背景(用户需求)

建模/网格/流场/AI 四页要求手填 NodeId 整数——用户不可能知道内部 ID,是最重可用性断点(SV 全部用下拉/复选,见任务 research/sv-workflow-comparison.md §2.3/§2.4)。本批:四页 QSpinBox 换场景节点下拉 + 数据管理器右键直达 + 顺带修同步回退分支绕过刷新回调。

## 源码事实(已核,行号以 ead692c 为准)

- 四处手填(全部 `QSpinBox* sourceSpin`,消费 `idFromSpin` XQStageWidgets.cpp:61-64):
  - 建模 :636-637,label "Contour group NodeId" :642,run :665-668 → `contourGroupProvider(idFromSpin(sourceSpin))`;
  - 网格 :732-733,"Model source NodeId" :740,run :759-762 → intent.modelNode/sourceNode;
  - 流场 :833-834,"Case source NodeId" :837,run :859-862 → `flowInputProvider(...)`;
  - AI :934-935,"Flow source NodeId" :950,run :970-974 → intent.flowNode。
- `StagePanelContext`(XQStageWidgets.h:107-125):现有 provider 全是「传 id 反查」型,无枚举接口。
- 场景枚举:`XQScene::visit_nodes`(core/XQScene.h:65-73,按 NodeId 升序回调 `const XQDataNode&`);按 `node.domainType()` 自筛的先例 XQMainWindow.cpp:837-841。`XQDomainType`(core/XQDomainType.h:9-20):ContourGroup / SurfaceModel / SimulationCase / FlowResult 等。
- 右键菜单:`onSceneContextMenu` XQMainWindow.cpp:2635-2664,现仅 Delete 一项;NodeId/名称解析链 :2639-2652(经 sceneFilter_->mapToSource → sceneModel_->nodeForIndex);忙态门禁 :2640。**菜单现不读 domainType,需加。**
- 切页:`showStagePage(int)` XQMainWindow.cpp:2595-2604(0=Path,1=Seg,2=Modeling,3=Meshing,4=Flow;工具栏 connect :1209-1221)。
- 选中回调:`onSceneSelectionChanged` XQMainWindow.cpp:3054 起。
- 同步回退绕过刷新:六页 run 的 `if (asyncRunner) {...} else { controller->commitPrepared(...) }` 回退分支(XQStageWidgets.cpp :408/:607/:704/:804/:907)直推 stack_,不触发 refreshSceneTree。GUI 生产路径(asyncRunner,XQMainWindow.cpp:1003-1023)走 `session_->pushCommand` **已会**刷新,别动。`PathController::addPath`(ui/controllers/PathController.cpp:37-44)同样直推——先 grep 消费者,若 GUI 无调用(仅测试),只加英文注释标注 headless/test-only,不改行为。
- i18n:面板串走 `xqTr`(cpp:45-48,context "XQStageWidgets");主窗口串 tr(context "xq::XQMainWindow")。手工 ts 块照既有格式(无 <location>)。

## 改法

### 1. 枚举 provider(XQStageWidgets.h + XQMainWindow 接线)
- h 里 StagePanelContext 上方声明:
  ```cpp
  struct SceneNodeOption { NodeId id; QString name; };
  using SceneNodeLister = std::function<std::vector<SceneNodeOption>(XQDomainType)>;
  ```
  StagePanelContext 加成员 `SceneNodeLister nodeLister;`(include XQDomainType.h,注意 h 现有 include 集)。
- XQMainWindow buildStagePanel 处(stageContext 组装点,populateStagePanels 调用 :1042 前)接线:lambda 捕获 this,`session_->scene()` 为空返回空 vector;否则 visit_nodes 筛 `node.domainType()==domain`,收集 `{node.id(), QString::fromStdString(node.display_name())}`(display_name 返回类型先读 XQDataNode.h 核实,照 :2652 用法)。

### 2. 四页 QSpinBox → 节点下拉(XQStageWidgets.cpp)
- 匿名 namespace 加助手:`QComboBox* makeNodeCombo(QWidget* parent, const char* objectName, SceneNodeLister lister, XQDomainType domain)`——创建 QComboBox,**在 showPopup 前重填**:因 QComboBox::showPopup 非虚可重写受限,改用小派生类(文件内部):
  ```cpp
  class NodeComboBox : public QComboBox {
  public:
      NodeComboBox(SceneNodeLister lister, XQDomainType domain, QWidget* parent = nullptr);
      void showPopup() override;   // repopulate from lister, keep current id selected if still present
  };
  ```
  (QComboBox::showPopup **是** virtual,直接 override 即可;条目文本 `name + " (#" + id + ")"`,`addItem(text, QVariant(qulonglong(id.value())))`——NodeId 的数值访问器先读 core/NodeId.h 核实方法名。)
  构建时也填一次(场景可能已有节点)。
- 助手 `NodeId idFromCombo(const QComboBox*)`:currentData 无效(空列表/未选)返回 `NodeId(0)`(与现状 spin 默认 0 的无效语义一致,run 的报错路径不变)。
- 四页替换(label 同步改,去掉 "NodeId" 字样):
  - 建模:domain=ContourGroup,objectName `xqModelingSourceCombo`,label 源串 "Contour group";
  - 网格:domain=SurfaceModel,objectName `xqMeshingSourceCombo`,label "Model source";
  - 流场:domain=SimulationCase,objectName `xqFlowSourceCombo`,label "Case source";
  - AI:domain=FlowResult,objectName `xqAiSourceCombo`,label "Flow source"。
  消费点 `idFromSpin(sourceSpin)` → `idFromCombo(sourceCombo)`,其余 run 逻辑不动。
- **先 grep 测试**:`grep -rn "sourceSpin\|NodeId.*[Ss]pin" tests/` 找现有断言(test_path_stage/test_main_window 等),逐条申报适配(spin setValue → combo 填充+setCurrentIndex 或直接构造 intent,语义等价,不删断言)。

### 3. 右键直达(XQMainWindow.cpp onSceneContextMenu)
- :2646 拿到 node 后读 `node->domainType()`:
  - ContourGroup → 加 action `tr("Model from \"%1\"")`:`showStagePage(2)` + 预选;
  - SurfaceModel → 加 action `tr("Mesh from \"%1\"")`:`showStagePage(3)` + 预选。
- 预选助手(XQMainWindow 私有方法):`void preselectStageSource(int pageIndex, const NodeId& id)`——findChild 对应 objectName 的 QComboBox,先照 showPopup 的重填逻辑填一遍(可把重填提成 NodeComboBox 公有 `repopulate()`,预选走 `combo->repopulate(); combo->setCurrentIndex(findData(...))`),h 里前置声明即可(XQStageWidgets.h 已被 include)。
- Delete 行为、忙态门禁不变;菜单 action 顺序:直达在上,Delete 在下。

### 4. 选中自动带入(onSceneSelectionChanged :3054)
- 现有函数尾部加:选中节点 domainType==ContourGroup → `preselectStageSource(2, id)`(不切页,只带入);SurfaceModel → `preselectStageSource(3, id)`。**别在这里 showStagePage**(选中≠要建模)。

### 5. 同步回退回调修复(XQStageWidgets.cpp 五处)
- :408/:607/:704/:804/:907 的 else 分支:commitPrepared 返回成功(先读各处现状——返回值/错误处理方式)后补调 `stageChanged`(该回调 XQMainWindow.cpp:1000 已接 refreshSceneTree)。**先核对 done 回调路径**:async 分支的 done 里是否已调 stageChanged——若是,同步分支补齐即达成「任一提交路径恰一次刷新」;若 async 也没调(靠 session 网关刷),那同步分支补的应与之等价(直接调 stageChanged),报告写清核对结论。
- PathController::addPath:grep 消费者;GUI 无调用则仅加英文注释(header + cpp)"Headless/test convenience: pushes directly to the stack and does NOT fire the session sceneChanged callback; GUI code must go through the async runner / session gateway."

### 6. i18n
- 新串:四页新 label(若源串变)、两个右键 action("Model from \"%1\""/"Mesh from \"%1\"",context xq::XQMainWindow)。
- 手工 ts 块 + lrelease,报告 finished/unfinished 计数。

## 文件白名单

| 操作 | 文件 |
|---|---|
| 修改 | `src/ui/panels/XQStageWidgets.{h,cpp}` |
| 修改 | `src/app/XQMainWindow.{h,cpp}` |
| 修改 | `src/ui/controllers/PathController.{h,cpp}`(仅注释,除非 grep 发现 GUI 消费者——停下报告)|
| 修改 | `resources/i18n/xq_zh_CN.ts` + lrelease `.qm` |
| 修改 | `tests/` 下受影响测试(先 grep 申报)|

**不动**:core/services/io/visualization;XQWorkflowSession;其余 controllers 行为。

## 测试(新增)

1. **combo 枚举**(test_main_window):加载 0007 工程后 findChild `xqModelingSourceCombo`,调 repopulate(或模拟 showPopup),断言 count == 场景 ContourGroup 节点数、每项 userData 是有效 NodeId、文本含节点名;
2. **右键直达**:构造 ContourGroup 节点选中态,调 onSceneContextMenu 逻辑(若私有,经 QMetaObject::invokeMethod 或把 action 构建提成可测助手——照现有 test_main_window 对私有槽的既有测法,先 grep)断言触发后 `stagePanel_->currentIndex()==2` 且 combo currentData==该 id;
3. **同步回退刷新**:无 asyncRunner 的 headless 面板(populateStagePanels 传空 runner)执行一次 run,断言场景树 model 行数更新(refreshSceneTree 效果)——若现有测试架构取不到,退而断言 stageChanged 回调被调恰一次(计数器 lambda),报告说明;
4. 既有断言适配逐条申报。

## 验证

```bash
cd /c/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ
rm -rf build_gui
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"   # 全量全绿(基线 68,+新增)
cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\run_xq.bat"        # 冒烟:能起不崩
```

假绿抽查两项(必做,exe 时间戳核对):
1. 篡改 nodeLister 接线(恒返回空 vector)→ combo 枚举断言转红 → 还原绿;
2. 篡改右键直达(action 不调 showStagePage)→ 切页断言转红 → 还原绿。

## 禁做

白名单外文件;不动 GUI 生产提交路径(asyncRunner 分支);不做「树内改名/属性面板」等未要求扩展;既有断言只做申报过的等价适配;PathController 发现 GUI 消费者立即停下报告;不 commit;报告纯文本收尾;冲突/存疑停下等裁决。

## 完成报告格式

1. 文件清单;2. 全新构建+全量 ctest 总结行原文;3. 两项假绿抽查证据;4. 既有断言适配申报;5. async/同步两条提交路径的刷新语义核对结论;6. PathController::addPath 消费者 grep 结论;7. i18n 计数;8. 偏离与存疑。
