#pragma once

#include <xqModuleMultiPhysicsExports.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>

// Equation types for multi-physics coupling.
enum class XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsEquationType
{
    Fluid,
    Structure,
    FSI
};

XQMODULEMULTIPHYSICS_EXPORT std::string_view ToString(xq_MultiPhysicsEquationType t);
XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsEquationType
    EquationTypeFromString(std::string_view s);

// Boundary condition on a domain face.
enum class XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsBCType
{
    Dirichlet,
    Neumann,
    Traction,
    Resistance
};

XQMODULEMULTIPHYSICS_EXPORT std::string_view ToString(xq_MultiPhysicsBCType t);
XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsBCType
    BCTypeFromString(std::string_view s);

// Solver settings for one equation.
struct XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsSolverSettings
{
    std::string linearSolver = "gmres";
    double tolerance = 1e-6;
    int maxIterations = 100;
};

// One coupling equation in a multi-physics job.
struct XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsEquation
{
    std::string name;
    xq_MultiPhysicsEquationType type = xq_MultiPhysicsEquationType::Fluid;
    std::vector<std::string> domainNames; // domains this equation couples
    xq_MultiPhysicsSolverSettings solverSettings;
    std::map<std::string, std::string> properties;
};

// One boundary condition attached to a domain face.
struct XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsBoundaryCondition
{
    std::string faceName;
    std::string domainName;
    xq_MultiPhysicsBCType bcType = xq_MultiPhysicsBCType::Dirichlet;
    std::map<std::string, std::string> parameters; // e.g. "value", "function"
};
