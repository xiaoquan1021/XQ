#pragma once

#include <xqMeshCommonExports.h>

#include <mitkCoreObjectFactoryBase.h>

class XQMESHCOMMON_EXPORT xq_MitkGridObjectFactory : public mitk::CoreObjectFactoryBase
{
public:
    mitkClassMacro(xq_MitkGridObjectFactory, mitk::CoreObjectFactoryBase);
    itkFactorylessNewMacro(Self);

    [[nodiscard]] mitk::Mapper::Pointer CreateMapper(mitk::DataNode* node, MapperSlotId slotId) override;
    void SetDefaultProperties(mitk::DataNode* node) override;
    [[nodiscard]] std::string GetFileExtensions() override;
    [[nodiscard]] mitk::CoreObjectFactoryBase::MultimapType GetFileExtensionsMap() override;
    [[nodiscard]] std::string GetSaveFileExtensions() override;
    [[nodiscard]] mitk::CoreObjectFactoryBase::MultimapType GetSaveFileExtensionsMap() override;

protected:
    xq_MitkGridObjectFactory();
    ~xq_MitkGridObjectFactory() override = default;

    void CreateFileExtensionsMap();

    MultimapType m_FileExtensionsMap;
    MultimapType m_SaveFileExtensionsMap;
};

XQMESHCOMMON_EXPORT void RegisterXqMeshObjectFactory();
