#include "xq_MultiPhysicsEquation.h"

#include <algorithm>
#include <cctype>

std::string_view ToString(xq_MultiPhysicsEquationType t)
{
    switch (t)
    {
    case xq_MultiPhysicsEquationType::Fluid:      return "fluid";
    case xq_MultiPhysicsEquationType::Structure:  return "struct";
    case xq_MultiPhysicsEquationType::FSI:        return "fsi";
    }
    return "unknown";
}

xq_MultiPhysicsEquationType EquationTypeFromString(std::string_view s)
{
    std::string lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower == "fluid")  return xq_MultiPhysicsEquationType::Fluid;
    if (lower == "struct") return xq_MultiPhysicsEquationType::Structure;
    if (lower == "fsi")    return xq_MultiPhysicsEquationType::FSI;
    return xq_MultiPhysicsEquationType::Fluid;
}

std::string_view ToString(xq_MultiPhysicsBCType t)
{
    switch (t)
    {
    case xq_MultiPhysicsBCType::Dirichlet:  return "dirichlet";
    case xq_MultiPhysicsBCType::Neumann:    return "neumann";
    case xq_MultiPhysicsBCType::Traction:   return "traction";
    case xq_MultiPhysicsBCType::Resistance: return "resistance";
    }
    return "unknown";
}

xq_MultiPhysicsBCType BCTypeFromString(std::string_view s)
{
    std::string lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower == "dirichlet")  return xq_MultiPhysicsBCType::Dirichlet;
    if (lower == "neumann")    return xq_MultiPhysicsBCType::Neumann;
    if (lower == "traction")   return xq_MultiPhysicsBCType::Traction;
    if (lower == "resistance") return xq_MultiPhysicsBCType::Resistance;
    return xq_MultiPhysicsBCType::Dirichlet;
}
