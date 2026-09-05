#ifndef XQ_SERVICES_MESHING_MESH_QUALITY_H
#define XQ_SERVICES_MESHING_MESH_QUALITY_H

#include "core/GeometryTypes.h"

#include <cmath>

namespace xq {
namespace meshing {

// Normalized element shape-quality metrics in (0, 1], mirroring VTK's
// vtkMeshQuality normalized shape measures: 1.0 for the ideal (equilateral /
// regular) element, approaching 0 as the element degenerates. Degenerate
// elements (near-zero area / volume) return ~0 so the quality summary exposes
// them rather than hiding them.

// Triangle normalized shape: q = 4*sqrt(3) * Area / (l0^2 + l1^2 + l2^2).
// Equilateral triangle -> 1.0; sliver/needle -> 0.
inline double triangleQuality(const Point3& a, const Point3& b, const Point3& c)
{
    const double l0 = dot(sub(b, a), sub(b, a));
    const double l1 = dot(sub(c, b), sub(c, b));
    const double l2 = dot(sub(a, c), sub(a, c));
    const double sumSq = l0 + l1 + l2;
    if (sumSq <= 0.0) {
        return 0.0;
    }
    const double area = 0.5 * norm(cross(sub(b, a), sub(c, a)));
    const double kSqrt3 = 1.7320508075688772;
    return (4.0 * kSqrt3 * area) / sumSq;
}

// Signed volume of tetrahedron (a,b,c,d) = dot(cross(b-a,c-a), d-a) / 6.
inline double tetSignedVolume(const Point3& a, const Point3& b, const Point3& c,
                              const Point3& d)
{
    return dot(cross(sub(b, a), sub(c, a)), sub(d, a)) / 6.0;
}

// Tetrahedron normalized shape: q = 12 * (3*|V|)^(2/3) / (sum of 6 edge^2).
// Regular tetrahedron -> 1.0; flat/inverted-but-thin tet -> 0. Uses |V| so an
// inverted tet still scores by shape; the volume *sign* is reported separately
// for fold detection.
inline double tetQuality(const Point3& a, const Point3& b, const Point3& c,
                         const Point3& d)
{
    const double e0 = dot(sub(b, a), sub(b, a));
    const double e1 = dot(sub(c, a), sub(c, a));
    const double e2 = dot(sub(d, a), sub(d, a));
    const double e3 = dot(sub(c, b), sub(c, b));
    const double e4 = dot(sub(d, b), sub(d, b));
    const double e5 = dot(sub(d, c), sub(d, c));
    const double sumSq = e0 + e1 + e2 + e3 + e4 + e5;
    if (sumSq <= 0.0) {
        return 0.0;
    }
    const double absV = std::fabs(tetSignedVolume(a, b, c, d));
    if (absV <= 0.0) {
        return 0.0;
    }
    return (12.0 * std::pow(3.0 * absV, 2.0 / 3.0)) / sumSq;
}

} // namespace meshing
} // namespace xq

#endif // XQ_SERVICES_MESHING_MESH_QUALITY_H
