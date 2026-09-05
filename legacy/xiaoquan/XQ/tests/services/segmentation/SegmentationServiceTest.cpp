#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQImageVolume.h>
#include <core/XQMemoryImageBufferHandle.h>
#include <core/XQScene.h>
#include <core/XQSegmentationMask.h>
#include <core/XQSegmentationMaskPayload.h>
#include <core/command/XQCommandStack.h>
#include <services/segmentation/SegmentationService.h>
#include <core/XQAiSegmentationRequest.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

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

namespace {

// Builds a single-component UInt8 image + buffer from a value grid laid out
// x-fastest, then y, then z (same as XQMemoryImageBufferHandle). values.size()
// must equal dimX*dimY*dimZ.
struct SyntheticImage {
    xq::XQImageVolume image;
    std::shared_ptr<xq::XQMemoryImageBufferHandle> buffer;
};

SyntheticImage make_uint8_image(int dimX, int dimY, int dimZ,
                                const std::vector<std::uint8_t>& values)
{
    xq::ImageGeometry geometry = {};
    geometry.dimensions[0] = dimX;
    geometry.dimensions[1] = dimY;
    geometry.dimensions[2] = dimZ;
    geometry.spacing[0] = 1.0;
    geometry.spacing[1] = 1.0;
    geometry.spacing[2] = 1.0;
    geometry.origin[0] = 0.0;
    geometry.origin[1] = 0.0;
    geometry.origin[2] = 0.0;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            geometry.direction[r][c] = r == c ? 1.0 : 0.0;
        }
    }
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;

    SyntheticImage out;
    out.image.setGeometry(geometry);
    out.image.setScalarType(xq::ScalarType::UInt8);
    out.image.setComponentCount(1);

    std::vector<std::uint8_t> bytes(values.begin(), values.end());
    out.buffer = std::make_shared<xq::XQMemoryImageBufferHandle>(
        xq::ScalarType::UInt8, geometry.dimensions, 1, std::move(bytes));
    return out;
}

std::size_t node_count(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes([&count](const xq::XQDataNode&) { ++count; });
    return count;
}

std::size_t relation_count(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_derived_relations([&count](const xq::NodeId&, const xq::NodeId&) { ++count; });
    return count;
}

// Mock AI backend: thresholds the buffer at >= 128 into the first target label
// (or 1 if none requested) and binds the image's geometry. Stands in for the M6
// ONNX backend, exercising the image -> request -> backend -> mask chain with
// zero external dependencies.
class MockAiBackend : public xq::XQAiSegmentationBackend {
public:
    int lastSeenModelId_calls = 0;

    std::shared_ptr<xq::XQSegmentationMask> segment(
        const xq::XQImageVolume& image,
        const xq::XQMemoryImageBufferHandle& buffer,
        const xq::XQAiSegmentationRequest& request) override
    {
        ++lastSeenModelId_calls;
        const xq::ImageGeometry& geometry = image.geometry();
        auto mask = std::make_shared<xq::XQSegmentationMask>(geometry.dimensions);
        mask->setGeometry(geometry);
        const int label = request.targetLabels.empty() ? 1 : request.targetLabels.front();
        const auto labelValue = static_cast<xq::XQSegmentationMask::LabelType>(label);
        const std::size_t voxels = buffer.voxelCount();
        for (std::size_t i = 0; i < voxels; ++i) {
            if (buffer.scalarAt(i, 0) >= 128.0) {
                mask->setLabelAt(i, labelValue);
            }
        }
        return mask;
    }
};

// AI backend that always fails by returning a null mask, to exercise the
// service's null-mask failure path.
class NullAiBackend : public xq::XQAiSegmentationBackend {
public:
    std::shared_ptr<xq::XQSegmentationMask> segment(
        const xq::XQImageVolume&,
        const xq::XQMemoryImageBufferHandle&,
        const xq::XQAiSegmentationRequest&) override
    {
        return nullptr;
    }
};

