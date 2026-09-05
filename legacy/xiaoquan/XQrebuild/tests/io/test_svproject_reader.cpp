#include <core/NodeId.h>
#include <core/XQDataNode.h>
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

std::set<std::string> collect_node_keys(const xq::XQScene& scene)
{
    std::set<std::string> nodes;
    scene.visit_nodes(
        [&nodes](const xq::XQDataNode& node) {
            nodes.insert(node_key(node.domain_type(), node.display_name()));
        });
    return nodes;
}

std::map<xq::NodeId::ValueType, std::string> collect_display_names(const xq::XQScene& scene)
{
    std::map<xq::NodeId::ValueType, std::string> names;
    scene.visit_nodes(
        [&names](const xq::XQDataNode& node) {
            names[node.id().value()] = node.display_name();
        });
    return names;
}

std::set<std::pair<std::string, std::string>> collect_relations(const xq::XQScene& scene)
{
    const std::map<xq::NodeId::ValueType, std::string> display_names = collect_display_names(scene);
    std::set<std::pair<std::string, std::string>> relations;
    scene.visit_derived_relations(
        [&display_names, &relations](const xq::NodeId& source, const xq::NodeId& derived) {
            const std::map<xq::NodeId::ValueType, std::string>::const_iterator source_name =
                display_names.find(source.value());
            const std::map<xq::NodeId::ValueType, std::string>::const_iterator derived_name =
                display_names.find(derived.value());
            if (source_name != display_names.end() && derived_name != display_names.end()) {
                relations.insert(std::make_pair(source_name->second, derived_name->second));
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
    if (domain_counts.size() != 3) {
        return fail("svproject has no unexpected node domains", __LINE__);
    }
    if (collect_node_keys(scene) != expected_node_keys) {
        return fail("svproject node set matches expected scene", __LINE__);
    }

    const std::set<std::pair<std::string, std::string>> expected_relations = {
        std::make_pair("aorta", "aorta_final"),
        std::make_pair("btrunk", "btrunk_final"),
        std::make_pair("carotid", "carotid_final"),
        std::make_pair("rt_carotid", "rt_carotid_final"),
        std::make_pair("subclavian", "subclavian_final"),
    };
    if (collect_relations(scene) != expected_relations) {
        return fail("svproject path contour relations match expected scene", __LINE__);
    }

    return 0;
}
