#include "services/segmentation/SegmentationService.h"

#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQImageVolume.h"
#include "core/XQMemoryImageBufferHandle.h"
#include "core/XQScene.h"
#include "core/XQSegmentationMask.h"
#include "core/XQSegmentationMaskPayload.h"
#include "core/command/XQSceneCommands.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace xq {
namespace {

SegmentationService::Result mask_failure(SegmentationService::Status status)
{
    SegmentationService::Result result;
    result.status = status;
    result.mask = nullptr;
    return result;
}

SegmentationService::Result mask_success(std::shared_ptr<XQSegmentationMask> mask)
{
    SegmentationService::Result result;
    result.status = SegmentationService::Status::Ok;
    result.mask = std::move(mask);
    return result;
}

// Validates that the buffer is usable and its dimensions / component count match
// the image geometry, and that both are single-component. Returns Status::Ok on
// success, otherwise the specific failure.
SegmentationService::Status validate_image_buffer(const XQImageVolume& image,
                                                  const XQMemoryImageBufferHandle& buffer)
{
    if (!buffer.is_valid()) {
        return SegmentationService::Status::InvalidBuffer;
    }
    if (image.componentCount() != 1 || buffer.componentCount() != 1) {
        return SegmentationService::Status::NotSingleComponent;
    }
    const ImageGeometry& geometry = image.geometry();
    if (buffer.dimensionX() != geometry.dimensions[0]
        || buffer.dimensionY() != geometry.dimensions[1]
        || buffer.dimensionZ() != geometry.dimensions[2]) {
        return SegmentationService::Status::InvalidBuffer;
    }
    return SegmentationService::Status::Ok;
}

// Builds an empty mask sized to the image, carrying its geometry and (if valid)
// the source image node and a single foreground label entry.
std::shared_ptr<XQSegmentationMask> make_mask_for_image(const XQImageVolume& image,
                                                        const NodeId& sourceImageNode,
                                                        int foregroundLabel)
{
    const ImageGeometry& geometry = image.geometry();
    auto mask = std::make_shared<XQSegmentationMask>(geometry.dimensions);
    mask->setGeometry(geometry);
    if (sourceImageNode.is_valid()) {
        mask->setSourceImageNode(sourceImageNode);
    }
    std::vector<SegmentationLabel> labels;
    labels.push_back(SegmentationLabel{foregroundLabel, "foreground"});
    mask->setLabels(labels);
    return mask;
}

// A foreground label must be in [1, 255]: 0 is reserved for background, and the
// mask stores labels as uint8, so a value outside that range would be silently
// truncated by the cast. Reject it instead.
bool is_valid_foreground_label(int label)
{
    return label >= 1 && label <= 255;
}

} // namespace

SegmentationService::Result SegmentationService::thresholdMask(
    const XQImageVolume& image,
    const XQMemoryImageBufferHandle& buffer,
    const XQSegmentationThresholdParameters& params,
    const NodeId& sourceImageNode)
{
    const Status buffer_status = validate_image_buffer(image, buffer);
    if (buffer_status != Status::Ok) {
        return mask_failure(buffer_status);
    }
    if (params.lower > params.upper) {
        return mask_failure(Status::InvalidThreshold);
    }
    if (!is_valid_foreground_label(params.foregroundLabel)) {
        return mask_failure(Status::InvalidLabel);
    }

    std::shared_ptr<XQSegmentationMask> mask =
        make_mask_for_image(image, sourceImageNode, params.foregroundLabel);
    if (!mask->is_valid()) {
        return mask_failure(Status::InvalidBuffer);
    }

    const auto label = static_cast<XQSegmentationMask::LabelType>(params.foregroundLabel);
    const std::size_t voxels = buffer.voxelCount();
    for (std::size_t i = 0; i < voxels; ++i) {
        const double value = buffer.scalarAt(i, 0);
        if (value >= params.lower && value <= params.upper) {
            mask->setLabelAt(i, label);
        }
    }
    return mask_success(std::move(mask));
}

