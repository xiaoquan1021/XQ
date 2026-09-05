#ifndef XQ_CORE_XQ_AI_BACKENDS_H
#define XQ_CORE_XQ_AI_BACKENDS_H

#include "core/XQAiAnalysis.h"

#include <memory>
#include <string>

namespace xq {

class XQSurfaceModel;
class XQFlowResult;

// Request to identify / annotate geometry features (stenosis, plaque, branch).
struct XQAiIdentifyRequest {
    std::string modelId;
};

// Backend abstraction for AI feature identification. ONNX types never appear
// here -- this header pulls in no external library.
class XQAiIdentifyBackend {
public:
    virtual ~XQAiIdentifyBackend() = default;

    virtual XQAiAnalysis identify(const XQSurfaceModel& model,
                                  const XQAiIdentifyRequest& request) = 0;
};

// Request to predict a flow field with a surrogate model (in place of a solve).
struct XQSurrogateRequest {
    std::string modelId;
};

// Backend abstraction for surrogate flow prediction. Returns an XQ-owned flow
// result; a null return signals failure. Same no-external-dependency rule.
class XQSurrogateBackend {
public:
    virtual ~XQSurrogateBackend() = default;

    virtual std::shared_ptr<XQFlowResult> predict(const XQSurfaceModel& model,
                                                  const XQSurrogateRequest& request) = 0;
};

} // namespace xq

#endif // XQ_CORE_XQ_AI_BACKENDS_H
