#pragma once

#include <xqModelCommonExports.h>

#include <mitkVtkMapper.h>
#include <mitkLocalStorageHandler.h>

#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkCutter.h>
#include <vtkPlane.h>

#include <itkTimeStamp.h>

class XQMODELCOMMON_EXPORT xq_GeomRenderer2D : public mitk::VtkMapper
{
public:
    mitkClassMacro(xq_GeomRenderer2D, mitk::VtkMapper)
    itkFactorylessNewMacro(Self)

    class LocalStorage : public mitk::Mapper::BaseLocalStorage
    {
    public:
        vtkSmartPointer<vtkActor> m_Actor;
        vtkSmartPointer<vtkPolyDataMapper> m_Mapper;
        vtkSmartPointer<vtkCutter> m_Cutter;
        vtkSmartPointer<vtkPlane> m_CuttingPlane;
        itk::TimeStamp m_LastUpdateTime;

        LocalStorage();
        ~LocalStorage() override = default;
    };

    mitk::LocalStorageHandler<LocalStorage> m_LSH;

    vtkProp* GetVtkProp(mitk::BaseRenderer* renderer) override;
    void GenerateDataForRenderer(mitk::BaseRenderer* renderer) override;

protected:
    xq_GeomRenderer2D() = default;
    ~xq_GeomRenderer2D() override = default;
};
