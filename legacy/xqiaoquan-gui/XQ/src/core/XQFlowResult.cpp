#include "core/XQFlowResult.h"

#include <cstddef>

namespace xq {

XQFlowResult::XQFlowResult()
    : hasSourceCase_(false)
    , sourceCaseNode_(NodeId::invalid())
    , times_()
    , segments_()
    , flowQ_()
    , pressureP_()
    , areaA_()
    , converged_(false)
    , maxCfl_(0.0)
    , flowSmokeProvenance_()
{
}

void XQFlowResult::setSourceCaseNode(const NodeId& node)
{
    sourceCaseNode_ = node;
    hasSourceCase_ = true;
}

bool XQFlowResult::hasSourceCaseNode() const
{
    return hasSourceCase_;
}

NodeId XQFlowResult::sourceCaseNode() const
{
    if (!hasSourceCase_) {
        return NodeId::invalid();
    }
    return sourceCaseNode_;
}

void XQFlowResult::setTimes(const std::vector<double>& times)
{
    times_ = times;
}

const std::vector<double>& XQFlowResult::times() const
{
    return times_;
}

void XQFlowResult::addSegment(const FlowSegment& segment)
{
    segments_.push_back(segment);
}

const std::vector<FlowSegment>& XQFlowResult::segments() const
{
    return segments_;
}

void XQFlowResult::setSeries(const std::vector<std::vector<double>>& flowQ,
                             const std::vector<std::vector<double>>& pressureP,
                             const std::vector<std::vector<double>>& areaA)
{
    flowQ_ = flowQ;
    pressureP_ = pressureP;
    areaA_ = areaA;
}

const std::vector<std::vector<double>>& XQFlowResult::flowQ() const
{
    return flowQ_;
}

const std::vector<std::vector<double>>& XQFlowResult::pressureP() const
{
    return pressureP_;
}

const std::vector<std::vector<double>>& XQFlowResult::areaA() const
{
    return areaA_;
}

void XQFlowResult::setConverged(bool converged)
{
    converged_ = converged;
}

bool XQFlowResult::converged() const
{
    return converged_;
}

void XQFlowResult::setMaxCfl(double maxCfl)
{
    maxCfl_ = maxCfl;
}

double XQFlowResult::maxCfl() const
{
    return maxCfl_;
}

void XQFlowResult::setFlowSmokeProvenance(
    const FlowSmokeResultProvenance& provenance)
{
    flowSmokeProvenance_ = provenance;
}

bool XQFlowResult::hasFlowSmokeProvenance() const
{
    return flowSmokeProvenance_.has_value();
}

const FlowSmokeResultProvenance& XQFlowResult::flowSmokeProvenance() const
{
    return flowSmokeProvenance_.value();
}

bool XQFlowResult::isConsistent() const
{
    const std::size_t segmentCount = segments_.size();
    const std::size_t timeCount = times_.size();
    if (flowQ_.size() != segmentCount || pressureP_.size() != segmentCount
        || areaA_.size() != segmentCount) {
        return false;
    }
    for (std::size_t s = 0; s < segmentCount; ++s) {
        if (flowQ_[s].size() != timeCount || pressureP_[s].size() != timeCount
            || areaA_[s].size() != timeCount) {
            return false;
        }
    }
    return true;
}

} // namespace xq
