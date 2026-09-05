#include "ui/controllers/CenterlineBController.h"

#include "core/XQDataNode.h"
#include "core/XQImageVolumePayload.h"
#include "core/XQPathPayload.h"
#include "core/XQProject.h"
#include "core/XQSegmentationMask.h"
#include "core/XQSegmentationMaskPayload.h"
#include "core/XQVesselProfilePayload.h"
#include "core/command/XQCommandStack.h"
#include "core/path/ICenterlineSkeletonizer3D.h"
#include "core/source/IVoxelSource.h"
#include "io/project/XQProjectReader.h"
#include "io/project/XQProjectWriter.h"
#include "ui/controllers/PathModuleController.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

const xq::NodeId kImageNode(1);
const xq::NodeId kMaskNode(2);
const xq::NodeId kPathNode(10);
const xq::NodeId kProfileNode(11);
const xq::AssetId kImageAsset(1);
const xq::AssetId kMaskAsset(2);
const xq::AssetId kPathAsset(3);
const xq::AssetId kProfileAsset(4);

int fail(const char* expression, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    return 1;
}

#define CHECK(expression)                       \
    do {                                        \
        if (!(expression)) {                    \
            return fail(#expression, __LINE__); \
        }                                       \
    } while (false)

using Index = std::array<int, 3>;

xq::ImageGeometry geometry()
{
    xq::ImageGeometry value{};
    value.dimensions[0] = 9;
    value.dimensions[1] = 9;
    value.dimensions[2] = 9;
    value.spacing[0] = 0.8;
    value.spacing[1] = 1.1;
    value.spacing[2] = 1.5;
    value.origin[0] = 10.0;
    value.origin[1] = -3.0;
    value.origin[2] = 7.0;
    const double angle = 0.25;
    value.direction[0][0] = std::cos(angle);
    value.direction[0][1] = -std::sin(angle);
    value.direction[0][2] = 0.0;
    value.direction[1][0] = std::sin(angle);
    value.direction[1][1] = std::cos(angle);
    value.direction[1][2] = 0.0;
    value.direction[2][0] = 0.0;
    value.direction[2][1] = 0.0;
    value.direction[2][2] = 1.0;
    value.coordinateSystem = xq::ImageCoordinateSystem::LPS;
    return value;
}

std::size_t flat(const Index& index, const xq::ImageGeometry& value)
{
    return static_cast<std::size_t>(index[0])
        + static_cast<std::size_t>(value.dimensions[0])
            * (static_cast<std::size_t>(index[1])
               + static_cast<std::size_t>(value.dimensions[1])
                   * static_cast<std::size_t>(index[2]));
}

const std::vector<Index>& centerlineVoxels()
{
    static const std::vector<Index> value = {
        {4, 4, 2}, {4, 4, 3}, {4, 4, 4}, {4, 4, 5}, {4, 4, 6}};
    return value;
}

xq::CenterlineSkeletonizationResult successfulSkeleton()
{
    xq::CenterlineSkeletonV1 output;
    output.geometry = geometry();
    const std::size_t voxelCount =
        static_cast<std::size_t>(output.geometry.dimensions[0])
        * static_cast<std::size_t>(output.geometry.dimensions[1])
        * static_cast<std::size_t>(output.geometry.dimensions[2]);
    output.skeleton.assign(voxelCount, 0);
    output.radiusMm.assign(voxelCount, 0.0f);
    const float radii[] = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
    for (std::size_t index = 0; index < centerlineVoxels().size(); ++index) {
        const std::size_t voxel = flat(centerlineVoxels()[index], output.geometry);
        output.skeleton[voxel] = 1;
        output.radiusMm[voxel] = radii[index];
    }
    output.inputForegroundVoxelCount = centerlineVoxels().size() + 20;
    output.skeletonVoxelCount = centerlineVoxels().size();
    output.thinningBackendId =
        "ITKThickness3D.BinaryThinningImageFilter3D";
    output.thinningBackendVersion =
        "v5.3.0@36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb";
    output.distanceBackendId =
        "itk::SignedMaurerDistanceMapImageFilter";
    output.distanceBackendVersion = "5.4.0";

    xq::CenterlineSkeletonizationResult result;
    result.status = xq::CenterlineSkeletonizationStatus::Ok;
    result.stage = xq::CenterlineSkeletonizationStage::Complete;
    result.output.emplace(std::move(output));
    return result;
}

class FakeSkeletonizer final : public xq::ICenterlineSkeletonizer3D {
public:
    xq::CenterlineSkeletonizationResult result = successfulSkeleton();

    xq::CenterlineSkeletonizationResult run(
        const xq::ImageGeometry& inputGeometry,
        const xq::IVoxelSource& source) const override
    {
        ++runCount;
        const xq::VoxelMeta meta = source.meta();
        xq::VoxelLease lease = source.acquire_whole();
        sawCopiedMask = meta.valid && meta.type == xq::ScalarType::UInt8
            && meta.components == 1
            && meta.voxelCount
                == static_cast<std::size_t>(inputGeometry.dimensions[0])
                    * static_cast<std::size_t>(inputGeometry.dimensions[1])
                    * static_cast<std::size_t>(inputGeometry.dimensions[2])
            && lease.view().valid
            && lease.view().bytes.size() == meta.voxelCount;
        if (sawCopiedMask) {
            for (const Index& index : centerlineVoxels()) {
                if (lease.view().scalarAt(flat(index, inputGeometry)) != 1.0) {
                    sawCopiedMask = false;
                    break;
                }
            }
        }
        return result;
    }

    mutable std::size_t runCount = 0;
    mutable bool sawCopiedMask = false;
};

xq::AssetRecord asset(
    xq::AssetId id,
    xq::AssetCategory category,
    xq::AssetKind kind,
    const std::string& fingerprint)
{
    xq::AssetRecord value;
    value.id = id;
    value.category = category;
    value.kind = kind;
    value.contentFingerprint = fingerprint;
    return value;
}

std::size_t nodeCount(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes([&count](const xq::XQDataNode&) { ++count; });
    return count;
}

std::size_t relationCount(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_derived_relations(
        [&count](const xq::NodeId&, const xq::NodeId&) { ++count; });
    return count;
}

struct Fixture {
    xq::XQProject project;
    xq::XQCommandStack stack;
    bool ready = false;
    bool bindMaskAsset = true;

    explicit Fixture(bool bindMaskAssetValue = true)
        : bindMaskAsset(bindMaskAssetValue)
    {
        ready = project.open() == xq::XQProject::LifecycleResult::Ok
            && seed();
    }

    bool seed()
    {
        xq::XQImageVolume image;
        image.setGeometry(geometry());
        image.setScalarType(xq::ScalarType::Int16);
        image.setComponentCount(1);
        xq::DicomSeriesIdentity dicom;
        dicom.studyInstanceUid = "1.2.840.centerline-b.study";
        dicom.seriesInstanceUid = "1.2.840.centerline-b.series";
        dicom.frameOfReferenceUid = "1.2.840.centerline-b.frame";
        image.setDicomIdentity(dicom);

        xq::AssetRecord imageAsset = asset(
            kImageAsset,
            xq::AssetCategory::ExternalSource,
            xq::AssetKind::Image,
            "image-v1:sha256:fixture");
        imageAsset.hasDicom = true;
        imageAsset.dicom = dicom;
        imageAsset.hasGeometry = true;
        imageAsset.geometry = geometry();
        imageAsset.sourceAbsPath = "C:/non-phi/centerline-b-fixture";
        if (!project.assetRegistry().registerAsset(imageAsset)) {
            return false;
        }

        xq::XQDataNode imageNode(
            kImageNode,
            xq::XQDomainType::Image,
            "Source image",
            std::make_shared<xq::XQImageVolumePayload>(image));
        imageNode.setScaleSlot(xq::ScaleSlot::Organ);
        imageNode.setAssetId(kImageAsset);
        if (project.scene().insert(std::move(imageNode))
            != xq::XQScene::InsertResult::Inserted) {
            return false;
        }

        const xq::ImageGeometry valueGeometry = geometry();
        xq::XQSegmentationMask mask(valueGeometry.dimensions);
        mask.setGeometry(valueGeometry);
        mask.setSourceImageNode(kImageNode);
        for (const Index& index : centerlineVoxels()) {
            mask.setLabelAt(flat(index, valueGeometry), 1);
        }
        xq::XQDataNode maskNode(
            kMaskNode,
            xq::XQDomainType::SegmentationMask,
            "Vessel mask",
            std::make_shared<xq::XQSegmentationMaskPayload>(std::move(mask)));
        // Deliberately leave the mask ScaleSlot absent. The DICOM image is the
        // authoritative Organ-scale source and current segmentation producers
        // do not fabricate a second scale value on the mask.
        if (bindMaskAsset) {
            if (!project.assetRegistry().registerAsset(asset(
                    kMaskAsset,
                    xq::AssetCategory::Derived,
                    xq::AssetKind::SegmentationMask,
                    "mask-v1:sha256:fixture"))) {
                return false;
            }
            maskNode.setAssetId(kMaskAsset);
        }
        if (project.scene().insert(std::move(maskNode))
                != xq::XQScene::InsertResult::Inserted
            || project.scene().link_derived(kImageNode, kMaskNode)
                != xq::XQScene::RelationResult::Linked) {
            return false;
        }
        if (bindMaskAsset
            && !project.assetRegistry().addRelation(kImageAsset, kMaskAsset)) {
            return false;
        }
        return true;
    }
};

xq::CenterlineBController::Intent intent()
{
    xq::CenterlineBController::Intent value;
    value.sourceMaskNode = kMaskNode;
    value.output.pathNode = kPathNode;
    value.output.pathName = "Centerline B Path";
    value.output.profileNode = kProfileNode;
    value.output.profileName = "Centerline B Profile";
    value.graphProfile.shortSpurLengthMm = 0.0;
    return value;
}

bool hasPrefix(const std::string& value, const std::string& prefix)
{
    return value.compare(0, prefix.size(), prefix) == 0;
}

} // namespace

