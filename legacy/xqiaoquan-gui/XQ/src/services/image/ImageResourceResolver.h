#ifndef XQ_SERVICES_IMAGE_IMAGE_RESOURCE_RESOLVER_H
#define XQ_SERVICES_IMAGE_IMAGE_RESOURCE_RESOLVER_H

#include "core/XQImageVolume.h"
#include "core/asset/AssetId.h"
#include "core/image/IDicomSeriesReader.h"
#include "services/resource/GeometryResourceManager.h"

#include <string>
#include <vector>

namespace xq {

class AssetRegistry;

struct ImageResourceResolveResult {
    DicomSeriesStatus status = DicomSeriesStatus::InvalidArgument;
    GeometryResourceManager::VoxelSourceHandle source;
    std::vector<Diagnostic> diagnostics;

    bool ok() const
    {
        return status == DicomSeriesStatus::Ok && source.valid();
    }
};

// Resolves a persisted ExternalSource image AssetId to a project-scoped
// IVoxelSource. Caller-installed residency is checked first. On a miss, the
// persisted relative locator is resolved against the .xqproj parent directory,
// with the absolute locator used only when the relative source is unavailable.
// The persisted SeriesInstanceUID is always passed to the reader, and the read
// result must match the persisted identity, geometry, scalar metadata and
// content fingerprint before residency is installed.
// manager must be bound to registry. Project mutation is serialized by the
// caller while acquire snapshots and revalidates the asset around reader I/O.
class ImageResourceResolver {
public:
    static ImageResourceResolveResult acquire(
        const AssetId& assetId,
        const XQImageVolume& persistedImage,
        const std::string& projectFilePath,
        GeometryResourceManager& manager,
        const AssetRegistry& registry,
        IDicomSeriesReader& reader);
};

} // namespace xq

#endif // XQ_SERVICES_IMAGE_IMAGE_RESOURCE_RESOLVER_H
