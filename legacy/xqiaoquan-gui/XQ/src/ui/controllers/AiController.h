#ifndef XQ_UI_CONTROLLERS_AI_CONTROLLER_H
#define XQ_UI_CONTROLLERS_AI_CONTROLLER_H

#include "core/NodeId.h"
#include "services/ai/FlowMetricsService.h"

#include <string>

namespace xq {

class XQScene;
class XQCommandStack;

// Thin UI-facing controller for the AI / analysis stage. It reads the chosen
// flow result from its scene node payload, asks FlowMetricsService for the
// hemodynamic-metrics analysis command, and commits it through the command
// stack. It implements no metrics itself and never mutates the scene directly.
//
// First version covers the pure-numeric flow-metrics path (no backend needed).
// The identify / surrogate AI paths run through AiService with a backend and are
// exercised by the segmentation controller / service tests; wiring extra backend
// panels here is left as follow-up.
//
// Pure C++: depends only on core + services (no Qt / VTK).
class AiController {
public:
    AiController(XQScene* scene, XQCommandStack* stack);

    enum class Status {
        Ok,
        Rejected,        // FlowMetricsService rejected the flow (no scene change)
        FlowNotFound,    // flowNode is missing or carries no flow result
        NullScene,
    };

    struct AnalyzeFlowIntent {
        NodeId newAnalysisId;
        std::string name;
        NodeId flowNode;                       // flow-result node to analyze
        FlowMetricsService::Request request;   // mu / FFR threshold / reference pressure
    };

    // Reads the flow result from flowNode, computes the metrics, and pushes the
    // analysis-insertion command derived from flowNode.
    Status analyzeFlow(const AnalyzeFlowIntent& intent);

private:
    XQScene* scene_;
    XQCommandStack* stack_;
};

} // namespace xq

#endif // XQ_UI_CONTROLLERS_AI_CONTROLLER_H
