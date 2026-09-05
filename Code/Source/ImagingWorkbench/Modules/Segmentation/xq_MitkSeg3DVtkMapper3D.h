// XQ MitkSeg3DVtkMapper3D: dual-layer 3D segmentation display (surface + seed glyphs)
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkVtkMapper.h>
#include <mitkLocalStorageHandler.h>

#include <vtkSmartPointer.h>

#include <array>

class vtkPropAssembly;
class vtkActor;
class vtkPolyDataMapper;
class vtkPolyDataNormals;

class XQMODULESEGMENTATION_EXPORT xq_MitkSeg3DVtkMapper3D : public mitk::VtkMapper
{
public:
    mitkClassMacro(xq_MitkSeg3DVtkMapper3D, mitk::VtkMapper)
    itkFactorylessNewMacro(Self)

    vtkProp* GetVtkProp(mitk::BaseRenderer* renderer) override;
    static void SetDefaultProperties(mitk::DataNode* node, mitk::BaseRenderer* renderer = nullptr, bool overwrite = false);

    class LocalStorage : public mitk::Mapper::BaseLocalStorage
    {
    public:
        LocalStorage();
        ~LocalStorage() override = default;
        vtkSmartPointer<vtkPropAssembly> m_Assembly;
        vtkSmartPointer<vtkActor> m_SurfaceActor;
        vtkSmartPointer<vtkPolyDataMapper> m_SurfaceMapper;
        vtkSmartPointer<vtkPolyDataNormals> m_Normals;
        vtkSmartPointer<vtkActor> m_SeedPointsActor;
        vtkSmartPointer<vtkPolyDataMapper> m_SeedPointsMapper;
        itk::TimeStamp m_LastUpdateTime;
    };

    mitk::LocalStorageHandler<LocalStorage> m_LSH;

protected:
    xq_MitkSeg3DVtkMapper3D() = default;
    ~xq_MitkSeg3DVtkMapper3D() override = default;
    void GenerateDataForRenderer(mitk::BaseRenderer* renderer) override;
    void ResetMapper(mitk::BaseRenderer* renderer) override;

private:
    struct RenderProperties
    {
        std::array<float, 3> color = {0.8f, 0.8f, 0.2f};
        std::array<float, 3> edgeColor = {0.2f, 0.2f, 0.2f};
        float opacity = 0.8f;
        float seedRadius = 1.5f;
        bool edgeVisibility = true;
    };

    static RenderProperties fetchRenderProperties(const mitk::DataNode* node, mitk::BaseRenderer* renderer);
};
