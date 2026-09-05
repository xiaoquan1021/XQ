# 所有权 · 命令/undo · 领域关系

> 来源:`plan/00-architecture.md`、`plan/01-core-and-io.md`。core 的核心契约,所有服务都依赖它。

---

## 所有权

- `XQProject` 拥有项目生命周期、根标识、诊断信息、修改状态,以及唯一一个权威的 `XQScene`。
- `XQScene` 拥有所有领域节点及其关系。UI 模型、viewer、reader、算法只**观察或变换** scene 数据,
  **不持有平行的业务对象图**。
- `XQDataNode` 拥有稳定节点标识、显示元数据、领域类型、来源(provenance)、诊断,以及一个 XQ 自有 payload。

## 命令与 undo(强约束)

- 所有对 scene 的变更**经 `XQCommandStack` 提交**。
- service 计算"该怎么改",返回 XQ 自有的 command;command 在 execute 时改 scene,undo 时复原。
- service **不直接改 `XQScene`**;UI 只收集意图、提交 command。
- 预置通用命令:`AddNodeCommand`、`AddNodeWithSourceRelationCommand`、`ReplacePayloadCommand`、`RemoveNodeCommand`。
- `XQCommandStack` 支持 push+execute、undo、redo、清空。
- 编辑类命令复制旧 payload,保留原节点 id 与来源关系(如 path 编辑保留 path id 与 sourceImageNode)。

## payload 机制

- `XQDataNode` 持有一个 `std::shared_ptr<XQPayload>`。
- `XQDomainType` 枚举:Image / Path / ContourGroup / SegmentationMask / SurfaceModel / Mesh / SimulationCase。
- `XQScene` 分组:Images / Paths / Segmentations / Models / Meshes / Simulations;`groupForDomain(XQDomainType)` 做映射。

## 领域关系

- **所有权关系**:一个节点属于且仅属于一个 `XQScene` 分组。
- **来源关系(source)**:节点由另一节点加载或派生而来。
- **派生关系(derived)**:算法输出依赖一个或多个来源节点,可能变陈旧(stale)。
- **stale 传播**:来源节点变更后,沿 derived 关系向下游传播 stale 标记。

主线本身是一条 source/derived 链:

```
image --源--> path --源--> contour group --派生--> surface model
      --派生--> segmentation mask --派生--> surface model
surface model --派生--> mesh --派生--> simulation case / flow result --派生--> ai analysis
```

## 标识与来源

- 节点 ID 在 save/reopen 之间必须**稳定**(除非迁移策略另有规定)。
- payload 版本与 provenance 必须随项目保存,保证可复现。
- copy/move/share 语义需要显式的所有权与 ID 规则。

## 线程与错误

- core 对象默认**非线程安全**(除非契约明确)。
- 公开 API 返回显式的 result / diagnostic,**而不是穿过 UI 代码路径抛异常**。
- 诊断信息**不得包含**私有患者/机构字段。

## Scenario: Command success gates undo stack

### 1. Scope / Trigger
- Trigger: any change that adds or modifies an `XQCommand` subclass, a controller that submits commands, or command stack undo/redo behavior.
- Purpose: failed or no-op commands must not be recorded as reversible scene mutations.

### 2. Signatures
- `class XQCommand { virtual bool execute() = 0; virtual void undo() = 0; }`
- `bool XQCommandStack::push(std::unique_ptr<XQCommand> command)`
- `bool XQCommandStack::redo()`

### 3. Contracts
- `execute() == true`: the command completed one reversible scene mutation and may enter the undo stack.
- `execute() == false`: the command did not complete a reversible mutation; it must not enter undo, and `push()` must not clear redo.
- `push(nullptr)` returns `false` and leaves scene, undo stack, and redo stack unchanged.
- `redo()` whose `execute() == false` must return the command to the top of the redo stack (not drop it): a transiently-failed redo stays retryable, `redo_count()` is unchanged, and nothing enters the undo stack.
- Controllers must return a non-Ok status when `stack_->push(...)` returns `false`.
- Commands with multi-step mutations must be atomic: either all scene changes succeed, or previously applied steps are rolled back before returning `false`.

