#include "xq_GridFactory.h"
#include "xq_TetGenGrid.h"

#include <algorithm>

std::map<std::string, xq_GridFactory::CreateFunc, std::less<>>& xq_GridFactory::GetRegistry()
{
    static std::map<std::string, CreateFunc, std::less<>> registry;
    return registry;
}

std::unique_ptr<xq_Grid> xq_GridFactory::CreateMesh(std::string_view type)
{
    auto& registry = GetRegistry();
    if (auto it = registry.find(type); it != registry.end())
    {
        return it->second();
    }
    return nullptr;
}

void xq_GridFactory::RegisterCreator(std::string_view type, CreateFunc func)
{
    GetRegistry().insert_or_assign(std::string(type), std::move(func));
}

std::vector<std::string> xq_GridFactory::GetAvailableTypes()
{
    std::vector<std::string> types;
    auto& registry = GetRegistry();
    types.reserve(registry.size());
    std::transform(registry.cbegin(), registry.cend(), std::back_inserter(types),
                   [](const auto& entry) { return entry.first; });
    return types;
}

// Auto-register TetGen mesh type
static const bool s_tetgenRegistered = (xq_GridFactory::RegisterCreator(
    "TetGen", []() { return std::make_unique<xq_TetGenGrid>(); }), true);
