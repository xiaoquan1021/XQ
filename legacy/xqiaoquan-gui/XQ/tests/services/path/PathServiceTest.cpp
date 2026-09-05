#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQPath.h>
#include <core/XQPathPayload.h>
#include <core/XQScaleSlot.h>
#include <core/XQScene.h>
#include <core/XQSourcePayload.h>
#include <core/command/XQCommandStack.h>
#include <services/path/PathService.h>

#include <cstddef>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

} // namespace

// Explicit-failure checks (no assert): survive Release /DNDEBUG. Calls with side
// effects are evaluated into a variable before being checked.
#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

namespace {

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

const xq::XQPathPayload* path_payload(const xq::XQScene& scene, const xq::NodeId& id)
{
    const xq::XQDataNode* node = scene.find(id);
    if (node == nullptr) {
        return nullptr;
    }
    return dynamic_cast<const xq::XQPathPayload*>(node->payload().get());
}

bool same_point(const xq::Point3& lhs, const xq::Point3& rhs)
{
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

bool same_path(const xq::XQPath& lhs, const xq::XQPath& rhs)
{
    if (lhs.id() != rhs.id()
        || lhs.interpolation() != rhs.interpolation()
        || lhs.sampleSpacing() != rhs.sampleSpacing()
        || lhs.hasSourceImageNode() != rhs.hasSourceImageNode()) {
        return false;
    }
    if (lhs.hasSourceImageNode() && lhs.sourceImageNode() != rhs.sourceImageNode()) {
        return false;
    }

    const std::vector<xq::PathControlPoint>& lhs_control = lhs.controlPoints();
    const std::vector<xq::PathControlPoint>& rhs_control = rhs.controlPoints();
    if (lhs_control.size() != rhs_control.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs_control.size(); ++i) {
        if (!same_point(lhs_control[i].position, rhs_control[i].position)) {
            return false;
        }
    }

    const std::vector<xq::PathSamplePoint>& lhs_samples = lhs.samplePoints();
    const std::vector<xq::PathSamplePoint>& rhs_samples = rhs.samplePoints();
    if (lhs_samples.size() != rhs_samples.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs_samples.size(); ++i) {
        if (!same_point(lhs_samples[i].position, rhs_samples[i].position)
            || !same_point(lhs_samples[i].tangent, rhs_samples[i].tangent)
            || !same_point(lhs_samples[i].normal, rhs_samples[i].normal)
            || !same_point(lhs_samples[i].binormal, rhs_samples[i].binormal)
            || lhs_samples[i].arcLength != rhs_samples[i].arcLength) {
            return false;
        }
    }
    return true;
}

std::vector<xq::PathControlPoint> straight_line_points()
{
    return {
        {{0.0, 0.0, 0.0}},
        {{10.0, 0.0, 0.0}},
        {{10.0, 10.0, 0.0}},
    };
}

} // namespace

int main()
{
    const xq::NodeId image_id(1);
    const xq::NodeId path_id(2);

    // --- create: validation ------------------------------------------------
    {
        xq::XQScene scene;
        scene.insert(xq::XQDataNode(image_id,
                                    xq::XQDomainType::Image,
                                    "img",
                                    std::make_shared<xq::XQSourcePayload>(xq::XQDomainType::Image,
                                                                          "Images/x.vti")));

        const std::vector<xq::PathControlPoint> one_point = {{{0.0, 0.0, 0.0}}};
        const xq::PathService::Result too_few =
            xq::PathService::createPathCommand(&scene, path_id, "p", image_id, one_point, 1.0);
        CHECK(too_few.status == xq::PathService::Status::NotEnoughControlPoints);
        CHECK(too_few.command == nullptr);

        const xq::PathService::Result bad_spacing = xq::PathService::createPathCommand(
            &scene, path_id, "p", image_id, straight_line_points(), 0.0);
        CHECK(bad_spacing.status == xq::PathService::Status::InvalidSpacing);
        CHECK(bad_spacing.command == nullptr);

        const xq::PathService::Result null_scene = xq::PathService::createPathCommand(
            nullptr, path_id, "p", image_id, straight_line_points(), 1.0);
        CHECK(null_scene.status == xq::PathService::Status::NullScene);
    }

    // --- create: execute / undo / redo, payload identity -------------------
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        xq::XQDataNode imageNode(
            image_id,
            xq::XQDomainType::Image,
            "img",
            std::make_shared<xq::XQSourcePayload>(
                xq::XQDomainType::Image, "Images/x.vti"));
        imageNode.setScaleSlot(xq::ScaleSlot::Organ);
        CHECK(scene.insert(std::move(imageNode))
              == xq::XQScene::InsertResult::Inserted);

        xq::PathService::Result created = xq::PathService::createPathCommand(
            &scene, path_id, "aorta", image_id, straight_line_points(), 1.0);
        CHECK(created.ok());
        CHECK(created.command != nullptr);

        stack.push(std::move(created.command));
        CHECK(node_count(scene) == 2);
        CHECK(relation_count(scene) == 1);

        const xq::XQPathPayload* payload = path_payload(scene, path_id);
        CHECK(payload != nullptr);
        CHECK(payload->path().id() == path_id);
        CHECK(payload->path().hasSourceImageNode());
        CHECK(payload->path().sourceImageNode() == image_id);
        CHECK(payload->path().controlPoints().size() == 3);
        CHECK(!payload->path().samplePoints().empty());
        CHECK(scene.find(path_id)->scaleSlot() == xq::ScaleSlot::Organ);

        const bool undone = stack.undo();
        CHECK(undone);
        CHECK(node_count(scene) == 1);
        CHECK(relation_count(scene) == 0);

        const bool redone = stack.redo();
        CHECK(redone);
        CHECK(node_count(scene) == 2);
        CHECK(relation_count(scene) == 1);
        CHECK(path_payload(scene, path_id) != nullptr);
    }

