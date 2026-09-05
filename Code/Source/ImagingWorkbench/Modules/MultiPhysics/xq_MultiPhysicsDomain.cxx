#include "xq_MultiPhysicsDomain.h"

#include <algorithm>
#include <cctype>

std::string_view ToString(xq_MultiPhysicsDomainType t)
{
    switch (t)
    {
    case xq_MultiPhysicsDomainType::Fluid: return "fluid";
    case xq_MultiPhysicsDomainType::Solid: return "solid";
    case xq_MultiPhysicsDomainType::Mesh:  return "mesh";
    }
    return "unknown";
}

xq_MultiPhysicsDomainType DomainTypeFromString(std::string_view s)
{
    std::string lower(s);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (lower == "fluid") return xq_MultiPhysicsDomainType::Fluid;
    if (lower == "solid") return xq_MultiPhysicsDomainType::Solid;
    if (lower == "mesh")  return xq_MultiPhysicsDomainType::Mesh;
    return xq_MultiPhysicsDomainType::Fluid;
}

bool xq_MultiPhysicsMaterial::HasFluidProperties() const
{
    return density > 0.0 && viscosity > 0.0;
}

bool xq_MultiPhysicsMaterial::HasSolidProperties() const
{
    return density > 0.0 &&
           elasticModulus > 0.0 &&
           poissonRatio > 0.0 &&
           poissonRatio < 0.5;
}

bool xq_MultiPhysicsDomain::IsValid() const
{
    if (name.empty())
        return false;

    switch (type)
    {
    case xq_MultiPhysicsDomainType::Fluid:
        return material.HasFluidProperties();
    case xq_MultiPhysicsDomainType::Solid:
        return material.HasSolidProperties();
    case xq_MultiPhysicsDomainType::Mesh:
        return true;
    }

    return false;
}
