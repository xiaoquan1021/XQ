#include "core/command/XQSceneCommands.h"

#include <limits>

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
// SemanticReplacePayloadCommand
// ---------------------------------------------------------------------------

SemanticReplacePayloadCommand::SemanticReplacePayloadCommand(
    XQScene* scene,
    const NodeId& id,
    XQDomainType domain,
    std::shared_ptr<XQPayload> payload,
    std::string label)
    : scene_(scene)
    , id_(id)
    , new_domain_(domain)
    , new_payload_snapshot_(payload ? payload->clone() : std::shared_ptr<XQPayload>())
    , old_domain_(XQDomainType::Unknown)
    , old_payload_snapshot_()
    , old_revision_(0)
    , new_revision_(0)
    , stale_before_()
    , stale_after_()
    , captured_(false)
    , label_(std::move(label))
{
}

bool SemanticReplacePayloadCommand::execute()
{
    if (scene_ == nullptr || new_payload_snapshot_ == nullptr
        || new_domain_ == XQDomainType::Unknown
        || new_payload_snapshot_->domainType() != new_domain_) {
        return false;
    }
    XQDataNode* node = scene_->find(id_);
    if (node == nullptr) {
        return false;
    }

    const std::shared_ptr<XQPayload> applied = new_payload_snapshot_->clone();
    if (applied == nullptr) {
        return false;
    }

    if (!captured_) {
        if (node->contentRevision()
            == (std::numeric_limits<ContentRevision>::max)()) {
            return false;
        }

        const std::shared_ptr<XQPayload> current_payload = node->payload();
        const std::shared_ptr<XQPayload> old_snapshot = current_payload
            ? current_payload->clone()
            : std::shared_ptr<XQPayload>();
        if (current_payload != nullptr && old_snapshot == nullptr) {
            return false;
        }

        old_domain_ = node->domainType();
        old_payload_snapshot_ = old_snapshot;
        old_revision_ = node->contentRevision();
        new_revision_ = old_revision_ + 1;
        stale_before_ = scene_->stale_snapshot();

        node->setPayload(new_domain_, applied);
        node->setContentRevision(new_revision_);
        scene_->mark_source_changed(id_);
        stale_after_ = scene_->stale_snapshot();
        captured_ = true;
        return true;
    }

    // A redo is valid only against the exact revision/stale baseline restored
    // by undo. If external state changed while the command was on redo, fail
    // without partially applying the old prepared edit.
    if (node->contentRevision() != old_revision_
        || scene_->stale_snapshot() != stale_before_
        || !scene_->restore_stale_snapshot(stale_after_)) {
        return false;
    }
    node->setPayload(new_domain_, applied);
    node->setContentRevision(new_revision_);
    return true;
}

void SemanticReplacePayloadCommand::undo()
{
    if (scene_ == nullptr || !captured_) {
        return;
    }
    XQDataNode* node = scene_->find(id_);
    if (node == nullptr || !scene_->restore_stale_snapshot(stale_before_)) {
        return;
    }
    const std::shared_ptr<XQPayload> restored = old_payload_snapshot_
        ? old_payload_snapshot_->clone()
        : std::shared_ptr<XQPayload>();
    node->setPayload(old_domain_, restored);
    node->setContentRevision(old_revision_);
}

std::string SemanticReplacePayloadCommand::label() const
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
    , removed_stale_snapshot_()
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
    removed_stale_snapshot_ = scene_->stale_snapshot();

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
    scene_->restore_stale_snapshot(removed_stale_snapshot_);
}

std::string RemoveNodeCommand::label() const
{
    return label_;
}

} // namespace xq
