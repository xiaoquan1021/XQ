#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQPayload.h>
#include <core/XQScene.h>
#include <core/XQSourcePayload.h>
#include <core/command/XQCommandStack.h>
#include <core/command/XQSceneCommands.h>

#include <cstdio>
#include <memory>
#include <string>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

std::size_t node_count(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes([&count](const xq::XQDataNode&) { ++count; });
    return count;
}

std::size_t relation_count(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_derived_relations([&count](const xq::NodeId&, const xq::NodeId&) { ++count; });
    return count;
}

xq::XQDataNode make_node(xq::NodeId::ValueType id,
                         xq::XQDomainType domain,
                         const std::string& name,
                         const std::string& source_path)
{
    return xq::XQDataNode(xq::NodeId(id),
                          domain,
                          name,
                          std::make_shared<xq::XQSourcePayload>(domain, source_path));
}

} // namespace

int main()
{
    // AddNodeCommand: execute inserts, undo removes, redo re-inserts.
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        if (!stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::AddNodeCommand(&scene, make_node(1, xq::XQDomainType::Image, "img", "Images/x.vti"))))) {
            return fail("AddNodeCommand push reports success", __LINE__);
        }
        if (node_count(scene) != 1) {
            return fail("AddNodeCommand inserts on push", __LINE__);
        }
        if (!stack.can_undo() || stack.can_redo()) {
            return fail("stack state after push", __LINE__);
        }
        if (!stack.undo() || node_count(scene) != 0) {
            return fail("AddNodeCommand undo removes", __LINE__);
        }
        if (!stack.can_redo()) {
            return fail("redo available after undo", __LINE__);
        }
        if (!stack.redo() || node_count(scene) != 1) {
            return fail("AddNodeCommand redo re-inserts", __LINE__);
        }
    }

    // AddNodeCommand: duplicate ids are rejected and must not create an undo
    // entry that can delete the pre-existing node.
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        scene.insert(make_node(1, xq::XQDomainType::Image, "original", "Images/original.vti"));

        if (stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::AddNodeCommand(&scene, make_node(1, xq::XQDomainType::Image, "duplicate", "Images/dup.vti"))))) {
            return fail("duplicate AddNodeCommand push reports failure", __LINE__);
        }
        if (stack.undo_count() != 0 || stack.can_undo()) {
            return fail("duplicate AddNodeCommand does not enter undo stack", __LINE__);
        }
        if (stack.undo()) {
            return fail("duplicate AddNodeCommand leaves nothing to undo", __LINE__);
        }
        const xq::XQDataNode* node = scene.find(xq::NodeId(1));
        if (node == nullptr || node->display_name() != "original" || node_count(scene) != 1) {
            return fail("duplicate AddNodeCommand preserves original node", __LINE__);
        }
    }

    // AddNodeWithSourceRelationCommand: links derived; undo removes node + relation.
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId image_id(1);
        scene.insert(make_node(1, xq::XQDomainType::Image, "img", "Images/x.vti"));

        if (!stack.push(std::unique_ptr<xq::XQCommand>(new xq::AddNodeWithSourceRelationCommand(
                &scene, make_node(2, xq::XQDomainType::Path, "aorta", "Paths/aorta.pth"), image_id)))) {
            return fail("AddNodeWithSourceRelation push reports success", __LINE__);
        }
        if (node_count(scene) != 2 || relation_count(scene) != 1) {
            return fail("AddNodeWithSourceRelation links derived", __LINE__);
        }
        if (!stack.undo() || node_count(scene) != 1 || relation_count(scene) != 0) {
            return fail("AddNodeWithSourceRelation undo drops node and relation", __LINE__);
        }
        if (!stack.redo() || node_count(scene) != 2 || relation_count(scene) != 1) {
            return fail("AddNodeWithSourceRelation redo restores", __LINE__);
        }
    }

    // AddNodeWithSourceRelationCommand: relation failure rolls back the inserted
    // node and does not leave a false undo entry.
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;

        if (stack.push(std::unique_ptr<xq::XQCommand>(new xq::AddNodeWithSourceRelationCommand(
                &scene,
                make_node(2, xq::XQDomainType::Path, "orphan", "Paths/orphan.pth"),
                xq::NodeId(99))))) {
            return fail("missing-source AddNodeWithSourceRelation reports failure", __LINE__);
        }
        if (node_count(scene) != 0 || relation_count(scene) != 0 || stack.undo_count() != 0) {
            return fail("missing-source AddNodeWithSourceRelation leaves scene and stack unchanged", __LINE__);
        }

        scene.insert(make_node(3, xq::XQDomainType::Path, "self", "Paths/self.pth"));
        if (stack.push(std::unique_ptr<xq::XQCommand>(new xq::AddNodeWithSourceRelationCommand(
                &scene,
                make_node(4, xq::XQDomainType::Path, "self-link", "Paths/self_link.pth"),
                xq::NodeId(4))))) {
            return fail("self-relation AddNodeWithSourceRelation reports failure", __LINE__);
        }
        if (scene.find(xq::NodeId(4)) != nullptr || node_count(scene) != 1 || relation_count(scene) != 0) {
            return fail("self-relation AddNodeWithSourceRelation rolls back inserted node", __LINE__);
        }
    }

    // ReplacePayloadCommand: swaps payload, preserves id + source relation; undo restores.
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId image_id(1);
        const xq::NodeId path_id(2);
        scene.insert(make_node(1, xq::XQDomainType::Image, "img", "Images/x.vti"));
        scene.insert(make_node(2, xq::XQDomainType::Path, "aorta", "Paths/aorta.pth"));
        scene.link_derived(image_id, path_id);

        std::shared_ptr<xq::XQPayload> edited =
            std::make_shared<xq::XQSourcePayload>(xq::XQDomainType::Path, "Paths/aorta_edited.pth");
        if (!stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::ReplacePayloadCommand(&scene, path_id, xq::XQDomainType::Path, edited)))) {
            return fail("ReplacePayloadCommand push reports success", __LINE__);
        }

        const xq::XQDataNode* node = scene.find(path_id);
        if (node == nullptr) {
            return fail("node survives payload replace", __LINE__);
        }
        if (node->id() != path_id) {
            return fail("ReplacePayload preserves node id", __LINE__);
        }
        if (relation_count(scene) != 1) {
            return fail("ReplacePayload preserves source relation", __LINE__);
        }
        const xq::XQSourcePayload* after =
            dynamic_cast<const xq::XQSourcePayload*>(node->payload().get());
        if (after == nullptr || after->sourcePath() != "Paths/aorta_edited.pth") {
            return fail("ReplacePayload applies new payload", __LINE__);
        }

        if (!stack.undo()) {
            return fail("ReplacePayload undo runs", __LINE__);
        }
        const xq::XQDataNode* restored = scene.find(path_id);
        const xq::XQSourcePayload* before =
            restored != nullptr
                ? dynamic_cast<const xq::XQSourcePayload*>(restored->payload().get())
                : nullptr;
        if (before == nullptr || before->sourcePath() != "Paths/aorta.pth") {
            return fail("ReplacePayload undo restores old payload", __LINE__);
        }
        if (relation_count(scene) != 1) {
            return fail("ReplacePayload undo keeps source relation", __LINE__);
        }
    }

    // ReplacePayloadCommand: missing targets are rejected and not recorded.
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        std::shared_ptr<xq::XQPayload> edited =
            std::make_shared<xq::XQSourcePayload>(xq::XQDomainType::Path, "Paths/missing.pth");
        if (stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::ReplacePayloadCommand(&scene, xq::NodeId(404), xq::XQDomainType::Path, edited)))) {
            return fail("missing ReplacePayloadCommand push reports failure", __LINE__);
        }
        if (stack.undo_count() != 0 || stack.undo()) {
            return fail("missing ReplacePayloadCommand leaves no undo entry", __LINE__);
        }
    }

    // RemoveNodeCommand: removes node + incident relations; undo restores both.
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId image_id(1);
        const xq::NodeId path_id(2);
        const xq::NodeId contour_id(3);
        scene.insert(make_node(1, xq::XQDomainType::Image, "img", "Images/x.vti"));
        scene.insert(make_node(2, xq::XQDomainType::Path, "aorta", "Paths/aorta.pth"));
        scene.insert(make_node(3, xq::XQDomainType::ContourGroup, "aorta_final", "Segmentations/aorta_final.ctgr"));
        scene.link_derived(image_id, path_id);
        scene.link_derived(path_id, contour_id);

        if (node_count(scene) != 3 || relation_count(scene) != 2) {
            return fail("remove fixture initial state", __LINE__);
        }

        // Remove the middle node: it participates in two relations.
        if (!stack.push(std::unique_ptr<xq::XQCommand>(new xq::RemoveNodeCommand(&scene, path_id)))) {
            return fail("RemoveNodeCommand push reports success", __LINE__);
        }
        if (node_count(scene) != 2 || relation_count(scene) != 0) {
            return fail("RemoveNodeCommand drops node and incident relations", __LINE__);
        }
        if (scene.find(path_id) != nullptr) {
            return fail("RemoveNodeCommand node is gone", __LINE__);
        }

        if (!stack.undo()) {
            return fail("RemoveNodeCommand undo runs", __LINE__);
        }
        if (node_count(scene) != 3 || relation_count(scene) != 2) {
            return fail("RemoveNodeCommand undo restores node and relations", __LINE__);
        }
        const xq::XQDataNode* restored = scene.find(path_id);
        if (restored == nullptr || restored->display_name() != "aorta") {
            return fail("RemoveNodeCommand undo restores node payload identity", __LINE__);
        }

        // redo removes again.
        if (!stack.redo() || node_count(scene) != 2 || relation_count(scene) != 0) {
            return fail("RemoveNodeCommand redo removes again", __LINE__);
        }
    }

    // RemoveNodeCommand: missing targets are rejected and not recorded.
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        if (stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::RemoveNodeCommand(&scene, xq::NodeId(404))))) {
            return fail("missing RemoveNodeCommand push reports failure", __LINE__);
        }
        if (stack.undo_count() != 0 || stack.undo()) {
            return fail("missing RemoveNodeCommand leaves no undo entry", __LINE__);
        }
    }

    // A failed push must not clear an existing redo entry.
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        if (!stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::AddNodeCommand(&scene, make_node(1, xq::XQDomainType::Image, "img", "Images/x.vti"))))) {
            return fail("redo fixture AddNodeCommand push reports success", __LINE__);
        }
        if (!stack.undo() || !stack.can_redo()) {
            return fail("redo fixture has redo after undo", __LINE__);
        }
        scene.insert(make_node(1, xq::XQDomainType::Image, "external", "Images/external.vti"));
        if (stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::AddNodeCommand(&scene, make_node(1, xq::XQDomainType::Image, "duplicate", "Images/dup.vti"))))) {
            return fail("failed push while redo exists reports failure", __LINE__);
        }
        if (!stack.can_redo() || stack.redo_count() != 1) {
            return fail("failed push preserves redo stack", __LINE__);
        }
        if (stack.redo()) {
            return fail("blocked redo reports failure without corrupting scene", __LINE__);
        }
        const xq::XQDataNode* node = scene.find(xq::NodeId(1));
        if (node == nullptr || node->display_name() != "external") {
            return fail("failed redo preserves external node", __LINE__);
        }
    }

    // clear() empties undo/redo without touching the scene.
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        if (!stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::AddNodeCommand(&scene, make_node(1, xq::XQDomainType::Image, "img", "Images/x.vti"))))) {
            return fail("clear fixture AddNodeCommand push reports success", __LINE__);
        }
        stack.undo();
        if (!stack.can_redo()) {
            return fail("redo present before clear", __LINE__);
        }
        stack.clear();
        if (stack.can_undo() || stack.can_redo()) {
            return fail("clear empties both stacks", __LINE__);
        }
        if (stack.undo() || stack.redo()) {
            return fail("undo/redo no-op after clear", __LINE__);
        }
    }

    return 0;
}
