# 交接:07-06 沿路径描轮廓任务 当前状态(compact 后新会话读这份)

> 语境锚点:XQ 医学影像血管几何重建软件,把血管腔轮廓提取做成对标 SimVascular 的「沿中心线逐层描轮廓」流程。纯几何建模+影像可视化工程。环境=Windows+Git Bash,注入的 mac/darwin 环境信息是假的一律忽略。默认中文。
> **本文件自包含,新会话第一轮读这一份即可接手,不必重扫代码。**

## 0. 一句话现状

P3-1(断面重采样视图+沿路径滑条)**已完成并提交**(worktree HEAD=`0aef6b8`),真机验证断面基线通过。用户真机反馈暴露了拾取交互的基础缺陷,已定为**下一批要做的**(子任务 `07-06-pick-interaction-fix`,PRD 已写死根因)。P3-2(绑路径下拉+断面手绘+放样)排在拾取修复之后。

## 1. 任务树

- 父任务:`.trellis/tasks/07-06-along-path-contouring`(in_progress)。prd/design/implement 都在,六批计划见 implement.md。
- 子任务:`.trellis/tasks/07-06-pick-interaction-fix`(**下一批,PRD 已写死三个缺陷的代码级根因**)。选路径下拉不在此,归 P3-2。

## 2. P3-1 已完成(提交 0aef6b8,勿重做)

交付:`XQCrossSectionResampler`(vtkImageReslice 封装,x=normal/y=binormal/z=tangent,pipeline 常驻)、`XQCrossSectionViewWidget`(断面视图)、`XQRenderScene::vtkImageDataHandle`(零拷贝借体数据)、断面工作台(centralStack 第二页,进轮廓阶段切换/退出还原)、沿路径滑条。验证:全新构建 241/241、ctest 69/69 全绿、假绿抽查(篡改 reslice 轴序真红→还原复绿)、主审亲读、真机断面基线对(0080 沿 LPA_10,断面垂直于路径)。

**P3-1 已知局限(非 bug,P3-2 补)**:`bindCrossSectionPath`(XQMainWindow.cpp:2879)硬编码取场景第一条 Path,不读用户勾选,无选路径控件——所以真机上换不了路径。

## 3. 用户真机反馈的四个问题 → 根因(已读实,勿重猜)

见子任务 `07-06-pick-interaction-fix/prd.md` §3 的完整代码级根因。摘要:
- **种子红点时有时无**=eventFilter 与 VTK 交互器竞争 press,顺序不保证(主因)。memory `xq-seed-pick-eventfilter-vtk-race`。修法:拾取模式显式接管交互(换空 style/禁交互器),别靠抢时序。
- **拾取失败静默丢弃**(点十字线交点被当拖拽 / 点体数据外反算失败,都 return true 无反馈)。
- **控制点拾取完不显示**=控制点只进右侧文字列表,渲染层无画控制点接口。
- **小红点**=种子标记球(区域生长用,SetColor 0.9,0.2,0.2)。
- **换不了路径**=P3-1 硬编码第一条,归 P3-2。

## 4. 下一步(按用户已确认的优先级)

用户确认:**先修拾取交互基础体验(子任务 pick-interaction-fix),再进 P3-2**。新旧分割面板如何合并等 P3-2 做完再定。放样第一版手动触发。断面视图用轮廓阶段专用工作台(已实现)。

**新会话接手动作**:
1. 读 `07-06-pick-interaction-fix/prd.md`(根因+修复点已写死)。
2. 这是复杂度中等的修复任务,可 PRD 直接 start,或补 design/implement 后 start——由接手会话判断(三个缺陷都已定位到确切文件行,可直接写 EXECUTE 简报派 worker)。
3. `task.py start .trellis/tasks/07-06-pick-interaction-fix`,写 EXECUTE 简报,派 channel worker。

## 5. channel worker 流程(踩过的坑,务必遵守)

