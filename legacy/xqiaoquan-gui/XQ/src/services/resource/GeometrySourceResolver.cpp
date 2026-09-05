#include "services/resource/GeometrySourceResolver.h"

#include "core/XQMeshPayload.h"
#include "core/XQPayload.h"
#include "core/XQSurfaceModelPayload.h"
#include "core/asset/AssetRecord.h"
#include "core/asset/AssetRegistry.h"

namespace xq {

namespace {

// Finds the BufferRef for a role in an asset record, or nullptr.
const BufferRef* findBlobRef(const AssetRecord& rec, const char* role)
{
    for (std::vector<std::pair<std::string, BufferRef>>::const_iterator it = rec.blobs.begin();
         it != rec.blobs.end(); ++it) {
        if (it->first == role) {
            return &it->second;
        }
    }
    return nullptr;
}

// Extracts a payload's lazy geometry binding. Returns false (hasId == false)
// for payloads that carry no geometryAssetId or are not a lazy-capable kind.
bool payloadGeometryAssetId(const XQPayload& payload, AssetId* out)
{
    if (const auto* surf = dynamic_cast<const XQSurfaceModelPayload*>(&payload)) {
        if (surf->hasGeometryAssetId()) {
            *out = surf->geometryAssetId();
            return true;
        }
        return false;
    }
    if (const auto* mesh = dynamic_cast<const XQMeshPayload*>(&payload)) {
        if (mesh->hasGeometryAssetId()) {
            *out = mesh->geometryAssetId();
            return true;
        }
        return false;
    }
    return false;
}

} // namespace

GeometryResourceManager::GeometrySourceHandle resolveLazyGeometrySource(
    const XQPayload& payload,
    GeometryResourceManager& manager,
    const AssetRegistry& registry)
{
    return resolveLazyGeometrySource(payload, manager, registry,
                                     LazyGeometrySourceMode::All);
}

GeometryResourceManager::GeometrySourceHandle resolveLazyGeometrySource(
    const XQPayload& payload,
    GeometryResourceManager& manager,
    const AssetRegistry& registry,
    LazyGeometrySourceMode mode)
{
    AssetId id;
    if (!payloadGeometryAssetId(payload, &id)) {
        return GeometryResourceManager::GeometrySourceHandle();
    }

    const AssetRecord* rec = registry.find(id);
    if (rec == nullptr) {
        return GeometryResourceManager::GeometrySourceHandle();
    }

    // Build the spec from the asset's BufferRefs. elementCount is the per-role
    // element count the writer recorded (points/surfPoints: pointCount;
    // tris/surfTris: triCount; volPoints: volPointCount; tets: tetCount), so no
    // byte arithmetic is needed. Surface uses points/tris/faceId, falling back to
    // surfPoints/surfTris/surfFaceId for a mesh node; tets use volPoints/tets.
    GeometryResourceManager::GeometrySourceSpec spec;

    const BufferRef* points = findBlobRef(*rec, "points");
    const BufferRef* tris = findBlobRef(*rec, "tris");
    const BufferRef* faceId = findBlobRef(*rec, "faceId");
    if (points == nullptr) {
        points = findBlobRef(*rec, "surfPoints");
        tris = findBlobRef(*rec, "surfTris");
        faceId = findBlobRef(*rec, "surfFaceId");
    }
    if (mode != LazyGeometrySourceMode::TetOnly
        && points != nullptr && tris != nullptr && faceId != nullptr) {
        spec.hasSurface = true;
        spec.pointCount = static_cast<std::size_t>(points->elementCount);
        spec.triCount = static_cast<std::size_t>(tris->elementCount);
    }

    const BufferRef* volPoints = findBlobRef(*rec, "volPoints");
    const BufferRef* tets = findBlobRef(*rec, "tets");
    if (mode != LazyGeometrySourceMode::SurfaceOnly
        && volPoints != nullptr && tets != nullptr) {
        spec.hasTet = true;
        spec.volPointCount = static_cast<std::size_t>(volPoints->elementCount);
        spec.tetCount = static_cast<std::size_t>(tets->elementCount);
    }

    if (!spec.hasSurface && !spec.hasTet) {
        return GeometryResourceManager::GeometrySourceHandle();
    }

    spec.segmented = true;
    GeometryResourceManager::GeometrySourceHandle segmented =
        manager.acquireGeometrySource(id, spec);
    if (segmented.valid()) {
        return segmented;
    }

    spec.segmented = false;
    return manager.acquireGeometrySource(id, spec);
}

} // namespace xq
