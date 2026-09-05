#include "services/modules/PathModuleRegistry.h"

#include <utility>

namespace xq {
namespace {

bool valid_descriptor(const PathModuleDescriptor& descriptor)
{
    return !descriptor.id.empty()
        && !descriptor.name.empty()
        && !descriptor.version.empty();
}

class NoopPathModule final : public IPathModule {
public:
    PathModuleDescriptor descriptor() const override
    {
        return {"noop", "Noop", "1"};
    }

    PathModuleExecutionResult run(const VesselPathV1& path) const override
    {
        PathModuleExecutionResult result;
        result.validation = VesselPathValidator::validate(path);
        if (!result.validation.ok()) {
            result.status = PathModuleExecutionStatus::InvalidPath;
            result.diagnostic = "noop:invalid-path";
            return result;
        }
        result.status = PathModuleExecutionStatus::Ok;
        result.diagnostic = "noop:ok";
        return result;
    }
};

class PathValidateModule final : public IPathModule {
public:
    PathModuleDescriptor descriptor() const override
    {
        return {"path-validate", "PathValidate", "1"};
    }

    PathModuleExecutionResult run(const VesselPathV1& path) const override
    {
        PathModuleExecutionResult result;
        result.validation = VesselPathValidator::validate(path);
        if (!result.validation.ok()) {
            result.status = PathModuleExecutionStatus::InvalidPath;
            result.diagnostic = "path-validate:invalid";
            return result;
        }
        result.status = PathModuleExecutionStatus::Ok;
        result.diagnostic = "path-validate:ok";
        return result;
    }
};

} // namespace

PathModuleRegistry::PathModuleRegistry() = default;
PathModuleRegistry::~PathModuleRegistry() = default;
PathModuleRegistry::PathModuleRegistry(PathModuleRegistry&& other) noexcept =
    default;
PathModuleRegistry& PathModuleRegistry::operator=(
    PathModuleRegistry&& other) noexcept = default;

PathModuleRegistry::RegisterStatus PathModuleRegistry::registerModule(
    std::unique_ptr<IPathModule> module)
{
    if (module == nullptr) {
        return RegisterStatus::NullModule;
    }
    const PathModuleDescriptor descriptor = module->descriptor();
    if (!valid_descriptor(descriptor)) {
        return RegisterStatus::InvalidDescriptor;
    }
    for (const Entry& entry : entries_) {
        if (entry.descriptor.id == descriptor.id) {
            return RegisterStatus::DuplicateId;
        }
    }
    Entry entry;
    entry.descriptor = descriptor;
    entry.module = std::move(module);
    entries_.push_back(std::move(entry));
    return RegisterStatus::Ok;
}

std::vector<PathModuleDescriptor> PathModuleRegistry::descriptors() const
{
    std::vector<PathModuleDescriptor> result;
    result.reserve(entries_.size());
    for (const Entry& entry : entries_) {
        result.push_back(entry.descriptor);
    }
    return result;
}

PathModuleRegistry::RunResult PathModuleRegistry::run(
    const std::string& id,
    const VesselPathV1& path) const
{
    RunResult result;
    for (const Entry& entry : entries_) {
        if (entry.descriptor.id != id) {
            continue;
        }
        result.descriptor = entry.descriptor;
        result.execution = entry.module->run(path);
        result.status = result.execution.ok()
            ? RunStatus::Ok
            : RunStatus::ModuleFailed;
        return result;
    }
    result.execution.diagnostic = "registry:unknown-module:" + id;
    return result;
}

PathModuleRegistry PathModuleRegistry::builtIn()
{
    PathModuleRegistry registry;
    registry.registerModule(std::make_unique<NoopPathModule>());
    registry.registerModule(std::make_unique<PathValidateModule>());
    return registry;
}

} // namespace xq
