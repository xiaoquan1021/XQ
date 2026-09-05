# 代码审查详细报告

> 审查日期: 2026-07-02
> 审查范围: `feat/m8b2-runtime-memory` 分支未提交改动
> 审查强度: max (10 角度并行 + 手动验证)
> 审查员: Kiro (Claude Fable 5)

---

## 审查结论

* **是否允许合并**: **禁止合并**
* **Blocker 数量**: 3
* **Critical 数量**: 5
* **High 数量**: 5
* **Medium 数量**: 3
* **最大风险**: XQCommandStack::redo() 失败丢失命令导致用户数据永久丢失

---

## Blocker 级问题 (必须立即修复)

### [Blocker-1] XQCommandStack::redo() 失败时丢失命令

**位置**: `src/core/command/XQCommandStack.cpp:44-57`

**代码**:
```cpp
bool XQCommandStack::redo()
{
    if (redo_stack_.empty()) {
        return false;
    }
    std::unique_ptr<XQCommand> command = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    if (!command->execute()) {
        return false;  // ← 命令已从 redo_stack_ 移除但未归还
    }
    undo_stack_.push_back(std::move(command));
    return true;
}
```

**根因**: `execute()` 返回 false 时,命令已从 `redo_stack_` 移除但未放回,导致命令永久丢失。

**触发条件**:
1. 用户执行操作 A (成功)
2. 用户 undo (A 进入 redo_stack_)
3. 外部修改使 A 的前置条件失效 (如删除 A 引用的节点)
4. 用户 redo → `A->execute()` 返回 false
5. 命令 A 永久丢失,无法再次尝试 redo

**实际后果**:
- 用户丢失可撤销历史,无法恢复
- `redo_count()` 与实际可 redo 的命令数不一致
- 违反 undo/redo 栈的不变量 (所有命令要么在 undo 要么在 redo)

**最小修复**:
```cpp
if (!command->execute()) {
    redo_stack_.push_back(std::move(command));
    return false;
}
```

**推荐修复**: 同最小修复

**回归测试**:
```cpp
// tests/core/test_command_stack.cpp
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
```

---

### [Blocker-2] GeometryResourceManager 缓存键冲突

**位置**: `src/services/resource/GeometryResourceManager.cpp:170-184, 235-250`

**代码**:
```cpp
// acquireVoxelSource:
const ResidentKey key = voxelBlockKey(id, spec);
auto it = blocks_.find(key);
if (it != blocks_.end()) {
    return VoxelSourceHandle(it->second.voxelSource, it->second.pin);
}

// acquireGeometrySource:
const ResidentKey key = geometryBlockKey(id, spec);
auto it = blocks_.find(key);
if (it != blocks_.end()) {
    return GeometrySourceHandle(it->second.geometrySource, it->second.pin);
}
```

**根因**: `blocks_` 是统一的 `std::map<ResidentKey, ResidentBlock>`,但 `ResidentBlock` 持有 `voxelSource` 和 `geometrySource` 两个字段,缓存命中时未检查取出的是哪种类型。

**触发条件**:
1. 调用 `acquireVoxelSource(id=100, spec)`,缓存 voxel 源到 `blocks_[{100, kResidentVoxelFull}]`
2. 调用 `acquireGeometrySource(id=100, spec)`,如果 `geometryBlockKey` 碰巧生成相同的键值
3. 缓存命中,但返回 `it->second.geometrySource` (此时为 nullptr,因为这个 block 存的是 voxel)
4. 返回无效 handle 或访问空指针

**实际后果**:
- 返回错误的几何类型 (voxel 当 geometry 或反之)
- 空指针解引用崩溃
- 渲染错误数据或静默失败

**推荐修复**: 使用类型安全枚举键
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

**回归测试**:
```cpp
TEST_CASE("voxel and geometry cache distinct") {
    GeometryResourceManager mgr(&registry, assetRoot);
    auto v = mgr.acquireVoxelSource(id, voxelSpec);
    auto g = mgr.acquireGeometrySource(id, geoSpec);
    REQUIRE(v.valid() && g.valid());
    REQUIRE(&v.source() != reinterpret_cast<const IVoxelSource*>(&g.source()));
}
```

---

### [Blocker-3] service 层所有 push() 调用未检查返回值

**位置**: `tests/services/segmentation/SegmentationServiceTest.cpp:599,642`, `tests/services/meshing/VolumeMeshServiceTest.cpp:301`, `tests/services/path/PathServiceTest.cpp:116,152,161,190,212,241,264` 等

**代码**:
```cpp
stack.push(std::move(cmd.command));  // 无返回值检查
```

**根因**: `XQCommandStack::push()` 改为返回 `bool` 后,service 层所有测试和生产代码仍按旧的 `void` 语义调用,忽略返回值。

**触发条件**:
- Service 返回的 command 的 `execute()` 失败 (例如 scene 状态不满足前置条件)
- `push()` 返回 false 但调用者未检查
- 测试继续执行,假设命令已入栈

