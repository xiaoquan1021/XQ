#pragma once

#include <xqModelCommonExports.h>

#include "xq_VascularGeometry.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

class XQMODELCOMMON_EXPORT xq_GeometryFactory
{
public:
    using CreateFunc = std::function<std::unique_ptr<xq_VascularGeometry>()>;

    [[nodiscard]] static std::unique_ptr<xq_VascularGeometry> CreateModelElement(const std::string& type);
    static void RegisterCreator(const std::string& type, CreateFunc func);
    [[nodiscard]] static std::vector<std::string> GetAvailableTypes();

private:
    xq_GeometryFactory() = delete;
    ~xq_GeometryFactory() = delete;

    [[nodiscard]] static std::map<std::string, CreateFunc>& GetCreatorMap();
};
