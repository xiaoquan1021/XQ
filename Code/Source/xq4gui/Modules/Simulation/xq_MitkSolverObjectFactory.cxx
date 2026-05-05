#include "xq_MitkSolverObjectFactory.h"
#include "xq_MitkSolverJob.h"
#include "xq_MitkSolverJobIO.h"

#include <mitkDataNode.h>
#include <mitkCoreObjectFactory.h>

xq_MitkSolverObjectFactory::xq_MitkSolverObjectFactory()
{
    CreateFileExtensionsMap();
}

mitk::Mapper::Pointer xq_MitkSolverObjectFactory::CreateMapper(
    mitk::DataNode* /*node*/, MapperSlotId /*slotId*/)
{
    return nullptr;
}

void xq_MitkSolverObjectFactory::SetDefaultProperties(mitk::DataNode* /*node*/) {}

std::string xq_MitkSolverObjectFactory::GetFileExtensions()
{
    CreateFileExtensionsMap();
    std::string result;
    for (const auto& [ext, desc] : m_FileExtensionsMap)
    {
        result += ext;
        result += ";;";
    }
    return result;
}

mitk::CoreObjectFactoryBase::MultimapType
xq_MitkSolverObjectFactory::GetFileExtensionsMap()
{
    return m_FileExtensionsMap;
}

std::string xq_MitkSolverObjectFactory::GetSaveFileExtensions()
{
    std::string result;
    for (const auto& [ext, desc] : m_SaveFileExtensionsMap)
    {
        result += ext;
        result += ";;";
    }
    return result;
}

mitk::CoreObjectFactoryBase::MultimapType
xq_MitkSolverObjectFactory::GetSaveFileExtensionsMap()
{
    return m_SaveFileExtensionsMap;
}

void xq_MitkSolverObjectFactory::CreateFileExtensionsMap()
{
    m_FileExtensionsMap.clear();
    m_SaveFileExtensionsMap.clear();

    constexpr auto kEntry = "*.xqsjb";
    constexpr auto kDesc  = "XQ Simulation Job File";

    m_FileExtensionsMap.emplace(kEntry, kDesc);
    m_SaveFileExtensionsMap.emplace(kEntry, kDesc);
}

// --- Registration helper ---

RegisterXqSolverObjectFactory::RegisterXqSolverObjectFactory()
    : m_Factory(xq_MitkSolverObjectFactory::New())
    , m_SimJobIO(std::make_unique<xq_MitkSolverJobIO>())
{
    mitk::CoreObjectFactory::GetInstance()->RegisterExtraFactory(m_Factory);
}

RegisterXqSolverObjectFactory::~RegisterXqSolverObjectFactory()
{
    mitk::CoreObjectFactory::GetInstance()->UnRegisterExtraFactory(m_Factory);
}

static RegisterXqSolverObjectFactory s_RegisterXqSolverObjectFactory;
