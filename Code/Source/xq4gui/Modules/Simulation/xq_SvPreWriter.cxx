#include "xq_SvPreWriter.h"

#include "xq_SolverJob.h"

#include <xq_Grid.h>
#include <xq_VascularGeometry.h>

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkXMLUnstructuredGridWriter.h>

#include <fstream>
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

xq_SvPreExportResult xq_SvPreWriter::Export(const xq_SvPreExportRequest& request)
{
    xq_SvPreExportResult result;

    if (request.jobName.empty())
    {
        result.diagnostic = "SvPreWriter: jobName is empty.";
        return result;
    }
    if (!request.solverJob || !request.geometry || !request.grid)
    {
        result.diagnostic = "SvPreWriter: solverJob / geometry / grid must all be non-null.";
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
        result.diagnostic = "SvPreWriter: grid has no volume mesh.";
        return result;
    }
    const auto vtuPath = meshDir / "mesh-complete.mesh.vtu";
    auto vtuWriter = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
    vtuWriter->SetFileName(vtuPath.string().c_str());
    vtuWriter->SetInputData(volume);
    vtuWriter->SetDataModeToBinary();
    if (!vtuWriter->Write())
    {
        result.diagnostic = "SvPreWriter: failed to write " + vtuPath.string();
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

    // 4) Write the .svpre script.
    const auto svpre = root / (std::string(request.jobName) + ".svpre");
    {
        std::ofstream os(svpre);
        if (!os)
        {
            result.diagnostic = "SvPreWriter: cannot open " + svpre.string();
            return result;
        }
        os << "# XQ-generated svsolver preprocessor script\n";
        os << "mesh_and_adjncy_vtu mesh-complete/mesh-complete.mesh.vtu\n";
        bool hasDummyOutflow = false;
        for (const auto& fi : faces)
        {
            const auto it = roles.find(fi.name);
            const std::string role = it != roles.end() ? it->second : fi.type;

            os << "set_surface_id_vtp mesh-complete/" << fi.name << ".vtp "
               << (fi.id + 1) << "\n";
            if (role == "wall")
                os << "noslip_vtp mesh-complete/" << fi.name << ".vtp\n";
            else if (role == "inflow")
                os << "prescribed_velocities_vtp mesh-complete/"
                   << fi.name << ".vtp\n"
                   << "bct_analytical_shape plug\n"
                   << "bct_period 1.0\n"
                   << "bct_point_number 201\n"
                   << "bct_fourier_mode_number 10\n"
                   << "bct_create mesh-complete/" << fi.name << ".vtp bct.dat\n"
                   << "bct_write_dat bct.dat\n"
                   << "bct_write_vtp bct.vtp\n";
            else if (role == "outflow")
            {
                os << "pressure_vtp mesh-complete/" << fi.name
                   << ".vtp 0.0  # TODO: configure RCR/impedance\n";
                hasDummyOutflow = true;
            }
        }
        os << "write_geombc geombc.dat.1\n";
        os << "write_restart restart.0.1\n";
        os << "write_numstart 0 numstart.dat\n";

        if (hasDummyOutflow)
        {
            result.diagnostics.push_back(
                {xq::pipeline::Severity::Warning,
                 "Outflow boundary conditions use zero-pressure placeholders. "
                 "Configure RCR or impedance values before running the solver."});
        }
    }
    result.filesWritten.push_back(svpre);

    // Diagnostic: geombc.dat.1 is referenced in .svpre but is generated by
    // svsolver's own svpre tool, not by XQ.
    result.diagnostics.push_back(
        {xq::pipeline::Severity::Warning,
         "Note: geombc.dat.1 is referenced in .svpre but generated by svsolver's svpre, not by XQ."});

    // 5) Write solver.inp.
    const auto solverInp = root / "solver.inp";
    {
        std::ofstream os(solverInp);
        if (!os)
        {
            result.diagnostic = "SvPreWriter: cannot open " + solverInp.string();
            return result;
        }
        const auto& job = *request.solverJob;
        os << "# XQ-generated svsolver input file\n";
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

    // 6) Write restart.0.1 placeholder.
    const auto restart = root / "restart.0.1";
    {
        std::ofstream os(restart);
        os << "0\n";
    }
    result.filesWritten.push_back(restart);

    // 7) Write bct.dat inlet waveform.
    const auto bct = root / "bct.dat";
    {
        std::ofstream os(bct);
        if (!os)
        {
            result.diagnostic = "SvPreWriter: cannot open " + bct.string();
            return result;
        }
        // Legacy bct.dat header:
        //   <numPoints> <numFourierModes>
        //   t0 Q0
        //   t1 Q1
        //   ...
        const auto& wf = request.inletWaveform;
        if (!wf.empty())
        {
            os << wf.size() << " 10\n";
            os.precision(9);
            for (const auto& [t, q] : wf)
                os << t << " " << q << "\n";
        }
        else
        {
            // Constant-flow placeholder: 1 s period, two samples.
            os << "2 10\n";
            os << "0.0 1.0\n";
            os << "1.0 1.0\n";
            os << "# NOTE: replace this placeholder with a measured inflow waveform\n";
            result.diagnostics.push_back(
                {xq::pipeline::Severity::Warning,
                 "bct.dat contains a constant-flow placeholder waveform. "
                 "Replace with a measured inflow waveform before running the solver."});
        }
    }
    result.filesWritten.push_back(bct);

    result.ok = true;
    return result;
}
