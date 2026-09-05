# Design: transitive stale propagation

## Data Flow

`XQScene` stores a directed adjacency map:

```cpp
std::map<NodeId, std::set<NodeId>> derived_by_source_;
```

`mark_source_changed(source)` is the mutation point used by core, IO, UI model,
and services to mark derived data invalid when an upstream node changes.

## Algorithm

Use an iterative graph traversal from the changed source:

1. Return `0` if `source` is not in `nodes_`.
2. Seed a work queue with direct derived nodes of `source`.
3. Maintain `visited` to avoid revisiting nodes reached through fan-in or cycles.
4. For each visited node:
   - skip the initial `source` so the source itself is never stale;
   - if the node exists and is not already stale, increment the return count;
   - set stale reason to `SourceChanged`;
   - enqueue its direct derived nodes.

The traversal should not recurse; long scene chains should not consume call
stack depth.

## Compatibility

No public API changes. Return value keeps the existing meaning: number of nodes
that became newly stale during this call.

## Risks

- Cycles are currently possible because `link_derived()` rejects self-relations
  but does not reject multi-node cycles. The traversal must therefore use
  `visited` and must skip marking the initial source.
- Removed nodes are already pruned from relation sets by `remove()`. If a stale
  relation somehow references a missing node, traversal should not insert stale
  state for a node absent from `nodes_`.
