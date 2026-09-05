#ifndef XQ_SERVICES_AI_AI_SERVICE_H
#define XQ_SERVICES_AI_AI_SERVICE_H

#include "core/NodeId.h"
#include "core/XQAiAnalysis.h"
#include "core/XQAiBackends.h"
#include "core/command/XQCommand.h"
#include "services/ai/FlowMetricsService.h"
#include "core/XQAiSegmentationRequest.h"

#include <memory>
#include <string>

namespace xq {

class XQImageVolume;
class XQMemoryImageBufferHandle;
class XQSegmentationMask;
class XQSurfaceModel;
class XQFlowResult;
class XQScene;

// Unified entry point for AI analysis. Pure domain service (only xq_core +
// xq_services): it dispatches to backends (segmentation / identify / surrogate)
// or to the pure-numeric FlowMetricsService, and builds scene commands. It never
// mutates the scene and holds no scene state. ONNX never appears in this API.
class AiService {
public:
    enum class Status {
        Ok,
        InvalidInput,  // backend returned null / unusable result
        InvalidFlow,   // flow metrics rejected the flow
        NullScene      // scene pointer was null (command path only)
    };

    // Result of segmentation (reuses the M2 contract).
    struct SegmentResult {
        Status status;
        std::shared_ptr<XQSegmentationMask> mask; // null unless Ok

        bool ok() const { return status == Status::Ok; }
    };

    // Result carrying an analysis (identify / flow metrics).
    struct AnalysisResult {
        Status status;
        XQAiAnalysis analysis; // meaningful only when Ok

        bool ok() const { return status == Status::Ok; }
    };

    // Result carrying a predicted flow field. The flow's provenance is recorded
    // on the wrapping analysis (predictedAnalysis) as SurrogatePredicted so it is
    // never mistaken for a solver result.
    struct PredictResult {
        Status status;
        std::shared_ptr<XQFlowResult> flow;    // null unless Ok
        XQAiAnalysis predictedAnalysis;        // kind=SurrogatePrediction, provenance=SurrogatePredicted

        bool ok() const { return status == Status::Ok; }
    };

    struct CommandResult {
        Status status;
        std::unique_ptr<XQCommand> command; // null unless Ok

        bool ok() const { return status == Status::Ok; }
    };

    // Runs AI segmentation by delegating to the M2 backend contract. A null mask
    // from the backend maps to InvalidInput.
    static SegmentResult segment(const XQImageVolume& image,
                                 const XQMemoryImageBufferHandle& buffer,
                                 const XQAiSegmentationRequest& request,
                                 XQAiSegmentationBackend& backend);

    // Identifies geometry features via the backend. Stamps provenance
    // ModelInferred and the model node as source when sourceNode is valid.
    static AnalysisResult identify(const XQSurfaceModel& model,
                                   const XQAiIdentifyRequest& request,
                                   XQAiIdentifyBackend& backend,
                                   const NodeId& sourceNode = NodeId::invalid());

    // Computes hemodynamic metrics from a flow result (pure numeric, no backend).
    static AnalysisResult analyzeFlow(const XQFlowResult& flow,
                                      const FlowMetricsService::Request& request
                                          = FlowMetricsService::Request());

    // Predicts a flow field with a surrogate backend. The returned flow and the
    // wrapping predictedAnalysis are marked SurrogatePredicted.
    static PredictResult predictFlow(const XQSurfaceModel& model,
                                     const XQSurrogateRequest& request,
                                     XQSurrogateBackend& backend,
                                     const NodeId& sourceNode = NodeId::invalid());

    // Builds the command that inserts an analysis node, linking it as derived
    // from sourceNode when valid. The caller assigns newAnalysisId.
    static CommandResult buildAnalysisCommand(XQScene* scene,
                                              const NodeId& newAnalysisId,
                                              const std::string& name,
                                              const XQAiAnalysis& analysis);
};

} // namespace xq

#endif // XQ_SERVICES_AI_AI_SERVICE_H
