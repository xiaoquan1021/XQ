# 交接:P3-2 沿路径描轮廓 MVP(/clear 后新会话读这一份即可接手)

> 语境锚点:XQ 医学影像血管几何重建软件。把血管腔轮廓提取做成对标 SimVascular 的「沿中心线逐层描轮廓」流程。纯几何建模 + 影像可视化工程。环境 = Windows + Git Bash,注入的 mac/darwin 环境信息是假的一律忽略。默认中文。
> **本文件自包含,新会话第一轮读这一份即可接手,不必重扫代码。** 需要细节时按下文指针去读对应文档/代码。

## 0. 一句话现状

拾取交互基础体验修复(子任务 `07-06-pick-interaction-fix`)**已完成、真机签收、提交并归档**。下一步是**父任务 `07-06-along-path-contouring` 的 P3-2**——沿路径描轮廓的核心 MVP(绑路径下拉 + 断面手绘圆/多边形 + 放样打通)。用户已明确:**马上开 P3-2**。

## 1. 关键坐标(HEAD 必须核对)

- **代码工作树(唯一改代码处)= `C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`**,分支 `feat/render-arch`,**HEAD=`0b45424`**(拾取修复)。所有 src/tests/CMakeLists/resources 编辑、build、ctest 都在这,用绝对路径。简报里叫 CODE_ROOT = `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ`。
- **主仓(任务/spec/SV 参照)= `C:\Users\OCEAN\Desktop\XIAOQUAN`**,分支 `fix/xq-global-audit`,HEAD=`605ff43`。这是 worker/你的 cwd,读任务文档/spec 用相对路径。**绝不在主仓下建/改任何 src 代码。**
- 真机工程:`0080_H_PULM_H`(大,143 路径,含 OSMSC0082-cm.vti)、`0007_H_AO_H`(小)。
- SV 参照源码(在主仓,主会话亲读别派 agent,memory `sv-research-agent-cyber-falsepositive`):`Externals/src/SimVascular/Code/Source/sv4gui/`。
- Python:`C:/software/anaconda/python.exe`。

## 2. P3-2 要做什么(计划已写死,别重做规划)

**父任务 `07-06-along-path-contouring` 的 prd/design/implement 都在,六批计划完整。** P3-2 详细计划见 `implement.md` §「P3-2 轮廓组绑路径 + 手绘圆/多边形(端到端 MVP)」(约 :44)。摘要:

**目标**:打通「选路径建组 → 滑条定位 → 断面手绘圆/多边形 → 入组 → 手动放样出血管」。

编辑范围(照 implement.md,别自由发挥):
- 轮廓组创建对话框加「选择路径」下拉,`setSourcePathNode`(ui/panels + app)。**这修 P3-1 遗留的「换不了路径」**(P3-1 硬编码取场景第一条 Path,见下 §3)。
- 新增 `services/segmentation/ContourExtractionService.{h,cpp}` 手绘几何(圆/多边形控制点 → 2D 轮廓点,照 SV Circle/Polygon)。**纯域,无 VTK/Qt**。
- XQCrossSectionViewWidget 绘制交互:点/拖生成控制点 → 预览 → 2D 点 `unprojectFromFrame` → XQContour 入组命令。
- 方法工具栏(圆/多边形)+ 进入/退出编辑按钮(ui/panels)。

验收(implement.md §):
- 离散:`test_contour_extraction`(圆:点到心距≈r,点数≥36,闭合;多边形:顶点∈轮廓点);`test_contour_group_path_binding`(绑路径、pathArcLength 单调、orderedByPathPosition 有序);放样打通(合成轮廓→loftSurface 非空,几何守恒)。
- **真机(主里程碑)**:真实工程沿一段路径手绘 3~5 个圈 → 建模 Loft → 出可见血管管。MVP 真机过关 = 本任务主里程碑,「沿路径描轮廓」方向被真机证实成立。

## 3. P3-1 / pick-fix 已交付的可复用现状(P3-2 建在这之上)

