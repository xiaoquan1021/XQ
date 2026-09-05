#include "core/source/ResidentTetSource.h"

#include <utility>
#include <vector>

namespace xq {

ResidentTetSource::ResidentTetSource(std::shared_ptr<const XQTetVolumeMeshHandle> handle)
    : handle_(std::move(handle))
{
}

GeometryMeta ResidentTetSource::meta() const
{
    GeometryMeta m;
    if (!handle_) {
        return m;
    }
    m.valid = true;
    m.pointCount = handle_->pointCount();
    m.triangleCount = 0;
    m.tetCount = handle_->tetCount();
    return m;
}

GeometryLease<Point3> ResidentTetSource::acquire_points() const
{
    if (!handle_) {
        return GeometryLease<Point3>::empty();
    }
    const std::vector<Point3>& pts = handle_->points();
    return GeometryLease<Point3>::borrow(
        handle_, ReadSpan<Point3>(pts.data(), pts.size()));
}

TriangleLease ResidentTetSource::acquire_triangles() const
{
    return TriangleLease::empty();
}

GeometryLease<SourceTet> ResidentTetSource::acquire_tetrahedra() const
{
    if (!handle_) {
        return GeometryLease<SourceTet>::empty();
    }
    const std::vector<SourceTet>& tets = handle_->tets();
    return GeometryLease<SourceTet>::borrow(
        handle_, ReadSpan<SourceTet>(tets.data(), tets.size()));
}

} // namespace xq
