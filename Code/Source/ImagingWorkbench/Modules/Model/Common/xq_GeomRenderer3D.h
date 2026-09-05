#pragma once

#include <xqModelCommonExports.h>

#include <mitkVtkMapper.h>
#include <mitkLocalStorageHandler.h>

#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkLookupTable.h>

#include <itkTimeStamp.h>

class XQMODELCOMMON_EXPORT xq_GeomRenderer3D : public mitk::VtkMapper
{
public:
    mitkClassMacro(xq_GeomRenderer3D, mitk::VtkMapper)
    itkFactorylessNewMacro(Self)

    class LocalStorage : public mitk::Mapper::BaseLocalStorage
    {
    public:
        vtkSmartPointer<vtkActor> m_Actor;
        vtkSmartPointer<vtkPolyDataMapper> m_Mapper;
        vtkSmartPointer<vtkLookupTable> m_FaceLUT;
        itk::TimeStamp m_LastUpdateTime;

        LocalStorage();
        ~LocalStorage() override = default;
    };

    mitk::LocalStorageHandler<LocalStorage> m_LSH;

    vtkProp* GetVtkProp(mitk::BaseRenderer* renderer) override;
    void GenerateDataForRenderer(mitk::BaseRenderer* renderer) override;
    void ApplyColorAndOpacityProperties(mitk::BaseRenderer* renderer, vtkActor* actor) override;

protected:
    xq_GeomRenderer3D() = default;
    ~xq_GeomRenderer3D() override = default;
};