    // --- move / insert / delete / resample: edits preserve id + source -----
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        scene.insert(xq::XQDataNode(image_id,
                                    xq::XQDomainType::Image,
                                    "img",
                                    std::make_shared<xq::XQSourcePayload>(xq::XQDomainType::Image,
                                                                          "Images/x.vti")));
        xq::PathService::Result created = xq::PathService::createPathCommand(
            &scene, path_id, "aorta", image_id, straight_line_points(), 1.0);
        CHECK(created.ok());
        stack.push(std::move(created.command));

        // move control point 0.
        {
            const xq::XQDataNode* node = scene.find(path_id);
            CHECK(node != nullptr);
            xq::PathService::Result moved = xq::PathService::moveControlPointCommand(
                &scene, *node, 0, {-5.0, 0.0, 0.0}, 1.0);
            CHECK(moved.ok());
            stack.push(std::move(moved.command));

            const xq::XQPathPayload* payload = path_payload(scene, path_id);
            CHECK(payload != nullptr);
            CHECK(payload->path().id() == path_id);          // id preserved
            CHECK(payload->path().sourceImageNode() == image_id); // source preserved
            CHECK(payload->path().controlPoints().size() == 3);
            const xq::Point3 moved_pos = payload->path().controlPoints()[0].position;
            CHECK(moved_pos.x == -5.0);
        }

        // move index out of range.
        {
            const xq::XQDataNode* node = scene.find(path_id);
            CHECK(node != nullptr);
            xq::PathService::Result bad = xq::PathService::moveControlPointCommand(
                &scene, *node, 99, {0.0, 0.0, 0.0}, 1.0);
            CHECK(bad.status == xq::PathService::Status::IndexOutOfRange);
            CHECK(bad.command == nullptr);
        }

        // insert at end (index == size is allowed).
        {
            const xq::XQDataNode* node = scene.find(path_id);
            CHECK(node != nullptr);
            const std::size_t size = path_payload(scene, path_id)->path().controlPoints().size();
            xq::PathService::Result inserted = xq::PathService::insertControlPointCommand(
                &scene, *node, size, {20.0, 10.0, 0.0}, 1.0);
            CHECK(inserted.ok());
            stack.push(std::move(inserted.command));
            CHECK(path_payload(scene, path_id)->path().controlPoints().size() == size + 1);
        }

        // insert index > size rejected.
        {
            const xq::XQDataNode* node = scene.find(path_id);
            CHECK(node != nullptr);
            const std::size_t size = path_payload(scene, path_id)->path().controlPoints().size();
            xq::PathService::Result bad = xq::PathService::insertControlPointCommand(
                &scene, *node, size + 1, {0.0, 0.0, 0.0}, 1.0);
            CHECK(bad.status == xq::PathService::Status::IndexOutOfRange);
        }

