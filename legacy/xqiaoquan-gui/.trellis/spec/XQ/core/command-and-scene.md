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
- Controllers must return a non-Ok status when `stack_->push(...)` returns `false`.
- Commands with multi-step mutations must be atomic: either all scene changes succeed, or previously applied steps are rolled back before returning `false`.

### 4. Validation & Error Matrix
- duplicate node id in `AddNodeCommand` -> `execute() == false`; original node remains; no undo entry.
- missing source or failed relation in `AddNodeWithSourceRelationCommand` -> rollback inserted node; no relation; no undo entry.
- missing target in `ReplacePayloadCommand` or `RemoveNodeCommand` -> `execute() == false`; no undo entry.
- redo blocked by current scene state -> `redo() == false`; command must not re-enter undo; scene must not corrupt unrelated nodes.

### 5. Good/Base/Bad Cases
- Good: command inserts a new node, returns `true`, undo removes only that inserted node.
- Base: duplicate insert returns `false`, `undo_count()` is unchanged, and calling `undo()` cannot delete the pre-existing node.
- Bad: command ignores `XQScene::insert()` / `link_derived()` results and is pushed onto undo after a failed mutation.

### 6. Tests Required
- Unit test duplicate `AddNodeCommand` and assert push failure, unchanged undo count, and original node preservation.
- Unit test `AddNodeWithSourceRelationCommand` missing source and relation failure rollback.
- Unit test missing target `ReplacePayloadCommand` / `RemoveNodeCommand` do not enter undo.
- Controller test where service returns a command but scene rejects it; assert non-Ok status and unchanged scene/undo counts.
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

## Scenario: Prepared work is scoped to one project lifecycle

### 1. Scope / Trigger
- Trigger: any controller that captures Scene/Registry state before background
  work, or any change to `XQProject` lifecycle and copy/move assignment.
- Purpose: node ids, revisions, payload handles, and stale flags can all repeat
  after close/reopen or project replacement. They are insufficient to identify
  the project lifetime that originated prepared work.

### 2. Signatures
- `std::uint64_t XQProject::lifecycleEpoch() const`
- `XQProject::open()`, `close()`, and `reopen()`
- `XQProject& operator=(const XQProject&)`
- `XQProject& operator=(XQProject&&)`
- Prepared controller guard: `{ const XQProject* project; uint64_t lifecycleEpoch; }`

### 3. Contracts
- A newly constructed project starts at epoch `0`. Every successful `open`,
  `close`, or `reopen` increments the epoch exactly once. Invalid lifecycle
  transitions do not change it.
- Copy/move assignment replaces Scene, Registry, state, and other project data,
  but must not copy the source object's epoch over the target. It preserves the
  target epoch and advances it once after replacement. Self-assignment is a
  no-op and preserves the epoch.
- The epoch is runtime-only freshness identity. It is not serialized and must
  not be reconstructed from project schema, node ids, revisions, or assets.
- Owner-thread capture records both the exact originating `XQProject*` and its
  epoch. Commit through another project object, or through the same object after
  any successful lifecycle transition/assignment, is rejected as source change.
- Pointer and epoch checks are required even when callers restore the exact same
  `shared_ptr` payload identities, node ids, revisions, stale state, and assets.
  Source-level checks still run for work from the same live epoch.

### 4. Validation & Error Matrix
- prepared work committed through a different project pointer -> reject with no
  command push or project mutation.
- close + reopen followed by exact source restoration -> epoch mismatch; reject.
- copy/move assignment of equivalent project contents -> target epoch advances;
  previously prepared work rejects.
- invalid `open`/`close`/`reopen` or self-assignment -> epoch unchanged.
- ordinary source edit without project transition -> pointer/epoch still match;
  revision/payload/stale/asset guard rejects it.

### 5. Good/Base/Bad Cases
- Good: a worker finishes after the project was closed and reopened; commit
  returns `SourceChanged` even if fixtures recreated identical source nodes.
- Base: capture and commit on the same open project epoch succeeds when every
  source and target guard remains current.
