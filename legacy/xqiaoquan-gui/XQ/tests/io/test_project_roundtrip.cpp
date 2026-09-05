#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQProject.h>
#include <core/XQScene.h>
#include <io/project/XQProjectReader.h>
#include <io/project/XQProjectWriter.h>

#include <cstdio>
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

struct ExpectedNode {
    xq::NodeId id;
    std::string domain_type;
    std::string display_name;
};

bool node_matches(const xq::XQDataNode* node, const ExpectedNode& expected)
{
    return node != nullptr
        && node->id() == expected.id
        && node->domain_type() == expected.domain_type
        && node->display_name() == expected.display_name;
}

std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>>
collect_relations(const xq::XQScene& scene)
{
    std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>> relations;
    scene.visit_derived_relations(
        [&relations](const xq::NodeId& source, const xq::NodeId& derived) {
            relations.insert(std::make_pair(source.value(), derived.value()));
        });
    return relations;
}

std::set<xq::NodeId::ValueType> collect_stale_nodes(const xq::XQScene& scene)
{
    std::set<xq::NodeId::ValueType> stale_nodes;
    scene.visit_stale_nodes(
        [&stale_nodes](const xq::NodeId& node,
                       xq::XQScene::StaleReason reason) {
            if (reason == xq::XQScene::StaleReason::SourceChanged) {
                stale_nodes.insert(node.value());
            }
        });
    return stale_nodes;
}

std::string roundtrip_path()
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "xq_m2a_project_roundtrip.xqproj";
    return path.string();
}

} // namespace

int main()
{
    xq::XQProject project;
    if (project.open() != xq::XQProject::LifecycleResult::Ok) {
        return fail("project opens before synthetic roundtrip setup", __LINE__);
    }

    const std::vector<ExpectedNode> expected_nodes = {
        {xq::NodeId(1001), "case", "Synthetic Project"},
        {xq::NodeId(1002), "image.volume", "Aorta Volume % baseline"},
        {xq::NodeId(1003), "vascular.path", "Centerline Path"},
        {xq::NodeId(1004), "contour.group", "Aorta Contours"},
        {xq::NodeId(1005), "surface.mesh", "Flow Mesh"},
    };

    for (const ExpectedNode& expected : expected_nodes) {
        if (project.scene().insert(xq::XQDataNode(expected.id,
                                                 expected.domain_type,
                                                 expected.display_name))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert synthetic project node", __LINE__);
        }
    }

    const std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>> expected_relations = {
        std::make_pair(1002, 1003),
        std::make_pair(1003, 1004),
        std::make_pair(1004, 1005),
        std::make_pair(1002, 1005),
    };

    if (project.scene().link_derived(xq::NodeId(1002), xq::NodeId(1003))
        != xq::XQScene::RelationResult::Linked) {
        return fail("link volume to path", __LINE__);
    }
    if (project.scene().link_derived(xq::NodeId(1003), xq::NodeId(1004))
        != xq::XQScene::RelationResult::Linked) {
        return fail("link path to contour group", __LINE__);
    }
    if (project.scene().link_derived(xq::NodeId(1004), xq::NodeId(1005))
        != xq::XQScene::RelationResult::Linked) {
        return fail("link contour group to mesh", __LINE__);
    }
    if (project.scene().link_derived(xq::NodeId(1002), xq::NodeId(1005))
        != xq::XQScene::RelationResult::Linked) {
        return fail("link volume to mesh", __LINE__);
    }

    if (project.scene().mark_source_changed(xq::NodeId(1003)) != 2) {
        return fail("mark changed path stales all downstream nodes", __LINE__);
    }

    const std::set<xq::NodeId::ValueType> expected_stale_nodes = {
        1004,
        1005,
    };

    const std::string path = roundtrip_path();
    const xq::XQProjectWriter::Status save_status =
        xq::XQProjectWriter::save(project, path);
    if (save_status != xq::XQProjectWriter::Status::Ok) {
        return fail("save synthetic project", __LINE__);
    }

    if (project.close() != xq::XQProject::LifecycleResult::Ok) {
        return fail("close saved project", __LINE__);
    }
    if (project.scene().find(xq::NodeId(1002)) != nullptr) {
        return fail("close clears saved project scene", __LINE__);
    }

    xq::XQProjectReadResult loaded = {};
    const xq::XQProjectReader::Status load_status =
        xq::XQProjectReader::load(path, &loaded);
    if (load_status != xq::XQProjectReader::Status::Ok) {
        return fail("load saved synthetic project", __LINE__);
    }
    if (loaded.project.state() != xq::XQProject::LifecycleState::Open) {
        return fail("loaded project is open", __LINE__);
    }
    if (!loaded.diagnostics.empty()) {
        return fail("valid roundtrip has no reader diagnostics", __LINE__);
    }

    const xq::XQScene& scene = loaded.project.scene();
    for (const ExpectedNode& expected : expected_nodes) {
        if (!node_matches(scene.find(expected.id), expected)) {
            return fail("loaded node metadata matches", __LINE__);
        }
    }

    if (collect_relations(scene) != expected_relations) {
        return fail("loaded derived relations match", __LINE__);
    }

    if (collect_stale_nodes(scene) != expected_stale_nodes) {
        return fail("loaded stale nodes match", __LINE__);
    }
    if (scene.stale_reason(xq::NodeId(1004)) != xq::XQScene::StaleReason::SourceChanged) {
        return fail("loaded contour stale reason matches", __LINE__);
    }
    if (scene.stale_reason(xq::NodeId(1005)) != xq::XQScene::StaleReason::SourceChanged) {
        return fail("loaded mesh stale reason matches", __LINE__);
    }
    if (scene.is_stale(xq::NodeId(1002))) {
        return fail("loaded source volume is not stale", __LINE__);
    }
    if (scene.is_stale(xq::NodeId(1003))) {
        return fail("loaded path source is not stale", __LINE__);
    }

    std::filesystem::remove(path);
    return 0;
}
