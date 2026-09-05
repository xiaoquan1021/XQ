#ifndef XQ_CORE_XQ_AI_SEGMENTATION_REQUEST_H
#define XQ_CORE_XQ_AI_SEGMENTATION_REQUEST_H

#include "core/XQImageVolume.h"
#include "core/XQMemoryImageBufferHandle.h"
#include "core/XQSegmentationMask.h"

#include <memory>
#include <string>
#include <vector>

namespace xq {

// Minimal, stable contract for AI-driven segmentation. M2 fixes this contract
// and validates the "image -> request -> backend -> mask" chain with a mock
// backend; the real ONNX backend lands in M6 implementing the same interface.
// ONNX does NOT appear in M2 and this header pulls in no external library.
//
// The contract is deliberately minimal: a model id, an optional region of
// interest, and the labels the caller wants produced. M6 reuses this request
// type for AiService::segment.
struct XQAiSegmentationRequest {
    // Identifies which model the backend should run (opaque to the contract).
    std::string modelId;

    // Optional voxel-space region of interest, as an inclusive extent
    // {xMin, xMax, yMin, yMax, zMin, zMax}. When hasRoi is false the whole
    // image is considered.
    bool hasRoi = false;
    int roi[6] = {0, 0, 0, 0, 0, 0};

    // Label values the caller wants the backend to produce. May be empty, in
    // which case the backend chooses its own default labeling.
    std::vector<int> targetLabels;
};

// Backend abstraction for AI segmentation. M2 ships a mock implementation to
// exercise the chain; M6 ships the real ONNX-backed implementation. The backend
// receives the image geometry/type (XQImageVolume), the real scalar buffer, and
// the request, and returns an XQ-owned mask. A null return signals failure.
class XQAiSegmentationBackend {
public:
    virtual ~XQAiSegmentationBackend() = default;

    virtual std::shared_ptr<XQSegmentationMask> segment(
        const XQImageVolume& image,
        const XQMemoryImageBufferHandle& buffer,
        const XQAiSegmentationRequest& request) = 0;
};

} // namespace xq

#endif // XQ_CORE_XQ_AI_SEGMENTATION_REQUEST_H
