# 架构层面问题讨论

> 本文档讨论审查中发现的架构级问题及长期解决方案

---

## 1. Command 模式的"执行-记录"原子性问题

### 现状

当前 `XQCommand::execute()` 返回 bool，`XQCommandStack::push()` 先执行再入栈：

```cpp
bool XQCommandStack::push(std::unique_ptr<XQCommand> command) {
    if (!command->execute()) {
        return false;  // 执行失败，不入栈
    }
    undo_stack_.push_back(std::move(command));  // 成功后入栈
    redo_stack_.clear();
    return true;
}
```

**设计意图**: 只有成功的命令才进入 undo 栈。

### 发现的问题

1. **`redo()` 破坏不变量**: execute() 失败时命令从 redo_stack_ 移除但未归还
2. **冗余状态追踪**: 每个命令用 `inserted_` 标志追踪执行状态，但这可从栈位置推导
3. **回滚逻辑脆弱**: `AddNodeWithSourceRelationCommand` 的多步操作回滚容易出错

### 架构根因

Command 模式的经典实现假设 execute() 总是成功（或抛异常），但 XQ 采用"explicit result, no exceptions"原则，导致需要手动管理"执行成功才记录"的不变量。

### 对比业界实践

| 实现方式 | 代表 | 原子性保证 | 复杂度 |
|---------|------|----------|--------|
| 异常驱动 | Qt Undo Framework | try { execute(); push(); } catch {} | 简单，但违反 XQ 无异常原则 |
| 两阶段提交 | Eclipse EMF | canExecute() + execute() + commit() | 复杂，但原子性强 |
| RAII Guard | 本提案 | 自动回滚，手动 commit | 中等，类型安全 |
| 当前实现 | XQ | 手动 bool 返回值 + 手动状态 | 简单但易错 |

### 推荐长期方案: RAII Command Guard

```cpp
class CommandExecutionGuard {
    std::function<void()> rollback_;
    bool committed_ = false;
public:
    explicit CommandExecutionGuard(std::function<void()> rollback)
        : rollback_(std::move(rollback)) {}
    
    ~CommandExecutionGuard() {
        if (!committed_ && rollback_) {
            rollback_();
        }
    }
    
    void commit() { committed_ = true; }
};

// 使用示例
bool AddNodeWithSourceRelationCommand::execute() {
    if (scene_->insert(node_) != Inserted) {
        return false;
    }
    
    CommandExecutionGuard guard([&] { scene_->remove(id_); });
    
    if (scene_->link_derived(source_, id_) != Linked) {
        return false;  // 自动回滚
    }
    
    guard.commit();  // 成功，取消回滚
    return true;
}
```

**优点**:
- 自动回滚，消除 `inserted_` 标志
- 异常安全（即使未来引入异常）
- 代码更清晰，意图明确

**缺点**:
- 需重写所有命令
- 增加一个辅助类

**迁移策略**:
1. Phase 1: 修复当前 redo() bug（最小改动）
2. Phase 2: 新命令采用 Guard 模式
3. Phase 3: 逐步迁移旧命令

---

## 2. 缓存管理的类型安全问题

### 现状

`GeometryResourceManager` 用一个统一的 map 存储异构类型：

```cpp
std::map<ResidentKey, ResidentBlock> blocks_;

struct ResidentBlock {
    std::unique_ptr<IVoxelSource> voxelSource;      // 互斥
    std::unique_ptr<IGeometrySource> geometrySource; // 互斥
    std::shared_ptr<void> pin;
    std::size_t bytes;
    std::list<AssetId>::iterator lruIt;
};
```

**问题**: 
- 缓存键 `ResidentKey = pair<AssetId, int>` 的 int 值手动分配，无冲突保护
- `ResidentBlock` 持有两个互斥字段，类型检查在运行时

### 业界对比

| 方案 | 类型安全 | 扩展性 | 示例 |
|------|---------|--------|------|
| std::variant | 编译时 | 需枚举所有类型 | Boost.Variant |
| std::any + RTTI | 运行时 | 任意类型 | Qt QVariant |
| 分离 map | 编译时 | 需维护多个容器 | Chromium base::IDMap |
| Tagged union + enum | 编译时 | 需手动标签 | **推荐** |

### 推荐方案: Enum Class 标签

```cpp
enum class BlockKind : int {
    VoxelFull = 0,
    VoxelSegmented = 1,
    
    GeometrySurfaceFull = 20,
    GeometrySurfaceSegmented = 21,
    
    GeometryTetFull = 30,
    GeometryTetSegmented = 31,
    
    GeometryAllFull = 10,
    GeometryAllSegmented = 11
};

using ResidentKey = std::tuple<AssetId, BlockKind>;

// 生成键时编译时检查
ResidentKey voxelBlockKey(const AssetId& id, const VoxelSourceSpec& spec) {
    return {id, spec.segmented ? BlockKind::VoxelSegmented 
                               : BlockKind::VoxelFull};
}
```

**优点**:
- 编译时类型安全
- 值冲突编译错误
- 自文档化

**缺点**:
- 需要迁移现有代码
- `std::tuple` 比 `std::pair` 稍慢（但可忽略）

---

## 3. 安全校验的分层混乱

### 现状