### 4. Validation & Error Matrix
- duplicate node id in `AddNodeCommand` -> `execute() == false`; original node remains; no undo entry.
- missing source or failed relation in `AddNodeWithSourceRelationCommand` -> rollback inserted node; no relation; no undo entry.
- missing target in `ReplacePayloadCommand` or `RemoveNodeCommand` -> `execute() == false`; no undo entry.
- redo blocked by current scene state -> `redo() == false`; command must not re-enter undo; it stays on the redo stack for a later retry; scene must not corrupt unrelated nodes.

### 5. Good/Base/Bad Cases
- Good: command inserts a new node, returns `true`, undo removes only that inserted node.
- Base: duplicate insert returns `false`, `undo_count()` is unchanged, and calling `undo()` cannot delete the pre-existing node.
- Bad: command ignores `XQScene::insert()` / `link_derived()` results and is pushed onto undo after a failed mutation.

### 6. Tests Required
- Unit test duplicate `AddNodeCommand` and assert push failure, unchanged undo count, and original node preservation.
- Unit test `AddNodeWithSourceRelationCommand` missing source and relation failure rollback.
- Unit test missing target `ReplacePayloadCommand` / `RemoveNodeCommand` do not enter undo.
- Controller test where service returns a command but scene rejects it; assert non-Ok status and unchanged scene/undo counts.
- Unit test a `redo()` whose `execute()` fails transiently: assert `redo()` returns `false`, the command stays on the redo stack (`redo_count()` unchanged, nothing on undo), and a later `redo()` retries and succeeds.
- Run at least `test_command_stack`, `test_workflow_controllers`, and a full Release `ctest` after changing `XQCommand` signatures.

### 7. Wrong vs Correct

#### Wrong
```cpp
void AddNodeCommand::execute()
{
    scene_->insert(node_);
}

stack_->push(std::move(command));
return Status::Ok;
```

#### Correct
```cpp
bool AddNodeCommand::execute()
{
    inserted_ = scene_ != nullptr
        && scene_->insert(node_) == XQScene::InsertResult::Inserted;
    return inserted_;
}

return stack_->push(std::move(command)) ? Status::Ok : Status::Rejected;
```

## Scenario: Transitive stale propagation

### 1. Scope / Trigger
- Trigger: any code that changes `XQScene::mark_source_changed`,
  `link_derived`, scene relation restoration, project reader stale restoration,
  or UI/model code that displays stale state.
- Stale is a derived-graph property: changing one source invalidates every
  downstream node reachable through `derived` relations.

### 2. Signatures
- `std::size_t XQScene::mark_source_changed(const NodeId& source)`
- `bool XQScene::is_stale(const NodeId& id) const`
- `XQScene::StaleReason XQScene::stale_reason(const NodeId& id) const`

### 3. Contracts
- For `A -> B -> C`, `mark_source_changed(A)` marks `B` and `C` stale in one
  call.
- For fan-in such as `A -> B`, `A -> C`, `B -> D`, `C -> D`, `D` is marked and
  counted once.
- The return value is the number of nodes that became newly stale during this
  call; nodes already stale are not counted again.
- The changed source itself is not marked stale by its own change.
- Traversal must be iterative or otherwise cycle-safe; `link_derived` currently
  rejects self-relations but does not reject multi-node cycles.

### 4. Validation & Error Matrix
- Missing source node -> return `0`, no stale changes.
- Source with no derived descendants -> return `0`.
- Direct child already stale but grandchild fresh -> child remains stale,
  grandchild becomes stale, return counts only the grandchild.
- Multi-path descendant -> one stale entry and one count increment.
- Cycle reaching the original source -> traversal terminates and original source
  remains non-stale.

### 5. Good/Base/Bad Cases
- Good: editing a path stales contour groups, surfaces, meshes, simulation cases,
  flow results, and AI analyses downstream.
- Base: direct `source -> derived` behavior remains unchanged.
- Bad: caller must manually call `mark_source_changed()` at every chain level to
  get downstream stale state.

### 6. Tests Required
- `test_scene_relations`: chain, diamond/fan-in, repeated mark, and source
  non-stale assertions.
- `test_project_roundtrip`: one upstream call should persist all downstream
  stale nodes.
