#pragma once

#include <xqModuleMultiPhysicsExports.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>

// Domain types for multi-physics simulations.
enum class XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsDomainType
{
    Fluid,
    Solid,
    Mesh
};

// Returns the string representation of a domain type.
XQMODULEMULTIPHYSICS_EXPORT std::string_view ToString(xq_MultiPhysicsDomainType t);
XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsDomainType
    DomainTypeFromString(std::string_view s);

// Material properties for a single domain.
struct XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsMaterial
{
    double density = 1.0;
    double viscosity = 0.0;      // fluid only
    double elasticModulus = 0.0; // solid only
    double poissonRatio = 0.0;   // solid only

    [[nodiscard]] bool HasFluidProperties() const;
    [[nodiscard]] bool HasSolidProperties() const;
};

// One domain in a multi-physics job.
struct XQMODULEMULTIPHYSICS_EXPORT xq_MultiPhysicsDomain
{
    std::string name;
    xq_MultiPhysicsDomainType type = xq_MultiPhysicsDomainType::Fluid;
    xq_MultiPhysicsMaterial material;
    std::map<std::string, std::string> properties; // extensible key-value

    [[nodiscard]] bool IsValid() const; // name non-empty, type set
};
