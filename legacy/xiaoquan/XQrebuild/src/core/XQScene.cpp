#include "core/XQScene.h"

namespace xq {

XQScene::InsertResult XQScene::insert(XQDataNode node)
{
    const NodeId id = node.id();
    const std::pair<std::map<NodeId, XQDataNode>::iterator, bool> inserted =
        nodes_.emplace(id, node);

    if (!inserted.second) {
        return InsertResult::DuplicateNodeId;
    }

    return InsertResult::Inserted;
}

XQDataNode* XQScene::find(const NodeId& id)
{
    std::map<NodeId, XQDataNode>::iterator it = nodes_.find(id);
    if (it == nodes_.end()) {
        return nullptr;
    }

    return &it->second;
}

const XQDataNode* XQScene::find(const NodeId& id) const
{
    std::map<NodeId, XQDataNode>::const_iterator it = nodes_.find(id);
    if (it == nodes_.end()) {
        return nullptr;
    }

    return &it->second;
}

XQScene::RemoveResult XQScene::remove(const NodeId& id)
{
    if (nodes_.erase(id) > 0) {
        derived_by_source_.erase(id);
        for (std::map<NodeId, std::set<NodeId>>::iterator it = derived_by_source_.begin();
             it != derived_by_source_.end();
             ++it) {
            it->second.erase(id);
        }
        stale_.erase(id);

        return RemoveResult::Removed;
    }

    return RemoveResult::NotFound;
}

XQScene::RelationResult XQScene::link_derived(const NodeId& source, const NodeId& derived)
{
    if (source == derived) {
        return RelationResult::SelfRelation;
    }
    if (nodes_.find(source) == nodes_.end()) {
        return RelationResult::SourceMissing;
    }
    if (nodes_.find(derived) == nodes_.end()) {
        return RelationResult::DerivedMissing;
    }

    std::set<NodeId>& derived_nodes = derived_by_source_[source];
    const std::pair<std::set<NodeId>::iterator, bool> inserted = derived_nodes.insert(derived);
    if (!inserted.second) {
        return RelationResult::DuplicateRelation;
    }

    return RelationResult::Linked;
}

std::size_t XQScene::mark_source_changed(const NodeId& source)
{
    if (nodes_.find(source) == nodes_.end()) {
        return 0;
    }

    std::map<NodeId, std::set<NodeId>>::const_iterator it = derived_by_source_.find(source);
    if (it == derived_by_source_.end()) {
        return 0;
    }

    std::size_t newly_stale_count = 0;
    for (std::set<NodeId>::const_iterator derived_it = it->second.begin();
         derived_it != it->second.end();
         ++derived_it) {
        if (!is_stale(*derived_it)) {
            ++newly_stale_count;
        }
        stale_[*derived_it] = StaleReason::SourceChanged;
    }

    return newly_stale_count;
}

bool XQScene::is_stale(const NodeId& id) const
{
    std::map<NodeId, StaleReason>::const_iterator it = stale_.find(id);
    return it != stale_.end() && it->second != StaleReason::None;
}

XQScene::StaleReason XQScene::stale_reason(const NodeId& id) const
{
    std::map<NodeId, StaleReason>::const_iterator it = stale_.find(id);
    if (it == stale_.end()) {
        return StaleReason::None;
    }

    return it->second;
}

void XQScene::clear()
{
    nodes_.clear();
    derived_by_source_.clear();
    stale_.clear();
}

} // namespace xq