- Bad: assignment copies `other.lifecycleEpoch_`, allowing the target epoch to
  move backward to the value stored in an old prepared result.

### 6. Tests Required
- `test_project_lifecycle`: initial/successful/invalid transitions plus copy,
  move, and self-assignment epoch behavior; use Release-active checks, not
  side-effecting `assert` expressions.
- Controller tests: cross-project rejection, close/reopen ABA with restored
  source identities, and same-object assignment ABA; assert no output node and
  no undo entry.
- `test_app_startup`: no-argument and loaded-project assignment paths use the
  current `XQProject` object safely.
- Final gate: full Release `ctest` after changing `XQProject` layout.

### 7. Wrong vs Correct
#### Wrong
```cpp
prepared.revision = node.contentRevision();
if (live.contentRevision() == prepared.revision) commit();
```

#### Correct
```cpp
prepared.project = project;
prepared.epoch = project->lifecycleEpoch();
// commit: project pointer + epoch first, then source/asset freshness guards
```

## Scenario: Semantic revision versus payload materialization

### 1. Scope / Trigger
- Trigger: editing an existing node payload (including contour append),
  restoring persisted payloads, lazily parsing a source-backed payload, or
  committing a background parse result.
- Purpose: `contentRevision` identifies semantic content, while loading or
  materializing the same content is rebuildable residency state. Mixing the two
  produces false stale propagation or lets an old worker overwrite a newer edit.

### 2. Signatures
- `using ContentRevision = unsigned long long`
- `XQScene::StaleSnapshot XQScene::stale_snapshot() const`
- `bool XQScene::restore_stale_snapshot(const StaleSnapshot& snapshot)`
- `SemanticReplacePayloadCommand(XQScene*, NodeId, XQDomainType,
  std::shared_ptr<XQPayload>, std::string)`
- `ReplacePayloadCommand(...)` and `XQDataNode::setPayload(...)` remain the
  explicit non-semantic primitives.
- MainWindow contour tail: `appendContourToGroup(NodeId, XQContour)` and shared
  Scene/contour id allocator `allocateSceneObjectId()`.

### 3. Contracts
- A successful semantic edit increments the target revision exactly once and
  calls transitive `mark_source_changed(target)`; the target itself is not stale.
- The command snapshots old payload/domain/revision and the complete stale map
  before mutation. Undo restores all four; redo restores the original post-edit
  revision and stale snapshot without incrementing again.
- A non-null old or new payload must be cloneable before the first mutation.
  Revision overflow, missing target, domain mismatch, or clone failure returns
  `false` with no scene or undo-stack side effect.
- Reader reconstruction, lazy source parsing, voxel/geometry residency changes,
  and ordinary `ReplacePayloadCommand` do not change revision or stale state.
- Appending a contour copies `XQContourGroup`, adds the contour to the copy, and
  pushes one `SemanticReplacePayloadCommand`. It never mutates the payload held
  by the live Scene node in place.
- Workbench-created Scene nodes and nested `ContourId` values share one
  uniqueness namespace. Automatic allocation scans every Scene node and every
  contour in every ContourGroup and returns one past the maximum; overflow
  returns invalid. Explicit contour ids colliding with either namespace reject.
- MainWindow contour-group creation also uses the command stack. It submits one
  `AddNodeWithSourceRelationCommand`, preserves the payload's Path binding as a
  `Path -> ContourGroup` Scene relation, and copies an explicit source ScaleSlot.
  A successful command clears redo, so an id released by contour undo cannot be
  reused for a node and then reintroduced as a nested contour by stale redo.
- A background source parse captures node id, domain, revision, original source
  payload identity, and source path. The GUI-thread commit rechecks every field;
  a mismatch discards the old result silently.
- `restore_stale_snapshot` validates every entry before replacing the map.
  Missing nodes or `StaleReason::None` reject the whole snapshot; an empty map
  clears stale state.

