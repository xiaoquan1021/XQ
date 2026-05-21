#pragma once

#include <xqModelCommonExports.h>

#include <mitkCoreObjectFactoryBase.h>

#include <map>
#include <string>

class XQMODELCOMMON_EXPORT xq_ModelObjectFactory : public mitk::CoreObjectFactoryBase
{
public:
    mitkClassMacro(xq_ModelObjectFactory, mitk::CoreObjectFactoryBase)
    itkFactorylessNewMacro(Self)

    mitk::Mapper::Pointer CreateMapper(mitk::DataNode* node, MapperSlotId slotId) override;
    void SetDefaultProperties(mitk::DataNode* node) override;

    [[nodiscard]] std::string GetFileExtensions() override;
    [[nodiscard]] mitk::CoreObjectFactoryBase::MultimapType GetFileExtensionsMap() override;

    [[nodiscard]] std::string GetSaveFileExtensions() override;
    [[nodiscard]] mitk::CoreObjectFactoryBase::MultimapType GetSaveFileExtensionsMap() override;

protected:
    xq_ModelObjectFactory();
    ~xq_ModelObjectFactory() override = default;

    void CreateFileExtensionMaps();

private:
    bool m_AlreadyRegistered = false;
    std::multimap<std::string, std::string> m_FileExtensionsMap;
    std::multimap<std::string, std::string> m_SaveFileExtensionsMap;
};

XQMODELCOMMON_EXPORT void RegisterXqModelObjectFactory();
