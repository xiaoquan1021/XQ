#ifndef XQ_SERVICES_PATH_CENTERLINE_B_SERVICE_H
#define XQ_SERVICES_PATH_CENTERLINE_B_SERVICE_H

#include "core/XQDerivationStamp.h"
#include "core/XQPath.h"
#include "core/XQVesselPath.h"
#include "core/XQVesselProfile.h"
#include "core/path/ICenterlineSkeletonizer3D.h"
#include "services/path/CenterlineBGraph.h"
#include "services/path/ShellGeometrySmokeService.h"
#include "services/profile/VesselPathSnapshotService.h"

#include <optional>
#include <string>

namespace xq {

class IVoxelSource;

class CenterlineBService {
public:
    static constexpr const char* AlgorithmId = "xq.centerline-b";
    static constexpr const char* AlgorithmVersion = "1.0.0";

    enum class Status {
        Ok,
        InvalidRequest,
        SkeletonizationFailed,
        GraphFailed,
        PathAssemblyFailed,
        ProfileValidationFailed,
        FingerprintFailed,
        SnapshotFailed,
        GeometrySmokeFailed
    };

    struct Request {
        NodeId sourceImageNode;
        std::string frameOfReferenceId;
        DerivationInputStamp sourceMask;
        NodeId outputPathNode;
        AssetId outputPathAsset;
        NodeId outputProfileNode;
        AssetId outputProfileAsset;
        CenterlineBGraphProfileV1 graphProfile;
    };

    struct Output {
        XQPath path;
        VesselProfileV1 profile;
        VesselPathV1 moduleSnapshot;
        ShellGeometrySmokeService::Summary geometrySmoke;
        CenterlineBGraphStats graphStats;
        std::size_t inputForegroundVoxelCount = 0;
        std::size_t skeletonVoxelCount = 0;
        std::string thinningBackendId;
        std::string thinningBackendVersion;
        std::string distanceBackendId;
        std::string distanceBackendVersion;
        std::string pathContentFingerprint;
        std::string profileContentFingerprint;
    };

    struct Result {
        Status status = Status::InvalidRequest;
        CenterlineSkeletonizationStatus skeletonizationStatus =
            CenterlineSkeletonizationStatus::InvalidSource;
        CenterlineSkeletonizationStage skeletonizationStage =
            CenterlineSkeletonizationStage::ValidateInput;
        CenterlineBGraphStatus graphStatus =
            CenterlineBGraphStatus::InvalidSkeleton;
        XQPath::ResampleStatus pathResampleStatus =
            XQPath::ResampleStatus::NotEnoughPoints;
        VesselProfileValidationResult profileValidation;
        VesselPathSnapshotService::Status snapshotStatus =
            VesselPathSnapshotService::Status::InvalidSourceStamp;
        VesselPathValidationResult pathValidation;
        ShellGeometrySmokeService::Status smokeStatus =
            ShellGeometrySmokeService::Status::InvalidPath;
        std::optional<Output> output;

        bool ok() const
        {
            return status == Status::Ok && output.has_value();
        }
    };

    static Result run(const ICenterlineSkeletonizer3D& skeletonizer,
                      const ImageGeometry& geometry,
                      const IVoxelSource& source,
                      const Request& request);
};

const char* centerlineBServiceStatusToken(CenterlineBService::Status status);

} // namespace xq

#endif // XQ_SERVICES_PATH_CENTERLINE_B_SERVICE_H
