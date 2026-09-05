# PRD — GUI 工作流可用性:消灭 NodeId 手填 + 分步引导(07-04 B6 承接)

> 任务:`07-05-workflow-usability`;承接 07-04-render-arch-rebuild 规划的 B6 批次。
> 背景:07-04 B4 真机验收时用户反馈「工具指引性太弱、操作也很复杂,我没有理解怎么使用这些工具」。SV 对照调研(research/sv-workflow-comparison.md)给出 P1/P2 清单,用户拍板「P1/P2 合成 B6」。
> worktree:`C:\Users\OCEAN\Desktop\XQIAOQUAN-gui\XQ`,分支 `feat/render-arch`(基线 = B5 ead692c)。

## Goal(用户视角)

不看文档、不问人,按界面引导独立走通 **路径 → 分割 → 建模 → 网格** 链。

## Requirements(P1 + P2,七项 + 一项既有缺陷修复)

### P1 — 消灭 NodeId 手填(可用性断点)
1. **建模页**:contour group 的 NodeId 手填输入改为**场景节点选择控件**(下拉或复选列表,列出场景内全部 ContourGroup 节点名;照 SV SegSelectionWidget)。数据管理器选中 ContourGroup 时自动带入。
2. **网格页**:model 的 NodeId 手填改**下拉**(列出全部 Model/Surface 节点),选中自动带入同上。
3. **数据管理器右键直达**:右键 ContourGroup → 「用它建模」;右键 Model → 「用它建网格」(SV CreateAction 模式)。点击后切到对应阶段页并预选该节点。

### P2 — 操作引导
4. **分步引导条**:每页顶部 hint 升级为分步引导(「第 1/3 步:…」),按钮 enable 状态机可视化,完成一步自动亮下一步。
5. **参数 tooltip 补齐**:六页全部参数控件(照 SV 文案密度,中文)。
6. **阈值估计按钮**:分割页阈值控件旁加「估计」按钮,按当前图像强度直方图给建议值(照 SV Estimate 先例;不自动改用户已填值)。
7. **Ctrl+A 加点 + 模式指示浮层**:拾取态下 Ctrl+A 把当前十字线位置加为控制点(SV 同款);拾取/画轮廓模式激活时视图角落浮层显示当前模式与退出方式(Esc),阶段页按钮保持 checked 态。

### 既有缺陷(07-04 B2a worker 发现,顺带修)
8. **addPath 绕过 session 回调**:路径添加走了 commandStack 直推而非 session 网关,漏 refreshSceneTree 统一时机。修正为与其它交互命令一致。

## 不做(明确出界)

- P3 沿路径 reslice 分割(单独任务出 PRD);
- 色阶条(后置遗留);
- 新渲染能力/几何算法;core/services/io 层改动(除非引导条需要只读探针);
- 「可配置快捷键」之类未要求的扩展。

## Acceptance Criteria

- [ ] 全新构建 + 全量 ctest 绿(含假绿抽查纪律);
- [ ] **真机(硬门禁,用户签收)**:打开 0007 工程,按界面引导独立走通四阶段链;建模/网格页无任何手填 NodeId;右键直达可用;模式浮层/Ctrl+A 生效;
- [ ] i18n:全部新串中文齐(xqTr 手工 ts + lrelease,0 unfinished)。

## 约束(纪律沿用 07-04)

- channel worker 实现 + check 复核 + 主审亲读复跑;
- 改 Q_OBJECT 头必须全新构建;新文件注释英文;不 mock 测试;
- headless 可测性不得反向决定交互设计,观感/手感只认真机。
