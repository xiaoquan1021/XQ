#ifndef XQ_CORE_SOURCE_RESIDENT_TET_SOURCE_H
#define XQ_CORE_SOURCE_RESIDENT_TET_SOURCE_H

#include "core/XQTetVolumeMeshHandle.h"
#include "core/source/IGeometrySource.h"

#include <memory>

namespace xq {

// Adapts an XQTetVolumeMeshHandle as an IGeometrySource. Points and tetrahedra
// are borrowed (zero copy). It holds no triangles: meta().triangleCount == 0
// and acquire_triangles() returns an empty lease (both spans empty).
class ResidentTetSource : public IGeometrySource {
public:
    explicit ResidentTetSource(std::shared_ptr<const XQTetVolumeMeshHandle> handle);

    GeometryMeta meta() const override;
    GeometryLease<Point3> acquire_points() const override;
    TriangleLease acquire_triangles() const override;
    GeometryLease<SourceTet> acquire_tetrahedra() const override;

private:
    std::shared_ptr<const XQTetVolumeMeshHandle> handle_;
};

} // namespace xq

#endif // XQ_CORE_SOURCE_RESIDENT_TET_SOURCE_H
