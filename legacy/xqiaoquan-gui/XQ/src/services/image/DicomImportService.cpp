#include "services/image/DicomImportService.h"

#include "services/image/DicomServiceSupport.h"

#include "core/XQImageVolumePayload.h"
#include "core/XQScaleSlot.h"
#include "core/source/ResidentVoxelSource.h"

#include <filesystem>
#include <memory>
#include <utility>

namespace xq {
namespace {

void addError(DicomImportResult* result,
              DicomSeriesStatus status,
              const std::string& message)
{
    result->status = status;
    result->diagnostics.emplace_back(
        DiagnosticSeverity::Error,
        std::string("dicom.import.") + dicomSeriesStatusToken(status),
        message);
}

bool readResultConsistent(const DicomImportRequest& request,
                          const DicomSeriesReadResult& read)
{
    if (!read.ok()
        || !isValidCanonicalDicomImageMetadata(read.volume)
        || !isValidDicomSeriesFingerprint(read.descriptor.contentFingerprint)
        || read.descriptor.identity.seriesInstanceUid != request.seriesInstanceUid
        || read.volume.dicomIdentity().seriesInstanceUid != request.seriesInstanceUid
        || !sameDicomSeriesIdentity(
            read.descriptor.identity, read.volume.dicomIdentity())
        || read.descriptor.modality != read.volume.modality()
        || !isValidVoxelBufferForImage(read.volume, *read.buffer)) {
        return false;
    }

    const ImageGeometry& geometry = read.volume.geometry();
    for (int axis = 0; axis < 3; ++axis) {
        if (read.descriptor.dimensions[axis] != geometry.dimensions[axis]) {
            return false;
        }
    }
    return read.descriptor.sliceCount
        == static_cast<std::size_t>(geometry.dimensions[2]);
}

std::string absoluteLocator(const std::string& sourceDirectory)
{
    std::error_code error;
    const std::filesystem::path absolute =
        std::filesystem::absolute(std::filesystem::path(sourceDirectory), error);
    return error ? sourceDirectory : absolute.lexically_normal().string();
}

} // namespace

DicomImportResult DicomImportService::prepare(
    IDicomSeriesReader& reader,
    const DicomImportRequest& request)
{
    DicomImportResult result;
    const std::filesystem::path sourceRelPath(request.sourceRelPath);
    if (request.sourceDirectory.empty() || request.seriesInstanceUid.empty()
        || !request.nodeId.is_valid() || !request.assetId.is_valid()
        || (!request.sourceRelPath.empty()
            && (sourceRelPath.is_absolute() || sourceRelPath.has_root_name()
                || sourceRelPath.has_root_directory()))) {
        addError(&result, DicomSeriesStatus::InvalidArgument,
                 "A source directory, explicit series UID, stable node id and stable asset id are required.");
        return result;
    }

    DicomSeriesReadResult read =
        reader.read(request.sourceDirectory, request.seriesInstanceUid);
    result.status = read.status;
    appendSafeDicomReaderDiagnostics(
        &result.diagnostics, read.diagnostics, "dicom.import");
    if (read.status != DicomSeriesStatus::Ok) {
        addError(&result, read.status,
                 "The selected DICOM series could not be read.");
        return result;
    }
    if (!readResultConsistent(request, read)) {
        addError(&result, DicomSeriesStatus::InconsistentSeries,
                 "The selected DICOM series returned inconsistent XQ image metadata.");
        return result;
    }

    DicomSeriesDescriptor safeDescriptor = read.descriptor;
    safeDescriptor.identity = read.volume.dicomIdentity();
    safeDescriptor.modality = read.volume.modality();
    safeDescriptor.sliceCount = static_cast<std::size_t>(
        read.volume.geometry().dimensions[2]);
    for (int axis = 0; axis < 3; ++axis) {
        safeDescriptor.dimensions[axis] = read.volume.geometry().dimensions[axis];
    }
    const std::string safeDisplayName =
        makeSafeDicomSeriesDisplayName(safeDescriptor);

    XQDataNode node(
        request.nodeId,
        XQDomainType::Image,
        safeDisplayName,
        std::make_shared<XQImageVolumePayload>(read.volume));
    node.setScaleSlot(ScaleSlot::Organ);

    AssetRecord asset;
    asset.id = request.assetId;
    asset.category = AssetCategory::ExternalSource;
    asset.kind = AssetKind::Image;
    asset.sourceAbsPath = absoluteLocator(request.sourceDirectory);
    asset.sourceRelPath = request.sourceRelPath;
    asset.dicom = read.volume.dicomIdentity();
    asset.hasDicom = true;
    asset.contentFingerprint = read.descriptor.contentFingerprint;
    asset.hasGeometry = true;
    asset.geometry = read.volume.geometry();
    asset.displayName = safeDisplayName;

    std::shared_ptr<const IVoxelSource> resident =
        std::make_shared<ResidentVoxelSource>(read.buffer);
    if (!resident->meta().valid) {
        addError(&result, DicomSeriesStatus::ReadFailed,
                 "The decoded DICOM voxel source is internally inconsistent.");
        return result;
    }

    ProjectNodeBatchSpec batch(std::move(node));
    batch.assetToRegister = std::move(asset);
    batch.bindAsset = request.assetId;
    result.batchSpec = std::move(batch);
    result.residentSource = std::move(resident);
    result.status = DicomSeriesStatus::Ok;
    return result;
}

} // namespace xq