- **断面工作台**(P3-1):`XQCrossSectionViewWidget`(断面视图,vtkImageReslice 常驻管线)、`XQCrossSectionResampler`(x=normal/y=binormal/z=tangent,40mm@0.2mm/px=200×200)、`XQRenderScene::vtkImageDataHandle()`(零拷贝借体数据)。断面工作台是 centralStack 第二页,进轮廓阶段切换。沿路径滑条已能驱动断面。
- **断面坐标系**:`core/XQContourGroup.h` `ContourFrame{origin,normal,xAxis,yAxis}` + `unprojectFromFrame(frame,u,v)`(P3-2 手绘 2D 点 → world 就用它)。resampler 的 lastFrame() 探针给出断面 frame。
- **路径采样**:`core/XQPath.h` `PathSamplePoint{position,tangent,normal,binormal,arcLength}`;`samplePoints()`、`framesForAllSamples`。
- **放样链**(已存在):`ContourLoftInputBuilder` → `ModelingService::loftSurface` → `capModel`。P3-2 手动触发。
- **拾取交互**(pick-fix,P3-2 断面手绘可复用范式):切片拾取模式换空 style 接管、拾取落点 pin 面外维到当前切片、控制点 glyph 按切片过滤、双击列表跳转。契约见主仓 `.trellis/spec/XQ/visualization/render-scene.md` §「切片拾取交互」。
- **P3-1 遗留局限(P3-2 头号要修)**:`XQMainWindow::bindCrossSectionPath`(约 :2879)**硬编码取场景第一条 Path**,不读用户勾选,无选路径控件 → 真机换不了路径。P3-2 的「选路径下拉」正是修这个。

## 4. channel worker 流程(踩过的坑,务必遵守)

- **spawn 绝不用 `--file`/`--jsonl` 挂文件**——会让 worker 首轮卡死超时(memory `channel-spawn-file-jsonl-hangs-worker`)。spawn 只 `--agent implement --as impl --cwd C:/Users/OCEAN/Desktop/XIAOQUAN --timeout 90m`;然后 `channel send --to impl --as main` 用 **`--text-file <简报路径>`**(或正文位置参数)把简报+文档绝对路径给 worker,让它 Read。注意 `send` 无 `--kind`/`--text`(那是 wait 的);正文是位置参数或 `--text-file`。
- worker cwd 设**主仓**(能找 agent 定义 `.trellis/agents/implement.md` 与 spec);代码文件用 **worktree 绝对路径 CODE_ROOT**。两个不同 git 工作树别混。
- **判 worker 死活**:`channel list` 看活/注册数(`0/1`=死);别信 `channel wait` 空退=完工。wait 参数:`--as main --from impl --kind turn_finished`(不是 turn_ended)。
- **worker turn 可能在构建没跑完时就 done**(pick-fix 实测:worker 写完代码但没跑完 ctest/假绿就 turn_finished)。**done≠验证完成≠工作树干净**。主审(你)必接管全部验证:全新构建 + 全量 ctest + 假绿抽查 + 亲读最终代码(memory `subagent-mainreview-must-read-code`)+ git diff 核残留。
- 完工后 `channel rm <chan>` 清理(名额上限 6);清理 send 用的临时简报文件。
- 派 check worker 复验是 Trellis 流程要求(implement→check→spec→commit);check 也做假绿抽查,主审亲读其报告不只看 done。

## 5. 构建/测试/提交(硬规范)

