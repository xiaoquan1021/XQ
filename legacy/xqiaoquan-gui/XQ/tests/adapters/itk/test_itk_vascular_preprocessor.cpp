#include "adapters/itk/ItkVascularPreprocessor.h"
#include "core/XQMemoryImageBufferHandle.h"
#include "core/source/ResidentVoxelSource.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
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

SyntheticVolume makeGaussianTube(double spacingX,
                                 double spacingY,
                                 double spacingZ,
                                 double radiusSigmaMm,
                                 bool oblique)
{
    const double physicalX = 25.6;
    const double physicalY = 25.6;
    const double physicalZ = 19.2;
    const int dimensions[3] = {
        static_cast<int>(std::floor(physicalX / spacingX)) + 1,
        static_cast<int>(std::floor(physicalY / spacingY)) + 1,
        static_cast<int>(std::floor(physicalZ / spacingZ)) + 1
    };

    xq::ImageGeometry geometry{};
    for (int axis = 0; axis < 3; ++axis) {
        geometry.dimensions[axis] = dimensions[axis];
    }
    geometry.spacing[0] = spacingX;
    geometry.spacing[1] = spacingY;
    geometry.spacing[2] = spacingZ;
    geometry.origin[0] = 13.25;
    geometry.origin[1] = -27.5;
    geometry.origin[2] = 4.75;
    const double angle = oblique ? 0.37 : 0.0;
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
    std::vector<float> values(voxelCount, 0.0f);
    const double centerX = 0.5 * static_cast<double>(dimensions[0] - 1) * spacingX;
    const double centerY = 0.5 * static_cast<double>(dimensions[1] - 1) * spacingY;
    float minimum = (std::numeric_limits<float>::max)();
    float maximum = (std::numeric_limits<float>::lowest)();
    for (int z = 0; z < dimensions[2]; ++z) {
        for (int y = 0; y < dimensions[1]; ++y) {
            for (int x = 0; x < dimensions[0]; ++x) {
                const double dx = static_cast<double>(x) * spacingX - centerX;
                const double dy = static_cast<double>(y) * spacingY - centerY;
                const double radialSquared = dx * dx + dy * dy;
                const double vessel = 450.0
                    * std::exp(-radialSquared
                               / (2.0 * radiusSigmaMm * radiusSigmaMm));
                const float value = static_cast<float>(-80.0 + vessel);
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
            if (left.direction[axis][column] != right.direction[axis][column]) {
                return false;
            }
        }
    }
    return true;
}

bool hasDiagnostic(const xq::VascularPreprocessResult& result,
                   xq::VascularPreprocessDiagnosticCode code)
{
    for (const xq::VascularPreprocessDiagnostic& diagnostic : result.diagnostics) {
        if (diagnostic.code == code) {
            return true;
        }
    }
    return false;
}

void verifyWorldRoundTrip(const xq::XQImageVolume& image)
{
    const xq::ImageGeometry& geometry = image.geometry();
    const double index[3] = {
        0.37 * static_cast<double>(geometry.dimensions[0] - 1),
        0.61 * static_cast<double>(geometry.dimensions[1] - 1),
        0.43 * static_cast<double>(geometry.dimensions[2] - 1)
    };
    double world[3] = {};
    double roundTrip[3] = {};
    CHECK(image.voxelToWorld(index, world)
              == xq::XQImageVolume::TransformStatus::Ok,
          "output index converts to LPS world");
    CHECK(image.worldToVoxel(world, roundTrip)
              == xq::XQImageVolume::TransformStatus::Ok,
          "output LPS world converts back to index");
    for (int axis = 0; axis < 3; ++axis) {
        CHECK(std::abs(roundTrip[axis] - index[axis]) < 1e-9,
              "output index/LPS round trip is conserved");
    }
}

} // namespace

