#include "adapters/itk/ItkAutomaticVesselSegmenter.h"
#include "adapters/itk/ItkVascularPreprocessor.h"
#include "adapters/itk/VascularRoiPriorFingerprint.h"
#include "core/XQMemoryImageBufferHandle.h"
#include "core/source/ResidentVoxelSource.h"
#include "io/vascular/AutomaticVesselSegmentationArtifact.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

int failures = 0;

#define CHECK(condition, message)                                                \
    do {                                                                         \
        if (!(condition)) {                                                       \
            std::printf("FAIL: %s (%s:%d)\n", (message), __FILE__, __LINE__);    \
            ++failures;                                                           \
        }                                                                         \
    } while (0)

class CountingVoxelSource final : public xq::IVoxelSource {
public:
    explicit CountingVoxelSource(
        std::shared_ptr<const xq::XQMemoryImageBufferHandle> buffer)
        : inner_(std::move(buffer))
    {
    }

    xq::VoxelMeta meta() const override { return inner_.meta(); }

    xq::VoxelLease acquire_whole() const override
    {
        ++wholeAcquireCount_;
        return inner_.acquire_whole();
    }

    xq::VoxelLease acquire_region(const int extent[6]) const override
    {
        return inner_.acquire_region(extent);
    }

    xq::VoxelLease acquire_slab(int z) const override
    {
        return inner_.acquire_slab(z);
    }

    std::size_t wholeAcquireCount() const { return wholeAcquireCount_; }

private:
    xq::ResidentVoxelSource inner_;
    mutable std::size_t wholeAcquireCount_ = 0;
};

struct SyntheticVolume {
    xq::XQImageVolume image;
    std::shared_ptr<xq::XQMemoryImageBufferHandle> buffer;
};

double gaussianTube(double xMm,
                    double yMm,
                    double centerX,
                    double centerY,
                    double sigmaMm,
                    double amplitude)
{
    const double dx = xMm - centerX;
    const double dy = yMm - centerY;
    return amplitude * std::exp(-(dx * dx + dy * dy)
                                / (2.0 * sigmaMm * sigmaMm));
}

