#include "core/source/ResidentSurfaceSource.h"

#include <utility>
#include <vector>

namespace xq {

ResidentSurfaceSource::ResidentSurfaceSource(
    std::shared_ptr<const XQTriangleSurfaceGeometryHandle> handle)
    : handle_(std::move(handle))
{
}

GeometryMeta ResidentSurfaceSource::meta() const
{
    GeometryMeta m;
    if (!handle_) {
        return m;
    }
    m.valid = true;
    m.pointCount = handle_->pointCount();
    m.triangleCount = handle_->triangleCount();
    m.tetCount = 0;
    return m;
}

GeometryLease<Point3> ResidentSurfaceSource::acquire_points() const
{
    if (!handle_) {
        return GeometryLease<Point3>::empty();
    }
    const std::vector<Point3>& pts = handle_->points();
    return GeometryLease<Point3>::borrow(
        handle_, ReadSpan<Point3>(pts.data(), pts.size()));
}

TriangleLease ResidentSurfaceSource::acquire_triangles() const
{
    if (!handle_) {
        return TriangleLease::empty();
    }
    const std::vector<SourceTriangle>& tris = handle_->triangles();
    // faceIds are stored contiguously in the handle (parallel to triangles_):
    // borrow them zero-copy instead of the old O(N) triangleFaceId(i)
    // materialization. Both spans pin the same handle (M9b-A).
    const std::vector<int>& faceIds = handle_->triangleFaceIds();
    return TriangleLease::borrow_borrowed_faceids(
        handle_, ReadSpan<SourceTriangle>(tris.data(), tris.size()),
        handle_, ReadSpan<int>(faceIds.data(), faceIds.size()));
}

GeometryLease<SourceTet> ResidentSurfaceSource::acquire_tetrahedra() const
{
    return GeometryLease<SourceTet>::empty();
}

} // namespace xq
