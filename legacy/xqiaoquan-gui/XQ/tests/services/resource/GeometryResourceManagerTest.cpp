#include "core/asset/AssetRecord.h"
#include "core/asset/AssetRegistry.h"
#include "core/XQMemoryImageBufferHandle.h"
#include "core/source/IGeometrySource.h"
#include "core/source/IVoxelSource.h"
#include "core/source/ResidentVoxelSource.h"
#include "io/blob/MerkleSidecar.h"
#include "services/resource/GeometryResourceManager.h"

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) {
        std::cout << "check failed: " << what << std::endl;
        ++g_failures;
    }
}

fs::path freshDir()
{
    const fs::path dir = fs::temp_directory_path() / "xq_geometry_resource_manager_test";
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    return dir;
}

// dims 16x16x16, UInt16, 1 component -> 8192 bytes.
constexpr int kDimX = 16;
constexpr int kDimY = 16;
constexpr int kDimZ = 16;
constexpr std::uint64_t kSegBytes = 1024;
constexpr std::size_t kBlobBytes =
    static_cast<std::size_t>(kDimX) * kDimY * kDimZ * 2;

// Deterministic per-asset content so a borrowed span can be verified.
std::vector<std::uint8_t> makeVoxelBlob(std::uint16_t salt)
{
    const std::size_t voxels = static_cast<std::size_t>(kDimX) * kDimY * kDimZ;
    std::vector<std::uint8_t> v(voxels * 2);
    for (std::size_t i = 0; i < voxels; ++i) {
        const std::uint16_t val = static_cast<std::uint16_t>(i * 7 + 3 + salt * 101);
        v[i * 2] = static_cast<std::uint8_t>(val & 0xFF);
        v[i * 2 + 1] = static_cast<std::uint8_t>((val >> 8) & 0xFF);
    }
    return v;
}

std::shared_ptr<const xq::IVoxelSource> makeResidentVoxelSource(
    std::uint16_t salt)
{
    const int dims[3] = {kDimX, kDimY, kDimZ};
    std::shared_ptr<xq::XQMemoryImageBufferHandle> buffer =
        std::make_shared<xq::XQMemoryImageBufferHandle>(
            xq::ScalarType::UInt16, dims, 1, makeVoxelBlob(salt));
    return std::make_shared<xq::ResidentVoxelSource>(std::move(buffer));
}

class FixedMetaVoxelSource final : public xq::IVoxelSource {
public:
    explicit FixedMetaVoxelSource(xq::VoxelMeta meta)
        : meta_(meta)
    {
    }

    xq::VoxelMeta meta() const override { return meta_; }
    xq::VoxelLease acquire_whole() const override { return xq::VoxelLease(); }
    xq::VoxelLease acquire_region(const int[6]) const override
    {
        return xq::VoxelLease();
    }
    xq::VoxelLease acquire_slab(int) const override { return xq::VoxelLease(); }

private:
    xq::VoxelMeta meta_;
};

std::shared_ptr<const xq::IVoxelSource> makeFixedMetaSource(
    const xq::VoxelMeta& meta)
{
    return std::make_shared<FixedMetaVoxelSource>(meta);
}

