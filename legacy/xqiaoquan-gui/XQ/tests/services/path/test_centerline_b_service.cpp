#include "services/path/CenterlineBService.h"

#include "core/source/IVoxelSource.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace {

int fail(const char* expression, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    return 1;
}

#define CHECK(expression)                        \
    do {                                         \
        if (!(expression)) {                     \
            return fail(#expression, __LINE__);  \
        }                                        \
    } while (0)

class EmptyVoxelSource final : public xq::IVoxelSource {
public:
    xq::VoxelMeta meta() const override { return {}; }
    xq::VoxelLease acquire_whole() const override
    {
        return xq::VoxelLease::invalid();
    }
    xq::VoxelLease acquire_region(const int[6]) const override
    {
        return xq::VoxelLease::invalid();
    }
    xq::VoxelLease acquire_slab(int) const override
    {
        return xq::VoxelLease::invalid();
    }
};

class FakeSkeletonizer final : public xq::ICenterlineSkeletonizer3D {
public:
    xq::CenterlineSkeletonizationResult result;

    xq::CenterlineSkeletonizationResult run(
        const xq::ImageGeometry& geometry,
        const xq::IVoxelSource& source) const override
    {
        ++runCount;
        lastGeometry = &geometry;
        lastSource = &source;
        return result;
    }

    mutable std::size_t runCount = 0;
    mutable const xq::ImageGeometry* lastGeometry = nullptr;
    mutable const xq::IVoxelSource* lastSource = nullptr;
};

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

std::size_t flat(const Index& index, const xq::ImageGeometry& geometry)
{
    return static_cast<std::size_t>(index[0])
        + static_cast<std::size_t>(geometry.dimensions[0])
            * (static_cast<std::size_t>(index[1])
               + static_cast<std::size_t>(geometry.dimensions[1])
                   * static_cast<std::size_t>(index[2]));
}

xq::CenterlineSkeletonV1 skeleton(
    const std::vector<Index>& active,
    const std::vector<float>& radii)
{
    xq::CenterlineSkeletonV1 value;
    value.geometry = geometry();
    const std::size_t voxelCount =
        static_cast<std::size_t>(value.geometry.dimensions[0])
        * static_cast<std::size_t>(value.geometry.dimensions[1])
        * static_cast<std::size_t>(value.geometry.dimensions[2]);
    value.skeleton.assign(voxelCount, 0);
    value.radiusMm.assign(voxelCount, 0.0f);
    for (std::size_t index = 0; index < active.size(); ++index) {
        const std::size_t voxel = flat(active[index], value.geometry);
        value.skeleton[voxel] = 1;
        value.radiusMm[voxel] = radii[index];
    }
    value.inputForegroundVoxelCount = active.size() + 20;
    value.skeletonVoxelCount = active.size();
    value.thinningBackendId = "ITKThickness3D.BinaryThinningImageFilter3D";
    value.thinningBackendVersion =
        "v5.3.0@36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb";
    value.distanceBackendId = "itk::SignedMaurerDistanceMapImageFilter";
    value.distanceBackendVersion = "5.4.0";
    return value;
}

xq::CenterlineSkeletonizationResult successfulSkeleton(
    xq::CenterlineSkeletonV1 output)
{
    xq::CenterlineSkeletonizationResult result;
    result.status = xq::CenterlineSkeletonizationStatus::Ok;
    result.stage = xq::CenterlineSkeletonizationStage::Complete;
    result.output.emplace(std::move(output));
    return result;
}

xq::CenterlineBService::Request request()
{
    xq::CenterlineBService::Request value;
    value.sourceImageNode = xq::NodeId(1);
    value.frameOfReferenceId = "1.2.840.centerline-b.test";
    value.sourceMask.nodeId = xq::NodeId(2);
    value.sourceMask.contentRevision = 7;
    value.sourceMask.assetId = xq::AssetId(5);
    value.sourceMask.assetFingerprint = "mask-v1:sha256:test";
    value.outputPathNode = xq::NodeId(10);
    value.outputPathAsset = xq::AssetId(20);
    value.outputProfileNode = xq::NodeId(11);
    value.outputProfileAsset = xq::AssetId(21);
    value.graphProfile.shortSpurLengthMm = 0.0;
    return value;
}

bool near(double left, double right, double tolerance = 1e-10)
{
    return std::abs(left - right) <= tolerance;
}

} // namespace

