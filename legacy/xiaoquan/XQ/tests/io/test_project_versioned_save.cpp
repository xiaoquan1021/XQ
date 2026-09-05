#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQProject.h>
#include <core/XQScene.h>
#include <io/project/XQProjectReader.h>
#include <io/project/XQProjectWriter.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
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

std::filesystem::path temp_project_path(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

bool write_text_file(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc);
    output << text;
    return output.good();
}

bool read_text_file(const std::filesystem::path& path, std::string* out)
{
    if (out == nullptr) {
        return false;
    }

    std::ifstream input(path.c_str());
    if (!input.good()) {
        return false;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        return false;
    }

    *out = buffer.str();
    return true;
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
        [&stale_nodes](const xq::NodeId& node, xq::XQScene::StaleReason reason) {
            if (reason == xq::XQScene::StaleReason::SourceChanged) {
                stale_nodes.insert(node.value());
            }
        });
    return stale_nodes;
}

bool scene_has_node(const xq::XQScene& scene, const ExpectedNode& expected)
{
    const xq::XQDataNode* node = scene.find(expected.id);
    return node != nullptr
        && node->id().value() == expected.id.value()
        && node->domain_type() == expected.domain_type
        && node->display_name() == expected.display_name;
}

int expect_scene(const xq::XQScene& scene,
                 const std::vector<ExpectedNode>& nodes,
                 const std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>>& relations,
                 const std::set<xq::NodeId::ValueType>& stale_nodes,
                 int line)
{
    for (std::vector<ExpectedNode>::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
        if (!scene_has_node(scene, *it)) {
            return fail("scene node metadata and NodeId value match", line);
        }
    }

    if (collect_relations(scene) != relations) {
        return fail("scene relations preserve NodeId values", line);
    }

    if (collect_stale_nodes(scene) != stale_nodes) {
        return fail("scene stale nodes preserve NodeId values", line);
    }

    for (std::set<xq::NodeId::ValueType>::const_iterator it = stale_nodes.begin();
         it != stale_nodes.end();
         ++it) {
        if (scene.stale_reason(xq::NodeId(*it)) != xq::XQScene::StaleReason::SourceChanged) {
            return fail("scene stale reason is SourceChanged", line);
        }
    }

    return 0;
}

int test_future_major_rejected()
{
    const std::filesystem::path path = temp_project_path("xq_m8_future_major.xqproj");
    std::filesystem::remove(path);
    if (!write_text_file(path, "XQ_NATIVE_PROJECT schemaVersion 2.0\n")) {
        return fail("write future major project file", __LINE__);
    }

    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path.string(), &result);
    std::filesystem::remove(path);

    if (status != xq::XQProjectReader::Status::UnsupportedVersion) {
        return fail("future major version is rejected", __LINE__);
    }
    if (result.diagnostics.empty()) {
        return fail("future major rejection emits diagnostics", __LINE__);
    }
    if (result.diagnostics[0].code() != "PROJECT_UNSUPPORTED_SCHEMA_VERSION") {
        return fail("future major rejection diagnostic code is structured", __LINE__);
    }

    return 0;
}

int test_same_major_higher_minor_loads()
{
    const std::filesystem::path path = temp_project_path("xq_m8_same_major_higher_minor.xqproj");
    const std::string text =
        "XQ_NATIVE_PROJECT schemaVersion 1.9\n"
        "writerVersion future-writer\n"
        "minimumReaderVersion 1.0\n"
        "createdWith XQrebuild\n"
        "projectId same-major-higher-minor\n"
        "scene\n"
        "nodes 2\n"
        "node 4101 volume SourceVolume\n"
        "node 4102 mesh DerivedMesh\n"
        "relations 1\n"
        "derived 4101 4102\n"
        "stale 1\n"
        "staleNode 4102 SourceChanged\n"
        "endScene\n"
        "provenance\n"
        "records 1\n"
        "record synthetic load future-writer 1\n"
        "endProvenance\n"
        "diagnostics 0\n"
        "end\n";
    std::filesystem::remove(path);
    if (!write_text_file(path, text)) {
        return fail("write same-major higher-minor project file", __LINE__);
    }

    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path.string(), &result);
    std::filesystem::remove(path);

    if (status != xq::XQProjectReader::Status::Ok) {
        return fail("same-major higher-minor project loads", __LINE__);
    }
    // No assets section in this older document: load succeeds with exactly one
    // Info diagnostic (PROJECT_NO_ASSET_DATA), prd §7.
    if (result.diagnostics.size() != 1
        || result.diagnostics[0].severity() != xq::DiagnosticSeverity::Info
        || result.diagnostics[0].code() != "PROJECT_NO_ASSET_DATA") {
        return fail("same-major higher-minor load reports no asset data", __LINE__);
    }
    if (result.project.state() != xq::XQProject::LifecycleState::Open) {
        return fail("same-major higher-minor project opens", __LINE__);
    }

    const std::vector<ExpectedNode> nodes = {
        {xq::NodeId(4101), "volume", "SourceVolume"},
        {xq::NodeId(4102), "mesh", "DerivedMesh"},
    };
    const std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>> relations = {
        std::make_pair(4101, 4102),
    };
    const std::set<xq::NodeId::ValueType> stale_nodes = {4102};
    return expect_scene(result.project.scene(), nodes, relations, stale_nodes, __LINE__);
}

