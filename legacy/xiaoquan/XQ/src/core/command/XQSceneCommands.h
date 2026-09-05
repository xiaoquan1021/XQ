#ifndef XQ_CORE_COMMAND_XQ_SCENE_COMMANDS_H
#define XQ_CORE_COMMAND_XQ_SCENE_COMMANDS_H

#include "core/NodeId.h"
#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQPayload.h"
#include "core/XQScene.h"
#include "core/command/XQCommand.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace xq {

// Adds a node to the scene. undo() removes it again.
class AddNodeCommand : public XQCommand {
public:
    AddNodeCommand(XQScene* scene, XQDataNode node, std::string label = "Add node");

    bool execute() override;
    void undo() override;
    std::string label() const override;

private:
    XQScene* scene_;
    XQDataNode node_;
    NodeId id_;
    bool inserted_;
    std::string label_;
};

// Adds a node and links it as derived from an existing source node. undo()
// removes the node (which also drops the relation).
class AddNodeWithSourceRelationCommand : public XQCommand {
public:
    AddNodeWithSourceRelationCommand(XQScene* scene,
                                     XQDataNode node,
                                     const NodeId& source,
                                     std::string label = "Add node with source");

    bool execute() override;
    void undo() override;
    std::string label() const override;

private:
    XQScene* scene_;
    XQDataNode node_;
    NodeId id_;
    NodeId source_;
    bool inserted_;
    std::string label_;
};

// Replaces a node's payload while preserving its id and source/derived
// relations. undo() restores the previous payload and domain.
class ReplacePayloadCommand : public XQCommand {
public:
    ReplacePayloadCommand(XQScene* scene,
                          const NodeId& id,
                          XQDomainType domain,
                          std::shared_ptr<XQPayload> payload,
                          std::string label = "Replace payload");

    bool execute() override;
    void undo() override;
    std::string label() const override;

private:
    XQScene* scene_;
    NodeId id_;
    XQDomainType new_domain_;
    std::shared_ptr<XQPayload> new_payload_;
    XQDomainType old_domain_;
    std::shared_ptr<XQPayload> old_payload_;
    bool captured_;
    std::string label_;
};

// Removes a node, snapshotting it and its incident derived relations so undo()
// can restore both the node and the relations.
class RemoveNodeCommand : public XQCommand {
public:
    RemoveNodeCommand(XQScene* scene, const NodeId& id, std::string label = "Remove node");

    bool execute() override;
    void undo() override;
    std::string label() const override;

private:
    struct Relation {
        NodeId source;
        NodeId derived;
    };

    XQScene* scene_;
    NodeId id_;
    std::vector<XQDataNode> removed_node_;        // 0 or 1 entry (XQDataNode is not default-constructible)
    std::vector<Relation> removed_relations_;
    std::string label_;
};

} // namespace xq

#endif // XQ_CORE_COMMAND_XQ_SCENE_COMMANDS_H