SegmentationService::Result SegmentationService::regionGrowMask(
    const XQImageVolume& image,
    const XQMemoryImageBufferHandle& buffer,
    const XQRegionGrowingParameters& params,
    const NodeId& sourceImageNode)
{
    const Status buffer_status = validate_image_buffer(image, buffer);
    if (buffer_status != Status::Ok) {
        return mask_failure(buffer_status);
    }
    if (params.lower > params.upper) {
        return mask_failure(Status::InvalidThreshold);
    }
    if (!is_valid_foreground_label(params.foregroundLabel)) {
        return mask_failure(Status::InvalidLabel);
    }

    const int dimX = buffer.dimensionX();
    const int dimY = buffer.dimensionY();
    const int dimZ = buffer.dimensionZ();
    const int sx = params.seed[0];
    const int sy = params.seed[1];
    const int sz = params.seed[2];
    if (sx < 0 || sx >= dimX || sy < 0 || sy >= dimY || sz < 0 || sz >= dimZ) {
        return mask_failure(Status::SeedOutOfRange);
    }

    const auto in_threshold = [&](std::size_t index) {
        const double value = buffer.scalarAt(index, 0);
        return value >= params.lower && value <= params.upper;
    };

    const std::size_t seedIndex = buffer.voxelIndex(sx, sy, sz);
    if (!in_threshold(seedIndex)) {
        return mask_failure(Status::SeedNotInThreshold);
    }

    std::shared_ptr<XQSegmentationMask> mask =
        make_mask_for_image(image, sourceImageNode, params.foregroundLabel);
    if (!mask->is_valid()) {
        return mask_failure(Status::InvalidBuffer);
    }

    const auto label = static_cast<XQSegmentationMask::LabelType>(params.foregroundLabel);
    // Iterative 6-connected flood fill. visited tracks enqueued voxels so each
    // is processed once; the mask itself records the grown region.
    std::vector<std::uint8_t> visited(buffer.voxelCount(), 0);
    std::vector<std::size_t> stack;
    stack.push_back(seedIndex);
    visited[seedIndex] = 1;

    const int neighbourOffsets[6][3] = {
        {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};

    while (!stack.empty()) {
        const std::size_t current = stack.back();
        stack.pop_back();
        mask->setLabelAt(current, label);

        // Recover voxel coordinates from the linear index.
        const std::size_t plane = static_cast<std::size_t>(dimX) * static_cast<std::size_t>(dimY);
        const int cz = static_cast<int>(current / plane);
        const std::size_t rem = current % plane;
        const int cy = static_cast<int>(rem / static_cast<std::size_t>(dimX));
        const int cx = static_cast<int>(rem % static_cast<std::size_t>(dimX));

        for (const auto& offset : neighbourOffsets) {
            const int nx = cx + offset[0];
            const int ny = cy + offset[1];
            const int nz = cz + offset[2];
            if (nx < 0 || nx >= dimX || ny < 0 || ny >= dimY || nz < 0 || nz >= dimZ) {
                continue;
            }
            const std::size_t neighbour = buffer.voxelIndex(nx, ny, nz);
            if (visited[neighbour]) {
                continue;
            }
            visited[neighbour] = 1;
            if (in_threshold(neighbour)) {
                stack.push_back(neighbour);
            }
        }
    }
    return mask_success(std::move(mask));
}

SegmentationService::Result SegmentationService::keepLargestConnectedComponent(
    const XQSegmentationMask& mask)
{
    if (!mask.is_valid()) {
        return mask_failure(Status::InvalidMask);
    }

    const int dimX = mask.dimensionX();
    const int dimY = mask.dimensionY();
    const int dimZ = mask.dimensionZ();

    // Result mask starts empty, preserving geometry / source / labels of input.
    auto result = std::make_shared<XQSegmentationMask>(mask.dimensions());
    if (mask.hasGeometry()) {
        result->setGeometry(mask.geometry());
    }
    if (mask.hasSourceImageNode()) {
        result->setSourceImageNode(mask.sourceImageNode());
    }
    result->setLabels(mask.labels());

    const std::size_t voxelCount = mask.voxelCount();
    std::vector<std::uint8_t> visited(voxelCount, 0);
    const int neighbourOffsets[6][3] = {
        {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    const std::size_t plane = static_cast<std::size_t>(dimX) * static_cast<std::size_t>(dimY);

    std::vector<std::size_t> bestComponent;
    std::vector<std::size_t> component;
    std::vector<std::size_t> stack;

    for (std::size_t start = 0; start < voxelCount; ++start) {
        if (visited[start] || mask.labelAt(start) == 0) {
            continue;
        }
        component.clear();
        stack.clear();
        stack.push_back(start);
        visited[start] = 1;

        while (!stack.empty()) {
            const std::size_t current = stack.back();
            stack.pop_back();
            component.push_back(current);

            const int cz = static_cast<int>(current / plane);
            const std::size_t rem = current % plane;
            const int cy = static_cast<int>(rem / static_cast<std::size_t>(dimX));
            const int cx = static_cast<int>(rem % static_cast<std::size_t>(dimX));

            for (const auto& offset : neighbourOffsets) {
                const int nx = cx + offset[0];
                const int ny = cy + offset[1];
                const int nz = cz + offset[2];
                if (nx < 0 || nx >= dimX || ny < 0 || ny >= dimY || nz < 0 || nz >= dimZ) {
                    continue;
                }
                const std::size_t neighbour = mask.voxelIndex(nx, ny, nz);
                if (visited[neighbour] || mask.labelAt(neighbour) == 0) {
                    continue;
                }
                visited[neighbour] = 1;
                stack.push_back(neighbour);
            }
        }

        if (component.size() > bestComponent.size()) {
            bestComponent.swap(component);
        }
    }

    // Empty input (no foreground) yields the empty mask built above.
    for (std::size_t index : bestComponent) {
        result->setLabelAt(index, mask.labelAt(index));
    }
    return mask_success(std::move(result));
}

SegmentationService::Result SegmentationService::aiSegmentMask(
    const XQImageVolume& image,
    const XQMemoryImageBufferHandle& buffer,
    const XQAiSegmentationRequest& request,
    XQAiSegmentationBackend& backend,
    const NodeId& sourceImageNode)
{
    const Status buffer_status = validate_image_buffer(image, buffer);
    if (buffer_status != Status::Ok) {
        return mask_failure(buffer_status);
    }

    std::shared_ptr<XQSegmentationMask> mask = backend.segment(image, buffer, request);
    if (mask == nullptr || !mask->is_valid()) {
        return mask_failure(Status::InvalidMask);
    }
    // Bind the source image relation if the backend did not set one.
    if (!mask->hasSourceImageNode() && sourceImageNode.is_valid()) {
        mask->setSourceImageNode(sourceImageNode);
    }
    return mask_success(std::move(mask));
}

SegmentationService::CommandResult SegmentationService::createMaskNodeCommand(
    XQScene* scene,
    const NodeId& newMaskId,
    const std::string& name,
    const std::shared_ptr<XQSegmentationMask>& mask)
{
    CommandResult result;
    if (scene == nullptr) {
        result.status = Status::NullScene;
        result.command = nullptr;
        return result;
    }
    if (mask == nullptr || !mask->is_valid()) {
        result.status = Status::InvalidMask;
        result.command = nullptr;
        return result;
    }

    auto payload = std::make_shared<XQSegmentationMaskPayload>(*mask);
    const XQDataNode node(newMaskId, XQDomainType::SegmentationMask, name, payload);

    result.status = Status::Ok;
    if (mask->hasSourceImageNode()) {
        result.command.reset(new AddNodeWithSourceRelationCommand(
            scene, node, mask->sourceImageNode(), "Add segmentation mask"));
    } else {
        result.command.reset(new AddNodeCommand(scene, node, "Add segmentation mask"));
    }
    return result;
}

} // namespace xq
