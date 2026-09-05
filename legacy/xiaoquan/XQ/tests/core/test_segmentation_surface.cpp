#include <core/XQDataNode.h>
#include <core/XQImageVolume.h>
#include <core/XQScene.h>
#include <core/XQSegmentation.h>
#include <core/XQSurfaceModel.h>

#include <cmath>
#include <cstdio>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace {

int fail(const char* check)
{
    std::fprintf(stderr, "FAIL: %s\n", check);
    return 1;
}

bool close(double a, double b)
{
    return std::abs(a - b) < 1e-9;
}

bool same_geometry(const xq::ImageGeometry& a, const xq::ImageGeometry& b)
{
    for (int i = 0; i < 3; ++i) {
        if (a.dimensions[i] != b.dimensions[i]) {
            return false;
        }
        if (!close(a.spacing[i], b.spacing[i])) {
            return false;
        }
        if (!close(a.origin[i], b.origin[i])) {
            return false;
        }
        for (int j = 0; j < 3; ++j) {
            if (!close(a.direction[i][j], b.direction[i][j])) {
                return false;
            }
        }
    }
    return a.coordinateSystem == b.coordinateSystem;
}

xq::ImageGeometry canonical_geometry()
{
    xq::ImageGeometry geometry = {};
    geometry.dimensions[0] = 16;
    geometry.dimensions[1] = 12;
    geometry.dimensions[2] = 5;
    geometry.spacing[0] = 0.7;
    geometry.spacing[1] = 0.8;
    geometry.spacing[2] = 1.5;
    geometry.origin[0] = -10.0;
    geometry.origin[1] = 4.0;
    geometry.origin[2] = 22.0;
    geometry.direction[0][0] = 1.0;
    geometry.direction[1][1] = 1.0;
    geometry.direction[2][2] = 1.0;
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;
    return geometry;
}

} // namespace

int main()
{
    {
        const xq::NodeId segmentation_id(1101);
        const xq::NodeId source_image_node(1102);
        const xq::ImageGeometry geometry = canonical_geometry();

        xq::XQImageVolume source_image;
        source_image.setGeometry(geometry);

        xq::XQSegmentation segmentation;
        segmentation.setId(segmentation_id);
        segmentation.setSourceImageNode(source_image_node);
        segmentation.setGeometry(geometry);

        std::shared_ptr<xq::SegmentationMaskHandle> mask(new xq::SegmentationMaskHandle());
        segmentation.setMask(mask);

        std::vector<xq::SegmentationLabel> labels;
        labels.push_back({1, "lumen"});
        labels.push_back({2, "wall"});
        segmentation.setLabels(labels);

        if (segmentation.id() != segmentation_id) {
            return fail("segmentation id round-trips through accessors");
        }
        if (!segmentation.hasSourceImageNode()) {
            return fail("setting source image toggles source image presence");
        }
        if (segmentation.sourceImageNode() != source_image_node) {
            return fail("source image node round-trips through accessors");
        }
        if (!same_geometry(segmentation.geometry(), geometry)) {
            return fail("segmentation geometry round-trips through accessors");
        }
        if (!same_geometry(segmentation.geometry(), source_image.geometry())) {
            return fail("segmentation geometry matches source image geometry");
        }
        if (!segmentation.maskHandle()) {
            return fail("segmentation stores mask handle");
        }
        if (segmentation.maskHandle()->voxelCount() != static_cast<std::size_t>(16 * 12 * 5)) {
            return fail("mask voxel count matches geometry dimensions product");
        }
        if (segmentation.maskHandle()->labelCount() != static_cast<std::size_t>(2)) {
            return fail("mask label count matches segmentation labels");
        }
        if (segmentation.labels().size() != 2) {
            return fail("labels round-trip through accessors");
        }
        if (segmentation.labels()[0].value != 1 || segmentation.labels()[0].name != "lumen") {
            return fail("first label metadata round-trips through accessors");
        }
        if (segmentation.labels()[1].value != 2 || segmentation.labels()[1].name != "wall") {
            return fail("second label metadata round-trips through accessors");
        }
    }

    {
        xq::XQScene scene;
        const xq::NodeId image_node(1201);
        const xq::NodeId segmentation_node(1202);
        const xq::NodeId unrelated_node(1203);

        if (scene.insert(xq::XQDataNode(image_node, "image", "Synthetic Image"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert image node");
        }
        if (scene.insert(xq::XQDataNode(segmentation_node, "segmentation", "Synthetic Segmentation"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert segmentation node");
        }
        if (scene.insert(xq::XQDataNode(unrelated_node, "segmentation", "Unrelated Segmentation"))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert unrelated node");
        }
        if (scene.link_derived(image_node, segmentation_node) != xq::XQScene::RelationResult::Linked) {
            return fail("link image as segmentation source");
        }
        if (scene.mark_source_changed(image_node) != 1) {
            return fail("marking image changed reports one newly stale segmentation");
        }
        if (!scene.is_stale(segmentation_node)) {
            return fail("segmentation is stale after source image change");
        }
        if (scene.stale_reason(segmentation_node) != xq::XQScene::StaleReason::SourceChanged) {
            return fail("segmentation stale reason is source changed");
        }
        if (scene.is_stale(unrelated_node)) {
            return fail("unrelated node is not marked stale");
        }
    }

    {
        xq::XQSurfaceModel model;
        const xq::NodeId contour_group_node(1301);
        model.setSourceContourGroupNode(contour_group_node);

        if (!model.hasSourceContourGroupNode()) {
            return fail("surface model has source contour group after setting it");
        }
        if (model.sourceContourGroupNode() != contour_group_node) {
            return fail("surface model source contour group round-trips");
        }

        std::shared_ptr<xq::SurfaceGeometryHandle> geometry(new xq::SurfaceGeometryHandle());
        geometry->setCounts(7, 3);
        model.setGeometry(geometry);

        if (!model.geometry()) {
            return fail("surface model stores geometry handle");
        }
        if (model.geometry()->pointCount() != 7) {
            return fail("surface model geometry point count is readable");
        }
        if (model.geometry()->cellCount() != 3) {
            return fail("surface model geometry cell count is readable");
        }
    }

    return 0;
}
