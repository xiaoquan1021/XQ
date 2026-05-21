#include "xq_RegisterPolyDataFunction.h"
#include "xq_GeometryFactory.h"
#include "xq_PolyGeometry.h"
#include "xq_AnalyticGeometry.h"

#include <memory>

void RegisterPolyDataModelType()
{
    xq_GeometryFactory::RegisterCreator("PolyData", []() -> std::unique_ptr<xq_VascularGeometry> {
        return std::make_unique<xq_PolyGeometry>();
    });

    xq_GeometryFactory::RegisterCreator("Analytic", []() -> std::unique_ptr<xq_VascularGeometry> {
        return std::make_unique<xq_AnalyticGeometry>();
    });
}
