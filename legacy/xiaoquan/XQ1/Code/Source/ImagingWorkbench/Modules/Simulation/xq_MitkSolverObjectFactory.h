#pragma once

#include <xqModuleSimulationExports.h>

#include <mitkCoreObjectFactoryBase.h>

#include <memory>

class xq_MitkSolverJobIO;

class XQMODULESIMULATION_EXPORT xq_MitkSolverObjectFactory
    : public mitk::CoreObjectFactoryBase
{
public:
    mitkClassMacro(xq_MitkSolverObjectFactory, mitk::CoreObjectFactoryBase)
    itkFactorylessNewMacro(Self)

    mitk::Mapper::Pointer CreateMapper(mitk::DataNode* node,
                                       MapperSlotId slotId) override;
    void SetDefaultProperties(mitk::DataNode* node) override;
    [[nodiscard]] std::string GetFileExtensions() override;
    [[nodiscard]] mitk::CoreObjectFactoryBase::MultimapType GetFileExtensionsMap() override;
    [[nodiscard]] std::string GetSaveFileExtensions() override;
    [[nodiscard]] mitk::CoreObjectFactoryBase::MultimapType GetSaveFileExtensionsMap() override;

protected:
    xq_MitkSolverObjectFactory();
    ~xq_MitkSolverObjectFactory() override = default;

    void CreateFileExtensionsMap();

    MultimapType m_FileExtensionsMap;
    MultimapType m_SaveFileExtensionsMap;
};

struct XQMODULESIMULATION_EXPORT RegisterXqSolverObjectFactory
{
    RegisterXqSolverObjectFactory();
    ~RegisterXqSolverObjectFactory();

    xq_MitkSolverObjectFactory::Pointer m_Factory;
    std::unique_ptr<xq_MitkSolverJobIO> m_SimJobIO;
};