// AI backend that returns an invalid (zero-dimension) mask, to exercise the
// service's invalid-mask failure path.
class InvalidMaskAiBackend : public xq::XQAiSegmentationBackend {
public:
    std::shared_ptr<xq::XQSegmentationMask> segment(
        const xq::XQImageVolume&,
        const xq::XQMemoryImageBufferHandle&,
        const xq::XQAiSegmentationRequest&) override
    {
        const int badDims[3] = {0, 1, 1}; // dimension < 1 -> invalid mask
        return std::make_shared<xq::XQSegmentationMask>(badDims);
    }
};

} // namespace

int main()
{
    // ===================================================================
    // 1. XQMemoryImageBufferHandle: construction validation + scalar read
    // ===================================================================
    {
        const int dims[3] = {2, 2, 2}; // 8 voxels
        std::vector<std::uint8_t> bytes = {10, 20, 30, 40, 50, 60, 70, 80};
        xq::XQMemoryImageBufferHandle buffer(xq::ScalarType::UInt8, dims, 1, bytes);
        CHECK(buffer.is_valid());
        CHECK(buffer.voxelCount() == 8);
        CHECK(buffer.scalarAt(0, 0) == 10.0);
        CHECK(buffer.scalarAt(7, 0) == 80.0);
        const std::size_t idx = buffer.voxelIndex(1, 1, 1);
        CHECK(idx == 7);
        CHECK(buffer.scalarAt(idx, 0) == 80.0);
        // out-of-range index returns 0.0
        CHECK(buffer.scalarAt(8, 0) == 0.0);
    }
    {
        // byte count mismatch -> invalid buffer (no throw)
        const int dims[3] = {2, 2, 2};
        std::vector<std::uint8_t> tooFew = {1, 2, 3};
        xq::XQMemoryImageBufferHandle buffer(xq::ScalarType::UInt8, dims, 1, tooFew);
        CHECK(!buffer.is_valid());
        CHECK(buffer.voxelCount() == 0);
    }
    {
        // Int16: two bytes per scalar, little-endian native
        const int dims[3] = {2, 1, 1};
        std::vector<std::uint8_t> bytes = {0x00, 0x01, 0xFF, 0x7F}; // 256, 32767
        xq::XQMemoryImageBufferHandle buffer(xq::ScalarType::Int16, dims, 1, bytes);
        CHECK(buffer.is_valid());
        const double v0 = buffer.scalarAt(0, 0);
        const double v1 = buffer.scalarAt(1, 0);
        CHECK(v0 == 256.0);
        CHECK(v1 == 32767.0);
    }

    // ===================================================================
    // 2. XQSegmentationMask + payload: voxel read/write, domain, clone
    // ===================================================================
    {
        const int dims[3] = {3, 1, 1};
        xq::XQSegmentationMask mask(dims);
        CHECK(mask.is_valid());
        CHECK(mask.voxelCount() == 3);
        CHECK(mask.foregroundVoxelCount() == 0);
        mask.setLabelAt(1, 5);
        CHECK(mask.labelAt(1) == 5);
        CHECK(mask.foregroundVoxelCount() == 1);

        xq::XQSegmentationMaskPayload payload(mask);
        CHECK(payload.domainType() == xq::XQDomainType::SegmentationMask);
        const std::shared_ptr<xq::XQPayload> cloned = payload.clone();
        CHECK(cloned != nullptr);
        CHECK(cloned->domainType() == xq::XQDomainType::SegmentationMask);
        // deep copy: mutating the clone does not touch the original payload
        auto* clonedMask = dynamic_cast<xq::XQSegmentationMaskPayload*>(cloned.get());
        CHECK(clonedMask != nullptr);
        clonedMask->mask().setLabelAt(0, 9);
        CHECK(clonedMask->mask().labelAt(0) == 9);
        CHECK(payload.mask().labelAt(0) == 0);
    }

    const xq::NodeId image_id(1);
    const xq::NodeId mask_id(2);

    // ===================================================================
    // 3. thresholdMask: validation + known voxel count on synthetic data
    // ===================================================================
    {
        // 4x1x1 with values 10, 100, 150, 200. Threshold [100, 200] selects 3.
        SyntheticImage syn = make_uint8_image(4, 1, 1, {10, 100, 150, 200});
        CHECK(syn.buffer->is_valid());

        xq::XQSegmentationThresholdParameters params;
        params.lower = 100.0;
        params.upper = 200.0;
        params.foregroundLabel = 1;

        const xq::SegmentationService::Result result =
            xq::SegmentationService::thresholdMask(syn.image, *syn.buffer, params, image_id);
        CHECK(result.ok());
        CHECK(result.mask != nullptr);
        CHECK(result.mask->foregroundVoxelCount() == 3);
        CHECK(result.mask->hasSourceImageNode());
        CHECK(result.mask->sourceImageNode() == image_id);
        CHECK(result.mask->hasGeometry());
        // background voxel 0 (value 10) is not selected
        CHECK(result.mask->labelAt(0) == 0);
        CHECK(result.mask->labelAt(1) == 1);
    }
    {
        // lower > upper rejected
        SyntheticImage syn = make_uint8_image(2, 1, 1, {0, 255});
        xq::XQSegmentationThresholdParameters bad;
        bad.lower = 200.0;
        bad.upper = 100.0;
        const xq::SegmentationService::Result result =
            xq::SegmentationService::thresholdMask(syn.image, *syn.buffer, bad);
        CHECK(result.status == xq::SegmentationService::Status::InvalidThreshold);
        CHECK(result.mask == nullptr);
    }
    {
        // foreground label 0 rejected (reserved for background)
        SyntheticImage syn = make_uint8_image(2, 1, 1, {0, 255});
        xq::XQSegmentationThresholdParameters bad;
        bad.lower = 0.0;
        bad.upper = 255.0;
        bad.foregroundLabel = 0;
        const xq::SegmentationService::Result result =
            xq::SegmentationService::thresholdMask(syn.image, *syn.buffer, bad);
        CHECK(result.status == xq::SegmentationService::Status::InvalidLabel);
    }

    // ===================================================================
    // 4. regionGrowMask: two separated blobs; grow only the seeded one
    // ===================================================================
    {
        // 5x1x1 line: high, high, LOW, high, high. Two foreground blobs of size 2
        // separated by a background voxel. Region grow from voxel 0 yields only
        // the left blob (2 voxels).
        SyntheticImage syn = make_uint8_image(5, 1, 1, {200, 200, 0, 200, 200});
        CHECK(syn.buffer->is_valid());

        xq::XQRegionGrowingParameters params;
        params.seed[0] = 0;
        params.seed[1] = 0;
        params.seed[2] = 0;
        params.lower = 100.0;
        params.upper = 255.0;
        params.foregroundLabel = 1;

        const xq::SegmentationService::Result result =
            xq::SegmentationService::regionGrowMask(syn.image, *syn.buffer, params, image_id);
        CHECK(result.ok());
        CHECK(result.mask != nullptr);
        CHECK(result.mask->foregroundVoxelCount() == 2);
        CHECK(result.mask->labelAt(0) == 1);
        CHECK(result.mask->labelAt(1) == 1);
        CHECK(result.mask->labelAt(2) == 0);
        CHECK(result.mask->labelAt(3) == 0); // right blob NOT reached
        CHECK(result.mask->labelAt(4) == 0);
    }
    {
        // 3x3x1 plus-shaped 6-connected region grow. Center + 4 edge mids high,
        // 4 corners low. Grow from center reaches center + 4 = 5 voxels.
        //   row layout (x fastest): (0,0)(1,0)(2,0) (0,1)(1,1)(2,1) (0,2)(1,2)(2,2)
        //   high at (1,0),(0,1),(1,1),(2,1),(1,2); low at corners
        SyntheticImage syn = make_uint8_image(3, 3, 1,
                                              {0, 200, 0, 200, 200, 200, 0, 200, 0});
        xq::XQRegionGrowingParameters params;
        params.seed[0] = 1;
        params.seed[1] = 1;
        params.seed[2] = 0;
        params.lower = 100.0;
        params.upper = 255.0;
        const xq::SegmentationService::Result result =
            xq::SegmentationService::regionGrowMask(syn.image, *syn.buffer, params);
        CHECK(result.ok());
        CHECK(result.mask->foregroundVoxelCount() == 5);
    }
    {
        // seed out of range
        SyntheticImage syn = make_uint8_image(3, 1, 1, {200, 200, 200});
        xq::XQRegionGrowingParameters params;
        params.seed[0] = 9;
        params.lower = 0.0;
        params.upper = 255.0;
        const xq::SegmentationService::Result result =
            xq::SegmentationService::regionGrowMask(syn.image, *syn.buffer, params);
        CHECK(result.status == xq::SegmentationService::Status::SeedOutOfRange);
        CHECK(result.mask == nullptr);
    }
    {
        // seed does not satisfy threshold
        SyntheticImage syn = make_uint8_image(3, 1, 1, {0, 200, 200});
        xq::XQRegionGrowingParameters params;
        params.seed[0] = 0; // value 0, below threshold
        params.lower = 100.0;
        params.upper = 255.0;
        const xq::SegmentationService::Result result =
            xq::SegmentationService::regionGrowMask(syn.image, *syn.buffer, params);
        CHECK(result.status == xq::SegmentationService::Status::SeedNotInThreshold);
    }

    // ===================================================================
    // 5. keepLargestConnectedComponent: two blobs -> keep the larger
    // ===================================================================
    {
        // 6x1x1: blob A size 1 (voxel 0), gap, blob B size 3 (voxels 2,3,4), gap.
        //   labels: 1 0 1 1 1 0
        const int dims[3] = {6, 1, 1};
        xq::XQSegmentationMask mask(dims);
        mask.setLabelAt(0, 1);
        mask.setLabelAt(2, 1);
        mask.setLabelAt(3, 1);
        mask.setLabelAt(4, 1);
        const std::size_t before = mask.foregroundVoxelCount();
        CHECK(before == 4);

        const xq::SegmentationService::Result result =
            xq::SegmentationService::keepLargestConnectedComponent(mask);
        CHECK(result.ok());
        CHECK(result.mask != nullptr);
        CHECK(result.mask->foregroundVoxelCount() == 3);
        CHECK(result.mask->labelAt(0) == 0); // small blob removed
        CHECK(result.mask->labelAt(2) == 1);
        CHECK(result.mask->labelAt(3) == 1);
        CHECK(result.mask->labelAt(4) == 1);
    }
    {
        // empty mask -> empty mask preserving geometry + source
        const int dims[3] = {4, 1, 1};
        xq::XQSegmentationMask mask(dims);
        xq::ImageGeometry geometry = {};
        geometry.dimensions[0] = 4;
        geometry.dimensions[1] = 1;
        geometry.dimensions[2] = 1;
        mask.setGeometry(geometry);
        mask.setSourceImageNode(image_id);

        const xq::SegmentationService::Result result =
            xq::SegmentationService::keepLargestConnectedComponent(mask);
        CHECK(result.ok());
        CHECK(result.mask->foregroundVoxelCount() == 0);
        CHECK(result.mask->hasGeometry());
        CHECK(result.mask->hasSourceImageNode());
        CHECK(result.mask->sourceImageNode() == image_id);
    }

    // ===================================================================
    // 6. AI minimal contract + mock backend: image -> request -> mask chain
    // ===================================================================
    {
        SyntheticImage syn = make_uint8_image(4, 1, 1, {10, 200, 50, 130});
        MockAiBackend backend;

        xq::XQAiSegmentationRequest request;
        request.modelId = "mock-unet-v0";
        request.targetLabels = {7};

        const xq::SegmentationService::Result result =
            xq::SegmentationService::aiSegmentMask(
                syn.image, *syn.buffer, request, backend, image_id);
        CHECK(result.ok());
        CHECK(result.mask != nullptr);
        CHECK(backend.lastSeenModelId_calls == 1);
        // values >= 128 are foreground: voxels 1 (200) and 3 (130) -> 2 voxels
        CHECK(result.mask->foregroundVoxelCount() == 2);
        CHECK(result.mask->labelAt(1) == 7);
        CHECK(result.mask->labelAt(3) == 7);
        // service bound the source image node (backend left it unset)
        CHECK(result.mask->hasSourceImageNode());
        CHECK(result.mask->sourceImageNode() == image_id);
    }

    // ===================================================================
    // 6b. Failure paths: invalid buffer / non-single-component / bad label /
    //     AI backend returning null or an invalid mask.
    // ===================================================================
    {
        // Invalid buffer (byte-count mismatch) -> InvalidBuffer for both algorithms.
        SyntheticImage syn = make_uint8_image(2, 1, 1, {100, 200});
        const int dims[3] = {2, 1, 1};
        std::vector<std::uint8_t> tooFew = {1}; // needs 2 bytes
        auto badBuffer = std::make_shared<xq::XQMemoryImageBufferHandle>(
            xq::ScalarType::UInt8, dims, 1, tooFew);
        CHECK(!badBuffer->is_valid());

        xq::XQSegmentationThresholdParameters tp;
        tp.lower = 0.0;
        tp.upper = 255.0;
        const xq::SegmentationService::Result tr =
            xq::SegmentationService::thresholdMask(syn.image, *badBuffer, tp);
        CHECK(tr.status == xq::SegmentationService::Status::InvalidBuffer);
        CHECK(tr.mask == nullptr);

        xq::XQRegionGrowingParameters rp;
        rp.lower = 0.0;
        rp.upper = 255.0;
        const xq::SegmentationService::Result rr =
            xq::SegmentationService::regionGrowMask(syn.image, *badBuffer, rp);
        CHECK(rr.status == xq::SegmentationService::Status::InvalidBuffer);
        CHECK(rr.mask == nullptr);
    }
    {
        // Buffer dimensions disagree with the image geometry -> InvalidBuffer.
        SyntheticImage syn = make_uint8_image(4, 1, 1, {10, 20, 30, 40});
        const int wrongDims[3] = {2, 1, 1};
        std::vector<std::uint8_t> bytes = {10, 20};
        auto mismatched = std::make_shared<xq::XQMemoryImageBufferHandle>(
            xq::ScalarType::UInt8, wrongDims, 1, bytes);
        CHECK(mismatched->is_valid());

        xq::XQSegmentationThresholdParameters tp;
        tp.lower = 0.0;
        tp.upper = 255.0;
        const xq::SegmentationService::Result tr =
            xq::SegmentationService::thresholdMask(syn.image, *mismatched, tp);
        CHECK(tr.status == xq::SegmentationService::Status::InvalidBuffer);
        CHECK(tr.mask == nullptr);
    }
    {
        // Multi-component image/buffer -> NotSingleComponent.
        xq::ImageGeometry geometry = {};
        geometry.dimensions[0] = 2;
        geometry.dimensions[1] = 1;
        geometry.dimensions[2] = 1;
        geometry.spacing[0] = 1.0;
        geometry.spacing[1] = 1.0;
        geometry.spacing[2] = 1.0;
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                geometry.direction[r][c] = r == c ? 1.0 : 0.0;
            }
        }
        geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;

        xq::XQImageVolume image;
        image.setGeometry(geometry);
        image.setScalarType(xq::ScalarType::UInt8);
        image.setComponentCount(2); // not single-component

        std::vector<std::uint8_t> bytes = {1, 2, 3, 4}; // 2 voxels * 2 components
        xq::XQMemoryImageBufferHandle buffer(
            xq::ScalarType::UInt8, geometry.dimensions, 2, std::move(bytes));
        CHECK(buffer.is_valid());

        xq::XQSegmentationThresholdParameters tp;
        tp.lower = 0.0;
        tp.upper = 255.0;
        const xq::SegmentationService::Result tr =
            xq::SegmentationService::thresholdMask(image, buffer, tp);
        CHECK(tr.status == xq::SegmentationService::Status::NotSingleComponent);
        CHECK(tr.mask == nullptr);

        xq::XQRegionGrowingParameters rp;
        rp.lower = 0.0;
        rp.upper = 255.0;
        const xq::SegmentationService::Result rr =
            xq::SegmentationService::regionGrowMask(image, buffer, rp);
        CHECK(rr.status == xq::SegmentationService::Status::NotSingleComponent);
        CHECK(rr.mask == nullptr);
    }
    {
        // W1: foreground label out of [1, 255] is rejected, not silently
        // truncated. Label 256 casts to uint8 0 (background); the old code
        // produced an all-background mask. Now it must fail with InvalidLabel
        // and return no mask.
        SyntheticImage syn = make_uint8_image(2, 1, 1, {100, 200});
        xq::XQSegmentationThresholdParameters tp;
        tp.lower = 0.0;
        tp.upper = 255.0;
        tp.foregroundLabel = 256;
        const xq::SegmentationService::Result tr =
            xq::SegmentationService::thresholdMask(syn.image, *syn.buffer, tp);
        CHECK(tr.status == xq::SegmentationService::Status::InvalidLabel);
        CHECK(tr.mask == nullptr);

        xq::XQRegionGrowingParameters rp;
        rp.seed[0] = 0;
        rp.lower = 0.0;
        rp.upper = 255.0;
        rp.foregroundLabel = 256;
        const xq::SegmentationService::Result rr =
            xq::SegmentationService::regionGrowMask(syn.image, *syn.buffer, rp);
        CHECK(rr.status == xq::SegmentationService::Status::InvalidLabel);
        CHECK(rr.mask == nullptr);

        // Negative label is rejected too.
        xq::XQSegmentationThresholdParameters tpNeg;
        tpNeg.lower = 0.0;
        tpNeg.upper = 255.0;
        tpNeg.foregroundLabel = -1;
        const xq::SegmentationService::Result trNeg =
            xq::SegmentationService::thresholdMask(syn.image, *syn.buffer, tpNeg);
        CHECK(trNeg.status == xq::SegmentationService::Status::InvalidLabel);
    }
    {
        // aiSegmentMask: invalid buffer -> InvalidBuffer (backend not consulted).
        SyntheticImage syn = make_uint8_image(2, 1, 1, {100, 200});
        const int dims[3] = {2, 1, 1};
        std::vector<std::uint8_t> tooFew = {1};
        auto badBuffer = std::make_shared<xq::XQMemoryImageBufferHandle>(
            xq::ScalarType::UInt8, dims, 1, tooFew);
        CHECK(!badBuffer->is_valid());

        MockAiBackend backend;
        xq::XQAiSegmentationRequest request;
        request.modelId = "mock";
        const xq::SegmentationService::Result result =
            xq::SegmentationService::aiSegmentMask(
                syn.image, *badBuffer, request, backend, image_id);
        CHECK(result.status == xq::SegmentationService::Status::InvalidBuffer);
        CHECK(result.mask == nullptr);
        CHECK(backend.lastSeenModelId_calls == 0); // backend never called
    }
    {
        // aiSegmentMask: backend returns null mask -> InvalidMask.
        SyntheticImage syn = make_uint8_image(2, 1, 1, {100, 200});
        NullAiBackend backend;
        xq::XQAiSegmentationRequest request;
        request.modelId = "mock";
        const xq::SegmentationService::Result result =
            xq::SegmentationService::aiSegmentMask(
                syn.image, *syn.buffer, request, backend, image_id);
        CHECK(result.status == xq::SegmentationService::Status::InvalidMask);
        CHECK(result.mask == nullptr);
    }
    {
        // aiSegmentMask: backend returns an invalid mask -> InvalidMask.
        SyntheticImage syn = make_uint8_image(2, 1, 1, {100, 200});
        InvalidMaskAiBackend backend;
        xq::XQAiSegmentationRequest request;
        request.modelId = "mock";
        const xq::SegmentationService::Result result =
            xq::SegmentationService::aiSegmentMask(
                syn.image, *syn.buffer, request, backend, image_id);
        CHECK(result.status == xq::SegmentationService::Status::InvalidMask);
        CHECK(result.mask == nullptr);
    }

    // ===================================================================
    // 7. createMaskNodeCommand: into scene with source relation + undo/redo
    // ===================================================================
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        // an image node to be the source
        scene.insert(xq::XQDataNode(image_id, xq::XQDomainType::Image, "img",
                                    std::shared_ptr<xq::XQPayload>()));

        SyntheticImage syn = make_uint8_image(4, 1, 1, {10, 200, 150, 50});
        xq::XQSegmentationThresholdParameters params;
        params.lower = 100.0;
        params.upper = 255.0;
        const xq::SegmentationService::Result seg =
            xq::SegmentationService::thresholdMask(syn.image, *syn.buffer, params, image_id);
        CHECK(seg.ok());

        xq::SegmentationService::CommandResult cmd =
            xq::SegmentationService::createMaskNodeCommand(&scene, mask_id, "aorta-mask", seg.mask);
        CHECK(cmd.ok());
        CHECK(cmd.command != nullptr);

        const std::size_t nodesBefore = node_count(scene);
        stack.push(std::move(cmd.command));
        CHECK(node_count(scene) == nodesBefore + 1);
        CHECK(relation_count(scene) == 1); // ImageToSegmentationMask relation

        // mask node carries a real mask payload preserving the source binding
        const xq::XQDataNode* node = scene.find(mask_id);
        CHECK(node != nullptr);
        CHECK(node->domainType() == xq::XQDomainType::SegmentationMask);
        const auto* payload =
            dynamic_cast<const xq::XQSegmentationMaskPayload*>(node->payload().get());
        CHECK(payload != nullptr);
        CHECK(payload->mask().foregroundVoxelCount() == 2);
        CHECK(payload->mask().sourceImageNode() == image_id);

        // undo removes node + relation; redo restores both
        const bool undone = stack.undo();
        CHECK(undone);
        CHECK(node_count(scene) == nodesBefore);
        CHECK(relation_count(scene) == 0);
        CHECK(scene.find(mask_id) == nullptr);

        const bool redone = stack.redo();
        CHECK(redone);
        CHECK(node_count(scene) == nodesBefore + 1);
        CHECK(relation_count(scene) == 1);
        CHECK(scene.find(mask_id) != nullptr);
    }
    {
        // mask without a source image node -> plain AddNodeCommand (no relation)
        xq::XQScene scene;
        xq::XQCommandStack stack;
        SyntheticImage syn = make_uint8_image(3, 1, 1, {200, 200, 200});
        xq::XQSegmentationThresholdParameters params;
        params.lower = 100.0;
        params.upper = 255.0;
        const xq::SegmentationService::Result seg =
            xq::SegmentationService::thresholdMask(syn.image, *syn.buffer, params);
        CHECK(seg.ok());
        CHECK(!seg.mask->hasSourceImageNode());

        xq::SegmentationService::CommandResult cmd =
            xq::SegmentationService::createMaskNodeCommand(&scene, mask_id, "m", seg.mask);
        CHECK(cmd.ok());
        stack.push(std::move(cmd.command));
        CHECK(node_count(scene) == 1);
        CHECK(relation_count(scene) == 0);
    }
    {
        // null scene rejected
        SyntheticImage syn = make_uint8_image(2, 1, 1, {200, 200});
        xq::XQSegmentationThresholdParameters params;
        params.lower = 0.0;
        params.upper = 255.0;
        const xq::SegmentationService::Result seg =
            xq::SegmentationService::thresholdMask(syn.image, *syn.buffer, params);
        CHECK(seg.ok());
        xq::SegmentationService::CommandResult cmd =
            xq::SegmentationService::createMaskNodeCommand(nullptr, mask_id, "m", seg.mask);
        CHECK(cmd.status == xq::SegmentationService::Status::NullScene);
        CHECK(cmd.command == nullptr);
    }

    return 0;
}
