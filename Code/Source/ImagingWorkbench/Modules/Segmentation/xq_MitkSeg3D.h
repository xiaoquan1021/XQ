// XQ MitkSeg3D: volumetric segmentation data with constexpr method-name lookup
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkBaseData.h>

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>

#include <array>
#include <string_view>
#include <vector>

class XQMODULESEGMENTATION_EXPORT xq_MitkSeg3D : public mitk::BaseData
{
public:
    mitkClassMacro(xq_MitkSeg3D, mitk::BaseData)
    itkFactorylessNewMacro(Self)

    enum class Seg3DMethod { THRESHOLD = 0, REGION_GROWING, LEVEL_SET, COLLIDING_FRONTS };

    static constexpr std::array<const char*, 4> kMethodNames = {
        "Threshold", "Region Growing", "Level Set", "Colliding Fronts"
    };

    void SetSurfaceMesh(vtkSmartPointer<vtkPolyData> pd);
    vtkSmartPointer<vtkPolyData> GetSurfaceMesh() const;

    void SetMethod(Seg3DMethod method);
    Seg3DMethod GetMethod() const;
    std::string_view GetMethodString() const;

    void AddSeedPoint(const mitk::Point3D& pt);
    void ClearSeedPoints();
    std::vector<mitk::Point3D> GetSeedPoints() const;

    void SetUpperThreshold(double val);
    void SetLowerThreshold(double val);
    double GetUpperThreshold() const;
    double GetLowerThreshold() const;

    void SetRequestedRegionToLargestPossibleRegion() override;
    bool RequestedRegionIsOutsideOfTheBufferedRegion() override;
    bool VerifyRequestedRegion() override;
    void SetRequestedRegion(const itk::DataObject* data) override;
    void UpdateOutputInformation() override;

    void ExecuteOperation(mitk::Operation* operation) override;

protected:
    xq_MitkSeg3D();
    xq_MitkSeg3D(const xq_MitkSeg3D& other);
    ~xq_MitkSeg3D() override = default;

    vtkSmartPointer<vtkPolyData> m_PolyData = nullptr;
    Seg3DMethod m_Method = Seg3DMethod::THRESHOLD;
    std::vector<mitk::Point3D> m_SeedPoints;
    double m_UpperThreshold = 1000.0;
    double m_LowerThreshold = 0.0;
};
