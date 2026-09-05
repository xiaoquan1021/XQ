#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQPayload.h>
#include <core/XQScene.h>
#include <core/XQSourcePayload.h>
#include <core/command/XQCommandStack.h>
#include <core/command/XQSceneCommands.h>
#include <core/command/XQCommand.h>

#include <cstdio>
#include <limits>
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

// execute() succeeds on the 1st call, fails on the 2nd, succeeds again from the
// 3rd on; undo() always succeeds. Reproduces a redo() whose execute() fails
// transiently: the command must stay on the redo stack so the user can retry.
class FlakyCommand : public xq::XQCommand {
public:
    int executeCalls = 0;
    bool executed = false;

    bool execute() override
    {
        ++executeCalls;
        if (executeCalls == 2) {
            return false;
        }
        executed = true;
        return true;
    }

    void undo() override
    {
        executed = false;
    }

    std::string label() const override
    {
        return "flaky";
    }
};

class UnclonablePayload : public xq::XQPayload {
public:
    xq::XQDomainType domainType() const override
    {
        return xq::XQDomainType::Path;
    }

    std::shared_ptr<xq::XQPayload> clone() const override
    {
        return std::shared_ptr<xq::XQPayload>();
    }
};

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
        const xq::NodeId contour_id(3);
        scene.insert(make_node(1, xq::XQDomainType::Image, "img", "Images/x.vti"));
        scene.insert(make_node(2, xq::XQDomainType::Path, "aorta", "Paths/aorta.pth"));
        scene.insert(make_node(
            3, xq::XQDomainType::ContourGroup, "contours", "Segmentations/aorta.ctgr"));
        scene.link_derived(image_id, path_id);
        scene.link_derived(path_id, contour_id);
        scene.find(path_id)->setContentRevision(9);
        const xq::XQScene::StaleSnapshot staleBefore = scene.stale_snapshot();

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
        if (relation_count(scene) != 2) {
            return fail("ReplacePayload preserves source relation", __LINE__);
        }
        if (node->contentRevision() != 9 || scene.is_stale(contour_id)
            || scene.stale_snapshot() != staleBefore) {
            return fail("ordinary ReplacePayload does not change revision or stale", __LINE__);
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
        if (relation_count(scene) != 2) {
            return fail("ReplacePayload undo keeps source relation", __LINE__);
        }
        if (restored->contentRevision() != 9 || scene.stale_snapshot() != staleBefore) {
            return fail("ordinary ReplacePayload undo preserves revision and stale", __LINE__);
        }

        if (!stack.redo()) {
            return fail("ReplacePayload redo runs", __LINE__);
        }
        const xq::XQDataNode* redone = scene.find(path_id);
        const xq::XQSourcePayload* redonePayload = redone == nullptr
            ? nullptr
            : dynamic_cast<const xq::XQSourcePayload*>(redone->payload().get());
        if (redonePayload == nullptr
            || redonePayload->sourcePath() != "Paths/aorta_edited.pth"
            || redone->contentRevision() != 9
            || scene.stale_snapshot() != staleBefore) {
            return fail("ordinary ReplacePayload redo preserves revision and stale", __LINE__);
        }
    }

    // SemanticReplacePayloadCommand: revision +1, transitive stale, and exact
    // payload/revision/full-stale undo+redo (including unrelated prior stale).
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId path_id(10);
        const xq::NodeId profile_id(11);
        const xq::NodeId result_id(12);
        const xq::NodeId unrelated_source(20);
        const xq::NodeId unrelated_derived(21);
        scene.insert(make_node(10, xq::XQDomainType::Path, "path", "Paths/original.pth"));
        scene.insert(make_node(
            11, xq::XQDomainType::VesselProfile, "profile", "Profiles/profile.vpf"));
        scene.insert(make_node(
            12, xq::XQDomainType::FlowResult, "result", "Results/result.flow"));
        scene.insert(make_node(
            20, xq::XQDomainType::Image, "unrelated source", "Images/other.vti"));
        scene.insert(make_node(
            21, xq::XQDomainType::Mesh, "unrelated stale", "Meshes/other.msh"));
        scene.link_derived(path_id, profile_id);
        scene.link_derived(profile_id, result_id);
        scene.link_derived(unrelated_source, unrelated_derived);
        scene.find(path_id)->setContentRevision(41);
        scene.mark_source_changed(unrelated_source);
        const xq::XQScene::StaleSnapshot staleBefore = scene.stale_snapshot();

        const std::shared_ptr<xq::XQPayload> edited =
            std::make_shared<xq::XQSourcePayload>(
                xq::XQDomainType::Path, "Paths/edited.pth");
        if (!stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::SemanticReplacePayloadCommand(
                    &scene, path_id, xq::XQDomainType::Path, edited, "Edit path")))) {
            return fail("semantic replace push succeeds", __LINE__);
        }
        const xq::XQDataNode* editedNode = scene.find(path_id);
        const xq::XQSourcePayload* editedPayload = editedNode == nullptr
            ? nullptr
            : dynamic_cast<const xq::XQSourcePayload*>(editedNode->payload().get());
        if (editedPayload == nullptr || editedPayload->sourcePath() != "Paths/edited.pth"
            || editedNode->contentRevision() != 42
            || scene.is_stale(path_id)
            || !scene.is_stale(profile_id) || !scene.is_stale(result_id)
            || !scene.is_stale(unrelated_derived)) {
            return fail("semantic replace advances revision and stales all downstream", __LINE__);
        }
        const xq::XQScene::StaleSnapshot staleAfter = scene.stale_snapshot();

        if (!stack.undo()) {
            return fail("semantic replace undo succeeds", __LINE__);
        }
        const xq::XQDataNode* restoredNode = scene.find(path_id);
        const xq::XQSourcePayload* restoredPayload = restoredNode == nullptr
            ? nullptr
            : dynamic_cast<const xq::XQSourcePayload*>(restoredNode->payload().get());
        if (restoredPayload == nullptr
            || restoredPayload->sourcePath() != "Paths/original.pth"
            || restoredNode->contentRevision() != 41
            || scene.stale_snapshot() != staleBefore) {
            return fail("semantic undo exactly restores payload revision and stale", __LINE__);
        }

        if (!stack.redo()) {
            return fail("semantic replace redo succeeds", __LINE__);
        }
        const xq::XQDataNode* redoneNode = scene.find(path_id);
        const xq::XQSourcePayload* redonePayload = redoneNode == nullptr
            ? nullptr
            : dynamic_cast<const xq::XQSourcePayload*>(redoneNode->payload().get());
        if (redonePayload == nullptr || redonePayload->sourcePath() != "Paths/edited.pth"
            || redoneNode->contentRevision() != 42
            || scene.stale_snapshot() != staleAfter) {
            return fail("semantic redo exactly restores post-edit state", __LINE__);
        }

        if (!stack.undo()) {
            return fail("semantic second undo succeeds", __LINE__);
        }
        xq::XQDataNode* materializedNode = scene.find(path_id);
        materializedNode->setPayload(
            xq::XQDomainType::Path,
            std::make_shared<xq::XQSourcePayload>(
                xq::XQDomainType::Path, "Paths/materialized.pth"));
        if (materializedNode->contentRevision() != 41
            || scene.stale_snapshot() != staleBefore) {
            return fail("direct materialization changes neither revision nor stale", __LINE__);
        }
    }

    // Semantic replace rejects a missing target and revision overflow with no
    // mutation or undo entry.
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const std::shared_ptr<xq::XQPayload> edited =
            std::make_shared<xq::XQSourcePayload>(
                xq::XQDomainType::Path, "Paths/overflow.pth");
        if (stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::SemanticReplacePayloadCommand(
                    &scene, xq::NodeId(404), xq::XQDomainType::Path, edited)))) {
            return fail("missing semantic target is rejected", __LINE__);
        }
        scene.insert(make_node(30, xq::XQDomainType::Path, "max revision", "Paths/max.pth"));
        scene.find(xq::NodeId(30))->setContentRevision(
            (std::numeric_limits<xq::ContentRevision>::max)());
        const xq::XQScene::StaleSnapshot staleBefore = scene.stale_snapshot();
        if (stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::SemanticReplacePayloadCommand(
                    &scene, xq::NodeId(30), xq::XQDomainType::Path, edited)))) {
            return fail("semantic revision overflow is rejected", __LINE__);
        }
        const xq::XQSourcePayload* payload = dynamic_cast<const xq::XQSourcePayload*>(
            scene.find(xq::NodeId(30))->payload().get());
        if (payload == nullptr || payload->sourcePath() != "Paths/max.pth"
            || scene.stale_snapshot() != staleBefore || stack.undo_count() != 0) {
            return fail("semantic failures have zero side effects", __LINE__);
        }
    }

    // A semantic edit must be reversible before it mutates the scene. Reject
    // a non-null old payload that cannot be cloned, leaving all state intact.
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId path_id(31);
        scene.insert(xq::XQDataNode(
            path_id,
            xq::XQDomainType::Path,
            "unclonable",
            std::make_shared<UnclonablePayload>()));
        scene.find(path_id)->setContentRevision(7);
        const xq::XQScene::StaleSnapshot staleBefore = scene.stale_snapshot();
        const std::shared_ptr<xq::XQPayload> edited =
            std::make_shared<xq::XQSourcePayload>(
                xq::XQDomainType::Path, "Paths/rejected.pth");

        if (stack.push(std::unique_ptr<xq::XQCommand>(
                new xq::SemanticReplacePayloadCommand(
                    &scene, path_id, xq::XQDomainType::Path, edited)))) {
            return fail("unclonable old semantic payload is rejected", __LINE__);
        }
        const xq::XQDataNode* unchanged = scene.find(path_id);
        if (unchanged == nullptr
            || dynamic_cast<const UnclonablePayload*>(unchanged->payload().get()) == nullptr
            || unchanged->contentRevision() != 7
            || scene.stale_snapshot() != staleBefore
            || stack.undo_count() != 0) {
            return fail("failed semantic snapshot has zero side effects", __LINE__);
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
        scene.mark_source_changed(image_id);
        const xq::XQScene::StaleSnapshot staleBefore = scene.stale_snapshot();

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
        if (scene.stale_snapshot() != staleBefore) {
            return fail("RemoveNodeCommand undo restores full stale snapshot", __LINE__);
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

    // A redo() whose execute() fails must keep the command on the redo stack
    // (not silently drop it), so a later redo() can retry and succeed.
    {
        xq::XQCommandStack stack;
        auto owned = std::unique_ptr<FlakyCommand>(new FlakyCommand());
        FlakyCommand* flaky = owned.get();

        if (!stack.push(std::move(owned))) {
            return fail("flaky command initial push succeeds", __LINE__);
        }
        if (!stack.undo() || !stack.can_redo()) {
            return fail("flaky command undo leaves a redo entry", __LINE__);
        }

        const bool redo_failed_result = stack.redo(); // 2nd execute() -> false
        if (redo_failed_result) {
            return fail("failed redo reports false", __LINE__);
        }
        if (!stack.can_redo() || stack.redo_count() != 1) {
            return fail("failed redo keeps command on redo stack", __LINE__);
        }
        if (stack.can_undo()) {
            return fail("failed redo puts nothing on undo stack", __LINE__);
        }

        const bool redo_retry_result = stack.redo(); // 3rd execute() -> true
        if (!redo_retry_result) {
            return fail("retried redo succeeds", __LINE__);
        }
        if (!flaky->executed || flaky->executeCalls != 3) {
            return fail("retried redo re-executed the command", __LINE__);
        }
        if (!stack.can_undo() || stack.can_redo()) {
            return fail("retried redo restores normal stack state", __LINE__);
        }
    }

    return 0;
}
