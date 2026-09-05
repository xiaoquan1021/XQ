#include "xq_SegmentationObjectFactory.h"
#include "xq_LumenSurface.h"
#include "xq_LumenSegIO.h"
#include "xq_ContourGroupIO.h"
#include "xq_MitkSeg3D.h"
#include "xq_MitkSeg3DIO.h"
#include "xq_MitkSeg3DVtkMapper3D.h"
#include "xq_ProfileGroup.h"
#include "xq_ProfileRenderer2D.h"
#include "xq_ProfileRenderer3D.h"
#include "xq_SurfaceRenderer3D.h"

#include <mitkBaseRenderer.h>
#include <mitkCoreObjectFactory.h>
#include <mitkDataNode.h>
#include <mitkProperties.h>

#include <array>
#include <functional>

// ---------------------------------------------------------------------------
// xq_SegmentationObjectFactory
// ---------------------------------------------------------------------------

xq_SegmentationObjectFactory::xq_SegmentationObjectFactory()
{
    CreateFileExtensionsMap();
}

// ---------------------------------------------------------------------------
// CreateMapper – table-driven dispatch
// ---------------------------------------------------------------------------

mitk::Mapper::Pointer xq_SegmentationObjectFactory::CreateMapper(
    mitk::DataNode* node, MapperSlotId slotId)
{
    mitk::Mapper::Pointer mapper;

    if (!node || !node->GetData())
        return mapper;

    using MatchFn  = std::function<bool(mitk::BaseData*)>;
    using CreateFn = std::function<mitk::Mapper::Pointer(MapperSlotId)>;

    struct MapperEntry
    {
        MatchFn  match;
        CreateFn create;
    };

    const std::array<MapperEntry, 3> mapperTable = {{
        {
            [](mitk::BaseData* data) { return dynamic_cast<xq_ProfileGroup*>(data) != nullptr; },
            [](MapperSlotId slot) -> mitk::Mapper::Pointer {
                if (slot == mitk::BaseRenderer::Standard2D)
                    return xq_ProfileRenderer2D::New().GetPointer();
                if (slot == mitk::BaseRenderer::Standard3D)
                    return xq_ProfileRenderer3D::New().GetPointer();
                return nullptr;
            }
        },
        {
            [](mitk::BaseData* data) { return dynamic_cast<xq_MitkSeg3D*>(data) != nullptr; },
            [](MapperSlotId slot) -> mitk::Mapper::Pointer {
                if (slot == mitk::BaseRenderer::Standard3D)
                    return xq_MitkSeg3DVtkMapper3D::New().GetPointer();
                return nullptr;
            }
        },
        {
            [](mitk::BaseData* data) { return dynamic_cast<xq_LumenSurface*>(data) != nullptr; },
            [](MapperSlotId slot) -> mitk::Mapper::Pointer {
                if (slot == mitk::BaseRenderer::Standard3D)
                    return xq_SurfaceRenderer3D::New().GetPointer();
                return nullptr;
            }
        }
    }};

    for (const auto& entry : mapperTable)
    {
        if (entry.match(node->GetData()))
        {
            mapper = entry.create(slotId);
            if (mapper.IsNotNull())
                mapper->SetDataNode(node);
            return mapper;
        }
    }

    return mapper;
}

// ---------------------------------------------------------------------------
// SetDefaultProperties
// ---------------------------------------------------------------------------

