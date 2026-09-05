#include <services/modules/PathModuleRegistry.h>

#include <cstdio>
#include <memory>
#include <optional>
#include <string>

namespace {

int fail(const char* expression, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    return 1;
}

#define CHECK(expression)                     \
    do {                                      \
        if (!(expression)) {                  \
            return fail(#expression, __LINE__); \
        }                                     \
    } while (0)

xq::VesselPathV1 path()
{
    xq::VesselPathV1 value;
    value.coordinateSystem = xq::VesselPathCoordinateSystem::LPS;
    value.lengthUnit = xq::VesselPathLengthUnit::Millimeter;
    value.radiusDefinition =
        xq::VesselPathRadiusDefinition::EquivalentCircularArea;
    value.frameOfReferenceId = "module-frame";
    value.source.kind = xq::VesselPathSourceKind::SemiAutomatic;
    value.derivationStamp.algorithmId = "snapshot";
    value.derivationStamp.algorithmVersion = "1";
    value.derivationStamp.inputs.push_back(
        {xq::NodeId(1), 0, std::nullopt, std::string()});
    value.stations = {
        {xq::VesselPathStationId(1), {0.0, 0.0, 0.0}, 1.0, 0.0},
        {xq::VesselPathStationId(2), {1.0, 0.0, 0.0}, 1.0, 1.0},
    };
    return value;
}

class FixedModule final : public xq::IPathModule {
public:
    FixedModule(xq::PathModuleDescriptor descriptor,
                xq::PathModuleExecutionStatus status)
        : descriptor_(std::move(descriptor))
        , status_(status)
    {
    }

    xq::PathModuleDescriptor descriptor() const override
    {
        return descriptor_;
    }

    xq::PathModuleExecutionResult run(
        const xq::VesselPathV1&) const override
    {
        xq::PathModuleExecutionResult result;
        result.status = status_;
        result.diagnostic = status_ == xq::PathModuleExecutionStatus::Ok
            ? "fixed:ok"
            : "fixed:failed";
        return result;
    }

private:
    xq::PathModuleDescriptor descriptor_;
    xq::PathModuleExecutionStatus status_;
};

} // namespace

int main()
{
    xq::PathModuleRegistry registry = xq::PathModuleRegistry::builtIn();
    const std::vector<xq::PathModuleDescriptor> descriptors =
        registry.descriptors();
    CHECK(descriptors.size() == 2);
    CHECK(descriptors[0].id == "noop");
    CHECK(descriptors[1].id == "path-validate");
    CHECK(registry.run("noop", path()).ok());
    CHECK(registry.run("path-validate", path()).ok());

    xq::VesselPathV1 invalid = path();
    invalid.stations[0].radiusMm = 0.0;
    const xq::PathModuleRegistry::RunResult invalidRun =
        registry.run("path-validate", invalid);
    CHECK(!invalidRun.ok());
    CHECK(invalidRun.status == xq::PathModuleRegistry::RunStatus::ModuleFailed);
    CHECK(invalidRun.execution.status
          == xq::PathModuleExecutionStatus::InvalidPath);
    CHECK(invalidRun.execution.validation.hasIssue(
        xq::VesselPathValidationCode::NonPositiveRadius));

    const xq::PathModuleRegistry::RunResult unknown =
        registry.run("missing", path());
    CHECK(unknown.status == xq::PathModuleRegistry::RunStatus::UnknownModule);
    CHECK(unknown.execution.diagnostic == "registry:unknown-module:missing");

    CHECK(registry.registerModule(nullptr)
          == xq::PathModuleRegistry::RegisterStatus::NullModule);
    CHECK(registry.registerModule(std::make_unique<FixedModule>(
              xq::PathModuleDescriptor{"", "Bad", "1"},
              xq::PathModuleExecutionStatus::Ok))
          == xq::PathModuleRegistry::RegisterStatus::InvalidDescriptor);
    CHECK(registry.registerModule(std::make_unique<FixedModule>(
              xq::PathModuleDescriptor{"noop", "Duplicate", "1"},
              xq::PathModuleExecutionStatus::Ok))
          == xq::PathModuleRegistry::RegisterStatus::DuplicateId);
    CHECK(registry.registerModule(std::make_unique<FixedModule>(
              xq::PathModuleDescriptor{"failure", "Failure", "1"},
              xq::PathModuleExecutionStatus::Failed))
          == xq::PathModuleRegistry::RegisterStatus::Ok);
    const xq::PathModuleRegistry::RunResult failed =
        registry.run("failure", path());
    CHECK(failed.status == xq::PathModuleRegistry::RunStatus::ModuleFailed);
    CHECK(failed.execution.diagnostic == "fixed:failed");

    std::printf("static Path module registry checks passed\n");
    return 0;
}
