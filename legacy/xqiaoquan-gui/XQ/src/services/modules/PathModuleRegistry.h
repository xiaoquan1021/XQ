#ifndef XQ_SERVICES_MODULES_PATH_MODULE_REGISTRY_H
#define XQ_SERVICES_MODULES_PATH_MODULE_REGISTRY_H

#include "core/XQVesselPath.h"

#include <memory>
#include <string>
#include <vector>

namespace xq {

struct PathModuleDescriptor {
    std::string id;
    std::string name;
    std::string version;
};

enum class PathModuleExecutionStatus {
    Ok,
    InvalidPath,
    Failed
};

struct PathModuleExecutionResult {
    PathModuleExecutionStatus status = PathModuleExecutionStatus::Failed;
    VesselPathValidationResult validation;
    std::string diagnostic;

    bool ok() const { return status == PathModuleExecutionStatus::Ok; }
};

class IPathModule {
public:
    virtual ~IPathModule() = default;

    virtual PathModuleDescriptor descriptor() const = 0;
    virtual PathModuleExecutionResult run(const VesselPathV1& path) const = 0;
};

class PathModuleRegistry {
public:
    enum class RegisterStatus {
        Ok,
        NullModule,
        InvalidDescriptor,
        DuplicateId
    };

    enum class RunStatus {
        Ok,
        UnknownModule,
        ModuleFailed
    };

    struct RunResult {
        RunStatus status = RunStatus::UnknownModule;
        PathModuleDescriptor descriptor;
        PathModuleExecutionResult execution;

        bool ok() const
        {
            return status == RunStatus::Ok && execution.ok();
        }
    };

    PathModuleRegistry();
    ~PathModuleRegistry();
    PathModuleRegistry(PathModuleRegistry&& other) noexcept;
    PathModuleRegistry& operator=(PathModuleRegistry&& other) noexcept;

    PathModuleRegistry(const PathModuleRegistry&) = delete;
    PathModuleRegistry& operator=(const PathModuleRegistry&) = delete;

    RegisterStatus registerModule(std::unique_ptr<IPathModule> module);
    std::vector<PathModuleDescriptor> descriptors() const;
    RunResult run(const std::string& id, const VesselPathV1& path) const;

    static PathModuleRegistry builtIn();

private:
    struct Entry {
        PathModuleDescriptor descriptor;
        std::unique_ptr<IPathModule> module;
    };

    std::vector<Entry> entries_;
};

} // namespace xq

#endif // XQ_SERVICES_MODULES_PATH_MODULE_REGISTRY_H