void writeFile(const fs::path& p, const std::vector<std::uint8_t>& bytes)
{
    std::ofstream f(p.string(), std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

// Create a voxel blob on disk under `dir`, register an asset with a "voxels"
// blob whose relPath is the file name, and (optionally) a .merkle sidecar.
xq::AssetId makeVoxelAsset(xq::AssetRegistry& reg, const fs::path& dir,
                           const std::string& fileName, std::uint16_t salt,
                           bool withSidecar)
{
    const std::vector<std::uint8_t> blob = makeVoxelBlob(salt);
    writeFile(dir / fileName, blob);
    if (withSidecar) {
        const std::string merklePath = (dir / (fileName + ".merkle")).string();
        xq::write_sidecar(blob, kSegBytes, merklePath);
    }

    const xq::AssetId id =
        reg.createAsset(xq::AssetCategory::ManagedCanonical, xq::AssetKind::Image);
    xq::AssetRecord* rec = reg.find(id);
    xq::BufferRef ref;
    ref.relPath = fileName;
    ref.byteCount = blob.size();
    ref.elementType = xq::BlobElementType::U8;
    ref.components = 1;
    ref.elementCount = blob.size();
    rec->blobs.emplace_back("voxels", ref);
    return id;
}

xq::GeometryResourceManager::VoxelSourceSpec voxelSpec(bool segmented)
{
    xq::GeometryResourceManager::VoxelSourceSpec spec;
    spec.dims[0] = kDimX;
    spec.dims[1] = kDimY;
    spec.dims[2] = kDimZ;
    spec.type = xq::ScalarType::UInt16;
    spec.components = 1;
    spec.segmented = segmented;
    return spec;
}

// --- geometry asset helpers (M9b-A) -----------------------------------------
// A small surface: kSurfPoints points, kSurfTris triangles. Content salted per
// asset so a borrowed span can be verified. F64/I32 little-endian (writer fmt).
constexpr std::size_t kSurfPoints = 32;
constexpr std::size_t kSurfTris = 40;
constexpr std::size_t kVolPoints = 12;
constexpr std::size_t kTets = 6;
// points 32*3*8=768, tris 40*3*4=480, faceId 40*1*4=160 -> 1408 bytes / surface.
constexpr std::size_t kSurfBytes = kSurfPoints * 3 * 8 + kSurfTris * 3 * 4 + kSurfTris * 4;

void putF64(std::vector<std::uint8_t>& out, double v)
{
    std::uint8_t buf[8];
    std::memcpy(buf, &v, 8);
    out.insert(out.end(), buf, buf + 8);
}
void putI32(std::vector<std::uint8_t>& out, int v)
{
    std::int32_t v32 = static_cast<std::int32_t>(v);
    std::uint8_t buf[4];
    std::memcpy(buf, &v32, 4);
    out.insert(out.end(), buf, buf + 4);
}

std::vector<std::uint8_t> surfPointsBlob(int salt)
{
    std::vector<std::uint8_t> b;
    for (std::size_t i = 0; i < kSurfPoints; ++i) {
        putF64(b, static_cast<double>(i + salt));
        putF64(b, static_cast<double>(i * 2 + salt));
        putF64(b, static_cast<double>(i * 3 + salt));
    }
    return b;
}
std::vector<std::uint8_t> surfTrisBlob()
{
    std::vector<std::uint8_t> b;
    for (std::size_t i = 0; i < kSurfTris; ++i) {
        putI32(b, static_cast<int>(i % kSurfPoints));
        putI32(b, static_cast<int>((i + 1) % kSurfPoints));
        putI32(b, static_cast<int>((i + 2) % kSurfPoints));
    }
    return b;
}
std::vector<std::uint8_t> surfFaceIdBlob(int salt)
{
    std::vector<std::uint8_t> b;
    for (std::size_t i = 0; i < kSurfTris; ++i) {
        putI32(b, static_cast<int>(i + salt));
    }
    return b;
}

std::vector<std::uint8_t> volPointsBlob(int salt)
{
    std::vector<std::uint8_t> b;
    for (std::size_t i = 0; i < kVolPoints; ++i) {
        putF64(b, static_cast<double>(i + salt));
        putF64(b, static_cast<double>(i + 10 + salt));
        putF64(b, static_cast<double>(i + 20 + salt));
    }
    return b;
}

std::vector<std::uint8_t> tetsBlob()
{
    std::vector<std::uint8_t> b;
    for (std::size_t i = 0; i < kTets; ++i) {
        putI32(b, static_cast<int>(i % kVolPoints));
        putI32(b, static_cast<int>((i + 1) % kVolPoints));
        putI32(b, static_cast<int>((i + 2) % kVolPoints));
        putI32(b, static_cast<int>((i + 3) % kVolPoints));
    }
    return b;
}

void addGeometryBlob(xq::AssetRecord* rec, const fs::path& dir, const char* role,
                     const std::string& fileName, const std::vector<std::uint8_t>& blob,
                     xq::BlobElementType et, std::uint16_t comps,
                     std::uint64_t elemCount)
{
    writeFile(dir / fileName, blob);
    xq::BufferRef ref;
    ref.relPath = fileName;
    ref.byteCount = blob.size();
    ref.elementType = et;
    ref.components = comps;
    ref.elementCount = elemCount;
    rec->blobs.emplace_back(role, ref);
}

// Register a surface asset with points/tris/faceId blobs on disk.
xq::AssetId makeSurfaceAsset(xq::AssetRegistry& reg, const fs::path& dir,
                             const std::string& prefix, int salt)
{
    const xq::AssetId id =
        reg.createAsset(xq::AssetCategory::Derived, xq::AssetKind::Surface);
    xq::AssetRecord* rec = reg.find(id);
    addGeometryBlob(rec, dir, "points", prefix + "_points.bin", surfPointsBlob(salt),
                    xq::BlobElementType::F64, 3, kSurfPoints);
    addGeometryBlob(rec, dir, "tris", prefix + "_tris.bin", surfTrisBlob(),
                    xq::BlobElementType::I32, 3, kSurfTris);
    addGeometryBlob(rec, dir, "faceId", prefix + "_faceId.bin", surfFaceIdBlob(salt),
                    xq::BlobElementType::I32, 1, kSurfTris);
    return id;
}

void addTetBlobs(xq::AssetRecord* rec, const fs::path& dir, const std::string& prefix, int salt);

// Register a mesh geometry asset with separate surface and tet point arrays.
// This mirrors project-writer mesh blobs: surfPoints/surfTris/surfFaceId plus
// volPoints/tets under one AssetId.
xq::AssetId makeMeshGeometryAsset(xq::AssetRegistry& reg, const fs::path& dir,
                                  const std::string& prefix, int salt)
{
    const xq::AssetId id =
        reg.createAsset(xq::AssetCategory::Derived, xq::AssetKind::Mesh);
    xq::AssetRecord* rec = reg.find(id);
    addGeometryBlob(rec, dir, "surfPoints", prefix + "_surfPoints.bin",
                    surfPointsBlob(salt), xq::BlobElementType::F64, 3, kSurfPoints);
    addGeometryBlob(rec, dir, "surfTris", prefix + "_surfTris.bin",
                    surfTrisBlob(), xq::BlobElementType::I32, 3, kSurfTris);
    addGeometryBlob(rec, dir, "surfFaceId", prefix + "_surfFaceId.bin",
                    surfFaceIdBlob(salt), xq::BlobElementType::I32, 1, kSurfTris);
    addTetBlobs(rec, dir, prefix, salt + 100);
    return id;
}

void addTetBlobs(xq::AssetRecord* rec, const fs::path& dir, const std::string& prefix, int salt)
{
    addGeometryBlob(rec, dir, "volPoints", prefix + "_volPoints.bin", volPointsBlob(salt),
                    xq::BlobElementType::F64, 3, kVolPoints);
    addGeometryBlob(rec, dir, "tets", prefix + "_tets.bin", tetsBlob(),
                    xq::BlobElementType::I32, 4, kTets);
}

xq::BufferRef* mutableBlobRef(xq::AssetRecord* rec, const char* role)
{
    if (rec == nullptr) {
        return nullptr;
    }
    for (std::vector<std::pair<std::string, xq::BufferRef>>::iterator it = rec->blobs.begin();
         it != rec->blobs.end();
         ++it) {
        if (it->first == role) {
            return &it->second;
        }
    }
    return nullptr;
}

bool eraseBlobRef(xq::AssetRecord* rec, const char* role)
{
    if (rec == nullptr) {
        return false;
    }
    for (std::vector<std::pair<std::string, xq::BufferRef>>::iterator it = rec->blobs.begin();
         it != rec->blobs.end();
         ++it) {
        if (it->first == role) {
            rec->blobs.erase(it);
            return true;
        }
    }
    return false;
}

xq::GeometryResourceManager::GeometrySourceSpec surfaceSpec()
{
    xq::GeometryResourceManager::GeometrySourceSpec spec;
    spec.pointCount = kSurfPoints;
    spec.triCount = kSurfTris;
    spec.hasSurface = true;
    spec.hasTet = false;
    spec.segmented = false;
    return spec;
}

xq::GeometryResourceManager::GeometrySourceSpec tetSpec()
{
    xq::GeometryResourceManager::GeometrySourceSpec spec;
    spec.volPointCount = kVolPoints;
    spec.tetCount = kTets;
    spec.hasSurface = false;
    spec.hasTet = true;
    spec.segmented = false;
    return spec;
}

// AC1: first acquire maps, second acquire of the same asset hits the cache.
void test_lazy_residency_and_cache_hit(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId a = makeVoxelAsset(reg, dir, "ac1_a.bin", 1, false);

    xq::GeometryResourceManager mgr(&reg, dir.string());
    check(mgr.mapCount() == 0 && mgr.blockCount() == 0,
          "AC1 nothing resident before first acquire");

    xq::GeometryResourceManager::VoxelSourceHandle h1 =
        mgr.acquireVoxelSource(a, voxelSpec(false));
    check(h1.valid(), "AC1 first acquire valid");
    check(mgr.mapCount() == 1, "AC1 first acquire maps once");
    check(mgr.hitCount() == 0, "AC1 first acquire is not a cache hit");
    check(mgr.blockCount() == 1, "AC1 one block resident");
    check(mgr.residentBytes() == kBlobBytes, "AC1 resident bytes accounted");

    xq::GeometryResourceManager::VoxelSourceHandle h2 =
        mgr.acquireVoxelSource(a, voxelSpec(false));
    check(h2.valid(), "AC1 second acquire valid");
    check(mgr.mapCount() == 1, "AC1 second acquire does NOT remap (cached)");
    check(mgr.hitCount() == 1, "AC1 second acquire is a cache hit");
    check(mgr.blockCount() == 1, "AC1 still one block");
}

// Unknown asset / missing voxels blob -> invalid handle.
void test_invalid_acquire(const fs::path& dir)
{
    xq::AssetRegistry reg;
    xq::GeometryResourceManager mgr(&reg, dir.string());

    xq::GeometryResourceManager::VoxelSourceHandle bad =
        mgr.acquireVoxelSource(xq::AssetId(99999), voxelSpec(false));
    check(!bad.valid(), "unknown asset -> invalid handle");
    check(mgr.blockCount() == 0, "no block created for unknown asset");

    // Asset with no "voxels" blob.
    const xq::AssetId noBlob =
        reg.createAsset(xq::AssetCategory::Derived, xq::AssetKind::Surface);
    (void)noBlob;
    xq::GeometryResourceManager::VoxelSourceHandle bad2 =
        mgr.acquireVoxelSource(noBlob, voxelSpec(false));
    check(!bad2.valid(), "asset without voxels blob -> invalid handle");
}

void test_installed_resident_lifecycle(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId id =
        reg.createAsset(xq::AssetCategory::ExternalSource, xq::AssetKind::Image);
    xq::GeometryResourceManager mgr(&reg, dir.string());
    const std::shared_ptr<const xq::IVoxelSource> source =
        makeResidentVoxelSource(40);

    check(!mgr.installResidentVoxelSource(xq::AssetId(99999), source).valid(),
          "resident install rejects unknown asset");
    check(!mgr.installResidentVoxelSource(id, nullptr).valid(),
          "resident install rejects null source");

    xq::GeometryResourceManager::VoxelSourceHandle installed =
        mgr.installResidentVoxelSource(id, source);
    check(installed.valid(), "resident install succeeds for registered asset");
    check(mgr.blockCount() == 1 && mgr.residentBytes() == kBlobBytes,
          "resident install accounts one in-memory block");
    check(mgr.mapCount() == 0, "resident install is not counted as a blob mapping");

    xq::GeometryResourceManager::VoxelSourceHandle acquired =
        mgr.acquireResidentVoxelSource(id);
    check(acquired.valid(), "resident lookup returns installed source");
    check(mgr.hitCount() == 1, "resident lookup counts as cache hit");
    xq::VoxelLease lease = acquired->acquire_whole();
    check(lease.view().valid
              && lease.view().scalarAt(0) == static_cast<double>(3 + 40 * 101),
          "resident lookup exposes expected voxels");

    xq::GeometryResourceManager::VoxelSourceHandle idempotent =
        mgr.installResidentVoxelSource(id, source);
    check(idempotent.valid() && mgr.blockCount() == 1 && mgr.hitCount() == 2,
          "reinstalling identical resident source is idempotent");
    check(!mgr.installResidentVoxelSource(id, makeResidentVoxelSource(41)).valid(),
          "different resident source requires explicit removal");

    check(mgr.removeResidentVoxelSource(id), "resident removal succeeds");
    check(!mgr.acquireResidentVoxelSource(id).valid(),
          "removed resident source is no longer discoverable");
    check(mgr.blockCount() == 0 && mgr.residentBytes() == 0,
          "resident removal updates cache accounting");
    xq::VoxelLease liveAfterRemoval = installed->acquire_whole();
    check(liveAfterRemoval.view().valid
              && liveAfterRemoval.view().scalarAt(0)
                  == static_cast<double>(3 + 40 * 101),
          "returned resident handle stays safe after explicit removal");
    check(!mgr.removeResidentVoxelSource(id),
          "removing an absent resident source reports false");

    xq::GeometryResourceManager::VoxelSourceHandle replacement =
        mgr.installResidentVoxelSource(id, source);
    check(replacement.valid(),
          "reinstall same source pointer after explicit removal");
    check(!mgr.removeResidentVoxelSourceIfMatches(id, installed),
          "stale generation cannot remove same-pointer reinstall");
    xq::GeometryResourceManager::VoxelSourceHandle current =
        mgr.acquireResidentVoxelSource(id);
    xq::VoxelLease currentLease = current.valid()
        ? current->acquire_whole()
        : xq::VoxelLease();
    check(currentLease.view().valid
              && currentLease.view().scalarAt(0)
                  == static_cast<double>(3 + 40 * 101),
          "same-pointer reinstall remains discoverable after stale cleanup");
    check(mgr.removeResidentVoxelSourceIfMatches(id, current)
              && mgr.blockCount() == 0,
          "compare-and-remove removes the expected resident source");
}

void test_installed_resident_rejects_invalid_meta(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId id =
        reg.createAsset(xq::AssetCategory::ExternalSource, xq::AssetKind::Image);
    xq::GeometryResourceManager mgr(&reg, dir.string());

    xq::VoxelMeta valid;
    valid.valid = true;
    valid.type = xq::ScalarType::UInt16;
    valid.dims[0] = kDimX;
    valid.dims[1] = kDimY;
    valid.dims[2] = kDimZ;
    valid.components = 1;
    valid.voxelCount = static_cast<std::size_t>(kDimX) * kDimY * kDimZ;

    xq::VoxelMeta invalid = valid;
    invalid.valid = false;
    check(!mgr.installResidentVoxelSource(id, makeFixedMetaSource(invalid)).valid(),
          "resident install rejects invalid meta flag");

    invalid = valid;
    invalid.type = xq::ScalarType::Unknown;
    check(!mgr.installResidentVoxelSource(id, makeFixedMetaSource(invalid)).valid(),
          "resident install rejects unknown scalar type");

    invalid = valid;
    invalid.components = 0;
    check(!mgr.installResidentVoxelSource(id, makeFixedMetaSource(invalid)).valid(),
          "resident install rejects zero components");

    invalid = valid;
    invalid.voxelCount -= 1;
    check(!mgr.installResidentVoxelSource(id, makeFixedMetaSource(invalid)).valid(),
          "resident install rejects mismatched voxel count");

    invalid = valid;
    invalid.dims[0] = (std::numeric_limits<int>::max)();
    invalid.dims[1] = (std::numeric_limits<int>::max)();
    invalid.dims[2] = (std::numeric_limits<int>::max)();
    invalid.voxelCount = (std::numeric_limits<std::size_t>::max)();
    check(!mgr.installResidentVoxelSource(id, makeFixedMetaSource(invalid)).valid(),
          "resident install rejects overflowing dimensions");

    check(mgr.blockCount() == 0 && mgr.residentBytes() == 0,
          "invalid resident metadata leaves cache unchanged");
}

void test_installed_resident_tracks_asset_association(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId id =
        reg.createAsset(xq::AssetCategory::ExternalSource, xq::AssetKind::Image);
    xq::AssetRecord* original = reg.find(id);
    original->sourceAbsPath = "C:/first-source";
    original->contentFingerprint = "first-fingerprint";

    xq::GeometryResourceManager mgr(&reg, dir.string());
    xq::GeometryResourceManager::VoxelSourceHandle originalHandle =
        mgr.installResidentVoxelSource(id, makeResidentVoxelSource(43));
    check(originalHandle.valid() && mgr.blockCount() == 1,
          "resident install snapshots its registered asset association");

    check(reg.unregisterAsset(id), "unregister resident asset for id reuse");
    xq::AssetRecord replacement;
    replacement.id = id;
    replacement.category = xq::AssetCategory::ExternalSource;
    replacement.kind = xq::AssetKind::Image;
    replacement.sourceAbsPath = "C:/second-source";
    replacement.contentFingerprint = "second-fingerprint";
    check(reg.registerAsset(replacement), "re-register colliding asset id");

    check(!mgr.acquireResidentVoxelSource(id).valid()
              && mgr.blockCount() == 0 && mgr.residentBytes() == 0,
          "asset id reuse invalidates stale resident association");
    xq::VoxelLease originalLease = originalHandle->acquire_whole();
    check(originalLease.view().valid
              && originalLease.view().scalarAt(0)
                  == static_cast<double>(3 + 43 * 101),
          "invalidated association does not break an existing handle");
}

void test_resident_and_mapped_voxel_flavors_are_distinct(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId id = makeVoxelAsset(reg, dir, "resident_vs_mapped.bin", 50, false);
    xq::GeometryResourceManager mgr(&reg, dir.string());

    xq::GeometryResourceManager::VoxelSourceHandle resident =
        mgr.installResidentVoxelSource(id, makeResidentVoxelSource(51));
    xq::GeometryResourceManager::VoxelSourceHandle mapped =
        mgr.acquireVoxelSource(id, voxelSpec(false));
    check(resident.valid() && mapped.valid(),
          "same asset can own distinct resident and mapped voxel flavors");
    check(mgr.blockCount() == 2 && mgr.mapCount() == 1
              && mgr.residentBytes() == kBlobBytes * 2,
          "resident flavor does not masquerade as managed voxel blob");

    xq::VoxelLease residentLease = resident->acquire_whole();
    xq::VoxelLease mappedLease = mapped->acquire_whole();
    check(residentLease.view().valid && mappedLease.view().valid
              && residentLease.view().scalarAt(0)
                  == static_cast<double>(3 + 51 * 101)
              && mappedLease.view().scalarAt(0)
                  == static_cast<double>(3 + 50 * 101),
          "resident and mapped flavors preserve independent content");

    check(mgr.removeResidentVoxelSource(id),
          "resident flavor can be removed independently");
    check(mgr.blockCount() == 1 && mgr.mapCount() == 1
              && !mgr.acquireResidentVoxelSource(id).valid(),
          "removing resident flavor preserves mapped block");
    xq::GeometryResourceManager::VoxelSourceHandle mappedAgain =
        mgr.acquireVoxelSource(id, voxelSpec(false));
    check(mappedAgain.valid() && mgr.mapCount() == 1,
          "mapped flavor remains cached after resident removal");
}

void test_installed_resident_budget_and_registry_cleanup(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId first =
        reg.createAsset(xq::AssetCategory::ExternalSource, xq::AssetKind::Image);
    const xq::AssetId second =
        reg.createAsset(xq::AssetCategory::ExternalSource, xq::AssetKind::Image);
    xq::GeometryResourceManager mgr(&reg, dir.string());
    mgr.setBudgetBytes(kBlobBytes);

    xq::GeometryResourceManager::VoxelSourceHandle firstHandle =
        mgr.installResidentVoxelSource(first, makeResidentVoxelSource(60));
    xq::GeometryResourceManager::VoxelSourceHandle secondHandle =
        mgr.installResidentVoxelSource(second, makeResidentVoxelSource(61));
    check(firstHandle.valid() && secondHandle.valid() && mgr.blockCount() == 2,
          "live handles pin installed resident blocks over budget");
    firstHandle = xq::GeometryResourceManager::VoxelSourceHandle();
    secondHandle = xq::GeometryResourceManager::VoxelSourceHandle();
    mgr.evictToBudget();
    check(mgr.blockCount() == 1 && mgr.residentBytes() <= kBlobBytes,
          "released installed resident blocks obey the shared budget");
    check(!mgr.acquireResidentVoxelSource(first).valid()
              && mgr.acquireResidentVoxelSource(second).valid(),
          "installed resident eviction follows LRU order");

    check(reg.unregisterAsset(second), "remove resident asset from registry");
    check(!mgr.acquireResidentVoxelSource(second).valid() && mgr.blockCount() == 0,
          "resident lookup drops orphaned cache entry");
}

// AC2 (budget + LRU): over budget evicts the least-recently-used block.
void test_budget_lru_eviction(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId a = makeVoxelAsset(reg, dir, "ac2_a.bin", 2, false);
    const xq::AssetId b = makeVoxelAsset(reg, dir, "ac2_b.bin", 3, false);
    const xq::AssetId c = makeVoxelAsset(reg, dir, "ac2_c.bin", 4, false);

    xq::GeometryResourceManager mgr(&reg, dir.string());
    // Budget for two blocks only.
    mgr.setBudgetBytes(kBlobBytes * 2);

    // Acquire and release each (handles go out of scope) so nothing stays pinned.
    { mgr.acquireVoxelSource(a, voxelSpec(false)); } // a resident, LRU
    { mgr.acquireVoxelSource(b, voxelSpec(false)); } // a,b resident
    { mgr.acquireVoxelSource(c, voxelSpec(false)); } // over budget -> evict a

    check(mgr.blockCount() == 2, "AC2 budget holds two blocks");
    check(mgr.evictCount() == 1, "AC2 exactly one eviction");

    // a was the LRU -> re-acquiring it remaps (mapCount increments again).
    const std::uint64_t mapsBefore = mgr.mapCount();
    { mgr.acquireVoxelSource(a, voxelSpec(false)); }
    check(mgr.mapCount() == mapsBefore + 1, "AC2 evicted LRU 'a' was remapped");
}

// AC2 (refcount gating): a block with a live handle is never evicted, even under
// a tiny budget; once released it becomes evictable.
void test_pinned_block_not_evicted(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId a = makeVoxelAsset(reg, dir, "pin_a.bin", 5, false);
    const xq::AssetId b = makeVoxelAsset(reg, dir, "pin_b.bin", 6, false);
    const xq::AssetId c = makeVoxelAsset(reg, dir, "pin_c.bin", 7, false);

    xq::GeometryResourceManager mgr(&reg, dir.string());
    mgr.setBudgetBytes(kBlobBytes); // one block fits

    // Pin 'a' by keeping its handle alive.
    xq::GeometryResourceManager::VoxelSourceHandle pinned =
        mgr.acquireVoxelSource(a, voxelSpec(false));
    check(pinned.valid(), "pinned acquire valid");

    // Churn other assets; budget wants to drop to one block but 'a' is pinned.
    { mgr.acquireVoxelSource(b, voxelSpec(false)); }
    { mgr.acquireVoxelSource(c, voxelSpec(false)); }

    // 'a' must still be resident: re-acquiring it is a cache hit, not a remap.
    const std::uint64_t mapsBefore = mgr.mapCount();
    xq::GeometryResourceManager::VoxelSourceHandle again =
        mgr.acquireVoxelSource(a, voxelSpec(false));
    check(again.valid(), "pinned 'a' re-acquire valid");
    check(mgr.mapCount() == mapsBefore, "pinned 'a' never evicted (no remap)");

    // The pinned source still yields correct data.
    xq::VoxelLease lease = pinned.source().acquire_whole();
    const bool spanValid = lease.view().valid;
    check(spanValid, "pinned source acquire_whole valid");
    const std::vector<std::uint8_t> expected = makeVoxelBlob(5);
    bool same = spanValid && lease.view().bytes.size() == expected.size();
    for (std::size_t i = 0; same && i < expected.size(); ++i) {
        same = lease.view().bytes[i] == expected[i];
    }
    check(same, "pinned source data matches origin");

    // Release both handles; now 'a' is unpinned and can be evicted under budget.
    pinned = xq::GeometryResourceManager::VoxelSourceHandle();
    again = xq::GeometryResourceManager::VoxelSourceHandle();
    mgr.evictToBudget();
    check(mgr.residentBytes() <= kBlobBytes, "after release, budget enforced");
}

// AC4 path through the manager: SegmentedMerkle source acquired via the manager.
void test_segmented_merkle_through_manager(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId a = makeVoxelAsset(reg, dir, "seg_a.bin", 8, true);

    xq::GeometryResourceManager mgr(&reg, dir.string());
    xq::GeometryResourceManager::VoxelSourceHandle h =
        mgr.acquireVoxelSource(a, voxelSpec(true));
    check(h.valid(), "segmented-merkle acquire valid");

    xq::VoxelLease lease = h.source().acquire_whole();
    const bool spanValid = lease.view().valid;
    check(spanValid, "segmented-merkle acquire_whole valid");
    const std::vector<std::uint8_t> expected = makeVoxelBlob(8);
    bool same = spanValid && lease.view().bytes.size() == expected.size();
    for (std::size_t i = 0; same && i < expected.size(); ++i) {
        same = lease.view().bytes[i] == expected[i];
    }
    check(same, "segmented-merkle data matches origin");
}

// AC5 concurrency: a background evictor churns under a tiny budget while the
// foreground holds a handle + a live lease into the pinned block; the borrowed
// bytes must stay valid for the whole run (pin-before-evict: the handle's pin is
// established before the evictor starts).
void test_concurrent_evict_vs_held_lease(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId x = makeVoxelAsset(reg, dir, "cc_x.bin", 9, false);
    std::vector<xq::AssetId> churn;
    for (int i = 0; i < 4; ++i) {
        churn.push_back(makeVoxelAsset(
            reg, dir, "cc_churn_" + std::to_string(i) + ".bin",
            static_cast<std::uint16_t>(20 + i), false));
    }

    xq::GeometryResourceManager mgr(&reg, dir.string());
    mgr.setBudgetBytes(kBlobBytes); // one block fits -> heavy eviction pressure

    // Pin X and take a live lease BEFORE the evictor starts (pin-before-evict).
    xq::GeometryResourceManager::VoxelSourceHandle hx =
        mgr.acquireVoxelSource(x, voxelSpec(false));
    check(hx.valid(), "concurrency: X acquired");
    xq::VoxelLease lease = hx.source().acquire_whole();
    check(lease.view().valid, "concurrency: X lease valid");

    const std::vector<std::uint8_t> expected = makeVoxelBlob(9);

    std::atomic<bool> stop{false};
    std::atomic<bool> evictorRan{false};
    std::thread evictor([&]() {
        for (int iter = 0; iter < 2000 && !stop.load(); ++iter) {
            const xq::AssetId& victim = churn[iter % churn.size()];
            mgr.acquireVoxelSource(victim, voxelSpec(false)); // released immediately
            mgr.evictToBudget();
            evictorRan.store(true);
        }
    });

    // Foreground: hammer the held lease's bytes; they must never change/dangle.
    // Keep going until the evictor has churned at least once (and a floor of
    // rounds), so the test actually exercises concurrent eviction pressure.
    bool foregroundOk = true;
    for (int iter = 0; iter < 4000 || !evictorRan.load(); ++iter) {
        const xq::VoxelView& view = lease.view();
        if (!view.valid || view.bytes.size() != expected.size()) {
            foregroundOk = false;
            break;
        }
        // Spot-check a few bytes across the span each round.
        const std::size_t probe[4] = {0, expected.size() / 3,
                                      expected.size() / 2, expected.size() - 1};
        for (std::size_t p : probe) {
            if (view.bytes[p] != expected[p]) {
                foregroundOk = false;
                break;
            }
        }
        if (!foregroundOk) {
            break;
        }
    }

    stop.store(true);
    evictor.join();

    check(evictorRan.load(), "concurrency: evictor actually ran");
    check(foregroundOk, "concurrency: held lease bytes stayed valid (AC5)");

    // X stayed resident the whole time (pinned): re-acquire is a cache hit.
    const std::uint64_t mapsBefore = mgr.mapCount();
    xq::GeometryResourceManager::VoxelSourceHandle again =
        mgr.acquireVoxelSource(x, voxelSpec(false));
    check(again.valid() && mgr.mapCount() == mapsBefore,
          "concurrency: pinned X never evicted during the run");
}

void test_voxel_handle_outlives_manager(const fs::path& dir)
{
    xq::GeometryResourceManager::VoxelSourceHandle handle;
    {
        xq::AssetRegistry reg;
        const xq::AssetId a = makeVoxelAsset(reg, dir, "life_voxel.bin", 30, false);
        xq::GeometryResourceManager mgr(&reg, dir.string());
        handle = mgr.acquireVoxelSource(a, voxelSpec(false));
        check(handle.valid(), "lifetime voxel handle acquired before manager destruction");
    }

    check(handle.valid(), "lifetime voxel handle remains valid after manager destruction");
    xq::VoxelLease lease = handle.source().acquire_whole();
    const std::vector<std::uint8_t> expected = makeVoxelBlob(30);
    bool same = lease.view().valid && lease.view().bytes.size() == expected.size();
    for (std::size_t i = 0; same && i < expected.size(); ++i) {
        same = lease.view().bytes[i] == expected[i];
    }
    check(same, "lifetime voxel handle reads correct data after manager destruction");
}

void test_voxel_lease_outlives_manager(const fs::path& dir)
{
    xq::VoxelLease lease;
    {
        xq::AssetRegistry reg;
        const xq::AssetId a = makeVoxelAsset(reg, dir, "life_voxel_lease.bin", 31, false);
        xq::GeometryResourceManager mgr(&reg, dir.string());
        xq::GeometryResourceManager::VoxelSourceHandle handle =
            mgr.acquireVoxelSource(a, voxelSpec(false));
        check(handle.valid(), "lifetime voxel lease source acquired");
        lease = handle.source().acquire_whole();
        check(lease.view().valid, "lifetime voxel lease valid before manager destruction");
    }

    const std::vector<std::uint8_t> expected = makeVoxelBlob(31);
    bool same = lease.view().valid && lease.view().bytes.size() == expected.size();
    for (std::size_t i = 0; same && i < expected.size(); ++i) {
        same = lease.view().bytes[i] == expected[i];
    }
    check(same, "lifetime voxel lease reads correct data after manager destruction");
}

// AC3 (geometry): first acquireGeometrySource maps, second hits the cache and
// the borrowed spans match the source.
void test_geometry_cache_hit(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId a = makeSurfaceAsset(reg, dir, "g_ac3", 5);

    xq::GeometryResourceManager mgr(&reg, dir.string());
    xq::GeometryResourceManager::GeometrySourceHandle h1 =
        mgr.acquireGeometrySource(a, surfaceSpec());
    check(h1.valid(), "geometry AC3 first acquire valid");
    check(mgr.mapCount() == 1, "geometry AC3 first acquire maps once");
    check(mgr.residentBytes() == kSurfBytes, "geometry AC3 resident bytes = sum of blobs");

    // Borrowed spans match source content.
    auto pl = h1.source().acquire_points();
    bool psame = pl.span().size() == kSurfPoints;
    for (std::size_t i = 0; psame && i < kSurfPoints; ++i) {
        psame = pl.span()[i].x == static_cast<double>(i + 5);
    }
    check(psame, "geometry AC3 points span matches source");
    auto tl = h1.source().acquire_triangles();
    check(tl.view().triangles.size() == kSurfTris
              && tl.view().faceIds.size() == kSurfTris
              && tl.view().faceIds[3] == static_cast<int>(3 + 5),
          "geometry AC3 tris+faceId span matches source");

    xq::GeometryResourceManager::GeometrySourceHandle h2 =
        mgr.acquireGeometrySource(a, surfaceSpec());
    check(h2.valid() && mgr.mapCount() == 1 && mgr.hitCount() == 1,
          "geometry AC3 second acquire is a cache hit (no remap)");
}

void test_geometry_cache_distinguishes_surface_and_tet_aspects(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId a = makeMeshGeometryAsset(reg, dir, "g_aspect", 21);

    xq::GeometryResourceManager mgr(&reg, dir.string());

    xq::GeometryResourceManager::GeometrySourceHandle surface =
        mgr.acquireGeometrySource(a, surfaceSpec());
    check(surface.valid(), "geometry aspect surface acquire valid");
    if (surface.valid()) {
        const xq::GeometryMeta meta = surface.source().meta();
        check(meta.valid && meta.pointCount == kSurfPoints
                  && meta.triangleCount == kSurfTris
                  && meta.tetCount == 0,
              "geometry aspect surface-only meta");
    }

    xq::GeometryResourceManager::GeometrySourceHandle tet =
        mgr.acquireGeometrySource(a, tetSpec());
    check(tet.valid(), "geometry aspect tet acquire valid after surface");
    if (tet.valid()) {
        const xq::GeometryMeta meta = tet.source().meta();
        check(meta.valid && meta.pointCount == kVolPoints
                  && meta.triangleCount == 0
                  && meta.tetCount == kTets,
              "geometry aspect tet-only meta after surface cache");
    }

    check(mgr.mapCount() == 2, "geometry aspect surface and tet use separate cache entries");

    xq::GeometryResourceManager::GeometrySourceHandle surfaceAgain =
        mgr.acquireGeometrySource(a, surfaceSpec());
    check(surfaceAgain.valid() && mgr.mapCount() == 2 && mgr.hitCount() == 1,
          "geometry aspect repeated surface acquire hits matching cache entry");
}

// Unknown asset / no geometry blobs -> invalid handle.
void test_geometry_invalid(const fs::path& dir)
{
    xq::AssetRegistry reg;
    xq::GeometryResourceManager mgr(&reg, dir.string());

    xq::GeometryResourceManager::GeometrySourceHandle bad =
        mgr.acquireGeometrySource(xq::AssetId(88888), surfaceSpec());
    check(!bad.valid(), "geometry unknown asset -> invalid handle");

    const xq::AssetId noBlob =
        reg.createAsset(xq::AssetCategory::Derived, xq::AssetKind::Surface);
    xq::GeometryResourceManager::GeometrySourceHandle bad2 =
        mgr.acquireGeometrySource(noBlob, surfaceSpec());
    check(!bad2.valid(), "geometry asset without geometry blobs -> invalid handle");
}

// Bad registry metadata must be rejected before mapping, even if the blob files
// exist. This covers callers that bypass XQProjectReader.
void test_geometry_rejects_bad_registry_metadata(const fs::path& dir)
{
    {
        xq::AssetRegistry reg;
        const xq::AssetId a = makeSurfaceAsset(reg, dir, "g_bad_comp", 10);
        xq::AssetRecord* rec = reg.find(a);
        xq::BufferRef* points = mutableBlobRef(rec, "points");
        check(points != nullptr, "bad metadata fixture has points");
        if (points != nullptr) {
            points->components = 2; // points must be F64x3
        }
        xq::GeometryResourceManager mgr(&reg, dir.string());
        xq::GeometryResourceManager::GeometrySourceHandle h =
            mgr.acquireGeometrySource(a, surfaceSpec());
        check(!h.valid() && mgr.blockCount() == 0,
              "geometry rejects wrong role components before map");
    }

    {
        xq::AssetRegistry reg;
        const xq::AssetId a = makeSurfaceAsset(reg, dir, "g_bad_count", 11);
        xq::AssetRecord* rec = reg.find(a);
        xq::BufferRef* faceId = mutableBlobRef(rec, "faceId");
        check(faceId != nullptr, "bad metadata fixture has faceId");
        if (faceId != nullptr) {
            faceId->elementCount = kSurfTris - 1;
            faceId->byteCount = (kSurfTris - 1) * 4;
        }
        xq::GeometryResourceManager mgr(&reg, dir.string());
        xq::GeometryResourceManager::GeometrySourceHandle h =
            mgr.acquireGeometrySource(a, surfaceSpec());
        check(!h.valid() && mgr.blockCount() == 0,
              "geometry rejects faceId count mismatch before map");
    }

    {
        xq::AssetRegistry reg;
        const xq::AssetId a = makeSurfaceAsset(reg, dir, "g_bad_path", 12);
        xq::AssetRecord* rec = reg.find(a);
        xq::BufferRef* tris = mutableBlobRef(rec, "tris");
        check(tris != nullptr, "bad metadata fixture has tris");
        if (tris != nullptr) {
            tris->relPath = "../outside.bin";
        }
        xq::GeometryResourceManager mgr(&reg, dir.string());
        xq::GeometryResourceManager::GeometrySourceHandle h =
            mgr.acquireGeometrySource(a, surfaceSpec());
        check(!h.valid() && mgr.blockCount() == 0,
              "geometry rejects traversal relPath before map");
    }

    {
        xq::AssetRegistry reg;
        const xq::AssetId a = makeSurfaceAsset(reg, dir, "g_missing_tet", 13);
        xq::GeometryResourceManager::GeometrySourceSpec spec = surfaceSpec();
        spec.hasTet = true;
        spec.volPointCount = kVolPoints;
        spec.tetCount = kTets;
        xq::GeometryResourceManager mgr(&reg, dir.string());
        xq::GeometryResourceManager::GeometrySourceHandle h =
            mgr.acquireGeometrySource(a, spec);
        check(!h.valid() && mgr.blockCount() == 0,
              "geometry rejects missing requested tet group");
    }

    {
        xq::AssetRegistry reg;
        const xq::AssetId a = makeSurfaceAsset(reg, dir, "g_missing_surface", 14);
        xq::AssetRecord* rec = reg.find(a);
        addTetBlobs(rec, dir, "g_missing_surface", 14);
        check(eraseBlobRef(rec, "faceId"), "remove faceId from mixed geometry fixture");
        xq::GeometryResourceManager::GeometrySourceSpec spec = surfaceSpec();
        spec.hasTet = true;
        spec.volPointCount = kVolPoints;
        spec.tetCount = kTets;
        xq::GeometryResourceManager mgr(&reg, dir.string());
        xq::GeometryResourceManager::GeometrySourceHandle h =
            mgr.acquireGeometrySource(a, spec);
        check(!h.valid() && mgr.blockCount() == 0,
              "geometry rejects missing requested surface group");
    }
}

// AC4 (geometry): budget LRU + pinned-not-evicted on geometry blocks.
void test_geometry_budget_and_pin(const fs::path& dir)
{
    xq::AssetRegistry reg;
    const xq::AssetId a = makeSurfaceAsset(reg, dir, "g_b_a", 1);
    const xq::AssetId b = makeSurfaceAsset(reg, dir, "g_b_b", 2);

    xq::GeometryResourceManager mgr(&reg, dir.string());
    mgr.setBudgetBytes(kSurfBytes); // one surface fits

    // Pin 'a' by holding its handle; acquiring 'b' must NOT evict pinned 'a'.
    xq::GeometryResourceManager::GeometrySourceHandle pinned =
        mgr.acquireGeometrySource(a, surfaceSpec());
    check(pinned.valid(), "geometry AC4 pinned acquire valid");
    { mgr.acquireGeometrySource(b, surfaceSpec()); } // released immediately

    const std::uint64_t mapsBefore = mgr.mapCount();
    xq::GeometryResourceManager::GeometrySourceHandle again =
        mgr.acquireGeometrySource(a, surfaceSpec());
    check(again.valid() && mgr.mapCount() == mapsBefore,
          "geometry AC4 pinned 'a' never evicted (use_count gating)");

    // Release and enforce budget -> resident drops to fit.
    pinned = xq::GeometryResourceManager::GeometrySourceHandle();
    again = xq::GeometryResourceManager::GeometrySourceHandle();
    mgr.evictToBudget();
    check(mgr.residentBytes() <= kSurfBytes, "geometry AC4 budget enforced after release");
}

void test_geometry_handle_outlives_manager(const fs::path& dir)
{
    xq::GeometryResourceManager::GeometrySourceHandle handle;
    {
        xq::AssetRegistry reg;
        const xq::AssetId a = makeSurfaceAsset(reg, dir, "life_geometry", 32);
        xq::GeometryResourceManager mgr(&reg, dir.string());
        handle = mgr.acquireGeometrySource(a, surfaceSpec());
        check(handle.valid(), "lifetime geometry handle acquired before manager destruction");
    }

    check(handle.valid(), "lifetime geometry handle remains valid after manager destruction");
    xq::GeometryLease<xq::Point3> points = handle.source().acquire_points();
    bool pointsOk = points.span().size() == kSurfPoints;
    for (std::size_t i = 0; pointsOk && i < kSurfPoints; ++i) {
        pointsOk = points.span()[i].x == static_cast<double>(i + 32);
    }
    check(pointsOk, "lifetime geometry points read after manager destruction");

    xq::TriangleLease tris = handle.source().acquire_triangles();
    check(tris.view().triangles.size() == kSurfTris
              && tris.view().faceIds.size() == kSurfTris
              && tris.view().faceIds[3] == static_cast<int>(3 + 32),
          "lifetime geometry tris+faceId read after manager destruction");
}

// --- voxel path confinement (P0-2) ------------------------------------------
// The voxel link has TWO gates: a lexical one (isConfinedRelativePath rejects
// "../") and a physical one (isPathWithinRoot rejects a junction whose target
// is outside the asset root). Each escape target is a REAL, correctly-sized
// voxel blob so that, before the patch, the map would succeed and hand back a
// valid handle (true red); the check must be what turns it invalid.

// Lexical escape: relPath = "../outside.bin", with a real voxel blob planted at
// <assetRoot>/../outside.bin. Pre-patch: file exists -> valid handle (red).
// Post-patch: isConfinedRelativePath rejects "../" -> invalid handle (green).
void test_voxel_lexical_escape_rejected()
{
    const fs::path base = fs::temp_directory_path() / "xq_grm_voxel_lexical";
    std::error_code ec;
    fs::remove_all(base, ec);
    const fs::path assetRoot = base / "assets";
    fs::create_directories(assetRoot, ec);

    // Plant a valid-sized voxel blob one level ABOVE the asset root.
    const std::vector<std::uint8_t> blob = makeVoxelBlob(77);
    writeFile(base / "outside.bin", blob);

    xq::AssetRegistry reg;
    const xq::AssetId id =
        reg.createAsset(xq::AssetCategory::ManagedCanonical, xq::AssetKind::Image);
    xq::AssetRecord* rec = reg.find(id);
    xq::BufferRef ref;
    ref.relPath = "../outside.bin";
    ref.byteCount = blob.size();
    ref.elementType = xq::BlobElementType::U8;
    ref.components = 1;
    ref.elementCount = blob.size();
    rec->blobs.emplace_back("voxels", ref);

    xq::GeometryResourceManager mgr(&reg, assetRoot.string());
    xq::GeometryResourceManager::VoxelSourceHandle h =
        mgr.acquireVoxelSource(id, voxelSpec(false));
    check(!h.valid(), "voxel lexical '../' escape -> invalid handle");
    check(mgr.blockCount() == 0, "voxel lexical escape maps nothing");

    fs::remove_all(base, ec);
}

// Physical junction escape: relPath is lexically clean ("pre/x.bin"), but
// <assetRoot>/pre is a junction to an outside directory holding a real blob.
// Pre-patch: mapped through junction -> valid handle (red). Post-patch:
// isPathWithinRoot canonical prefix compare rejects it -> invalid handle.
void test_voxel_junction_escape_rejected()
{
    const fs::path base = fs::temp_directory_path() / "xq_grm_voxel_junction";
    std::error_code ec;
    fs::remove_all(base, ec);
    const fs::path assetRoot = base / "assets";
    const fs::path outside = base / "outside";
    fs::create_directories(assetRoot, ec);
    fs::create_directories(outside, ec);

    // Real, correctly-sized voxel blob in the outside directory.
    const std::vector<std::uint8_t> blob = makeVoxelBlob(88);
    writeFile(outside / "x.bin", blob);

    // <assetRoot>/pre -> outside (junction).
    const fs::path junction = assetRoot / "pre";
    const std::string cmd = "cmd /c mklink /J \"" + junction.string() + "\" \"" +
                            outside.string() + "\" >nul 2>&1";
    const int rc = std::system(cmd.c_str());
    if (rc != 0 || !fs::exists(junction)) {
        check(false, "voxel junction: cannot create junction (environment unsupported)");
        fs::remove_all(base, ec);
        return;
    }

    xq::AssetRegistry reg;
    const xq::AssetId id =
        reg.createAsset(xq::AssetCategory::ManagedCanonical, xq::AssetKind::Image);
    xq::AssetRecord* rec = reg.find(id);
    xq::BufferRef ref;
    ref.relPath = "pre/x.bin";
    ref.byteCount = blob.size();
    ref.elementType = xq::BlobElementType::U8;
    ref.components = 1;
    ref.elementCount = blob.size();
    rec->blobs.emplace_back("voxels", ref);

    xq::GeometryResourceManager mgr(&reg, assetRoot.string());
    xq::GeometryResourceManager::VoxelSourceHandle h =
        mgr.acquireVoxelSource(id, voxelSpec(false));
    check(!h.valid(), "voxel junction escape -> invalid handle");
    check(mgr.blockCount() == 0, "voxel junction escape maps nothing");

    const std::string rm = "cmd /c rmdir \"" + junction.string() + "\" >nul 2>&1";
    std::system(rm.c_str());
    fs::remove_all(base, ec);
}

} // namespace

int main()
{
    const fs::path dir = freshDir();

    test_lazy_residency_and_cache_hit(dir);
    test_invalid_acquire(dir);
    test_installed_resident_lifecycle(dir);
    test_installed_resident_rejects_invalid_meta(dir);
    test_installed_resident_tracks_asset_association(dir);
    test_resident_and_mapped_voxel_flavors_are_distinct(dir);
    test_installed_resident_budget_and_registry_cleanup(dir);
    test_budget_lru_eviction(dir);
    test_pinned_block_not_evicted(dir);
    test_segmented_merkle_through_manager(dir);
    test_concurrent_evict_vs_held_lease(dir);
    test_voxel_handle_outlives_manager(dir);
    test_voxel_lease_outlives_manager(dir);
    test_geometry_cache_hit(dir);
    test_geometry_cache_distinguishes_surface_and_tet_aspects(dir);
    test_geometry_invalid(dir);
    test_geometry_rejects_bad_registry_metadata(dir);
    test_geometry_budget_and_pin(dir);
    test_geometry_handle_outlives_manager(dir);

    std::error_code ec;
    fs::remove_all(dir, ec);

    // Confinement negative tests build their own throwaway roots.
    test_voxel_lexical_escape_rejected();
    test_voxel_junction_escape_rejected();

    if (g_failures != 0) {
        std::cout << g_failures << " check(s) failed" << std::endl;
        return 1;
    }
    std::cout << "all geometry resource manager checks passed" << std::endl;
    return 0;
}
