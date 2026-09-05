#include "xq_MitkGridObjectFactory.h"
#include "xq_MitkGrid.h"
#include "xq_MitkGridMapper2D.h"
#include "xq_MitkGridMapper3D.h"
#include "xq_MitkGridIO.h"

#include <mitkDataNode.h>
#include <mitkProperties.h>
#include <mitkCoreObjectFactory.h>

#include <cstring>
#include <functional>
#include <unordered_map>

namespace
{

struct DefaultGridProperties
{
    const char* key;
    std::function<mitk::BaseProperty::Pointer()> create;
};

constexpr auto kGridColor = 0.0f;
constexpr auto kGridGreen = 0.8f;
constexpr auto kGridBlue  = 0.2f;

const DefaultGridProperties kDefaultProps[] = {
    {"color",          []() { return mitk::ColorProperty::New(kGridColor, kGridGreen, kGridBlue).GetPointer(); }},
    {"opacity",        []() { return mitk::FloatProperty::New(1.0f).GetPointer(); }},
    {"visible",        []() { return mitk::BoolProperty::New(true).GetPointer(); }},
    {"mesh.wireframe", []() { return mitk::BoolProperty::New(false).GetPointer(); }},
};

inline std::string BuildExtensionString(const mitk::CoreObjectFactoryBase::MultimapType& extMap)
{
    std::string result;
    for (const auto& [ext, desc] : extMap)
    {
        if (!result.empty()) result += ' ';
        result += ext;
    }
    return result;
}

const mitk::CoreObjectFactoryBase::MultimapType& GetXqMeshExtensions()
{
    static const mitk::CoreObjectFactoryBase::MultimapType extensions = {
        {"*.xqmsh", "XQ Mesh Files"}
    };
    return extensions;
}

class XqMeshRegistrationGuard
{
public:
    static XqMeshRegistrationGuard& Instance()
    {
        static XqMeshRegistrationGuard guard;
        return guard;
    }

    bool IsRegistered() const { return m_Registered; }

private:
    XqMeshRegistrationGuard()
    {
        auto factory = xq_MitkGridObjectFactory::New();
        mitk::CoreObjectFactory::GetInstance()->RegisterExtraFactory(factory);
        m_Registered = true;
    }

    ~XqMeshRegistrationGuard() = default;

    bool m_Registered = false;
};

} // anonymous namespace

xq_MitkGridObjectFactory::xq_MitkGridObjectFactory()
{
    const auto& exts = GetXqMeshExtensions();
    m_FileExtensionsMap = exts;
    m_SaveFileExtensionsMap = exts;
}

mitk::Mapper::Pointer xq_MitkGridObjectFactory::CreateMapper(
    mitk::DataNode* node, MapperSlotId slotId)
{
    using MapperFactory = std::function<mitk::Mapper::Pointer()>;
    static const std::unordered_map<MapperSlotId, MapperFactory> mapperCreators = {
        {mitk::BaseRenderer::Standard2D, []() { return xq_MitkGridMapper2D::New().GetPointer(); }},
        {mitk::BaseRenderer::Standard3D, []() { return xq_MitkGridMapper3D::New().GetPointer(); }}
    };

    if (!node || !dynamic_cast<xq_MitkGrid*>(node->GetData()))
        return nullptr;

    auto it = mapperCreators.find(slotId);
    if (it == mapperCreators.end())
        return nullptr;

    auto mapper = it->second();
    if (mapper.IsNotNull())
        mapper->SetDataNode(node);

    return mapper;
}

void xq_MitkGridObjectFactory::SetDefaultProperties(mitk::DataNode* node)
{
    if (!node || !node->GetData())
        return;

    if (std::strcmp(node->GetData()->GetNameOfClass(), "xq_MitkGrid") != 0)
        return;

    // Apply defaults inline rather than through std::function lambdas
    // to avoid a static-initialisation-order issue where the function
    // targets inside kDefaultProps can be null when the factory is
    // first invoked across a shared-library boundary.
    node->SetProperty("color",          mitk::ColorProperty::New(0.0f, 0.8f, 0.2f));
    node->SetProperty("opacity",        mitk::FloatProperty::New(1.0f));
    node->SetProperty("visible",        mitk::BoolProperty::New(true));
    node->SetProperty("mesh.wireframe", mitk::BoolProperty::New(false));
}

std::string xq_MitkGridObjectFactory::GetFileExtensions()
{
    return BuildExtensionString(m_FileExtensionsMap);
}

mitk::CoreObjectFactoryBase::MultimapType xq_MitkGridObjectFactory::GetFileExtensionsMap()
{
    return m_FileExtensionsMap;
}

std::string xq_MitkGridObjectFactory::GetSaveFileExtensions()
{
    return BuildExtensionString(m_SaveFileExtensionsMap);
}

mitk::CoreObjectFactoryBase::MultimapType xq_MitkGridObjectFactory::GetSaveFileExtensionsMap()
{
    return m_SaveFileExtensionsMap;
}

void xq_MitkGridObjectFactory::CreateFileExtensionsMap()
{
    // Extensions now initialized in constructor via GetXqMeshExtensions()
}

void RegisterXqMeshObjectFactory()
{
    XqMeshRegistrationGuard::Instance();
}

// XQ fix: previously RegisterXqMeshObjectFactory() was declared but never
// called, so no mapper was ever assigned to xq_MitkGrid data — volume
// meshes silently produced no visible output regardless of the Data Manager
// checkbox state. The static trigger below forces registration at library
// load time, matching the pattern used by xq_PathObjectFactory.
namespace
{
struct XqMeshFactoryAutoRegister
{
    XqMeshFactoryAutoRegister()
    {
        RegisterXqMeshObjectFactory();
    }
};
static const XqMeshFactoryAutoRegister s_XqMeshFactoryAutoRegister;
}
