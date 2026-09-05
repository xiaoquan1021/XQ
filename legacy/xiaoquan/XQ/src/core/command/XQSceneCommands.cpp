#include "core/command/XQSceneCommands.h"

namespace xq {

// ---------------------------------------------------------------------------
// AddNodeCommand
// ---------------------------------------------------------------------------

AddNodeCommand::AddNodeCommand(XQScene* scene, XQDataNode node, std::string label)
    : scene_(scene)
    , node_(node)
    , id_(node.id())
    , inserted_(false)
    , label_(std::move(label))
{
}

bool AddNodeCommand::execute()
{
    if (scene_ == nullptr) {
        inserted_ = false;
        return false;
    }

    inserted_ = scene_->insert(node_) == XQScene::InsertResult::Inserted;
    return inserted_;
}

void AddNodeCommand::undo()
{
    if (scene_ != nullptr && inserted_) {
        scene_->remove(id_);
        inserted_ = false;
    }
}

std::string AddNodeCommand::label() const
{
    return label_;
}

// ---------------------------------------------------------------------------
// AddNodeWithSourceRelationCommand
// ---------------------------------------------------------------------------

AddNodeWithSourceRelationCommand::AddNodeWithSourceRelationCommand(XQScene* scene,
                                                                   XQDataNode node,
                                                                   const NodeId& source,
                                                                   std::string label)
    : scene_(scene)
    , node_(node)
    , id_(node.id())
    , source_(source)
    , inserted_(false)
    , label_(std::move(label))
{
}

bool AddNodeWithSourceRelationCommand::execute()
{
    if (scene_ == nullptr) {
        inserted_ = false;
        return false;
    }

    if (scene_->insert(node_) != XQScene::InsertResult::Inserted) {
        inserted_ = false;
        return false;
    }

    if (scene_->link_derived(source_, id_) != XQScene::RelationResult::Linked) {
        scene_->remove(id_);
        inserted_ = false;
        return false;
    }

    inserted_ = true;
    return true;
}

void AddNodeWithSourceRelationCommand::undo()
{
    if (scene_ != nullptr && inserted_) {
        // remove() drops the node and its incident relations.
        scene_->remove(id_);
        inserted_ = false;
    }
}

std::string AddNodeWithSourceRelationCommand::label() const
{
    return label_;
}

// ---------------------------------------------------------------------------
// ReplacePayloadCommand
// ---------------------------------------------------------------------------

ReplacePayloadCommand::ReplacePayloadCommand(XQScene* scene,
                                             const NodeId& id,
                                             XQDomainType domain,
                                             std::shared_ptr<XQPayload> payload,
                                             std::string label)
    : scene_(scene)
    , id_(id)
    , new_domain_(domain)
    , new_payload_(std::move(payload))
    , old_domain_(XQDomainType::Unknown)
    , old_payload_()
    , captured_(false)
    , label_(std::move(label))
{
}

bool ReplacePayloadCommand::execute()
{
    if (scene_ == nullptr) {
        return false;
    }
    XQDataNode* node = scene_->find(id_);
    if (node == nullptr) {
        return false;
    }

    if (!captured_) {
        old_domain_ = node->domainType();
        old_payload_ = node->payload();
        captured_ = true;
    }

    // Preserve node id and relations; only the payload/domain change.
    node->setPayload(new_domain_, new_payload_);
    return true;
}

void ReplacePayloadCommand::undo()
{
    if (scene_ == nullptr || !captured_) {
        return;
    }
    XQDataNode* node = scene_->find(id_);
    if (node == nullptr) {
        return;
    }
    node->setPayload(old_domain_, old_payload_);
}

std::string ReplacePayloadCommand::label() const
{
    return label_;
}

// ---------------------------------------------------------------------------
// RemoveNodeCommand
// ---------------------------------------------------------------------------

RemoveNodeCommand::RemoveNodeCommand(XQScene* scene, const NodeId& id, std::string label)
    : scene_(scene)
    , id_(id)
    , removed_node_()
    , removed_relations_()
    , label_(std::move(label))
{
}

bool RemoveNodeCommand::execute()
{
    if (scene_ == nullptr) {
        return false;
    }

    const XQDataNode* node = scene_->find(id_);
    if (node == nullptr) {
        return false;
    }

    // Snapshot the node and every relation it participates in, so undo can
    // restore both. Capture only on the first execute (re-executes via redo
    // re-snapshot identical state).
    removed_node_.clear();
    removed_node_.push_back(*node);

    removed_relations_.clear();
    const NodeId target = id_;
    std::vector<Relation>* relations = &removed_relations_;
    scene_->visit_derived_relations(
        [relations, target](const NodeId& source, const NodeId& derived) {
            if (source == target || derived == target) {
                Relation relation;
                relation.source = source;
                relation.derived = derived;
                relations->push_back(relation);
            }
        });

    scene_->remove(id_);
    return true;
}

void RemoveNodeCommand::undo()
{
    if (scene_ == nullptr || removed_node_.empty()) {
        return;
    }

    scene_->insert(removed_node_.front());
    for (std::vector<Relation>::const_iterator it = removed_relations_.begin();
         it != removed_relations_.end();
         ++it) {
        scene_->link_derived(it->source, it->derived);
    }
}

std::string RemoveNodeCommand::label() const
{
    return label_;
}

} // namespace xq
