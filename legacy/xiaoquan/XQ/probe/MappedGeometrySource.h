#ifndef XQ_PROBE_MAPPED_GEOMETRY_SOURCE_H
#define XQ_PROBE_MAPPED_GEOMETRY_SOURCE_H

// scale-probe — real Win32 mmap-backed IGeometrySource (task 06-30-scale-probe).
// Disposable spike code; only compiles under -DXQ_ENABLE_SCALE_PROBE=ON.
//
// Maps the M8a geometry blobs (points F64x3, tris I32x3, faceId I32x1, or
// volPoints F64x3, tets I32x4) read-only and exposes them through IGeometrySource.
// This is where AoS-vs-SoA (defect ④) gets stress-tested: because the blob bytes
// are little-endian F64 triples and Point3 is {double x,y,z} with no padding, the
// mapped pages can be reinterpret_cast straight to const Point3* and handed out as
// a zero-copy ReadSpan<Point3> — exactly the M8b-1 AC10 contract, but now over
// mmap'd disk instead of a resident vector. Same for SourceTet (int[4]).
//
// faceId is the known non-zero-copy spot: M8b-1's TriangleLease materializes the
// faceId vector (the handle exposes no faceId accessor). Here faceId IS its own
// blob, so we map it and could borrow it directly — but the M8b-1 TriangleLease
// API only takes an owned std::vector<int>, so we still copy. That copy cost at
// 20M tris is one of the numbers the probe reports.

#include "MmapBlob.h"

#include "core/GeometryTypes.h"
#include "core/source/IGeometrySource.h"

#include <cstddef>
#include <string>

namespace xq {
namespace probe {

// One mapped geometry source. Either a surface (points+tris+faceId) or a tet
// volume (volPoints+tets); the unused kind maps nothing and its acquire returns
// an empty lease (partial source, per IGeometrySource contract).
class MappedGeometrySource : public IGeometrySource {
public:
    struct Inputs {
        // Surface side (empty path = absent).
        std::string pointsPath;
        std::size_t pointCount = 0;
        std::string trisPath;
        std::size_t triCount = 0;
        std::string faceIdPath;
        // Tet side (empty path = absent).
        std::string tetPointsPath;
        std::size_t tetPointCount = 0;
        std::string tetsPath;
        std::size_t tetCount = 0;
    };

    explicit MappedGeometrySource(const Inputs& in);

    bool valid() const { return mappedOk_; }

    // IGeometrySource
    GeometryMeta meta() const override;
    GeometryLease<Point3> acquire_points() const override;
    TriangleLease acquire_triangles() const override;
    GeometryLease<SourceTet> acquire_tetrahedra() const override;

private:
    Inputs in_;
    bool mappedOk_ = true;

    // Mapped blobs (only the present kinds are mapped).
    MmapBlob points_;
    MmapBlob tris_;
    MmapBlob faceId_;
    MmapBlob tetPoints_;
    MmapBlob tets_;

    bool hasSurface_ = false;
    bool hasTet_ = false;
};

} // namespace probe
} // namespace xq

#endif // XQ_PROBE_MAPPED_GEOMETRY_SOURCE_H
