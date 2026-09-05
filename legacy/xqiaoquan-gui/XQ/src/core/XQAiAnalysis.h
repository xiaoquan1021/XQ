#ifndef XQ_CORE_XQ_AI_ANALYSIS_H
#define XQ_CORE_XQ_AI_ANALYSIS_H

#include "core/NodeId.h"

#include <string>
#include <vector>

namespace xq {

// What an analysis is about. FlowMetrics is computed directly from a flow
// solution; Identify labels geometry features (stenosis / plaque / branch);
// SurrogatePrediction wraps a model-predicted result.
enum class AnalysisKind {
    Identify,
    FlowMetrics,
    SurrogatePrediction
};

// How the analysis values were produced. This is load-bearing: a surrogate
// prediction must never be mistaken for a solver result, so predicted analyses
// carry SurrogatePredicted while pure post-processing carries Computed.
enum class AnalysisProvenance {
    Computed,            // derived analytically from existing data (e.g. flow metrics)
    ModelInferred,       // produced by a model run over geometry (identify)
    SurrogatePredicted   // produced by a surrogate model in place of a solve
};

// A single scalar metric, e.g. {"FFR", 0.78, ""} or {"WSS_max", 1234.5,
// "dyn/cm^2"}.
struct NamedMetric {
    std::string name;
    double value = 0.0;
    std::string unit;
};

// A located finding along the geometry: a label (e.g. "stenosis"), a position
// given by centerline arc length, an optional boundary-face id (-1 if none), and
// a confidence / severity score.
struct Annotation {
    std::string label;
    double arcLength = 0.0;
    int faceId = -1;
    double score = 0.0;
};

// Generic AI / post-processing analysis result attached to a scene node. Pure
// value object (owned vectors / strings), zero external dependencies: copying it
// deep-copies its contents.
class XQAiAnalysis {
public:
    XQAiAnalysis();

    void setKind(AnalysisKind kind);
    AnalysisKind kind() const;

    // Empty modelId means a pure computation (no model involved).
    void setModelId(const std::string& modelId);
    const std::string& modelId() const;

    void setProvenance(AnalysisProvenance provenance);
    AnalysisProvenance provenance() const;

    void addMetric(const NamedMetric& metric);
    const std::vector<NamedMetric>& metrics() const;
    // Looks up a metric by name; returns true and fills *out when found.
    bool metricByName(const std::string& name, NamedMetric* out) const;

    void addAnnotation(const Annotation& annotation);
    const std::vector<Annotation>& annotations() const;

    void setSourceNode(const NodeId& node);
    bool hasSourceNode() const;
    NodeId sourceNode() const;

    void setDiagnostic(const std::string& diagnostic);
    const std::string& diagnostic() const;

private:
    AnalysisKind kind_;
    std::string modelId_;
    AnalysisProvenance provenance_;
    std::vector<NamedMetric> metrics_;
    std::vector<Annotation> annotations_;
    bool hasSourceNode_;
    NodeId sourceNode_;
    std::string diagnostic_;
};

} // namespace xq

#endif // XQ_CORE_XQ_AI_ANALYSIS_H
