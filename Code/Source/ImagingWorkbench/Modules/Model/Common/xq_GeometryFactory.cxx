#include "xq_GeometryFactory.h"

#include <algorithm>

std::map<std::string, xq_GeometryFactory::CreateFunc>& xq_GeometryFactory::GetCreatorMap()
{
    static std::map<std::string, CreateFunc> s_Creators;
    return s_Creators;
}

std::unique_ptr<xq_VascularGeometry> xq_GeometryFactory::CreateModelElement(const std::string& type)
{
    const auto& creators = GetCreatorMap();
    if (auto it = creators.find(type); it != creators.end())
    {
        return it->second();
    }
    return nullptr;
}

void xq_GeometryFactory::RegisterCreator(const std::string& type, CreateFunc func)
{
    GetCreatorMap()[type] = std::move(func);
}

std::vector<std::string> xq_GeometryFactory::GetAvailableTypes()
{
    std::vector<std::string> types;
    const auto& creators = GetCreatorMap();
    types.reserve(creators.size());
    std::transform(creators.cbegin(), creators.cend(), std::back_inserter(types),
        [](const auto& pair) { return pair.first; });
    return types;
}
