#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQPath.h>
#include <core/XQPathPayload.h>
#include <core/XQProject.h>
#include <core/XQScene.h>
#include <io/project/SvProjectReader.h>

#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

struct ExpectedNode {
    std::string domain_type;
    std::string display_name;
};

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

std::string node_key(const std::string& domain_type, const std::string& display_name)
{
    return domain_type + ":" + display_name;
}

std::map<std::string, std::size_t> collect_domain_counts(const xq::XQScene& scene)
{
    std::map<std::string, std::size_t> counts;
    scene.visit_nodes(
        [&counts](const xq::XQDataNode& node) {
            ++counts[node.domain_type()];
        });
    return counts;
}

std::map<xq::XQScene::Group, std::size_t> collect_group_counts(const xq::XQScene& scene)
{
    std::map<xq::XQScene::Group, std::size_t> counts;
    scene.visit_nodes(
        [&counts](const xq::XQDataNode& node) {
            ++counts[xq::XQScene::groupForDomain(node.domainType())];
        });
    return counts;
}

std::set<std::string> collect_node_keys(const xq::XQScene& scene)
{
    std::set<std::string> nodes;
    scene.visit_nodes(
        [&nodes](const xq::XQDataNode& node) {
            nodes.insert(node_key(node.domain_type(), node.display_name()));
        });
    return nodes;
}

// Maps a node id to its "domain_type:display_name" key so that relations
// involving distinct domains with the same display name (e.g. surface_model /
// mesh / simulation_case all named "0090_0001") stay distinguishable.
std::map<xq::NodeId::ValueType, std::string> collect_node_key_by_id(const xq::XQScene& scene)
{
    std::map<xq::NodeId::ValueType, std::string> keys;
    scene.visit_nodes(
        [&keys](const xq::XQDataNode& node) {
            keys[node.id().value()] = node_key(node.domain_type(), node.display_name());
        });
    return keys;
}

std::set<std::pair<std::string, std::string>> collect_relations(const xq::XQScene& scene)
{
    const std::map<xq::NodeId::ValueType, std::string> keys = collect_node_key_by_id(scene);
    std::set<std::pair<std::string, std::string>> relations;
    scene.visit_derived_relations(
        [&keys, &relations](const xq::NodeId& source, const xq::NodeId& derived) {
            const std::map<xq::NodeId::ValueType, std::string>::const_iterator source_key =
                keys.find(source.value());
            const std::map<xq::NodeId::ValueType, std::string>::const_iterator derived_key =
                keys.find(derived.value());
            if (source_key != keys.end() && derived_key != keys.end()) {
                relations.insert(std::make_pair(source_key->second, derived_key->second));
            }
        });
    return relations;
}

std::size_t scene_node_count(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes(
        [&count](const xq::XQDataNode&) {
            ++count;
        });
    return count;
}

// Every node read from the project must carry an XQ-owned payload with a typed
// domain (not the legacy Unknown). This is the "data lands in XQ-owned
// payloads" acceptance check.
bool all_nodes_have_typed_payload(const xq::XQScene& scene)
{
    bool ok = true;
    scene.visit_nodes(
        [&ok](const xq::XQDataNode& node) {
            if (node.domainType() == xq::XQDomainType::Unknown || node.payload() == nullptr) {
                ok = false;
            }
        });
    return ok;
}

// Path nodes must carry real geometry: an XQPathPayload (the same payload type a
// newly created path uses) holding control points and resampled samples, with
// the path id matching the node id. This is the "0007/Paths/*.pth read into
// XQPath nodes" acceptance check.
bool all_path_nodes_carry_geometry(const xq::XQScene& scene)
{
    bool ok = true;
    scene.visit_nodes(
        [&ok](const xq::XQDataNode& node) {
            if (node.domainType() != xq::XQDomainType::Path) {
                return;
            }
            const xq::XQPathPayload* payload =
                dynamic_cast<const xq::XQPathPayload*>(node.payload().get());
            if (payload == nullptr) {
                ok = false;
                return;
            }
            const xq::XQPath& path = payload->path();
            const bool has_geometry =
                path.controlPoints().size() >= 2 && !path.samplePoints().empty();
            if (!has_geometry || path.id() != node.id()) {
                ok = false;
            }
        });
    return ok;
}

} // namespace

