#ifndef XQ_UI_CONTROLLERS_PATH_MODULE_CONTROLLER_H
#define XQ_UI_CONTROLLERS_PATH_MODULE_CONTROLLER_H

#include "core/NodeId.h"
#include "core/XQVesselPath.h"
#include "services/modules/PathModuleRegistry.h"
#include "services/path/ShellGeometrySmokeService.h"
#include "services/profile/VesselPathSnapshotService.h"

#include <string>
#include <vector>

namespace xq {

class XQProject;

class PathModuleController {
public:
    explicit PathModuleController(XQProject* project);

    enum class Status {
        Ok,
        NullContext,
        ProjectNotOpen,
        InvalidIntent,
        SourceNotFound,
        SourceTypeMismatch,
        SourceStale,
        SourcePayloadInvalid,
        SourceAssetInvalid,
        SnapshotFailed,
        GeometrySmokeFailed,
        UnknownModule,
        ModuleFailed
    };

    struct Intent {
        NodeId sourceProfileNode;
        std::string moduleId;
    };

    struct Result {
        Status status = Status::InvalidIntent;
        VesselPathSnapshotService::Status snapshotStatus =
            VesselPathSnapshotService::Status::InvalidSourceStamp;
        ShellGeometrySmokeService::Status smokeStatus =
            ShellGeometrySmokeService::Status::InvalidPath;
        PathModuleRegistry::RunStatus registryStatus =
            PathModuleRegistry::RunStatus::UnknownModule;
        PathModuleExecutionStatus moduleStatus =
            PathModuleExecutionStatus::Failed;
        PathModuleDescriptor module;
        VesselPathSourceKind sourceKind = VesselPathSourceKind::Unknown;
        ShellGeometrySmokeService::Summary geometry;
        VesselProfileValidationResult profileValidation;
        VesselPathValidationResult validation;
        std::string diagnostic;

        bool ok() const { return status == Status::Ok; }
    };

    std::vector<PathModuleDescriptor> modules() const;
    Result run(const Intent& intent) const;

private:
    XQProject* project_;
    PathModuleRegistry registry_;
};

} // namespace xq

#endif // XQ_UI_CONTROLLERS_PATH_MODULE_CONTROLLER_H
