#include "xq_PathObjectFactory.h"
#include "xq_VesselCenterline.h"
#include "xq_CenterlineIO.h"
#include "xq_VesselTracer3D.h"
#include "xq_VesselTracer2D.h"

#include <mitkDataNode.h>
#include <mitkProperties.h>
#include <mitkCoreObjectFactory.h>
#include <mitkBaseRenderer.h>

#include <algorithm>
#include <array>
#include <numeric>
#include <string>

// XQ uses a factory-based composition approach for mapper registration (differs from SV)

namespace
{

// constexpr default property values
constexpr float kDefaultOpacity     = 1.0f;
constexpr float kDefaultLineWidth   = 1.0f;
constexpr float kDefaultTubeRadius  = 0.3f;
constexpr float kDefaultPointSize   = 1.0f;
constexpr float kDefaultSliceThick  = 1.0f;

struct DefaultProperty
{
    const char* name;
    enum { COLOR, FLOAT, BOOL } type;
    float r, g, b;  // color uses r,g,b; float uses r; bool uses r as 0/1
};

constexpr std::array kDefaultProperties = {
    DefaultProperty{"color",            DefaultProperty::COLOR, 0.0f, 1.0f, 0.0f},
    DefaultProperty{"opacity",          DefaultProperty::FLOAT, kDefaultOpacity, 0, 0},
    DefaultProperty{"line width",       DefaultProperty::FLOAT, kDefaultLineWidth, 0, 0},
    DefaultProperty{"tube radius",      DefaultProperty::FLOAT, kDefaultTubeRadius, 0, 0},
    DefaultProperty{"point size",       DefaultProperty::FLOAT, kDefaultPointSize, 0, 0},
    DefaultProperty{"path.show.control.points", DefaultProperty::BOOL, 1.0f, 0, 0},
    DefaultProperty{"use tube",         DefaultProperty::BOOL,  1.0f, 0, 0},
    DefaultProperty{"selected color r", DefaultProperty::FLOAT, 1.0f, 0, 0},
    DefaultProperty{"selected color g", DefaultProperty::FLOAT, 0.0f, 0, 0},
    DefaultProperty{"selected color b", DefaultProperty::FLOAT, 0.0f, 0, 0},
    DefaultProperty{"slice thickness",  DefaultProperty::FLOAT, kDefaultSliceThick, 0, 0},
};

} // anonymous namespace

xq_PathObjectFactory::xq_PathObjectFactory()
{
    CreateFileExtensionsMap();
}

mitk::Mapper::Pointer xq_PathObjectFactory::CreateMapper(
    mitk::DataNode* node, MapperSlotId slotId)
{
    mitk::Mapper::Pointer mapper;

    if (!node || !node->GetData())
        return mapper;

    if (dynamic_cast<xq_VesselCenterline*>(node->GetData()))
    {
        if (slotId == mitk::BaseRenderer::Standard3D)
            mapper = xq_VesselTracer3D::New();
        else if (slotId == mitk::BaseRenderer::Standard2D)
            mapper = xq_VesselTracer2D::New();

        if (mapper.IsNotNull())
            mapper->SetDataNode(node);
    }

    return mapper;
}

void xq_PathObjectFactory::SetDefaultProperties(mitk::DataNode* node)
{
    if (!node || !node->GetData())
        return;

    if (!dynamic_cast<xq_VesselCenterline*>(node->GetData()))
        return;

    // Apply defaults from the property table
    std::for_each(kDefaultProperties.cbegin(), kDefaultProperties.cend(),
        [&](const auto& prop) {
            switch (prop.type)
            {
            case DefaultProperty::COLOR:
                node->SetProperty(prop.name,
                    mitk::ColorProperty::New(prop.r, prop.g, prop.b));
                break;
            case DefaultProperty::FLOAT:
                node->SetProperty(prop.name,
                    mitk::FloatProperty::New(prop.r));
                break;
            case DefaultProperty::BOOL:
                node->SetProperty(prop.name,
                    mitk::BoolProperty::New(prop.r > 0.0f));
                break;
            }
        });
}

std::string xq_PathObjectFactory::GetFileExtensions()
{
    CreateFileExtensionsMap();

    return std::accumulate(m_FileExtensionsMap.cbegin(), m_FileExtensionsMap.cend(),
        std::string{},
        [](std::string acc, const auto& kv) {
            return std::move(acc) + kv.first + ";;";
        });
}

xq_PathObjectFactory::MultimapType xq_PathObjectFactory::GetFileExtensionsMap()
{
    return m_FileExtensionsMap;
}

std::string xq_PathObjectFactory::GetSaveFileExtensions()
{
    return std::accumulate(m_SaveFileExtensionsMap.cbegin(), m_SaveFileExtensionsMap.cend(),
        std::string{},
        [](std::string acc, const auto& kv) {
            return std::move(acc) + kv.first + ";;";
        });
}

xq_PathObjectFactory::MultimapType xq_PathObjectFactory::GetSaveFileExtensionsMap()
{
    return m_SaveFileExtensionsMap;
}

void xq_PathObjectFactory::CreateFileExtensionsMap()
{
    m_FileExtensionsMap.clear();
    m_SaveFileExtensionsMap.clear();

    m_FileExtensionsMap.insert({{"*.xqpth", "XQ Path File"},
                                {"*.pth",   "Legacy Path File"}});

    m_SaveFileExtensionsMap.insert({{"*.xqpth", "XQ Path File"}});
}

RegisterxqPathObjectFactory::RegisterxqPathObjectFactory()
    : m_Factory(xq_PathObjectFactory::New())
    , m_PathIO(std::make_unique<xq_CenterlineIO>())
{
    mitk::CoreObjectFactory::GetInstance()->RegisterExtraFactory(m_Factory);
}

RegisterxqPathObjectFactory::~RegisterxqPathObjectFactory()
{
    mitk::CoreObjectFactory::GetInstance()->UnRegisterExtraFactory(m_Factory);
}

static RegisterxqPathObjectFactory s_RegisterxqPathObjectFactory;
