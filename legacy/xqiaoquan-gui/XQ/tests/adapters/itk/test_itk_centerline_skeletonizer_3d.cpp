#include "adapters/itk/ItkCenterlineSkeletonizer3D.h"

#include "core/XQMemoryImageBufferHandle.h"
#include "core/source/ResidentVoxelSource.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
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

xq::ImageGeometry geometry(const int dimensions[3])
{
    xq::ImageGeometry value{};
    for (int axis = 0; axis < 3; ++axis) {
        value.dimensions[axis] = dimensions[axis];
    }
    value.spacing[0] = 0.7;
    value.spacing[1] = 1.1;
    value.spacing[2] = 1.6;
    value.origin[0] = 12.5;
    value.origin[1] = -8.25;
    value.origin[2] = 3.75;
    const double angle = 0.31;
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

std::size_t flat(int x, int y, int z, const int dimensions[3])
{
    return static_cast<std::size_t>(x)
        + static_cast<std::size_t>(dimensions[0])
            * (static_cast<std::size_t>(y)
               + static_cast<std::size_t>(dimensions[1])
                   * static_cast<std::size_t>(z));
}

std::shared_ptr<xq::XQMemoryImageBufferHandle> makeTube(
    const int dimensions[3])
{
    std::vector<std::uint8_t> voxels(
        static_cast<std::size_t>(dimensions[0])
            * static_cast<std::size_t>(dimensions[1])
            * static_cast<std::size_t>(dimensions[2]),
        0);
    const double centerX = 0.5 * static_cast<double>(dimensions[0] - 1);
    const double centerY = 0.5 * static_cast<double>(dimensions[1] - 1);
    for (int z = 3; z + 3 < dimensions[2]; ++z) {
        for (int y = 0; y < dimensions[1]; ++y) {
            for (int x = 0; x < dimensions[0]; ++x) {
                const double dx =
                    (static_cast<double>(x) - centerX) * 0.7;
                const double dy =
                    (static_cast<double>(y) - centerY) * 1.1;
                if (dx * dx + dy * dy <= 3.2 * 3.2) {
                    voxels[flat(x, y, z, dimensions)] = 1;
                }
            }
        }
    }
    return std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::UInt8, dimensions, 1, std::move(voxels));
}

bool sameGeometry(const xq::ImageGeometry& left,
                  const xq::ImageGeometry& right)
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

} // namespace