void xq_SegmentationObjectFactory::SetDefaultProperties(mitk::DataNode* node)
{
    if (!node || !node->GetData())
        return;

    if (dynamic_cast<xq_ProfileGroup*>(node->GetData()))
    {
        node->SetProperty("color", mitk::ColorProperty::New(1.0f, 1.0f, 0.0f));
        node->SetProperty("opacity", mitk::FloatProperty::New(1.0f));
        node->SetProperty("contour.width", mitk::FloatProperty::New(1.0f));
        node->SetProperty("contour.show.control.points",
                           mitk::BoolProperty::New(true));
        node->SetProperty("contour.point.size", mitk::FloatProperty::New(3.0f));
        xq_ProfileRenderer2D::SetDefaultProperties(node);
        xq_ProfileRenderer3D::SetDefaultProperties(node);
    }
    else if (dynamic_cast<xq_MitkSeg3D*>(node->GetData()))
    {
        node->SetProperty("color", mitk::ColorProperty::New(0.0f, 0.8f, 0.8f));
        node->SetProperty("opacity", mitk::FloatProperty::New(0.6f));
        xq_MitkSeg3DVtkMapper3D::SetDefaultProperties(node);
    }
    else if (dynamic_cast<xq_LumenSurface*>(node->GetData()))
    {
        node->SetProperty("color", mitk::ColorProperty::New(0.8f, 0.8f, 0.0f));
        node->SetProperty("opacity", mitk::FloatProperty::New(1.0f));
        xq_SurfaceRenderer3D::SetDefaultProperties(node);
    }
}

// ---------------------------------------------------------------------------
// File extensions
// ---------------------------------------------------------------------------

std::string xq_SegmentationObjectFactory::GetFileExtensions()
{
    CreateFileExtensionsMap();

    std::string fileExtensions;
    for (const auto& [ext, desc] : m_FileExtensionsMap)
    {
        fileExtensions += ext;
        fileExtensions += ";;";
    }
    return fileExtensions;
}

xq_SegmentationObjectFactory::MultimapType
xq_SegmentationObjectFactory::GetFileExtensionsMap()
{
    return m_FileExtensionsMap;
}

std::string xq_SegmentationObjectFactory::GetSaveFileExtensions()
{
    std::string saveExtensions;
    for (const auto& [ext, desc] : m_SaveFileExtensionsMap)
    {
        saveExtensions += ext;
        saveExtensions += ";;";
    }
    return saveExtensions;
}

xq_SegmentationObjectFactory::MultimapType
xq_SegmentationObjectFactory::GetSaveFileExtensionsMap()
{
    return m_SaveFileExtensionsMap;
}

// ---------------------------------------------------------------------------
// CreateFileExtensionsMap – brace-initialized pairs
// ---------------------------------------------------------------------------

void xq_SegmentationObjectFactory::CreateFileExtensionsMap()
{
    m_FileExtensionsMap = {
        {"*.xqctgr", "XQ Contour Group File"},
        {"*.xqcg",   "XQ Contour Group CG File"},
        {"*.xqseg3d", "XQ 3D Segmentation File"}
    };

    m_SaveFileExtensionsMap = {
        {"*.xqctgr", "XQ Contour Group File"},
        {"*.xqcg",   "XQ Contour Group CG File"},
        {"*.xqseg3d", "XQ 3D Segmentation File"}
    };
}

// ---------------------------------------------------------------------------
// RegisterFactory
// ---------------------------------------------------------------------------

void xq_SegmentationObjectFactory::RegisterFactory()
{
    static bool registered = false;
    if (registered)
        return;

    auto factory = xq_SegmentationObjectFactory::New();
    mitk::CoreObjectFactory::GetInstance()->RegisterExtraFactory(factory);

    registered = true;
}

// Static registration to ensure xq_ContourGroupIO is instantiated and registered
static xq_ContourGroupIO s_ContourGroupIO;

RegisterXqSegmentationObjectFactory::RegisterXqSegmentationObjectFactory()
    : m_Factory(xq_SegmentationObjectFactory::New())
{
    mitk::CoreObjectFactory::GetInstance()->RegisterExtraFactory(m_Factory);
}

RegisterXqSegmentationObjectFactory::~RegisterXqSegmentationObjectFactory()
{
    if (m_Factory.IsNotNull())
        mitk::CoreObjectFactory::GetInstance()->UnRegisterExtraFactory(m_Factory);
}

static RegisterXqSegmentationObjectFactory s_RegisterXqSegmentationObjectFactory;
