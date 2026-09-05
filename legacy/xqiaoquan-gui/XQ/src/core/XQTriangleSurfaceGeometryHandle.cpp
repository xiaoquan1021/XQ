#include "core/XQTriangleSurfaceGeometryHandle.h"

namespace xq {

XQTriangleSurfaceGeometryHandle::XQTriangleSurfaceGeometryHandle() = default;

int XQTriangleSurfaceGeometryHandle::addPoint(const Point3& p)
{
    points_.push_back(p);
    return static_cast<int>(points_.size()) - 1;
}

void XQTriangleSurfaceGeometryHandle::addTriangle(int a, int b, int c)
{
    triangles_.push_back(Triangle{a, b, c});
    triangleFaceIds_.push_back(0);
}

void XQTriangleSurfaceGeometryHandle::addTriangle(int a, int b, int c, int faceId)
{
    triangles_.push_back(Triangle{a, b, c});
    triangleFaceIds_.push_back(faceId);
}

std::size_t XQTriangleSurfaceGeometryHandle::pointCount() const
{
    return points_.size();
}

std::size_t XQTriangleSurfaceGeometryHandle::triangleCount() const
{
    return triangles_.size();
}

const Point3& XQTriangleSurfaceGeometryHandle::point(std::size_t i) const
{
    return points_[i];
}

const XQTriangleSurfaceGeometryHandle::Triangle&
XQTriangleSurfaceGeometryHandle::triangle(std::size_t i) const
{
    return triangles_[i];
}

int XQTriangleSurfaceGeometryHandle::triangleFaceId(std::size_t i) const
{
    return triangleFaceIds_[i];
}

void XQTriangleSurfaceGeometryHandle::setTriangleFaceId(std::size_t i, int faceId)
{
    triangleFaceIds_[i] = faceId;
}

const std::vector<Point3>& XQTriangleSurfaceGeometryHandle::points() const
{
    return points_;
}

const std::vector<XQTriangleSurfaceGeometryHandle::Triangle>&
XQTriangleSurfaceGeometryHandle::triangles() const
{
    return triangles_;
}

const std::vector<int>& XQTriangleSurfaceGeometryHandle::triangleFaceIds() const
{
    return triangleFaceIds_;
}

bool XQTriangleSurfaceGeometryHandle::is_valid() const
{
    if (points_.empty() || triangles_.empty()) {
        return false;
    }
    const int pointCount = static_cast<int>(points_.size());
    for (const Triangle& t : triangles_) {
        for (int idx : t) {
            if (idx < 0 || idx >= pointCount) {
                return false;
            }
        }
        if (t[0] == t[1] || t[1] == t[2] || t[0] == t[2]) {
            return false;
        }
    }
    return true;
}

} // namespace xq
