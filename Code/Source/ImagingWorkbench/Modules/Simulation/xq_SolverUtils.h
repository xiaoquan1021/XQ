#pragma once

#include <xqModuleSimulationExports.h>
#include "xq_SolverJob.h"

#include <filesystem>
#include <string>

namespace xq::solver {

[[nodiscard]] XQMODULESIMULATION_EXPORT
std::filesystem::path createFlowFile(const xq_SolverJob& job,
                                     const std::filesystem::path& outputDir);

[[nodiscard]] XQMODULESIMULATION_EXPORT
std::filesystem::path createSolverInputFile(const xq_SolverJob& job,
                                            const std::filesystem::path& outputDir);

[[nodiscard]] XQMODULESIMULATION_EXPORT
std::filesystem::path writePresolverScript(const xq_SolverJob& job,
                                           const std::filesystem::path& outputDir);

} // namespace xq::solver

// Legacy wrapper kept for backward compatibility with existing plugin code
class XQMODULESIMULATION_EXPORT xq_SolverUtils
{
public:
    static std::string CreateFlowFile(xq_SolverJob* job, const std::string& outputDir);
    static std::string CreateSolverInputFile(xq_SolverJob* job, const std::string& outputDir);
    static std::string WritePresolverScript(xq_SolverJob* job, const std::string& outputDir);
};