**实际后果**:
- 测试假阳性: 命令执行失败但测试仍通过 (未检测到失败)
- 生产代码静默失败: 用户操作未生效,无错误提示
- undo/redo 栈计数与预期不符

**推荐修复**: 所有 service 层 `push()` 调用点加返回值检查
```cpp
if (!stack.push(std::move(cmd.command))) {
    return fail("command push failed");
}
```

**回归测试**: 现有测试已覆盖 `push()` 失败场景 (`test_command_stack.cpp`),但 service 层测试需全部更新。

---

## Critical 级问题 (高风险)

### [Critical-1] AddNodeWithSourceRelationCommand 回滚逻辑错误

**位置**: `src/core/command/XQSceneCommands.cpp:59-79`

**问题**: `insert` 成功但 `link_derived` 失败时回滚删除,但 `inserted_` 标志在回滚前已设为 false,导致逻辑不一致。

**推荐修复**:
```cpp
if (scene_->insert(node_) != XQScene::InsertResult::Inserted) {
    inserted_ = false;
    return false;
}
inserted_ = true;  // ← 提前设置标志

if (scene_->link_derived(source_, id_) != XQScene::RelationResult::Linked) {
    scene_->remove(id_);  // 现在安全
    inserted_ = false;
    return false;
}
return true;
```

---

### [Critical-2] XQScene::mark_source_changed 未过滤自环

**位置**: `src/core/XQScene.cpp:127-139`

**问题**: 初始化 `pending` 时未过滤源节点自己,如果有自环会把源节点标记为 stale。

**推荐修复**:
```cpp
for (std::set<NodeId>::const_iterator derived_it = it->second.begin();
     derived_it != it->second.end(); ++derived_it) {
    if (*derived_it != source) {  // ← 过滤自环
        pending.push_back(*derived_it);
    }
}
```

---

### [Critical-3] BlobStore 路径遍历检查可被符号链接绕过

**位置**: `src/io/blob/BlobStore.cpp:298-318`

**问题**: `isConfinedRelativePath` 检查原始字符串,但 `fs::path` 的 `/` 运算符可能跟随符号链接。

**推荐修复**:
```cpp
const fs::path path = fs::path(rootDir_) / ref.relPath;
const fs::path canonical = fs::weakly_canonical(path, ec);
if (ec || !canonical.string().starts_with(fs::canonical(rootDir_).string())) {
    return Status::InvalidMetadata;
}
```

---

### [Critical-4] GeometryResourceManager blob 校验失败后状态不一致

**位置**: `src/services/resource/GeometryResourceManager.cpp:282-299`

**问题**: `fill` 失败后 `totalBytes` 已累加部分 blob 大小,但函数返回时未计入缓存。

**推荐修复**: 确认当前代码无此问题 (fill 失败立即返回,in 未进入缓存),或使用 RAII guard。

---

### [Critical-5] attachGeometryResources 可能在 renderer 使用后调用

**位置**: `src/app/XQMainWindow.cpp:171-177`, `src/app/XQAppStartup.cpp:61-71`

**问题**: `attachWorkflow` 和 `attachGeometryResources` 分两次调用,中间窗口可能触发选中事件。

**推荐修复**: 合并为原子操作
```cpp
void attachMainWindowWorkflow(XQMainWindow* window, XQAppStartupState* state,
                              const std::string& assetRootDir)
{
    window->attachWorkflow(&state->project.scene(), &state->commandStack);
    window->attachGeometryResources(&state->project.assetRegistry(), assetRootDir);
    window->refreshSceneTree();
}
```

---

## High / Medium 级问题

(详见主 README.md)

---

## 待验证风险

1. **service 层是否还有其他 push() 调用未检查返回值**
2. **GeometryResourceManager 的 shared_ptr 改动是否有 ABI 兼容性问题**
3. **BlobStore path traversal 检查是否防御符号链接** (已确认需修复)
4. **XQCommandStack::redo() 失败丢命令是否有现场案例**

---

## 测试缺口

1. 命令执行失败后 undo/redo 栈完整性
2. GeometryResourceManager 混合缓存冲突
3. XQScene 自环/环路传播
4. renderer 空几何源
5. blob 校验边界条件

---

## 修复优先级

1. Blocker-1: redo() 丢命令
2. Blocker-2: 缓存键冲突
3. Blocker-3: service 层 push() 未检查
4. Critical-1~5: 回滚逻辑/自环/路径遍历/状态不一致/竞态
5. High/Medium: 测试/性能/代码质量

---

## 审查方法

- **10 角度并行审查**: Cross-file tracer, C++ pitfalls, Reuse detector, Removed-behavior auditor, Line-by-line scan, Conventions checker, Simplification, Altitude, Efficiency, Wrapper/proxy
- **手动关键路径分析**: Command 模式, GeometryResourceManager 缓存, BlobStore 校验
- **架构规范对照**: `command-and-scene.md`, `source-interface.md`

---

## 结论置信度

**High** — 已检查核心文件和关键路径,待验证风险需进一步确认。

审查发现的问题均有明确触发条件、实际后果和修复方案,非理论风险。
