#include "xq_SimulationPrepPipeline.h"

#include "xq_MitkSolverJob.h"
#include "xq_FlowSolverRegistry.h"
#include "xq_ResultImport.h"
#include "xq_SolverJob.h"

#include <xq_Model.h>
#include <xq_VascularGeometry.h>
#include <xq_VesselCenterline.h>
#include <xq_CenterlineSegment.h>
#include <xq_MitkGrid.h>
#include <xq_Grid.h>

#include <vtkCenterOfMass.h>
#include <vtkPolyData.h>

#include <QDateTime>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <utility>

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

std::string SerializeFaceRoles(const std::map<std::string, std::string>& faceRoles)
{
    std::ostringstream os;
    bool first = true;
    for (const auto& [faceName, role] : faceRoles)
    {
        if (!first)
            os << '\n';
        first = false;
        os << faceName << '=' << role;
    }
    return os.str();
}

std::string BuildSolverCommandLine(
    const std::string& backendId,
    const std::string& caseDir,
    int numProcessors)
{
    std::ostringstream os;
    os << backendId << " --case_dir " << caseDir
       << " --num_processors " << std::max(1, numProcessors);
    return os.str();
}

std::string SerializeBoundaryConditions(const std::vector<xq_BoundaryCondition>& bcs)
{
    std::ostringstream os;
    for (size_t i = 0; i < bcs.size(); ++i)
    {
        const auto& bc = bcs[i];
        if (i > 0)
            os << '\n';
        os << bc.faceName << '|' << bc.faceRole << '|' << bc.bcType;
        if (!bc.parameters.empty())
        {
            os << "|params:";
            bool first = true;
            for (const auto& [key, value] : bc.parameters)
            {
                if (!first)
                    os << ';';
                first = false;
                os << key << '=' << value;
            }
        }
        if (!bc.waveform.empty())
        {
            os << "|waveform:";
            for (size_t j = 0; j < bc.waveform.size(); ++j)
            {
                if (j > 0)
                    os << ';';
                os << bc.waveform[j].first << ',' << bc.waveform[j].second;
            }
        }
    }
    return os.str();
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

std::string ValidateBoundaryCondition(const xq_BoundaryCondition& bc)
{
    if (bc.faceName.empty())
        return "Boundary condition has an empty face name.";
    if (bc.faceRole != "wall" && bc.faceRole != "inflow" && bc.faceRole != "outflow")
        return "Boundary condition for face '" + bc.faceName +
               "' has unsupported role '" + bc.faceRole + "'.";
    if (bc.bcType.empty())
        return "Boundary condition for face '" + bc.faceName + "' has an empty type.";

    const bool knownType =
        bc.bcType == "no_slip" ||
        bc.bcType == "prescribed_velocity" ||
        bc.bcType == "resistance" ||
        bc.bcType == "rcr" ||
        bc.bcType == "pressure" ||
        bc.bcType == "impedance" ||
        bc.bcType == "coronary";
    if (!knownType)
        return "Boundary condition for face '" + bc.faceName +
               "' has unsupported type '" + bc.bcType + "'.";

    const bool compatible =
        (bc.faceRole == "wall" && bc.bcType == "no_slip") ||
        (bc.faceRole == "inflow" && bc.bcType == "prescribed_velocity") ||
        (bc.faceRole == "outflow" &&
         (bc.bcType == "resistance" || bc.bcType == "rcr" ||
          bc.bcType == "pressure" || bc.bcType == "impedance" ||
          bc.bcType == "coronary"));
    if (!compatible)
        return "Boundary condition for face '" + bc.faceName + "' has role '" +
               bc.faceRole + "' incompatible with type '" + bc.bcType + "'.";

    if (bc.bcType == "prescribed_velocity")
    {
        if (bc.waveform.empty() &&
            bc.parameters.find("value") == bc.parameters.end() &&
            bc.parameters.find("flowRate") == bc.parameters.end())
            return "Boundary condition for face '" + bc.faceName +
                   "' requires either a waveform or a numeric inflow value.";
    }
    else if (bc.bcType == "resistance" || bc.bcType == "pressure")
    {
        if (bc.parameters.find("value") == bc.parameters.end() &&
            bc.parameters.find("resistance") == bc.parameters.end() &&
            bc.parameters.find("pressure") == bc.parameters.end())
            return "Boundary condition for face '" + bc.faceName +
                   "' requires a numeric value.";
    }
    else if (bc.bcType == "rcr")
    {
        if (bc.parameters.find("Rp") == bc.parameters.end() ||
            bc.parameters.find("C") == bc.parameters.end() ||
            bc.parameters.find("Rd") == bc.parameters.end() ||
            (bc.parameters.find("pressure") == bc.parameters.end() &&
             bc.parameters.find("Pressure") == bc.parameters.end()))
        {
            return "Boundary condition for face '" + bc.faceName +
                   "' uses RCR but is missing Rp, C, Rd, or pressure.";
        }
    }

    return {};
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
            else if (bestInflowFaceId >= 0)
                role = "outflow";
        }

        const auto overrideIt = overrides.find(fi.name);
        if (overrideIt != overrides.end())
            role = overrideIt->second;

        faceRoles[fi.name] = role;
    }

    return faceRoles;
}

