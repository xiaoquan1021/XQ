// XQ SegmentationObjectFactory: lambda-dispatch mapper factory with pair-initialized extension maps
#pragma once

#include <xqModuleSegmentationExports.h>
#include <mitkCoreObjectFactoryBase.h>

class XQMODULESEGMENTATION_EXPORT xq_SegmentationObjectFactory : public mitk::CoreObjectFactoryBase
{
public:
    mitkClassMacro(xq_SegmentationObjectFactory, mitk::CoreObjectFactoryBase)
    itkFactorylessNewMacro(Self)

    mitk::Mapper::Pointer CreateMapper(mitk::DataNode* node, MapperSlotId slotId) override;
    void SetDefaultProperties(mitk::DataNode* node) override;
    std::string GetFileExtensions() override;
    mitk::CoreObjectFactoryBase::MultimapType GetFileExtensionsMap() override;
    std::string GetSaveFileExtensions() override;
    mitk::CoreObjectFactoryBase::MultimapType GetSaveFileExtensionsMap() override;

    static void RegisterFactory();

protected:
    xq_SegmentationObjectFactory();
    ~xq_SegmentationObjectFactory() override = default;

    void CreateFileExtensionsMap();

    MultimapType m_FileExtensionsMap;
    MultimapType m_SaveFileExtensionsMap;
};

struct XQMODULESEGMENTATION_EXPORT RegisterXqSegmentationObjectFactory
{
    RegisterXqSegmentationObjectFactory();
    ~RegisterXqSegmentationObjectFactory();

    xq_SegmentationObjectFactory::Pointer m_Factory;
};
