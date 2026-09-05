# M8b-2 运行时内存管理 Critical Fixes

> 任务创建时间: 2026-07-02
> 优先级: **Blocker** (禁止合并当前分支直到修复完成)
> 分支: `feat/m8b2-runtime-memory` (当前未提交改动)

---

## 执行摘要

**严格代码审查发现 16 个必须修复的问题 (3 Blocker + 5 Critical + 5 High + 3 Medium)**，最大风险：

1. **XQCommandStack::redo() 失败丢失命令** → 用户数据永久丢失
2. **GeometryResourceManager 缓存键冲突** → 渲染错误几何或崩溃
3. **service 层 push() 返回值未检查** → 静默失败无错误提示

当前分支**禁止合并**，必须先修复所有 Blocker 和 Critical 问题。

---

## 问题分类

### Blocker (必须立即修复，阻止合并)

1. **redo() 丢失命令** (`XQCommandStack.cpp:44-57`)
   - 根因: execute() 失败时命令已从 redo_stack_ 移除但未归还
   - 后果: 用户撤销历史永久丢失，栈状态不一致
   - 修复: 失败时 `redo_stack_.push_back(std::move(command))`

2. **缓存键冲突** (`GeometryResourceManager.cpp:170-250`)
   - 根因: voxel 和 geometry 共用一个 map，类型检查缺失
   - 后果: 返回错误几何类型，空指针崩溃
   - 修复: 分离缓存或在 ResidentKey 加类型标签

3. **service 层 push() 静默失败** (所有 service 测试)
   - 根因: push() 改返回 bool 后调用点未检查
   - 后果: 测试假阳性，生产代码静默失败
   - 修复: 所有 `stack.push()` 调用点加返回值检查

### Critical (高风险，必须修复)

4. **AddNodeWithSourceRelationCommand 回滚误删外部节点** (`XQSceneCommands.cpp:59-79`)
5. **mark_source_changed 自环标记源节点 stale** (`XQScene.cpp:127-139`)
6. **BlobStore 路径遍历检查可被符号链接绕过** (`BlobStore.cpp:298-318`)
7. **GeometryResourceManager blob 校验失败后状态不一致** (`GeometryResourceManager.cpp:282-299`)
8. **attachGeometryResources 竞态条件** (`XQMainWindow.cpp:171-177`)

### High (重要但非阻塞)

9. **validation 函数无单元测试** (9 个安全关键函数)
10. **mark_source_changed 性能退化** (O(N) 拷贝)
11. **connectivity_in_bounds 逻辑顺序错误** (reinterpret_cast 在检查前)
12. **缓存键生成无冲突防护** (手动分配整数值)

### Medium (代码质量)

13. **未经请求的降级分支** (违反 CLAUDE.md)
14. **路径/blob 校验逻辑重复 3 处**
15. **缓存键文档化不足**

---

## 架构层面问题

### 1. Command 模式实现缺陷

**问题**: 当前 `XQCommand::execute()` 返回 bool，但以下违反"Command success gates undo stack"契约:

- `XQCommandStack::redo()` 在 execute() 失败时丢失命令 (违反 `command-and-scene.md:72-78`)
- service 层未检查 push() 返回值 (违反 `command-and-scene.md:70`)
- 命令的 `inserted_` 标志是冗余状态 (可由栈位置推导)

**架构根因**: Command 模式的"执行-记录-撤销"三步骤中，第二步(记录)与第一步(执行)的原子性保证不足。

**推荐方案** (按复杂度递增):

#### 方案 A: 最小修复 (立即采用)
```cpp
bool XQCommandStack::redo() {
    if (redo_stack_.empty()) return false;
    std::unique_ptr<XQCommand> command = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    if (!command->execute()) {
        redo_stack_.push_back(std::move(command));  // ← 归还
        return false;
    }
    undo_stack_.push_back(std::move(command));
    return true;
}
```

优点: 1 行修复，符合现有 API
缺点: `inserted_` 标志仍冗余

#### 方案 B: 移除冗余标志 (中期重构)
去掉所有命令的 `inserted_` 字段，`undo()` 直接执行 (栈保证命令已成功执行)。

