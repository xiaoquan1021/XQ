#pragma once

#include <xqMeshCommonExports.h>

#include <mitkVtkMapper.h>
#include <mitkLocalStorageHandler.h>

#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkCutter.h>
#include <vtkPlane.h>
#include <vtkDataSetSurfaceFilter.h>
#include <vtkTimeStamp.h>

class XQMESHCOMMON_EXPORT xq_MitkGridMapper2D : public mitk::VtkMapper
{
public:
    mitkClassMacro(xq_MitkGridMapper2D, mitk::VtkMapper);
    itkFactorylessNewMacro(Self);

    class LocalStorage : public mitk::Mapper::BaseLocalStorage
    {
    public:
        LocalStorage();
        ~LocalStorage() override = default;

        vtkSmartPointer<vtkActor> m_Actor;
        vtkSmartPointer<vtkPolyDataMapper> m_Mapper;
        vtkSmartPointer<vtkCutter> m_Cutter;
        vtkSmartPointer<vtkPlane> m_CuttingPlane;
        vtkSmartPointer<vtkDataSetSurfaceFilter> m_SurfaceFilter;
        vtkTimeStamp m_LastUpdateTime;
    };

    [[nodiscard]] vtkProp* GetVtkProp(mitk::BaseRenderer* renderer) override;
    void GenerateDataForRenderer(mitk::BaseRenderer* renderer) override;

    mitk::LocalStorageHandler<LocalStorage> m_LSH;

protected:
    xq_MitkGridMapper2D() = default;
    ~xq_MitkGridMapper2D() override = default;
};