SyntheticVolume makeThreeTubeVolume()
{
    const int dimensions[3] = {56, 56, 48};
    const double spacing[3] = {0.8, 0.8, 1.0};

    xq::ImageGeometry geometry{};
    for (int axis = 0; axis < 3; ++axis) {
        geometry.dimensions[axis] = dimensions[axis];
        geometry.spacing[axis] = spacing[axis];
    }
    geometry.origin[0] = 18.25;
    geometry.origin[1] = -42.5;
    geometry.origin[2] = 7.75;
    const double angle = 0.29;
    const double cosine = std::cos(angle);
    const double sine = std::sin(angle);
    geometry.direction[0][0] = cosine;
    geometry.direction[0][1] = -sine;
    geometry.direction[0][2] = 0.0;
    geometry.direction[1][0] = sine;
    geometry.direction[1][1] = cosine;
    geometry.direction[1][2] = 0.0;
    geometry.direction[2][0] = 0.0;
    geometry.direction[2][1] = 0.0;
    geometry.direction[2][2] = 1.0;
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;

    const std::size_t voxelCount = static_cast<std::size_t>(dimensions[0])
        * static_cast<std::size_t>(dimensions[1])
        * static_cast<std::size_t>(dimensions[2]);
    std::vector<float> values(voxelCount, -80.0f);
    float minimum = (std::numeric_limits<float>::max)();
    float maximum = (std::numeric_limits<float>::lowest)();
    for (int z = 0; z < dimensions[2]; ++z) {
        for (int y = 0; y < dimensions[1]; ++y) {
            for (int x = 0; x < dimensions[0]; ++x) {
                const double xMm = static_cast<double>(x) * spacing[0];
                const double yMm = static_cast<double>(y) * spacing[1];
                const double zFraction = static_cast<double>(z)
                    / static_cast<double>(dimensions[2] - 1);
                const double first = gaussianTube(
                    xMm, yMm, 22.0 + 1.8 * (zFraction - 0.5), 22.0,
                    1.9, 330.0);
                const double second = gaussianTube(
                    xMm, yMm, 9.0, 33.5 - 1.2 * (zFraction - 0.5),
                    1.5, 285.0);
                const double third = gaussianTube(
                    xMm, yMm, 35.0, 10.0 + 1.5 * (zFraction - 0.5),
                    1.3, 260.0);
                const double deterministicNoise =
                    static_cast<double>((x * 17 + y * 11 + z * 5) % 9) - 4.0;
                const float value = static_cast<float>(
                    -80.0 + (std::max)({first, second, third})
                    + deterministicNoise);
                const std::size_t index = static_cast<std::size_t>(x)
                    + static_cast<std::size_t>(dimensions[0])
                        * (static_cast<std::size_t>(y)
                           + static_cast<std::size_t>(dimensions[1])
                               * static_cast<std::size_t>(z));
                values[index] = value;
                minimum = (std::min)(minimum, value);
                maximum = (std::max)(maximum, value);
            }
        }
    }

    std::vector<std::uint8_t> bytes(values.size() * sizeof(float));
    std::memcpy(bytes.data(), values.data(), bytes.size());

    SyntheticVolume result;
    result.buffer = std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::Float32, dimensions, 1, std::move(bytes));
    result.image.setGeometry(geometry);
    result.image.setScalarType(xq::ScalarType::Float32);
    result.image.setComponentCount(1);
    result.image.setIntensityRange(
        {static_cast<double>(minimum), static_cast<double>(maximum)});
    result.image.setBuffer(std::make_shared<xq::ImageBufferHandle>());
    result.image.setModality(xq::ImageModality::CT);
    result.image.setRescaleSlope(1.0);
    result.image.setRescaleIntercept(0.0);
    return result;
}

std::shared_ptr<xq::XQSegmentationMask> makeMask(
    const xq::ImageGeometry& geometry,
    const std::vector<std::uint8_t>& values)
{
    auto mask = std::make_shared<xq::XQSegmentationMask>(geometry.dimensions);
    mask->setGeometry(geometry);
    mask->setLabels({xq::SegmentationLabel{1, "roi"}});
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (values[index] != 0) {
            mask->setLabelAt(index, 1);
        }
    }
    return mask;
}

xq::XQVascularRoiLayer makeLayer(
    xq::VascularRoiRole role,
    const xq::ImageGeometry& geometry,
    const std::vector<std::uint8_t>& values,
    const char* sourceFingerprint)
{
    xq::XQVascularRoiLayer layer;
    layer.role = role;
    layer.mask = makeMask(geometry, values);
    layer.sourceGeometry = geometry;
    layer.hasSourceGeometry = true;
    layer.resampledToReference = false;
    layer.sourceForegroundVoxelCount = layer.mask->foregroundVoxelCount();
    layer.alignedForegroundVoxelCount = layer.sourceForegroundVoxelCount;
    layer.sourceFingerprint = sourceFingerprint;
    layer.alignedFingerprint = xq::itk_detail::vascularRoiAlignedFingerprint(
        role, geometry, layer.mask->voxels());
    layer.generatorId = "xq.synthetic-roi-fixture";
    layer.generatorVersion = "1";
    return layer;
}

