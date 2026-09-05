#include "adapters/onnx/OnnxBackend.h"

#include "core/XQAiAnalysis.h"
#include "core/XQFlowResult.h"
#include "core/XQSegmentationMask.h"
#include "core/XQSurfaceModel.h"

#include <memory>
#include <string>

// ===========================================================================
// Real ONNX Runtime inference. onnxruntime headers / objects appear ONLY inside
// this guard and never escape into XQTensor or any public header. The whole
// translation unit is added to the build only when XQ_ENABLE_ONNX is ON (see
// CMakeLists.txt), so the default build never compiles or links this file.
// ===========================================================================
#ifdef XQ_ENABLE_ONNX
#include <onnxruntime_cxx_api.h>
#endif

namespace xq {

#ifdef XQ_ENABLE_ONNX

// Opaque session state. onnxruntime types live only here.
struct OnnxBackend::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "xq_onnx"};
    Ort::SessionOptions sessionOptions;
    // Sessions are created lazily per modelId from modelDirectory.
};

OnnxBackend::OnnxBackend(const std::string& modelDirectory)
    : modelDirectory_(modelDirectory)
    , impl_(new Impl())
{
}

OnnxBackend::~OnnxBackend() = default;

bool OnnxBackend::isAvailable()
{
    return true;
}

std::shared_ptr<XQSegmentationMask> OnnxBackend::segment(
    const XQImageVolume& /*image*/,
    const XQMemoryImageBufferHandle& /*buffer*/,
    const XQAiSegmentationRequest& /*request*/)
{
    // TECHNICAL DEBT (M6): implement real segmentation inference.
    // Build XQTensor from the image/buffer (apply normalizeMean/normalizeStd),
    // run the selected .onnx via impl_->env, convert the output logits/argmax
    // back into an XQSegmentationMask. onnxruntime usage stays inside this guard.
    return nullptr;
}

XQAiAnalysis OnnxBackend::identify(const XQSurfaceModel& /*model*/,
                                   const XQAiIdentifyRequest& /*request*/)
{
    // TECHNICAL DEBT (M6): implement real identification inference.
    XQAiAnalysis analysis;
    analysis.setDiagnostic("OnnxBackend::identify not yet implemented");
    return analysis;
}

std::shared_ptr<XQFlowResult> OnnxBackend::predict(const XQSurfaceModel& /*model*/,
                                                   const XQSurrogateRequest& /*request*/)
{
    // TECHNICAL DEBT (M6): implement real surrogate inference.
    return nullptr;
}

#else // !XQ_ENABLE_ONNX

// Non-ONNX fallback definitions. This branch exists so the file is well-formed
// even if compiled without the option; the build normally excludes the file
// entirely when XQ_ENABLE_ONNX is OFF.
struct OnnxBackend::Impl {
};

OnnxBackend::OnnxBackend(const std::string& modelDirectory)
    : modelDirectory_(modelDirectory)
    , impl_(nullptr)
{
}

OnnxBackend::~OnnxBackend() = default;

bool OnnxBackend::isAvailable()
{
    return false;
}

std::shared_ptr<XQSegmentationMask> OnnxBackend::segment(
    const XQImageVolume& /*image*/,
    const XQMemoryImageBufferHandle& /*buffer*/,
    const XQAiSegmentationRequest& /*request*/)
{
    return nullptr;
}

XQAiAnalysis OnnxBackend::identify(const XQSurfaceModel& /*model*/,
                                   const XQAiIdentifyRequest& /*request*/)
{
    XQAiAnalysis analysis;
    analysis.setDiagnostic("ONNX backend not built (XQ_ENABLE_ONNX OFF)");
    return analysis;
}

std::shared_ptr<XQFlowResult> OnnxBackend::predict(const XQSurfaceModel& /*model*/,
                                                   const XQSurrogateRequest& /*request*/)
{
    return nullptr;
}

#endif // XQ_ENABLE_ONNX

} // namespace xq
