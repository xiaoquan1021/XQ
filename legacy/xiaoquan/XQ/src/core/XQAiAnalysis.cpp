#include "core/XQAiAnalysis.h"

#include <cstddef>

namespace xq {

XQAiAnalysis::XQAiAnalysis()
    : kind_(AnalysisKind::FlowMetrics)
    , modelId_()
    , provenance_(AnalysisProvenance::Computed)
    , metrics_()
    , annotations_()
    , hasSourceNode_(false)
    , sourceNode_()
    , diagnostic_()
{
}

void XQAiAnalysis::setKind(AnalysisKind kind)
{
    kind_ = kind;
}

AnalysisKind XQAiAnalysis::kind() const
{
    return kind_;
}

void XQAiAnalysis::setModelId(const std::string& modelId)
{
    modelId_ = modelId;
}

const std::string& XQAiAnalysis::modelId() const
{
    return modelId_;
}

void XQAiAnalysis::setProvenance(AnalysisProvenance provenance)
{
    provenance_ = provenance;
}

AnalysisProvenance XQAiAnalysis::provenance() const
{
    return provenance_;
}

void XQAiAnalysis::addMetric(const NamedMetric& metric)
{
    metrics_.push_back(metric);
}

const std::vector<NamedMetric>& XQAiAnalysis::metrics() const
{
    return metrics_;
}

bool XQAiAnalysis::metricByName(const std::string& name, NamedMetric* out) const
{
    for (std::size_t i = 0; i < metrics_.size(); ++i) {
        if (metrics_[i].name == name) {
            if (out != nullptr) {
                *out = metrics_[i];
            }
            return true;
        }
    }
    return false;
}

void XQAiAnalysis::addAnnotation(const Annotation& annotation)
{
    annotations_.push_back(annotation);
}

const std::vector<Annotation>& XQAiAnalysis::annotations() const
{
    return annotations_;
}

void XQAiAnalysis::setSourceNode(const NodeId& node)
{
    sourceNode_ = node;
    hasSourceNode_ = node.is_valid();
}

bool XQAiAnalysis::hasSourceNode() const
{
    return hasSourceNode_;
}

NodeId XQAiAnalysis::sourceNode() const
{
    return sourceNode_;
}

void XQAiAnalysis::setDiagnostic(const std::string& diagnostic)
{
    diagnostic_ = diagnostic;
}

const std::string& XQAiAnalysis::diagnostic() const
{
    return diagnostic_;
}

} // namespace xq