int test_v1_legacy_without_provenance_migrates()
{
    const std::filesystem::path path = temp_project_path("xq_m8_legacy_v1_without_provenance.xqproj");
    const std::string text =
        "XQ_NATIVE_PROJECT schemaVersion 1\n"
        "writerVersion XQ-M2A-001\n"
        "minimumReaderVersion 1.0\n"
        "createdWith XQrebuild\n"
        "projectId legacy-v1-project\n"
        "scene\n"
        "nodes 3\n"
        "node 3001 volume SourceVolume\n"
        "node 3002 path CenterlinePath\n"
        "node 3003 mesh FlowMesh\n"
        "relations 2\n"
        "derived 3001 3002\n"
        "derived 3002 3003\n"
        "stale 2\n"
        "staleNode 3002 SourceChanged\n"
        "staleNode 3003 SourceChanged\n"
        "endScene\n"
        "diagnostics 0\n"
        "end\n";
    std::filesystem::remove(path);
    if (!write_text_file(path, text)) {
        return fail("write legacy v1 project file", __LINE__);
    }

    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path.string(), &result);
    std::filesystem::remove(path);

    if (status != xq::XQProjectReader::Status::Ok) {
        return fail("legacy v1 project without provenance migrates", __LINE__);
    }
    // Legacy v1 document has no assets section: one Info diagnostic, prd §7.
    if (result.diagnostics.size() != 1
        || result.diagnostics[0].severity() != xq::DiagnosticSeverity::Info
        || result.diagnostics[0].code() != "PROJECT_NO_ASSET_DATA") {
        return fail("legacy v1 migration reports no asset data", __LINE__);
    }
    if (result.project.state() != xq::XQProject::LifecycleState::Open) {
        return fail("legacy v1 migrated project opens", __LINE__);
    }

    const std::vector<ExpectedNode> nodes = {
        {xq::NodeId(3001), "volume", "SourceVolume"},
        {xq::NodeId(3002), "path", "CenterlinePath"},
        {xq::NodeId(3003), "mesh", "FlowMesh"},
    };
    const std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>> relations = {
        std::make_pair(3001, 3002),
        std::make_pair(3002, 3003),
    };
    const std::set<xq::NodeId::ValueType> stale_nodes = {3002, 3003};
    return expect_scene(result.project.scene(), nodes, relations, stale_nodes, __LINE__);
}

int test_current_v11_roundtrip_preserves_node_ids()
{
    xq::XQProject project;
    if (project.open() != xq::XQProject::LifecycleResult::Ok) {
        return fail("open current v1.1 source project", __LINE__);
    }

    const std::vector<ExpectedNode> nodes = {
        {xq::NodeId(5101), "case", "Roundtrip Project"},
        {xq::NodeId(5102), "image.volume", "Volume Baseline"},
        {xq::NodeId(5103), "surface.mesh", "Mesh % Branch"},
    };

    for (std::vector<ExpectedNode>::const_iterator it = nodes.begin(); it != nodes.end(); ++it) {
        if (project.scene().insert(xq::XQDataNode(it->id, it->domain_type, it->display_name))
            != xq::XQScene::InsertResult::Inserted) {
            return fail("insert current v1.1 roundtrip node", __LINE__);
        }
    }
    if (project.scene().link_derived(xq::NodeId(5102), xq::NodeId(5103))
        != xq::XQScene::RelationResult::Linked) {
        return fail("link current v1.1 roundtrip relation", __LINE__);
    }
    if (project.scene().mark_source_changed(xq::NodeId(5102)) != 1) {
        return fail("mark current v1.1 roundtrip stale node", __LINE__);
    }

    const std::filesystem::path path = temp_project_path("xq_m8_current_v11_roundtrip.xqproj");
    std::filesystem::remove(path);
    if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
        return fail("save current v1.1 project", __LINE__);
    }

    std::string saved_text;
    if (!read_text_file(path, &saved_text)) {
        std::filesystem::remove(path);
        return fail("read current v1.1 saved project", __LINE__);
    }
    if (saved_text.find("XQ_NATIVE_PROJECT schemaVersion 1.2\n") == std::string::npos) {
        std::filesystem::remove(path);
        return fail("writer emits schemaVersion 1.2", __LINE__);
    }
    if (saved_text.find("\nprovenance\n") == std::string::npos) {
        std::filesystem::remove(path);
        return fail("writer emits required provenance section", __LINE__);
    }

    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path.string(), &result);
    std::filesystem::remove(path);

    if (status != xq::XQProjectReader::Status::Ok) {
        return fail("load current v1.1 saved project", __LINE__);
    }
    if (!result.diagnostics.empty()) {
        return fail("current v1.1 roundtrip has no diagnostics", __LINE__);
    }

    const std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>> relations = {
        std::make_pair(5102, 5103),
    };
    const std::set<xq::NodeId::ValueType> stale_nodes = {5103};
    return expect_scene(result.project.scene(), nodes, relations, stale_nodes, __LINE__);
}

} // namespace

int main()
{
    int result = test_future_major_rejected();
    if (result != 0) {
        return result;
    }

    result = test_same_major_higher_minor_loads();
    if (result != 0) {
        return result;
    }

    result = test_v1_legacy_without_provenance_migrates();
    if (result != 0) {
        return result;
    }

    result = test_current_v11_roundtrip_preserves_node_ids();
    if (result != 0) {
        return result;
    }

    return 0;
}