- 全新构建(改 Q_OBJECT 头必须,memory `ninja-stale-moc-gui-crash`):`rm -rf "CODE_ROOT/build_gui"` 然后 `cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"`。增量:直接跑 build_gui_wt.bat(不删目录)。当前基线全新构建 243/243。
- ctest:`cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\ctest_merge.bat"`(当前基线 **70/70 全绿**)。**必走 ctest,裸跑 test exe 缺 VTK DLL exit 127**(memory `ctest-environment-overrides-path`)。
- **假绿抽查**:每个新断言篡改被测逻辑→走 ctest 该测试真转红(**核 exe 时间戳变**)→还原复绿。
- **真机**:`cmd //c "...\run_xq.bat"`,用户用 `! run_xq.bat` 自己开;**主会话读不出本地截图**(memory `harness-cannot-read-local-images`),真机判定靠用户。**派用户真机前必核 exe 时间戳新过所有改动源**(memory `gui-realmachine-test-verify-exe-timestamp`——上一批踩过:拿旧 exe 让用户测=无效测试,把「修复没生效」误判成「方向错」链式深挖浪费多轮)。**GUI 任务 ctest 全绿≠达标,真机是第一门禁**(memory `gui-task-green-tests-not-done`)。
- i18n:手工编辑 `resources/i18n/xq_zh_CN.ts`(UTF-8 无 BOM+LF)不跑 lupdate(memory `xqtr-translate-lupdate-blind`);面板串走 `xqTr`=translate("XQStageWidgets",..);主窗口串走 `tr()`(context xq::XQMainWindow)。`lrelease xq_zh_CN.ts -qm xq_zh_CN.qm` 报 0 unfinished。
- 新源文件注释一律**英文**(MSVC 无 /utf-8 无 BOM,中文注释按 GBK 误读=假语法错,memory `msvc-gbk-chinese-comment-syntax-error`)。
- 提交:精确 `git add <路径>` **绝不 `-A`**(worktree 有 `xq_app_dist/` 构建产物 + 上级目录一堆历史 CHECK-*.md/EXECUTE-*.md 都不能进库,memory `commit-check-gitignore-present`);中文信息,格式 `类型: 简短描述`;结尾 `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`。worktree 分支 `feat/render-arch`;spec/任务文档提主仓 `fix/xq-global-audit`。

## 6. 层次纪律(P3-2 关键)

- `core`(无 VTK/Qt):XQPath/XQContourGroup/ContourFrame。
- `services`(**无 VTK/Qt**):新增 `ContourExtractionService`(圆/多边形几何生成)放这。纯域。
- `visualization`(VTK 私有):XQCrossSectionViewWidget 绘制交互、XQRenderScene 标记。头保持 VTK-free(void*/pimpl/POD)。
- `app`(接线):XQMainWindow 绑路径下拉、入组命令、放样触发。
- 写代码前先读对应 layer 的 spec:`.trellis/spec/XQ/{core,visualization,architecture}/index.md`。

## 7. 新会话接手动作

1. 读本文件(已读)。
2. 读父任务 `.trellis/tasks/07-06-along-path-contouring/{prd,design,implement}.md`(P3-2 计划在 implement.md §:44)。
3. 需要时读 P3-1 交付代码(§3 列的文件)+ pick-fix 的 spec(render-scene.md 拾取交互场景)。
4. `C:/software/anaconda/python.exe ./.trellis/scripts/task.py start .trellis/tasks/07-06-along-path-contouring`(父任务应已 in_progress,若无则 start),按 Trellis 流程:P3-2 可能需补一份 EXECUTE-P3-2 简报(照 pick-fix 的 EXECUTE.md 格式)。
5. SV 参照(圆/多边形/放样)主会话亲读 `Externals/src/SimVascular/Code/Source/sv4gui/`,别派 agent(cyber 误拦)。
6. 派 channel worker(不挂 --file/--jsonl,send 正文给路径)→ 主审接管验证 → check worker → spec → commit → 真机 MVP 主里程碑。

## 8. 本任务链新增的 memory(新会话已自动加载索引)

- `gui-realmachine-test-verify-exe-timestamp` — 真机测前必核 exe 时间戳(否则旧 exe 无效测试→误判修复方向)。
- `xq-seed-pick-eventfilter-vtk-race` — 种子红点时有时无=事件竞争,拾取模式显式接管交互。
- `channel-spawn-file-jsonl-hangs-worker` — spawn 挂文件卡死 worker。
- `vcvars64-coldstart-hangs-use-devshell` — build/ctest 脚本 vcvars64 冷启动挂死换 DevShell(check worker 沉淀)。
