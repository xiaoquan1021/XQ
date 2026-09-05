#ifndef XQ_CORE_XQ_SCENE_H
#define XQ_CORE_XQ_SCENE_H

#include "core/XQDataNode.h"
#include "core/XQDomainType.h"

#include <cstddef>
#include <map>
#include <set>

namespace xq {

class XQScene {
public:
    // Project-tree grouping. Each node belongs to exactly one group, derived
    // from its domain type via groupForDomain(). Unknown/legacy domains map to
    // Ungrouped.
    enum class Group {
        Ungrouped,
        Images,
        Paths,
        Segmentations,
        Models,
        Meshes,
        Simulations
    };

    static Group groupForDomain(XQDomainType domain);

    enum class InsertResult {
        Inserted,
        DuplicateNodeId
    };

    InsertResult insert(XQDataNode node);

    XQDataNode* find(const NodeId& id);
    const XQDataNode* find(const NodeId& id) const;

    enum class RemoveResult {
        Removed,
        NotFound
    };

    RemoveResult remove(const NodeId& id);

    enum class RelationResult {
        Linked,
        SourceMissing,
        DerivedMissing,
        SelfRelation,
        DuplicateRelation
    };

    enum class StaleReason {
        None,
        SourceChanged
    };

    using StaleSnapshot = std::map<NodeId, StaleReason>;

    RelationResult link_derived(const NodeId& source, const NodeId& derived);
    std::size_t mark_source_changed(const NodeId& source);
    bool is_stale(const NodeId& id) const;
    StaleReason stale_reason(const NodeId& id) const;
    StaleSnapshot stale_snapshot() const;

    // Atomically replaces the complete stale set. Every entry must reference
    // an existing node and carry a non-None reason; invalid snapshots leave the
    // scene unchanged. An empty snapshot clears all stale state.
    bool restore_stale_snapshot(const StaleSnapshot& snapshot);

    template <typename Callback>
    void visit_nodes(Callback callback) const
    {
        for (std::map<NodeId, XQDataNode>::const_iterator it = nodes_.begin();
             it != nodes_.end();
             ++it) {
            callback(it->second);
        }
    }

    template <typename Callback>
    void visit_derived_relations(Callback callback) const
    {
        for (std::map<NodeId, std::set<NodeId>>::const_iterator source_it = derived_by_source_.begin();
             source_it != derived_by_source_.end();
             ++source_it) {
            for (std::set<NodeId>::const_iterator derived_it = source_it->second.begin();
                 derived_it != source_it->second.end();
                 ++derived_it) {
                callback(source_it->first, *derived_it);
            }
        }
    }

    template <typename Callback>
    void visit_stale_nodes(Callback callback) const
    {
        for (std::map<NodeId, StaleReason>::const_iterator it = stale_.begin();
             it != stale_.end();
             ++it) {
            callback(it->first, it->second);
        }
    }

    void clear();

private:
    std::map<NodeId, XQDataNode> nodes_;
    std::map<NodeId, std::set<NodeId>> derived_by_source_;
    std::map<NodeId, StaleReason> stale_;
};

} // namespace xq

#endif // XQ_CORE_XQ_SCENE_H
