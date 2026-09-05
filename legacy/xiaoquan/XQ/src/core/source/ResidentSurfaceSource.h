#ifndef XQ_CORE_SOURCE_RESIDENT_SURFACE_SOURCE_H
#define XQ_CORE_SOURCE_RESIDENT_SURFACE_SOURCE_H

#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/source/IGeometrySource.h"

#include <memory>

namespace xq {

// Adapts an XQTriangleSurfaceGeometryHandle as an IGeometrySource. Points and
// triangles are borrowed (zero copy); faceIds are materialized once per acquire
// (the handle exposes only triangleFaceId(i)). It holds no tetrahedra:
// meta().tetCount == 0 and acquire_tetrahedra() returns an empty lease.
class ResidentSurfaceSource : public IGeometrySource {
public:
    explicit ResidentSurfaceSource(
        std::shared_ptr<const XQTriangleSurfaceGeometryHandle> handle);

    GeometryMeta meta() const override;
    GeometryLease<Point3> acquire_points() const override;
    TriangleLease acquire_triangles() const override;
    GeometryLease<SourceTet> acquire_tetrahedra() const override;

private:
    std::shared_ptr<const XQTriangleSurfaceGeometryHandle> handle_;
};

} // namespace xq

#endif // XQ_CORE_SOURCE_RESIDENT_SURFACE_SOURCE_H
