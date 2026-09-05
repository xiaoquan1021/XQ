#include "services/ai/FlowMetricsService.h"

#include "core/XQAiAnalysisPayload.h"
#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQFlowResult.h"
#include "core/XQScene.h"
#include "core/command/XQSceneCommands.h"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace xq {
namespace {

const double kPi = 3.14159265358979323846;

FlowMetricsService::Result metricsFailure(FlowMetricsService::Status status)
{
    FlowMetricsService::Result result;
    result.status = status;
    return result;
}

// Mean of a time series.
double timeMean(const std::vector<double>& series)
{
    if (series.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (std::size_t i = 0; i < series.size(); ++i) {
        sum += series[i];
    }
    return sum / static_cast<double>(series.size());
}

// Wall shear stress for a circular Poiseuille cross-section: tau = 4*mu*Q/(pi*R^3)
// with R = sqrt(A/pi). Returns 0 for non-positive area (guarded upstream).
double wallShearStress(double mu, double flowQ, double area)
{
    if (area <= 0.0) {
        return 0.0;
    }
    const double radius = std::sqrt(area / kPi);
    if (radius <= 0.0) {
        return 0.0;
    }
    return 4.0 * mu * flowQ / (kPi * radius * radius * radius);
}

} // namespace

FlowMetricsService::Result FlowMetricsService::analyzeFlow(const XQFlowResult& flow,
                                                          const Request& request)
{
    if (!flow.isConsistent()) {
        return metricsFailure(Status::InvalidFlow);
    }
    const std::vector<double>& times = flow.times();
    const std::vector<FlowSegment>& segments = flow.segments();
    if (times.empty() || segments.empty()) {
        return metricsFailure(Status::InvalidFlow);
    }

    const double mu = request.mu;
    const std::vector<std::vector<double>>& Q = flow.flowQ();
    const std::vector<std::vector<double>>& P = flow.pressureP();
    const std::vector<std::vector<double>>& A = flow.areaA();

    // Time integration weights dt[k] (trapezoidal mid-interval): each sample owns
    // half of its neighbouring intervals. For a single time sample weight = 1 so
    // the OSI integrals stay well-defined (and reduce to instantaneous values).
    const std::size_t timeCount = times.size();
    std::vector<double> dt(timeCount, 0.0);
    if (timeCount == 1) {
        dt[0] = 1.0;
    } else {
        for (std::size_t t = 0; t < timeCount; ++t) {
            double w = 0.0;
            if (t > 0) {
                w += 0.5 * (times[t] - times[t - 1]);
            }
            if (t + 1 < timeCount) {
                w += 0.5 * (times[t + 1] - times[t]);
            }
            dt[t] = w;
        }
    }

    // Locate proximal (smallest arc length) and distal (largest) segments. The
    // solver emits segments in increasing arc length, but pick by value to be
    // robust to ordering.
    std::size_t proximal = 0;
    std::size_t distal = 0;
    for (std::size_t s = 1; s < segments.size(); ++s) {
        if (segments[s].arcLengthStart < segments[proximal].arcLengthStart) {
            proximal = s;
        }
        if (segments[s].arcLengthEnd > segments[distal].arcLengthEnd) {
            distal = s;
        }
    }

    // --- WSS: per-segment time-averaged WSS (TAWSS) and global peak WSS_max ---
    // Also the per-segment OSI from the signed WSS waveform.
    double tawssMaxOverSegments = 0.0; // representative TAWSS = max over segments
    double wssPeak = 0.0;              // global |WSS| peak
    double osiMax = 0.0;              // representative OSI = max over segments

    for (std::size_t s = 0; s < segments.size(); ++s) {
        const std::vector<double>& qs = Q[s];
        const std::vector<double>& as = A[s];
        if (qs.size() != timeCount || as.size() != timeCount) {
            return metricsFailure(Status::InvalidFlow);
        }
        double sumAbsTau = 0.0;     // integral of |tau| dt  (denominator)
        double sumSignedTau = 0.0;  // integral of tau dt    (numerator, signed)
        double sumTawss = 0.0;      // sum of |tau| over samples (for TAWSS mean)
        for (std::size_t t = 0; t < timeCount; ++t) {
            if (as[t] <= 0.0) {
                return metricsFailure(Status::InvalidFlow);
            }
            const double tau = wallShearStress(mu, qs[t], as[t]);
            const double absTau = std::fabs(tau);
            sumSignedTau += tau * dt[t];
            sumAbsTau += absTau * dt[t];
            sumTawss += absTau;
            if (absTau > wssPeak) {
                wssPeak = absTau;
            }
        }
        const double tawss = sumTawss / static_cast<double>(timeCount);
        if (tawss > tawssMaxOverSegments) {
            tawssMaxOverSegments = tawss;
        }
        double osi = 0.0;
        if (sumAbsTau > 0.0) {
            osi = 0.5 * (1.0 - std::fabs(sumSignedTau) / sumAbsTau);
        }
        if (osi > osiMax) {
            osiMax = osi;
        }
    }

    // --- FFR: distal time-mean pressure / proximal time-mean pressure ---
    // The series may be wall-relative (centered near zero); add the absolute
    // baseline so FFR is a meaningful absolute-pressure ratio.
    const double ref = request.referencePressure;
    const double paMean = timeMean(P[proximal]) + ref; // proximal (inlet side)
    const double pdMean = timeMean(P[distal]) + ref;   // distal (outlet side)
    if (paMean <= 0.0) {
        return metricsFailure(Status::InvalidFlow);
    }
    const double ffr = pdMean / paMean;

    // --- Pressure drop along the vessel: proximal minus distal time-mean ---
    // (the baseline cancels, so dP is independent of the reference).
    const double pressureDrop = paMean - pdMean;

    XQAiAnalysis analysis;
    analysis.setKind(AnalysisKind::FlowMetrics);
    analysis.setProvenance(AnalysisProvenance::Computed);
    if (flow.hasSourceCaseNode()) {
        analysis.setSourceNode(flow.sourceCaseNode());
    }

    NamedMetric ffrMetric;
    ffrMetric.name = "FFR";
    ffrMetric.value = ffr;
    ffrMetric.unit = "";
    analysis.addMetric(ffrMetric);

    NamedMetric tawssMetric;
    tawssMetric.name = "TAWSS";
    tawssMetric.value = tawssMaxOverSegments;
    tawssMetric.unit = "dyn/cm^2";
    analysis.addMetric(tawssMetric);

    NamedMetric wssMaxMetric;
    wssMaxMetric.name = "WSS_max";
    wssMaxMetric.value = wssPeak;
    wssMaxMetric.unit = "dyn/cm^2";
    analysis.addMetric(wssMaxMetric);

    NamedMetric osiMetric;
    osiMetric.name = "OSI";
    osiMetric.value = osiMax;
    osiMetric.unit = "";
    analysis.addMetric(osiMetric);

    NamedMetric dpMetric;
    dpMetric.name = "dP";
    dpMetric.value = pressureDrop;
    dpMetric.unit = "dyn/cm^2";
    analysis.addMetric(dpMetric);

    // Risk annotation: a low FFR marks a hemodynamically significant lesion at
    // the distal segment.
    if (ffr < request.ffrRiskThreshold) {
        Annotation stenosis;
        stenosis.label = "stenosis";
        stenosis.arcLength = segments[distal].arcLengthEnd;
        stenosis.faceId = segments[distal].faceId != 0 ? segments[distal].faceId : -1;
        stenosis.score = 1.0 - ffr; // larger score = more severe
        analysis.addAnnotation(stenosis);
    }

    Result result;
    result.status = Status::Ok;
    result.analysis = analysis;
    return result;
}

FlowMetricsService::CommandResult FlowMetricsService::analyzeFlowCommand(
    XQScene* scene,
    const NodeId& newAnalysisId,
    const std::string& name,
    const XQFlowResult& flow,
    const NodeId& flowNodeId,
    const Request& request)
{
    CommandResult commandResult;
    if (scene == nullptr) {
        commandResult.status = Status::NullScene;
        commandResult.command = nullptr;
        return commandResult;
    }

    const Result analyzed = analyzeFlow(flow, request);
    if (!analyzed.ok()) {
        commandResult.status = analyzed.status;
        commandResult.command = nullptr;
        return commandResult;
    }

    XQAiAnalysis stored = analyzed.analysis;
    if (flowNodeId.is_valid()) {
        stored.setSourceNode(flowNodeId);
    }
    auto payload = std::make_shared<XQAiAnalysisPayload>(std::move(stored));
    const XQDataNode node(newAnalysisId, XQDomainType::AiAnalysis, name, payload);

    commandResult.status = Status::Ok;
    if (flowNodeId.is_valid()) {
        commandResult.command.reset(new AddNodeWithSourceRelationCommand(
            scene, node, flowNodeId, "Add flow metrics"));
    } else {
        commandResult.command.reset(new AddNodeCommand(scene, node, "Add flow metrics"));
    }
    return commandResult;
}

} // namespace xq