int main()
{
    const std::vector<Index> line = {
        {4, 4, 2}, {4, 4, 3}, {4, 4, 4}, {4, 4, 5}, {4, 4, 6}};
    const std::vector<float> radii = {1.0f, 1.25f, 1.5f, 1.75f, 2.0f};
    FakeSkeletonizer fake;
    fake.result = successfulSkeleton(skeleton(line, radii));
    EmptyVoxelSource source;
    const xq::ImageGeometry inputGeometry = geometry();
    const xq::CenterlineBService::Request input = request();

    const xq::CenterlineBService::Result first = xq::CenterlineBService::run(
        fake, inputGeometry, source, input);
    CHECK(first.ok());
    CHECK(fake.runCount == 1);
    CHECK(fake.lastGeometry == &inputGeometry);
    CHECK(fake.lastSource == &source);
    CHECK(first.output->path.id() == input.outputPathNode);
    CHECK(first.output->path.sourceImageNode() == input.sourceImageNode);
    CHECK(first.output->path.controlPoints().size() == line.size());
    CHECK(near(first.output->path.sampleSpacing(), 0.8));
    CHECK(first.output->profile.sourcePathNode == input.outputPathNode);
    CHECK(first.output->profile.sourceEvidenceNodes.size() == 1);
    CHECK(first.output->profile.sourceEvidenceNodes.front()
          == input.sourceMask.nodeId);
    CHECK(first.output->profile.derivationStamp.inputs.size() == 2);
    CHECK(first.output->profile.derivationStamp.inputs[0].nodeId
          == input.outputPathNode);
    CHECK(first.output->profile.derivationStamp.inputs[0].assetId
          == input.outputPathAsset);
    CHECK(first.output->profile.derivationStamp.inputs[0].assetFingerprint
          == first.output->pathContentFingerprint);
    CHECK(first.output->profile.derivationStamp.inputs[1].nodeId
          == input.sourceMask.nodeId);
    CHECK(first.output->profile.samples.size() == line.size());
    for (std::size_t index = 0;
         index < first.output->profile.samples.size(); ++index) {
        const xq::VesselProfileSample& sample =
            first.output->profile.samples[index];
        CHECK(sample.sampleId.value() == index + 1);
        CHECK(near(xq::norm(sample.unitTangent), 1.0));
        CHECK(near(sample.areaMm2,
                   3.141592653589793238462643383279502884
                       * static_cast<double>(radii[index])
                       * static_cast<double>(radii[index])));
        CHECK(sample.evidenceKind
              == xq::VesselEvidenceKind::SegmentationDerived);
        CHECK(sample.sourceEvidenceNode == input.sourceMask.nodeId);
    }
    CHECK(first.output->moduleSnapshot.source.kind
          == xq::VesselPathSourceKind::AutomaticCenterlineB);
    CHECK(first.output->geometrySmoke.stationCount == line.size());
    CHECK(first.output->pathContentFingerprint.find(
              "xq-centerline-b-path-v1:sha256:") == 0);
    CHECK(first.output->profileContentFingerprint.find(
              "xq-centerline-b-profile-v1:sha256:") == 0);
    CHECK(first.output->profile.derivationStamp.parameterSummary.find(
              "neighborhood=26") != std::string::npos);
    CHECK(first.output->profile.derivationStamp.parameterSummary.find(
              "endpoint-geodesic-diameter-v1") != std::string::npos);

    const xq::CenterlineBService::Result repeated =
        xq::CenterlineBService::run(fake, inputGeometry, source, input);
    CHECK(repeated.ok());
    CHECK(repeated.output->pathContentFingerprint
          == first.output->pathContentFingerprint);
    CHECK(repeated.output->profileContentFingerprint
          == first.output->profileContentFingerprint);
    CHECK(repeated.output->geometrySmoke.canonicalDump
          == first.output->geometrySmoke.canonicalDump);

    xq::CenterlineBService::Request invalid = input;
    invalid.outputProfileNode = invalid.outputPathNode;
    const std::size_t runsBeforeInvalid = fake.runCount;
    const xq::CenterlineBService::Result invalidResult =
        xq::CenterlineBService::run(fake, inputGeometry, source, invalid);
    CHECK(invalidResult.status == xq::CenterlineBService::Status::InvalidRequest);
    CHECK(!invalidResult.output.has_value());
    CHECK(fake.runCount == runsBeforeInvalid);

    FakeSkeletonizer failedSkeletonizer;
    failedSkeletonizer.result.status =
        xq::CenterlineSkeletonizationStatus::EmptyMask;
    failedSkeletonizer.result.stage =
        xq::CenterlineSkeletonizationStage::ValidateInput;
    const xq::CenterlineBService::Result skeletonFailure =
        xq::CenterlineBService::run(
            failedSkeletonizer, inputGeometry, source, input);
    CHECK(skeletonFailure.status
          == xq::CenterlineBService::Status::SkeletonizationFailed);
    CHECK(skeletonFailure.skeletonizationStatus
          == xq::CenterlineSkeletonizationStatus::EmptyMask);
    CHECK(!skeletonFailure.output.has_value());

    FakeSkeletonizer disconnected;
    disconnected.result = successfulSkeleton(skeleton(
        {{1, 1, 1}, {2, 1, 1}, {3, 1, 1},
         {6, 6, 6}, {7, 6, 6}, {8, 6, 6}},
        {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f}));
    const xq::CenterlineBService::Result graphFailure =
        xq::CenterlineBService::run(
            disconnected, inputGeometry, source, input);
    CHECK(graphFailure.status == xq::CenterlineBService::Status::GraphFailed);
    CHECK(graphFailure.graphStatus
          == xq::CenterlineBGraphStatus::DisconnectedSkeleton);
    CHECK(!graphFailure.output.has_value());

    std::printf("Centerline B service assembly checks passed\n");
    return 0;
}
