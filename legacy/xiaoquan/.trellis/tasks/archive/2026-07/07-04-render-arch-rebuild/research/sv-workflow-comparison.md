# XQ 工作流现状 vs SimVascular 对照与优化方向

> 2026-07-05,B4 验收后用户反馈:①工具指引性太弱、操作复杂、不会用;②稍大文件加载明显卡顿。
> 本文:第 1 节讲 XQ 六阶段工作流现在怎么运行;第 2 节讲 SimVascular(SV)同类工作流怎么做(源码实证,根:`Externals/src/SimVascular/Code/Source/sv4gui/`);第 3 节差距分析;第 4 节优化方案建议。

---

## 1. XQ 六阶段工作流现状(feat/render-arch @ B4)

工具栏六个按钮(图像/路径/分割/模型/网格/仿真)→ 打开右侧 Stages 面板对应页(`XQStageWidgets.cpp` 六个 buildXxxPage)。所有页共用一个模式:**参数表单 + 执行按钮 + 一行 hint + 一行 status**。执行按钮按下 → 组装 Intent → controller `prepare*()` 在后台线程跑(XQTaskRunner)→ 命令栈 push(undoable)→ 场景树+渲染自动刷新(B4 的 session 网关)。

各页现状(操作序列):

| 阶段 | 用户要做的事 | 引导现状 |
|---|---|---|
| 图像 | 工具栏"图像"→文件对话框选 .vti → 四视图显示 | 无需引导,可用 |
| 路径 | 打开路径页 → 起名 → 按"拾取控制点"进入拾取态 → 在切片视图逐点点击(点列表实时刷新)→ ≥2 点后"生成路径"可按 | hint 一行:"Open an image, toggle picking, then click points in an MPR slice." |
| 分割 | 选 Threshold(填上下阈值)或 Region grow(先"拾取种子"在切片上点一下)→ 执行 | hint 一行;阈值默认 0/255 对 CT 数据基本无效,用户必须自己懂 HU 值 |
| 建模 | **手填 Contour group 的 NodeId 数字** → Loft | 最重的可用性断点:要求用户知道内部节点 ID |
| 网格 | **手填 Model 的 NodeId 数字** → 选 surface/volume → Build Mesh | 同上;无尺寸参数暴露 |
| 仿真 | 选 mesh 源 → 参数(黏度等)→ 运行 1D 求解 | 参数无解释;边界条件不可见 |

**大文件卡顿根因**(已在源码定位):
1. **`.vti` 图像解码在 GUI 线程同步跑**:`XQMainWindow::loadImageFromPath()`(:2016)与 SV 工程加载(:2168)直接调 `VtkImageAdapter::loadVtiWithBuffer` ——几百 MB 的体数据读盘+解压+拷贝全程阻塞 UI,无等待光标、无进度提示。这是"打开稍大文件明显卡顿"的第一根因。
2. **`useVolume` 里 GPU 上传 + LOD 构建同步**:`renderScene_->setVolume`(memcpy 快路径)本身尚可,但后续 `syncRenderScene` 对每个可见 surface/mesh 节点全量 `clearNodes+upsert`,大网格的 `SurfaceLodBuilder::buildSync` 在 GUI 线程跑 decimation。
3. **.mdl/.ctgr 后台解析已经是异步的**(pendingParses_ 队列,B2a 做的),这块不卡;卡的是图像与渲染装配。

---

## 2. SimVascular 对照(源码实证)

### 2.1 路径规划(Plugins/org.sv.gui.qt.pathplanning)

