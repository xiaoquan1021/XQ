#pragma once

#include <xqMeshCommonExports.h>

#include <mitkVtkMapper.h>
#include <mitkLocalStorageHandler.h>

#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkDataSetMapper.h>
#include <vtkTimeStamp.h>

class XQMESHCOMMON_EXPORT xq_MitkGridMapper3D : public mitk::VtkMapper
{
public:
    mitkClassMacro(xq_MitkGridMapper3D, mitk::VtkMapper);
    itkFactorylessNewMacro(Self);

    class LocalStorage : public mitk::Mapper::BaseLocalStorage
    {
    public:
        LocalStorage();
        ~LocalStorage() override = default;

        vtkSmartPointer<vtkActor> m_Actor;
        vtkSmartPointer<vtkDataSetMapper> m_Mapper;
        vtkTimeStamp m_LastUpdateTime;
    };

    [[nodiscard]] vtkProp* GetVtkProp(mitk::BaseRenderer* renderer) override;
    void GenerateDataForRenderer(mitk::BaseRenderer* renderer) override;
    void ApplyColorAndOpacityProperties(mitk::BaseRenderer* renderer, vtkActor* actor) override;

    mitk::LocalStorageHandler<LocalStorage> m_LSH;

protected:
    xq_MitkGridMapper3D() = default;
    ~xq_MitkGridMapper3D() override = default;
};
