#include "xq_SolverJob.h"

#include <algorithm>
#include <sstream>

// --- Clone ---

std::unique_ptr<xq_SolverJob> xq_SolverJob::Clone() const
{
    return std::make_unique<xq_SolverJob>(*this);
}

// --- Private helpers ---

std::string xq_SolverJob::LookupMapValue(const XqSimPropertyMap& m, std::string_view key)
{
    const auto keyStr = std::string(key);
    if (auto it = m.find(keyStr); it != m.end())
        return it->second;
    return {};
}

std::string xq_SolverJob::LookupCapMapValue(const XqSimCapPropertyMap& m,
                                              std::string_view capName,
                                              std::string_view key)
{
    if (auto capIt = m.find(std::string(capName)); capIt != m.end())
        return LookupMapValue(capIt->second, key);
    return {};
}

// --- Basic properties ---

void xq_SolverJob::SetJobName(std::string_view name) { m_JobName = name; }
const std::string& xq_SolverJob::GetJobName() const { return m_JobName; }

void xq_SolverJob::SetNumTimesteps(int n) { m_NumTimesteps = n; }
int xq_SolverJob::GetNumTimesteps() const { return m_NumTimesteps; }

void xq_SolverJob::SetTimeStepSize(double dt) { m_TimeStepSize = dt; }
double xq_SolverJob::GetTimeStepSize() const { return m_TimeStepSize; }

void xq_SolverJob::SetNumCycles(int n) { m_NumCycles = n; }
int xq_SolverJob::GetNumCycles() const { return m_NumCycles; }

// --- Wall properties ---

void xq_SolverJob::SetDeformable(bool flag) { m_Deformable = flag; }
bool xq_SolverJob::GetDeformable() const { return m_Deformable; }

// --- Fluid properties ---

void xq_SolverJob::SetFluidDensity(double v) { m_FluidDensity = v; }
double xq_SolverJob::GetFluidDensity() const { return m_FluidDensity; }

void xq_SolverJob::SetFluidViscosity(double v) { m_FluidViscosity = v; }
double xq_SolverJob::GetFluidViscosity() const { return m_FluidViscosity; }

void xq_SolverJob::SetInitialPressure(double v) { m_InitialPressure = v; }
double xq_SolverJob::GetInitialPressure() const { return m_InitialPressure; }

void xq_SolverJob::SetInitialVelocity(double v) { m_InitialVelocity = v; }
double xq_SolverJob::GetInitialVelocity() const { return m_InitialVelocity; }

// --- Wall properties (continued) ---

void xq_SolverJob::SetWallThickness(double t) { m_WallThickness = t; }
double xq_SolverJob::GetWallThickness() const { return m_WallThickness; }

void xq_SolverJob::SetWallElasticModulus(double e) { m_WallElasticModulus = e; }
double xq_SolverJob::GetWallElasticModulus() const { return m_WallElasticModulus; }

void xq_SolverJob::SetWallDensity(double d) { m_WallDensity = d; }
double xq_SolverJob::GetWallDensity() const { return m_WallDensity; }

void xq_SolverJob::SetWallPoissonRatio(double v) { m_WallPoissonRatio = v; }
double xq_SolverJob::GetWallPoissonRatio() const { return m_WallPoissonRatio; }

// --- Boundary condition list ---

void xq_SolverJob::SetBoundaryConditions(const std::vector<xq_BoundaryCondition>& bcs)
{
    m_BoundaryConditions = bcs;
}

const std::vector<xq_BoundaryCondition>& xq_SolverJob::GetBoundaryConditions() const
{
    return m_BoundaryConditions;
}

void xq_SolverJob::AddBoundaryCondition(const xq_BoundaryCondition& bc)
{
    m_BoundaryConditions.push_back(bc);
}

