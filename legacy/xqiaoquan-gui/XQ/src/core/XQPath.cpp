#include "core/XQPath.h"

#include <cmath>
#include <cstddef>
#include <vector>

namespace xq {
namespace {

const double kEpsilon = 1e-12;

Point3 interpolate(Point3 a, Point3 b, double t)
{
    return add(scale(a, 1.0 - t), scale(b, t));
}

Vec3 reject_from_axis(Vec3 value, Vec3 axis)
{
    return sub(value, scale(axis, dot(value, axis)));
}

Vec3 fallback_normal(Vec3 tangent)
{
    const Vec3 reference = std::abs(tangent.x) < 0.9 ? Vec3{1.0, 0.0, 0.0} : Vec3{0.0, 1.0, 0.0};
    return normalized(cross(reference, tangent));
}

std::vector<double> cumulative_lengths(const std::vector<PathControlPoint>& points)
{
    std::vector<double> cumulative;
    cumulative.reserve(points.size());
    cumulative.push_back(0.0);

    for (std::size_t i = 1; i < points.size(); ++i) {
        const double segmentLength = distance(points[i - 1].position, points[i].position);
        cumulative.push_back(cumulative.back() + segmentLength);
    }

    return cumulative;
}

// cursor (optional): index of the segment found for the previous, smaller
// arcLength. resample() queries monotonically increasing arc lengths, so the
// containing segment index never decreases; scanning forward from the cursor
// makes a whole resample O(N + M) instead of O(N * M). Pass nullptr to scan
// from the beginning (single-query callers).
std::size_t segment_for_arc_length(const std::vector<PathControlPoint>& points,
                                   const std::vector<double>& cumulative,
                                   double arcLength,
                                   std::size_t* cursor)
{
    const std::size_t first = cursor != nullptr ? *cursor : 0;
    for (std::size_t i = first; i + 1 < points.size(); ++i) {
        if (cumulative[i + 1] >= arcLength && cumulative[i + 1] > cumulative[i]) {
            if (cursor != nullptr) {
                *cursor = i;
            }
            return i;
        }
    }

    for (std::size_t i = points.size() - 1; i > 0; --i) {
        if (cumulative[i] > cumulative[i - 1]) {
            return i - 1;
        }
    }

    return 0;
}

Point3 point_at_arc_length(const std::vector<PathControlPoint>& points,
                           const std::vector<double>& cumulative,
                           double arcLength,
                           std::size_t* cursor = nullptr)
{
    const std::size_t segment = segment_for_arc_length(points, cumulative, arcLength, cursor);
    const double segmentLength = cumulative[segment + 1] - cumulative[segment];
    const double t = segmentLength > 0.0 ? (arcLength - cumulative[segment]) / segmentLength : 0.0;
    return interpolate(points[segment].position, points[segment + 1].position, t);
}

Vec3 tangent_at_arc_length(const std::vector<PathControlPoint>& points,
                           const std::vector<double>& cumulative,
                           double arcLength,
                           std::size_t* cursor = nullptr)
{
    const std::size_t segment = segment_for_arc_length(points, cumulative, arcLength, cursor);
    return normalized(sub(points[segment + 1].position, points[segment].position));
}

Vec3 rotate_rodrigues(Vec3 value, Vec3 axis, double sinTheta, double cosTheta)
{
    const Vec3 unitAxis = scale(axis, 1.0 / sinTheta);
    return add(
        add(scale(value, cosTheta), scale(cross(unitAxis, value), sinTheta)),
        scale(unitAxis, dot(unitAxis, value) * (1.0 - cosTheta)));
}

Vec3 transport_normal(Vec3 previousTangent, Vec3 currentTangent, Vec3 previousNormal)
{
    const Vec3 rotationAxis = cross(previousTangent, currentTangent);
    const double sinTheta = norm(rotationAxis);
    const double cosTheta = dot(previousTangent, currentTangent);

    Vec3 normal = previousNormal;
    if (sinTheta > kEpsilon) {
        normal = rotate_rodrigues(previousNormal, rotationAxis, sinTheta, cosTheta);
    } else if (cosTheta < 0.0) {
        normal = scale(previousNormal, -1.0);
    }

    normal = normalized(reject_from_axis(normal, currentTangent));
    if (norm(normal) == 0.0) {
        normal = fallback_normal(currentTangent);
    }
    return normal;
}

void assign_parallel_transport_frames(std::vector<PathSamplePoint>* samples)
{
    if (samples == 0 || samples->empty()) {
        return;
    }

    Vec3 tangent = (*samples)[0].tangent;
    Vec3 normal = fallback_normal(tangent);
    Vec3 binormal = normalized(cross(tangent, normal));

    (*samples)[0].normal = normal;
    (*samples)[0].binormal = binormal;

    for (std::size_t i = 1; i < samples->size(); ++i) {
        const Vec3 currentTangent = (*samples)[i].tangent;
        normal = transport_normal(tangent, currentTangent, normal);
        binormal = normalized(cross(currentTangent, normal));

        (*samples)[i].normal = normal;
        (*samples)[i].binormal = binormal;
        tangent = currentTangent;
    }
}

Vec3 interpolate_unit(Vec3 a, Vec3 b, double t)
{
    Vec3 value = add(scale(a, 1.0 - t), scale(b, t));
    value = normalized(value);
    if (norm(value) == 0.0) {
        return a;
    }
    return value;
}

} // namespace

XQPath::XQPath()
    : id_(PathId::invalid())
    , interpolation_(PathInterpolation::Polyline)
    , sampleSpacing_(0.0)
{
}

void XQPath::setId(PathId id)
{
    id_ = id;
}

PathId XQPath::id() const
{
    return id_;
}

void XQPath::setInterpolation(PathInterpolation mode)
{
    interpolation_ = mode;
}

PathInterpolation XQPath::interpolation() const
{
    return interpolation_;
}

void XQPath::setControlPoints(const std::vector<PathControlPoint>& points)
{
    controlPoints_ = points;
    samplePoints_.clear();
}

const std::vector<PathControlPoint>& XQPath::controlPoints() const
{
    return controlPoints_;
}

const std::vector<PathSamplePoint>& XQPath::samplePoints() const
{
    return samplePoints_;
}

void XQPath::setSourceImageNode(const NodeId& node)
{
    sourceImageNode_ = node;
}

bool XQPath::hasSourceImageNode() const
{
    return sourceImageNode_.has_value();
}

NodeId XQPath::sourceImageNode() const
{
    if (!sourceImageNode_.has_value()) {
        return NodeId::invalid();
    }
    return *sourceImageNode_;
}

double XQPath::sampleSpacing() const
{
    return sampleSpacing_;
}

XQPath::ResampleStatus XQPath::resample(double sampleSpacing)
{
    if (controlPoints_.size() < 2) {
        return ResampleStatus::NotEnoughPoints;
    }
    if (sampleSpacing <= 0.0) {
        return ResampleStatus::InvalidSpacing;
    }

    const std::vector<double> cumulative = cumulative_lengths(controlPoints_);
    const double totalLength = cumulative.back();
    if (totalLength <= 0.0) {
        return ResampleStatus::NotEnoughPoints;
    }

    // Guard against a hostile (spacing, length) combo that would allocate an
    // unbounded number of samples (a malicious project file). 1e6 samples covers
    // any real centerline; beyond that, reject rather than hang/OOM.
    constexpr std::size_t kMaxSamples = 1'000'000;
    if (totalLength / sampleSpacing > static_cast<double>(kMaxSamples)) {
        return ResampleStatus::InvalidSpacing;
    }

    // TODO spline: Spline currently shares deterministic polyline resampling.
    std::vector<PathSamplePoint> samples;
    std::size_t segmentCursor = 0; // advances monotonically with arcLength
    for (double arcLength = 0.0; arcLength < totalLength; arcLength += sampleSpacing) {
        PathSamplePoint sample = {};
        sample.position = point_at_arc_length(controlPoints_, cumulative, arcLength, &segmentCursor);
        sample.tangent = tangent_at_arc_length(controlPoints_, cumulative, arcLength, &segmentCursor);
        sample.arcLength = arcLength;
        samples.push_back(sample);
    }

    if (samples.empty() || samples.back().arcLength < totalLength) {
        PathSamplePoint endpoint = {};
        endpoint.position = point_at_arc_length(controlPoints_, cumulative, totalLength, &segmentCursor);
        endpoint.tangent = tangent_at_arc_length(controlPoints_, cumulative, totalLength, &segmentCursor);
        endpoint.arcLength = totalLength;
        samples.push_back(endpoint);
    }

    assign_parallel_transport_frames(&samples);
    samplePoints_ = samples;
    sampleSpacing_ = sampleSpacing;
    return ResampleStatus::Ok;
}

XQPath::FrameStatus XQPath::frameAtArcLength(double arcLength, PathFrame* out) const
{
    if (samplePoints_.empty() || out == 0) {
        return FrameStatus::NotResampled;
    }

    if (arcLength <= samplePoints_.front().arcLength) {
        const PathSamplePoint& sample = samplePoints_.front();
        *out = {sample.position, sample.tangent, sample.normal, sample.binormal, sample.arcLength};
        return FrameStatus::Ok;
    }

    if (arcLength >= samplePoints_.back().arcLength) {
        const PathSamplePoint& sample = samplePoints_.back();
        *out = {sample.position, sample.tangent, sample.normal, sample.binormal, sample.arcLength};
        return FrameStatus::Ok;
    }

    for (std::size_t i = 1; i < samplePoints_.size(); ++i) {
        const PathSamplePoint& right = samplePoints_[i];
        if (right.arcLength < arcLength) {
            continue;
        }

        const PathSamplePoint& left = samplePoints_[i - 1];
        const double span = right.arcLength - left.arcLength;
        const double t = span > 0.0 ? (arcLength - left.arcLength) / span : 0.0;

        Vec3 tangent = interpolate_unit(left.tangent, right.tangent, t);
        Vec3 normal = interpolate_unit(left.normal, right.normal, t);
        normal = normalized(reject_from_axis(normal, tangent));
        if (norm(normal) == 0.0) {
            normal = fallback_normal(tangent);
        }
        Vec3 binormal = normalized(cross(tangent, normal));
        normal = normalized(cross(binormal, tangent));

        *out = {
            interpolate(left.position, right.position, t),
            tangent,
            normal,
            binormal,
            arcLength,
        };
        return FrameStatus::Ok;
    }

    const PathSamplePoint& sample = samplePoints_.back();
    *out = {sample.position, sample.tangent, sample.normal, sample.binormal, sample.arcLength};
    return FrameStatus::Ok;
}

XQPath::FrameStatus XQPath::framesForAllSamples(std::vector<PathFrame>* out) const
{
    if (samplePoints_.empty() || out == 0) {
        return FrameStatus::NotResampled;
    }

    out->clear();
    out->reserve(samplePoints_.size());
    for (std::size_t i = 0; i < samplePoints_.size(); ++i) {
        const PathSamplePoint& sample = samplePoints_[i];
        out->push_back(
            {sample.position, sample.tangent, sample.normal, sample.binormal, sample.arcLength});
    }
    return FrameStatus::Ok;
}

} // namespace xq
