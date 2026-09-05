#include "core/XQScene.h"

#include <vector>

namespace xq {

XQScene::Group XQScene::groupForDomain(XQDomainType domain)
{
    switch (domain) {
    case XQDomainType::Image:
        return Group::Images;
    case XQDomainType::Path:
    case XQDomainType::VesselProfile:
        return Group::Paths;
    case XQDomainType::ContourGroup:
    case XQDomainType::SegmentationMask:
        return Group::Segmentations;
    case XQDomainType::SurfaceModel:
        return Group::Models;
    case XQDomainType::Mesh:
        return Group::Meshes;
    case XQDomainType::SimulationCase:
        return Group::Simulations;
    case XQDomainType::FlowResult:
        return Group::Simulations;
    case XQDomainType::AiAnalysis:
        // AI analyses (flow metrics / identification / surrogate prediction)
        // hang off the simulation chain alongside flow results.
        return Group::Simulations;
    case XQDomainType::Unknown:
        break;
    }
    return Group::Ungrouped;
}

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
    std::set<NodeId> visited;
    std::vector<NodeId> pending;
    pending.reserve(it->second.size());
    for (std::set<NodeId>::const_iterator derived_it = it->second.begin();
         derived_it != it->second.end();
         ++derived_it) {
        pending.push_back(*derived_it);
    }

    while (!pending.empty()) {
        const NodeId current = pending.back();
        pending.pop_back();
        if (!visited.insert(current).second) {
            continue;
        }

        if (current != source && nodes_.find(current) != nodes_.end()) {
            if (!is_stale(current)) {
                ++newly_stale_count;
            }
            stale_[current] = StaleReason::SourceChanged;
        }

        std::map<NodeId, std::set<NodeId>>::const_iterator children =
            derived_by_source_.find(current);
        if (children == derived_by_source_.end()) {
            continue;
        }
        for (std::set<NodeId>::const_iterator child_it = children->second.begin();
             child_it != children->second.end();
             ++child_it) {
            if (visited.find(*child_it) == visited.end()) {
                pending.push_back(*child_it);
            }
        }
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

XQScene::StaleSnapshot XQScene::stale_snapshot() const
{
    return stale_;
}

bool XQScene::restore_stale_snapshot(const StaleSnapshot& snapshot)
{
    for (StaleSnapshot::const_iterator it = snapshot.begin(); it != snapshot.end(); ++it) {
        if (it->second == StaleReason::None || nodes_.find(it->first) == nodes_.end()) {
            return false;
        }
    }
    stale_ = snapshot;
    return true;
}

void XQScene::clear()
{
    nodes_.clear();
    derived_by_source_.clear();
    stale_.clear();
}

} // namespace xq
