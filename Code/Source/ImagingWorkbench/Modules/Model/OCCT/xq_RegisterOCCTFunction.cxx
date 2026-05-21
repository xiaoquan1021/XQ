#include "xq_RegisterOCCTFunction.h"
#include "xq_OCCTGeometry.h"
#include "xq_GeometryFactory.h"

void RegisterOCCTModelType()
{
    xq_GeometryFactory::RegisterCreator("OCCT", []() -> std::unique_ptr<xq_VascularGeometry> {
        return std::make_unique<xq_OCCTGeometry>();
    });
}
