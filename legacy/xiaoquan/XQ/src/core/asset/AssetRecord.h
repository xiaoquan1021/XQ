#ifndef XQ_CORE_ASSET_RECORD_H
#define XQ_CORE_ASSET_RECORD_H

#include "core/XQImageVolume.h" // DicomSeriesIdentity, ImageGeometry
#include "core/asset/AssetId.h"
#include "core/asset/BufferRef.h"

#include <string>
#include <utility>
#include <vector>

namespace xq {

// Storage class of an asset. "Is it first-class data" is NOT "does it go into
// the .xqproj" -- see prd §2.
enum class AssetCategory {
    ExternalSource,   // original imaging, referenced not copied
    ManagedOriginal,  // user imported original source (format reserved)
    ManagedCanonical, // user imported canonical volume (format reserved)
    Derived           // XQ-produced derived data: mask / surface / mesh / ...
};

// Kind of data the asset carries.
enum class AssetKind {
    Image,
    SegmentationMask,
    Surface,
    Mesh,
    SimulationCase,
    FlowResult,
    AiAnalysis,
    Path,
    Contour
};

// Identity, type, storage description, external locator, sidecar references,
// metadata for one asset. No load/cache/memory semantics live here (that is a
// later milestone); AssetRegistry guards that boundary.
struct AssetRecord {
    AssetId id;
    AssetCategory category = AssetCategory::Derived;
    AssetKind kind = AssetKind::Surface;

    // External locator (valid when category == ExternalSource).
    std::string sourceAbsPath;       // absolute path (may be empty)
    std::string sourceRelPath;       // relative to the .xqproj (may be empty)
    DicomSeriesIdentity dicom;       // study / series / frameOfReference UID
    bool hasDicom = false;
    std::string contentFingerprint;  // content fingerprint (e.g. source SHA-256), registration only

    // External imaging registers the necessary geometry metadata.
    bool hasGeometry = false;
    ImageGeometry geometry{};        // dims / spacing / origin / direction / coordSys

    // Derived/Managed entity references: 0..N blobs (one asset may split into
    // several arrays, e.g. a surface's points + tris + faceId). role examples:
    // "points" / "tris" / "faceId" / "voxels" / "volPoints" / "tets" / ...
    std::vector<std::pair<std::string /*role*/, BufferRef>> blobs;

    std::string displayName;         // may be empty
};

} // namespace xq

#endif // XQ_CORE_ASSET_RECORD_H
