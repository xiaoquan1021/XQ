#ifndef XQ_UI_CONTROLLERS_FLOW_SMOKE_CONTROLLER_H
#define XQ_UI_CONTROLLERS_FLOW_SMOKE_CONTROLLER_H

#include "core/NodeId.h"
#include "core/XQDerivationStamp.h"
#include "core/XQPayload.h"
#include "core/XQScaleSlot.h"
#include "core/asset/AssetRecord.h"
#include "core/command/XQProjectCommands.h"
#include "services/flow/FlowGeometrySmokeService.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xq {

class XQCommandStack;
class XQProject;

class FlowSmokeController {
private:
    struct ProjectGuard {
        const XQProject* project = nullptr;
        std::uint64_t lifecycleEpoch = 0;
    };

    struct SourceGuard {
        DerivationInputStamp stamp;
        std::shared_ptr<XQPayload> payloadIdentity;
        std::optional<ScaleSlot> scaleSlot;
    };

public:
    FlowSmokeController(XQProject* project, XQCommandStack* stack);

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
        ComputeFailed,
        SourceChanged,
        CommitRejected
    };

    struct OutputIntent {
        NodeId caseNode;
        std::string caseName;
        NodeId resultNode;
        std::string resultName;
        std::optional<AssetRecord> caseAsset;
        std::optional<AssetRecord> resultAsset;
    };

    struct SmokeIntent {
        NodeId sourceProfileNode;
        OutputIntent output;
    };

    struct CapturedInput {
        Status status = Status::InvalidIntent;

        bool ok() const { return status == Status::Ok; }

    private:
        friend class FlowSmokeController;
        FlowGeometrySmokeService::Request request_;
        OutputIntent output_;
        ProjectGuard projectGuard_;
        SourceGuard sourceGuard_;
    };

    struct PreparedCommand {
        Status status = Status::InvalidIntent;
        FlowGeometrySmokeService::Status serviceStatus =
            FlowGeometrySmokeService::Status::InvalidRequest;
        FlowInputAssembler::Status assemblyStatus =
            FlowInputAssembler::Status::InvalidProfile;
        FlowSolver1D::Status solverStatus = FlowSolver1D::Status::EmptyCenterline;

        bool ok() const { return status == Status::Ok && specs_.size() == 2; }

    private:
        friend class FlowSmokeController;
        std::vector<ProjectNodeBatchSpec> specs_;
        ProjectGuard projectGuard_;
        SourceGuard sourceGuard_;
        OutputIntent output_;
    };

    CapturedInput capture(const SmokeIntent& intent) const;
    static PreparedCommand compute(CapturedInput captured);
    PreparedCommand prepare(const SmokeIntent& intent) const;
    Status commitPrepared(PreparedCommand prepared) const;
    Status run(const SmokeIntent& intent) const;

private:
    Status validateOutput(const OutputIntent& output) const;
    bool sourceStillCurrent(const SourceGuard& guard) const;
    bool targetAssetsStillAvailable(const OutputIntent& output) const;

    XQProject* project_;
    XQCommandStack* stack_;
};

} // namespace xq

#endif // XQ_UI_CONTROLLERS_FLOW_SMOKE_CONTROLLER_H
