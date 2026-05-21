#pragma once

#include <xqModuleSimulationExports.h>

#include "xq_FlowSolverBackend.h"

class XQMODULESIMULATION_EXPORT xq_SimpleFlowSolverBackend final : public xq_FlowSolverBackend
{
public:
    std::string GetBackendId() const override;
    std::string GetDisplayName() const override;
    std::string GetVersion() const override;
    xq_FlowSolverCapabilities GetCapabilities() const override;

    xq_FlowSolverValidationResult Validate(const xq_FlowSolverInput& input) const override;
    xq_FlowSolverPrepareResult PrepareCase(
        const xq_FlowSolverInput& input,
        const std::filesystem::path& caseDir) const override;
    xq_FlowSolverRunResult RunCase(
        const std::filesystem::path& caseDir,
        const xq_FlowSolverRunOptions& options) override;
    xq_FlowSolverImportResult ImportResults(
        const std::filesystem::path& caseDir) const override;
};