xq::XQVascularRoiPriorV1 makeDisjointPrior(
    const xq::ImageGeometry& geometry,
    const std::string& ctFingerprint)
{
    const int dimX = geometry.dimensions[0];
    const int dimY = geometry.dimensions[1];
    const int dimZ = geometry.dimensions[2];
    const std::size_t voxelCount = static_cast<std::size_t>(dimX)
        * static_cast<std::size_t>(dimY) * static_cast<std::size_t>(dimZ);
    std::vector<std::uint8_t> coarse(voxelCount, 0);
    std::vector<std::uint8_t> organ(voxelCount, 0);

    for (int z = 0; z < dimZ; ++z) {
        const double zFraction = static_cast<double>(z)
            / static_cast<double>(dimZ - 1);
        const double centerX = 22.0 + 1.8 * (zFraction - 0.5);
        const double centerY = 22.0;
        for (int y = 0; y < dimY; ++y) {
            for (int x = 0; x < dimX; ++x) {
                const std::size_t index = static_cast<std::size_t>(x)
                    + static_cast<std::size_t>(dimX)
                        * (static_cast<std::size_t>(y)
                           + static_cast<std::size_t>(dimY)
                               * static_cast<std::size_t>(z));
                const double dx = static_cast<double>(x) * geometry.spacing[0]
                    - centerX;
                const double dy = static_cast<double>(y) * geometry.spacing[1]
                    - centerY;
                if (dx * dx + dy * dy <= 2.56) {
                    coarse[index] = 1;
                }
                if (x >= 3 && x < dimX - 3 && y >= 3 && y < dimY - 3
                    && z >= 2 && z < dimZ - 2) {
                    organ[index] = 1;
                }
            }
        }
    }
    for (std::size_t index = 0; index < voxelCount; ++index) {
        if (coarse[index] != 0) {
            organ[index] = 0;
        }
    }

    xq::XQVascularRoiPriorV1 prior;
    prior.ctInputFingerprint = ctFingerprint;
    prior.layers.push_back(makeLayer(xq::VascularRoiRole::Organ, geometry,
                                     organ, "synthetic:organ"));
    prior.layers.push_back(makeLayer(xq::VascularRoiRole::CoarseVessel,
                                     geometry, coarse, "synthetic:coarse"));
    prior.priorFingerprint = xq::itk_detail::vascularRoiPriorFingerprint(
        prior.ctInputFingerprint, prior.layers);
    return prior;
}

xq::XQVascularRoiPriorV1 makeAllForegroundOrganPrior(
    const xq::ImageGeometry& geometry,
    const xq::XQVascularRoiPriorV1& source)
{
    xq::XQVascularRoiPriorV1 prior = source;
    const std::size_t voxelCount = static_cast<std::size_t>(geometry.dimensions[0])
        * static_cast<std::size_t>(geometry.dimensions[1])
        * static_cast<std::size_t>(geometry.dimensions[2]);
    std::vector<std::uint8_t> allForeground(voxelCount, 1);
    for (xq::XQVascularRoiLayer& layer : prior.layers) {
        if (layer.role == xq::VascularRoiRole::Organ) {
            layer = makeLayer(xq::VascularRoiRole::Organ, geometry,
                              allForeground, "synthetic:all-organ");
        }
    }
    prior.priorFingerprint = xq::itk_detail::vascularRoiPriorFingerprint(
        prior.ctInputFingerprint, prior.layers);
    return prior;
}

bool sameGeometry(const xq::ImageGeometry& left, const xq::ImageGeometry& right)
{
    if (left.coordinateSystem != right.coordinateSystem) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (left.dimensions[axis] != right.dimensions[axis]
            || left.spacing[axis] != right.spacing[axis]
            || left.origin[axis] != right.origin[axis]) {
            return false;
        }
        for (int column = 0; column < 3; ++column) {
            if (left.direction[axis][column]
                != right.direction[axis][column]) {
                return false;
            }
        }
    }
    return true;
}

