#ifndef XQ_SERVICES_AI_FLOW_METRICS_SERVICE_H
#define XQ_SERVICES_AI_FLOW_METRICS_SERVICE_H

#include "core/NodeId.h"
#include "core/XQAiAnalysis.h"
#include "core/command/XQCommand.h"

#include <memory>
#include <string>

namespace xq {

class XQFlowResult;
class XQScene;

// Pure-numeric hemodynamic post-processing of a reduced-order / 1D flow result.
// Zero external dependencies (no Qt / VTK / ITK / ONNX / Python): only xq_core +
// standard C++. It reads a const XQFlowResult and produces an XQAiAnalysis whose
// provenance is Computed (these are not model predictions).
//
// All quantities CGS. Definitions follow standard hemodynamics (SimVascular /
// MITK convention):
//   WSS (wall shear stress), Poiseuille circular tube: tau_w = 4*mu*Q/(pi*R^3),
//       with effective radius R = sqrt(A/pi).
//   FFR (fractional flow reserve): distal time-mean pressure / proximal
//       time-mean pressure (Pd/Pa), in (0, 1]; < 0.8 flags a significant lesion.
//   OSI (oscillatory shear index): 0.5*(1 - |sum tau*dt| / sum |tau|*dt) in
//       [0, 0.5]; high values mark flow reversal.
//   dP (pressure drop): proximal minus distal time-mean pressure.
class FlowMetricsService {
public:
    // Inputs the metrics need that are not carried by XQFlowResult itself.
    struct Request {
        double mu = 0.04; // dynamic viscosity [poise], CGS blood default
        // Time-mean FFR below this flags a hemodynamically significant lesion
        // and emits a "stenosis" annotation at the distal segment.
        double ffrRiskThreshold = 0.8;
        // Absolute-pressure baseline [dyn/cm^2] added to the (possibly relative)
        // pressure series before FFR / dP. The reduced-order solver records a
        // wall-relative pressure centered near zero; FFR is an absolute-pressure
        // ratio, so callers working from such a field pass the physiological
        // operating pressure here. Default 0 means the series is already absolute
        // (e.g. synthetic test fields), so FFR = Pd/Pa is taken verbatim.
        double referencePressure = 0.0;
    };

    enum class Status {
        Ok,
        InvalidFlow, // flow inconsistent / empty / non-positive area / non-positive absolute Pa
        NullScene    // scene pointer was null (command path only)
    };

    struct Result {
        Status status;
        XQAiAnalysis analysis; // meaningful only when status == Ok

        bool ok() const
        {
            return status == Status::Ok;
        }
    };

    struct CommandResult {
        Status status;
        std::unique_ptr<XQCommand> command; // null unless status == Ok

        bool ok() const
        {
            return status == Status::Ok;
        }
    };

    // Computes FFR / TAWSS / WSS_max / OSI / dP from the flow result. Returns
    // InvalidFlow (no analysis) when the flow is inconsistent or empty.
    static Result analyzeFlow(const XQFlowResult& flow, const Request& request = Request());

    // Builds the command that inserts the analysis node, linking it as derived
    // from flowNodeId (the flow-result node). The caller assigns newAnalysisId.
    static CommandResult analyzeFlowCommand(XQScene* scene,
                                            const NodeId& newAnalysisId,
                                            const std::string& name,
                                            const XQFlowResult& flow,
                                            const NodeId& flowNodeId,
                                            const Request& request = Request());
};

} // namespace xq

#endif // XQ_SERVICES_AI_FLOW_METRICS_SERVICE_H
