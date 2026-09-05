#include "xq_ROMJob.h"

#include <algorithm>
#include <sstream>

// --- Job identity ---
void xq_ROMJob::SetJobName(std::string_view name) { m_JobName = name; }
const std::string& xq_ROMJob::GetJobName() const { return m_JobName; }

// --- Model properties ---
void xq_ROMJob::SetModelType(std::string_view type) { m_ModelType = type; }
const std::string& xq_ROMJob::GetModelType() const { return m_ModelType; }

// --- Cap properties ---
void xq_ROMJob::SetCapProp(std::string_view capName, std::string_view key,
                            std::string_view value)
{
    m_CapProps[std::string(capName)][std::string(key)] = std::string(value);
}

std::string xq_ROMJob::GetCapProp(std::string_view capName,
                                   std::string_view key) const
{
    auto cit = m_CapProps.find(std::string(capName));
    if (cit == m_CapProps.end()) return "";
    auto kit = cit->second.find(std::string(key));
    return kit != cit->second.end() ? kit->second : "";
}

const std::map<std::string, std::map<std::string, std::string>>&
xq_ROMJob::GetCapProps() const { return m_CapProps; }

void xq_ROMJob::SetCapProps(
    const std::map<std::string, std::map<std::string, std::string>>& caps)
{
    m_CapProps = caps;
}

void xq_ROMJob::SetRCR(std::string_view capName, double Rp, double C, double Rd)
{
    auto& props = m_CapProps[std::string(capName)];
    props["Rp"] = std::to_string(Rp);
    props["C"]  = std::to_string(C);
    props["Rd"] = std::to_string(Rd);
}

bool xq_ROMJob::GetRCR(std::string_view capName,
                        double& Rp, double& C, double& Rd) const
{
    auto cit = m_CapProps.find(std::string(capName));
    if (cit == m_CapProps.end()) return false;

    auto rpIt = cit->second.find("Rp");
    auto cIt  = cit->second.find("C");
    auto rdIt = cit->second.find("Rd");
    if (rpIt == cit->second.end() || cIt == cit->second.end() || rdIt == cit->second.end())
        return false;

    try
    {
        Rp = std::stod(rpIt->second);
        C  = std::stod(cIt->second);
        Rd = std::stod(rdIt->second);
        return true;
    }
    catch (...) { return false; }
}

// --- Wall properties ---
void xq_ROMJob::SetWallThickness(double t) { m_WallThickness = t; }
double xq_ROMJob::GetWallThickness() const { return m_WallThickness; }

void xq_ROMJob::SetWallElasticModulus(double e) { m_WallElasticModulus = e; }
double xq_ROMJob::GetWallElasticModulus() const { return m_WallElasticModulus; }

void xq_ROMJob::SetWallPoissonRatio(double v) { m_WallPoissonRatio = v; }
double xq_ROMJob::GetWallPoissonRatio() const { return m_WallPoissonRatio; }

// --- Solver run properties ---
void xq_ROMJob::SetTimeStepSize(double dt) { m_TimeStepSize = dt; }
double xq_ROMJob::GetTimeStepSize() const { return m_TimeStepSize; }

void xq_ROMJob::SetNumTimeSteps(int n) { m_NumTimeSteps = n; }
int xq_ROMJob::GetNumTimeSteps() const { return m_NumTimeSteps; }

void xq_ROMJob::SetSolverTolerance(double tol) { m_SolverTolerance = tol; }
double xq_ROMJob::GetSolverTolerance() const { return m_SolverTolerance; }

void xq_ROMJob::SetMaxSolverIterations(int n) { m_MaxSolverIterations = n; }
int xq_ROMJob::GetMaxSolverIterations() const { return m_MaxSolverIterations; }

// --- Result conversion ---
void xq_ROMJob::SetOutputFormat(std::string_view fmt) { m_OutputFormat = fmt; }
const std::string& xq_ROMJob::GetOutputFormat() const { return m_OutputFormat; }

void xq_ROMJob::AddOutputField(std::string_view fieldName)
{
    m_OutputFields.push_back(std::string(fieldName));
}

const std::vector<std::string>& xq_ROMJob::GetOutputFields() const
{
    return m_OutputFields;
}

// --- Properties ---
void xq_ROMJob::SetProperty(std::string_view key, std::string_view value)
{
    m_Properties[std::string(key)] = std::string(value);
}

std::string xq_ROMJob::GetProperty(std::string_view key) const
{
    auto it = m_Properties.find(std::string(key));
    return it != m_Properties.end() ? it->second : "";
}

const std::map<std::string, std::string>& xq_ROMJob::GetProperties() const
{
    return m_Properties;
}

void xq_ROMJob::SetProperties(const std::map<std::string, std::string>& props)
{
    m_Properties = props;
}

// --- Validation ---
std::string xq_ROMJob::Validate() const
{
    if (m_JobName.empty())
        return "Job name is empty.";

    // Check for at least one inlet and one outlet configured via cap props.
    bool hasInlet = false;
    bool hasOutlet = false;
    for (const auto& [capName, props] : m_CapProps)
    {
        auto roleIt = props.find("role");
        if (roleIt != props.end())
        {
            if (roleIt->second == "inflow" || roleIt->second == "inlet")
                hasInlet = true;
            if (roleIt->second == "outflow" || roleIt->second == "outlet")
                hasOutlet = true;
        }
    }

    if (!hasInlet)
        return "No inlet cap configured.";

    if (!hasOutlet)
        return "No outlet cap configured.";

    if (m_TimeStepSize <= 0.0)
        return "Time step size must be positive.";

    if (m_NumTimeSteps <= 0)
        return "Number of time steps must be positive.";

    return "";
}