int main()
{
    std::printf("test_itk_vascular_preprocessor: real 3D ITK filter path\n");
    xq::ItkVascularPreprocessor preprocessor;
    xq::VascularPreprocessProfileV1 profile =
        xq::portalVenousCtPreprocessProfileV1();
    profile.diffusionIterations = 2;
    profile.sigmaMinimumMm = 0.6;
    profile.sigmaMaximumMm = 2.4;
    profile.sigmaSteps = 4;

    SyntheticVolume oblique = makeGaussianTube(0.8, 1.0, 1.2, 1.8, true);
    CHECK(oblique.buffer->is_valid(), "synthetic XQ scalar buffer is valid");
    CountingVoxelSource source(oblique.buffer);
    const xq::VascularPreprocessResult first =
        preprocessor.run(oblique.image, source, profile);
    CHECK(first.ok(), "3D diffusion and vesselness produce a complete result");
    CHECK(source.wholeAcquireCount() == 1,
          "preprocessor acquires the whole IVoxelSource exactly once");
    if (first.ok()) {
        CHECK(sameGeometry(first.output->image.geometry(), oblique.image.geometry()),
              "oblique LPS geometry is preserved exactly");
        CHECK(first.output->positiveVoxelCount > 0,
              "vesselness contains positive tubular response");
        CHECK(first.output->scalarMaximum > first.output->scalarMinimum,
              "vesselness has a non-degenerate scalar range");
        CHECK(first.output->profile.sigmaMinimumMm == profile.sigmaMinimumMm
                  && first.output->profile.sigmaMaximumMm == profile.sigmaMaximumMm,
              "physical sigma profile is retained in provenance");
        CHECK(first.output->algorithmId
                  == xq::ItkVascularPreprocessor::algorithmId(),
              "result identifies the ITK production algorithm");
        CHECK(!first.output->itkVersion.empty(), "result records the ITK version");
        verifyWorldRoundTrip(first.output->image);
    }

    const xq::VascularPreprocessResult repeated =
        preprocessor.run(oblique.image, source, profile);
    CHECK(repeated.ok(), "repeated 3D preprocessing succeeds");
    CHECK(source.wholeAcquireCount() == 2,
          "each preprocessing run performs one whole-volume acquire");
    if (first.ok() && repeated.ok()) {
        CHECK(first.output->inputFingerprint == repeated.output->inputFingerprint,
              "same input has a stable fingerprint");
        CHECK(first.output->profileFingerprint == repeated.output->profileFingerprint,
              "same physical profile has a stable fingerprint");
        CHECK(first.output->outputFingerprint == repeated.output->outputFingerprint,
              "same input/profile has a deterministic vesselness fingerprint");
        CHECK(first.output->buffer->bytes() == repeated.output->buffer->bytes(),
              "same input/profile materializes identical float voxels");
    }

    xq::VascularPreprocessCancellation cancelled;
    cancelled.requestCancellation();
    const std::size_t acquiresBeforeCancel = source.wholeAcquireCount();
    const xq::VascularPreprocessResult cancelledResult =
        preprocessor.run(oblique.image, source, profile, &cancelled);
    CHECK(cancelledResult.status == xq::VascularPreprocessStatus::Cancelled
              && !cancelledResult.output.has_value(),
          "pre-cancelled run returns no partial output");
    CHECK(source.wholeAcquireCount() == acquiresBeforeCancel,
          "pre-cancelled run does not acquire voxel bytes");

    xq::VascularPreprocessProfileV1 invalidProfile = profile;
    invalidProfile.diffusionTimeStep = 0.1;
    const xq::VascularPreprocessResult invalidProfileResult =
        preprocessor.run(oblique.image, source, invalidProfile);
    CHECK(invalidProfileResult.status == xq::VascularPreprocessStatus::InvalidProfile
              && !invalidProfileResult.output.has_value(),
          "invalid diffusion profile fails with zero half result");
    CHECK(source.wholeAcquireCount() == acquiresBeforeCancel,
          "invalid profile is rejected before voxel acquisition");

    SyntheticVolume coarse = makeGaussianTube(1.0, 1.0, 1.2, 1.8, false);
    CountingVoxelSource coarseSource(coarse.buffer);
    const xq::VascularPreprocessResult coarseResult =
        preprocessor.run(coarse.image, coarseSource, profile);
    CHECK(coarseResult.ok(), "same physical tube on a second spacing succeeds");
    if (first.ok() && coarseResult.ok()) {
        const double ratio = first.output->scalarMaximum
            / coarseResult.output->scalarMaximum;
        std::printf("physical_spacing.max_response_ratio=%.9g\n", ratio);
        CHECK(ratio > 0.25 && ratio < 4.0,
              "millimetre sigma gives a consistent physical-scale response");
    }

    SyntheticVolume nonFinite = makeGaussianTube(1.0, 1.0, 1.2, 1.8, false);
    std::vector<std::uint8_t> badBytes = nonFinite.buffer->bytes();
    const float nan = (std::numeric_limits<float>::quiet_NaN)();
    std::memcpy(badBytes.data(), &nan, sizeof(nan));
    const int* badDims = nonFinite.image.geometry().dimensions;
    nonFinite.buffer = std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::Float32, badDims, 1, std::move(badBytes));
    CountingVoxelSource nonFiniteSource(nonFinite.buffer);
    const xq::VascularPreprocessResult nonFiniteResult =
        preprocessor.run(nonFinite.image, nonFiniteSource, profile);
    CHECK(nonFiniteResult.status == xq::VascularPreprocessStatus::UnsupportedInput
              && !nonFiniteResult.output.has_value()
              && hasDiagnostic(nonFiniteResult,
                               xq::VascularPreprocessDiagnosticCode::InputScalarNonFinite),
          "non-finite source scalar is rejected without a partial vesselness");

    if (failures == 0) {
        std::printf("test_itk_vascular_preprocessor: all functional checks passed\n");
        return 0;
    }
    std::printf("test_itk_vascular_preprocessor: %d failure(s)\n", failures);
    return 1;
}
