#include "xq_SimpleFlowSolverBackend.h"

#include "xq_FlowSolverExportWriter.h"
#include "xq_SolverJob.h"

#include <xq_Grid.h>

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkXMLUnstructuredGridWriter.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace
{

constexpr const char* kResultFile = "xq_simple_flow_result_0001.vtu";
constexpr const char* kRunLogFile = "xq_simple_flow_run.log";
constexpr const char* kManifestFile = "xq_simple_flow_results.txt";

bool EnsureDir(const fs::path& dir, std::string& diagnostic)
{
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec)
    {
        diagnostic = "Failed to create directory '" + dir.string() +
                     "': " + ec.message();
        return false;
    }
    return true;
}

double FirstFiniteExtent(double minValue, double maxValue)
{
    const double extent = maxValue - minValue;
    if (std::isfinite(extent) && extent > 1e-12)
        return extent;
    return 1.0;
}

} // namespace

std::string xq_SimpleFlowSolverBackend::GetBackendId() const
{
    return "xq_simple_flow";
}

std::string xq_SimpleFlowSolverBackend::GetDisplayName() const
{
    return "XQ Simple Native Flow Solver";
}

std::string xq_SimpleFlowSolverBackend::GetVersion() const
{
    return "1";
}

xq_FlowSolverCapabilities xq_SimpleFlowSolverBackend::GetCapabilities() const
{
    xq_FlowSolverCapabilities caps;
    caps.supports_steady = true;
    caps.supports_transient = false;
    caps.supports_rigid_wall = true;
    caps.supports_deformable_wall = false;
    caps.supports_newtonian_fluid = true;
    caps.supports_non_newtonian_fluid = false;
    caps.supports_resistance_bc = true;
    caps.supports_rcr_bc = true;
    caps.supports_pressure_bc = true;
    caps.supports_prescribed_velocity_bc = true;
    caps.supports_multiple_inlets = false;
    caps.supports_mpi = false;
    caps.supports_restart = false;
    caps.supports_result_import = true;
    caps.supported_mesh_types = {"xq_MitkGrid"};
    caps.supported_units = {"cm", "s", "cm^3/s", "dyn/cm^2", "dyn*s/cm^2", "g/cm^3"};
    return caps;
}

xq_FlowSolverValidationResult xq_SimpleFlowSolverBackend::Validate(
    const xq_FlowSolverInput& input) const
{
    xq_FlowSolverValidationResult result;
    if (!input.solverJob)
    {
        result.diagnostic = "Simple flow backend requires a solver job.";
        return result;
    }
    if (!input.geometry || !input.grid)
    {
        result.diagnostic = "Simple flow backend requires geometry and grid data.";
        return result;
    }
    auto* volume = input.grid->GetVolumeMesh();
    if (!volume || volume->GetNumberOfPoints() == 0 || volume->GetNumberOfCells() == 0)
    {
        result.diagnostic = "Simple flow backend requires a non-empty volume mesh.";
        return result;
    }
    if (input.solverJob->GetDeformable())
    {
        result.diagnostic = "Simple flow backend supports rigid-wall jobs only.";
        return result;
    }

    const auto jobDiagnostic = input.solverJob->Validate();
    if (!jobDiagnostic.empty())
    {
        result.diagnostic = jobDiagnostic;
        return result;
    }

    int inflows = 0;
    int outflows = 0;
    for (const auto& bc : input.solverJob->GetBoundaryConditions())
    {
        if (bc.faceRole == "inflow")
            ++inflows;
        else if (bc.faceRole == "outflow")
            ++outflows;
        else if (bc.faceRole == "wall" && bc.bcType != "no_slip")
        {
            result.diagnostic = "Simple flow backend requires wall faces to use no_slip.";
            return result;
        }

        if (bc.bcType == "impedance" || bc.bcType == "coronary")
        {
            result.diagnostic = "Simple flow backend does not support impedance or coronary BCs.";
            return result;
        }
    }
    if (inflows != 1)
    {
        result.diagnostic = "Simple flow backend requires exactly one inflow BC.";
        return result;
    }
    if (outflows < 1)
    {
        result.diagnostic = "Simple flow backend requires at least one outflow BC.";
        return result;
    }
    if (input.inletWaveform.empty())
    {
        result.diagnostic = "Simple flow backend requires an inlet waveform or constant inflow.";
        return result;
    }

    result.ok = true;
    result.diagnostic =
        "Validated for XQ simple native reduced flow backend. This backend "
        "generates a deterministic analytic field for workflow testing; it is "
        "not a full Navier-Stokes CFD solver.";
    return result;
}