理由: `push()` 只在 execute() 成功后入栈，所以 `undo()` 被调用时 `inserted_` 必为 true，检查是死代码。

#### 方案 C: RAII Command Guard (长期架构改进)
```cpp
class CommandGuard {
    XQScene* scene_;
    std::function<void()> rollback_;
public:
    ~CommandGuard() { if (rollback_) rollback_(); }
    void commit() { rollback_ = nullptr; }
};

bool AddNodeCommand::execute() {
    CommandGuard guard(scene_, [&]{ scene_->remove(id_); });
    if (scene_->insert(node_) != Inserted) return false;
    guard.commit();
    return true;
}
```

优点: 自动回滚，消除手动状态追踪
缺点: 需重写所有命令

**本次任务采用方案 A**，B/C 留待后续重构。

---

### 2. GeometryResourceManager 缓存设计缺陷

**问题**: `blocks_` 是 `std::map<ResidentKey, ResidentBlock>`，其中:
- `ResidentKey = std::pair<AssetId, int>`
- `ResidentBlock` 持有 `voxelSource` 和 `geometrySource` 两个字段 (互斥但无类型标签)
- `int` 值手动分配 (0,1,10,11,20,21,30,31)，无编译时冲突检测

**架构根因**: 用一个统一 map 存储异构类型 (voxel vs geometry)，但键空间未隔离。

**推荐方案**:

#### 方案 A: 类型安全枚举键 (推荐)
```cpp
enum class BlockKind {
    VoxelFull = 0,
    VoxelSegmented = 1,
    GeometrySurfaceFull = 20,
    GeometrySurfaceSegmented = 21,
    GeometryTetFull = 30,
    GeometryTetSegmented = 31,
    GeometryAllFull = 10,
    GeometryAllSegmented = 11
};

typedef std::tuple<AssetId, BlockKind> ResidentKey;
```

优点:
- 编译时类型安全
- 可扩展 (新增类型无冲突风险)
- 自文档化

实施:
1. 定义 `BlockKind` enum class
2. 改 `ResidentKey` 为 `std::tuple<AssetId, BlockKind>`
3. 更新 `voxelBlockKey` / `geometryBlockKey` 返回类型
4. 缓存命中时断言类型匹配:
```cpp
auto it = blocks_.find(key);
if (it != blocks_.end()) {
    assert(it->second.voxelSource != nullptr);  // 类型匹配
    return VoxelSourceHandle(it->second.voxelSource, it->second.pin);
}
```

#### 方案 B: 分离 voxel 和 geometry 缓存 (备选)
```cpp
std::map<VoxelCacheKey, VoxelBlock> voxelBlocks_;
std::map<GeometryCacheKey, GeometryBlock> geometryBlocks_;
std::list<CacheKey> lru_;  // 混合 LRU
```

优点: 类型完全隔离
缺点: LRU 跨两个 map，evict 逻辑复杂

**本次任务采用方案 A**。

---

### 3. 安全校验分层混乱

**问题**: 路径遍历/溢出检查在 3 个地方重复:
- `XQProjectReader.cpp:979` `is_confined_relative_path()`
- `BlobStore.cpp:119` `isConfinedRelativePath()`
- `GeometryResourceManager.cpp:80` `isConfinedRelativePath()`

逐字相同但未复用，且检查时机不一致:
- BlobStore 在文件打开**之后**检查 (可能被符号链接绕过)
- XQProjectReader 在解析时检查 (太早，无法处理规范化路径)

**架构根因**: 缺少统一的"安全路径"类型，校验逻辑散落在各层。

**推荐方案**:

#### 方案: 引入 `ConfinedPath` 值类型
```cpp
// src/core/util/ConfinedPath.h
class ConfinedPath {
public:
    static std::optional<ConfinedPath> from(
        const std::string& relPath,
        const std::string& rootDir);
    
    const std::string& relativePath() const { return relPath_; }
    std::string absolutePath() const { return absolute_; }

private:
    ConfinedPath(std::string rel, std::string abs);
    std::string relPath_;
    std::string absolute_;  // 已规范化且验证在 rootDir 内
};
```

