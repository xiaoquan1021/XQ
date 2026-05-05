#pragma once

// XQ Pipeline Stage 5 output: serialize a simulation-prep bundle to a
// svsolver-compatible directory on disk. Files produced (all text except vtu):
//
//   <outDir>/<jobName>.svpre           legacy preprocessor script
//   <outDir>/mesh-complete/mesh-complete.mesh.vtu   unstructured volume mesh
//   <outDir>/mesh-complete/<face>.vtp               one polydata per face
//   <outDir>/bct.dat                                 inlet BC waveform table
//
// The writer reads from a solver job (face-role map, time stepping) and a
// mesh grid + model geometry exposed by the Simulation pipeline stage. It
// does NOT touch DataStorage.

#include <xqModuleSimulationExports.h>

#include <xq_PipelineDataUtils.h>

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class xq_SolverJob;
class xq_VascularGeometry;
class xq_Grid;

struct XQMODULESIMULATION_EXPORT xq_SvPreExportRequest
{
    std::filesystem::path outputDir;     // created if missing
    std::string_view      jobName;

    const xq_SolverJob*      solverJob = nullptr;
    xq_VascularGeometry*     geometry  = nullptr;
    const xq_Grid*           grid      = nullptr;

    // Optional inlet waveform: time(s) -> volumetric flow (cm^3/s or mL/s).
    // When empty the .bct.dat writer emits only a constant-flow placeholder.
    std::vector<std::pair<double, double>> inletWaveform;

    // Per-face role map (wall|inflow|outflow). If empty, the writer falls
    // back to the solver job's cap properties.
    std::map<std::string, std::string> faceRoles;
};

struct XQMODULESIMULATION_EXPORT xq_SvPreExportResult
{
    bool ok = false;
    std::string diagnostic;
    std::vector<xq::pipeline::Diagnostic> diagnostics;
    std::vector<std::filesystem::path> filesWritten;
};

class XQMODULESIMULATION_EXPORT xq_SvPreWriter
{
public:
    static xq_SvPreExportResult Export(const xq_SvPreExportRequest& request);
};
