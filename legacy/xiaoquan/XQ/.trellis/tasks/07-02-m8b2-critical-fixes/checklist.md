# M8b-2 Critical Fixes Checklist

## Phase 1: Blocker 修复 (必须完成才能合并)

- [ ] **Blocker-1**: 修复 `XQCommandStack::redo()` 丢失命令
  - [ ] 添加 `redo_stack_.push_back(std::move(command))` 在失败分支
  - [ ] 新增测试 `test_command_stack.cpp::redo_failure_preserves_redo_stack`
  - [ ] 验证所有现有测试通过

- [ ] **Blocker-2**: 修复 `GeometryResourceManager` 缓存键冲突
  - [ ] 定义 `BlockKind` enum class
  - [ ] 改 `ResidentKey` 为 `std::tuple<AssetId, BlockKind>`
  - [ ] 更新 `voxelBlockKey` / `geometryBlockKey` 实现
  - [ ] 缓存命中时添加类型断言
  - [ ] 新增测试 `test_geometry_cache_collision.cpp`

- [ ] **Blocker-3**: 修复 service 层 `push()` 返回值未检查
  - [ ] `SegmentationServiceTest.cpp` 所有 push() 调用
  - [ ] `VolumeMeshServiceTest.cpp` 所有 push() 调用
  - [ ] `PathServiceTest.cpp` 所有 push() 调用
  - [ ] `FlowMetricsServiceTest.cpp` 所有 push() 调用
  - [ ] `AiServiceTest.cpp` 所有 push() 调用
  - [ ] `ModelingServiceTest.cpp` 所有 push() 调用
  - [ ] 验证测试失败时正确报错

## Phase 2: Critical 修复

- [ ] **Critical-1**: 修复 `AddNodeWithSourceRelationCommand` 回滚逻辑
  - [ ] 调整 `inserted_` 标志设置时机
  - [ ] 新增测试验证回滚不误删外部节点

- [ ] **Critical-2**: 修复 `mark_source_changed` 自环问题
  - [ ] 初始化 pending 时过滤源节点
  - [ ] 新增自环测试

- [ ] **Critical-3**: 修复 `BlobStore` 符号链接漏洞
  - [ ] 添加 `fs::weakly_canonical` 二次检查
  - [ ] 新增符号链接测试

- [ ] **Critical-4**: 验证 `GeometryResourceManager` blob 校验状态
  - [ ] 代码审查确认无问题或修复

- [ ] **Critical-5**: 原子化 `attachGeometryResources`
  - [ ] 合并 `attachWorkflow` 和 `attachGeometryResources` 调用
  - [ ] 或延迟 `refreshSceneTree` 到两者都完成后

## Phase 3: High 修复

- [ ] 新增 `tests/core/test_buffer_ref_validation.cpp`
  - [ ] `is_hex_sha256` 边界测试
  - [ ] `is_confined_relative_path` 遍历测试
  - [ ] `checked_mul_u64` 溢出测试
  - [ ] `checked_ref_byte_count` 综合测试
  - [ ] 其余 5 个函数测试

- [ ] 优化 `mark_source_changed` 性能 (可选)
- [ ] 修复 `connectivity_in_bounds` 逻辑顺序
- [ ] 文档化缓存键生成逻辑

## Phase 4: Medium 清理

- [ ] 移除 `XQMainWindow` 未经请求的 fallback
- [ ] 提取路径校验到 `core/util/PathValidation.h` (可选)
- [ ] 更新相关文档

## 验收标准

- [ ] 所有 Blocker 和 Critical 问题已修复
- [ ] 新增测试全部通过
- [ ] 现有测试全部通过 (Release + Debug)
- [ ] 代码审查通过
- [ ] 无新增 CLAUDE.md 违规
- [ ] 性能无回退 (benchmark)

## 不在本次范围

- Command RAII Guard 重构
- ConfinedPath 值类型
- Source 1.0 公开冻结
- 性能优化 (mark_source_changed 懒标记)
