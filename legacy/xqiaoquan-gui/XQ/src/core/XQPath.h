#ifndef XQ_CORE_XQ_PATH_H
#define XQ_CORE_XQ_PATH_H

#include "core/GeometryTypes.h"
#include "core/NodeId.h"

#include <optional>
#include <vector>

namespace xq {

using PathId = NodeId;

enum class PathInterpolation {
    Polyline,
    Spline,
};

struct PathControlPoint {
    Point3 position;
};

struct PathFrame {
    Point3 position;
    Vec3 tangent;
    Vec3 normal;
    Vec3 binormal;
    double arcLength;
};

struct PathSamplePoint {
    Point3 position;
    Vec3 tangent;
    Vec3 normal;
    Vec3 binormal;
    double arcLength;
};

class XQPath {
public:
    enum class ResampleStatus {
        Ok,
        NotEnoughPoints,
        InvalidSpacing,
    };

    enum class FrameStatus {
        Ok,
        NotResampled,
    };

    XQPath();

    void setId(PathId id);
    PathId id() const;

    void setInterpolation(PathInterpolation mode);
    PathInterpolation interpolation() const;

    void setControlPoints(const std::vector<PathControlPoint>& points);
    const std::vector<PathControlPoint>& controlPoints() const;
    const std::vector<PathSamplePoint>& samplePoints() const;

    void setSourceImageNode(const NodeId& node);
    bool hasSourceImageNode() const;
    NodeId sourceImageNode() const;

    double sampleSpacing() const;

    ResampleStatus resample(double sampleSpacing);
    FrameStatus frameAtArcLength(double arcLength, PathFrame* out) const;

    // Copies the frame stored at every sample point (one PathFrame per
    // samplePoints() entry, same order) in a single O(M) pass. Requires a prior
    // successful resample(); returns NotResampled otherwise (out untouched).
    // Equivalent to calling frameAtArcLength at each sample's arc length,
    // without the per-call linear scan (O(M^2) -> O(M) for whole-path callers).
    FrameStatus framesForAllSamples(std::vector<PathFrame>* out) const;

private:
    PathId id_;
    PathInterpolation interpolation_;
    std::vector<PathControlPoint> controlPoints_;
    std::vector<PathSamplePoint> samplePoints_;
    double sampleSpacing_;
    std::optional<NodeId> sourceImageNode_;
};

} // namespace xq

#endif // XQ_CORE_XQ_PATH_H