struct BackendContext
{
    xq_MitkSolverJob* mitkJob = nullptr;
    xq_SolverJob* job = nullptr;
    xq_FlowSolverBackend* backend = nullptr;
    xq_FlowSolverInput input;
};

bool BuildBackendContext(
    mitk::DataStorage* dataStorage,
    const mitk::DataNode::Pointer& simPrepNode,
    const std::string& caseDir,
    const std::vector<std::pair<double, double>>& inletWaveform,
    BackendContext& context,
    std::vector<xq::pipeline::Diagnostic>& diagnostics)
{
    if (!dataStorage || simPrepNode.IsNull() ||
        !xq::pipeline::HasStage(simPrepNode, xq::pipeline::Stage::SimulationPrep))
    {
        diagnostics.push_back(makeError(
            "Solver backend requires a SimulationPrep node."));
        return false;
    }

    auto modelNode = xq::pipeline::ResolveUpstreamNode(
        dataStorage, simPrepNode.GetPointer(),
        xq::pipeline::kSourceModelProperty, xq::pipeline::Stage::Model);
    auto meshNode  = xq::pipeline::ResolveUpstreamNode(
        dataStorage, simPrepNode.GetPointer(),
        xq::pipeline::kSourceMeshProperty, xq::pipeline::Stage::VolumeMesh);

    if (modelNode.IsNull() || meshNode.IsNull())
    {
        diagnostics.push_back(makeError(
            "Solver backend: upstream Model or VolumeMesh is missing from DataStorage."));
        return false;
    }

    context.mitkJob = dynamic_cast<xq_MitkSolverJob*>(simPrepNode->GetData());
    context.job = context.mitkJob ? context.mitkJob->GetSimJob(0) : nullptr;
    auto* model = dynamic_cast<xq_Model*>(modelNode->GetData());
    auto* geometry = model ? model->GetModelElement(0) : nullptr;
    auto* mitkGrid = dynamic_cast<xq_MitkGrid*>(meshNode->GetData());
    auto* gridPtr = mitkGrid ? mitkGrid->GetMesh(0) : nullptr;

    if (!context.job || !geometry || !gridPtr)
    {
        diagnostics.push_back(makeError(
            "Solver backend: solver job / model geometry / mesh grid unavailable."));
        return false;
    }

    context.input.solverJob = context.job;
    context.input.geometry = geometry;
    context.input.grid = gridPtr;
    context.input.jobName = simPrepNode->GetName();
    context.input.caseDir = caseDir;
    context.input.inletWaveform = inletWaveform;
    for (const auto& [capName, props] : context.job->GetCapProps())
    {
        const auto roleIt = props.find("role");
        if (roleIt != props.end())
            context.input.faceRoles[capName] = roleIt->second;
    }
    if (context.input.inletWaveform.empty())
    {
        for (const auto& bc : context.job->GetBoundaryConditions())
        {
            if (bc.faceRole != "inflow" && bc.bcType != "prescribed_velocity")
                continue;

            if (!context.input.inletWaveform.empty())
            {
                diagnostics.push_back(makeError(
                    "Solver backend supports only one inflow boundary condition unless the selected backend declares otherwise."));
                return false;
            }

            if (!bc.waveform.empty())
            {
                context.input.inletWaveform = bc.waveform;
                continue;
            }

            const auto valueIt = bc.parameters.find("value");
            const auto flowIt = bc.parameters.find("flowRate");
            const auto numericIt =
                valueIt != bc.parameters.end() ? valueIt : flowIt;
            if (numericIt == bc.parameters.end())
            {
                diagnostics.push_back(makeError(
                    "Inflow boundary condition for face '" + bc.faceName +
                    "' is missing a waveform, value, or flowRate."));
                return false;
            }

            try
            {
                const double flow = std::stod(numericIt->second);
                context.input.inletWaveform = {{0.0, flow}, {1.0, flow}};
            }
            catch (...)
            {
                diagnostics.push_back(makeError(
                    "Inflow boundary condition for face '" + bc.faceName +
                    "' has a non-numeric flow value."));
                return false;
            }
        }
    }
    if (context.input.inletWaveform.empty())
    {
        diagnostics.push_back(makeError(
            "Solver backend requires an inflow waveform or a constant inflow value."));
        return false;
    }

    context.backend = xq_FlowSolverRegistry::Instance().FindBackend(context.job->GetSolverType());
    if (!context.backend)
    {
        diagnostics.push_back(makeError(
            "No solver backend is registered for solver type '" +
            context.job->GetSolverType() + "'."));
        return false;
    }

    return true;
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
    bool modelQaOk = true;
    if (modelNode->GetBoolProperty("xq.model.qa.ok", modelQaOk) && !modelQaOk)
    {
        result.diagnostics.push_back(makeError(
            "Simulation prep blocked: upstream model QA failed."));
        return result;
    }
    bool meshQaOk = true;
    if (meshNode->GetBoolProperty("xq.mesh.qa.ok", meshQaOk) && !meshQaOk)
    {
        result.diagnostics.push_back(makeError(
            "Simulation prep blocked: upstream mesh QA failed."));
        return result;
    }

    // Resolve the upstream Path (via Model's xq.source.path) so we can
    // determine inflow vs outflow geometrically instead of by cap order.
    xq_VesselCenterline* centerline = nullptr;
    const auto pathCsv = xq::pipeline::GetStringProperty(
        modelNode.GetPointer(), xq::pipeline::kSourcePathProperty);
    const auto pathNames = xq::pipeline::SplitSourceList(pathCsv);
    if (pathNames.size() == 1)
    {
        auto pathNode = xq::pipeline::FindNodeByNameAndStage(
            dataStorage, pathNames[0], xq::pipeline::Stage::Path);
        if (pathNode.IsNotNull())
            centerline = dynamic_cast<xq_VesselCenterline*>(pathNode->GetData());
    }
    if (pathNames.size() > 1)
        result.diagnostics.push_back(makeWarning(
            "Multiple upstream Paths are recorded on the model; cap roles were "
            "not inferred from an arbitrary first path."));
    else if (!centerline)
        result.diagnostics.push_back(makeWarning(
            "No upstream Path resolved; explicit face-role overrides are required "
            "for cap faces."));

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
    {
        if (role == "cap")
        {
            result.diagnostics.push_back(makeError(
                "Unable to determine a simulation role for face '" + faceName +
                "'. Provide an upstream Path or explicit face-role overrides."));
            return result;
        }
    }
    for (const auto& bc : request.boundaryConditions)
    {
        const auto validation = ValidateBoundaryCondition(bc);
        if (!validation.empty())
        {
            result.diagnostics.push_back(makeError(validation));
            return result;
        }
        if (faceRoles.find(bc.faceName) == faceRoles.end())
        {
            result.diagnostics.push_back(makeError(
                "Boundary condition references unknown face '" + bc.faceName + "'."));
            return result;
        }
    }
    for (const auto& [faceName, role] : faceRoles)
        solverJob->SetCapProp(faceName, "role", role);

    // Build boundary condition list from face roles
    if (!request.boundaryConditions.empty())
    {
        // Use user-configured BCs from the UI table
        for (const auto& bc : request.boundaryConditions)
        {
            const auto roleIt = faceRoles.find(bc.faceName);
            if (roleIt != faceRoles.end() && roleIt->second != bc.faceRole)
            {
                result.diagnostics.push_back(makeError(
                    "Boundary condition for face '" + bc.faceName +
                    "' has role '" + bc.faceRole +
                    "' but the resolved simulation role is '" +
                    roleIt->second + "'."));
                return result;
            }
            solverJob->AddBoundaryCondition(bc);
        }
        for (const auto& bc : request.boundaryConditions)
            solverJob->SetCapProp(bc.faceName, "role", bc.faceRole);

        for (const auto& [faceName, role] : faceRoles)
        {
            if (role != "inflow" && role != "outflow")
                continue;

            const auto hasBc = std::find_if(
                request.boundaryConditions.begin(), request.boundaryConditions.end(),
                [&faceName](const xq_BoundaryCondition& bc) {
                    return bc.faceName == faceName;
                }) != request.boundaryConditions.end();
            if (!hasBc)
            {
                result.diagnostics.push_back(makeError(
                    "Missing boundary condition for face '" + faceName +
                    "' with simulation role '" + role + "'."));
                return result;
            }
        }
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
            {
                result.diagnostics.push_back(makeError(
                    "Inflow face '" + fi.name +
                    "' requires an explicit prescribed_velocity boundary condition."));
                return result;
            }
            else if (bc.faceRole == "outflow")
            {
                result.diagnostics.push_back(makeError(
                    "Outflow face '" + fi.name +
                    "' requires an explicit resistance, RCR, pressure, or coronary boundary condition."));
                return result;
            }
            solverJob->AddBoundaryCondition(bc);
        }
    }

    const auto jobValidation = solverJob->Validate();
    if (!jobValidation.empty())
    {
        result.diagnostics.push_back(makeError(jobValidation));
        return result;
    }

    solverJobData->SetSimJob(std::move(solverJob));
    solverJobData->SetMeshName(meshNode->GetName());
    solverJobData->SetModelName(modelNode->GetName());
    solverJobData->SetStatus("configured");

    const auto* backend = xq_FlowSolverRegistry::Instance().FindBackend(request.solverType);
    const std::string backendId = backend ? backend->GetBackendId() : request.solverType;
    const std::string backendName = backend ? backend->GetDisplayName() : request.solverType;
    const std::string backendVersion = backend ? backend->GetVersion() : "unknown";
    const std::string faceRoleSummary = SerializeFaceRoles(faceRoles);
    const std::string bcSummary = SerializeBoundaryConditions(
        solverJobData->GetSimJob(0) ? solverJobData->GetSimJob(0)->GetBoundaryConditions()
                                    : std::vector<xq_BoundaryCondition>{});

    auto solverNode = mitk::DataNode::New();
    solverNode->SetData(solverJobData);
    solverNode->SetName(request.jobName.empty() ? meshNode->GetName() + "_simprep" : request.jobName);
    solverNode->SetIntProperty(
        xq::pipeline::kFaceRoleCountProperty,
        static_cast<int>(faceRoles.size()));
    xq::pipeline::MarkGeneratedNode(
        solverNode,
        xq::pipeline::Stage::SimulationPrep,
        "simulation_prep",
        "org.xq.imaging.flowanalysis");
    xq::pipeline::SetStringProperty(
        solverNode, xq::pipeline::kSourceModelProperty, modelNode->GetName());
    xq::pipeline::SetStringProperty(
        solverNode, xq::pipeline::kSourceMeshProperty, meshNode->GetName());
    xq::pipeline::SetStringProperty(solverNode, "xq.solver.backend_id", backendId);
    xq::pipeline::SetStringProperty(solverNode, "xq.solver.backend_name", backendName);
    xq::pipeline::SetStringProperty(solverNode, "xq.solver.backend_version", backendVersion);
    xq::pipeline::SetStringProperty(solverNode, "xq.solver.case_format_version", "1");
    xq::pipeline::SetStringProperty(solverNode, "xq.solver.status", "configured");
    xq::pipeline::SetStringProperty(solverNode, "xq.units.length", "mm");
    xq::pipeline::SetStringProperty(solverNode, "xq.units.time", "s");
    xq::pipeline::SetStringProperty(solverNode, "xq.units.density", "g/cm^3");
    xq::pipeline::SetStringProperty(solverNode, "xq.units.viscosity", "poise");
    xq::pipeline::SetStringProperty(solverNode, "xq.units.pressure", "dyn/cm^2");
    xq::pipeline::SetStringProperty(
        solverNode, "xq.params.simulation.face_roles", faceRoleSummary);
    xq::pipeline::SetStringProperty(
        solverNode, "xq.params.simulation.boundary_conditions", bcSummary);
    xq::pipeline::SetStringProperty(solverNode, "xq.sim.bc_table", bcSummary);
    xq::pipeline::SetStringProperty(
        solverNode,
        xq::pipeline::kLimitationsProperty,
        backend
            ? "Simulation prep is configured for a registered native backend but still requires explicit Run/Export."
            : "Simulation prep is export-ready only because the requested solver backend is not registered.");

    // Persist simulation parameters as node properties for downstream inspection
    auto* storedJob = solverJobData->GetSimJob(0);
    if (storedJob)
    {
        solverNode->SetStringProperty("xq.sim.status", "configured");
        solverNode->SetStringProperty("xq.sim.solver_type", storedJob->GetSolverType().c_str());
        solverNode->SetBoolProperty("xq.sim.deformable_wall", storedJob->GetDeformable());
        solverNode->SetDoubleProperty("xq.sim.fluid_density", storedJob->GetFluidDensity());
        solverNode->SetDoubleProperty("xq.sim.fluid_viscosity", storedJob->GetFluidViscosity());
        solverNode->SetDoubleProperty("xq.sim.initial_pressure", storedJob->GetInitialPressure());
        solverNode->SetDoubleProperty("xq.sim.initial_velocity", storedJob->GetInitialVelocity());
        solverNode->SetDoubleProperty("xq.sim.wall_thickness", storedJob->GetWallThickness());
        solverNode->SetDoubleProperty("xq.sim.wall_elastic_modulus", storedJob->GetWallElasticModulus());
        solverNode->SetDoubleProperty("xq.sim.wall_poisson_ratio", storedJob->GetWallPoissonRatio());
        solverNode->SetDoubleProperty("xq.sim.wall_density", storedJob->GetWallDensity());
        solverNode->SetIntProperty("xq.sim.num_timesteps", storedJob->GetNumTimesteps());
        solverNode->SetDoubleProperty("xq.sim.time_step_size", storedJob->GetTimeStepSize());
        solverNode->SetIntProperty("xq.sim.num_cycles", storedJob->GetNumCycles());
        solverNode->SetIntProperty("xq.sim.num_linear_iterations", storedJob->GetNumLinearIterations());
        solverNode->SetIntProperty("xq.sim.num_nonlinear_iterations", storedJob->GetNumNonlinearIterations());
        solverNode->SetIntProperty("xq.sim.bc_count", static_cast<int>(storedJob->GetBoundaryConditions().size()));
        solverNode->SetIntProperty(
            "xq.sim.face_role_count", static_cast<int>(faceRoles.size()));
        solverNode->SetStringProperty("xq.params.simulation.wall_model",
            storedJob->GetDeformable() ? "deformable" : "rigid");
        solverNode->SetStringProperty("xq.params.simulation.export_format", "xq_flow_case_v1");
        solverNode->SetDoubleProperty("xq.params.simulation.fluid_density", storedJob->GetFluidDensity());
        solverNode->SetDoubleProperty("xq.params.simulation.fluid_viscosity", storedJob->GetFluidViscosity());
        solverNode->SetDoubleProperty("xq.params.simulation.time_step", storedJob->GetTimeStepSize());
        solverNode->SetIntProperty("xq.params.simulation.num_steps", storedJob->GetNumTimesteps());
        solverNode->SetIntProperty("xq.params.simulation.num_cycles", storedJob->GetNumCycles());
        solverNode->SetIntProperty(
            "xq.params.simulation.boundary_condition_count",
            static_cast<int>(storedJob->GetBoundaryConditions().size()));
        solverNode->SetIntProperty(
            "xq.params.simulation.face_role_count",
            static_cast<int>(faceRoles.size()));
        solverNode->SetStringProperty(
            "xq.solver.diagnostic",
            storedJob->Validate().empty()
                ? "Simulation prep validated successfully."
                : storedJob->Validate().c_str());
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

    BackendContext context;
    if (!BuildBackendContext(
            dataStorage, simPrepNode, request.outputDir, request.inletWaveform,
            context, result.diagnostics))
        return result;

    const auto validation = context.backend->Validate(context.input);
    if (!validation.ok)
    {
        if (!validation.diagnostic.empty())
            result.diagnostics.push_back(makeError(validation.diagnostic));
        result.diagnostics.insert(
            result.diagnostics.end(),
            validation.diagnostics.begin(),
            validation.diagnostics.end());
        return result;
    }

    const auto prep = context.backend->PrepareCase(context.input, request.outputDir);
    if (!prep.ok)
    {
        if (!prep.diagnostic.empty())
            result.diagnostics.push_back(makeError(prep.diagnostic));
        result.diagnostics.insert(
            result.diagnostics.end(),
            prep.diagnostics.begin(),
            prep.diagnostics.end());
        return result;
    }

    // Collect diagnostics from the backend.
    for (const auto& d : prep.diagnostics)
        result.diagnostics.push_back(d);

    for (const auto& p : prep.filesWritten)
        result.filesWritten.push_back(p.string());

    const bool hasWarnings = !result.diagnostics.empty();
    const char* exportStatus = hasWarnings ? "exported_with_warnings" : "exported";

    // Mark the simulation-prep node as exported and record the export directory.
    // Warnings are persisted so a later project reload does not hide export
    // limitations from the user.
    if (context.mitkJob)
        context.mitkJob->SetStatus(exportStatus);
    simPrepNode->SetStringProperty("xq.sim.status", exportStatus);
    simPrepNode->SetStringProperty("xq.sim.export_dir", request.outputDir.c_str());
    simPrepNode->SetIntProperty(
        "xq.sim.files_written_count", static_cast<int>(result.filesWritten.size()));
    std::ostringstream files;
    for (size_t i = 0; i < result.filesWritten.size(); ++i)
    {
        if (i > 0)
            files << '\n';
        files << result.filesWritten[i];
    }
    simPrepNode->SetStringProperty("xq.sim.files_written", files.str().c_str());
    if (hasWarnings)
    {
        std::ostringstream warnings;
        for (size_t i = 0; i < result.diagnostics.size(); ++i)
        {
            if (i > 0)
                warnings << '\n';
            warnings << result.diagnostics[i].message;
        }
        simPrepNode->SetStringProperty("xq.sim.export_warnings", warnings.str().c_str());
    }
    xq::pipeline::SetStringProperty(
        simPrepNode, "xq.solver.backend_id", context.backend->GetBackendId());
    xq::pipeline::SetStringProperty(
        simPrepNode, "xq.solver.backend_name", context.backend->GetDisplayName());
    xq::pipeline::SetStringProperty(
        simPrepNode, "xq.solver.backend_version", context.backend->GetVersion());
    xq::pipeline::SetStringProperty(
        simPrepNode, "xq.solver.command_line",
        BuildSolverCommandLine(context.backend->GetBackendId(), request.outputDir, 1));
    xq::pipeline::SetStringProperty(
        simPrepNode, "xq.solver.case_dir", request.outputDir);
    simPrepNode->SetStringProperty("xq.solver.status", exportStatus);
    simPrepNode->SetStringProperty("xq.solver.result_import_status", "pending");

    result.ok = true;
    return result;
}