int main()
{
    const std::filesystem::path expected_scene_path = XQ_EXPECTED_SCENE;
    if (!std::filesystem::exists(expected_scene_path)) {
        return fail("expected scene contract exists", __LINE__);
    }

    xq::XQProjectReadResult result = {};
    const xq::SvProjectReader::Status status =
        xq::SvProjectReader::load(XQ_SVPROJECT_DIR, &result);
    if (status != xq::SvProjectReader::Status::Ok) {
        return fail("svproject load status", __LINE__);
    }
    if (result.project.state() != xq::XQProject::LifecycleState::Open) {
        return fail("svproject project is open", __LINE__);
    }
    if (!result.diagnostics.empty()) {
        return fail("valid svproject has no diagnostics", __LINE__);
    }

    const std::vector<ExpectedNode> expected_nodes = {
        {"image", "OSMSC0090-cm"},
        {"path", "aorta"},
        {"path", "btrunk"},
        {"path", "carotid"},
        {"path", "rt_carotid"},
        {"path", "subclavian"},
        {"contour_group", "aorta_final"},
        {"contour_group", "btrunk_final"},
        {"contour_group", "carotid_final"},
        {"contour_group", "rt_carotid_final"},
        {"contour_group", "subclavian_final"},
        {"surface_model", "0090_0001"},
        {"mesh", "0090_0001"},
        {"simulation_case", "0090_0001"},
    };

    std::set<std::string> expected_node_keys;
    for (const ExpectedNode& expected : expected_nodes) {
        expected_node_keys.insert(node_key(expected.domain_type, expected.display_name));
    }

    const xq::XQScene& scene = result.project.scene();
    if (scene_node_count(scene) != expected_nodes.size()) {
        return fail("svproject node count matches expected", __LINE__);
    }

    const std::map<std::string, std::size_t> domain_counts = collect_domain_counts(scene);
    if (domain_counts.find("image") == domain_counts.end() || domain_counts.at("image") != 1) {
        return fail("svproject image node count", __LINE__);
    }
    if (domain_counts.find("path") == domain_counts.end() || domain_counts.at("path") != 5) {
        return fail("svproject path node count", __LINE__);
    }
    if (domain_counts.find("contour_group") == domain_counts.end()
        || domain_counts.at("contour_group") != 5) {
        return fail("svproject contour group node count", __LINE__);
    }
    if (domain_counts.find("surface_model") == domain_counts.end()
        || domain_counts.at("surface_model") != 1) {
        return fail("svproject surface model node count", __LINE__);
    }
    if (domain_counts.find("mesh") == domain_counts.end() || domain_counts.at("mesh") != 1) {
        return fail("svproject mesh node count", __LINE__);
    }
    if (domain_counts.find("simulation_case") == domain_counts.end()
        || domain_counts.at("simulation_case") != 1) {
        return fail("svproject simulation case node count", __LINE__);
    }
    if (domain_counts.size() != 6) {
        return fail("svproject has no unexpected node domains", __LINE__);
    }
    if (collect_node_keys(scene) != expected_node_keys) {
        return fail("svproject node set matches expected scene", __LINE__);
    }

    if (!all_nodes_have_typed_payload(scene)) {
        return fail("svproject nodes carry typed XQ payloads", __LINE__);
    }

    if (!all_path_nodes_carry_geometry(scene)) {
        return fail("svproject path nodes carry real XQPath geometry", __LINE__);
    }

    const std::map<xq::XQScene::Group, std::size_t> group_counts = collect_group_counts(scene);
    const std::map<xq::XQScene::Group, std::size_t> expected_groups = {
        {xq::XQScene::Group::Images, 1},
        {xq::XQScene::Group::Paths, 5},
        {xq::XQScene::Group::Segmentations, 5},
        {xq::XQScene::Group::Models, 1},
        {xq::XQScene::Group::Meshes, 1},
        {xq::XQScene::Group::Simulations, 1},
    };
    if (group_counts != expected_groups) {
        return fail("svproject scene groups match expected scene", __LINE__);
    }

    const std::set<std::pair<std::string, std::string>> expected_relations = {
        std::make_pair("path:aorta", "contour_group:aorta_final"),
        std::make_pair("path:btrunk", "contour_group:btrunk_final"),
        std::make_pair("path:carotid", "contour_group:carotid_final"),
        std::make_pair("path:rt_carotid", "contour_group:rt_carotid_final"),
        std::make_pair("path:subclavian", "contour_group:subclavian_final"),
        std::make_pair("contour_group:aorta_final", "surface_model:0090_0001"),
        std::make_pair("contour_group:btrunk_final", "surface_model:0090_0001"),
        std::make_pair("contour_group:carotid_final", "surface_model:0090_0001"),
        std::make_pair("contour_group:rt_carotid_final", "surface_model:0090_0001"),
        std::make_pair("contour_group:subclavian_final", "surface_model:0090_0001"),
        std::make_pair("surface_model:0090_0001", "mesh:0090_0001"),
        std::make_pair("mesh:0090_0001", "simulation_case:0090_0001"),
    };
    if (collect_relations(scene) != expected_relations) {
        return fail("svproject source/derived relations match expected scene", __LINE__);
    }

    return 0;
}
