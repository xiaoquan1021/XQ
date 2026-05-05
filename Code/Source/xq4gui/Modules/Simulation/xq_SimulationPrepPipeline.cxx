#include "xq_SimulationPrepPipeline.h"

#include "xq_MitkSolverJob.h"
#include "xq_SolverJob.h"
#include "xq_SvPreWriter.h"

#include <xq_Model.h>
#include <xq_VascularGeometry.h>
#include <xq_VesselCenterline.h>
#include <xq_CenterlineSegment.h>
#include <xq_MitkGrid.h>
#include <xq_Grid.h>

#include <vtkCenterOfMass.h>
#include <vtkPolyData.h>

#include <cmath>
#include <limits>
#include <optional>

namespace
{

auto makeError(std::string message) -> xq::pipeline::Diagnostic
{
    return {xq::pipeline::Severity::Error, std::move(message)};
}

auto makeWarning(std::string message) -> xq::pipeline::Diagnostic
{
    return {xq::pipeline::Severity::Warning, std::move(message)};
}

// Compute the centroid of a face's polygonal mesh in world coordinates.
// Returns {NaN,NaN,NaN} when unavailable.
std::array<double, 3> FaceCentroid(xq_VascularGeometry* geom, int faceId)
{
    std::array<double, 3> c{
        std::nan(""), std::nan(""), std::nan("")};
    if (!geom)
        return c;
    auto face = geom->GetFaceVtkPolyData(faceId);
    if (!face || face->GetNumberOfPoints() == 0)
        return c;
    auto com = vtkSmartPointer<vtkCenterOfMass>::New();
    com->SetInputData(face);
    com->SetUseScalarsAsWeights(false);
    com->Update();
    double p[3];
    com->GetCenter(p);
    c = {p[0], p[1], p[2]};
    return c;
}

double DistanceSquared(const std::array<double, 3>& a, const mitk::Point3D& b)
{
    if (std::isnan(a[0]))
        return std::numeric_limits<double>::infinity();
    const double dx = a[0] - b[0];
    const double dy = a[1] - b[1];
    const double dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

// XQ fix: the previous implementation blindly labelled the first cap as
// "inflow" and the rest as "outflow", which is wrong for any branched
// geometry or even single vessels where the lofting order is arbitrary.
//
// New rule: if a centerline is available, find the cap whose centroid is
// nearest to the centerline START vertex — that is the inflow. Every
// other cap is an outflow. User overrides still win.
auto deriveFaceRoles(
    xq_VascularGeometry* geometry,
    xq_VesselCenterline* centerline,
    const std::map<std::string, std::string>& overrides)
    -> std::map<std::string, std::string>
{
    std::map<std::string, std::string> faceRoles;
    if (!geometry)
        return faceRoles;

    const auto faceInfos = geometry->GetAllFaceInfos();

    std::optional<mitk::Point3D> inletProbe;
    if (centerline)
    {
        if (auto* segment = centerline->GetSegment())
        {
            if (segment->GetTraceVertexCount() == 0 && segment->GetAnchorCount() >= 2)
                segment->Interpolate();
            const auto tv = segment->GetTraceVertices();
            if (!tv.empty())
                inletProbe = tv.front().pos;
        }
    }

    // Identify the closest cap to the inlet probe.
    int bestInflowFaceId = -1;
    if (inletProbe)
    {
        double bestD = std::numeric_limits<double>::infinity();
        for (const auto& fi : faceInfos)
        {
            if (fi.type != "cap")
                continue;
            const double d = DistanceSquared(FaceCentroid(geometry, fi.id), *inletProbe);
            if (d < bestD)
            {
                bestD = d;
                bestInflowFaceId = fi.id;
            }
        }
    }

    int outflowIdx = 0;
    for (const auto& fi : faceInfos)
    {
        std::string role = fi.type;

        // Normalize user-facing FaceInfo.type names to SV solver role names.
        // User assigns "inlet"/"outlet" in the model editor; solver expects
        // "inflow"/"outflow" in bct.dat / svpre.
        if (role == "inlet")
            role = "inflow";
        else if (role == "outlet")
            role = "outflow";

        // If the face is still a generic "cap" (no user override), derive
        // inflow/outflow from centerline proximity.
        if (role == "cap")
        {
            if (fi.id == bestInflowFaceId)
                role = "inflow";
            else
                role = "outflow";
        }

        // Backward compatibility: when no centerline is available we keep
        // the legacy "first cap = inflow" heuristic so existing projects
        // continue to work.
        if (bestInflowFaceId < 0 && role == "cap")
        {
            role = outflowIdx == 0 ? "inflow" : "outflow";
            ++outflowIdx;
        }

        const auto overrideIt = overrides.find(fi.name);
        if (overrideIt != overrides.end())
            role = overrideIt->second;

        faceRoles[fi.name] = role;
    }

    return faceRoles;
}

} // namespace

xq_SimulationPrepResult xq_SimulationPrepPipelineService::CreateOrUpdateSimulationPrep(
    mitk::DataStorage* dataStorage,
    const mitk::DataNode::Pointer& modelNode,
    const mitk::DataNode::Pointer& meshNode,
    const xq_SimulationPrepRequest& request)
{
    xq_SimulationPrepResult result;

    if (!dataStorage)
    {
        result.diagnostics.push_back(makeError("DataStorage is null."));
        return result;
    }

    if (modelNode.IsNull() || meshNode.IsNull() ||
        !xq::pipeline::HasStage(modelNode, xq::pipeline::Stage::Model) ||
        !xq::pipeline::HasStage(meshNode,  xq::pipeline::Stage::VolumeMesh))
    {
        result.diagnostics.push_back(makeError(
            "Simulation prep requires an upstream Model node (stage=model) "
            "and VolumeMesh node (stage=volume_mesh)."));
        return result;
    }

    auto* model = dynamic_cast<xq_Model*>(modelNode->GetData());
    auto* geometry = model ? model->GetModelElement(0) : nullptr;
    if (!geometry)
    {
        result.diagnostics.push_back(makeError("Model node does not expose a model element."));
        return result;
    }

    // Resolve the upstream Path (via Model's xq.source.path) so we can
    // determine inflow vs outflow geometrically instead of by cap order.
    xq_VesselCenterline* centerline = nullptr;
    const auto pathCsv = xq::pipeline::GetStringProperty(
        modelNode.GetPointer(), xq::pipeline::kSourcePathProperty);
    const auto pathNames = xq::pipeline::SplitSourceList(pathCsv);
    if (!pathNames.empty())
    {
        auto pathNode = xq::pipeline::FindNodeByNameAndStage(
            dataStorage, pathNames.front(), xq::pipeline::Stage::Path);
        if (pathNode.IsNotNull())
            centerline = dynamic_cast<xq_VesselCenterline*>(pathNode->GetData());
    }
    if (!centerline)
        result.diagnostics.push_back(makeWarning(
            "No upstream Path resolved; falling back to legacy cap-order role assignment."));

    auto solverJobData = xq_MitkSolverJob::New();
    auto solverJob = std::make_unique<xq_SolverJob>();
    solverJob->SetJobName(request.jobName.empty() ? meshNode->GetName() + "_sim" : request.jobName);
    solverJob->SetNumTimesteps(request.numTimesteps);
    solverJob->SetTimeStepSize(request.timeStepSize);
    solverJob->SetNumCycles(request.numCycles);
    solverJob->SetDeformable(request.deformableWall);

    solverJob->SetFluidDensity(request.fluidDensity);
    solverJob->SetFluidViscosity(request.fluidViscosity);
    solverJob->SetInitialPressure(request.initialPressure);
    solverJob->SetInitialVelocity(request.initialVelocity);

    // Wall properties
    solverJob->SetWallThickness(request.wallThickness);
    solverJob->SetWallElasticModulus(request.wallElasticModulus);
    solverJob->SetWallDensity(request.wallDensity);
    solverJob->SetWallPoissonRatio(request.wallPoissonRatio);

    // Solver properties
    solverJob->SetSolverType(request.solverType);
    solverJob->SetNumLinearIterations(request.numLinearIterations);
    solverJob->SetNumNonlinearIterations(request.numNonlinearIterations);

    const auto faceRoles = deriveFaceRoles(geometry, centerline, request.faceRoleOverrides);
    for (const auto& [faceName, role] : faceRoles)
        solverJob->SetCapProp(faceName, "role", role);

    // Build boundary condition list from face roles
    if (!request.boundaryConditions.empty())
    {
        // Use user-configured BCs from the UI table
        for (const auto& bc : request.boundaryConditions)
            solverJob->AddBoundaryCondition(bc);
        for (const auto& bc : request.boundaryConditions)
            solverJob->SetCapProp(bc.faceName, "role", bc.faceRole);
    }
    else
    {
        // Auto face-role inference from centerline / face geometry
        const auto& faceInfos = geometry->GetAllFaceInfos();
        for (const auto& fi : faceInfos)
        {
            auto it = faceRoles.find(fi.name);
            if (it == faceRoles.end())
                continue;
            xq_BoundaryCondition bc;
            bc.faceName = fi.name;
            bc.faceRole = it->second;
            if (bc.faceRole == "wall")
                bc.bcType = "no_slip";
            else if (bc.faceRole == "inflow")
                bc.bcType = "prescribed_velocity";
            else if (bc.faceRole == "outflow")
                bc.bcType = "resistance";
            solverJob->AddBoundaryCondition(bc);
        }
    }

    solverJobData->SetSimJob(std::move(solverJob));
    solverJobData->SetMeshName(meshNode->GetName());
    solverJobData->SetModelName(modelNode->GetName());
    solverJobData->SetStatus("configured");

    auto solverNode = mitk::DataNode::New();
    solverNode->SetData(solverJobData);
    solverNode->SetName(request.jobName.empty() ? meshNode->GetName() + "_simprep" : request.jobName);
    solverNode->SetIntProperty(
        xq::pipeline::kFaceRoleCountProperty,
        static_cast<int>(faceRoles.size()));
    xq::pipeline::MarkNode(solverNode, xq::pipeline::Stage::SimulationPrep);
    xq::pipeline::SetStringProperty(
        solverNode, xq::pipeline::kSourceModelProperty, modelNode->GetName());
    xq::pipeline::SetStringProperty(
        solverNode, xq::pipeline::kSourceMeshProperty, meshNode->GetName());

    // Persist simulation parameters as node properties for downstream inspection
    auto* storedJob = solverJobData->GetSimJob(0);
    if (storedJob)
    {
        solverNode->SetStringProperty("xq.sim.status", "configured");
        solverNode->SetDoubleProperty("xq.sim.fluid_density", storedJob->GetFluidDensity());
        solverNode->SetDoubleProperty("xq.sim.fluid_viscosity", storedJob->GetFluidViscosity());
        solverNode->SetIntProperty("xq.sim.num_timesteps", storedJob->GetNumTimesteps());
        solverNode->SetDoubleProperty("xq.sim.time_step_size", storedJob->GetTimeStepSize());
        solverNode->SetIntProperty("xq.sim.bc_count", static_cast<int>(storedJob->GetBoundaryConditions().size()));
    }

    auto simFolder = xq::pipeline::FindCategoryFolder(
        dataStorage, xq::pipeline::Stage::SimulationPrep, meshNode.GetPointer());
    if (simFolder.IsNotNull())
        dataStorage->Add(solverNode, simFolder);
    else
        dataStorage->Add(solverNode, meshNode);

    result.ok = true;
    result.node = solverNode;
    result.solverJobData = solverJobData;
    return result;
}

xq_SimulationExportResult xq_SimulationPrepPipelineService::ExportForSolver(
    mitk::DataStorage* dataStorage,
    const mitk::DataNode::Pointer& simPrepNode,
    const xq_SimulationExportRequest& request)
{
    xq_SimulationExportResult result;

    if (!dataStorage || simPrepNode.IsNull() ||
        !xq::pipeline::HasStage(simPrepNode, xq::pipeline::Stage::SimulationPrep))
    {
        result.diagnostics.push_back(makeError(
            "ExportForSolver requires a SimulationPrep node."));
        return result;
    }

    auto modelNode = xq::pipeline::ResolveUpstreamNode(
        dataStorage, simPrepNode.GetPointer(),
        xq::pipeline::kSourceModelProperty, xq::pipeline::Stage::Model);
    auto meshNode  = xq::pipeline::ResolveUpstreamNode(
        dataStorage, simPrepNode.GetPointer(),
        xq::pipeline::kSourceMeshProperty, xq::pipeline::Stage::VolumeMesh);

    if (modelNode.IsNull() || meshNode.IsNull())
    {
        result.diagnostics.push_back(makeError(
            "ExportForSolver: upstream Model or VolumeMesh is missing from DataStorage."));
        return result;
    }

    auto* mitkJob  = dynamic_cast<xq_MitkSolverJob*>(simPrepNode->GetData());
    auto* jobPtr   = mitkJob ? mitkJob->GetSimJob(0) : nullptr;
    auto* model    = dynamic_cast<xq_Model*>(modelNode->GetData());
    auto* geometry = model ? model->GetModelElement(0) : nullptr;
    auto* mitkGrid = dynamic_cast<xq_MitkGrid*>(meshNode->GetData());
    auto* gridPtr  = mitkGrid ? mitkGrid->GetMesh(0) : nullptr;

    if (!jobPtr || !geometry || !gridPtr)
    {
        result.diagnostics.push_back(makeError(
            "ExportForSolver: solver job / model geometry / mesh grid unavailable."));
        return result;
    }

    xq_SvPreExportRequest exportReq;
    exportReq.outputDir     = request.outputDir;
    exportReq.jobName       = simPrepNode->GetName();
    exportReq.solverJob     = jobPtr;
    exportReq.geometry      = geometry;
    exportReq.grid          = gridPtr;
    exportReq.inletWaveform = request.inletWaveform;

    const auto er = xq_SvPreWriter::Export(exportReq);
    if (!er.ok)
    {
        result.diagnostics.push_back(makeError(
            er.diagnostic.empty() ? "SvPre writer failed." : er.diagnostic));
        return result;
    }

    // Collect diagnostics from the writer (warnings about placeholders, etc.)
    for (const auto& d : er.diagnostics)
        result.diagnostics.push_back(d);

    for (const auto& p : er.filesWritten)
        result.filesWritten.push_back(p.string());

    // Mark the simulation-prep node as exported and record the export directory
    simPrepNode->SetStringProperty("xq.sim.status", "exported");
    simPrepNode->SetStringProperty("xq.sim.export_dir", request.outputDir.c_str());

    result.ok = true;
    return result;
}
