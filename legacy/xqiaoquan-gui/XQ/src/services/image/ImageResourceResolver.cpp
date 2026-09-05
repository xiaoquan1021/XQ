#include "services/image/ImageResourceResolver.h"

#include "core/asset/AssetRecord.h"
#include "core/asset/AssetRegistry.h"
#include "core/source/ResidentVoxelSource.h"
#include "services/image/DicomServiceSupport.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <utility>

namespace xq {
namespace {

void addError(ImageResourceResolveResult* result,
              DicomSeriesStatus status,
              const std::string& message)
{
    result->status = status;
    result->diagnostics.emplace_back(
        DiagnosticSeverity::Error,
        std::string("dicom.resource.") + dicomSeriesStatusToken(status),
        message);
}

bool closeEnough(double left, double right)
{
    const double scale = (std::max)(
        1.0, (std::max)(std::abs(left), std::abs(right)));
    return std::isfinite(left) && std::isfinite(right)
        && std::abs(left - right) <= 1e-9 * scale;
}

bool checkedMultiply(std::size_t left,
                     std::size_t right,
                     std::size_t* product)
{
    if (product == nullptr
        || (right != 0
            && left > (std::numeric_limits<std::size_t>::max)() / right)) {
        return false;
    }
    *product = left * right;
    return true;
}

bool validPersistedMetadata(const AssetRecord& asset,
                            const XQImageVolume& image)
{
    return asset.category == AssetCategory::ExternalSource
        && asset.kind == AssetKind::Image
        && asset.hasDicom && asset.hasGeometry
        && isValidDicomSeriesFingerprint(asset.contentFingerprint)
        && !asset.dicom.studyInstanceUid.empty()
        && !asset.dicom.seriesInstanceUid.empty()
        && !asset.dicom.frameOfReferenceUid.empty()
        && isValidCanonicalDicomImageMetadata(image)
        && isValidImageGeometry(asset.geometry)
        && sameDicomSeriesIdentity(asset.dicom, image.dicomIdentity())
        && sameImageGeometry(asset.geometry, image.geometry());
}

bool samePersistedAssetVersion(const AssetRecord& snapshot,
                               const AssetRecord& current,
                               const XQImageVolume& image)
{
    return validPersistedMetadata(current, image)
        && snapshot.sourceRelPath == current.sourceRelPath
        && snapshot.sourceAbsPath == current.sourceAbsPath
        && snapshot.contentFingerprint == current.contentFingerprint
        && sameDicomSeriesIdentity(snapshot.dicom, current.dicom)
        && sameImageGeometry(snapshot.geometry, current.geometry);
}

bool sourceMetaMatches(const VoxelMeta& meta,
                       const XQImageVolume& image)
{
    if (!meta.valid || !image.hasGeometry()
        || meta.type != image.scalarType()
        || meta.components != image.componentCount()) {
        return false;
    }
    std::size_t expectedVoxels = 1;
    for (int axis = 0; axis < 3; ++axis) {
        if (meta.dims[axis] <= 0
            || meta.dims[axis] != image.geometry().dimensions[axis]
            || !checkedMultiply(
                expectedVoxels,
                static_cast<std::size_t>(meta.dims[axis]),
                &expectedVoxels)) {
            return false;
        }
    }
    return meta.voxelCount == expectedVoxels;
}

bool readMatchesPersistedMetadata(const DicomSeriesReadResult& read,
                                  const AssetRecord& asset,
                                  const XQImageVolume& image)
{
    if (!read.ok()
        || !isValidCanonicalDicomImageMetadata(read.volume)
        || !isValidVoxelBufferForImage(read.volume, *read.buffer)
        || !isValidDicomSeriesFingerprint(read.descriptor.contentFingerprint)
        || read.descriptor.contentFingerprint != asset.contentFingerprint
        || !sameDicomSeriesIdentity(read.descriptor.identity, asset.dicom)
        || !sameDicomSeriesIdentity(read.volume.dicomIdentity(), asset.dicom)
        || read.descriptor.modality != image.modality()
        || read.descriptor.dimensions[0] != image.geometry().dimensions[0]
        || read.descriptor.dimensions[1] != image.geometry().dimensions[1]
        || read.descriptor.dimensions[2] != image.geometry().dimensions[2]
        || read.descriptor.sliceCount
            != static_cast<std::size_t>(image.geometry().dimensions[2])
        || !sameImageGeometry(read.volume.geometry(), asset.geometry)
        || !sameImageGeometry(read.volume.geometry(), image.geometry())
        || read.volume.scalarType() != image.scalarType()
        || read.volume.componentCount() != image.componentCount()
        || read.volume.modality() != image.modality()
        || !closeEnough(read.volume.intensityRange().minimum,
                        image.intensityRange().minimum)
        || !closeEnough(read.volume.intensityRange().maximum,
                        image.intensityRange().maximum)
        || !closeEnough(read.volume.windowCenter(), image.windowCenter())
        || !closeEnough(read.volume.windowWidth(), image.windowWidth())
        || !closeEnough(read.volume.rescaleSlope(), image.rescaleSlope())
        || !closeEnough(read.volume.rescaleIntercept(), image.rescaleIntercept())) {
        return false;
    }
    return sourceMetaMatches(
        ResidentVoxelSource(read.buffer).meta(), image);
}

DicomSeriesStatus inspectDirectory(const std::filesystem::path& path)
{
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        return DicomSeriesStatus::SourceUnreadable;
    }
    if (!exists) {
        return DicomSeriesStatus::SourceNotFound;
    }
    const bool isDirectory = std::filesystem::is_directory(path, error);
    if (error || !isDirectory) {
        return DicomSeriesStatus::SourceUnreadable;
    }
    return DicomSeriesStatus::Ok;
}

DicomSeriesStatus resolveSourceDirectory(
    const AssetRecord& asset,
    const std::string& projectFilePath,
    std::string* resolved)
{
    if (resolved == nullptr) {
        return DicomSeriesStatus::InvalidArgument;
    }
    resolved->clear();

    if (!asset.sourceRelPath.empty()) {
        const std::filesystem::path relative(asset.sourceRelPath);
        if (relative.is_absolute() || relative.has_root_name()
            || relative.has_root_directory()) {
            return DicomSeriesStatus::InvalidArgument;
        }
        if (!projectFilePath.empty()) {
            std::error_code error;
            const std::filesystem::path absoluteProject =
                std::filesystem::absolute(projectFilePath, error);
            if (error) {
                return DicomSeriesStatus::SourceUnreadable;
            }
            const std::filesystem::path candidate =
                (absoluteProject.parent_path() / relative).lexically_normal();
            const DicomSeriesStatus relativeStatus = inspectDirectory(candidate);
            if (relativeStatus == DicomSeriesStatus::Ok) {
                *resolved = candidate.string();
                return DicomSeriesStatus::Ok;
            }
            if (relativeStatus != DicomSeriesStatus::SourceNotFound) {
                return relativeStatus;
            }
        }
    }

    if (!asset.sourceAbsPath.empty()) {
        const std::filesystem::path absolute(asset.sourceAbsPath);
        if (!absolute.is_absolute()) {
            return DicomSeriesStatus::InvalidArgument;
        }
        const DicomSeriesStatus absoluteStatus = inspectDirectory(absolute);
        if (absoluteStatus == DicomSeriesStatus::Ok) {
            *resolved = absolute.lexically_normal().string();
        }
        return absoluteStatus;
    }
    return DicomSeriesStatus::SourceNotFound;
}

} // namespace