xq_SimulationRunResult xq_SimulationPrepPipelineService::RunSolverAndImportResults(
    mitk::DataStorage* dataStorage,
    const mitk::DataNode::Pointer& simPrepNode,
    const xq_SimulationRunRequest& request)
{
    xq_SimulationRunResult result;

    BackendContext context;
    if (!BuildBackendContext(
            dataStorage, simPrepNode, request.caseDir, request.inletWaveform,
            context, result.diagnostics))
        return result;

    const auto validation = context.backend->Validate(context.input);
    if (!validation.ok)
    {
        if (!validation.diagnostic.empty())
            result.diagnostics.push_back(makeError(validation.diagnostic));
        result.diagnostics.insert(
            result.diagnostics.end(),
            validation.diagnostics.begin(),
            validation.diagnostics.end());
        return result;
    }

    const auto prep = context.backend->PrepareCase(context.input, request.caseDir);
    if (!prep.ok)
    {
        if (!prep.diagnostic.empty())
            result.diagnostics.push_back(makeError(prep.diagnostic));
        result.diagnostics.insert(
            result.diagnostics.end(),
            prep.diagnostics.begin(),
            prep.diagnostics.end());
        return result;
    }
    for (const auto& d : prep.diagnostics)
        result.diagnostics.push_back(d);
    for (const auto& p : prep.filesWritten)
        result.filesWritten.push_back(p.string());

    xq_FlowSolverRunOptions runOptions;
    runOptions.numProcessors = request.numProcessors;
    runOptions.mpiPath = request.mpiPath;

    const auto startedAt =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
    xq::pipeline::SetStringProperty(simPrepNode, "xq.solver.started_at", startedAt);
    xq::pipeline::SetStringProperty(simPrepNode, "xq.solver.case_dir", request.caseDir);
    xq::pipeline::SetStringProperty(
        simPrepNode, "xq.solver.backend_id", context.backend->GetBackendId());
    xq::pipeline::SetStringProperty(
        simPrepNode, "xq.solver.backend_name", context.backend->GetDisplayName());
    xq::pipeline::SetStringProperty(
        simPrepNode, "xq.solver.backend_version", context.backend->GetVersion());
    xq::pipeline::SetStringProperty(
        simPrepNode, "xq.solver.command_line",
        BuildSolverCommandLine(
            context.backend->GetBackendId(), request.caseDir, request.numProcessors));
    simPrepNode->SetIntProperty("xq.solver.num_processors", request.numProcessors);
    simPrepNode->SetStringProperty("xq.solver.status", "running");
    simPrepNode->SetStringProperty("xq.sim.status", "running");
    if (context.mitkJob)
        context.mitkJob->SetStatus("running");

    const auto run = context.backend->RunCase(request.caseDir, runOptions);
    result.exitCode = run.exitCode;
    for (const auto& d : run.diagnostics)
        result.diagnostics.push_back(d);

    const auto finishedAt =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs).toStdString();
    xq::pipeline::SetStringProperty(simPrepNode, "xq.solver.finished_at", finishedAt);
    simPrepNode->SetIntProperty("xq.solver.exit_code", run.exitCode);
    xq::pipeline::SetStringProperty(simPrepNode, "xq.solver.diagnostic", run.diagnostic);

    if (!run.ok)
    {
        if (!run.diagnostic.empty())
            result.diagnostics.push_back(makeError(run.diagnostic));
        simPrepNode->SetStringProperty("xq.solver.status", "failed");
        simPrepNode->SetStringProperty("xq.sim.status", "failed");
        simPrepNode->SetStringProperty("xq.solver.result_import_status", "not_started");
        if (context.mitkJob)
            context.mitkJob->SetStatus("failed");
        return result;
    }

    const auto import = context.backend->ImportResults(request.caseDir);
    for (const auto& d : import.diagnostics)
        result.diagnostics.push_back(d);
    if (!import.ok)
    {
        if (!import.diagnostic.empty())
            result.diagnostics.push_back(makeError(import.diagnostic));
        simPrepNode->SetStringProperty("xq.solver.status", "results_missing");
        simPrepNode->SetStringProperty("xq.sim.status", "results_missing");
        simPrepNode->SetStringProperty("xq.solver.result_import_status", "failed");
        if (context.mitkJob)
            context.mitkJob->SetStatus("results_missing");
        return result;
    }

    for (size_t i = 0; i < import.resultFiles.size(); ++i)
    {
        xq_ResultImportEntry entry;
        entry.filePath = import.resultFiles[i];
        entry.simulationName = simPrepNode->GetName();
        entry.nodeName = simPrepNode->GetName() + "_result_" + std::to_string(i + 1);
        auto outcome = xq_ResultImport::Import(dataStorage, entry);
        if (!outcome.ok || outcome.node.IsNull())
        {
            for (const auto& diagnostic : outcome.diagnostics)
                result.diagnostics.push_back(makeError(diagnostic));
            simPrepNode->SetStringProperty("xq.solver.status", "result_import_failed");
            simPrepNode->SetStringProperty("xq.sim.status", "result_import_failed");
            simPrepNode->SetStringProperty("xq.solver.result_import_status", "failed");
            if (context.mitkJob)
                context.mitkJob->SetStatus("result_import_failed");
            return result;
        }

        xq::pipeline::SetStringProperty(
            outcome.node, "xq.result.backend_id", context.backend->GetBackendId());
        xq::pipeline::SetStringProperty(
            outcome.node, "xq.result.backend_name", context.backend->GetDisplayName());
        xq::pipeline::SetStringProperty(
            outcome.node, "xq.result.backend_version", context.backend->GetVersion());
        xq::pipeline::SetStringProperty(
            outcome.node, xq::pipeline::kSourceSolverCaseProperty, request.caseDir);
        xq::pipeline::SetStringProperty(
            outcome.node, xq::pipeline::kSourceSimulationJobProperty, simPrepNode->GetName());
        xq::pipeline::SetStringProperty(outcome.node, "xq.result.units.pressure", "dyn/cm^2");
        xq::pipeline::SetStringProperty(outcome.node, "xq.result.units.velocity", "cm/s");
        xq::pipeline::SetStringProperty(outcome.node, "xq.result.units.wall_shear", "dyn/cm^2");
        outcome.node->SetIntProperty("xq.result.time_step", static_cast<int>(i));
        outcome.node->SetIntProperty("xq.result.time_step_index", static_cast<int>(i));
        outcome.node->SetIntProperty(
            "xq.result.time_step_count", static_cast<int>(import.resultFiles.size()));
        double timeValue = static_cast<double>(i);
        double startTime = 0.0;
        double timeStep = 1.0;
        if (simPrepNode->GetDoubleProperty("xq.sim.start_time", startTime) &&
            simPrepNode->GetDoubleProperty("xq.sim.time_step_size", timeStep) &&
            timeStep > 0.0)
        {
            timeValue = startTime + static_cast<double>(i) * timeStep;
        }
        outcome.node->SetDoubleProperty("xq.result.time_value", timeValue);
        result.resultNodes.push_back(outcome.node);
    }

    const bool hasWarnings = !result.diagnostics.empty();
    const char* status = hasWarnings ? "completed_with_warnings" : "completed";
    simPrepNode->SetStringProperty("xq.solver.status", status);
    simPrepNode->SetStringProperty("xq.sim.status", status);
    simPrepNode->SetStringProperty("xq.solver.result_import_status", "imported");
    simPrepNode->SetIntProperty(
        "xq.solver.result_file_count", static_cast<int>(import.resultFiles.size()));
    if (context.mitkJob)
        context.mitkJob->SetStatus(status);

    result.ok = true;
    return result;
}