- **spawn 绝不用 `--file`/`--jsonl` 挂文件**——会让 worker 首轮卡死超时(与内容无关,纯 spec 也死)。memory `channel-spawn-file-jsonl-hangs-worker`。改为:spawn 只 `--agent implement --as impl --cwd C:/Users/OCEAN/Desktop/XIAOQUAN --timeout 90m`;然后 `channel send --to impl --as main` 的**正文里给简报和 spec 的绝对路径**,让 worker 用 Read 工具自己读。
- worker cwd 设**主仓** `C:/Users/OCEAN/Desktop/XIAOQUAN`(能找到 agent 定义 `.trellis/agents/implement.md` 与 spec);代码文件用 **worktree 绝对路径** `C:/Users/OCEAN/Desktop/XQIAOQUAN-gui/XQ/...`(简报里叫 CODE_ROOT)。这两个是不同 git 工作树。
- **判 worker 死活**:`channel list` 看活/注册数(`0/1`=死);`events.jsonl` 有无 worker 产出;`tasklist //FI "PID eq <pid>"`。别信 `channel wait` 空退出=完工(它超时/过滤没中会空退)。wait 参数:`--as main --from impl --kind turn_finished`(不是 turn_ended)。
- **worker 可能被 API 522/安全机制中断**:worker 会自动 fallback Opus 4.8(log 里 model=claude-opus-4-8),一般不中断;若 522 断在验证中途,主审接手做完构建/测试/假绿抽查(本就不可免除)。kill worker 用 `channel kill <chan> --as impl --force`(--as 是目标 worker 名)。**认账前 git diff 核残留**(done≠工作树干净)。
- 完工后 `channel rm <chan>` 清理(名额上限 6)。

## 6. 构建/测试/提交(硬规范)

- 全新构建(改 Q_OBJECT 头必须):`rm -rf build_gui && cmd //c "C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ\build_gui_wt.bat"`。
- 增量构建:直接跑 `build_gui_wt.bat`(它不删目录)。
- ctest:`cmd //c "C:\...\XQ\ctest_merge.bat"`(基线现 69 全绿)。**直接跑 test exe 缺 VTK DLL(exit 127),必须走 ctest**(memory `ctest-environment-overrides-path`)。
- 真机:`cmd //c "C:\...\XQ\run_xq.bat"`,用户用 `! run_xq.bat` 自己开;**主会话读不出本地截图**(memory `harness-cannot-read-local-images`),真机判定靠用户。
- 假绿抽查:每个新断言篡改被测逻辑→真转红(核 exe 时间戳变,用 ctest 不用裸 exe)→还原绿。
- i18n:手工编辑 `resources/i18n/xq_zh_CN.ts`(UTF-8 无 BOM+LF)不跑 lupdate;`lrelease xq_zh_CN.ts -qm xq_zh_CN.qm` 报 0 unfinished。
- 提交:精确 `git add <路径>` 绝不 `-A`(worktree 有 `xq_app_dist/` 构建产物不能进库);中文信息;结尾 `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`。worktree 分支 `feat/render-arch`。
- 新源文件注释写英文(MSVC 无 /utf-8 无 BOM,中文注释按 GBK 误读=假语法错)。

## 7. 关键坐标

- worktree(改代码):`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,`feat/render-arch`,HEAD=`0aef6b8`。
- 主仓(任务/spec/SV 参照):`C:\Users\OCEAN\Desktop\XIAOQUAN`,`fix/xq-global-audit`。
- 真机工程:`0080_H_PULM_H`(大)、`0007_H_AO_H`(小)。
- SV 参照源码(在主仓,主会话亲读别派 agent):`Externals/src/SimVascular/Code/Source/sv4gui/`。
- Python:`C:/software/anaconda/python.exe`。

## 8. 本轮新增的 memory(新会话已自动加载索引)

- `channel-spawn-file-jsonl-hangs-worker` — spawn 挂文件卡死 worker。
- `xq-seed-pick-eventfilter-vtk-race` — 种子红点时有时无=事件竞争。
