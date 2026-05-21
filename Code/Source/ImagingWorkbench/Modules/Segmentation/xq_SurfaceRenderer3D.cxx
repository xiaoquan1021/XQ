#include "xq_SurfaceRenderer3D.h"
#include "xq_LumenSurface.h"

#include <mitkDataNode.h>
#include <mitkProperties.h>
#include <mitkBaseRenderer.h>

#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkProperty.h>

// ---------------------------------------------------------------------------
// LocalStorage
// ---------------------------------------------------------------------------

xq_SurfaceRenderer3D::LocalStorage::LocalStorage()
{
    m_Actor = vtkSmartPointer<vtkActor>::New();
    m_Mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_Normals = vtkSmartPointer<vtkPolyDataNormals>::New();

    m_Actor->SetMapper(m_Mapper);
}

// ---------------------------------------------------------------------------
// Mapper lifecycle
// ---------------------------------------------------------------------------

vtkProp* xq_SurfaceRenderer3D::GetVtkProp(mitk::BaseRenderer* renderer)
{
    return m_LSH.GetLocalStorage(renderer)->m_Actor;
}

void xq_SurfaceRenderer3D::ResetMapper(mitk::BaseRenderer* renderer)
{
    m_LSH.GetLocalStorage(renderer)->m_Actor->VisibilityOff();
}

// ---------------------------------------------------------------------------
// SetDefaultProperties
// ---------------------------------------------------------------------------

void xq_SurfaceRenderer3D::SetDefaultProperties(mitk::DataNode* node,
                                                 mitk::BaseRenderer* renderer,
                                                 bool overwrite)
{
    if (!node)
        return;

    node->AddProperty("color", mitk::ColorProperty::New(1.0f, 1.0f, 1.0f), renderer, overwrite);
    node->AddProperty("opacity", mitk::FloatProperty::New(1.0f), renderer, overwrite);
    node->AddProperty("surface.representation", mitk::IntProperty::New(2), renderer, overwrite);
    node->AddProperty("surface.edge visibility", mitk::BoolProperty::New(false), renderer, overwrite);
    node->AddProperty("surface.edge color", mitk::ColorProperty::New(0.0f, 0.0f, 0.0f), renderer, overwrite);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

xq_SurfaceRenderer3D::RenderProperties
xq_SurfaceRenderer3D::fetchRenderProperties(const mitk::DataNode* node,
                                             mitk::BaseRenderer* renderer)
{
    RenderProperties props;
    node->GetColor(props.color.data(), renderer);
    node->GetOpacity(props.opacity, renderer);
    node->GetIntProperty("surface.representation", props.representation, renderer);
    node->GetBoolProperty("surface.edge visibility", props.edgeVisibility, renderer);

    if (props.edgeVisibility)
    {
        mitk::ColorProperty* edgeColorProp = nullptr;
        if (node->GetProperty(edgeColorProp, "surface.edge color", renderer) && edgeColorProp)
        {
            const auto c = edgeColorProp->GetColor();
            props.edgeColor = {c.GetRed(), c.GetGreen(), c.GetBlue()};
        }
    }
    return props;
}

void xq_SurfaceRenderer3D::applyRepresentation(vtkProperty* prop, int representation)
{
    switch (representation)
    {
    case 0:  prop->SetRepresentationToPoints();    break;
    case 1:  prop->SetRepresentationToWireframe();  break;
    case 2:
    default: prop->SetRepresentationToSurface();    break;
    }
}

// ---------------------------------------------------------------------------
// GenerateDataForRenderer
// ---------------------------------------------------------------------------

void xq_SurfaceRenderer3D::GenerateDataForRenderer(mitk::BaseRenderer* renderer)
{
    auto* ls = m_LSH.GetLocalStorage(renderer);

    const auto* node = this->GetDataNode();
    if (!node)
    {
        ls->m_Actor->VisibilityOff();
        return;
    }

    bool visible = true;
    node->GetBoolProperty("visible", visible, renderer);
    if (!visible)
    {
        ls->m_Actor->VisibilityOff();
        return;
    }

    auto* surface = dynamic_cast<xq_LumenSurface*>(node->GetData());
    if (!surface)
    {
        ls->m_Actor->VisibilityOff();
        return;
    }

    const unsigned int timeStep = renderer->GetTimeStep(surface);
    auto polyData = surface->GetSurfaceMesh(timeStep);

    if (!polyData || polyData->GetNumberOfPoints() == 0)
    {
        ls->m_Actor->VisibilityOff();
        return;
    }

    // Compute normals
    ls->m_Normals->SetInputData(polyData);
    ls->m_Normals->SetComputePointNormals(true);
    ls->m_Normals->SetComputeCellNormals(true);
    ls->m_Normals->SetSplitting(false);
    ls->m_Normals->SetConsistency(true);
    ls->m_Normals->Update();

    ls->m_Mapper->SetInputConnection(ls->m_Normals->GetOutputPort());

    const auto props = fetchRenderProperties(node, renderer);

    auto* actorProp = ls->m_Actor->GetProperty();
    actorProp->SetColor(props.color[0], props.color[1], props.color[2]);
    actorProp->SetOpacity(props.opacity);
    applyRepresentation(actorProp, props.representation);

    actorProp->SetEdgeVisibility(props.edgeVisibility);
    if (props.edgeVisibility)
        actorProp->SetEdgeColor(props.edgeColor[0], props.edgeColor[1], props.edgeColor[2]);

    ls->m_Actor->VisibilityOn();
}
