#pragma once

#include <xqModulePathExports.h>

#include <mitkCoreObjectFactoryBase.h>

#include <memory>

class xq_CenterlineIO;

class XQMODULEPATH_EXPORT xq_PathObjectFactory : public mitk::CoreObjectFactoryBase
{
public:
    mitkClassMacro(xq_PathObjectFactory, mitk::CoreObjectFactoryBase)
    itkFactorylessNewMacro(Self)

    mitk::Mapper::Pointer CreateMapper(mitk::DataNode* node, MapperSlotId slotId) override;
    void SetDefaultProperties(mitk::DataNode* node) override;
    [[nodiscard]] std::string GetFileExtensions() override;
    [[nodiscard]] MultimapType GetFileExtensionsMap() override;
    [[nodiscard]] std::string GetSaveFileExtensions() override;
    [[nodiscard]] MultimapType GetSaveFileExtensionsMap() override;

protected:
    xq_PathObjectFactory();
    ~xq_PathObjectFactory() override = default;

    void CreateFileExtensionsMap();

    MultimapType m_FileExtensionsMap;
    MultimapType m_SaveFileExtensionsMap;
};

struct XQMODULEPATH_EXPORT RegisterxqPathObjectFactory
{
    RegisterxqPathObjectFactory();
    ~RegisterxqPathObjectFactory();

    xq_PathObjectFactory::Pointer m_Factory;
    std::unique_ptr<xq_CenterlineIO> m_PathIO;
};

