#include "services/resource/GeometryResourceManager.h"

#include "core/XQMemoryImageBufferHandle.h" // scalarSize
#include "core/asset/AssetRecord.h"
#include "core/asset/AssetRegistry.h"
#include "core/asset/BufferRef.h"
#include "core/source/IGeometrySource.h"
#include "core/source/IVoxelSource.h"
#include "core/io/PathSafety.h"
#include "io/blob/BlobStore.h"
#include "io/source/MappedGeometrySource.h"
#include "io/source/MappedVoxelSource.h"

#include <filesystem>
#include <limits>
#include <utility>

namespace xq {

namespace {

constexpr int kResidentVoxelFull = 0;
constexpr int kResidentVoxelSegmented = 1;
constexpr int kResidentGeometryAllFull = 10;
constexpr int kResidentGeometryAllSegmented = 11;
constexpr int kResidentGeometrySurfaceFull = 20;
constexpr int kResidentGeometrySurfaceSegmented = 21;
constexpr int kResidentGeometryTetFull = 30;
constexpr int kResidentGeometryTetSegmented = 31;

std::size_t voxelBytes(const GeometryResourceManager::VoxelSourceSpec& spec)
{
    const std::size_t voxels = static_cast<std::size_t>(spec.dims[0])
        * static_cast<std::size_t>(spec.dims[1])
        * static_cast<std::size_t>(spec.dims[2]);
    const std::size_t stride = static_cast<std::size_t>(spec.components)
        * XQMemoryImageBufferHandle::scalarSize(spec.type);
    return voxels * stride;
}

// Look up a blob by role; returns nullptr if the asset or role is absent.
const BufferRef* findBlob(const AssetRecord* rec, const char* role)
{
    if (rec == nullptr) {
        return nullptr;
    }
    for (const auto& roleBlob : rec->blobs) {
        if (roleBlob.first == role) {
            return &roleBlob.second;
        }
    }
    return nullptr;
}

std::string joinPath(const std::string& root, const std::string& rel)
{
    return root.empty() ? rel : root + "/" + rel;
}

bool checkedMul(std::size_t a, std::size_t b, std::size_t* out)
{
    if (out == nullptr) {
        return false;
    }
    if (a != 0 && b > (std::numeric_limits<std::size_t>::max)() / a) {
        return false;
    }
    *out = a * b;
    return true;
}

bool checkedAdd(std::size_t a, std::size_t b, std::size_t* out)
{
    if (out == nullptr || b > (std::numeric_limits<std::size_t>::max)() - a) {
        return false;
    }
    *out = a + b;
    return true;
}

bool validateGeometryBlobRef(const BufferRef& ref,
                             BlobElementType expectedType,
                             std::uint16_t expectedComponents,
                             std::size_t expectedElementCount,
                             std::size_t* outBytes)
{
    if (outBytes == nullptr || ref.relPath.empty() || !isConfinedRelativePath(ref.relPath)
        || ref.formatVersion != 1 || ref.endianness != 0
        || ref.elementType != expectedType || ref.components != expectedComponents
        || ref.elementCount != static_cast<std::uint64_t>(expectedElementCount)
        || ref.byteCount > (std::numeric_limits<std::size_t>::max)()) {
        return false;
    }

    std::size_t scalars = 0;
    std::size_t bytes = 0;
    if (!checkedMul(expectedElementCount, expectedComponents, &scalars)
        || !checkedMul(scalars, BlobStore::byteWidth(expectedType), &bytes)
        || bytes != static_cast<std::size_t>(ref.byteCount)) {
        return false;
    }

    *outBytes = bytes;
    return true;
}

} // namespace

GeometryResourceManager::ResidentKey
GeometryResourceManager::voxelBlockKey(const AssetId& id,
                                       const VoxelSourceSpec& spec)
{
    return ResidentKey(id, spec.segmented ? kResidentVoxelSegmented
                                          : kResidentVoxelFull);
}

GeometryResourceManager::ResidentKey
GeometryResourceManager::geometryBlockKey(const AssetId& id,
                                          const GeometrySourceSpec& spec)
{
    int kind = spec.segmented ? kResidentGeometryAllSegmented
                              : kResidentGeometryAllFull;
    if (spec.hasSurface && !spec.hasTet) {
        kind = spec.segmented ? kResidentGeometrySurfaceSegmented
                              : kResidentGeometrySurfaceFull;
    } else if (spec.hasTet && !spec.hasSurface) {
        kind = spec.segmented ? kResidentGeometryTetSegmented
                              : kResidentGeometryTetFull;
    }
    return ResidentKey(id, kind);
}

GeometryResourceManager::GeometryResourceManager(const AssetRegistry* registry,
                                                 std::string assetRootDir)
    : registry_(registry)
    , assetRootDir_(std::move(assetRootDir))
    , budgetBytes_((std::numeric_limits<std::size_t>::max)())
{
}

GeometryResourceManager::~GeometryResourceManager() = default;

void GeometryResourceManager::setBudgetBytes(std::size_t budget)
{
    std::lock_guard<std::mutex> lock(mutex_);
    budgetBytes_ = budget;
    evictToBudgetLocked();
}

GeometryResourceManager::VoxelSourceHandle
GeometryResourceManager::acquireVoxelSource(const AssetId& id,
                                            const VoxelSourceSpec& spec)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Cache hit: bump LRU, hand back a fresh pin (no remap).
    const ResidentKey key = voxelBlockKey(id, spec);
    auto it = blocks_.find(key);
    if (it != blocks_.end()) {
        ++hitCount_;
        lru_.erase(it->second.lruIt);
        lru_.push_front(key);
        it->second.lruIt = lru_.begin();
        return VoxelSourceHandle(it->second.voxelSource, it->second.pin);
    }

