#ifndef XQ_ADAPTERS_ONNX_ONNX_BACKEND_H
#define XQ_ADAPTERS_ONNX_ONNX_BACKEND_H

#include "core/XQAiAnalysis.h"
#include "core/XQAiBackends.h"
#include "core/XQAiSegmentationRequest.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace xq {

// XQ-owned tensor description. This is the boundary type between XQ and ONNX
// Runtime: the real backend converts XQ domain data into XQTensor, hands it to
// onnxruntime, and converts outputs back. ONNX Runtime types NEVER appear here
// (or in any public header) -- they live only inside OnnxBackend.cpp guarded by
// XQ_ENABLE_ONNX.
struct XQTensor {
    // Logical dimensions, e.g. {1, 1, D, H, W} for a 3D segmentation volume.
    std::vector<int64_t> shape;
    // Row-major contiguous float data, size == product(shape).
    std::vector<float> data;

    // Linear normalization applied to source scalars before inference:
    //   x' = (x - mean) / std. Identity (0, 1) leaves data untouched.
    double normalizeMean = 0.0;
    double normalizeStd = 1.0;

    std::size_t elementCount() const
    {
        std::size_t n = 1;
        for (std::size_t i = 0; i < shape.size(); ++i) {
            if (shape[i] <= 0) {
                return 0;
            }
            n *= static_cast<std::size_t>(shape[i]);
        }
        return shape.empty() ? 0 : n;
    }
};

// ONNX-backed implementation of the three AI backend contracts (segmentation /
// identify / surrogate). The real load + inference is compiled only when
// XQ_ENABLE_ONNX is ON (onnxruntime must be available); otherwise the methods
// report "not built" via diagnostics / null so the default build links with zero
// external AI dependency.
//
// Constructed with a model directory; modelId in each request selects the .onnx
// file. The backend owns the ONNX session internally (opaque pImpl) so this
// header stays free of onnxruntime.
class OnnxBackend : public XQAiSegmentationBackend,
                    public XQAiIdentifyBackend,
                    public XQSurrogateBackend {
public:
    explicit OnnxBackend(const std::string& modelDirectory);
    ~OnnxBackend() override;

    // True when the build included real ONNX Runtime support (XQ_ENABLE_ONNX).
    static bool isAvailable();

    std::shared_ptr<XQSegmentationMask> segment(
        const XQImageVolume& image,
        const XQMemoryImageBufferHandle& buffer,
        const XQAiSegmentationRequest& request) override;

    XQAiAnalysis identify(const XQSurfaceModel& model,
                          const XQAiIdentifyRequest& request) override;

    std::shared_ptr<XQFlowResult> predict(const XQSurfaceModel& model,
                                          const XQSurrogateRequest& request) override;

private:
    std::string modelDirectory_;

    // Opaque ONNX session state; defined only when XQ_ENABLE_ONNX is ON.
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xq

#endif // XQ_ADAPTERS_ONNX_ONNX_BACKEND_H
