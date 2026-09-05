#ifndef XQ_CORE_SOURCE_I_GEOMETRY_SOURCE_H
#define XQ_CORE_SOURCE_I_GEOMETRY_SOURCE_H

#include "core/GeometryTypes.h"
#include "core/source/ReadLease.h"
#include "core/source/SourceViews.h"

namespace xq {

// Read-only geometry access contract. One acquire per entity kind, each a
// single virtual dispatch returning a contiguous view. A source may hold only
// part of a geometry (surface has no tets; tet mesh has no triangles): meta()
// reports the missing count as 0 and the corresponding acquire returns an empty
// lease (span().empty()). That is not an error.
class IGeometrySource {
public:
    virtual ~IGeometrySource() = default;

    virtual GeometryMeta meta() const = 0;

    virtual GeometryLease<Point3> acquire_points() const = 0;

    // Triangles plus the parallel faceId span (equal length).
    virtual TriangleLease acquire_triangles() const = 0;

    virtual GeometryLease<SourceTet> acquire_tetrahedra() const = 0;
};

} // namespace xq

#endif // XQ_CORE_SOURCE_I_GEOMETRY_SOURCE_H
