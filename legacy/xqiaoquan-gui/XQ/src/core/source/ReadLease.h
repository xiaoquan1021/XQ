#ifndef XQ_CORE_SOURCE_READ_LEASE_H
#define XQ_CORE_SOURCE_READ_LEASE_H

#include "core/source/ReadSpan.h"
#include "core/source/SourceViews.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace xq {

// RAII read-only borrow handles. Move-only. While a lease is alive its view
// pointers stay valid and stable:
//   - borrow(): holds a shared_ptr keepalive so the underlying handle outlives
//     the lease even if the adapter is destroyed first (AC8); view points into
//     the handle's vector.
//   - own(): materializes a buffer the lease owns; view points into that buffer.
// A std::vector move preserves its data pointer, so the defaulted move keeps
// owned views valid after a move.
//
// keepalive is an EVICTION GATING PIN, not merely a lifetime extender. Any
// IVoxelSource/IGeometrySource implementer backed by evictable storage (mmap,
// budgeted/LRU eviction, etc.) MUST gate its evictor on the keepalive's
// use_count: the underlying storage may only be reclaimed when
// keepalive.use_count() == 1 (i.e. no live lease holds a copy). A live borrow()
// lease therefore pins its backing storage and forbids eviction for as long as
// it is alive; an own() lease carries its own buffer (keepalive is null) and is
// immune to eviction. Resident baseline sources never evict and are unaffected.
// Concurrency: each acquire_* returns an independent move-only lease that copies
// the keepalive (shared_ptr refcount is atomic), so multiple threads pin
// independently; the evictor's use_count==1 check and acquire must share the
// owner's lock to avoid a TOCTOU race.

class VoxelLease {
public:
    VoxelLease() = default;
    VoxelLease(VoxelLease&&) = default;
    VoxelLease& operator=(VoxelLease&&) = default;
    VoxelLease(const VoxelLease&) = delete;
    VoxelLease& operator=(const VoxelLease&) = delete;

    const VoxelView& view() const { return view_; }

    // Borrow: view spans bytes the keepalive keeps alive (zero copy).
    static VoxelLease borrow(std::shared_ptr<const void> keepalive, VoxelView view)
    {
        VoxelLease lease;
        lease.keepalive_ = std::move(keepalive);
        lease.view_ = view;
        return lease;
    }

    // Own: materialize a sub-block; view points into the owned buffer.
    static VoxelLease own(std::vector<std::uint8_t> bytes,
                          ScalarType type,
                          const int dims[3],
                          int components)
    {
        VoxelLease lease;
        lease.owned_ = std::move(bytes);
        lease.view_.valid = true;
        lease.view_.type = type;
        lease.view_.dims[0] = dims[0];
        lease.view_.dims[1] = dims[1];
        lease.view_.dims[2] = dims[2];
        lease.view_.components = components;
        lease.view_.bytes = ReadSpan<std::uint8_t>(lease.owned_.data(), lease.owned_.size());
        return lease;
    }

    // Invalid (empty) lease: out-of-range region/slab or missing data.
    static VoxelLease invalid()
    {
        return VoxelLease();
    }

private:
    std::shared_ptr<const void> keepalive_;
    std::vector<std::uint8_t> owned_;
    VoxelView view_;
};

template <class T>
class GeometryLease {
public:
    GeometryLease() = default;
    GeometryLease(GeometryLease&&) = default;
    GeometryLease& operator=(GeometryLease&&) = default;
    GeometryLease(const GeometryLease&) = delete;
    GeometryLease& operator=(const GeometryLease&) = delete;

    const ReadSpan<T>& span() const { return view_; }

    static GeometryLease borrow(std::shared_ptr<const void> keepalive, ReadSpan<T> view)
    {
        GeometryLease lease;
        lease.keepalive_ = std::move(keepalive);
        lease.view_ = view;
        return lease;
    }

    static GeometryLease own(std::vector<T> values)
    {
        GeometryLease lease;
        lease.owned_ = std::move(values);
        lease.view_ = ReadSpan<T>(lease.owned_.data(), lease.owned_.size());
        return lease;
    }

    static GeometryLease empty()
    {
        return GeometryLease();
    }

private:
    std::shared_ptr<const void> keepalive_;
    std::vector<T> owned_;
    ReadSpan<T> view_;
};

class TriangleLease {
public:
    TriangleLease() = default;
    TriangleLease(TriangleLease&&) = default;
    TriangleLease& operator=(TriangleLease&&) = default;
    TriangleLease(const TriangleLease&) = delete;
    TriangleLease& operator=(const TriangleLease&) = delete;

    const TriangleView& view() const { return view_; }

    // Triangles borrowed from the keepalive; faceIds materialized (the handle
    // exposes no faceId vector accessor, only triangleFaceId(i)).
    static TriangleLease borrow(std::shared_ptr<const void> keepalive,
                                ReadSpan<SourceTriangle> triangles,
                                std::vector<int> faceIds)
    {
        TriangleLease lease;
        lease.keepalive_ = std::move(keepalive);
        lease.ownedFaceIds_ = std::move(faceIds);
        lease.view_.triangles = triangles;
        lease.view_.faceIds =
            ReadSpan<int>(lease.ownedFaceIds_.data(), lease.ownedFaceIds_.size());
        return lease;
    }

    // M9b-A: both triangles AND faceIds borrowed zero-copy. triangles and
    // faceIds may live in different backing storage (e.g. two separate mmap'd
    // blobs of one geometry asset, or the same resident handle), so each carries
    // its own keepalive eviction-gating pin -- a live lease pins BOTH. ownedFaceIds_
    // stays empty (no O(N) materialization). The existing borrow() above is
    // unchanged; consumers read view().faceIds as a contiguous ReadSpan<int>
    // either way, so the borrow-vs-own distinction is transparent to them.
    static TriangleLease borrow_borrowed_faceids(std::shared_ptr<const void> triKeepalive,
                                                 ReadSpan<SourceTriangle> triangles,
                                                 std::shared_ptr<const void> faceIdKeepalive,
                                                 ReadSpan<int> faceIds)
    {
        TriangleLease lease;
        lease.keepalive_ = std::move(triKeepalive);
        lease.faceIdKeepalive_ = std::move(faceIdKeepalive);
        lease.view_.triangles = triangles;
        lease.view_.faceIds = faceIds;
        return lease;
    }

    static TriangleLease empty()
    {
        return TriangleLease();
    }

private:
    std::shared_ptr<const void> keepalive_;
    std::shared_ptr<const void> faceIdKeepalive_;
    std::vector<int> ownedFaceIds_;
    TriangleView view_;
};

} // namespace xq

#endif // XQ_CORE_SOURCE_READ_LEASE_H