    if (registry_ == nullptr) {
        return VoxelSourceHandle();
    }
    const AssetRecord* rec = registry_->find(id);
    const BufferRef* voxelsRef = findBlob(rec, "voxels");
    if (voxelsRef == nullptr || voxelsRef->relPath.empty()) {
        return VoxelSourceHandle();
    }
    const std::string relPath = voxelsRef->relPath;
    if (!isConfinedRelativePath(relPath)) {
        return VoxelSourceHandle();
    }

    const std::string blobPath = assetRootDir_.empty()
        ? relPath
        : assetRootDir_ + "/" + relPath;
    if (!isPathWithinRoot(assetRootDir_, blobPath)) {
        return VoxelSourceHandle();   // junction/symlink escapes the asset root
    }
    const std::string merklePath = blobPath + ".merkle";

    const MappedVoxelSource::IntegrityMode mode = spec.segmented
        ? MappedVoxelSource::IntegrityMode::SegmentedMerkle
        : MappedVoxelSource::IntegrityMode::FullVerify;

    // Pass the content-address anchor so FullVerify detects a corrupt/tampered
    // blob (BufferRef.sha256 is the whole-blob digest the writer recorded).
    auto src = std::make_shared<MappedVoxelSource>(
        blobPath, spec.dims, spec.type, spec.components, mode, merklePath,
        voxelsRef->sha256);
    if (!src->valid()) {
        return VoxelSourceHandle();
    }

    ResidentBlock block;
    block.voxelSource = std::move(src);
    block.pin = std::make_shared<int>(0); // baseline gating-pin owner
    block.bytes = voxelBytes(spec);

    lru_.push_front(key);
    auto inserted = blocks_.emplace(key, std::move(block));
    inserted.first->second.lruIt = lru_.begin();
    residentBytes_ += inserted.first->second.bytes;
    ++mapCount_;

    std::shared_ptr<const IVoxelSource> srcPtr = inserted.first->second.voxelSource;
    std::shared_ptr<void> pin = inserted.first->second.pin;

    // Evict other unpinned blocks if this push put us over budget. The just-
    // acquired block is pinned by `pin` (use_count == 2 here) so it survives.
    evictToBudgetLocked();

    return VoxelSourceHandle(srcPtr, std::move(pin));
}