校验逻辑集中在 `from()`:
```cpp
std::optional<ConfinedPath> ConfinedPath::from(
    const std::string& relPath,
    const std::string& rootDir)
{
    if (relPath.empty()) return std::nullopt;
    
    const fs::path rel(relPath);
    if (rel.is_absolute() || rel.has_root_name() || 
        rel.has_root_directory()) {
        return std::nullopt;
    }
    
    // 检查路径组件 (阻止 "..")
    for (const auto& part : rel) {
        if (part == "..") return std::nullopt;
    }
    
    // 拼接并规范化
    const fs::path joined = fs::path(rootDir) / rel;
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(joined, ec);
    if (ec) return std::nullopt;
    
    // 验证最终路径在 rootDir 内 (防符号链接逃逸)
    const fs::path canonicalRoot = fs::canonical(rootDir, ec);
    if (ec) return std::nullopt;
    
    const std::string absStr = canonical.string();
    const std::string rootStr = canonicalRoot.string();
    if (!absStr.starts_with(rootStr)) {
        return std::nullopt;
    }
    
    return ConfinedPath(relPath, absStr);
}
```

消费侧:
```cpp
// BlobStore
auto path = ConfinedPath::from(ref.relPath, rootDir_);
if (!path) return Status::InvalidMetadata;
std::ifstream is(path->absolutePath(), std::ios::binary);
```

优点:
- 一次校验，到处安全使用
- 防符号链接逃逸 (canonical 解析后二次检查)
- 类型安全 (有 `ConfinedPath` 即已验证)

**本次任务先修 BlobStore 符号链接漏洞，ConfinedPath 留后续重构**。

---

### 4. 测试策略缺陷

**问题**: 
- 9 个安全关键校验函数无单元测试，仅有集成测试
- service 层测试未检查 push() 返回值 (假阳性)
- 命令失败场景覆盖不足 (redo 失败、回滚逃逸)

**架构根因**: 缺少"安全关键代码必须单测"的强制规则。

**推荐方案**:

#### 测试分层策略
```
单元测试 (必须)
├─ 安全校验函数 (溢出/路径遍历/格式校验)
├─ Command execute/undo 原子性
└─ 缓存键生成/冲突检测

集成测试 (补充)
├─ 端到端流程 (reader → payload → renderer)
└─ 并发/驱逐/预算策略

系统测试 (验收)
└─ 完整 GUI 工作流
```

**立即行动**:
1. 新建 `tests/core/test_buffer_ref_validation.cpp` 覆盖所有校验函数
2. 新建 `tests/core/test_command_atomicity.cpp` 覆盖失败回滚
3. 所有 service 测试加 `REQUIRE(stack.push(...))`

---

### 5. 违反 CLAUDE.md 规则

**问题**: `XQMainWindow.cpp:306-333` 的 fallback 逻辑:
```cpp
if (!stats.ok) {
    lazy = resolveLazyGeometrySource(..., SurfaceOnly);
    if (lazy.valid()) { stats = renderer.addSurfaceProgressive(...); }
}
```

体网格渲染失败自动降级为表面，改变可见行为，违反:
> 别在我没要求时加降级/兜底分支(会改变可见行为) — CLAUDE.md:42

**架构根因**: UI 层试图"智能恢复"，但掩盖了真实错误。

**推荐方案**:

#### 方案 A: 移除 fallback，显示错误 (推荐)
```cpp
if (lazy.valid() && lazy->meta().tetCount > 0) {
    stats = renderer.addVolumeMeshProgressive(lazy.source());
    if (!stats.ok) {
        showErrorMessage("Failed to render volume mesh");
        return;
    }
}
```

#### 方案 B: 显式用户选择
如果确实需要降级，应该:
1. 渲染失败时弹对话框: "Volume mesh failed. Render surface instead?"
2. 用户确认后再降级
3. 记录用户选择到 session state

**本次任务采用方案 A** (移除 fallback)。

---

## 修复优先级与时间线

### Phase 1: Blocker 修复 (1-2 天)
- [ ] `XQCommandStack::redo()` 归还命令
- [ ] `GeometryResourceManager` 缓存键类型安全
- [ ] service 层所有 `push()` 调用点加检查

**验收**: 所有现有测试通过 + 新增回归测试

