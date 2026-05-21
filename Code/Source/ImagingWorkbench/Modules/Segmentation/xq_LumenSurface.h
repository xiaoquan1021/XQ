// XQ LumenSurface: timestep-indexed surface data with factored mass-property computation
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkBaseData.h>

#include <vtkMassProperties.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <map>

class XQMODULESEGMENTATION_EXPORT xq_LumenSurface : public mitk::BaseData
{
public:
    mitkClassMacro(xq_LumenSurface, mitk::BaseData)
    itkFactorylessNewMacro(Self)
    itkCloneMacro(Self)

    void SetSurfaceMesh(vtkSmartPointer<vtkPolyData> polyData, unsigned int timeStep = 0);
    vtkSmartPointer<vtkPolyData> GetSurfaceMesh(unsigned int timeStep = 0) const;

    double GetSurfaceArea(unsigned int timeStep = 0) const;
    double GetVolume(unsigned int timeStep = 0) const;

    void SetRequestedRegionToLargestPossibleRegion() override;
    bool RequestedRegionIsOutsideOfTheBufferedRegion() override;
    bool VerifyRequestedRegion() override;
    void SetRequestedRegion(const itk::DataObject* data) override;
    void UpdateOutputInformation() override;
    void ClearData() override;
    void InitializeEmpty() override;

protected:
    xq_LumenSurface();
    ~xq_LumenSurface() override = default;

private:
    static auto computeMassProperties(vtkPolyData* pd) -> vtkSmartPointer<vtkMassProperties>;

    std::map<unsigned int, vtkSmartPointer<vtkPolyData>> m_MeshSeries;
};
