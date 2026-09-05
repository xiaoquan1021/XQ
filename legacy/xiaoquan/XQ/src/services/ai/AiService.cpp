#include "services/ai/AiService.h"

#include "core/XQAiAnalysisPayload.h"
#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQFlowResult.h"
#include "core/XQScene.h"
#include "core/XQSegmentationMask.h"
#include "core/XQSurfaceModel.h"
#include "core/command/XQSceneCommands.h"
#include "services/ai/FlowMetricsService.h"

#include <memory>
#include <string>
#include <utility>

namespace xq {

AiService::SegmentResult AiService::segment(const XQImageVolume& image,
                                           const XQMemoryImageBufferHandle& buffer,
                                           const XQAiSegmentationRequest& request,
                                           XQAiSegmentationBackend& backend)
{
    SegmentResult result;
    std::shared_ptr<XQSegmentationMask> mask = backend.segment(image, buffer, request);
    if (mask == nullptr || !mask->is_valid()) {
        result.status = Status::InvalidInput;
        result.mask = nullptr;
        return result;
    }
    result.status = Status::Ok;
    result.mask = mask;
    return result;
}

AiService::AnalysisResult AiService::identify(const XQSurfaceModel& model,
                                             const XQAiIdentifyRequest& request,
                                             XQAiIdentifyBackend& backend,
                                             const NodeId& sourceNode)
{
    AnalysisResult result;
    XQAiAnalysis analysis = backend.identify(model, request);
    analysis.setKind(AnalysisKind::Identify);
    analysis.setProvenance(AnalysisProvenance::ModelInferred);
    if (analysis.modelId().empty()) {
        analysis.setModelId(request.modelId);
    }
    if (sourceNode.is_valid()) {
        analysis.setSourceNode(sourceNode);
    }
    result.status = Status::Ok;
    result.analysis = analysis;
    return result;
}

AiService::AnalysisResult AiService::analyzeFlow(const XQFlowResult& flow,
                                               const FlowMetricsService::Request& request)
{
    AnalysisResult result;
    const FlowMetricsService::Result metrics = FlowMetricsService::analyzeFlow(flow, request);
    if (!metrics.ok()) {
        result.status = Status::InvalidFlow;
        return result;
    }
    result.status = Status::Ok;
    result.analysis = metrics.analysis;
    return result;
}

AiService::PredictResult AiService::predictFlow(const XQSurfaceModel& model,
                                               const XQSurrogateRequest& request,
                                               XQSurrogateBackend& backend,
                                               const NodeId& sourceNode)
{
    PredictResult result;
    std::shared_ptr<XQFlowResult> flow = backend.predict(model, request);
    if (flow == nullptr || !flow->isConsistent()) {
        result.status = Status::InvalidInput;
        result.flow = nullptr;
        return result;
    }

    // Wrap the prediction in an analysis whose provenance marks it as a surrogate
    // prediction -- this is what keeps it distinguishable from a solver result.
    XQAiAnalysis analysis;
    analysis.setKind(AnalysisKind::SurrogatePrediction);
    analysis.setProvenance(AnalysisProvenance::SurrogatePredicted);
    analysis.setModelId(request.modelId);
    if (sourceNode.is_valid()) {
        analysis.setSourceNode(sourceNode);
    }

    result.status = Status::Ok;
    result.flow = flow;
    result.predictedAnalysis = analysis;
    return result;
}

AiService::CommandResult AiService::buildAnalysisCommand(XQScene* scene,
                                                        const NodeId& newAnalysisId,
                                                        const std::string& name,
                                                        const XQAiAnalysis& analysis)
{
    CommandResult commandResult;
    if (scene == nullptr) {
        commandResult.status = Status::NullScene;
        commandResult.command = nullptr;
        return commandResult;
    }

    auto payload = std::make_shared<XQAiAnalysisPayload>(analysis);
    const XQDataNode node(newAnalysisId, XQDomainType::AiAnalysis, name, payload);

    commandResult.status = Status::Ok;
    if (analysis.hasSourceNode()) {
        commandResult.command.reset(new AddNodeWithSourceRelationCommand(
            scene, node, analysis.sourceNode(), "Add AI analysis"));
    } else {
        commandResult.command.reset(new AddNodeCommand(scene, node, "Add AI analysis"));
    }
    return commandResult;
}

} // namespace xq
