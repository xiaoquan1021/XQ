// Structural project round-trip for the new M5/M6 domains (FlowResult,
// AiAnalysis). The writer serializes node structure only -- id / domain_type /
// display_name / derived relations / stale flags -- not payload entity data
// (entity persistence is tracked tech debt). domainTypeToString already maps the
// new domains, so the structure of a flow-result / ai-analysis node must survive
// save + load verbatim.
//
// Mirrors test_project_roundtrip but covers a flow/AI main-line tail and uses
// payload-aware (typed-domain) nodes whose domain_type() is the new token.

#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQPayload.h>
#include <core/XQProject.h>
#include <core/XQScene.h>
#include <io/project/XQProjectReader.h>
#include <io/project/XQProjectWriter.h>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

} // namespace

#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

namespace {

struct ExpectedNode {
    xq::NodeId id;
    std::string domain_type;   // the serialized token (domainTypeToString)
    std::string display_name;
};

bool nodeMatches(const xq::XQDataNode* node, const ExpectedNode& expected)
{
    return node != nullptr
        && node->id() == expected.id
        && node->domain_type() == expected.domain_type
        && node->display_name() == expected.display_name;
}

std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>>
collectRelations(const xq::XQScene& scene)
{
    std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>> relations;
    scene.visit_derived_relations(
        [&relations](const xq::NodeId& source, const xq::NodeId& derived) {
            relations.insert(std::make_pair(source.value(), derived.value()));
        });
    return relations;
}

std::string roundtripPath()
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "xq_m7_new_domain_roundtrip.xqproj";
    return path.string();
}

// A typed-domain node with no payload (structure-only; payload persistence is
// out of scope -- the writer stores only the structure anyway).
xq::XQDataNode typedNode(const xq::NodeId& id, xq::XQDomainType domain, const std::string& name)
{
    return xq::XQDataNode(id, domain, name, std::shared_ptr<xq::XQPayload>());
}

} // namespace

int main()
{
    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);

    // Main-line tail down to the new domains: mesh -> case -> flow -> analysis.
    const std::vector<ExpectedNode> expected = {
        {xq::NodeId(2001), "mesh", "Aorta Volume Mesh"},
        {xq::NodeId(2002), "simulation_case", "Aorta Case"},
        {xq::NodeId(2003), "flow_result", "Aorta Flow"},
        {xq::NodeId(2004), "ai_analysis", "Aorta Metrics"},
    };

    CHECK(project.scene().insert(typedNode(xq::NodeId(2001), xq::XQDomainType::Mesh,
                                           "Aorta Volume Mesh"))
          == xq::XQScene::InsertResult::Inserted);
    CHECK(project.scene().insert(typedNode(xq::NodeId(2002), xq::XQDomainType::SimulationCase,
                                           "Aorta Case"))
          == xq::XQScene::InsertResult::Inserted);
    CHECK(project.scene().insert(typedNode(xq::NodeId(2003), xq::XQDomainType::FlowResult,
                                           "Aorta Flow"))
          == xq::XQScene::InsertResult::Inserted);
    CHECK(project.scene().insert(typedNode(xq::NodeId(2004), xq::XQDomainType::AiAnalysis,
                                           "Aorta Metrics"))
          == xq::XQScene::InsertResult::Inserted);

    // mesh -> case -> flow -> analysis derived chain
    CHECK(project.scene().link_derived(xq::NodeId(2001), xq::NodeId(2002))
          == xq::XQScene::RelationResult::Linked);
    CHECK(project.scene().link_derived(xq::NodeId(2002), xq::NodeId(2003))
          == xq::XQScene::RelationResult::Linked);
    CHECK(project.scene().link_derived(xq::NodeId(2003), xq::NodeId(2004))
          == xq::XQScene::RelationResult::Linked);

    const std::set<std::pair<xq::NodeId::ValueType, xq::NodeId::ValueType>> expectedRelations = {
        std::make_pair(2001, 2002),
        std::make_pair(2002, 2003),
        std::make_pair(2003, 2004),
    };

    // marking the case changed stales all downstream flow/AI nodes
    CHECK(project.scene().mark_source_changed(xq::NodeId(2002)) == 2);
    const std::set<xq::NodeId::ValueType> expectedStale = {2003, 2004};

    const std::string path = roundtripPath();
    CHECK(xq::XQProjectWriter::save(project, path) == xq::XQProjectWriter::Status::Ok);
    CHECK(project.close() == xq::XQProject::LifecycleResult::Ok);

    xq::XQProjectReadResult loaded = {};
    CHECK(xq::XQProjectReader::load(path, &loaded) == xq::XQProjectReader::Status::Ok);
    CHECK(loaded.diagnostics.empty());

    const xq::XQScene& scene = loaded.project.scene();

    // node structure (id / domain_type token / display_name) survives
    for (const ExpectedNode& e : expected) {
        if (!nodeMatches(scene.find(e.id), e)) {
            return fail("loaded new-domain node structure matches", __LINE__);
        }
    }
    // the new-domain tokens specifically
    CHECK(scene.find(xq::NodeId(2003))->domain_type()
          == xq::domainTypeToString(xq::XQDomainType::FlowResult));
    CHECK(scene.find(xq::NodeId(2004))->domain_type()
          == xq::domainTypeToString(xq::XQDomainType::AiAnalysis));

    // relations survive
    if (collectRelations(scene) != expectedRelations) {
        return fail("loaded new-domain relations match", __LINE__);
    }

    // stale flags survive
    std::set<xq::NodeId::ValueType> staleNodes;
    scene.visit_stale_nodes([&staleNodes](const xq::NodeId& node, xq::XQScene::StaleReason reason) {
        if (reason == xq::XQScene::StaleReason::SourceChanged) {
            staleNodes.insert(node.value());
        }
    });
    if (staleNodes != expectedStale) {
        return fail("loaded new-domain stale flags match", __LINE__);
    }
    CHECK(scene.stale_reason(xq::NodeId(2003)) == xq::XQScene::StaleReason::SourceChanged);
    CHECK(scene.stale_reason(xq::NodeId(2004)) == xq::XQScene::StaleReason::SourceChanged);
    CHECK(!scene.is_stale(xq::NodeId(2002)));

    std::filesystem::remove(path);
    std::printf("OK: new-domain (flow_result / ai_analysis) structural round-trip\n");
    return 0;
}