### Phase 2: Critical 修复 (2-3 天)
- [ ] `AddNodeWithSourceRelationCommand` 回滚逻辑
- [ ] `mark_source_changed` 自环过滤
- [ ] `BlobStore` 符号链接防御
- [ ] `GeometryResourceManager` blob 校验状态
- [ ] `attachGeometryResources` 原子化

**验收**: 安全测试通过 + 代码审查

### Phase 3: High 修复 (3-5 天)
- [ ] 9 个 validation 函数单元测试
- [ ] 性能优化 (可选)
- [ ] 代码清理

**验收**: 测试覆盖率 > 90%

### Phase 4: Medium 清理 (1-2 天)
- [ ] 移除未经请求的 fallback
- [ ] 提取路径校验到公共模块
- [ ] 文档化缓存键

**验收**: CLAUDE.md 规则合规

---

## 测试策略

### 必须新增的测试

#### 1. `tests/core/test_command_atomicity.cpp`
```cpp
// redo 失败归还命令
TEST_CASE("redo failure preserves redo stack") {
    XQScene scene;
    XQCommandStack stack;
    stack.push(AddNodeCommand(&scene, node1));
    stack.undo();
    scene.insert(node1);  // 外部插入使 redo 失败
    const size_t before = stack.redo_count();
    REQUIRE_FALSE(stack.redo());
    REQUIRE(stack.redo_count() == before);
}

// 回滚不误删外部节点
TEST_CASE("relation failure rollback only removes inserted node") {
    XQScene scene;
    scene.insert(XQDataNode(100, "external"));
    // ... 验证 external 节点不被误删
}
```

#### 2. `tests/core/test_buffer_ref_validation.cpp`
```cpp
TEST_CASE("is_hex_sha256 edge cases") {
    REQUIRE_FALSE(is_hex_sha256(""));
    REQUIRE_FALSE(is_hex_sha256("G00..."));  // 非十六进制
    REQUIRE_FALSE(is_hex_sha256("abc"));     // 长度错误
}

TEST_CASE("checked_mul_u64 overflow") {
    uint64_t out;
    REQUIRE_FALSE(checked_mul_u64(UINT64_MAX, 2, &out));
}

TEST_CASE("is_confined_relative_path traversal") {
    REQUIRE_FALSE(is_confined_relative_path("../etc/passwd"));
    REQUIRE_FALSE(is_confined_relative_path("/abs/path"));
}
```

#### 3. `tests/services/test_geometry_cache_collision.cpp`
```cpp
TEST_CASE("voxel and geometry cache distinct") {
    GeometryResourceManager mgr(&registry, assetRoot);
    auto v = mgr.acquireVoxelSource(id, voxelSpec);
    auto g = mgr.acquireGeometrySource(id, geoSpec);
    REQUIRE(v.valid());
    REQUIRE(g.valid());
    // 验证两者不冲突
}
```

### 修改现有测试

所有 service 层测试的 `stack.push()` 调用改为:
```cpp
if (!stack.push(std::move(cmd.command))) {
    return TestResult::Failed;
}
```

---

## 风险与依赖

### 风险
1. **API 兼容性**: `ResidentKey` 类型变更可能影响已编译的测试
2. **性能回退**: 缓存键改用 tuple 可能影响查找性能 (需 benchmark)
3. **测试时间**: 新增大量单元测试会延长 CI 时间

### 缓解措施
- Phase 1 完成后立即合并 (减少分支发散)
- 性能回归用 `test_geometry_resource_manager` 的计时断言保护
- 单元测试并行执行 (ctest -j)

### 依赖
- 无外部依赖
- 需要 git worktree 隔离 (避免干扰主分支开发)

---

## 后续架构改进 (不在本次范围)

1. **Command RAII Guard** (方案 C)
2. **ConfinedPath 值类型** (统一路径校验)
3. **Source 1.0 公开冻结** (单测覆盖率达标后)
4. **性能优化**: mark_source_changed 懒标记 / 缓存派生集

---

## 参考

- 审查报告: 本 README 同级 `audit-report.md`
- 架构规范: `.trellis/spec/XQ/core/command-and-scene.md`
- Source 接口: `.trellis/spec/XQ/core/source-interface.md`
- Memory: `~/.claude/projects/.../memory/` 相关坑点记录
