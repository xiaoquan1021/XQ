// XQ ContourModelVtkMapper2D: threshold-contour overlay with plane-distance culling
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkVtkMapper.h>
#include <mitkLocalStorageHandler.h>

#include <vtkSmartPointer.h>

#include <array>

class vtkActor;
class vtkPolyDataMapper;
class vtkPolyData;
class vtkPoints;
class vtkCellArray;

class XQMODULESEGMENTATION_EXPORT xq_ContourModelVtkMapper2D : public mitk::VtkMapper
{
public:
    mitkClassMacro(xq_ContourModelVtkMapper2D, mitk::VtkMapper)
    itkFactorylessNewMacro(Self)

    vtkProp* GetVtkProp(mitk::BaseRenderer* renderer) override;
    static void SetDefaultProperties(mitk::DataNode* node, mitk::BaseRenderer* renderer = nullptr, bool overwrite = false);

    class LocalStorage : public mitk::Mapper::BaseLocalStorage
    {
    public:
        LocalStorage();
        ~LocalStorage() override = default;
        vtkSmartPointer<vtkActor>          m_Actor;
        vtkSmartPointer<vtkPolyDataMapper> m_Mapper;
        vtkSmartPointer<vtkPolyData>       m_PolyData;
        vtkSmartPointer<vtkPoints>         m_Points;
        vtkSmartPointer<vtkCellArray>      m_Lines;
        itk::TimeStamp                     m_LastUpdateTime;
    };

    mitk::LocalStorageHandler<LocalStorage> m_LSH;

protected:
    xq_ContourModelVtkMapper2D() = default;
    ~xq_ContourModelVtkMapper2D() override = default;
    void GenerateDataForRenderer(mitk::BaseRenderer* renderer) override;
    void ResetMapper(mitk::BaseRenderer* renderer) override;

private:
    struct RenderProperties
    {
        std::array<float, 3> color = {0.0f, 1.0f, 0.0f};
        float opacity = 1.0f;
        float lineWidth = 2.0f;
    };

    static RenderProperties fetchRenderProperties(const mitk::DataNode* node, mitk::BaseRenderer* renderer);
    static void buildClosedPolyLine(const std::vector<vtkIdType>& ids, vtkCellArray* lines, bool closed);
};
