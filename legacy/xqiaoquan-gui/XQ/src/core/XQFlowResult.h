#ifndef XQ_CORE_XQ_FLOW_RESULT_H
#define XQ_CORE_XQ_FLOW_RESULT_H

#include "core/NodeId.h"
#include "core/XQFlowSmoke.h"

#include <optional>
#include <vector>

namespace xq {

// One reduced-order segment along the centerline. arcLength bounds locate it on
// the path; faceId optionally ties the segment to a boundary face (e.g. the
// outlet cap whose RCR it feeds). All quantities CGS.
struct FlowSegment {
    int segmentId = 0;
    double arcLengthStart = 0.0;
    double arcLengthEnd = 0.0;
    int faceId = 0; // optional boundary-face association; 0 if none
};

// Reduced-order / 1D blood-flow solution sampled over the last simulated cycle.
//
// Layout convention (load-bearing, consumers rely on it): the per-segment time
// series are indexed [segment][time]. flowQ.size() == segments.size(), and for
// every segment s, flowQ[s].size() == times.size() (same for pressureP, areaA).
// times[] are the sample instants within the last cycle. Units CGS: Q [cm^3/s],
// P [dyn/cm^2], A [cm^2].
class XQFlowResult {
public:
    XQFlowResult();

    void setSourceCaseNode(const NodeId& node);
    bool hasSourceCaseNode() const;
    NodeId sourceCaseNode() const;

    void setTimes(const std::vector<double>& times);
    const std::vector<double>& times() const;

    void addSegment(const FlowSegment& segment);
    const std::vector<FlowSegment>& segments() const;

    // Sets the full [segment][time] series at once. Each outer vector must have
    // segments().size() entries and each inner vector times().size() entries;
    // callers (the solver) guarantee this consistency.
    void setSeries(const std::vector<std::vector<double>>& flowQ,
                   const std::vector<std::vector<double>>& pressureP,
                   const std::vector<std::vector<double>>& areaA);
    const std::vector<std::vector<double>>& flowQ() const;
    const std::vector<std::vector<double>>& pressureP() const;
    const std::vector<std::vector<double>>& areaA() const;

    void setConverged(bool converged);
    bool converged() const;

    void setMaxCfl(double maxCfl);
    double maxCfl() const;

    void setFlowSmokeProvenance(const FlowSmokeResultProvenance& provenance);
    bool hasFlowSmokeProvenance() const;
    const FlowSmokeResultProvenance& flowSmokeProvenance() const;

    // True when the stored series are dimensionally consistent with times() and
    // segments() (every series has one row per segment and one column per time).
    bool isConsistent() const;

private:
    bool hasSourceCase_;
    NodeId sourceCaseNode_;
    std::vector<double> times_;
    std::vector<FlowSegment> segments_;
    std::vector<std::vector<double>> flowQ_;
    std::vector<std::vector<double>> pressureP_;
    std::vector<std::vector<double>> areaA_;
    bool converged_;
    double maxCfl_;
    std::optional<FlowSmokeResultProvenance> flowSmokeProvenance_;
};

} // namespace xq

#endif // XQ_CORE_XQ_FLOW_RESULT_H