        // delete one point.
        {
            const xq::XQDataNode* node = scene.find(path_id);
            CHECK(node != nullptr);
            const std::size_t size = path_payload(scene, path_id)->path().controlPoints().size();
            xq::PathService::Result deleted =
                xq::PathService::deleteControlPointCommand(&scene, *node, 0, 1.0);
            CHECK(deleted.ok());
            stack.push(std::move(deleted.command));
            CHECK(path_payload(scene, path_id)->path().controlPoints().size() == size - 1);
            CHECK(path_payload(scene, path_id)->path().id() == path_id);
        }

        // undo the delete restores the point and keeps id.
        {
            const std::size_t before = path_payload(scene, path_id)->path().controlPoints().size();
            const bool undone = stack.undo();
            CHECK(undone);
            CHECK(path_payload(scene, path_id)->path().controlPoints().size() == before + 1);
            CHECK(path_payload(scene, path_id)->path().id() == path_id);
        }

        // redo the delete.
        {
            const bool redone = stack.redo();
            CHECK(redone);
            CHECK(path_payload(scene, path_id)->path().id() == path_id);
        }

        // resample with a finer spacing changes sample count, keeps id + source.
        {
            const xq::XQDataNode* node = scene.find(path_id);
            CHECK(node != nullptr);
            const std::size_t coarse = path_payload(scene, path_id)->path().samplePoints().size();
            xq::PathService::Result resampled =
                xq::PathService::resamplePathCommand(&scene, *node, 0.25);
            CHECK(resampled.ok());
            stack.push(std::move(resampled.command));
            const xq::XQPathPayload* payload = path_payload(scene, path_id);
            CHECK(payload != nullptr);
            CHECK(payload->path().sampleSpacing() == 0.25);
            CHECK(payload->path().samplePoints().size() > coarse);
            CHECK(payload->path().id() == path_id);
            CHECK(payload->path().sourceImageNode() == image_id);
        }
    }

    // --- delete below 2 points rejected; edit on non-path rejected ---------
    {
        xq::XQScene scene;
        scene.insert(xq::XQDataNode(image_id,
                                    xq::XQDomainType::Image,
                                    "img",
                                    std::make_shared<xq::XQSourcePayload>(xq::XQDomainType::Image,
                                                                          "Images/x.vti")));
        const std::vector<xq::PathControlPoint> two = {{{0.0, 0.0, 0.0}}, {{10.0, 0.0, 0.0}}};
        xq::XQCommandStack stack;
        xq::PathService::Result created =
            xq::PathService::createPathCommand(&scene, path_id, "p", image_id, two, 1.0);
        CHECK(created.ok());
        stack.push(std::move(created.command));

        const xq::XQDataNode* node = scene.find(path_id);
        CHECK(node != nullptr);
        xq::PathService::Result deleted =
            xq::PathService::deleteControlPointCommand(&scene, *node, 0, 1.0);
        CHECK(deleted.status == xq::PathService::Status::NotEnoughControlPoints);
        CHECK(deleted.command == nullptr);

        // editing the image node (not a path) is rejected.
        const xq::XQDataNode* image_node = scene.find(image_id);
        CHECK(image_node != nullptr);
        xq::PathService::Result not_path = xq::PathService::resamplePathCommand(
            &scene, *image_node, 1.0);
        CHECK(not_path.status == xq::PathService::Status::NotAPathNode);
    }

    // --- semantic edit: revision/stale and exact payload undo/redo ----------
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId contour_id(3);
        const xq::NodeId mesh_id(4);
        const xq::NodeId unrelated_source_id(5);
        const xq::NodeId unrelated_stale_id(6);

        CHECK(scene.insert(xq::XQDataNode(
                  image_id,
                  xq::XQDomainType::Image,
                  "img",
                  std::make_shared<xq::XQSourcePayload>(
                      xq::XQDomainType::Image, "Images/x.vti")))
              == xq::XQScene::InsertResult::Inserted);
        xq::PathService::Result created = xq::PathService::createPathCommand(
            &scene, path_id, "aorta", image_id, straight_line_points(), 1.0);
        CHECK(created.ok());
        CHECK(stack.push(std::move(created.command)));

        CHECK(scene.insert(xq::XQDataNode(
                  contour_id,
                  xq::XQDomainType::ContourGroup,
                  "contours",
                  std::make_shared<xq::XQSourcePayload>(
                      xq::XQDomainType::ContourGroup, "Segmentations/aorta.ctgr")))
              == xq::XQScene::InsertResult::Inserted);
        CHECK(scene.insert(xq::XQDataNode(
                  mesh_id,
                  xq::XQDomainType::Mesh,
                  "mesh",
                  std::make_shared<xq::XQSourcePayload>(
                      xq::XQDomainType::Mesh, "Meshes/aorta.vtu")))
              == xq::XQScene::InsertResult::Inserted);
        CHECK(scene.insert(xq::XQDataNode(
                  unrelated_source_id,
                  xq::XQDomainType::Path,
                  "unrelated source",
                  std::make_shared<xq::XQSourcePayload>(
                      xq::XQDomainType::Path, "Paths/unrelated.pth")))
              == xq::XQScene::InsertResult::Inserted);
        CHECK(scene.insert(xq::XQDataNode(
                  unrelated_stale_id,
                  xq::XQDomainType::Mesh,
                  "unrelated stale",
                  std::make_shared<xq::XQSourcePayload>(
                      xq::XQDomainType::Mesh, "Meshes/unrelated.vtu")))
              == xq::XQScene::InsertResult::Inserted);
        CHECK(scene.link_derived(path_id, contour_id) == xq::XQScene::RelationResult::Linked);
        CHECK(scene.link_derived(contour_id, mesh_id) == xq::XQScene::RelationResult::Linked);
        CHECK(scene.link_derived(unrelated_source_id, unrelated_stale_id)
              == xq::XQScene::RelationResult::Linked);
        CHECK(scene.mark_source_changed(unrelated_source_id) == 1);

        xq::XQDataNode* path_node = scene.find(path_id);
        CHECK(path_node != nullptr);
        path_node->setContentRevision(41);
        const xq::XQPathPayload* before_payload = path_payload(scene, path_id);
        CHECK(before_payload != nullptr);
        const xq::XQPath before_path = before_payload->path();
        const xq::XQScene::StaleSnapshot stale_before = scene.stale_snapshot();
        CHECK(stale_before.size() == 1);
        CHECK(scene.is_stale(unrelated_stale_id));

        xq::PathService::Result moved = xq::PathService::moveControlPointCommand(
            &scene, *path_node, 0, {-5.0, 0.0, 0.0}, 1.0);
        CHECK(moved.ok());
        CHECK(stack.push(std::move(moved.command)));

        const xq::XQDataNode* after_edit_node = scene.find(path_id);
        const xq::XQPathPayload* after_edit_payload = path_payload(scene, path_id);
        CHECK(after_edit_node != nullptr);
        CHECK(after_edit_payload != nullptr);
        CHECK(after_edit_node->contentRevision() == 42);
        CHECK(after_edit_payload->path().controlPoints()[0].position.x == -5.0);
        CHECK(!scene.is_stale(path_id));
        CHECK(scene.is_stale(contour_id));
        CHECK(scene.is_stale(mesh_id));
        CHECK(scene.is_stale(unrelated_stale_id));
        const xq::XQPath edited_path = after_edit_payload->path();
        const xq::XQScene::StaleSnapshot stale_after = scene.stale_snapshot();
        CHECK(stale_after.size() == 3);

        CHECK(stack.undo());
        const xq::XQDataNode* after_undo_node = scene.find(path_id);
        const xq::XQPathPayload* after_undo_payload = path_payload(scene, path_id);
        CHECK(after_undo_node != nullptr);
        CHECK(after_undo_payload != nullptr);
        CHECK(after_undo_node->contentRevision() == 41);
        CHECK(same_path(after_undo_payload->path(), before_path));
        CHECK(scene.stale_snapshot() == stale_before);

        CHECK(stack.redo());
        const xq::XQDataNode* after_redo_node = scene.find(path_id);
        const xq::XQPathPayload* after_redo_payload = path_payload(scene, path_id);
        CHECK(after_redo_node != nullptr);
        CHECK(after_redo_payload != nullptr);
        CHECK(after_redo_node->contentRevision() == 42);
        CHECK(same_path(after_redo_payload->path(), edited_path));
        CHECK(scene.stale_snapshot() == stale_after);
    }

    return 0;
}
