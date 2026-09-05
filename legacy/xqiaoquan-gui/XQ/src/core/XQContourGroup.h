#ifndef XQ_CORE_XQ_CONTOUR_GROUP_H
#define XQ_CORE_XQ_CONTOUR_GROUP_H

#include "core/GeometryTypes.h"
#include "core/NodeId.h"

#include <optional>
#include <vector>

namespace xq {

using ContourGroupId = NodeId;
using ContourId = NodeId;

enum class ContourType {
    Manual,
    Circle,
    Ellipse,
    SplinePolygon,
    LevelSetResult,
    ThresholdResult,
};

struct ContourFrame {
    Point3 origin;
    Vec3 normal;
    Vec3 xAxis;
    Vec3 yAxis;
};

struct XQContour {
    ContourId contourId;
    double pathArcLength;
    ContourFrame frame;
    ContourType type;
    std::vector<Point3> points;
    bool closed;
};

class XQContourGroup {
public:
    XQContourGroup();

    void setId(ContourGroupId id);
    ContourGroupId id() const;

    void setSourcePathNode(const NodeId& node);
    bool hasSourcePathNode() const;
    NodeId sourcePathNode() const;

    void addContour(const XQContour& contour);
    const std::vector<XQContour>& contours() const;
    std::vector<XQContour> orderedByPathPosition() const;

    static void projectToFrame(const ContourFrame& frame, const Point3& worldPoint, double* u, double* v);
    static Point3 unprojectFromFrame(const ContourFrame& frame, double u, double v);

private:
    ContourGroupId id_;
    std::optional<NodeId> sourcePathNode_;
    std::vector<XQContour> contours_;
};

} // namespace xq

#endif // XQ_CORE_XQ_CONTOUR_GROUP_H
