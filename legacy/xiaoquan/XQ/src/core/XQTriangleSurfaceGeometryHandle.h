#ifndef XQ_CORE_XQ_TRIANGLE_SURFACE_GEOMETRY_HANDLE_H
#define XQ_CORE_XQ_TRIANGLE_SURFACE_GEOMETRY_HANDLE_H

#include "core/GeometryTypes.h"

#include <array>
#include <cstddef>
#include <vector>

namespace xq {

// A real, XQ-owned triangle surface mesh: a point list plus triangles given as
// three indices into that list. Unlike SurfaceGeometryHandle (a count-only
// handle used by the reader path), this carries the actual generated geometry
// produced by lofting / capping. Zero external dependencies (no VTK/OCCT): just
// XQ value types.
//
// A handle is valid() when it is non-empty and every triangle index lies within
// the point list. Degenerate triangles (repeated indices) are rejected by the
// same check so downstream closedness reasoning stays sound.
class XQTriangleSurfaceGeometryHandle {
public:
    using Triangle = std::array<int, 3>;

    XQTriangleSurfaceGeometryHandle();

    // Adds a point and returns its index.
    int addPoint(const Point3& p);

    // Adds a triangle by three point indices. No validation here; call
    // is_valid() afterwards (or rely on the service to feed valid indices). The
    // original (untagged) overload assigns faceId 0 so M3 callers and tests stay
    // unchanged; the tagged overload stamps a model/boundary face id so M4's
    // surface/volume meshers can map triangles back to faces without rebuilding
    // the association geometrically.
    void addTriangle(int a, int b, int c);
    void addTriangle(int a, int b, int c, int faceId);

    std::size_t pointCount() const;
    std::size_t triangleCount() const;

    const Point3& point(std::size_t i) const;
    const Triangle& triangle(std::size_t i) const;

    // Per-triangle face id (0 == untagged). Synchronized with triangles_.
    int triangleFaceId(std::size_t i) const;
    void setTriangleFaceId(std::size_t i, int faceId);

    const std::vector<Point3>& points() const;
    const std::vector<Triangle>& triangles() const;
    // Contiguous per-triangle faceId buffer (parallel to triangles()). Lets a
    // Source borrow faceIds zero-copy instead of materializing via
    // triangleFaceId(i) (M9b-A).
    const std::vector<int>& triangleFaceIds() const;

    // True when non-empty and every triangle references three distinct,
    // in-range point indices.
    bool is_valid() const;

private:
    std::vector<Point3> points_;
    std::vector<Triangle> triangles_;
    std::vector<int> triangleFaceIds_; // parallel to triangles_; 0 == untagged
};

} // namespace xq

#endif // XQ_CORE_XQ_TRIANGLE_SURFACE_GEOMETRY_HANDLE_H
