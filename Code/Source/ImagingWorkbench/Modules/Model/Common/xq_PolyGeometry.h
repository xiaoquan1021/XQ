#pragma once

#include <xqModelCommonExports.h>

#include "xq_VascularGeometry.h"

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkIntArray.h>

class XQMODELCOMMON_EXPORT xq_PolyGeometry : public xq_VascularGeometry
{
public:
    xq_PolyGeometry();
    ~xq_PolyGeometry() override = default;

    std::unique_ptr<xq_VascularGeometry> Clone() const override;

    void SetWholeVtkPolyData(vtkSmartPointer<vtkPolyData> polyData);
    void AssignFaceIds(vtkIntArray* faceIds);
    [[nodiscard]] vtkSmartPointer<vtkPolyData> ExtractFace(int faceId);

    vtkSmartPointer<vtkPolyData> GetWholeVtkPolyData() override;
    vtkSmartPointer<vtkPolyData> GetFaceVtkPolyData(int faceId) override;

private:
    vtkSmartPointer<vtkPolyData> m_WholePolyData;
};
