#pragma once

#include <xqMeshCommonExports.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class xq_Grid;

class XQMESHCOMMON_EXPORT xq_GridFactory
{
public:
    using CreateFunc = std::function<std::unique_ptr<xq_Grid>()>;

    [[nodiscard]] static std::unique_ptr<xq_Grid> CreateMesh(std::string_view type);
    static void RegisterCreator(std::string_view type, CreateFunc func);
    [[nodiscard]] static std::vector<std::string> GetAvailableTypes();

private:
    xq_GridFactory() = delete;
    ~xq_GridFactory() = delete;

    [[nodiscard]] static std::map<std::string, CreateFunc, std::less<>>& GetRegistry();
};