xq_FlowSolverPrepareResult xq_SimpleFlowSolverBackend::PrepareCase(
    const xq_FlowSolverInput& input,
    const std::filesystem::path& caseDir) const
{
    xq_FlowSolverPrepareResult result;
    const auto validation = Validate(input);
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
    if (!result.ok)
        return result;

    const auto notes = caseDir / "xq_simple_flow_README.txt";
    std::ofstream os(notes);
    if (!os)
    {
        result.ok = false;
        result.diagnostic = "Simple flow backend could not write " + notes.string();
        return result;
    }
    os << "XQ Simple Native Flow Solver\n";
    os << "Backend ID: " << GetBackendId() << "\n";
    os << "Version: " << GetVersion() << "\n";
    os << "Purpose: deterministic workflow/result-import backend for XQ.\n";
    os << "Limitation: this is a reduced analytic demonstrator, not a full CFD solver.\n";
    os << "Replacement point: implement another xq_FlowSolverBackend and register it.\n";
    result.filesWritten.push_back(notes);
    result.diagnostics.push_back({
        xq::pipeline::Severity::Warning,
        "xq_simple_flow is a reduced analytic backend for native XQ workflow validation, not a full CFD solver."});

    return result;
}

xq_FlowSolverRunResult xq_SimpleFlowSolverBackend::RunCase(
    const std::filesystem::path& caseDir,
    const xq_FlowSolverRunOptions& options)
{
    xq_FlowSolverRunResult result;
    if (options.numProcessors != 1)
    {
        result.diagnostic = "Simple flow backend is serial-only; numProcessors must be 1.";
        return result;
    }

    std::string diagnostic;
    if (!EnsureDir(caseDir / "results", diagnostic))
    {
        result.diagnostic = diagnostic;
        return result;
    }

    const auto meshPath = caseDir / "mesh-complete" / "mesh-complete.mesh.vtu";
    auto reader = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
    reader->SetFileName(meshPath.string().c_str());
    reader->Update();
    auto* sourceGrid = reader->GetOutput();
    if (!sourceGrid || sourceGrid->GetNumberOfPoints() == 0 || sourceGrid->GetNumberOfCells() == 0)
    {
        result.diagnostic = "Simple flow backend could not read exported volume mesh: " +
                            meshPath.string();
        return result;
    }

    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    grid->DeepCopy(sourceGrid);

    double bounds[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    grid->GetBounds(bounds);
    const double axisLength = FirstFiniteExtent(bounds[0], bounds[1]);
    const double x0 = bounds[0];

    double averageFlow = 1.0;
    {
        std::ifstream bct(caseDir / "bct.dat");
        int sampleCount = 0;
        int modes = 0;
        if (bct >> sampleCount >> modes && sampleCount > 0)
        {
            double sum = 0.0;
            for (int i = 0; i < sampleCount; ++i)
            {
                double t = 0.0;
                double q = 0.0;
                if (bct >> t >> q)
                    sum += q;
            }
            averageFlow = sum / static_cast<double>(sampleCount);
        }
    }

    const double velocityMagnitude =
        averageFlow / std::max(1.0, std::cbrt(static_cast<double>(grid->GetNumberOfCells())));
    const double basePressure = 80.0;
    const double pressureDrop = std::abs(averageFlow) * 0.1;

    auto pressure = vtkSmartPointer<vtkDoubleArray>::New();
    pressure->SetName("pressure");
    pressure->SetNumberOfComponents(1);
    pressure->SetNumberOfTuples(grid->GetNumberOfPoints());

    auto velocity = vtkSmartPointer<vtkDoubleArray>::New();
    velocity->SetName("velocity");
    velocity->SetNumberOfComponents(3);
    velocity->SetNumberOfTuples(grid->GetNumberOfPoints());

    for (vtkIdType i = 0; i < grid->GetNumberOfPoints(); ++i)
    {
        double p[3];
        grid->GetPoint(i, p);
        const double s = std::clamp((p[0] - x0) / axisLength, 0.0, 1.0);
        pressure->SetValue(i, basePressure - pressureDrop * s);
        velocity->SetTuple3(i, velocityMagnitude, 0.0, 0.0);
    }

    auto wallShear = vtkSmartPointer<vtkDoubleArray>::New();
    wallShear->SetName("wall_shear");
    wallShear->SetNumberOfComponents(1);
    wallShear->SetNumberOfTuples(grid->GetNumberOfCells());
    const double shear = std::abs(velocityMagnitude) * 0.04;
    for (vtkIdType i = 0; i < grid->GetNumberOfCells(); ++i)
        wallShear->SetValue(i, shear);

    grid->GetPointData()->AddArray(pressure);
    grid->GetPointData()->AddArray(velocity);
    grid->GetPointData()->SetActiveScalars("pressure");
    grid->GetPointData()->SetActiveVectors("velocity");
    grid->GetCellData()->AddArray(wallShear);

    const auto resultPath = caseDir / "results" / kResultFile;
    auto writer = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
    writer->SetFileName(resultPath.string().c_str());
    writer->SetInputData(grid);
    writer->SetDataModeToBinary();
    if (!writer->Write())
    {
        result.diagnostic = "Simple flow backend failed to write " + resultPath.string();
        return result;
    }

    const auto manifestPath = caseDir / kManifestFile;
    {
        std::ofstream manifest(manifestPath);
        if (!manifest)
        {
            result.diagnostic = "Simple flow backend failed to write " + manifestPath.string();
            return result;
        }
        manifest << resultPath.string() << "\n";
    }

    const auto logPath = caseDir / kRunLogFile;
    {
        std::ofstream log(logPath);
        if (!log)
        {
            result.diagnostic = "Simple flow backend failed to write " + logPath.string();
            return result;
        }
        log << "XQ Simple Native Flow Solver completed.\n";
        log << "Input mesh: " << meshPath.string() << "\n";
        log << "Output result: " << resultPath.string() << "\n";
        log << "Average inflow: " << averageFlow << "\n";
        log << "Limitation: reduced analytic demonstrator, not full Navier-Stokes CFD.\n";
    }

    result.ok = true;
    result.exitCode = 0;
    result.diagnostic = "Simple flow backend generated analytic pressure, velocity, and wall_shear fields.";
    result.diagnostics.push_back({
        xq::pipeline::Severity::Warning,
        "xq_simple_flow completed. Results are deterministic reduced analytic fields, not full CFD."});
    return result;
}

xq_FlowSolverImportResult xq_SimpleFlowSolverBackend::ImportResults(
    const std::filesystem::path& caseDir) const
{
    xq_FlowSolverImportResult result;
    const auto manifestPath = caseDir / kManifestFile;
    std::ifstream manifest(manifestPath);
    if (!manifest)
    {
        result.diagnostic = "Simple flow backend could not open result manifest: " +
                            manifestPath.string();
        return result;
    }

    std::string path;
    while (std::getline(manifest, path))
    {
        if (!path.empty())
            result.resultFiles.push_back(path);
    }
    if (result.resultFiles.empty())
    {
        result.diagnostic = "Simple flow backend result manifest is empty.";
        return result;
    }

    for (const auto& file : result.resultFiles)
    {
        if (!fs::exists(file))
        {
            result.diagnostic = "Simple flow backend result file is missing: " + file;
            result.resultFiles.clear();
            return result;
        }
    }

    result.ok = true;
    result.diagnostic = "Simple flow backend found generated result files.";
    result.diagnostics.push_back({
        xq::pipeline::Severity::Warning,
        "Imported xq_simple_flow reduced analytic results; do not interpret as validated CFD."});
    return result;
}
