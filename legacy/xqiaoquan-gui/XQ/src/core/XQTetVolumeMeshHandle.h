#ifndef XQ_CORE_XQ_TET_VOLUME_MESH_HANDLE_H
#define XQ_CORE_XQ_TET_VOLUME_MESH_HANDLE_H

#include "core/GeometryTypes.h"

#include <array>
#include <cstddef>
#include <vector>

namespace xq {

// A real, XQ-owned tetrahedral volume mesh: a point list plus tetrahedra given
// as four indices into that list. Mirrors XQTriangleSurfaceGeometryHandle (the
// real surface geometry) for the volume side and carries the actual generated
// cells (centroid star tetrahedralization). Zero external dependencies (no
// VTK/TetGen): just XQ value types.
//
// A handle is valid() when it is non-empty and every tet references four
// distinct, in-range point indices. Geometric validity (volume sign,
// degeneracy) is left to the service's quality summary, not enforced here.
class XQTetVolumeMeshHandle {
public:
    using Tet = std::array<int, 4>;

    XQTetVolumeMeshHandle();

    // Adds a point and returns its index.
    int addPoint(const Point3& p);

    // Adds a tetrahedron by four point indices. No validation here; call
    // is_valid() afterwards (or rely on the service to feed valid indices).
    void addTet(int a, int b, int c, int d);

    std::size_t pointCount() const;
    std::size_t tetCount() const;

    const Point3& point(std::size_t i) const;
    const Tet& tet(std::size_t i) const;

    const std::vector<Point3>& points() const;
    const std::vector<Tet>& tets() const;

    // True when non-empty and every tet references four distinct, in-range
    // point indices.
    bool is_valid() const;

private:
    std::vector<Point3> points_;
    std::vector<Tet> tets_;
};

} // namespace xq

#endif // XQ_CORE_XQ_TET_VOLUME_MESH_HANDLE_H
