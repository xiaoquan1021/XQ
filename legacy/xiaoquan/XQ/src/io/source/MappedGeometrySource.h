#ifndef XQ_IO_SOURCE_MAPPED_GEOMETRY_SOURCE_H
#define XQ_IO_SOURCE_MAPPED_GEOMETRY_SOURCE_H

#include "core/GeometryTypes.h"
#include "core/source/IGeometrySource.h"
#include "io/blob/MerkleSidecar.h"
#include "io/source/MmapBlob.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace xq {

// Production IGeometrySource backed by Win32 read-only memory mappings of the
// content-addressed geometry blobs of one asset (M9b-A). A surface asset maps
// points (F64x3) + tris (I32x3) + faceId (I32x1); a tet asset maps volPoints
// (F64x3) + tets (I32x4). A partial source (only the surface or only the tet
// kind present) reports the missing counts as 0 and returns empty leases for
// the absent kind, per the IGeometrySource contract.
//
// All acquires are zero copy: the mapped bytes are reinterpret_cast straight to
// const Point3* / SourceTriangle* / SourceTet* and borrowed (keepalive = the
// per-blob mmap control block). acquire_triangles borrows BOTH triangles and
// faceId via TriangleLease::borrow_borrowed_faceids (no materialization). This
// class never touches BlobStore::get (which re-decodes element-by-element); it
// maps once and reinterprets (scale-probe defect 2 conclusion).
//
// Integrity is checked lazily per blob and memoized. FullVerify hashes a blob
// once on first touch and is the compatibility fallback for missing sidecars;
// SegmentedMerkle verifies only touched segments against a per-blob .merkle
// sidecar. A failed check yields an invalid/empty lease.
class MappedGeometrySource : public IGeometrySource {
public:
    enum class IntegrityMode {
        FullVerify,     // hash whole blob once on first touch (no sidecar)
        SegmentedMerkle // per-segment lazy hashing against a .merkle sidecar
    };

    // One blob's location + decoded shape. An empty path marks the role absent.
    // merklePath is only consulted under SegmentedMerkle. expectedSha256
    // (optional, lower-case hex = BufferRef.sha256) is the content-address
    // anchor: under FullVerify the whole-blob digest is compared against it and a
    // mismatch yields an invalid source (tamper detection). Empty = no anchor
    // (hash still memoized, not compared -- back-compat).
    struct BlobInput {
        std::string path;
        std::size_t elementCount = 0; // points/tris/tets count (NOT scalar count)
        std::string merklePath;       // optional; SegmentedMerkle only
        std::string expectedSha256;   // optional; FullVerify anchor
    };

    // Inputs for one geometry asset. Surface side = points+tris+faceId; tet side
    // = volPoints+tets. Leave a role's path empty to mark it absent. faceId's
    // elementCount must equal tris.elementCount (parallel); it is taken from tris.
    struct Inputs {
        BlobInput points;    // F64x3
        BlobInput tris;      // I32x3
        BlobInput faceId;    // I32x1 (count == tris count)
        BlobInput volPoints; // F64x3
        BlobInput tets;      // I32x4
        IntegrityMode mode = IntegrityMode::FullVerify;
    };

    // Counters exposed for tests to prove laziness and caching (mirrors
    // MappedVoxelSource::IntegrityStats).
    struct IntegrityStats {
        std::uint64_t bytesHashed = 0;
        std::uint64_t segmentsChecked = 0;
        std::uint64_t acquireCount = 0;
    };

    explicit MappedGeometrySource(const Inputs& in);

    // True when every present role mapped and its size matches count*comp*sizeof,
    // and (SegmentedMerkle) its sidecar root is sound.
    bool valid() const { return valid_; }

    GeometryMeta meta() const override;
    GeometryLease<Point3> acquire_points() const override;
    TriangleLease acquire_triangles() const override;
    GeometryLease<SourceTet> acquire_tetrahedra() const override;

    IntegrityStats stats() const;

private:
    // One mapped blob with its own verify memo and (SegmentedMerkle) sidecar.
    struct Blob {
        bool present = false;
        std::size_t elementCount = 0;
        std::size_t components = 0;   // 3 (points/tris), 1 (faceId), 4 (tets)
        std::size_t elemSize = 0;     // 8 (F64), 4 (I32)
        MappedFile mapped;
        std::string expectedSha256;   // FullVerify anchor; empty = no comparison
        // SegmentedMerkle state
        MerkleSidecar sidecar;
        mutable std::vector<bool> segChecked;
        mutable bool fullVerified = false;

        std::size_t totalBytes() const { return elementCount * components * elemSize; }
    };

    // Map + size-check + (SegmentedMerkle) sidecar-check one blob. Returns false
    // (leaving present=false) if the role is absent; sets *ok=false on a present
    // role that fails to map/validate.
    bool initBlob(Blob* blob, const BlobInput& in, std::size_t components,
                  std::size_t elemSize, IntegrityMode mode, bool* ok);

    // Verify the whole of one blob lazily, memoized. Returns false on a failed
    // digest. Takes the internal mutex.
    bool verifyBlob(const Blob& blob) const;

    IntegrityMode mode_ = IntegrityMode::FullVerify;
    bool valid_ = false;
    bool hasSurface_ = false;
    bool hasTet_ = false;

    Blob points_;
    Blob tris_;
    Blob faceId_;
    Blob volPoints_;
    Blob tets_;

    mutable std::mutex mutex_;
    mutable IntegrityStats stats_;
};

} // namespace xq

#endif // XQ_IO_SOURCE_MAPPED_GEOMETRY_SOURCE_H
