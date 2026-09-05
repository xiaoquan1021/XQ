#include "MappedGeometrySource.h"

#include <vector>

namespace xq {
namespace probe {

MappedGeometrySource::MappedGeometrySource(const Inputs& in)
    : in_(in)
{
    hasSurface_ = !in_.pointsPath.empty();
    hasTet_ = !in_.tetPointsPath.empty();

    if (hasSurface_) {
        points_ = map_file_readonly(in_.pointsPath);
        tris_ = map_file_readonly(in_.trisPath);
        faceId_ = map_file_readonly(in_.faceIdPath);
        mappedOk_ = mappedOk_ && points_.ok && tris_.ok && faceId_.ok;
    }
    if (hasTet_) {
        tetPoints_ = map_file_readonly(in_.tetPointsPath);
        tets_ = map_file_readonly(in_.tetsPath);
        mappedOk_ = mappedOk_ && tetPoints_.ok && tets_.ok;
    }
}

GeometryMeta MappedGeometrySource::meta() const
{
    GeometryMeta m;
    m.valid = mappedOk_;
    // points = surface points if present, else tet points (a source is one or the
    // other here; the probe builds two separate sources).
    if (hasSurface_) {
        m.pointCount = in_.pointCount;
        m.triangleCount = in_.triCount;
    }
    if (hasTet_) {
        m.pointCount = in_.tetPointCount; // tet-only source exposes tet points
        m.tetCount = in_.tetCount;
    }
    return m;
}

GeometryLease<Point3> MappedGeometrySource::acquire_points() const
{
    // Zero-copy AoS reinterpret: the mapped F64x3 bytes ARE a contiguous Point3
    // array on a little-endian host (Point3 = {double x,y,z}, 24B, no padding).
    // MapViewOfFile returns a page-aligned base, so the 8-byte alignment Point3
    // needs is satisfied. This is defect ④'s core experiment.
    if (hasSurface_ && points_.ok) {
        ReadSpan<Point3> span(static_cast<const Point3*>(points_.base), in_.pointCount);
        return GeometryLease<Point3>::borrow(points_.keepalive, span);
    }
    if (hasTet_ && tetPoints_.ok) {
        ReadSpan<Point3> span(static_cast<const Point3*>(tetPoints_.base), in_.tetPointCount);
        return GeometryLease<Point3>::borrow(tetPoints_.keepalive, span);
    }
    return GeometryLease<Point3>::empty();
}

TriangleLease MappedGeometrySource::acquire_triangles() const
{
    if (!hasSurface_ || !tris_.ok || !faceId_.ok) {
        return TriangleLease::empty();
    }
    // tris: zero-copy reinterpret to SourceTriangle (= std::array<int,3>, 12B).
    ReadSpan<SourceTriangle> triSpan(
        static_cast<const SourceTriangle*>(tris_.base), in_.triCount);

    // faceId: the M8b-1 TriangleLease::borrow takes an owned std::vector<int>, so
    // even though faceId is its own mapped blob we must copy it in. At 20M tris
    // this is the ~80MB-per-acquire materialization the report calls out (defect:
    // faceId could be borrowed if the lease accepted a span).
    const int* faceBase = static_cast<const int*>(faceId_.base);
    std::vector<int> faceIds(faceBase, faceBase + in_.triCount);

    return TriangleLease::borrow(tris_.keepalive, triSpan, std::move(faceIds));
}

GeometryLease<SourceTet> MappedGeometrySource::acquire_tetrahedra() const
{
    if (!hasTet_ || !tets_.ok) {
        return GeometryLease<SourceTet>::empty();
    }
    // Zero-copy reinterpret to SourceTet (= std::array<int,4>, 16B).
    ReadSpan<SourceTet> span(static_cast<const SourceTet*>(tets_.base), in_.tetCount);
    return GeometryLease<SourceTet>::borrow(tets_.keepalive, span);
}

} // namespace probe
} // namespace xq
