#pragma once

#include <xqModuleSimulationExports.h>

#include <xq_PipelineDataUtils.h>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class xq_Grid;
class xq_SolverJob;
class xq_VascularGeometry;

struct XQMODULESIMULATION_EXPORT xq_FlowSolverInput
{
    const xq_SolverJob* solverJob = nullptr;
    xq_VascularGeometry* geometry = nullptr;
    const xq_Grid* grid = nullptr;
    std::string jobName;
    std::filesystem::path caseDir;
    std::map<std::string, std::string> faceRoles;
    std::vector<std::pair<double, double>> inletWaveform;
};

struct XQMODULESIMULATION_EXPORT xq_FlowSolverRunOptions
{
    int numProcessors = 1;
    std::string mpiPath = "mpiexec";
};

struct XQMODULESIMULATION_EXPORT xq_FlowSolverCapabilities
{
    bool supports_transient = false;
    bool supports_steady = false;
    bool supports_rigid_wall = false;
    bool supports_deformable_wall = false;
    bool supports_newtonian_fluid = false;
    bool supports_non_newtonian_fluid = false;
    bool supports_resistance_bc = false;
    bool supports_rcr_bc = false;
    bool supports_pressure_bc = false;
    bool supports_prescribed_velocity_bc = false;
    bool supports_multiple_inlets = false;
    bool supports_mpi = false;
    bool supports_restart = false;
    bool supports_result_import = false;
    std::vector<std::string> supported_mesh_types;
    std::vector<std::string> supported_units;
};

struct XQMODULESIMULATION_EXPORT xq_FlowSolverValidationResult
{
    bool ok = false;
    std::string diagnostic;
    std::vector<xq::pipeline::Diagnostic> diagnostics;
};

struct XQMODULESIMULATION_EXPORT xq_FlowSolverPrepareResult
{
    bool ok = false;
    std::string diagnostic;
    std::vector<xq::pipeline::Diagnostic> diagnostics;
    std::vector<std::filesystem::path> filesWritten;
};

struct XQMODULESIMULATION_EXPORT xq_FlowSolverRunResult
{
    bool ok = false;
    std::string diagnostic;
    int exitCode = -1;
    std::vector<xq::pipeline::Diagnostic> diagnostics;
};

struct XQMODULESIMULATION_EXPORT xq_FlowSolverImportResult
{
    bool ok = false;
    std::string diagnostic;
    std::vector<xq::pipeline::Diagnostic> diagnostics;
    std::vector<std::string> resultFiles;
};

class XQMODULESIMULATION_EXPORT xq_FlowSolverBackend
{
public:
    virtual ~xq_FlowSolverBackend() = default;

    virtual std::string GetBackendId() const = 0;
    virtual std::string GetDisplayName() const = 0;
    virtual std::string GetVersion() const = 0;
    virtual xq_FlowSolverCapabilities GetCapabilities() const = 0;

    virtual xq_FlowSolverValidationResult Validate(const xq_FlowSolverInput& input) const = 0;
    virtual xq_FlowSolverPrepareResult PrepareCase(
        const xq_FlowSolverInput& input,
        const std::filesystem::path& caseDir) const = 0;
    virtual xq_FlowSolverRunResult RunCase(
        const std::filesystem::path& caseDir,
        const xq_FlowSolverRunOptions& options) = 0;
    virtual xq_FlowSolverImportResult ImportResults(
        const std::filesystem::path& caseDir) const = 0;
};
