#pragma once

// XQ composes VTK pipeline objects rather than deep mapper inheritance (differs from SV)

#include <xqModulePathExports.h>

#include <mitkVtkMapper.h>
#include <mitkLocalStorageHandler.h>

#include <vtkSmartPointer.h>
#include <vtkPropAssembly.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>

class XQMODULEPATH_EXPORT xq_VesselTracer2D : public mitk::VtkMapper
{
public:
    mitkClassMacro(xq_VesselTracer2D, mitk::VtkMapper)
    itkFactorylessNewMacro(Self)

    [[nodiscard]] vtkProp* GetVtkProp(mitk::BaseRenderer* renderer) override;

    class LocalStorage : public mitk::Mapper::BaseLocalStorage
    {
    public:
        LocalStorage();
        ~LocalStorage() override = default;

        vtkSmartPointer<vtkPropAssembly>    m_Assembly     = vtkSmartPointer<vtkPropAssembly>::New();
        vtkSmartPointer<vtkActor>           m_PathActor    = vtkSmartPointer<vtkActor>::New();
        vtkSmartPointer<vtkPolyDataMapper>  m_PathMapper   = vtkSmartPointer<vtkPolyDataMapper>::New();
        vtkSmartPointer<vtkActor>           m_PointsActor  = vtkSmartPointer<vtkActor>::New();
        vtkSmartPointer<vtkPolyDataMapper>  m_PointsMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        itk::TimeStamp                      m_LastUpdateTime;
    };

    mitk::LocalStorageHandler<LocalStorage> m_LSH;

protected:
    xq_VesselTracer2D() = default;
    ~xq_VesselTracer2D() override = default;

    void GenerateDataForRenderer(mitk::BaseRenderer* renderer) override;
    void ResetMapper(mitk::BaseRenderer* renderer) override;
};