std::string xq_SolverJob::Validate() const
{
    if (m_JobName.empty())
        return "Job name must not be empty.";
    if (m_NumTimesteps <= 0)
        return "Number of timesteps must be positive.";
    if (m_TimeStepSize <= 0.0)
        return "Time step size must be positive.";
    if (m_NumCycles <= 0)
        return "Number of cycles must be positive.";

    if (m_FluidDensity <= 0.0)
        return "Fluid density must be positive.";
    if (m_FluidViscosity <= 0.0)
        return "Fluid viscosity must be positive.";

    if (m_Deformable)
    {
        if (m_WallThickness <= 0.0)
            return "Wall thickness must be positive for deformable-wall jobs.";
        if (m_WallElasticModulus <= 0.0)
            return "Wall elastic modulus must be positive for deformable-wall jobs.";
        if (m_WallDensity <= 0.0)
            return "Wall density must be positive for deformable-wall jobs.";
    }

    if (m_SolverType.empty())
        return "Solver type must not be empty.";
    if (m_NumLinearIterations <= 0)
        return "Number of linear iterations must be positive.";
    if (m_NumNonlinearIterations <= 0)
        return "Number of nonlinear iterations must be positive.";

    std::vector<std::string> seenFaces;
    for (const auto& bc : m_BoundaryConditions)
    {
        if (bc.faceName.empty())
            return "Boundary condition has an empty face name.";
        if (std::find(seenFaces.begin(), seenFaces.end(), bc.faceName) != seenFaces.end())
            return "Duplicate boundary condition for face '" + bc.faceName + "'.";
        seenFaces.push_back(bc.faceName);

        if (bc.faceRole != "wall" && bc.faceRole != "inflow" && bc.faceRole != "outflow")
            return "Boundary condition for face '" + bc.faceName +
                   "' has unsupported role '" + bc.faceRole + "'.";
        if (bc.bcType.empty())
            return "Boundary condition for face '" + bc.faceName + "' has an empty type.";
        if (bc.faceRole == "wall" && bc.bcType != "no_slip")
            return "Wall face '" + bc.faceName + "' must use no_slip.";
        if (bc.faceRole == "inflow" && bc.bcType != "prescribed_velocity")
            return "Inflow face '" + bc.faceName + "' must use prescribed_velocity.";
        if (bc.faceRole == "outflow" &&
            bc.bcType != "resistance" && bc.bcType != "rcr" &&
            bc.bcType != "pressure" && bc.bcType != "impedance" &&
            bc.bcType != "coronary")
        {
            return "Outflow face '" + bc.faceName +
                   "' must use resistance, rcr, pressure, impedance, or coronary.";
        }

        if (bc.bcType == "prescribed_velocity" &&
            bc.waveform.empty() &&
            bc.parameters.find("value") == bc.parameters.end() &&
            bc.parameters.find("flowRate") == bc.parameters.end())
        {
            return "Inflow face '" + bc.faceName +
                   "' requires a waveform or a numeric inflow value.";
        }
        if ((bc.bcType == "resistance" || bc.bcType == "pressure") &&
            bc.parameters.find("value") == bc.parameters.end() &&
            bc.parameters.find("resistance") == bc.parameters.end() &&
            bc.parameters.find("pressure") == bc.parameters.end())
        {
            return "Boundary condition for face '" + bc.faceName +
                   "' requires a numeric value.";
        }
        if (bc.bcType == "rcr" &&
            (bc.parameters.find("Rp") == bc.parameters.end() ||
             bc.parameters.find("C") == bc.parameters.end() ||
             bc.parameters.find("Rd") == bc.parameters.end()))
        {
            return "RCR boundary condition for face '" + bc.faceName +
                   "' requires Rp, C, and Rd.";
        }
    }

    return {};
}

// --- Solver properties ---

void xq_SolverJob::SetSolverType(std::string_view type) { m_SolverType = type; }
const std::string& xq_SolverJob::GetSolverType() const { return m_SolverType; }

void xq_SolverJob::SetNumLinearIterations(int n) { m_NumLinearIterations = n; }
int xq_SolverJob::GetNumLinearIterations() const { return m_NumLinearIterations; }

void xq_SolverJob::SetNumNonlinearIterations(int n) { m_NumNonlinearIterations = n; }
int xq_SolverJob::GetNumNonlinearIterations() const { return m_NumNonlinearIterations; }

// --- Generic property map accessors ---

void xq_SolverJob::SetBasicProp(std::string_view key, std::string_view value)
{ m_BasicProps[std::string(key)] = std::string(value); }
std::string xq_SolverJob::GetBasicProp(std::string_view key) const
{ return LookupMapValue(m_BasicProps, key); }
const XqSimPropertyMap& xq_SolverJob::GetBasicProps() const { return m_BasicProps; }
void xq_SolverJob::SetBasicProps(const XqSimPropertyMap& props) { m_BasicProps = props; }

void xq_SolverJob::SetWallProp(std::string_view key, std::string_view value)
{ m_WallProps[std::string(key)] = std::string(value); }
std::string xq_SolverJob::GetWallProp(std::string_view key) const
{ return LookupMapValue(m_WallProps, key); }
const XqSimPropertyMap& xq_SolverJob::GetWallProps() const { return m_WallProps; }
void xq_SolverJob::SetWallProps(const XqSimPropertyMap& props) { m_WallProps = props; }

void xq_SolverJob::SetSolverProp(std::string_view key, std::string_view value)
{ m_SolverProps[std::string(key)] = std::string(value); }
std::string xq_SolverJob::GetSolverProp(std::string_view key) const
{ return LookupMapValue(m_SolverProps, key); }
const XqSimPropertyMap& xq_SolverJob::GetSolverProps() const { return m_SolverProps; }
void xq_SolverJob::SetSolverProps(const XqSimPropertyMap& props) { m_SolverProps = props; }

