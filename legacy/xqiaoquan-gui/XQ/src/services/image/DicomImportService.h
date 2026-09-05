#ifndef XQ_SERVICES_IMAGE_DICOM_IMPORT_SERVICE_H
#define XQ_SERVICES_IMAGE_DICOM_IMPORT_SERVICE_H

#include "core/NodeId.h"
#include "core/asset/AssetId.h"
#include "core/command/XQProjectCommands.h"
#include "core/image/IDicomSeriesReader.h"
#include "core/source/IVoxelSource.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xq {

// Pure preparation input. Stable node/asset ids are supplied by the caller so
// background preparation never mutates a project merely to allocate identity.
struct DicomImportRequest {
    std::string sourceDirectory;
    std::string sourceRelPath;
    std::string seriesInstanceUid;
    NodeId nodeId;
    AssetId assetId;
};

struct DicomImportResult {
    DicomSeriesStatus status = DicomSeriesStatus::InvalidArgument;
    std::optional<ProjectNodeBatchSpec> batchSpec;
    std::shared_ptr<const IVoxelSource> residentSource;
    std::vector<Diagnostic> diagnostics;

    bool ok() const
    {
        return status == DicomSeriesStatus::Ok
            && batchSpec.has_value() && residentSource != nullptr;
    }
};

// Converts a selected DICOM series into an atomic project mutation proposal
// plus a resident voxel source. The service has no XQProject/cache reference
// and therefore has zero project side effects. Callers submit the returned
// ProjectNodeBatchSpec on the owner thread and install residency only after the
// command succeeds.
class DicomImportService {
public:
    static DicomImportResult prepare(IDicomSeriesReader& reader,
                                     const DicomImportRequest& request);
};

} // namespace xq

#endif // XQ_SERVICES_IMAGE_DICOM_IMPORT_SERVICE_H
