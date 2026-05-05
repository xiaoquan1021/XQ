#pragma once

#include <xqModelCommonExports.h>

#include "xq_VascularGeometry.h"

#include <array>

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>

class XQMODELCOMMON_EXPORT xq_AnalyticGeometry : public xq_VascularGeometry
{
public:
    enum ShapeType
    {
        Cylinder = 0,
        Sphere,
        Box,
        Ellipsoid
    };

    xq_AnalyticGeometry();
    ~xq_AnalyticGeometry() override = default;

    std::unique_ptr<xq_VascularGeometry> Clone() const override;

    void CreateCylinder(double center[3], double axis[3], double radius, double length);
    void CreateSphere(double center[3], double radius);
    void CreateBox(double center[3], double dims[3]);

    vtkSmartPointer<vtkPolyData> GetWholeVtkPolyData() override;
    vtkSmartPointer<vtkPolyData> GetFaceVtkPolyData(int faceId) override;

    [[nodiscard]] ShapeType GetShapeType() const { return m_ShapeType; }

private:
    ShapeType m_ShapeType = Cylinder;
    std::array<double, 6> m_Params = {};
    vtkSmartPointer<vtkPolyData> m_PolyData;
};