### 4. Validation & Error Matrix
- target missing -> semantic execute fails; no undo entry.
- revision at maximum -> fails before payload mutation.
- non-null payload whose `clone()` returns null -> fails before revision/stale
  mutation.
- stale snapshot references a missing node or `None` reason -> restore returns
  false and the previous stale map is unchanged.
- worker result sees changed revision, payload pointer, domain, or source path ->
  discard result; do not create a command or diagnostic that implies parse error.
- unchanged source state -> materialize with direct `setPayload`; revision, stale,
  and undo depth remain unchanged.
- automatic id overflow or explicit node/nested-contour collision -> contour
  append fails before command push; payload/revision/stale/undo remain unchanged.
- contour undo followed by another successful workbench mutation -> redo is
  cleared before a released id can be restored into a conflicting namespace.

### 5. Good/Base/Bad Cases
- Good: edit Path revision 41 to 42, stale Profile and FlowResult, undo to the
  exact revision-41 payload/stale map, then redo to the exact revision-42 state.
- Good: append contour id 400 through a semantic command; undo restores the
  prior group/revision/stale map, and a later node-creation command clears that
  contour redo before it may reuse id 400.
- Base: project reader replaces `XQSourcePayload` with a typed payload while
  preserving the persisted revision and stale map.
- Bad: a `.ctgr` worker checks only `dynamic_cast<XQSourcePayload*>` and overwrites
  a newer same-domain source path created while the worker was running.

### 6. Tests Required
- `test_command_stack`: semantic revision +1, transitive stale, unrelated prior
  stale preservation, exact undo/redo, overflow/missing/clone-failure rejection,
  and ordinary replace redo invariants.
- `test_scene_relations`: full snapshot restore, empty clear, and invalid snapshot
  atomic rejection.
- Path and boundary-condition service tests: commands use semantic replacement
  and restore full payload/revision/stale state through a real command stack.
- `test_main_window`: start a source parse, perform a same-domain semantic source
  edit before commit, assert the worker result is discarded, then assert normal
  materialization changes neither revision, stale, nor undo depth.
- `test_main_window`: contour copy-on-write append, global node/contour id
  uniqueness, exact undo/redo, prepared Profile invalidation, and
  undo-contour -> create-group -> cleared-redo collision regression.
- Project round-trip/versioned-save tests: persisted stale state is restored
  directly after all relations, not reconstructed approximately by replay.

### 7. Wrong vs Correct
#### Wrong
```cpp
if (std::dynamic_pointer_cast<XQSourcePayload>(node->payload())) {
    node->setPayload(domain, parsedPayload); // old worker can win
}
```

#### Correct
```cpp
if (node->domainType() == expectedDomain
    && node->contentRevision() == expectedRevision
    && node->payload().get() == expectedSourcePayload.get()
    && currentSourcePath(node) == expectedSourcePath) {
    node->setPayload(expectedDomain, parsedPayload); // materialization only
}
```

Contour edits follow the same rule:

#### Wrong
```cpp
payload->group().addContour(contour); // no revision, stale, undo, or redo guard
scene.insert(newGroup);               // bypasses command-stack redo clearing
```

#### Correct
```cpp
auto next = payload->group();
next.addContour(contourWithGloballyUniqueId);
session.pushCommand(std::make_unique<SemanticReplacePayloadCommand>(
    scene, groupId, XQDomainType::ContourGroup,
    std::make_shared<XQContourGroupPayload>(std::move(next)), "Add contour"));
```

## Scenario: Atomic multi-source project node commit

### 1. Scope / Trigger
- Trigger: adding one authoritative derived node whose commit also includes an
  optional AssetRecord, node-to-asset binding, multiple Scene parents, and Asset
  lineage (for example `Path + Contour -> VesselProfile`).
- Purpose: services may compute in the background, but no caller may publish a
  half node, half asset, or provenance graph assembled through unrelated commands.

### 2. Signatures
- `ProjectNodeBatchSpec { node, assetToRegister, bindAsset, sources,
  additionalAssetSources }`