int main()
{
    std::printf(
        "test_itk_centerline_skeletonizer_3d: disclosed synthetic non-PHI tube\n");

    const int dimensions[3] = {25, 21, 23};
    const xq::ImageGeometry inputGeometry = geometry(dimensions);
    const std::shared_ptr<xq::XQMemoryImageBufferHandle> buffer =
        makeTube(dimensions);
    CHECK(buffer->is_valid(), "synthetic binary mask buffer is valid");

    CountingVoxelSource source(buffer);
    xq::ItkCenterlineSkeletonizer3D skeletonizer;
    const xq::CenterlineSkeletonizationResult first =
        skeletonizer.run(inputGeometry, source);
    CHECK(first.ok(), "real ITK 3D thinning and physical distance map succeed");
    CHECK(source.wholeAcquireCount() == 1,
          "adapter acquires the whole voxel source exactly once");
    if (first.ok()) {
        CHECK(sameGeometry(first.output->geometry, inputGeometry),
              "anisotropic oblique LPS geometry is preserved exactly");
        CHECK(first.output->inputForegroundVoxelCount
                  > first.output->skeletonVoxelCount,
              "true thinning reduces the solid tube to a skeleton");
        CHECK(first.output->thinningBackendId
                  == xq::ItkCenterlineSkeletonizer3D::thinningBackendId(),
              "output identifies the locked ITKThickness3D backend");
        CHECK(first.output->thinningBackendVersion.find(
                  "36b2c7a229be70c5b5afbba1b7d65fe26c9cbaeb")
                  != std::string::npos,
              "output records the audited thinning commit");

        double maximumSkeletonRadius = 0.0;
        std::size_t observedSkeleton = 0;
        for (std::size_t index = 0;
             index < first.output->skeleton.size(); ++index) {
            if (first.output->skeleton[index] == 0) {
                continue;
            }
            ++observedSkeleton;
            const double radius = first.output->radiusMm[index];
            CHECK(std::isfinite(radius) && radius > 0.0,
                  "every skeleton voxel has a finite positive physical radius");
            maximumSkeletonRadius =
                (std::max)(maximumSkeletonRadius, radius);
        }
        CHECK(observedSkeleton == first.output->skeletonVoxelCount,
              "materialized skeleton count matches the binary output");
        CHECK(maximumSkeletonRadius > 1.5 && maximumSkeletonRadius < 5.0,
              "spacing-aware radius follows the physical 3.2 mm tube scale");
    }

    const xq::CenterlineSkeletonizationResult repeated =
        skeletonizer.run(inputGeometry, source);
    CHECK(repeated.ok(), "repeated real adapter execution succeeds");
    CHECK(source.wholeAcquireCount() == 2,
          "each adapter run performs one whole-volume acquire");
    if (first.ok() && repeated.ok()) {
        CHECK(first.output->skeleton == repeated.output->skeleton,
              "ITK thinning output is deterministic for the same mask");
        CHECK(first.output->radiusMm == repeated.output->radiusMm,
              "physical distance-map output is deterministic");
    }

    std::vector<std::uint8_t> emptyBytes(buffer->bytes().size(), 0);
    const auto emptyBuffer = std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::UInt8, dimensions, 1, std::move(emptyBytes));
    CountingVoxelSource emptySource(emptyBuffer);
    const xq::CenterlineSkeletonizationResult empty =
        skeletonizer.run(inputGeometry, emptySource);
    CHECK(empty.status == xq::CenterlineSkeletonizationStatus::EmptyMask
              && !empty.output.has_value(),
          "empty mask returns a typed failure and no half output");

    std::vector<std::uint8_t> nonBinaryBytes(buffer->bytes());
    nonBinaryBytes.front() = 2;
    const auto nonBinaryBuffer = std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::UInt8, dimensions, 1, std::move(nonBinaryBytes));
    CountingVoxelSource nonBinarySource(nonBinaryBuffer);
    const xq::CenterlineSkeletonizationResult nonBinary =
        skeletonizer.run(inputGeometry, nonBinarySource);
    CHECK(nonBinary.status
              == xq::CenterlineSkeletonizationStatus::NonBinaryMask
              && !nonBinary.output.has_value(),
          "non-binary labels are rejected without a partial skeleton");

    xq::ImageGeometry ras = inputGeometry;
    ras.coordinateSystem = xq::ImageCoordinateSystem::RAS;
    const std::size_t acquireBeforeInvalid = source.wholeAcquireCount();
    const xq::CenterlineSkeletonizationResult invalidGeometry =
        skeletonizer.run(ras, source);
    CHECK(invalidGeometry.status
              == xq::CenterlineSkeletonizationStatus::InvalidGeometry,
          "non-LPS geometry is rejected");
    CHECK(source.wholeAcquireCount() == acquireBeforeInvalid,
          "invalid geometry is rejected before source acquisition");

    const int wrongDimensions[3] = {24, 21, 23};
    std::vector<std::uint8_t> wrongBytes(
        static_cast<std::size_t>(wrongDimensions[0])
            * static_cast<std::size_t>(wrongDimensions[1])
            * static_cast<std::size_t>(wrongDimensions[2]),
        1);
    const auto wrongBuffer = std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::UInt8, wrongDimensions, 1, std::move(wrongBytes));
    CountingVoxelSource wrongSource(wrongBuffer);
    const xq::CenterlineSkeletonizationResult wrongSourceResult =
        skeletonizer.run(inputGeometry, wrongSource);
    CHECK(wrongSourceResult.status
              == xq::CenterlineSkeletonizationStatus::InvalidSource,
          "source dimensions must match the declared geometry");
    CHECK(wrongSource.wholeAcquireCount() == 0,
          "metadata mismatch is rejected before whole acquisition");

    if (failures == 0) {
        std::printf(
            "test_itk_centerline_skeletonizer_3d: all checks passed\n");
        return 0;
    }
    std::printf("test_itk_centerline_skeletonizer_3d: %d failure(s)\n",
                failures);
    return 1;
}
