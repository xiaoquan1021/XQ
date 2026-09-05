#include <adapters/vtk/VtkImageAdapter.h>
#include <core/NodeId.h>
#include <core/XQImageVolume.h>
#include <core/XQMemoryImageBufferHandle.h>
#include <core/XQSegmentationMask.h>
#include <services/segmentation/SegmentationService.h>

#include <cstddef>
#include <cstdio>
#include <memory>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

} // namespace

// Explicit-failure checks (no assert): survive Release /DNDEBUG. Calls with side
// effects are evaluated into a variable before being checked.
#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

int main()
{
    // Decode the real 0007 image into geometry + a real scalar buffer.
    xq::XQImageVolume image;
    std::shared_ptr<xq::XQMemoryImageBufferHandle> buffer;
    const xq::VtkImageAdapter::LoadStatus status =
        xq::VtkImageAdapter::loadVtiWithBuffer(XQ_TEST_VTI_PATH, &image, &buffer);
    CHECK(status == xq::VtkImageAdapter::LoadStatus::Ok);
    CHECK(buffer != nullptr);
    CHECK(buffer->is_valid());

    // 0007 is single-component Int16, extent 0..99 x 0..511 x 0..511.
    CHECK(image.componentCount() == 1);
    CHECK(buffer->scalarType() == xq::ScalarType::Int16);
    CHECK(buffer->dimensionX() == 100);
    CHECK(buffer->dimensionY() == 512);
    CHECK(buffer->dimensionZ() == 512);
    const std::size_t expectedVoxels =
        static_cast<std::size_t>(100) * 512 * 512;
    CHECK(buffer->voxelCount() == expectedVoxels);

    const xq::NodeId image_id(1);

    // Threshold a mid-intensity band. The 0007 intensity range is [0, 3297];
    // [500, 3297] selects the brighter (vessel/contrast) voxels: a non-empty,
    // non-full mask.
    {
        xq::XQSegmentationThresholdParameters params;
        params.lower = 500.0;
        params.upper = 3297.0;
        params.foregroundLabel = 1;

        const xq::SegmentationService::Result result =
            xq::SegmentationService::thresholdMask(image, *buffer, params, image_id);
        CHECK(result.ok());
        CHECK(result.mask != nullptr);

        const std::size_t foreground = result.mask->foregroundVoxelCount();
        CHECK(foreground > 0);                 // non-empty
        CHECK(foreground < expectedVoxels);    // not the whole volume
        CHECK(result.mask->hasSourceImageNode());
        CHECK(result.mask->sourceImageNode() == image_id);

        // keepLargestConnectedComponent on the real mask: still non-empty and no
        // larger than the threshold result.
        const xq::SegmentationService::Result largest =
            xq::SegmentationService::keepLargestConnectedComponent(*result.mask);
        CHECK(largest.ok());
        const std::size_t largestCount = largest.mask->foregroundVoxelCount();
        CHECK(largestCount > 0);
        CHECK(largestCount <= foreground);
        CHECK(largest.mask->sourceImageNode() == image_id);
    }

    // Region grow from a seed known to be inside a bright region. We pick the
    // brightest voxel in the volume as the seed so the seed satisfies the
    // threshold deterministically, then grow a tight band around it.
    {
        std::size_t brightestIndex = 0;
        double brightestValue = buffer->scalarAt(0, 0);
        const std::size_t voxels = buffer->voxelCount();
        for (std::size_t i = 1; i < voxels; ++i) {
            const double v = buffer->scalarAt(i, 0);
            if (v > brightestValue) {
                brightestValue = v;
                brightestIndex = i;
            }
        }
        CHECK(brightestValue > 0.0);

        // Recover (x, y, z) of the brightest voxel.
        const int dimX = buffer->dimensionX();
        const int dimY = buffer->dimensionY();
        const std::size_t plane =
            static_cast<std::size_t>(dimX) * static_cast<std::size_t>(dimY);
        const int sz = static_cast<int>(brightestIndex / plane);
        const std::size_t rem = brightestIndex % plane;
        const int sy = static_cast<int>(rem / static_cast<std::size_t>(dimX));
        const int sx = static_cast<int>(rem % static_cast<std::size_t>(dimX));

        xq::XQRegionGrowingParameters params;
        params.seed[0] = sx;
        params.seed[1] = sy;
        params.seed[2] = sz;
        params.lower = brightestValue - 300.0;
        params.upper = brightestValue + 300.0;
        params.foregroundLabel = 1;

        const xq::SegmentationService::Result result =
            xq::SegmentationService::regionGrowMask(image, *buffer, params, image_id);
        CHECK(result.ok());
        CHECK(result.mask != nullptr);

        const std::size_t grown = result.mask->foregroundVoxelCount();
        CHECK(grown > 0); // at least the seed
        // the seed voxel itself is labeled
        const std::size_t seedIndex = buffer->voxelIndex(sx, sy, sz);
        CHECK(result.mask->labelAt(seedIndex) == 1);
        CHECK(result.mask->sourceImageNode() == image_id);
    }

    return 0;
}
