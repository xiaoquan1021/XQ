#pragma once

#include <xqModuleSimulationExports.h>

#include <xq_PipelineDataUtils.h>
#include "xq_SolverJob.h"

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

class xq_MitkSolverJob;

struct XQMODULESIMULATION_EXPORT xq_SimulationPrepRequest
{
    std::string jobName;
    int numTimesteps = 200;
    double timeStepSize = 0.001;
    int numCycles = 2;
    bool deformableWall = false;
    std::map<std::string, std::string> faceRoleOverrides;

    // Fluid properties
    double fluidDensity = 1.06;
    double fluidViscosity = 0.04;
    double initialPressure = 0.0;
    double initialVelocity = 0.0;

    // Wall properties
    double wallThickness = 0.5;
    double wallElasticModulus = 4.0e6;
    double wallPoissonRatio = 0.45;
    double wallDensity = 1.0;

    // Solver properties
    std::string solverType = "xq_export_only";
    int numLinearIterations = 5;
    int numNonlinearIterations = 25;

    // Boundary conditions configured from UI (when non-empty, these override
    // automatic face-role inference in CreateOrUpdateSimulationPrep)
    std::vector<xq_BoundaryCondition> boundaryConditions;
};

struct XQMODULESIMULATION_EXPORT xq_SimulationPrepResult
    : public xq::pipeline::OperationStatus
{
    mitk::DataNode::Pointer node;
    xq_MitkSolverJob* solverJobData = nullptr;
};

struct XQMODULESIMULATION_EXPORT xq_SimulationExportRequest
{
    std::string outputDir;                         // on-disk directory
    std::vector<std::pair<double, double>> inletWaveform; // t (s) -> Q
};

struct XQMODULESIMULATION_EXPORT xq_SimulationExportResult
    : public xq::pipeline::OperationStatus
{
    std::vector<std::string> filesWritten;
};

struct XQMODULESIMULATION_EXPORT xq_SimulationRunRequest
{
    std::string caseDir;
    std::vector<std::pair<double, double>> inletWaveform; // t (s) -> Q
    int numProcessors = 1;
    std::string mpiPath = "mpiexec";
};

struct XQMODULESIMULATION_EXPORT xq_SimulationRunResult
    : public xq::pipeline::OperationStatus
{
    int exitCode = -1;
    std::vector<std::string> filesWritten;
    std::vector<mitk::DataNode::Pointer> resultNodes;
};

class XQMODULESIMULATION_EXPORT xq_SimulationPrepPipelineService
{
public:
    static xq_SimulationPrepResult CreateOrUpdateSimulationPrep(
        mitk::DataStorage* dataStorage,
        const mitk::DataNode::Pointer& modelNode,
        const mitk::DataNode::Pointer& meshNode,
        const xq_SimulationPrepRequest& request);

    // Serialize a SimulationPrep node (plus its upstream Model and Mesh)
    // to XQ's existing export directory format. This does not run a solver.
    static xq_SimulationExportResult ExportForSolver(
        mitk::DataStorage* dataStorage,
        const mitk::DataNode::Pointer& simPrepNode,
        const xq_SimulationExportRequest& request);

    // Prepare, run, and import results through the selected registered backend.
    // This does not auto-run from tool opening; callers must invoke it from an
    // explicit Run action.
    static xq_SimulationRunResult RunSolverAndImportResults(
        mitk::DataStorage* dataStorage,
        const mitk::DataNode::Pointer& simPrepNode,
        const xq_SimulationRunRequest& request);
};