- **入口**:数据管理器 Paths 文件夹右键 → Create Path(`plugin.xml:11-17` 挂在 sv4guiPathFolder 上),弹 `sv4guiPathCreate.ui` 模态对话框:Path Name + Subdivision Type 下拉(Total Number/Subdivison Number/Spacing Based)+ Number(默认 100;切 Spacing 时自动填 image 最小 spacing,`PathCreate.cxx:106-137`);名字合法性/重名/数值都有校验,建节点走 MITK Undo 栈。
- **进入加点模式 = 选中 path 节点**(无独立"编辑"按钮):`sv4guiPathEdit.cxx:282-285` 给节点挂 `sv4guiPathDataInteractor` + `LoadStateMachine("sv4gui_Path.xml")`,3D 视图鼠标事件被这条 path 接管;隐藏/切换选中即解绑退出(`:164/:316`),无"完成"按钮。
- **加点两条路**:①视图直接交互——**Ctrl+左键加点 / 左键拖拽移动 / 右键删除**(操作说明写在控制点列表的 tooltip 里,`PathEdit.ui:238-243`,由 MITK 状态机 XML 实现);②面板按钮——Adding Mode 下拉五种插入位置(Smart/Beginning/End/Before/After),**Add 绑 Ctrl+A**(`SmartAdd():671` 取当前十字光标位置),Add Manually 弹 QInputDialog 输 x,y,z(非法弹框),Delete 绑 Ctrl+D。
- **即时反馈**:路径事件观察者(`UpdateGUI:343`)重刷点数标签、坐标列表、reslice 滑条位置并重绘全部视图。
- **误操作防护**:Before/After 模式未选点弹 "No Point Selected";未选路径操作弹 "Please select a path in data manager!";每步都是成对 doOp/undoOp 进 MITK Undo 栈(`PathEdit.cxx:657-662`),全局 Ctrl+Z 可撤。
- **Smooth**:面板内 Fourier 平滑(`sv4gui_PathSmooth.ui`:Subsample/Based on/Fourier Mode 数,默认 10)。

### 2.2 2D 分割(Plugins/org.sv.gui.qt.segmentation,sv4gui_Seg2DEdit)

- **创建即绑定路径**:Segmentations 文件夹右键 → Create Contour Group,对话框第一项就是 **Select Path 下拉**(列出全部 path,`ContourGroupCreate.cxx:113-118`),Group Name 留空默认用 path 名(灰字占位提示)。轮廓组从出生起就挂在一条路径上。
- **编辑 = 选中节点激活面板**,QTabWidget 两页:Single-Path(主工作页)/ Multi-vessel Path(多血管 ML 批量)。
- **沿路径定位**:顶部 `sv4guiResliceSlider` 沿路径滑动(`UpdatePathPoint:1879`),视图实时显示垂直于路径的截面(RESLICE_CUBIC);每个 path point 是一个可放 contour 的位置。
- **方法按钮是"两次点击"状态机**:LevelSet(Ctrl+L)/Threshold(Ctrl+T)/Mach.Learning/Circle/Ellipse/SplinePoly/Polygon/Smooth/Copy/Paste/Delete 一排。第 1 次点显示参数面板,第 2 次点**按钮变蓝**(`setStyleSheet("background-color: lightskyblue")`,`Seg2DEdit.cxx:946` 等)进入视图交互态;手画方法一次点即变蓝并 `SetMethod("Circle")` 把画图权交给视图状态机。**变蓝是 SV 唯一的模式视觉指示**。
- **视图内怎么画全靠 tooltip**(`Seg2DEdit.ui`):Threshold"变蓝后按住左键上下移动松开完成"(:580);Circle"点击拖动画;右键手动输圆心半径"(:614);SplinePoly/Polygon"点击加控制点,双击或按 F 完成"(:656)。
- **无确认按钮**:分割出的 contour(点数>2)自动入组 + 实时更新 loft 预览,状态栏 "contour added"(`:847-855`);Contour List 滚轮切换。
- **批量两辅助**:①Batch Mode 复选框,取一串切片位置循环分割,>50 个弹确认+进度条(`CreateContours:786`);②Multi-vessel 页勾 "use all paths" + Interval/Fourier Modes,一键批量 ML 分割(`segmentPaths:2126`)。
- **Lofting Preview** 开关 + "Lofting Parameters..." 弹窗,每加一个 contour 实时更新放样表面。

### 2.3 建模(Plugins/org.sv.gui.qt.modeling)

