#include "io/source/MappedGeometrySource.h"

#include "io/blob/Sha256.h"

#include <cassert>
#include <cstdint>
#include <utility>

namespace xq {

namespace {
constexpr std::size_t kF64 = 8;
constexpr std::size_t kI32 = 4;
} // namespace

MappedGeometrySource::MappedGeometrySource(const Inputs& in)
    : mode_(in.mode)
{
    bool ok = true;

    // Surface side: points (F64x3) + tris (I32x3) + faceId (I32x1, count == tris).
    const bool surfaceRequested = !in.points.path.empty() || !in.tris.path.empty();
    if (surfaceRequested) {
        BlobInput faceIdIn = in.faceId;
        faceIdIn.elementCount = in.tris.elementCount; // parallel to tris
        const bool p = initBlob(&points_, in.points, 3, kF64, mode_, &ok);
        const bool t = initBlob(&tris_, in.tris, 3, kI32, mode_, &ok);
        const bool f = initBlob(&faceId_, faceIdIn, 1, kI32, mode_, &ok);
        // A surface is all-or-nothing: points + tris + faceId must all be present.
        if (!(p && t && f)) {
            ok = false;
        }
        hasSurface_ = p && t && f;
    }

    // Tet side: volPoints (F64x3) + tets (I32x4).
    const bool tetRequested = !in.volPoints.path.empty() || !in.tets.path.empty();
    if (tetRequested) {
        const bool vp = initBlob(&volPoints_, in.volPoints, 3, kF64, mode_, &ok);
        const bool tt = initBlob(&tets_, in.tets, 4, kI32, mode_, &ok);
        if (!(vp && tt)) {
            ok = false;
        }
        hasTet_ = vp && tt;
    }

    if (!hasSurface_ && !hasTet_) {
        ok = false; // an empty source is invalid
    }

    valid_ = ok;
}

bool MappedGeometrySource::initBlob(Blob* blob, const BlobInput& in,
                                    std::size_t components, std::size_t elemSize,
                                    IntegrityMode mode, bool* ok)
{
    if (in.path.empty()) {
        blob->present = false;
        return false; // absent role (not an error by itself)
    }
    blob->elementCount = in.elementCount;
    blob->components = components;
    blob->elemSize = elemSize;
    blob->expectedSha256 = in.expectedSha256;

    blob->mapped = map_file_readonly(in.path);
    if (!blob->mapped.ok) {
        *ok = false;
        return false;
    }
    if (blob->mapped.size != blob->totalBytes()) {
        *ok = false;
        return false;
    }

    if (mode == IntegrityMode::SegmentedMerkle) {
        if (!load_sidecar(in.merklePath, &blob->sidecar)) {
            *ok = false;
            return false;
        }
        if (!blob->sidecar.verify_root()
            || blob->sidecar.blobBytes != static_cast<std::uint64_t>(blob->mapped.size)
            || blob->sidecar.segmentBytes == 0) {
            *ok = false;
            return false;
        }
        blob->segChecked.assign(static_cast<std::size_t>(blob->sidecar.segCount), false);
    }

    blob->present = true;
    return true;
}

bool MappedGeometrySource::verifyBlob(const Blob& blob) const
{
    if (!blob.present) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    ++stats_.acquireCount;

    const std::size_t total = blob.mapped.size;
    if (total == 0) {
        return true;
    }

    if (mode_ == IntegrityMode::FullVerify) {
        if (blob.fullVerified) {
            return true;
        }
        const std::string digest = Sha256::hashHex(blob.mapped.base, total);
        stats_.bytesHashed += static_cast<std::uint64_t>(total);
        // Compare against the content-address anchor (BufferRef.sha256) when the
        // caller supplied one: a mismatch means the blob was corrupted/tampered.
        // No anchor (empty) -> hash once + memoize but cannot compare (the M8a
        // anchor then lives only in the main document); back-compat.
        if (!blob.expectedSha256.empty() && digest != blob.expectedSha256) {
            return false; // corrupt / tampered blob
        }
        blob.fullVerified = true;
        return true;
    }

    // SegmentedMerkle: hash every segment of this blob once (a geometry acquire
    // always touches the whole blob), memoized.
    const std::size_t segBytes = static_cast<std::size_t>(blob.sidecar.segmentBytes);
    if (segBytes == 0) {
        return false;
    }
    const std::uint64_t lastSeg = static_cast<std::uint64_t>((total - 1) / segBytes);
    for (std::uint64_t seg = 0; seg <= lastSeg; ++seg) {
        if (seg >= blob.sidecar.segCount) {
            return false;
        }
        const std::size_t idx = static_cast<std::size_t>(seg);
        if (blob.segChecked[idx]) {
            continue;
        }
        if (!blob.sidecar.verify_segment(blob.mapped.base, total, seg)) {
            return false;
        }
        blob.segChecked[idx] = true;
        ++stats_.segmentsChecked;
        std::size_t n = segBytes;
        const std::size_t off = idx * segBytes;
        if (off + n > total) {
            n = total - off;
        }
        stats_.bytesHashed += static_cast<std::uint64_t>(n);
    }
    return true;
}

GeometryMeta MappedGeometrySource::meta() const
{
    GeometryMeta m;
    if (!valid_) {
        return m;
    }
    m.valid = true;
    m.pointCount = hasSurface_ ? points_.elementCount
                              : (hasTet_ ? volPoints_.elementCount : 0);
    m.triangleCount = hasSurface_ ? tris_.elementCount : 0;
    m.tetCount = hasTet_ ? tets_.elementCount : 0;
    return m;
}

GeometryLease<Point3> MappedGeometrySource::acquire_points() const
{
    if (!valid_) {
        return GeometryLease<Point3>::empty();
    }
    const Blob& src = hasSurface_ ? points_ : volPoints_;
    if (!src.present) {
        return GeometryLease<Point3>::empty();
    }
    if (!verifyBlob(src)) {
        return GeometryLease<Point3>::empty();
    }
    // Point3 is packed double[3]; mmap base is page-aligned (>= 8B), so a
    // reinterpret to const Point3* is a valid zero-copy view.
    static_assert(sizeof(Point3) == 3 * sizeof(double), "Point3 must be packed double[3]");
    assert(reinterpret_cast<std::uintptr_t>(src.mapped.base) % alignof(Point3) == 0);
    const Point3* data = reinterpret_cast<const Point3*>(src.mapped.base);
    return GeometryLease<Point3>::borrow(
        src.mapped.keepalive, ReadSpan<Point3>(data, src.elementCount));
}

TriangleLease MappedGeometrySource::acquire_triangles() const
{
    if (!valid_ || !hasSurface_) {
        return TriangleLease::empty();
    }
    if (!verifyBlob(tris_) || !verifyBlob(faceId_)) {
        return TriangleLease::empty();
    }
    assert(reinterpret_cast<std::uintptr_t>(tris_.mapped.base) % alignof(SourceTriangle) == 0);
    const SourceTriangle* triData =
        reinterpret_cast<const SourceTriangle*>(tris_.mapped.base);
    assert(reinterpret_cast<std::uintptr_t>(faceId_.mapped.base) % alignof(int) == 0);
    const int* faceData = reinterpret_cast<const int*>(faceId_.mapped.base);
    // Zero copy on BOTH spans: each borrows its own blob's mmap control block
    // (M9b-A borrow-faceId). A live lease pins both blobs.
    return TriangleLease::borrow_borrowed_faceids(
        tris_.mapped.keepalive,
        ReadSpan<SourceTriangle>(triData, tris_.elementCount),
        faceId_.mapped.keepalive,
        ReadSpan<int>(faceData, faceId_.elementCount));
}

GeometryLease<SourceTet> MappedGeometrySource::acquire_tetrahedra() const
{
    if (!valid_ || !hasTet_) {
        return GeometryLease<SourceTet>::empty();
    }
    if (!verifyBlob(tets_)) {
        return GeometryLease<SourceTet>::empty();
    }
    assert(reinterpret_cast<std::uintptr_t>(tets_.mapped.base) % alignof(SourceTet) == 0);
    const SourceTet* data = reinterpret_cast<const SourceTet*>(tets_.mapped.base);
    return GeometryLease<SourceTet>::borrow(
        tets_.mapped.keepalive, ReadSpan<SourceTet>(data, tets_.elementCount));
}

MappedGeometrySource::IntegrityStats MappedGeometrySource::stats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

} // namespace xq
