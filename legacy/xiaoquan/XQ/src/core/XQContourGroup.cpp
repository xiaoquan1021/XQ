#include "core/XQContourGroup.h"

#include <algorithm>

namespace xq {

XQContourGroup::XQContourGroup()
    : id_(ContourGroupId::invalid())
{
}

void XQContourGroup::setId(ContourGroupId id)
{
    id_ = id;
}

ContourGroupId XQContourGroup::id() const
{
    return id_;
}

void XQContourGroup::setSourcePathNode(const NodeId& node)
{
    sourcePathNode_ = node;
}

bool XQContourGroup::hasSourcePathNode() const
{
    return sourcePathNode_.has_value();
}

NodeId XQContourGroup::sourcePathNode() const
{
    if (!sourcePathNode_.has_value()) {
        return NodeId::invalid();
    }
    return *sourcePathNode_;
}

void XQContourGroup::addContour(const XQContour& contour)
{
    contours_.push_back(contour);
}

const std::vector<XQContour>& XQContourGroup::contours() const
{
    return contours_;
}

std::vector<XQContour> XQContourGroup::orderedByPathPosition() const
{
    std::vector<XQContour> ordered = contours_;
    std::stable_sort(ordered.begin(), ordered.end(), [](const XQContour& a, const XQContour& b) {
        return a.pathArcLength < b.pathArcLength;
    });
    return ordered;
}

void XQContourGroup::projectToFrame(const ContourFrame& frame, const Point3& worldPoint, double* u, double* v)
{
    const Vec3 offset = sub(worldPoint, frame.origin);
    if (u != 0) {
        *u = dot(offset, frame.xAxis);
    }
    if (v != 0) {
        *v = dot(offset, frame.yAxis);
    }
}

Point3 XQContourGroup::unprojectFromFrame(const ContourFrame& frame, double u, double v)
{
    return add(add(frame.origin, scale(frame.xAxis, u)), scale(frame.yAxis, v));
}

} // namespace xq