void xq_SolverJob::SetRunProp(std::string_view key, std::string_view value)
{ m_RunProps[std::string(key)] = std::string(value); }
std::string xq_SolverJob::GetRunProp(std::string_view key) const
{ return LookupMapValue(m_RunProps, key); }
const XqSimPropertyMap& xq_SolverJob::GetRunProps() const { return m_RunProps; }
void xq_SolverJob::SetRunProps(const XqSimPropertyMap& props) { m_RunProps = props; }

// --- Cap property accessors ---

void xq_SolverJob::SetCapProp(std::string_view capName, std::string_view key, std::string_view value)
{
    m_CapProps[std::string(capName)][std::string(key)] = std::string(value);
}

std::string xq_SolverJob::GetCapProp(std::string_view capName, std::string_view key) const
{
    return LookupCapMapValue(m_CapProps, capName, key);
}

const XqSimCapPropertyMap& xq_SolverJob::GetCapProps() const { return m_CapProps; }
void xq_SolverJob::SetCapProps(const XqSimCapPropertyMap& props) { m_CapProps = props; }

void xq_SolverJob::SetIcProp(std::string_view capName, std::string_view key, std::string_view value)
{
    m_IcProps[std::string(capName)][std::string(key)] = std::string(value);
}

std::string xq_SolverJob::GetIcProp(std::string_view capName, std::string_view key) const
{
    return LookupCapMapValue(m_IcProps, capName, key);
}

const XqSimCapPropertyMap& xq_SolverJob::GetIcProps() const { return m_IcProps; }
void xq_SolverJob::SetIcProps(const XqSimCapPropertyMap& props) { m_IcProps = props; }

// --- Builder implementation ---

xq_SolverJob::Builder& xq_SolverJob::Builder::withName(std::string_view name)
{
    m_Job.SetJobName(name);
    return *this;
}

xq_SolverJob::Builder& xq_SolverJob::Builder::withTimesteps(int count, double stepSize)
{
    m_Job.SetNumTimesteps(count);
    m_Job.SetTimeStepSize(stepSize);
    return *this;
}

xq_SolverJob::Builder& xq_SolverJob::Builder::withCycles(int n)
{
    m_Job.SetNumCycles(n);
    return *this;
}

xq_SolverJob::Builder& xq_SolverJob::Builder::withDeformableWall(double thickness,
                                                                    double elasticModulus,
                                                                    double density)
{
    m_Job.SetDeformable(true);
    m_Job.SetWallThickness(thickness);
    m_Job.SetWallElasticModulus(elasticModulus);
    m_Job.SetWallDensity(density);
    return *this;
}

xq_SolverJob::Builder& xq_SolverJob::Builder::withRigidWall()
{
    m_Job.SetDeformable(false);
    return *this;
}

xq_SolverJob::Builder& xq_SolverJob::Builder::withSolver(std::string_view type,
                                                            int linearIter,
                                                            int nonlinearIter)
{
    m_Job.SetSolverType(type);
    m_Job.SetNumLinearIterations(linearIter);
    m_Job.SetNumNonlinearIterations(nonlinearIter);
    return *this;
}

xq_SolverJob::Builder& xq_SolverJob::Builder::withBasicProp(std::string_view key, std::string_view value)
{
    m_Job.SetBasicProp(key, value);
    return *this;
}

xq_SolverJob::Builder& xq_SolverJob::Builder::withWallProp(std::string_view key, std::string_view value)
{
    m_Job.SetWallProp(key, value);
    return *this;
}

xq_SolverJob::Builder& xq_SolverJob::Builder::withSolverProp(std::string_view key, std::string_view value)
{
    m_Job.SetSolverProp(key, value);
    return *this;
}

xq_SolverJob::Builder& xq_SolverJob::Builder::withRunProp(std::string_view key, std::string_view value)
{
    m_Job.SetRunProp(key, value);
    return *this;
}

xq_SolverJob::Builder& xq_SolverJob::Builder::withCapBoundaryCondition(std::string_view capName,
                                                                          std::string_view key,
                                                                          std::string_view value)
{
    m_Job.SetCapProp(capName, key, value);
    return *this;
}

xq_SolverJob::Builder& xq_SolverJob::Builder::withInitialCondition(std::string_view capName,
                                                                      std::string_view key,
                                                                      std::string_view value)
{
    m_Job.SetIcProp(capName, key, value);
    return *this;
}

std::unique_ptr<xq_SolverJob> xq_SolverJob::Builder::build() const
{
    return std::make_unique<xq_SolverJob>(m_Job);
}
