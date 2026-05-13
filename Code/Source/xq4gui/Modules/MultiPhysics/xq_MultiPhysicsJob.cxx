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

    for (const auto& d : m_Domains)
    {
        if (!d.IsValid())
            return "Domain '" + d.name + "' is invalid.";
    }

    if (m_TimeStepSize <= 0.0)
        return "Time step size must be positive.";

    if (m_NumTimeSteps <= 0)
        return "Number of time steps must be positive.";

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