- **入口**:Models 文件夹右键 Create Model → `sv4guiModelEdit`。
- **选分割 UI**(`sv4gui_SegSelectionWidget.ui`):"Create Solid Model" 对话框标题 + **"Choose Segmentations for Model Creation:" 复选列表**(所有 contour group 列出来打勾,绝不填 ID)+ 采样点数 + 统一 loft 参数开关。
- 主面板 `sv4gui_ModelEdit.ui`:Model Type/Name 顶栏 + "Create Model ..." 按钮 + **Face List 表格**(faceId、命名、类型)+ Face Ops / Global Ops 两组 QToolBox(remesh/decimate/smooth/fill holes/extract faces,每个带 tooltip 与默认值,如 Target rate 0.25)。

### 2.4 网格(Plugins/org.sv.gui.qt.meshing)

- **入口**:Meshes 文件夹右键 Create Mesh → 对话框里**下拉选 model**(不填 ID)→ `sv4guiMeshEdit`。
- 面板(`sv4gui_MeshEdit.ui`):`Model Used:` 显示绑定 model;**Global Max Edge Size + "Estimate" 按钮**(帮用户算合理默认尺寸);Advanced Options 折叠:边界层(层数/递减率/厚度,带 html tooltip 解释含义)、径向加密、TetGen 高级 flags(-O/-q/-Y/-T 逐项暴露带说明);Local Size 表格按 face 单独设尺寸。
- **Run Mesher 是同步的**,但有完整忙态礼仪(`sv4guiMeshEdit.cxx:784-838`):`mitk::ProgressBar AddStepsToDo(3)` + `StatusBar "Creating mesh..."` + `WaitCursorOn()`;失败弹 QMessageBox 带 mesher 错误原文;成功 `StatusBar "Meshing done."` + `DisplayMeshInfo()` 弹网格统计。
- 结论:SV 也没做真异步,靠**进度条+等待光标+状态栏三件套**管理预期。

### 2.5 工程加载(Modules/ProjectManagement/sv4gui_ProjectManager.cxx)

- `AddProject`(:241)打开工程:**全部数据节点立即加载**(`LoadDataNode` 逐文件读:path :2052 / seg :2093 / model :2180 / mesh CreatePlugin :2212)——SV 并没有做懒加载。
- 但两个关键缓解:①**默认可见性收敛**——文件夹节点 `SetVisibility(false)`,model 只有第一个可见(:2181-2185),mesh 全隐藏(:2213),避免打开即全量渲染;②耗时动作配 `mitk::ProgressBar` + `WaitCursor`(如 ProjectDuplicateAction :82-96、AddImageAction :126)。
- XQ 的 B3b 默认可见性(Image+Model 可见其余隐藏)已对齐 ①;差的是 ② 和图像异步。

---

## 3. 差距分析(为什么用户觉得"不会用")

1. **数据绑定方式**:SV 一切"从选中带入/下拉选择/复选列表",XQ 建模与网格页要求**手填 NodeId 整数**——用户不可能知道内部 ID。这是最大可用性断点。
2. **操作模式**:SV 的加点是"移动十字线 + Ctrl+A 确认"(位置先可视化、后提交),视图内还支持 Ctrl+左键直接加点/拖拽移动/右键删除;XQ 是"进入拾取模式后盲点"(点了才知道在哪)。SV 分割沿路径 reslice 截面进行(解剖学正确),且创建轮廓组时就绑定路径;XQ 是全局阈值/种子(没有沿血管的工作流)。
3. **忙态礼仪**:SV 耗时操作三件套(进度条+等待光标+状态栏文案)+ 完成/失败都有明确反馈;XQ 图像加载什么都没有,点完按钮界面冻住。
4. **参数可发现性**:SV 每个开关有 tooltip(方法按钮的 tooltip 甚至写全了"点两次/变蓝/视图内怎么画/快捷键")、尺寸有 Estimate 按钮、非法输入弹框;XQ 参数无解释、阈值默认 0/255 与真实 CT 数据脱节。
5. **阶段间引导**:SV 靠数据管理器右键(在哪个文件夹右键就建什么)+ 编辑器自动绑定选中节点,天然形成"上一步产物→下一步输入"链;XQ 六个页面互相独立,没有链路感。