- `test_project_versioned_save`: legacy stale chains still restore correctly.
- `test_scene_model`: stale column still mirrors scene state.
- Final gate: Release full `ctest`.

### 7. Wrong vs Correct
#### Wrong
```cpp
for (NodeId child : derived_by_source_[source]) {
    stale_[child] = StaleReason::SourceChanged;
}
```

#### Correct
```cpp
while (!pending.empty()) {
    NodeId current = pending.back();
    pending.pop_back();
    if (!visited.insert(current).second) continue;
    if (current != source) stale_[current] = StaleReason::SourceChanged;
    enqueue_children(current);
}
```

## Scenario: Project close resets owned core state

### 1. Scope / Trigger
- Trigger: any change to `XQProject::open`, `XQProject::close`,
  `XQProject::reopen`, `AssetRegistry`, project startup reuse, or tests that
  reuse one `XQProject` instance across project lifecycles.
- Purpose: a closed project must not retain asset records or lineage from the
  previously open project after its scene has been cleared.

### 2. Signatures
- `XQProject::LifecycleResult XQProject::close()`
- `XQProject::LifecycleResult XQProject::reopen()`
- `void AssetRegistry::clear()`
- `std::size_t AssetRegistry::assetCount() const`
- `std::size_t AssetRegistry::relationCount() const`

### 3. Contracts
- `close()` is valid only from `LifecycleState::Open`; invalid transitions
  return `InvalidTransition` and leave scene, registry, and lifecycle state
  unchanged.
- Successful `close()` clears the authoritative `XQScene` and the
  project-owned `AssetRegistry` records and asset relations before entering
  `LifecycleState::Closed`.
- `reopen()` is valid only from `LifecycleState::Closed`; it exposes the clean
  state produced by `close()` and must not restore old scene nodes, asset
  records, or asset relations.
- `AssetRegistry::clear()` removes records and relations but keeps automatic
  id allocation monotonic for the registry lifetime; `createAsset()` after a
  clear must not reuse an id already issued by that registry.
- `close()` keeps the identity of the owned `XQScene` object stable; callers
  holding `&project.scene()` still point at the same scene object, now empty.

### 4. Validation & Error Matrix
- `close()` from `Created` -> `InvalidTransition`; no scene or registry
  mutation.
- `open()` on an already open project -> `InvalidTransition`; no cleanup side
  effects.
- `reopen()` from `Created` -> `InvalidTransition`; no scene or registry
  mutation.
- Open project with scene nodes, asset records, and asset relations ->
  successful `close()` leaves `scene.find(oldNode) == nullptr`,
  `assetCount() == 0`, `relationCount() == 0`, and `find(oldAsset) == nullptr`.
- `createAsset()` after `AssetRegistry::clear()` -> returns a fresh id greater
  than any automatic id already issued by that registry.

### 5. Good/Base/Bad Cases
- Good: close a project that has image/surface assets and lineage; reopen sees
  an empty scene and empty registry, and new asset ids do not collide with old
  ids.
- Base: close an empty open project; state becomes `Closed`, scene remains
  empty, registry remains empty.
- Bad: `close()` clears only `scene_`; a later reopen has no nodes but still
  writes old asset records or resolves stale asset ids from the previous
  project lifetime.

### 6. Tests Required
- `test_project_lifecycle`: populate scene nodes, bind them to asset ids, add
  asset lineage, close, and assert old nodes/assets/relations are gone.
- `test_project_lifecycle`: reopen the same project and assert the registry is
  still empty before new assets are created.
- `test_asset_registry`: call `clear()` on a populated registry and assert
  records and relations are removed while `createAsset()` remains monotonic.
- `test_project_roundtrip` and `test_project_lazy_geometry`: verify project
  save/load paths do not depend on closed projects retaining registry data.
- Final gate: full Release `ctest`.

### 7. Wrong vs Correct
#### Wrong
```cpp
XQProject::LifecycleResult XQProject::close()
{
    scene_.clear();
    state_ = LifecycleState::Closed;
    return LifecycleResult::Ok;
}
```

#### Correct
```cpp
XQProject::LifecycleResult XQProject::close()
{
    scene_.clear();
    assetRegistry_.clear();
    state_ = LifecycleState::Closed;
    return LifecycleResult::Ok;
}
```