- `sources` is `std::vector<DerivationInputStamp>`; it is both the Scene-parent
  list and the expected input freshness identity.
- `ProjectNodeBatchCommand(XQProject*, ProjectNodeBatchSpec, std::string)`
- `bool assetKindForDomain(XQDomainType, AssetKind*)`
- `bool assetKindMatchesDomain(AssetKind, XQDomainType)`

### 3. Contracts
- The project is open; the prepared node has a valid unused id, no asset binding,
  a typed cloneable payload, and matching domain token.
- Every source is unique, exists, is fresh, and matches captured revision. If it
  is asset-backed, the stamp must exactly match the live AssetId and fingerprint,
  and the source domain must match its AssetKind.
- VesselProfile command sources equal exactly its declared Path plus evidence
  nodes and exactly its persisted `DerivationStamp.inputs`; no undeclared Scene
  parent is permitted.
- Imported-gold external id/fingerprint is mandatory in the Profile contract.
  A corresponding gold Asset is optional; when supplied, exactly one registered
  matching-fingerprint Asset becomes an asset-only lineage parent.
- Domain-to-AssetKind interpretation comes from the shared core mapping used by
  writer, reader, and command; local copies of the switch are forbidden.
- Commit order is asset registration, node insertion, binding, all Scene
  relations, then newly needed Asset relations. Any failure rolls back applied
  steps in reverse order and returns false, so the command is not pushed.
- Undo removes only relations created by this command, then the target node and
  only the Asset registered by this command. Pre-existing lineage and existing
  bound Assets are preserved. Issued automatic AssetIds are never rewound.

### 4. Validation & Error Matrix
- closed/null project, invalid/duplicate target id, pre-bound node, null or
  domain-mismatched payload -> reject before mutation.
- duplicate/missing/self/stale source or revision mismatch -> reject.
- bound source omits AssetId/fingerprint, binding changed, fingerprint changed,
  or source AssetKind disagrees with domain -> reject.
- new record id differs from `bindAsset`, target AssetKind is wrong, or existing
  target Asset is missing -> reject.
- VesselProfile validator failure, payload/command stamp divergence, extra Scene
  parent, or undeclared asset-only parent -> reject.
- redo target id/asset conflict -> fail atomically and remain on redo; after the
  blocker is removed, the same redo must succeed exactly once.

### 5. Good/Base/Bad Cases
- Good: one command publishes Profile node + Profile Asset + binding + two Scene
  parents + Path/Contour Asset lineage, and undo removes only that child state.
- Base: a scene-only derived node has no Asset registration/binding and still
  commits/undoes all Scene parents atomically.
- Bad: controller pushes AddNode, then separately adds the second parent and
  Asset lineage; failure after the first command leaves a believable but false
  single-parent project.

### 6. Tests Required
- success with two Scene parents and all Asset parents; exact undo/redo and same
  explicit target AssetId.
- bind an existing Asset and prove undo preserves pre-existing Asset lineage.
- scene-only success path with the registry unchanged.
- table-driven zero-side-effect failures for invalid profile, stamp/source
  divergence, missing/stale/revision/fingerprint/AssetKind errors, duplicate ids,
  invalid binding, and undeclared parents.
- failed push preserves an existing redo entry; failed redo is atomic, remains
  retryable, and succeeds after its blocker is removed.
- unregister/undo keeps AssetId allocation monotonic and removes only incident
  relations owned by the removed Asset.

### 7. Wrong vs Correct
#### Wrong
```cpp
stack.push(AddNodeCommand(...));
scene.link_derived(pathId, profileId);
scene.link_derived(contourId, profileId); // partial graph if this fails
registry.addRelation(pathAsset, profileAsset);
```

#### Correct
```cpp
ProjectNodeBatchSpec spec(preparedProfileNode);
spec.assetToRegister = preparedProfileAsset;
spec.bindAsset = preparedProfileAsset.id;
spec.sources = profile.derivationStamp.inputs;
stack.push(std::make_unique<ProjectNodeBatchCommand>(&project, std::move(spec)));
```