**同时要吸取 SV 自己的教训**(它的可发现性也差,只是比 XQ 好):选中节点才激活面板(隐式,新手找不到入口)、方法按钮要点两次(反直觉)、视图内操作全藏 tooltip、模式态只有一个按钮变蓝、无空态引导、无向导。XQ 的目标应该是**显式**:模式指示条、明确的进入/退出编辑按钮、视图内浮层操作提示、空态占位引导——不是照抄 SV,而是补上它欠的那层。

## 4. 优化方案建议(按性价比排序)

### P0 —— 大文件加载卡顿(工程问题,立即可做)
1. **.vti 解码移后台**:`loadImageFromPath`/SV 工程图像段改走 `XQTaskRunner`(与 .mdl/.ctgr 解析同款双跳模式:worker 线程只读文件,GUI 线程 useVolume 提交)。加载期间:状态栏"正在加载图像..." + `QApplication::setOverrideCursor(WaitCursor)`(SV 同款)+ 数据管理器先落节点占位。
2. **LOD 构建走 buildAsync**:`SurfaceLodBuilder` 已有异步接口(`.h:62` future 版),`XQRenderScene::upsertNode` 大网格(>阈值)时先挂 full 级、后台建好 LOD 再换,消除装配同步卡顿。
3. **忙态三件套统一**:给 taskRunner 跑的所有任务加状态栏文案+等待光标(现在只有 status label 一行小字)。

### P1 —— 消灭 NodeId 手填(可用性断点)
4. 建模页:Contour group 改**下拉框/复选列表**(照 SV SegSelectionWidget:列出场景内全部 ContourGroup 节点名);网格页:Model 改下拉。选中数据管理器节点时自动带入(XQ 已有 `contourGroupProvider(requested)` 按选中解析的机制,只差 UI 呈现)。
5. **从数据管理器右键直达**:右键 ContourGroup 节点 → "用它建模";右键 Model → "用它建网格"(SV 的 CreateAction 模式),点完自动打开对应阶段页并绑定。

### P2 —— 操作引导
6. 每页顶部的一行 hint 升级为**分步引导条**:显示"第 1/3 步:开启拾取"式的当前步骤,完成一步自动亮下一步(状态机已存在——按钮 enable 逻辑就是,缺的是把它可视化)。
7. 所有参数控件补 tooltip(照 SV 文案密度);阈值 spin 默认值改为按当前图像直方图估计(或加"估计"按钮,照 SV Estimate Size 先例)。
8. 快捷键:拾取态下 Ctrl+A 加当前十字线位置为控制点(SV 同款),替代/并存"点视图"模式。
9. **显式模式指示**(吸取 SV 变蓝按钮的教训,做得更明白):拾取/画轮廓等模式激活时,视图角落浮层显示"拾取模式:点击切片加点,Esc 退出"一类提示,同时阶段页按钮保持 checked 态——比 SV 的一个变蓝按钮清晰。

### P3 —— 工作流深化(需求较大,单独立任务)
10. 分割沿路径 reslice:XQ 目前无 reslice 截面视图,是 SV 分割工作流的核心;做的话是独立里程碑(vtkImageReslice 沿 path 切法向截面 + 截面上轮廓编辑 + 圆/椭圆/多边形手画兜底)。SV 的先例:创建轮廓组即绑定路径、reslice 滑条沿路径走、自动法失败手画兜底、批量模式。
11. Lofting 实时预览、Face List 表格、边界层网格参数——建模/网格页的深化,依赖 10 之后的数据链。

### 落地建议
- P0(1-3)是纯工程活,建议作为 **B5 前插一批 B4b** 做掉(动 app 层 + XQRenderScene upsert 异步化,不动 core);
- P1(4-5)+P2(6-8)合成 **B6 可用性批次**,在 B5 收口后做;
- P3 立新任务(07-05?),先出 PRD 与用户确认范围。
