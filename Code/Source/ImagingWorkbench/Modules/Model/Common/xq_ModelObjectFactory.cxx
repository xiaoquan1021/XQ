#include "xq_ModelObjectFactory.h"
#include "xq_Model.h"
#include "xq_GeomRenderer2D.h"
#include "xq_GeomRenderer3D.h"

#include <mitkCoreObjectFactory.h>
#include <mitkDataNode.h>
#include <mitkProperties.h>
#include <mitkColorProperty.h>
#include <mitkVtkRepresentationProperty.h>
#include <mitkVtkInterpolationProperty.h>

xq_ModelObjectFactory::xq_ModelObjectFactory()
{
    CreateFileExtensionMaps();
}

mitk::Mapper::Pointer xq_ModelObjectFactory::CreateMapper(mitk::DataNode* node,
                                                           MapperSlotId slotId)
{
    mitk::Mapper::Pointer newMapper;

    if (!node || !node->GetData())
        return newMapper;

    if (auto* model = dynamic_cast<xq_Model*>(node->GetData()))
    {
        if (slotId == mitk::BaseRenderer::Standard2D)
        {
            newMapper = xq_GeomRenderer2D::New();
        }
        else if (slotId == mitk::BaseRenderer::Standard3D)
        {
            newMapper = xq_GeomRenderer3D::New();
        }

        if (newMapper)
        {
            newMapper->SetDataNode(node);
        }
    }

    return newMapper;
}

void xq_ModelObjectFactory::SetDefaultProperties(mitk::DataNode* node)
{
    if (!node || !node->GetData())
        return;

    if (!dynamic_cast<xq_Model*>(node->GetData()))
        return;

    node->SetProperty("visible", mitk::BoolProperty::New(true));
    node->SetProperty("color", mitk::ColorProperty::New(1.0f, 1.0f, 1.0f));
    node->SetProperty("opacity", mitk::FloatProperty::New(1.0f));
    node->SetProperty("wireframe", mitk::BoolProperty::New(false));
    node->SetProperty("selectedFaceId", mitk::IntProperty::New(-1));

    // Material lighting coefficients for realistic 3D rendering
    node->SetProperty("material.ambientCoefficient",  mitk::FloatProperty::New(0.08f));
    node->SetProperty("material.diffuseCoefficient",   mitk::FloatProperty::New(0.85f));
    node->SetProperty("material.specularCoefficient",  mitk::FloatProperty::New(0.80f));
    node->SetProperty("material.specularPower",        mitk::FloatProperty::New(12.0f));

    // Surface representation and interpolation
    node->SetProperty("material.representation",
                      mitk::VtkRepresentationProperty::New());
    node->SetProperty("material.interpolation",
                      mitk::VtkInterpolationProperty::New());

    // Edge and face display control
    node->SetProperty("show edges", mitk::BoolProperty::New(false));
    node->SetProperty("show faces", mitk::BoolProperty::New(true));
}

std::string xq_ModelObjectFactory::GetFileExtensions()
{
    std::string fileExtensions;
    CreateFileExtensionMaps();
    for (const auto& [ext, desc] : m_FileExtensionsMap)
    {
        fileExtensions += ext + " ";
    }
    return fileExtensions;
}

mitk::CoreObjectFactoryBase::MultimapType xq_ModelObjectFactory::GetFileExtensionsMap()
{
    return m_FileExtensionsMap;
}

std::string xq_ModelObjectFactory::GetSaveFileExtensions()
{
    std::string saveExtensions;
    CreateFileExtensionMaps();
    for (const auto& [ext, desc] : m_SaveFileExtensionsMap)
    {
        saveExtensions += ext + " ";
    }
    return saveExtensions;
}

mitk::CoreObjectFactoryBase::MultimapType xq_ModelObjectFactory::GetSaveFileExtensionsMap()
{
    return m_SaveFileExtensionsMap;
}

void xq_ModelObjectFactory::CreateFileExtensionMaps()
{
    m_FileExtensionsMap.clear();
    m_SaveFileExtensionsMap.clear();

    m_FileExtensionsMap.emplace("*.xqmdl", "XQ Model Files");
    m_SaveFileExtensionsMap.emplace("*.xqmdl", "XQ Model Files");
}

void RegisterXqModelObjectFactory()
{
    static auto instance = xq_ModelObjectFactory::New();
    mitk::CoreObjectFactory::GetInstance()->RegisterExtraFactory(instance);
}

// XQ fix: previously RegisterXqModelObjectFactory() was declared but never
// called, so no mapper was ever assigned to xq_Model / xq_PolyGeometry data.
// Pipeline-produced models rendered through the generic Surface fallback
// only when the underlying mitk::Surface mapper happened to fit, otherwise
// they were invisible. The static trigger below forces registration at
// library load time, matching xq_PathObjectFactory.
namespace
{
struct XqModelFactoryAutoRegister
{
    XqModelFactoryAutoRegister()
    {
        RegisterXqModelObjectFactory();
    }
};
static const XqModelFactoryAutoRegister s_XqModelFactoryAutoRegister;
}
