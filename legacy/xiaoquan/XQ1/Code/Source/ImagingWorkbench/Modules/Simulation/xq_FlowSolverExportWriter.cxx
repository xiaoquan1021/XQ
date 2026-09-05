#include "xq_FlowSolverExportWriter.h"

#include "xq_SolverJob.h"

#include <xq_Grid.h>
#include <xq_VascularGeometry.h>

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkXMLUnstructuredGridWriter.h>

#include <fstream>
#include <initializer_list>
#include <system_error>

namespace fs = std::filesystem;

namespace
{

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

} // namespace

xq_FlowSolverExportResult xq_FlowSolverExportWriter::Export(const xq_FlowSolverExportRequest& request)
{
    xq_FlowSolverExportResult result;

    if (request.jobName.empty())
    {
        result.diagnostic = "FlowSolverExportWriter: jobName is empty.";
        return result;
    }
    if (!request.solverJob || !request.geometry || !request.grid)
    {
        result.diagnostic = "FlowSolverExportWriter: solverJob / geometry / grid must all be non-null.";
        return result;
    }

    const fs::path root     = request.outputDir;
    const fs::path meshDir  = root / "mesh-complete";
    const fs::path facesDir = meshDir; // Legacy convention: faces live beside the mesh

    if (!EnsureDir(root, result.diagnostic)) return result;
    if (!EnsureDir(meshDir, result.diagnostic)) return result;

    // 1) Write the volume mesh as VTU.
    auto* volume = request.grid->GetVolumeMesh();
    if (!volume || volume->GetNumberOfCells() == 0)
    {
        result.diagnostic = "FlowSolverExportWriter: grid has no volume mesh.";
        return result;
    }
    const auto vtuPath = meshDir / "mesh-complete.mesh.vtu";
    auto vtuWriter = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
    vtuWriter->SetFileName(vtuPath.string().c_str());
    vtuWriter->SetInputData(volume);
    vtuWriter->SetDataModeToBinary();
    if (!vtuWriter->Write())
    {
        result.diagnostic = "FlowSolverExportWriter: failed to write " + vtuPath.string();
        return result;
    }
    result.filesWritten.push_back(vtuPath);

    // 2) Write one VTP per face.
    const auto& faces = request.geometry->GetAllFaceInfos();
    for (const auto& fi : faces)
    {
        auto facePoly = request.geometry->GetFaceVtkPolyData(fi.id);
        if (!facePoly || facePoly->GetNumberOfCells() == 0)
            continue;
        const auto vtpPath = facesDir / (fi.name + ".vtp");
        auto vtpWriter = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
        vtpWriter->SetFileName(vtpPath.string().c_str());
        vtpWriter->SetInputData(facePoly);
        vtpWriter->SetDataModeToBinary();
        if (vtpWriter->Write())
            result.filesWritten.push_back(vtpPath);
    }

    // 3) Resolve the face role map (argument overrides solver job).
    std::map<std::string, std::string> roles = request.faceRoles;
    if (roles.empty())
    {
        for (const auto& [capName, props] : request.solverJob->GetCapProps())
        {
            const auto it = props.find("role");
            if (it != props.end())
                roles[capName] = it->second;
        }
    }

    // 4) Resolve boundary conditions by face.
    std::map<std::string, const xq_BoundaryCondition*> bcsByFace;
    for (const auto& bc : request.solverJob->GetBoundaryConditions())
    {
        if (bc.faceName.empty())
        {
            result.diagnostic = "FlowSolverExportWriter: boundary condition has an empty face name.";
            return result;
        }
        if (bcsByFace.find(bc.faceName) != bcsByFace.end())
        {
            result.diagnostic = "FlowSolverExportWriter: duplicate boundary condition for face '" +
                                bc.faceName + "'.";
            return result;
        }
        bcsByFace[bc.faceName] = &bc;
    }

    auto parseDoubleParam = [](const xq_BoundaryCondition& bc,
                               std::initializer_list<const char*> keys,
                               double& value) -> bool
    {
        for (const char* key : keys)
        {
            const auto it = bc.parameters.find(key);
            if (it == bc.parameters.end())
                continue;
            try
            {
                value = std::stod(it->second);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
        return false;
    };

    bool hasInflow = false;
    bool hasResistance = false;
    bool hasRcr = false;
    bool hasPressure = false;
    std::vector<std::pair<std::string, double>> resistanceEntries;
    struct RcrEntry
    {
        std::string faceName;
        double rp = 0.0;
        double c = 0.0;
        double rd = 0.0;
        double pressure = 0.0;
    };
    std::vector<RcrEntry> rcrEntries;

    // 5) Write the .svpre script and collect outflow data files.
    const auto svpre = root / (std::string(request.jobName) + ".svpre");
    {
        std::ofstream os(svpre);
        if (!os)
        {
            result.diagnostic = "FlowSolverExportWriter: cannot open " + svpre.string();
            return result;
        }
        os << "# XQ-generated legacy-compatible preprocessor script\n";
        os << "mesh_and_adjncy_vtu mesh-complete/mesh-complete.mesh.vtu\n";
        for (const auto& fi : faces)
        {
            const auto roleIt = roles.find(fi.name);
            std::string role = roleIt != roles.end() ? roleIt->second : fi.type;
            if (role == "inlet")
                role = "inflow";
            else if (role == "outlet")
                role = "outflow";
            const auto bcIt = bcsByFace.find(fi.name);
            const xq_BoundaryCondition* bc = bcIt != bcsByFace.end() ? bcIt->second : nullptr;

            os << "set_surface_id_vtp mesh-complete/" << fi.name << ".vtp "
               << (fi.id + 1) << "\n";

            if (role == "wall")
            {
                if (bc && bc->bcType != "no_slip")
                {
                    result.diagnostic = "FlowSolverExportWriter: face '" + fi.name +
                                        "' is marked wall but BC type is '" +
                                        bc->bcType + "'.";
                    return result;
                }
                os << "noslip_vtp mesh-complete/" << fi.name << ".vtp\n";
                continue;
            }

            if (role == "inflow")
            {
                if (!bc || bc->bcType != "prescribed_velocity")
                {
                    result.diagnostic = "FlowSolverExportWriter: inflow face '" + fi.name +
                                        "' is missing a prescribed_velocity BC.";
                    return result;
                }
                if (hasInflow)
                {
                    result.diagnostic =
                        "FlowSolverExportWriter: only one inflow boundary condition is supported.";
                    return result;
                }
                hasInflow = true;
                double constantFlow = 0.0;
                const bool hasConstantFlow =
                    parseDoubleParam(*bc, {"value", "flowRate"}, constantFlow);
                os << "prescribed_velocities_vtp mesh-complete/"
                   << fi.name << ".vtp\n"
                   << "bct_analytical_shape plug\n"
                   << "bct_period 1.0\n"
                   << "bct_point_number 201\n"
                   << "bct_fourier_mode_number 10\n"
                   << "bct_create mesh-complete/" << fi.name << ".vtp bct.dat\n"
                   << "bct_write_dat bct.dat\n"
                   << "bct_write_vtp bct.vtp\n";
                if (hasConstantFlow)
                    os << "# constant inflow value: " << constantFlow << "\n";
                continue;
            }

            if (role != "outflow")
            {
                result.diagnostic = "FlowSolverExportWriter: unsupported face role '" + role +
                                    "' for face '" + fi.name + "'.";
                return result;
            }

            if (!bc)
            {
                result.diagnostic = "FlowSolverExportWriter: outflow face '" + fi.name +
                                    "' is missing a boundary condition.";
                return result;
            }

            if (bc->bcType == "resistance")
            {
                if (hasRcr || hasPressure)
                {
                    result.diagnostic =
                        "FlowSolverExportWriter: outlet boundary conditions must not mix resistance, RCR, and pressure types.";
                    return result;
                }
                double resistance = 0.0;
                if (!parseDoubleParam(*bc, {"value", "resistance"}, resistance))
                {
                    result.diagnostic = "FlowSolverExportWriter: resistance BC for face '" +
                                        fi.name + "' is missing a numeric resistance.";
                    return result;
                }
                hasResistance = true;
                resistanceEntries.emplace_back(fi.name, resistance);
                os << "# outflow face " << fi.name << " uses resistance.dat\n";
            }
            else if (bc->bcType == "rcr")
            {
                if (hasResistance || hasPressure)
                {
                    result.diagnostic =
                        "FlowSolverExportWriter: outlet boundary conditions must not mix resistance, RCR, and pressure types.";
                    return result;
                }
                double rp = 0.0;
                double c = 0.0;
                double rd = 0.0;
                double pressure = 0.0;
                if (!parseDoubleParam(*bc, {"Rp", "rp"}, rp) ||
                    !parseDoubleParam(*bc, {"C", "c"}, c) ||
                    !parseDoubleParam(*bc, {"Rd", "rd"}, rd) ||
                    !parseDoubleParam(*bc, {"pressure", "Pressure"}, pressure))
                {
                    result.diagnostic = "FlowSolverExportWriter: RCR BC for face '" + fi.name +
                                        "' is missing Rp, C, Rd, or pressure.";
                    return result;
                }
                hasRcr = true;
                rcrEntries.push_back({fi.name, rp, c, rd, pressure});
                os << "# outflow face " << fi.name << " uses rcrt.dat\n";
            }
            else if (bc->bcType == "pressure")
            {
                if (hasResistance || hasRcr)
                {
                    result.diagnostic =
                        "FlowSolverExportWriter: outlet boundary conditions must not mix pressure with resistance or RCR.";
                    return result;
                }
                double pressure = 0.0;
                if (!parseDoubleParam(*bc, {"value", "pressure", "Pressure"}, pressure))
                {
                    result.diagnostic = "FlowSolverExportWriter: pressure BC for face '" + fi.name +
                                        "' is missing a numeric pressure.";
                    return result;
                }
                hasPressure = true;
                os << "pressure_vtp mesh-complete/" << fi.name << ".vtp "
                   << pressure << "\n";
            }
            else if (bc->bcType == "coronary" || bc->bcType == "impedance")
            {
                result.diagnostic = "FlowSolverExportWriter: boundary condition type '" + bc->bcType +
                                    "' is not yet supported by the XQ solver export.";
                return result;
            }
            else
            {
                result.diagnostic = "FlowSolverExportWriter: unsupported boundary condition type '" +
                                    bc->bcType + "' on face '" + fi.name + "'.";
                return result;
            }
        }
        os << "write_geombc geombc.dat.1\n";
        os << "write_restart restart.0.1\n";
        os << "write_numstart 0 numstart.dat\n";
    }
    result.filesWritten.push_back(svpre);

    // Diagnostic: geombc.dat.1 is referenced by the legacy-compatible
    // preprocessor script but is generated by an external backend, not by XQ.
    result.diagnostics.push_back(
        {xq::pipeline::Severity::Warning,
         "Note: geombc.dat.1 is referenced in .svpre but generated by the selected external backend, not by XQ."});

    // 6) Write solver.inp.
    const auto solverInp = root / "solver.inp";
    {
        std::ofstream os(solverInp);
        if (!os)
        {
            result.diagnostic = "FlowSolverExportWriter: cannot open " + solverInp.string();
            return result;
        }
        const auto& job = *request.solverJob;
        os << "# XQ-generated legacy-compatible solver input file\n";
        os << "# ---------- Fluid ----------\n";
        os << "Density: " << job.GetFluidDensity() << "\n";
        os << "Viscosity: " << job.GetFluidViscosity() << "\n";
        os << "# ---------- Time stepping ----------\n";
        os << "Number of Timesteps: " << job.GetNumTimesteps() << "\n";
        os << "Time Step Size: " << job.GetTimeStepSize() << "\n";
        os << "Number of Cycles: " << job.GetNumCycles() << "\n";
        os << "# ---------- Wall ----------\n";
        os << "Deformable Wall: " << (job.GetDeformable() ? "true" : "false") << "\n";
        if (job.GetDeformable())
        {
            os << "Wall Thickness: " << job.GetWallThickness() << "\n";
            os << "Wall Elastic Modulus: " << job.GetWallElasticModulus() << "\n";
            os << "Wall Density: " << job.GetWallDensity() << "\n";
            os << "Wall Poisson Ratio: " << job.GetWallPoissonRatio() << "\n";
        }
        os << "# ---------- Solver ----------\n";
        os << "Solver Type: " << job.GetSolverType() << "\n";
        os << "Number of Linear Iterations: " << job.GetNumLinearIterations() << "\n";
        os << "Number of Nonlinear Iterations: " << job.GetNumNonlinearIterations() << "\n";
        os << "Initial Pressure: " << job.GetInitialPressure() << "\n";
        os << "Initial Velocity: " << job.GetInitialVelocity() << "\n";
        os << "# ---------- Boundary Conditions ----------\n";
        const auto& bcs = job.GetBoundaryConditions();
        for (const auto& bc : bcs)
        {
            os << "Face: " << bc.faceName
               << "  Role: " << bc.faceRole
               << "  Type: " << bc.bcType << "\n";
            for (const auto& [k, v] : bc.parameters)
                os << "  " << k << ": " << v << "\n";
        }
    }
    result.filesWritten.push_back(solverInp);

    // 7) Write restart.0.1.
    const auto restart = root / "restart.0.1";
    {
        std::ofstream os(restart);
        os << "0\n";
    }
    result.filesWritten.push_back(restart);

    // 8) Write inflow/outflow data files with explicit content.
    const auto bct = root / "bct.dat";
    {
        std::ofstream os(bct);
        if (!os)
        {
            result.diagnostic = "FlowSolverExportWriter: cannot open " + bct.string();
            return result;
        }
        const auto& wf = request.inletWaveform;
        if (wf.empty())
        {
            result.diagnostic =
                "FlowSolverExportWriter: inlet waveform is missing. ExportForSolver should supply a measured or constant inflow waveform.";
            return result;
        }
        os << wf.size() << " 10\n";
        os.precision(9);
        for (const auto& [t, q] : wf)
            os << t << " " << q << "\n";
    }
    result.filesWritten.push_back(bct);

    if (!resistanceEntries.empty())
    {
        const auto resistanceFile = root / "resistance.dat";
        std::ofstream os(resistanceFile);
        if (!os)
        {
            result.diagnostic = "FlowSolverExportWriter: cannot open " + resistanceFile.string();
            return result;
        }
        for (const auto& [faceName, resistance] : resistanceEntries)
            os << faceName << " " << resistance << "\n";
        result.filesWritten.push_back(resistanceFile);
    }

    if (!rcrEntries.empty())
    {
        const auto rcrFile = root / "rcrt.dat";
        std::ofstream os(rcrFile);
        if (!os)
        {
            result.diagnostic = "FlowSolverExportWriter: cannot open " + rcrFile.string();
            return result;
        }
        os << "2\n";
        for (const auto& entry : rcrEntries)
        {
            os << "2\n";
            os << entry.faceName << "\n";
            os << entry.rp << "\n";
            os << entry.c << "\n";
            os << entry.rd << "\n";
            os << "0.0 " << entry.pressure << "\n";
            os << "1.0 " << entry.pressure << "\n";
        }
        result.filesWritten.push_back(rcrFile);
    }

    result.ok = true;
    return result;
}
