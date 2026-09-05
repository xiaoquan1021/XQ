#ifndef XQ_SERVICES_RESOURCE_GEOMETRY_SOURCE_RESOLVER_H
#define XQ_SERVICES_RESOURCE_GEOMETRY_SOURCE_RESOLVER_H

#include "services/resource/GeometryResourceManager.h"

namespace xq {

class XQPayload;
class AssetRegistry;

enum class LazyGeometrySourceMode {
    All,
    SurfaceOnly,
    TetOnly
};

// Resolves a payload's lazy geometry binding (M9b-E) into a pinned, memory-
// mapped IGeometrySource. This is the services-layer half of the lazy path: the
// io reader stamps a surface/mesh payload with a geometryAssetId instead of
// materializing a resident handle (XQProjectReadOptions::lazyGeometry); this
// function turns that assetId back into geometry on demand.
//
// It builds the GeometrySourceSpec from the asset's BufferRefs in the registry
// (element counts + which surface/tet blobs exist) and calls
// manager.acquireGeometrySource. The resolver prefers SegmentedMerkle because
// the native writer emits geometry .merkle sidecars, then falls back to
// FullVerify for older/partial asset roots. Both paths reinterpret the blobs as
// zero-copy spans, identical element-for-element to what the eager whole-blob
// load would have built into a resident handle.
//
// Returns an invalid handle (valid() == false) when the payload carries no
// geometryAssetId, the asset is unknown, or no geometry blobs are present -- in
// those cases the caller should fall back to the payload's resident handle
// (the coexisting M9a path). Layering: services may see core payloads + the
// manager + the registry; this introduces no new dependency edge.
GeometryResourceManager::GeometrySourceHandle resolveLazyGeometrySource(
    const XQPayload& payload,
    GeometryResourceManager& manager,
    const AssetRegistry& registry);

GeometryResourceManager::GeometrySourceHandle resolveLazyGeometrySource(
    const XQPayload& payload,
    GeometryResourceManager& manager,
    const AssetRegistry& registry,
    LazyGeometrySourceMode mode);

} // namespace xq

#endif // XQ_SERVICES_RESOURCE_GEOMETRY_SOURCE_RESOLVER_H