bool hasDiagnostic(const xq::AutomaticVesselSegmentationResult& result,
                   xq::AutomaticVesselSegmentationDiagnosticCode code)
{
    for (const xq::AutomaticVesselSegmentationDiagnostic& diagnostic
         : result.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

bool hasImplementation(const xq::XQAutomaticVesselSegmentationV2& output,
                       const char* implementation)
{
    return std::any_of(
        output.upstreamStages.begin(), output.upstreamStages.end(),
        [implementation](const xq::AutomaticVesselUpstreamStage& stage) {
            return stage.implementation == implementation;
        });
}

std::size_t directOverlap(const xq::XQVascularRoiPriorV1& prior)
{
    const xq::XQVascularRoiLayer* organ =
        prior.layer(xq::VascularRoiRole::Organ);
    const xq::XQVascularRoiLayer* coarse =
        prior.layer(xq::VascularRoiRole::CoarseVessel);
    if (organ == nullptr || coarse == nullptr) {
        return 0;
    }
    std::size_t overlap = 0;
    for (std::size_t index = 0; index < organ->mask->voxelCount(); ++index) {
        if (organ->mask->labelAt(index) != 0
            && coarse->mask->labelAt(index) != 0) {
            ++overlap;
        }
    }
    return overlap;
}

} // namespace

int main()
{
    std::printf(
        "test_itk_automatic_vessel_segmenter: ITK GAC ROI v3 chain\n");
    SyntheticVolume synthetic = makeThreeTubeVolume();
    CHECK(synthetic.buffer->is_valid(), "synthetic CT buffer is valid");
    CountingVoxelSource source(synthetic.buffer);

    xq::VascularPreprocessProfileV1 preprocessProfile =
        xq::portalVenousCtPreprocessProfileV1();
    preprocessProfile.diffusionIterations = 2;
    preprocessProfile.sigmaMinimumMm = 0.6;
    preprocessProfile.sigmaMaximumMm = 2.8;
    preprocessProfile.sigmaSteps = 4;
    xq::ItkVascularPreprocessor preprocessor;
    const xq::VascularPreprocessResult preprocessed =
        preprocessor.run(synthetic.image, source, preprocessProfile);
    CHECK(preprocessed.ok(), "real ITK vesselness preprocessing succeeds");
    CHECK(source.wholeAcquireCount() == 1,
          "preprocessor acquires the CT volume once");
    if (!preprocessed.ok()) {
        return 1;
    }

    const xq::XQVascularRoiPriorV1 prior = makeDisjointPrior(
        synthetic.image.geometry(), preprocessed.output->inputFingerprint);
    CHECK(prior.isValid(), "synthetic organ/coarse ROI prior is valid");
    CHECK(xq::itk_detail::vascularRoiPriorFingerprintsMatch(prior),
          "synthetic ROI fingerprints are self-consistent");
    CHECK(directOverlap(prior) == 0,
          "organ and coarse-vessel semantic labels may be disjoint");

    const xq::AutomaticVesselSegmentationProfileV2 profile =
        xq::portalVenousCtAutomaticSegmentationProfileV2();
    xq::ItkAutomaticVesselSegmenter segmenter;
    const xq::AutomaticVesselSegmentationResult first = segmenter.run(
        synthetic.image, source, *preprocessed.output, prior, profile);
    CHECK(first.ok(), "ROI-guided automatic segmentation produces an XQ mask");
    CHECK(source.wholeAcquireCount() == 2,
          "automatic segmenter acquires the CT volume exactly once");
    if (first.ok()) {
        const xq::XQAutomaticVesselSegmentationV2& output = *first.output;
        CHECK(output.profile.schemaVersion == 3
                  && output.tubeTkVersion.empty(),
              "schema 3 is current and carries no TubeTK identity");
        CHECK(output.domainVoxelCount > output.initialSurfaceVoxelCount
                  && output.domainVoxelCount < output.mask->voxelCount(),
              "physical ROI composition retains disjoint organ and coarse roles");
        CHECK(output.initialSurfaceVoxelCount > 0
                  && output.edgePotentialPositiveVoxelCount
                      == output.domainVoxelCount,
              "coarse mask initializes a non-empty surface and domain speed");
        CHECK(output.edgePotentialMinimum > 0.0
                  && output.edgePotentialMaximum
                      >= output.edgePotentialMinimum
                  && output.edgePotentialMaximum <= 1.0,
              "bounded-reciprocal edge potential stays in the paper range");
        CHECK(output.levelSetElapsedIterations > 0
                  && output.levelSetElapsedIterations
                      <= output.profile.maximumIterations
                  && std::isfinite(output.levelSetRmsChange),
              "ITK GAC records actual convergence diagnostics");
        CHECK(output.coarseDomainDilationRadiusVoxels[0] == 3
                  && output.coarseDomainDilationRadiusVoxels[1] == 3
                  && output.coarseDomainDilationRadiusVoxels[2] == 2,
              "2 mm physical dilation is converted per CT spacing");
        CHECK(output.componentCount == output.componentVoxelCounts.size()
                  && output.componentCount > 0,
              "ITK components are reported without XQ selection records");
        CHECK(hasImplementation(
                  output, "itk::BinaryDilateImageFilter")
                  && hasImplementation(
                      output, "itk::SignedMaurerDistanceMapImageFilter")
                  && hasImplementation(
                      output,
                      "itk::GradientMagnitudeRecursiveGaussianImageFilter")
                  && hasImplementation(
                      output, "itk::BoundedReciprocalImageFilter")
                  && hasImplementation(
                      output,
                      "itk::GeodesicActiveContourLevelSetImageFilter")
                  && hasImplementation(output, "itk::AndImageFilter")
                  && hasImplementation(
                      output, "itk::ConnectedComponentImageFilter")
                  && hasImplementation(
                      output, "itk::RelabelComponentImageFilter"),
              "provenance names the complete reused ITK GAC workflow");
        CHECK(!hasImplementation(
                  output, "itk::ConfidenceConnectedImageFilter")
                  && !hasImplementation(
                      output,
                      "itk::BinaryReconstructionByDilationImageFilter")
                  && !hasImplementation(output, "tube::SegmentTubes")
                  && !hasImplementation(output, "tube::ConvertTubesToImage"),
              "rejected confidence and TubeTK chains are absent");
        CHECK(output.foregroundVoxelCount > 0,
              "final mask is materialized from the closed zero level set");
        CHECK(output.foregroundVoxelCount <= output.domainVoxelCount,
              "final mask cannot escape the allowed ROI domain");
        CHECK(sameGeometry(output.mask->geometry(), synthetic.image.geometry()),
              "oblique LPS geometry is preserved exactly");
        CHECK(output.inputFingerprint == preprocessed.output->inputFingerprint,
              "segmentation lineage retains the CT fingerprint");
        CHECK(output.vesselnessFingerprint
                  == preprocessed.output->outputFingerprint,
              "segmentation lineage retains the vesselness fingerprint");
        CHECK(output.roiPriorFingerprint == prior.priorFingerprint,
              "segmentation lineage retains the ROI-prior fingerprint");

        const std::filesystem::path artifactPath =
            std::filesystem::temp_directory_path()
            / (std::string("xq_vascular_v3_")
               + std::to_string(reinterpret_cast<std::uintptr_t>(&source))
               + ".xqvmask");
        const std::filesystem::path partPath = artifactPath.string() + ".part";
        std::error_code cleanupError;
        std::filesystem::remove(artifactPath, cleanupError);
        cleanupError.clear();
        std::filesystem::remove(partPath, cleanupError);
        const xq::AutomaticVesselSegmentationArtifactWriteResult written =
            xq::writeAutomaticVesselSegmentationArtifact(
                artifactPath.string(), output);
        CHECK(written.ok(), "artifact v6 writes the complete v3 result");
        const xq::AutomaticVesselSegmentationArtifactReadResult reopened =
            xq::readAutomaticVesselSegmentationArtifact(artifactPath.string());
        CHECK(reopened.ok() && reopened.formatVersion == 6
                  && !reopened.isLegacy(),
              "artifact v6 reopens as the current result model");
        if (reopened.ok() && !reopened.isLegacy()) {
            CHECK(reopened.segmentation->mask->voxels()
                      == output.mask->voxels(),
                  "artifact v6 preserves the output mask bytes");
            CHECK(reopened.segmentation->roiPrior.layers.size()
                      == output.roiPrior.layers.size(),
                  "artifact v6 preserves both ROI roles");
            CHECK(reopened.segmentation->roiPrior.layers[0].mask->voxels()
                      == output.roiPrior.layers[0].mask->voxels()
                      && reopened.segmentation->roiPrior.layers[1].mask->voxels()
                          == output.roiPrior.layers[1].mask->voxels(),
                  "artifact v6 preserves aligned ROI masks");
            CHECK(reopened.segmentation->upstreamStages.size()
                      == output.upstreamStages.size(),
                  "artifact v6 preserves upstream filter provenance");
            CHECK(reopened.segmentation->levelSetElapsedIterations
                          == output.levelSetElapsedIterations
                      && reopened.segmentation->levelSetRmsChange
                          == output.levelSetRmsChange
                      && reopened.segmentation->edgePotentialMinimum
                          == output.edgePotentialMinimum
                      && reopened.segmentation->domainVoxelCount
                          == output.domainVoxelCount,
                  "artifact v6 preserves level-set and domain metadata");
        }
        const xq::AutomaticVesselSegmentationArtifactWriteResult overwrite =
            xq::writeAutomaticVesselSegmentationArtifact(
                artifactPath.string(), output);
        CHECK(overwrite.status
                  == xq::AutomaticVesselSegmentationArtifactStatus::TargetExists,
              "artifact v6 never overwrites an existing target");
        cleanupError.clear();
        std::filesystem::remove(artifactPath, cleanupError);
        cleanupError.clear();
        std::filesystem::remove(partPath, cleanupError);
    }

    const xq::AutomaticVesselSegmentationResult repeated = segmenter.run(
        synthetic.image, source, *preprocessed.output, prior, profile);
    CHECK(repeated.ok(), "repeated ROI-guided segmentation succeeds");
    CHECK(source.wholeAcquireCount() == 3,
          "each automatic segmentation run performs one CT acquire");
    if (first.ok() && repeated.ok()) {
        CHECK(first.output->profileFingerprint
                  == repeated.output->profileFingerprint,
              "v3 profile fingerprint is deterministic");
        CHECK(first.output->outputFingerprint
                  == repeated.output->outputFingerprint,
              "same inputs produce one stable v3 mask hash");
        CHECK(first.output->mask->voxels() == repeated.output->mask->voxels(),
              "same inputs materialize identical labels");
    }

    xq::AutomaticVesselSegmentationCancellation cancelled;
    cancelled.requestCancellation();
    const std::size_t acquiresBeforeEarlyFailures = source.wholeAcquireCount();
    const xq::AutomaticVesselSegmentationResult cancelledResult = segmenter.run(
        synthetic.image, source, *preprocessed.output, prior, profile,
        &cancelled);
    CHECK(cancelledResult.status
              == xq::AutomaticVesselSegmentationStatus::Cancelled
              && !cancelledResult.output.has_value(),
          "pre-cancelled v3 run returns zero half-state");
    CHECK(source.wholeAcquireCount() == acquiresBeforeEarlyFailures,
          "pre-cancelled v3 run does not acquire CT bytes");

    xq::AutomaticVesselSegmentationProfileV2 invalidProfile = profile;
    invalidProfile.edgeSigmaMm = 0.0;
    const xq::AutomaticVesselSegmentationResult invalidProfileResult =
        segmenter.run(synthetic.image, source, *preprocessed.output, prior,
                      invalidProfile);
    CHECK(invalidProfileResult.status
              == xq::AutomaticVesselSegmentationStatus::InvalidProfile
              && !invalidProfileResult.output.has_value(),
          "invalid edge sigma fails before producing a mask");
    CHECK(source.wholeAcquireCount() == acquiresBeforeEarlyFailures,
          "invalid v3 profile is rejected before CT acquisition");

    xq::AutomaticVesselSegmentationProfileV2 legacyProfile = profile;
    legacyProfile.profileId =
        "xq.portal-venous-ct.auto-segmentation.tubetk.roi.v2";
    legacyProfile.schemaVersion = 2;
    const xq::AutomaticVesselSegmentationResult legacyProfileResult =
        segmenter.run(synthetic.image, source, *preprocessed.output, prior,
                      legacyProfile);
    CHECK(legacyProfileResult.status
              == xq::AutomaticVesselSegmentationStatus::InvalidProfile
              && !legacyProfileResult.output.has_value(),
          "schema 2 remains readable but cannot execute in production");
    CHECK(source.wholeAcquireCount() == acquiresBeforeEarlyFailures,
          "schema 2 is rejected before CT acquisition");

    const xq::XQVascularRoiPriorV1 allForegroundPrior =
        makeAllForegroundOrganPrior(synthetic.image.geometry(), prior);
    const xq::AutomaticVesselSegmentationResult allForegroundResult =
        segmenter.run(synthetic.image, source, *preprocessed.output,
                      allForegroundPrior, profile);
    CHECK(allForegroundResult.status
              == xq::AutomaticVesselSegmentationStatus::InvalidRoiPrior
              && !allForegroundResult.output.has_value(),
          "all-foreground ROI is a typed failure with zero output");
    CHECK(source.wholeAcquireCount() == acquiresBeforeEarlyFailures,
          "invalid ROI is rejected before CT acquisition");

    xq::XQVesselnessVolume tamperedVesselness = *preprocessed.output;
    tamperedVesselness.outputFingerprint =
        "xq-vesselness-output-v1:sha256:tampered";
    const xq::AutomaticVesselSegmentationResult tamperedVesselnessResult =
        segmenter.run(synthetic.image, source, tamperedVesselness, prior,
                      profile);
    CHECK(tamperedVesselnessResult.status
              == xq::AutomaticVesselSegmentationStatus::LineageMismatch
              && !tamperedVesselnessResult.output.has_value()
              && hasDiagnostic(
                  tamperedVesselnessResult,
                  xq::AutomaticVesselSegmentationDiagnosticCode::VesselnessFingerprintMismatch),
          "tampered vesselness provenance cannot enter v3 production");

    xq::XQVascularRoiPriorV1 tamperedPrior = prior;
    tamperedPrior.priorFingerprint =
        "xq-vascular-roi-prior-v1:sha256:tampered";
    const xq::AutomaticVesselSegmentationResult tamperedPriorResult =
        segmenter.run(synthetic.image, source, *preprocessed.output,
                      tamperedPrior, profile);
    CHECK(tamperedPriorResult.status
              == xq::AutomaticVesselSegmentationStatus::LineageMismatch
              && !tamperedPriorResult.output.has_value()
              && hasDiagnostic(
                  tamperedPriorResult,
                  xq::AutomaticVesselSegmentationDiagnosticCode::RoiFingerprintMismatch),
          "tampered ROI provenance cannot enter v3 production");

    xq::XQImageVolume wrongFrame = synthetic.image;
    xq::ImageGeometry wrongGeometry = wrongFrame.geometry();
    wrongGeometry.origin[0] += 1.0;
    wrongFrame.setGeometry(wrongGeometry);
    const xq::AutomaticVesselSegmentationResult wrongFrameResult =
        segmenter.run(wrongFrame, source, *preprocessed.output, prior, profile);
    CHECK(wrongFrameResult.status
              == xq::AutomaticVesselSegmentationStatus::LineageMismatch
              && !wrongFrameResult.output.has_value(),
          "mismatched image/vesselness frame fails with zero output");

    if (failures == 0) {
        std::printf("test_itk_automatic_vessel_segmenter: all checks passed\n");
        return 0;
    }
    std::printf("test_itk_automatic_vessel_segmenter: %d failure(s)\n",
                failures);
    return 1;
}