GeometryResourceManager::GeometrySourceHandle
GeometryResourceManager::acquireGeometrySource(const AssetId& id,
                                               const GeometrySourceSpec& spec)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!spec.hasSurface && !spec.hasTet) {
        return GeometrySourceHandle();
    }

    // Cache hit: bump LRU, hand back a fresh pin (no remap).
    const ResidentKey key = geometryBlockKey(id, spec);
    auto it = blocks_.find(key);
    if (it != blocks_.end()) {
        ++hitCount_;
        lru_.erase(it->second.lruIt);
        lru_.push_front(key);
        it->second.lruIt = lru_.begin();
        return GeometrySourceHandle(it->second.geometrySource, it->second.pin);
    }

    if (registry_ == nullptr) {
        return GeometrySourceHandle();
    }
    const AssetRecord* rec = registry_->find(id);
    if (rec == nullptr) {
        return GeometrySourceHandle();
    }

    // Resolve geometry blobs by role. A surface model uses points/tris/faceId; a
    // mesh node uses surfPoints/surfTris/surfFaceId (+ volPoints/tets). Try both
    // surface namings; tets always volPoints/tets.
    const BufferRef* points = findBlob(rec, "points");
    const BufferRef* tris = findBlob(rec, "tris");
    const BufferRef* faceId = findBlob(rec, "faceId");
    const BufferRef* surfPoints = findBlob(rec, "surfPoints");
    const BufferRef* surfTris = findBlob(rec, "surfTris");
    const BufferRef* surfFaceId = findBlob(rec, "surfFaceId");
    if (!(points != nullptr && tris != nullptr && faceId != nullptr)
        && surfPoints != nullptr && surfTris != nullptr && surfFaceId != nullptr) {
        points = surfPoints;
        tris = surfTris;
        faceId = surfFaceId;
    }
    const BufferRef* volPoints = findBlob(rec, "volPoints");
    const BufferRef* tets = findBlob(rec, "tets");

    MappedGeometrySource::Inputs in;
    in.mode = spec.segmented ? MappedGeometrySource::IntegrityMode::SegmentedMerkle
                             : MappedGeometrySource::IntegrityMode::FullVerify;
    std::size_t totalBytes = 0;

    auto fill = [&](MappedGeometrySource::BlobInput* dst, const BufferRef* ref,
                    std::size_t elementCount, BlobElementType expectedType,
                    std::uint16_t expectedComponents) -> bool {
        if (ref == nullptr) {
            return false;
        }
        std::size_t bytes = 0;
        std::size_t nextTotal = 0;
        if (!validateGeometryBlobRef(*ref, expectedType, expectedComponents, elementCount, &bytes)
            || !checkedAdd(totalBytes, bytes, &nextTotal)) {
            return false;
        }
        dst->path = joinPath(assetRootDir_, ref->relPath);
        if (!isPathWithinRoot(assetRootDir_, dst->path)) {
            return false;   // junction/symlink escapes the asset root
        }
        dst->elementCount = elementCount;
        dst->expectedSha256 = ref->sha256; // FullVerify tamper-detection anchor
        if (spec.segmented) {
            dst->merklePath = dst->path + ".merkle";
        }
        totalBytes = nextTotal;
        return true;
    };

    if (spec.hasSurface) {
        if (points == nullptr || tris == nullptr || faceId == nullptr) {
            return GeometrySourceHandle();
        }
        if (!fill(&in.points, points, spec.pointCount, BlobElementType::F64, 3)
            || !fill(&in.tris, tris, spec.triCount, BlobElementType::I32, 3)
            || !fill(&in.faceId, faceId, spec.triCount, BlobElementType::I32, 1)) {
            return GeometrySourceHandle();
        }
    }
    if (spec.hasTet) {
        if (volPoints == nullptr || tets == nullptr) {
            return GeometrySourceHandle();
        }
        if (!fill(&in.volPoints, volPoints, spec.volPointCount, BlobElementType::F64, 3)
            || !fill(&in.tets, tets, spec.tetCount, BlobElementType::I32, 4)) {
            return GeometrySourceHandle();
        }
    }

    auto src = std::make_shared<MappedGeometrySource>(in);
    if (!src->valid()) {
        return GeometrySourceHandle();
    }

    ResidentBlock block;
    block.geometrySource = std::move(src);
    block.pin = std::make_shared<int>(0);
    block.bytes = totalBytes;

    lru_.push_front(key);
    auto inserted = blocks_.emplace(key, std::move(block));
    inserted.first->second.lruIt = lru_.begin();
    residentBytes_ += inserted.first->second.bytes;
    ++mapCount_;

    std::shared_ptr<const IGeometrySource> srcPtr = inserted.first->second.geometrySource;
    std::shared_ptr<void> pin = inserted.first->second.pin;

    evictToBudgetLocked();

    return GeometrySourceHandle(srcPtr, std::move(pin));
}

void GeometryResourceManager::evictToBudget()
{
    std::lock_guard<std::mutex> lock(mutex_);
    evictToBudgetLocked();
}

void GeometryResourceManager::evictToBudgetLocked()
{
    // LRU tail first; skip pinned blocks (a live handle holds a pin copy, so the
    // baseline pin's use_count() > 1). Acquire shares mutex_ with us, so no new
    // handle can be minted between the check and the erase (no TOCTOU dangling).
    auto it = lru_.end();
    while (residentBytes_ > budgetBytes_ && it != lru_.begin()) {
        --it;
        auto blockIt = blocks_.find(*it);
        if (blockIt == blocks_.end()) {
            it = lru_.erase(it);
            continue;
        }
        if (blockIt->second.pin.use_count() > 1) {
            continue; // pinned: a live handle still references this block
        }
        residentBytes_ -= blockIt->second.bytes;
        blocks_.erase(blockIt);
        it = lru_.erase(it);
        ++evictCount_;
    }
}

std::size_t GeometryResourceManager::residentBytes() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return residentBytes_;
}

std::size_t GeometryResourceManager::blockCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return blocks_.size();
}

std::uint64_t GeometryResourceManager::mapCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return mapCount_;
}

std::uint64_t GeometryResourceManager::hitCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return hitCount_;
}

std::uint64_t GeometryResourceManager::evictCount() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return evictCount_;
}

} // namespace xq
