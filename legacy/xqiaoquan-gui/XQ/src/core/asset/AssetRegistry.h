#ifndef XQ_CORE_ASSET_REGISTRY_H
#define XQ_CORE_ASSET_REGISTRY_H

#include "core/asset/AssetId.h"
#include "core/asset/AssetRecord.h"
#include "core/asset/AssetRelation.h"

#include <functional>
#include <map>
#include <vector>

namespace xq {

// Owns asset identity, type, storage description, external locators, sidecar
// references, metadata, and asset-level derivation lineage.
//
// Hard boundary (AC1 audit point): this class has NO load / acquire / cache /
// evict / budget / mmap method. "Who holds the data, when it loads/frees" is a
// later milestone; here we only model identity + descriptions + lineage.
class AssetRegistry {
public:
    AssetRegistry();

    // Returns the id that createAsset() would mint next without reserving or
    // registering it. Returns invalid when the monotonic id space is exhausted.
    // Prepared atomic commands use this to choose output ids, then recheck that
    // they are still unoccupied immediately before commit.
    AssetId nextAvailableAssetId() const;

    // Allocate a fresh, monotonically increasing stable id and create its
    // record. The id never collides with any previously seen id.
    AssetId createAsset(AssetCategory category, AssetKind kind);

    // Register a record under a caller-supplied id (used when rebuilding from an
    // archive). Rejects an invalid id or one that already exists, returning
    // false and leaving the registry unchanged. On success the internal id
    // counter advances past the supplied id so later createAsset() never
    // collides with it.
    bool registerAsset(const AssetId& id, AssetCategory category, AssetKind kind);
    bool registerAsset(const AssetRecord& record);

    // Reversible mutation support for project-level commands. Removing a
    // record also removes its incident lineage but never rewinds next_id_.
    bool unregisterAsset(const AssetId& id);

    AssetRecord* find(const AssetId& id);
    const AssetRecord* find(const AssetId& id) const;

    void clear();
    std::size_t assetCount() const;
    void visit_assets(const std::function<void(const AssetRecord&)>& visitor) const;

    // Record that 'derived' was computed from 'source'.
    bool addRelation(const AssetId& source, const AssetId& derived);
    bool hasRelation(const AssetId& source, const AssetId& derived) const;
    bool removeRelation(const AssetId& source, const AssetId& derived);
    std::size_t relationCount() const;
    void visit_relations(
        const std::function<void(const AssetId& source, const AssetId& derived)>& visitor) const;

private:
    std::map<AssetId, AssetRecord> records_;
    std::vector<AssetRelation> relations_;
    AssetId::ValueType next_id_;
};

} // namespace xq

#endif // XQ_CORE_ASSET_REGISTRY_H
