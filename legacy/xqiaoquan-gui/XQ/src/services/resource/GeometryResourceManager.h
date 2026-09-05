#ifndef XQ_SERVICES_RESOURCE_GEOMETRY_RESOURCE_MANAGER_H
#define XQ_SERVICES_RESOURCE_GEOMETRY_RESOURCE_MANAGER_H

#include "core/XQImageVolume.h" // ScalarType
#include "core/asset/AssetId.h"

#include <cstddef>
#include <cstdint>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace xq {

class IVoxelSource;
class IGeometrySource;
class AssetRegistry;
struct AssetRecord;

// Data layer for "who holds the data, when it loads / frees" (M8b-2 §3.1). The
// AssetRegistry (core) owns asset identity + storage description; this manager
// (services) lazily residents the actual bytes per assetId behind the core
// IVoxelSource abstraction, caps total resident bytes with a budget, and evicts
// least-recently-used blocks under a cooperative use_count gate.
//
// Layering: the header speaks ONLY core abstractions (IVoxelSource / AssetId /
// ScalarType); the concrete production source (io's MappedVoxelSource) is
// constructed inside the .cpp, so xq_services links xq_io PRIVATE (the new edge
// services -> io is acyclic; io never depends on services).
//
    // Cooperative use_count gating (decision B): the manager keeps, per resident
    // block, its own std::shared_ptr<void> "pin" as the sole baseline owner. Every
    // acquire returns a handle that copies that pin (use_count++); the evictor
    // reclaims a block only when its pin's use_count() == 1 (no live handle). A
    // handle also owns the source via shared_ptr, so it remains safe even if the
    // manager is destroyed. Acquire and evict share one mutex so a block can never
    // be reclaimed between the use_count==1 check and a racing acquire (TOCTOU).
    // This per-block pin is layered above the source/lease keepalive; the pin owns
    // cache residency policy, while shared_ptr owns object lifetime.
class GeometryResourceManager {
public:
    // How to construct a voxel source for an asset. The bytes come from the
    // asset's "voxels" blob (resolved via the registry); the caller supplies the
    // voxel meta (dims/type/components) and the integrity mode. SegmentedMerkle
    // expects a "<blob>.merkle" sidecar next to the blob; FullVerify ignores it.
    struct VoxelSourceSpec {
        int dims[3] = {0, 0, 0};
        ScalarType type = ScalarType::Unknown;
        int components = 1;
        bool segmented = false; // true -> SegmentedMerkle, false -> FullVerify
    };

    // A pinned reference to a resident voxel source. While any handle to a block
    // is alive the block is pinned and the evictor must skip it. Copyable: each
    // copy adds a pin (so handing a handle around keeps the block resident);
    // dropping the last copy makes the block evictable again.
    class VoxelSourceHandle {
    public:
        VoxelSourceHandle() = default;

        bool valid() const { return source_ != nullptr; }
        const IVoxelSource& source() const { return *source_; }
        const IVoxelSource* operator->() const { return source_.get(); }

    private:
        friend class GeometryResourceManager;
        VoxelSourceHandle(std::shared_ptr<const IVoxelSource> source,
                          std::shared_ptr<void> pin)
            : source_(std::move(source))
            , pin_(std::move(pin))
        {
        }

        std::shared_ptr<const IVoxelSource> source_;
        std::shared_ptr<void> pin_; // copy of the block's gating pin
    };

    // How to construct a geometry source for an asset (M9b-A). The bytes come
    // from the asset's geometry blobs (resolved via the registry by role):
    // surface = points/tris/faceId (or surfPoints/surfTris/surfFaceId for a mesh
    // node), tet = volPoints/tets. The caller supplies the element counts (from
    // the BufferRefs) and the integrity mode. SegmentedMerkle expects a
    // "<blob>.merkle" sidecar next to each blob; FullVerify ignores sidecars
    // and remains the compatibility fallback for older/partial asset roots.
    struct GeometrySourceSpec {
        std::size_t pointCount = 0;    // surface points (role points/surfPoints)
        std::size_t triCount = 0;      // triangles (role tris/surfTris); faceId parallel
        std::size_t volPointCount = 0; // tet-mesh points (role volPoints)
        std::size_t tetCount = 0;      // tets (role tets)
        bool hasSurface = false;
        bool hasTet = false;
        bool segmented = false; // true -> SegmentedMerkle, false -> FullVerify
    };

    // A pinned reference to a resident geometry source. Same gating semantics as
    // VoxelSourceHandle: each copy adds a pin; the evictor skips a block while any
    // handle is alive.
    class GeometrySourceHandle {
    public:
        GeometrySourceHandle() = default;

        bool valid() const { return source_ != nullptr; }
        const IGeometrySource& source() const { return *source_; }
        const IGeometrySource* operator->() const { return source_.get(); }

    private:
        friend class GeometryResourceManager;
        GeometrySourceHandle(std::shared_ptr<const IGeometrySource> source,
                             std::shared_ptr<void> pin)
            : source_(std::move(source))
            , pin_(std::move(pin))
        {
        }

        std::shared_ptr<const IGeometrySource> source_;
        std::shared_ptr<void> pin_;
    };

