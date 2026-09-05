#include "ui/controllers/AiController.h"

#include "core/XQDataNode.h"
#include "core/XQFlowResult.h"
#include "core/XQFlowResultPayload.h"
#include "core/XQScene.h"
#include "core/command/XQCommandStack.h"

#include <utility>

namespace xq {

AiController::AiController(XQScene* scene, XQCommandStack* stack)
    : scene_(scene)
    , stack_(stack)
{
}

AiController::Status AiController::analyzeFlow(const AnalyzeFlowIntent& intent)
{
    if (scene_ == nullptr || stack_ == nullptr) {
        return Status::NullScene;
    }

    const XQDataNode* flowNode = scene_->find(intent.flowNode);
    if (flowNode == nullptr) {
        return Status::FlowNotFound;
    }
    const auto* payload =
        dynamic_cast<const XQFlowResultPayload*>(flowNode->payload().get());
    if (payload == nullptr) {
        return Status::FlowNotFound;
    }

    FlowMetricsService::CommandResult cmd = FlowMetricsService::analyzeFlowCommand(
        scene_, intent.newAnalysisId, intent.name, payload->result(),
        intent.flowNode, intent.request);
    if (!cmd.ok() || cmd.command == nullptr) {
        return Status::Rejected;
    }

    return stack_->push(std::move(cmd.command)) ? Status::Ok : Status::Rejected;
}

} // namespace xq