路径遍历防御在 3 处重复实现：
- `XQProjectReader::is_confined_relative_path()`
- `BlobStore::isConfinedRelativePath()`
- `GeometryResourceManager::isConfinedRelativePath()`

且检查时机不一致：
- BlobStore 在文件打开**后**检查（可被符号链接绕过）
- XQProjectReader 在解析时检查（太早，无法处理规范化路径）

### 架构根因

缺少"安全路径"值类型，校验逻辑耦合到各消费者。

### 推荐方案: ConfinedPath 值类型

```cpp
// src/core/util/ConfinedPath.h
class ConfinedPath {
public:
    // 工厂方法，校验失败返回 nullopt
    static std::optional<ConfinedPath> from(
        const std::string& relPath,
        const std::string& rootDir);
    
    const std::string& relativePath() const { return relPath_; }
    const std::string& absolutePath() const { return absolute_; }

private:
    ConfinedPath(std::string rel, std::string abs);
    
    std::string relPath_;   // 原始相对路径
    std::string absolute_;  // 已规范化且验证在 rootDir 内
};

// 实现
std::optional<ConfinedPath> ConfinedPath::from(
    const std::string& relPath,
    const std::string& rootDir)
{
    // 1. 检查路径组件（阻止 ".."）
    const fs::path rel(relPath);
    for (const auto& part : rel) {
        if (part == "..") return std::nullopt;
    }
    
    // 2. 拼接并规范化（解析符号链接）
    const fs::path joined = fs::path(rootDir) / rel;
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(joined, ec);
    if (ec) return std::nullopt;
    
    // 3. 验证最终路径在 rootDir 内（防符号链接逃逸）
    const fs::path canonicalRoot = fs::canonical(rootDir, ec);
    if (ec) return std::nullopt;
    
    if (!canonical.string().starts_with(canonicalRoot.string())) {
        return std::nullopt;
    }
    
    return ConfinedPath(relPath, canonical.string());
}
```

**使用**:
```cpp
// BlobStore
auto path = ConfinedPath::from(ref.relPath, rootDir_);
if (!path) return Status::InvalidMetadata;
std::ifstream is(path->absolutePath());
```

**优点**:
- 一次校验，到处安全
- 防符号链接逃逸
- 类型安全（有 ConfinedPath 即已验证）

**实施**:
1. Phase 1: 创建 ConfinedPath 类
2. Phase 2: BlobStore 先迁移（修复安全漏洞）
3. Phase 3: 其他消费者逐步迁移

---

## 4. 测试策略的缺失

### 现状

- 9 个安全关键函数无单元测试
- service 层测试未检查 push() 返回值（假阳性）
- 命令失败场景覆盖不足

### 问题

缺少"安全关键代码必须单测"的强制规则。

### 推荐测试金字塔

```
          /\
         /  \  系统测试 (GUI 工作流)
        /----\
       /      \  集成测试 (端到端流程)
      /--------\
     /          \  单元测试 (安全校验/Command 原子性)
    /------------\
   /--------------\
```

**分层原则**:
- 单元测试覆盖所有安全关键函数（溢出/路径遍历/格式校验）
- 集成测试覆盖跨层交互（reader → payload → renderer）
- 系统测试覆盖用户场景（打开项目 → 选中节点 → 渲染）

**强制规则**:
1. 任何涉及安全（路径/溢出/权限）的函数必须有单测
2. 任何返回 bool/Result 的函数必须测试失败路径
3. 任何多步操作必须测试部分失败回滚

---

## 5. CLAUDE.md 规则的架构体现

### 违规: 未经请求的降级分支

```cpp
// XQMainWindow.cpp
if (!stats.ok) {
    lazy = resolveLazyGeometrySource(..., SurfaceOnly);
    if (lazy.valid()) { 
        stats = renderer.addSurfaceProgressive(...); 
    }
}
```

**问题**: 体网格失败自动降级为表面，掩盖真实错误。

### 架构原则

> UI 层不应该"智能恢复"，应该忠实反映底层状态。

**对比**:

| 层级 | 职责 | 降级策略 |
|------|------|---------|
| UI | 展示状态，收集意图 | 显示错误，不降级 |
| Service | 领域逻辑 | 明确的降级参数（用户选择） |
| IO | 数据读写 | 版本兼容性降级（文档化） |
| Core | 不变量维护 | 无降级（违反不变量即失败） |

**推荐**:
```cpp
if (lazy.valid() && lazy->meta().tetCount > 0) {
    stats = renderer.addVolumeMeshProgressive(lazy.source());
    if (!stats.ok) {
        showError("Failed to render volume mesh: " + stats.message);
        return;
    }
}
// 不自动降级
```

---

## 总结

| 问题 | 严重性 | 短期方案 | 长期方案 |
|------|--------|---------|---------|
| Command 原子性 | Blocker | 修复 redo() bug | RAII Guard |
| 缓存类型安全 | Blocker | Enum 标签 | 分离缓存 |
| 路径校验分层 | Critical | 符号链接防御 | ConfinedPath |
| 测试覆盖 | High | 补充单测 | 强制规则 |
| 未请求降级 | Medium | 移除 fallback | UI 层原则 |

**本次任务聚焦短期方案（修复 Blocker + Critical），长期方案留待后续重构。**