ImageResourceResolveResult ImageResourceResolver::acquire(
    const AssetId& assetId,
    const XQImageVolume& persistedImage,
    const std::string& projectFilePath,
    GeometryResourceManager& manager,
    const AssetRegistry& registry,
    IDicomSeriesReader& reader)
{
    ImageResourceResolveResult result;
    if (!manager.usesAssetRegistry(&registry)) {
        addError(&result, DicomSeriesStatus::InvalidArgument,
                 "The image resource manager belongs to a different project registry.");
        return result;
    }

    const AssetRecord* registeredAsset = registry.find(assetId);
    if (!assetId.is_valid() || registeredAsset == nullptr) {
        addError(&result, DicomSeriesStatus::InvalidArgument,
                 "The persisted image asset metadata is incomplete or inconsistent.");
        return result;
    }
    const AssetRecord asset = *registeredAsset;
    if (!validPersistedMetadata(asset, persistedImage)) {
        addError(&result, DicomSeriesStatus::InvalidArgument,
                 "The persisted image asset metadata is incomplete or inconsistent.");
        return result;
    }

    GeometryResourceManager::VoxelSourceHandle resident =
        manager.acquireResidentVoxelSource(assetId);
    if (resident.valid()) {
        if (!sourceMetaMatches(resident->meta(), persistedImage)) {
            manager.removeResidentVoxelSourceIfMatches(assetId, resident);
            addError(&result, DicomSeriesStatus::SourceChanged,
                     "The resident image resource no longer matches persisted metadata.");
            return result;
        }
        result.status = DicomSeriesStatus::Ok;
        result.source = std::move(resident);
        return result;
    }

    std::string sourceDirectory;
    const DicomSeriesStatus locatorStatus =
        resolveSourceDirectory(asset, projectFilePath, &sourceDirectory);
    if (locatorStatus != DicomSeriesStatus::Ok) {
        addError(&result, locatorStatus,
                 locatorStatus == DicomSeriesStatus::SourceNotFound
                     ? "The persisted DICOM source directory is unavailable."
                     : "The persisted DICOM source locator is invalid or unreadable.");
        return result;
    }

    DicomSeriesReadResult read =
        reader.read(sourceDirectory, asset.dicom.seriesInstanceUid);
    appendSafeDicomReaderDiagnostics(
        &result.diagnostics, read.diagnostics, "dicom.resource");
    if (read.status != DicomSeriesStatus::Ok) {
        addError(&result, read.status,
                 "The persisted DICOM series could not be read.");
        return result;
    }
    if (!read.ok()) {
        addError(&result, DicomSeriesStatus::ReadFailed,
                 "The DICOM reader did not produce a valid voxel buffer.");
        return result;
    }
    if (!readMatchesPersistedMetadata(read, asset, persistedImage)) {
        addError(&result, DicomSeriesStatus::SourceChanged,
                 "The DICOM source no longer matches the persisted image asset.");
        return result;
    }

    const AssetRecord* currentAsset = registry.find(assetId);
    if (currentAsset == nullptr
        || !samePersistedAssetVersion(asset, *currentAsset, persistedImage)) {
        addError(&result, DicomSeriesStatus::SourceChanged,
                 "The persisted image asset changed while its source was being read.");
        return result;
    }

    std::shared_ptr<const IVoxelSource> source =
        std::make_shared<ResidentVoxelSource>(read.buffer);
    GeometryResourceManager::VoxelSourceHandle installed =
        manager.installResidentVoxelSource(assetId, std::move(source));
    if (!installed.valid()) {
        installed = manager.acquireResidentVoxelSource(assetId);
    }
    if (installed.valid()
        && !sourceMetaMatches(installed->meta(), persistedImage)) {
        manager.removeResidentVoxelSourceIfMatches(assetId, installed);
        installed = GeometryResourceManager::VoxelSourceHandle();
    }
    if (!installed.valid()) {
        addError(&result, DicomSeriesStatus::ReadFailed,
                 "The validated DICOM source could not be installed for runtime access.");
        return result;
    }

    result.status = DicomSeriesStatus::Ok;
    result.source = std::move(installed);
    return result;
}

} // namespace xq
