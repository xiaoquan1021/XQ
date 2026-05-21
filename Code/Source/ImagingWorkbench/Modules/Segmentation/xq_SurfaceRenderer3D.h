// XQ SurfaceRenderer3D: normal-computed surface rendering with configurable representation
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkVtkMapper.h>
#include <mitkLocalStorageHandler.h>

#include <vtkSmartPointer.h>

#include <array>

class vtkActor;
class vtkPolyDataMapper;
class vtkPolyDataNormals;

class XQMODULESEGMENTATION_EXPORT xq_SurfaceRenderer3D : public mitk::VtkMapper
{
public:
    mitkClassMacro(xq_SurfaceRenderer3D, mitk::VtkMapper)
    itkFactorylessNewMacro(Self)

    vtkProp* GetVtkProp(mitk::BaseRenderer* renderer) override;
    static void SetDefaultProperties(mitk::DataNode* node, mitk::BaseRenderer* renderer = nullptr, bool overwrite = false);

    class LocalStorage : public mitk::Mapper::BaseLocalStorage
    {
    public:
        LocalStorage();
        ~LocalStorage() override = default;
        vtkSmartPointer<vtkActor> m_Actor;
        vtkSmartPointer<vtkPolyDataMapper> m_Mapper;
        vtkSmartPointer<vtkPolyDataNormals> m_Normals;
        itk::TimeStamp m_LastUpdateTime;
    };

    mitk::LocalStorageHandler<LocalStorage> m_LSH;

protected:
    xq_SurfaceRenderer3D() = default;
    ~xq_SurfaceRenderer3D() override = default;
    void GenerateDataForRenderer(mitk::BaseRenderer* renderer) override;
    void ResetMapper(mitk::BaseRenderer* renderer) override;

private:
    struct RenderProperties
    {
        std::array<float, 3> color = {1.0f, 1.0f, 1.0f};
        std::array<float, 3> edgeColor = {0.0f, 0.0f, 0.0f};
        float opacity = 1.0f;
        int representation = 2;
        bool edgeVisibility = false;
    };

    static RenderProperties fetchRenderProperties(const mitk::DataNode* node, mitk::BaseRenderer* renderer);
    static void applyRepresentation(vtkProperty* prop, int representation);
};
