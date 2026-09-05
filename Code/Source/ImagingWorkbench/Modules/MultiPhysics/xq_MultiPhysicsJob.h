#pragma once

#include <xqModuleMultiPhysicsExports.h>

#include "xq_MultiPhysicsDomain.h"
#include "xq_MultiPhysicsEquation.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

// Top-level multi-physics job definition.  Owns all domains, equations,
// boundary conditions, and time-stepping parameters.
class XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsJob
{
public:
    xq_MultiPhysicsJob() = default;
    ~xq_MultiPhysicsJob() = default;

    // --- Job identity ---
    void SetJobName(std::string_view name);
    [[nodiscard]] const std::string& GetJobName() const;

    // --- Time stepping ---
    void SetTimeStepSize(double dt);
    [[nodiscard]] double GetTimeStepSize() const;

    void SetNumTimeSteps(int n);
    [[nodiscard]] int GetNumTimeSteps() const;

    // --- Domains ---
    void AddDomain(const xq_MultiPhysicsDomain& domain);
    [[nodiscard]] const std::vector<xq_MultiPhysicsDomain>& GetDomains() const;
    [[nodiscard]] xq_MultiPhysicsDomain* FindDomain(std::string_view name);

    // --- Equations ---
    void AddEquation(const xq_MultiPhysicsEquation& eq);
    [[nodiscard]] const std::vector<xq_MultiPhysicsEquation>& GetEquations() const;

    // --- Boundary conditions ---
    void AddBoundaryCondition(const xq_MultiPhysicsBoundaryCondition& bc);
    [[nodiscard]] const std::vector<xq_MultiPhysicsBoundaryCondition>&
        GetBoundaryConditions() const;

    // --- Validation ---
    // Returns empty string if the job is valid, or a diagnostic message.
    [[nodiscard]] std::string Validate() const;

    // --- Extensible properties ---
    void SetProperty(std::string_view key, std::string_view value);
    [[nodiscard]] std::string GetProperty(std::string_view key) const;
    [[nodiscard]] const std::map<std::string, std::string>& GetProperties() const;
    void SetProperties(const std::map<std::string, std::string>& props);

private:
    std::string m_JobName;
    double m_TimeStepSize = 0.001;
    int m_NumTimeSteps = 100;

    std::vector<xq_MultiPhysicsDomain> m_Domains;
    std::vector<xq_MultiPhysicsEquation> m_Equations;
    std::vector<xq_MultiPhysicsBoundaryCondition> m_BoundaryConditions;
    std::map<std::string, std::string> m_Properties;
};
