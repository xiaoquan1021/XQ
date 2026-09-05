#include "xq_FlowSolverRegistry.h"

#include "xq_FlowSolverExportWriter.h"
#include "xq_SimpleFlowSolverBackend.h"
#include "xq_SolverJob.h"

#include <algorithm>
#include <utility>

namespace
{

class xq_ExportOnlyFlowSolverBackend final : public xq_FlowSolverBackend
{
public:
    std::string GetBackendId() const override { return "xq_export_only"; }
    std::string GetDisplayName() const override { return "XQ Export-Only Solver Backend"; }
    std::string GetVersion() const override { return "1"; }

    xq_FlowSolverCapabilities GetCapabilities() const override
    {
        xq_FlowSolverCapabilities caps;
        caps.supports_steady = true;
        caps.supports_transient = true;
        caps.supports_rigid_wall = true;
        caps.supports_deformable_wall = true;
        caps.supports_newtonian_fluid = true;
        caps.supports_resistance_bc = true;
        caps.supports_rcr_bc = true;
        caps.supports_pressure_bc = true;
        caps.supports_prescribed_velocity_bc = true;
        caps.supports_multiple_inlets = false;
        caps.supports_restart = false;
        caps.supports_mpi = false;
        caps.supports_result_import = false;
        caps.supported_mesh_types = {"xq_MitkGrid"};
        caps.supported_units = {"cm", "s", "cm^3/s", "dyn*s/cm^2", "g/cm^3"};
        return caps;
    }

    xq_FlowSolverValidationResult Validate(const xq_FlowSolverInput& input) const override
    {
        xq_FlowSolverValidationResult result;
        if (!input.solverJob)
        {
            result.diagnostic = "Export-only backend requires a solver job.";
            return result;
        }
        if (!input.geometry || !input.grid)
        {
            result.diagnostic = "Export-only backend requires geometry and grid data.";
            return result;
        }
        const auto diag = input.solverJob->Validate();
        if (!diag.empty())
        {
            result.diagnostic = diag;
            return result;
        }

        result.ok = true;
        return result;
    }

    xq_FlowSolverPrepareResult PrepareCase(
        const xq_FlowSolverInput& input,
        const std::filesystem::path& caseDir) const override
    {
        xq_FlowSolverPrepareResult result;
        auto validation = Validate(input);
        if (!validation.ok)
        {
            result.diagnostic = validation.diagnostic;
            result.diagnostics = validation.diagnostics;
            return result;
        }

        xq_FlowSolverExportRequest request;
        request.outputDir = caseDir;
        request.jobName = input.jobName.empty()
            ? input.solverJob->GetJobName()
            : input.jobName;
        request.solverJob = input.solverJob;
        request.geometry = input.geometry;
        request.grid = input.grid;
        request.inletWaveform = input.inletWaveform;
        request.faceRoles = input.faceRoles;

        const auto exportResult = xq_FlowSolverExportWriter::Export(request);
        result.diagnostics = exportResult.diagnostics;
        result.filesWritten = exportResult.filesWritten;
        result.ok = exportResult.ok;
        result.diagnostic = exportResult.diagnostic;
        return result;
    }

    xq_FlowSolverRunResult RunCase(
        const std::filesystem::path&,
        const xq_FlowSolverRunOptions&) override
    {
        xq_FlowSolverRunResult result;
        result.diagnostic = "Export-only backend does not support native solver execution.";
        return result;
    }

    xq_FlowSolverImportResult ImportResults(const std::filesystem::path&) const override
    {
        xq_FlowSolverImportResult result;
        result.diagnostic = "Export-only backend does not import results.";
        return result;
    }
};

} // namespace

xq_FlowSolverRegistry& xq_FlowSolverRegistry::Instance()
{
    static xq_FlowSolverRegistry registry;
    return registry;
}

xq_FlowSolverRegistry::xq_FlowSolverRegistry()
{
    RegisterBackend(std::make_unique<xq_ExportOnlyFlowSolverBackend>());
    RegisterBackend(std::make_unique<xq_SimpleFlowSolverBackend>());
}

bool xq_FlowSolverRegistry::RegisterBackend(std::unique_ptr<xq_FlowSolverBackend> backend)
{
    if (!backend)
        return false;

    const auto id = backend->GetBackendId();
    if (id.empty())
        return false;

    auto existing = std::find_if(
        m_Backends.begin(), m_Backends.end(),
        [&id](const std::unique_ptr<xq_FlowSolverBackend>& candidate) {
            return candidate && candidate->GetBackendId() == id;
        });
    if (existing != m_Backends.end())
        return false;

    m_Backends.push_back(std::move(backend));
    return true;
}

xq_FlowSolverBackend* xq_FlowSolverRegistry::FindBackend(std::string_view backendId)
{
    for (auto& backend : m_Backends)
    {
        if (backend && backend->GetBackendId() == backendId)
            return backend.get();
    }
    return nullptr;
}

const xq_FlowSolverBackend* xq_FlowSolverRegistry::FindBackend(std::string_view backendId) const
{
    for (const auto& backend : m_Backends)
    {
        if (backend && backend->GetBackendId() == backendId)
            return backend.get();
    }
    return nullptr;
}

std::vector<std::string> xq_FlowSolverRegistry::ListBackendIds() const
{
    std::vector<std::string> ids;
    ids.reserve(m_Backends.size());
    for (const auto& backend : m_Backends)
    {
        if (backend)
            ids.push_back(backend->GetBackendId());
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}