int main()
{
    Fixture fixture;
    CHECK(fixture.ready);
    FakeSkeletonizer fake;
    xq::CenterlineBController controller(
        &fixture.project, &fixture.stack, &fake);
    CHECK(controller.run(intent()) == xq::CenterlineBController::Status::Ok);
    CHECK(fake.runCount == 1 && fake.sawCopiedMask);
    CHECK(nodeCount(fixture.project.scene()) == 4);
    CHECK(relationCount(fixture.project.scene()) == 4);
    CHECK(fixture.stack.undo_count() == 1);

    const xq::XQDataNode* pathNode = fixture.project.scene().find(kPathNode);
    const xq::XQDataNode* profileNode = fixture.project.scene().find(kProfileNode);
    CHECK(pathNode != nullptr && profileNode != nullptr);
    CHECK(pathNode->scaleSlot() == xq::ScaleSlot::Organ);
    CHECK(profileNode->scaleSlot() == xq::ScaleSlot::Organ);
    CHECK(pathNode->hasAssetId() && pathNode->assetId() == kPathAsset);
    CHECK(profileNode->hasAssetId()
          && profileNode->assetId() == kProfileAsset);
    const std::shared_ptr<xq::XQPathPayload> pathPayload =
        std::dynamic_pointer_cast<xq::XQPathPayload>(pathNode->payload());
    const std::shared_ptr<xq::XQVesselProfilePayload> profilePayload =
        std::dynamic_pointer_cast<xq::XQVesselProfilePayload>(
            profileNode->payload());
    CHECK(pathPayload != nullptr && profilePayload != nullptr);
    CHECK(pathPayload->path().sourceImageNode() == kImageNode);
    CHECK(profilePayload->profile().sourcePathNode == kPathNode);
    CHECK(profilePayload->profile().sourceEvidenceNodes.size() == 1);
    CHECK(profilePayload->profile().sourceEvidenceNodes.front() == kMaskNode);

    const xq::AssetRecord* pathAsset =
        fixture.project.assetRegistry().find(kPathAsset);
    const xq::AssetRecord* profileAsset =
        fixture.project.assetRegistry().find(kProfileAsset);
    CHECK(pathAsset != nullptr && profileAsset != nullptr);
    CHECK(pathAsset->kind == xq::AssetKind::Path);
    CHECK(profileAsset->kind == xq::AssetKind::VesselProfile);
    CHECK(hasPrefix(pathAsset->contentFingerprint,
                    "xq-centerline-b-path-v1:sha256:"));
    CHECK(hasPrefix(profileAsset->contentFingerprint,
                    "xq-centerline-b-profile-v1:sha256:"));
    CHECK(fixture.project.assetRegistry().assetCount() == 4);
    CHECK(fixture.project.assetRegistry().relationCount() == 4);
    CHECK(fixture.project.assetRegistry().hasRelation(kMaskAsset, kPathAsset));
    CHECK(fixture.project.assetRegistry().hasRelation(kMaskAsset, kProfileAsset));
    CHECK(fixture.project.assetRegistry().hasRelation(kPathAsset, kProfileAsset));
    CHECK(fixture.project.assetRegistry().nextAvailableAssetId()
          == xq::AssetId(5));

    xq::PathModuleController modules(&fixture.project);
    xq::PathModuleController::Intent moduleIntent;
    moduleIntent.sourceProfileNode = kProfileNode;
    moduleIntent.moduleId = "path-validate";
    const xq::PathModuleController::Result moduleResult =
        modules.run(moduleIntent);
    CHECK(moduleResult.ok());
    CHECK(moduleResult.sourceKind
          == xq::VesselPathSourceKind::AutomaticCenterlineB);

    CHECK(fixture.stack.undo());
    CHECK(fixture.project.scene().find(kPathNode) == nullptr);
    CHECK(fixture.project.scene().find(kProfileNode) == nullptr);
    CHECK(nodeCount(fixture.project.scene()) == 2);
    CHECK(relationCount(fixture.project.scene()) == 1);
    CHECK(fixture.project.assetRegistry().assetCount() == 2);
    CHECK(fixture.project.assetRegistry().relationCount() == 1);
    CHECK(fixture.stack.redo());
    CHECK(fixture.project.scene().find(kPathNode) != nullptr);
    CHECK(fixture.project.scene().find(kProfileNode) != nullptr);

    // Current in-memory segmentation producers publish a real Mask node before
    // it has an Asset binding. The existing writer materializes that payload on
    // save; Centerline B must remain reopenable through that normal path too.
    Fixture persistence(false);
    CHECK(persistence.ready);
    FakeSkeletonizer persistenceFake;
    xq::CenterlineBController persistenceController(
        &persistence.project, &persistence.stack, &persistenceFake);
    CHECK(persistenceController.run(intent())
          == xq::CenterlineBController::Status::Ok);
    const xq::XQDataNode* persistedPath =
        persistence.project.scene().find(kPathNode);
    const xq::XQDataNode* persistedProfile =
        persistence.project.scene().find(kProfileNode);
    CHECK(persistedPath != nullptr && persistedProfile != nullptr);
    CHECK(persistedPath->hasAssetId() && persistedProfile->hasAssetId());
    const xq::AssetId persistedPathAsset = persistedPath->assetId();
    const xq::AssetId persistedProfileAsset = persistedProfile->assetId();

    const std::filesystem::path projectPath =
        std::filesystem::temp_directory_path()
        / "xq_centerline_b_controller_roundtrip.xqproj";
    const std::filesystem::path assetPath =
        projectPath.parent_path()
        / (projectPath.stem().string() + ".assets");
    std::error_code ec;
    std::filesystem::remove(projectPath, ec);
    std::filesystem::remove_all(assetPath, ec);
    CHECK(xq::XQProjectWriter::save(persistence.project, projectPath.string())
          == xq::XQProjectWriter::Status::Ok);
    xq::XQProjectReadResult loaded;
    CHECK(xq::XQProjectReader::load(projectPath.string(), &loaded)
          == xq::XQProjectReader::Status::Ok);
    const xq::XQDataNode* loadedPath = loaded.project.scene().find(kPathNode);
    const xq::XQDataNode* loadedProfile =
        loaded.project.scene().find(kProfileNode);
    CHECK(loadedPath != nullptr && loadedProfile != nullptr);
    CHECK(loadedPath->hasAssetId()
          && loadedPath->assetId() == persistedPathAsset);
    CHECK(loadedProfile->hasAssetId()
          && loadedProfile->assetId() == persistedProfileAsset);
    CHECK(loaded.project.assetRegistry().hasRelation(
        persistedPathAsset, persistedProfileAsset));
    xq::PathModuleController loadedModules(&loaded.project);
    CHECK(loadedModules.run(moduleIntent).ok());
    std::filesystem::remove(projectPath, ec);
    std::filesystem::remove_all(assetPath, ec);

    CHECK(fixture.project.scene().mark_source_changed(kImageNode) == 3);
    CHECK(fixture.project.scene().is_stale(kMaskNode));
    CHECK(fixture.project.scene().is_stale(kPathNode));
    CHECK(fixture.project.scene().is_stale(kProfileNode));

    Fixture changed;
    CHECK(changed.ready);
    FakeSkeletonizer changedFake;
    xq::CenterlineBController changedController(
        &changed.project, &changed.stack, &changedFake);
    xq::CenterlineBController::PreparedCommand prepared =
        changedController.prepare(intent());
    CHECK(prepared.ok());
    xq::XQDataNode* changedMask = changed.project.scene().find(kMaskNode);
    CHECK(changedMask != nullptr);
    const std::shared_ptr<xq::XQSegmentationMaskPayload> oldMask =
        std::dynamic_pointer_cast<xq::XQSegmentationMaskPayload>(
            changedMask->payload());
    CHECK(oldMask != nullptr);
    changedMask->setPayload(
        xq::XQDomainType::SegmentationMask,
        std::make_shared<xq::XQSegmentationMaskPayload>(oldMask->mask()));
    CHECK(changedController.commitPrepared(std::move(prepared))
          == xq::CenterlineBController::Status::SourceChanged);
    CHECK(changed.project.scene().find(kPathNode) == nullptr);
    CHECK(changed.project.scene().find(kProfileNode) == nullptr);
    CHECK(changed.stack.undo_count() == 0);

    Fixture assetCollision;
    CHECK(assetCollision.ready);
    FakeSkeletonizer assetCollisionFake;
    xq::CenterlineBController assetCollisionController(
        &assetCollision.project, &assetCollision.stack, &assetCollisionFake);
    xq::CenterlineBController::PreparedCommand assetPrepared =
        assetCollisionController.prepare(intent());
    CHECK(assetPrepared.ok());
    CHECK(assetCollision.project.assetRegistry().registerAsset(asset(
        kPathAsset,
        xq::AssetCategory::Derived,
        xq::AssetKind::Path,
        "sha256:blocker")));
    CHECK(assetCollisionController.commitPrepared(std::move(assetPrepared))
          == xq::CenterlineBController::Status::SourceChanged);
    CHECK(assetCollision.project.scene().find(kPathNode) == nullptr);
    CHECK(assetCollision.project.scene().find(kProfileNode) == nullptr);
    CHECK(assetCollision.stack.undo_count() == 0);

    Fixture failed;
    CHECK(failed.ready);
    FakeSkeletonizer failedFake;
    failedFake.result = {};
    failedFake.result.status = xq::CenterlineSkeletonizationStatus::EmptyMask;
    xq::CenterlineBController failedController(
        &failed.project, &failed.stack, &failedFake);
    const xq::CenterlineBController::PreparedCommand failedPrepared =
        failedController.prepare(intent());
    CHECK(failedPrepared.status
          == xq::CenterlineBController::Status::ComputeFailed);
    CHECK(failedPrepared.skeletonizationStatus
          == xq::CenterlineSkeletonizationStatus::EmptyMask);
    CHECK(failed.project.scene().find(kPathNode) == nullptr);
    CHECK(failed.project.scene().find(kProfileNode) == nullptr);
    CHECK(failed.project.assetRegistry().assetCount() == 2);
    CHECK(failed.stack.undo_count() == 0);

    Fixture unsupportedScale;
    CHECK(unsupportedScale.ready);
    unsupportedScale.project.scene().find(kImageNode)->setScaleSlot(
        xq::ScaleSlot::Micro);
    FakeSkeletonizer unsupportedFake;
    xq::CenterlineBController unsupportedController(
        &unsupportedScale.project, &unsupportedScale.stack, &unsupportedFake);
    CHECK(unsupportedController.capture(intent()).status
          == xq::CenterlineBController::Status::UnsupportedScale);
    CHECK(unsupportedFake.runCount == 0);

    Fixture lifecycle;
    CHECK(lifecycle.ready);
    FakeSkeletonizer lifecycleFake;
    xq::CenterlineBController lifecycleController(
        &lifecycle.project, &lifecycle.stack, &lifecycleFake);
    xq::CenterlineBController::PreparedCommand lifecyclePrepared =
        lifecycleController.prepare(intent());
    CHECK(lifecyclePrepared.ok());
    CHECK(lifecycle.project.close() == xq::XQProject::LifecycleResult::Ok);
    CHECK(lifecycle.project.reopen() == xq::XQProject::LifecycleResult::Ok);
    CHECK(lifecycle.seed());
    CHECK(lifecycleController.commitPrepared(std::move(lifecyclePrepared))
          == xq::CenterlineBController::Status::SourceChanged);
    CHECK(lifecycle.stack.undo_count() == 0);

    std::printf("Centerline B controller checks passed\n");
    return 0;
}
