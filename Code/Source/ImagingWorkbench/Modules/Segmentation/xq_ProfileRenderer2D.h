// XQ ProfileRenderer2D: slice-plane contour visualization with lambda-based projection
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkVtkMapper.h>
#include <mitkLocalStorageHandler.h>

#include <vtkSmartPointer.h>

#include <array>

class vtkPropAssembly;
class vtkActor;
class vtkPolyDataMapper;
class vtkCellArray;
class vtkPoints;

class XQMODULESEGMENTATION_EXPORT xq_ProfileRenderer2D : public mitk::VtkMapper
{
public:
    mitkClassMacro(xq_ProfileRenderer2D, mitk::VtkMapper)
    itkFactorylessNewMacro(Self)

    vtkProp* GetVtkProp(mitk::BaseRenderer* renderer) override;
    static void SetDefaultProperties(mitk::DataNode* node, mitk::BaseRenderer* renderer = nullptr, bool overwrite = false);

    class LocalStorage : public mitk::Mapper::BaseLocalStorage
    {
    public:
        LocalStorage();
        ~LocalStorage() override = default;
        vtkSmartPointer<vtkPropAssembly> m_Assembly;
        vtkSmartPointer<vtkActor> m_ContourActor;
        vtkSmartPointer<vtkPolyDataMapper> m_ContourMapper;
        vtkSmartPointer<vtkActor> m_PointsActor;
        vtkSmartPointer<vtkPolyDataMapper> m_PointsMapper;
        itk::TimeStamp m_LastUpdateTime;
    };

    mitk::LocalStorageHandler<LocalStorage> m_LSH;

protected:
    xq_ProfileRenderer2D() = default;
    ~xq_ProfileRenderer2D() override = default;
    void GenerateDataForRenderer(mitk::BaseRenderer* renderer) override;
    void ResetMapper(mitk::BaseRenderer* renderer) override;

private:
    struct RenderProperties
    {
        std::array<float, 3> color = {0.0f, 1.0f, 0.0f};
        std::array<float, 3> selectedColor = {1.0f, 0.0f, 0.0f};
        float opacity = 1.0f;
        float lineWidth = 2.0f;
        float pointSize = 4.0f;
    };

    RenderProperties fetchRenderProperties(const mitk::DataNode* node, mitk::BaseRenderer* renderer) const;
    static void buildClosedPolyLine(const std::vector<vtkIdType>& ids, vtkCellArray* lines);
};
