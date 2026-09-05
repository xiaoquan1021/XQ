#include <ui/controllers/PathModuleController.h>

#include <core/XQDataNode.h>
#include <core/XQProject.h>
#include <core/XQSourcePayload.h>
#include <core/XQVesselProfilePayload.h>

#include <cstdio>
#include <memory>
#include <optional>

namespace {

int fail(const char* expression, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    return 1;
}

#define CHECK(expression)                     \
    do {                                      \
        if (!(expression)) {                  \
            return fail(#expression, __LINE__); \
        }                                     \
    } while (0)

xq::VesselProfileV1 gold_profile(const xq::NodeId& pathNode)
{
    xq::VesselProfileV1 profile;
    profile.coordinateSystem = xq::VesselProfileCoordinateSystem::LPS;
    profile.lengthUnit = xq::VesselProfileLengthUnit::Millimeter;
    profile.areaUnit = xq::VesselProfileAreaUnit::SquareMillimeter;
    profile.frameOfReferenceId = "controller-frame";
    profile.sourcePathNode = pathNode;
    profile.externalEvidenceId = "gold-id";
    profile.externalEvidenceFingerprint = "sha256:gold";
    profile.derivationStamp.algorithmId = "gold-import";
    profile.derivationStamp.algorithmVersion = "1";
    profile.derivationStamp.inputs.push_back(
        {pathNode, 0, std::nullopt, std::string()});
    for (unsigned long long i = 0; i < 3; ++i) {
        xq::VesselProfileSample sample;
        sample.sampleId = xq::VesselSampleId(i + 1);
        sample.arcLengthMm = static_cast<double>(i) * 10.0;
        sample.positionMm = {static_cast<double>(i) * 10.0, 0.0, 0.0};
        sample.unitTangent = {1.0, 0.0, 0.0};
        sample.areaMm2 = 100.0 + static_cast<double>(i);
        sample.evidenceKind = xq::VesselEvidenceKind::ImportedGold;
        sample.quality = xq::VesselSampleQuality::Accepted;
        profile.samples.push_back(sample);
    }
    return profile;
}

bool seed_profile(xq::XQProject* project,
                  const xq::NodeId& pathNode,
                  const xq::NodeId& profileNode)
{
    xq::XQDataNode path(
        pathNode, xq::XQDomainType::Path, "path",
        std::make_shared<xq::XQSourcePayload>(
            xq::XQDomainType::Path, std::string()));
    xq::XQDataNode profile(
        profileNode, xq::XQDomainType::VesselProfile, "profile",
        std::make_shared<xq::XQVesselProfilePayload>(gold_profile(pathNode)));
    profile.setContentRevision(4);
    return project->scene().insert(std::move(path))
            == xq::XQScene::InsertResult::Inserted
        && project->scene().insert(std::move(profile))
            == xq::XQScene::InsertResult::Inserted
        && project->scene().link_derived(pathNode, profileNode)
            == xq::XQScene::RelationResult::Linked;
}

std::size_t node_count(const xq::XQProject& project)
{
    std::size_t count = 0;
    project.scene().visit_nodes([&count](const xq::XQDataNode&) { ++count; });
    return count;
}

} // namespace

int main()
{
    const xq::NodeId pathNode(10);
    const xq::NodeId profileNode(20);
    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    CHECK(seed_profile(&project, pathNode, profileNode));
    const std::size_t before = node_count(project);

    xq::PathModuleController controller(&project);
    CHECK(controller.modules().size() == 2);
    xq::PathModuleController::Intent intent;
    intent.sourceProfileNode = profileNode;
    intent.moduleId = "path-validate";
    const xq::PathModuleController::Result result = controller.run(intent);
    CHECK(result.ok());
    CHECK(result.sourceKind == xq::VesselPathSourceKind::GoldFile);
    CHECK(result.module.id == "path-validate");
    CHECK(result.geometry.stationCount == 3);
    CHECK(result.geometry.canonicalDump.find("source=gold_file")
          != std::string::npos);
    CHECK(node_count(project) == before);

    intent.moduleId = "missing";
    CHECK(controller.run(intent).status
          == xq::PathModuleController::Status::UnknownModule);
    intent.moduleId = "noop";
    intent.sourceProfileNode = xq::NodeId(999);
    CHECK(controller.run(intent).status
          == xq::PathModuleController::Status::SourceNotFound);
    intent.sourceProfileNode = pathNode;
    CHECK(controller.run(intent).status
          == xq::PathModuleController::Status::SourceTypeMismatch);

    xq::XQProject staleProject;
    CHECK(staleProject.open() == xq::XQProject::LifecycleResult::Ok);
    CHECK(seed_profile(&staleProject, pathNode, profileNode));
    CHECK(staleProject.scene().mark_source_changed(pathNode) == 1);
    xq::PathModuleController staleController(&staleProject);
    intent.sourceProfileNode = profileNode;
    CHECK(staleController.run(intent).status
          == xq::PathModuleController::Status::SourceStale);

    xq::XQProject invalidPayload;
    CHECK(invalidPayload.open() == xq::XQProject::LifecycleResult::Ok);
    CHECK(invalidPayload.scene().insert(xq::XQDataNode(
              profileNode, xq::XQDomainType::VesselProfile, "bad",
              std::make_shared<xq::XQSourcePayload>(
                  xq::XQDomainType::VesselProfile, std::string())))
          == xq::XQScene::InsertResult::Inserted);
    xq::PathModuleController invalidController(&invalidPayload);
    CHECK(invalidController.run(intent).status
          == xq::PathModuleController::Status::SourcePayloadInvalid);

    xq::XQProject invalidProfile;
    CHECK(invalidProfile.open() == xq::XQProject::LifecycleResult::Ok);
    xq::VesselProfileV1 badProfile = gold_profile(pathNode);
    badProfile.samples[0].areaMm2 = 0.0;
    CHECK(invalidProfile.scene().insert(xq::XQDataNode(
              profileNode, xq::XQDomainType::VesselProfile, "invalid profile",
              std::make_shared<xq::XQVesselProfilePayload>(badProfile)))
          == xq::XQScene::InsertResult::Inserted);
    xq::PathModuleController invalidProfileController(&invalidProfile);
    const xq::PathModuleController::Result invalidProfileResult =
        invalidProfileController.run(intent);
    CHECK(invalidProfileResult.status
          == xq::PathModuleController::Status::SnapshotFailed);
    CHECK(invalidProfileResult.snapshotStatus
          == xq::VesselPathSnapshotService::Status::InvalidProfile);
    CHECK(invalidProfileResult.profileValidation.hasIssue(
        xq::VesselProfileValidationCode::NonPositiveArea));

    xq::PathModuleController nullController(nullptr);
    CHECK(nullController.run(intent).status
          == xq::PathModuleController::Status::NullContext);
    CHECK(project.close() == xq::XQProject::LifecycleResult::Ok);
    CHECK(controller.run(intent).status
          == xq::PathModuleController::Status::ProjectNotOpen);

    std::printf("Path module controller checks passed\n");
    return 0;
}
