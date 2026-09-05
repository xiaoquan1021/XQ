#ifndef XQ_UI_CONTROLLERS_CENTERLINE_B_CONTROLLER_H
#define XQ_UI_CONTROLLERS_CENTERLINE_B_CONTROLLER_H

#include "core/NodeId.h"
#include "core/XQDerivationStamp.h"
#include "core/XQDomainType.h"
#include "core/XQImageVolume.h"
#include "core/XQPayload.h"
#include "core/XQScaleSlot.h"
#include "core/asset/AssetRecord.h"
#include "core/command/XQProjectCommands.h"
#include "services/path/CenterlineBService.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xq {

class ICenterlineSkeletonizer3D;
class XQCommandStack;
class XQMemoryImageBufferHandle;
class XQProject;

// Project-aware boundary for the vmtk-OFF Centerline B fallback. Capture reads
// and copies XQ-owned mask values on the owner thread, compute invokes the
// injected 3-D skeletonizer without touching the project, and commit publishes
// Path + VesselProfile + Assets as one guarded undo entry.
class CenterlineBController {
private:
    struct ProjectGuard {
        const XQProject* project = nullptr;
        std::uint64_t lifecycleEpoch = 0;
    };

    struct SourceGuard {
        DerivationInputStamp stamp;
        XQDomainType domain = XQDomainType::Unknown;
        std::shared_ptr<XQPayload> payloadIdentity;
        std::optional<ScaleSlot> scaleSlot;
    };

public:
    CenterlineBController(
        XQProject* project,
        XQCommandStack* stack,
        const ICenterlineSkeletonizer3D* skeletonizer);

    enum class Status {
        Ok,
        NullContext,
        ProjectNotOpen,
        InvalidIntent,
        TargetExists,
        SourceNotFound,
        SourceTypeMismatch,
        SourceStale,
        SourcePayloadInvalid,
        SourceAssetInvalid,
        UnsupportedScale,
        AssetIdExhausted,
        ComputeFailed,
        SourceChanged,
        CommitRejected
    };

    struct OutputIntent {
        NodeId pathNode;
        std::string pathName;
        NodeId profileNode;
        std::string profileName;
    };

    struct Intent {
        NodeId sourceMaskNode;
        OutputIntent output;
        CenterlineBGraphProfileV1 graphProfile;
    };

    struct CapturedInput {
        Status status = Status::InvalidIntent;

        bool ok() const { return status == Status::Ok; }

    private:
        friend class CenterlineBController;
        ImageGeometry geometry_{};
        std::shared_ptr<XQMemoryImageBufferHandle> maskBuffer_;
        CenterlineBService::Request request_;
        OutputIntent output_;
        ProjectGuard projectGuard_;
        SourceGuard imageGuard_;
        SourceGuard maskGuard_;
    };

    struct PreparedCommand {
        Status status = Status::InvalidIntent;
        CenterlineBService::Status serviceStatus =
            CenterlineBService::Status::InvalidRequest;
        CenterlineSkeletonizationStatus skeletonizationStatus =
            CenterlineSkeletonizationStatus::InvalidSource;
        CenterlineSkeletonizationStage skeletonizationStage =
            CenterlineSkeletonizationStage::ValidateInput;
        CenterlineBGraphStatus graphStatus =
            CenterlineBGraphStatus::InvalidSkeleton;

        bool ok() const
        {
            return status == Status::Ok && specs_.size() == 2;
        }

    private:
        friend class CenterlineBController;
        std::vector<ProjectNodeBatchSpec> specs_;
        OutputIntent output_;
        AssetId pathAsset_;
        AssetId profileAsset_;
        ProjectGuard projectGuard_;
        SourceGuard imageGuard_;
        SourceGuard maskGuard_;
    };

    CapturedInput capture(const Intent& intent) const;
    PreparedCommand compute(CapturedInput captured) const;
    PreparedCommand prepare(const Intent& intent) const;
    Status commitPrepared(PreparedCommand prepared) const;
    Status run(const Intent& intent) const;

private:
    Status captureSourceGuard(
        const NodeId& id,
        XQDomainType expectedDomain,
        SourceGuard* guard) const;
    bool sourceStillCurrent(const SourceGuard& guard) const;
    bool targetsStillAvailable(const PreparedCommand& prepared) const;

    XQProject* project_;
    XQCommandStack* stack_;
    const ICenterlineSkeletonizer3D* skeletonizer_;
};

const char* centerlineBControllerStatusToken(
    CenterlineBController::Status status);

} // namespace xq

#endif // XQ_UI_CONTROLLERS_CENTERLINE_B_CONTROLLER_H