    // The registry resolves assetId -> "voxels" BufferRef; assetRootDir is the
    // directory the BufferRef.relPath is relative to (the .xqproj asset root).
    GeometryResourceManager(const AssetRegistry* registry, std::string assetRootDir);
    ~GeometryResourceManager();

    GeometryResourceManager(const GeometryResourceManager&) = delete;
    GeometryResourceManager& operator=(const GeometryResourceManager&) = delete;

    // Resident-bytes ceiling. Exceeding it triggers LRU eviction of unpinned
    // blocks on the next acquire / evictToBudget(). Default: unbounded.
    void setBudgetBytes(std::size_t budget);

    // Current resident-bytes ceiling (read-only; for assertions / status UI).
    std::size_t budgetBytes() const;

    // Confirms that a resolver-supplied registry is the same project registry
    // this manager was constructed with. The binding is immutable.
    bool usesAssetRegistry(const AssetRegistry* registry) const;

    // Installs a caller-owned in-memory voxel source for an already registered
    // project asset. This uses a dedicated resident cache flavor, distinct from
    // both FullVerify and SegmentedMerkle mapped-blob entries. Reinstalling the
    // same source is idempotent; a different source for the same AssetId is
    // rejected until the existing resident entry is explicitly removed.
    VoxelSourceHandle installResidentVoxelSource(
        const AssetId& id,
        std::shared_ptr<const IVoxelSource> source);

    // Looks up only the caller-installed resident flavor. It never falls
    // through to a managed "voxels" blob mapping.
    VoxelSourceHandle acquireResidentVoxelSource(const AssetId& id);

    // Removes only the caller-installed resident flavor. Already returned
    // handles and leases remain valid through their shared ownership.
    bool removeResidentVoxelSource(const AssetId& id);

    // Compare-and-remove variant for resolver cleanup. It removes the current
    // association only when it still matches the source and install generation
    // held by expected, preventing an ABA race from deleting a newer install.
    bool removeResidentVoxelSourceIfMatches(
        const AssetId& id,
        const VoxelSourceHandle& expected);

    // First touch: resolve the "voxels" blob, construct a MappedVoxelSource,
    // resident it (+LRU +budget) and return a pinned handle. Subsequent voxel
    // touches of the same assetId return the cached source (LRU bumped, no
    // remap). An unknown asset, a missing "voxels" blob, or an invalid mapping
    // yields an invalid handle (valid() == false).
    VoxelSourceHandle acquireVoxelSource(const AssetId& id, const VoxelSourceSpec& spec);

    // First touch: resolve the geometry blobs (by role), construct a
    // MappedGeometrySource, resident it (+LRU, +budget = sum of all geometry blob
    // bytes for the requested aspect) and return a pinned handle. Subsequent
    // touches of the same assetId AND geometry aspect return the cached source
    // (LRU bumped, no remap). Surface-only, tet-only, and combined geometry
    // sources are distinct cache entries because a mesh asset can carry separate
    // surface and volume point arrays.
    GeometrySourceHandle acquireGeometrySource(const AssetId& id,
                                               const GeometrySourceSpec& spec);

    // Run an eviction pass against the current budget (used by a background
    // evictor; acquire runs the same pass internally).
    void evictToBudget();

    std::size_t residentBytes() const;
    std::size_t blockCount() const;
    std::uint64_t mapCount() const;  // distinct first-touch mappings performed
    std::uint64_t hitCount() const;  // cache hits (re-acquire of a resident block)
    std::uint64_t evictCount() const;

private:
    // map/list key: AssetId + resident-source kind/aspect.
    typedef std::pair<AssetId, int> ResidentKey;

    struct ResidentBlock {
        // Exactly one of these is set per block (a block is a voxel OR a geometry
        // source). pin/bytes/lruIt/eviction are source-type-agnostic.
        std::shared_ptr<const IVoxelSource> voxelSource;
        std::shared_ptr<IGeometrySource> geometrySource;
        std::shared_ptr<const AssetRecord> assetSnapshot;
        std::shared_ptr<void> pin; // baseline owner of the gating pin
        std::size_t bytes = 0;
        std::list<ResidentKey>::iterator lruIt;
    };

    static ResidentKey voxelBlockKey(const AssetId& id,
                                     const VoxelSourceSpec& spec);
    static ResidentKey residentVoxelBlockKey(const AssetId& id);
    static ResidentKey geometryBlockKey(const AssetId& id,
                                        const GeometrySourceSpec& spec);

    // Evict LRU-tail-first while over budget, skipping any block whose pin still
    // has a live handle (use_count > 1). Caller must hold mutex_.
    void evictToBudgetLocked();

    const AssetRegistry* registry_;
    std::string assetRootDir_;
    std::size_t budgetBytes_;
    std::size_t residentBytes_ = 0;

    std::map<ResidentKey, ResidentBlock> blocks_;
    std::list<ResidentKey> lru_; // front = most recently used, back = eviction end

    mutable std::mutex mutex_;
    std::uint64_t mapCount_ = 0;
    std::uint64_t hitCount_ = 0;
    std::uint64_t evictCount_ = 0;
};

} // namespace xq

#endif // XQ_SERVICES_RESOURCE_GEOMETRY_RESOURCE_MANAGER_H
