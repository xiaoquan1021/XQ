#include "xq_MultiPhysicsJob.h"

#include <algorithm>
#include <sstream>

// --- Job identity ---
void xq_MultiPhysicsJob::SetJobName(std::string_view name) { m_JobName = name; }
const std::string& xq_MultiPhysicsJob::GetJobName() const { return m_JobName; }

// --- Time stepping ---
void xq_MultiPhysicsJob::SetTimeStepSize(double dt) { m_TimeStepSize = dt; }
double xq_MultiPhysicsJob::GetTimeStepSize() const { return m_TimeStepSize; }

void xq_MultiPhysicsJob::SetNumTimeSteps(int n) { m_NumTimeSteps = n; }
int xq_MultiPhysicsJob::GetNumTimeSteps() const { return m_NumTimeSteps; }

// --- Domains ---
void xq_MultiPhysicsJob::AddDomain(const xq_MultiPhysicsDomain& domain)
{
    m_Domains.push_back(domain);
}

const std::vector<xq_MultiPhysicsDomain>& xq_MultiPhysicsJob::GetDomains() const
{
    return m_Domains;
}

xq_MultiPhysicsDomain* xq_MultiPhysicsJob::FindDomain(std::string_view name)
{
    for (auto& d : m_Domains)
        if (d.name == name) return &d;
    return nullptr;
}

// --- Equations ---
void xq_MultiPhysicsJob::AddEquation(const xq_MultiPhysicsEquation& eq)
{
    m_Equations.push_back(eq);
}

const std::vector<xq_MultiPhysicsEquation>& xq_MultiPhysicsJob::GetEquations() const
{
    return m_Equations;
}

// --- Boundary conditions ---
void xq_MultiPhysicsJob::AddBoundaryCondition(const xq_MultiPhysicsBoundaryCondition& bc)
{
    m_BoundaryConditions.push_back(bc);
}

const std::vector<xq_MultiPhysicsBoundaryCondition>&
xq_MultiPhysicsJob::GetBoundaryConditions() const
{
    return m_BoundaryConditions;
}

// --- Validation ---
std::string xq_MultiPhysicsJob::Validate() const
{
    if (m_Domains.empty())
        return "At least one domain is required.";

    if (m_Equations.empty())
        return "At least one equation is required.";

    for (size_t i = 0; i < m_Domains.size(); ++i)
    {
        const auto& d = m_Domains[i];
        if (!d.IsValid())
            return "Domain '" + d.name + "' is invalid.";

        for (size_t j = i + 1; j < m_Domains.size(); ++j)
        {
            if (m_Domains[j].name == d.name)
                return "Duplicate domain name '" + d.name + "'.";
        }
    }

    if (m_TimeStepSize <= 0.0)
        return "Time step size must be positive.";

    if (m_NumTimeSteps <= 0)
        return "Number of time steps must be positive.";

    auto findDomain = [this](std::string_view name) {
        return std::find_if(
            m_Domains.begin(), m_Domains.end(),
            [name](const xq_MultiPhysicsDomain& d) { return d.name == name; });
    };

    for (const auto& eq : m_Equations)
    {
        if (eq.name.empty())
            return "Equation name must not be empty.";
        if (eq.domainNames.empty())
            return "Equation '" + eq.name + "' must reference at least one domain.";
        if (eq.solverSettings.linearSolver.empty())
            return "Equation '" + eq.name + "' has an empty linear solver.";
        if (eq.solverSettings.tolerance <= 0.0)
            return "Equation '" + eq.name + "' must use a positive tolerance.";
        if (eq.solverSettings.maxIterations <= 0)
            return "Equation '" + eq.name + "' must use positive maxIterations.";

        bool hasFluidDomain = false;
        bool hasSolidDomain = false;
        for (const auto& domainName : eq.domainNames)
        {
            const auto it = findDomain(domainName);
            if (it == m_Domains.end())
                return "Equation '" + eq.name + "' references unknown domain '" +
                       domainName + "'.";

            switch (eq.type)
            {
            case xq_MultiPhysicsEquationType::Fluid:
                if (it->type != xq_MultiPhysicsDomainType::Fluid)
                    return "Equation '" + eq.name +
                           "' must reference fluid domains only.";
                hasFluidDomain = true;
                break;
            case xq_MultiPhysicsEquationType::Structure:
                if (it->type != xq_MultiPhysicsDomainType::Solid)
                    return "Equation '" + eq.name +
                           "' must reference solid domains only.";
                hasSolidDomain = true;
                break;
            case xq_MultiPhysicsEquationType::FSI:
                if (it->type == xq_MultiPhysicsDomainType::Mesh)
                    return "Equation '" + eq.name +
                           "' cannot reference mesh-only domains.";
                if (it->type == xq_MultiPhysicsDomainType::Fluid)
                    hasFluidDomain = true;
                else if (it->type == xq_MultiPhysicsDomainType::Solid)
                    hasSolidDomain = true;
                break;
            }
        }

        if (eq.type == xq_MultiPhysicsEquationType::Fluid && !hasFluidDomain)
            return "Equation '" + eq.name + "' must reference at least one fluid domain.";
        if (eq.type == xq_MultiPhysicsEquationType::Structure && !hasSolidDomain)
            return "Equation '" + eq.name + "' must reference at least one solid domain.";
        if (eq.type == xq_MultiPhysicsEquationType::FSI &&
            (!hasFluidDomain || !hasSolidDomain))
        {
            return "Equation '" + eq.name +
                   "' must reference at least one fluid domain and one solid domain.";
        }
    }

    for (const auto& bc : m_BoundaryConditions)
    {
        if (bc.faceName.empty())
            return "Boundary condition has an empty face name.";
        if (!bc.domainName.empty())
        {
            const auto it = findDomain(bc.domainName);
            if (it == m_Domains.end())
                return "Boundary condition for face '" + bc.faceName +
                       "' references unknown domain '" + bc.domainName + "'.";
            if (it->type == xq_MultiPhysicsDomainType::Mesh)
                return "Boundary condition for face '" + bc.faceName +
                       "' cannot target a mesh-only domain.";
        }
    }

    return "";
}

// --- Properties ---
void xq_MultiPhysicsJob::SetProperty(std::string_view key, std::string_view value)
{
    m_Properties[std::string(key)] = std::string(value);
}

std::string xq_MultiPhysicsJob::GetProperty(std::string_view key) const
{
    auto it = m_Properties.find(std::string(key));
    return it != m_Properties.end() ? it->second : "";
}

const std::map<std::string, std::string>& xq_MultiPhysicsJob::GetProperties() const
{
    return m_Properties;
}

void xq_MultiPhysicsJob::SetProperties(const std::map<std::string, std::string>& props)
{
    m_Properties = props;
}
