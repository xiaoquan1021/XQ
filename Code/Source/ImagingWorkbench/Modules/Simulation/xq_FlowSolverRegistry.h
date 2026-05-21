#pragma once

#include <xqModuleSimulationExports.h>

#include "xq_FlowSolverBackend.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

class XQMODULESIMULATION_EXPORT xq_FlowSolverRegistry
{
public:
    static xq_FlowSolverRegistry& Instance();

    bool RegisterBackend(std::unique_ptr<xq_FlowSolverBackend> backend);
    xq_FlowSolverBackend* FindBackend(std::string_view backendId);
    const xq_FlowSolverBackend* FindBackend(std::string_view backendId) const;
    std::vector<std::string> ListBackendIds() const;

private:
    xq_FlowSolverRegistry();

    std::vector<std::unique_ptr<xq_FlowSolverBackend>> m_Backends;
};

