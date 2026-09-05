# 任务总结

## 已创建的文档

1. **README.md** (主文档)
   - 执行摘要、问题分类、架构方案
   - 修复优先级、测试策略、时间线

2. **audit-report.md** (详细审查报告)
   - 16 个问题完整描述
   - 每个问题：位置、代码、根因、触发条件、后果、修复、测试

3. **checklist.md** (清单)
   - 4 个 Phase 的详细 checklist
   - 验收标准、不在范围项

4. **architecture-discussion.md** (架构讨论)
   - 5 个架构级问题深度分析
   - 业界对比、推荐方案、迁移策略

5. **EXECUTE-PHASE1.md** (Phase 1 执行指令)
   - Blocker-1: redo() 丢命令（逐行修改指令）
   - Blocker-2: 缓存键冲突（完整代码替换）
   - Blocker-3: service 层 push()（自动化脚本）

6. **EXECUTE-PHASE2.md** (待创建)
   - Critical-1~5 的详细修复指令

7. **EXECUTE-PHASE3.md** (待创建)
   - High 级问题修复 + 测试补充

## 文档特点

所有 EXECUTE-*.md 文档都是**完整可执行的指令**，包含：
- 确切的文件路径和行号
- 当前代码 vs 新代码的完整对比
- 逐步执行指令（可交给便宜 agent）
- 验证步骤和预期输出
- 自动化脚本（如适用）

## 使用方法

### 方式 1: 手动执行
```bash
cd .trellis/tasks/07-02-m8b2-critical-fixes
# 阅读 EXECUTE-PHASE1.md
# 按指令逐条执行
```

### 方式 2: 交给 agent
```
你是一个代码修复 agent。
请严格按照 .trellis/tasks/07-02-m8b2-critical-fixes/EXECUTE-PHASE1.md 
中的指令逐条执行，完成所有修复。

不要偏离指令，不要自己发挥。每完成一个修复，运行验证步骤。
```

### 方式 3: 自动化脚本
Phase 1 Blocker-3 提供了 Python 脚本 `fix_push_checks.py`，
可直接运行批量修改 service 层测试。

## 当前状态

- [x] 完成严格代码审查（10 角度 + 手动）
- [x] 创建 trellis 任务结构
- [x] 编写 Phase 1 详细执行指令
- [ ] 编写 Phase 2-4 执行指令（待完成）
- [ ] 执行修复
- [ ] 验收测试

## 下一步

创建 Phase 2-4 的详细执行指令，确保便宜 agent 能无歧义执行。
