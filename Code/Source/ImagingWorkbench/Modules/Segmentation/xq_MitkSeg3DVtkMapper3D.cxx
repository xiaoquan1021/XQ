#include "xq_MitkSeg3DVtkMapper3D.h"
#include "xq_MitkSeg3D.h"

#include <mitkDataNode.h>
#include <mitkProperties.h>
#include <mitkBaseRenderer.h>

#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyData.h>
#include <vtkPolyDataNormals.h>
#include <vtkPropAssembly.h>
#include <vtkProperty.h>
#include <vtkSphereSource.h>
#include <vtkAppendPolyData.h>

// ---------------------------------------------------------------------------
// LocalStorage
// ---------------------------------------------------------------------------

xq_MitkSeg3DVtkMapper3D::LocalStorage::LocalStorage()
{
    m_Assembly = vtkSmartPointer<vtkPropAssembly>::New();

    m_SurfaceMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_SurfaceActor = vtkSmartPointer<vtkActor>::New();
    m_SurfaceActor->SetMapper(m_SurfaceMapper);

    m_Normals = vtkSmartPointer<vtkPolyDataNormals>::New();

    m_SeedPointsMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_SeedPointsActor = vtkSmartPointer<vtkActor>::New();
    m_SeedPointsActor->SetMapper(m_SeedPointsMapper);

    m_Assembly->AddPart(m_SurfaceActor);
    m_Assembly->AddPart(m_SeedPointsActor);
}

// ---------------------------------------------------------------------------
// Mapper lifecycle
// ---------------------------------------------------------------------------

vtkProp* xq_MitkSeg3DVtkMapper3D::GetVtkProp(mitk::BaseRenderer* renderer)
{
    return m_LSH.GetLocalStorage(renderer)->m_Assembly;
}

void xq_MitkSeg3DVtkMapper3D::ResetMapper(mitk::BaseRenderer* renderer)
{
    auto* ls = m_LSH.GetLocalStorage(renderer);
    ls->m_SurfaceActor->VisibilityOff();
    ls->m_SeedPointsActor->VisibilityOff();
}

// ---------------------------------------------------------------------------
// SetDefaultProperties
// ---------------------------------------------------------------------------

void xq_MitkSeg3DVtkMapper3D::SetDefaultProperties(mitk::DataNode* node,
                                                    mitk::BaseRenderer* renderer,
                                                    bool overwrite)
{
    node->AddProperty("color", mitk::ColorProperty::New(0.8f, 0.8f, 0.2f), renderer, overwrite);
    node->AddProperty("opacity", mitk::FloatProperty::New(0.8f), renderer, overwrite);
    node->AddProperty("seg3d.edge visibility", mitk::BoolProperty::New(true), renderer, overwrite);
    node->AddProperty("seg3d.edge color", mitk::ColorProperty::New(0.2f, 0.2f, 0.2f), renderer, overwrite);
    node->AddProperty("seg3d.seed point size", mitk::FloatProperty::New(1.5f), renderer, overwrite);

    Superclass::SetDefaultProperties(node, renderer, overwrite);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

xq_MitkSeg3DVtkMapper3D::RenderProperties
xq_MitkSeg3DVtkMapper3D::fetchRenderProperties(const mitk::DataNode* node,
                                                mitk::BaseRenderer* renderer)
{
    RenderProperties props;
    node->GetColor(props.color.data(), renderer);
    node->GetOpacity(props.opacity, renderer);
    node->GetBoolProperty("seg3d.edge visibility", props.edgeVisibility, renderer);
    node->GetFloatProperty("seg3d.seed point size", props.seedRadius, renderer);

    if (props.edgeVisibility)
    {
        mitk::ColorProperty* edgeColorProp = nullptr;
        if (auto* propList = node->GetPropertyList(renderer))
            edgeColorProp = dynamic_cast<mitk::ColorProperty*>(
                propList->GetProperty("seg3d.edge color"));
        if (!edgeColorProp)
            if (auto* propList = node->GetPropertyList())
                edgeColorProp = dynamic_cast<mitk::ColorProperty*>(
                    propList->GetProperty("seg3d.edge color"));
        if (edgeColorProp)
        {
            const auto c = edgeColorProp->GetColor();
            props.edgeColor = {c.GetRed(), c.GetGreen(), c.GetBlue()};
        }
    }
    return props;
}

// ---------------------------------------------------------------------------
// GenerateDataForRenderer
// ---------------------------------------------------------------------------

void xq_MitkSeg3DVtkMapper3D::GenerateDataForRenderer(mitk::BaseRenderer* renderer)
{
    auto* ls = m_LSH.GetLocalStorage(renderer);

    const auto* node = GetDataNode();
    if (!node)
        return;

    auto* seg3d = dynamic_cast<xq_MitkSeg3D*>(node->GetData());
    if (!seg3d)
        return;

    bool visible = true;
    node->GetVisibility(visible, renderer, "visible");
    if (!visible)
    {
        ls->m_SurfaceActor->VisibilityOff();
        ls->m_SeedPointsActor->VisibilityOff();
        return;
    }

    const auto props = fetchRenderProperties(node, renderer);

    // --- Part 1: Segmentation surface ---
    auto polyData = seg3d->GetSurfaceMesh();
    if (polyData && polyData->GetNumberOfPoints() > 0)
    {
        ls->m_Normals->SetInputData(polyData);
        ls->m_Normals->Update();
        ls->m_SurfaceMapper->SetInputConnection(ls->m_Normals->GetOutputPort());

        auto* surfProp = ls->m_SurfaceActor->GetProperty();
        surfProp->SetColor(props.color[0], props.color[1], props.color[2]);
        surfProp->SetOpacity(props.opacity);
        surfProp->SetEdgeVisibility(props.edgeVisibility);

        if (props.edgeVisibility)
            surfProp->SetEdgeColor(props.edgeColor[0], props.edgeColor[1], props.edgeColor[2]);

        ls->m_SurfaceActor->VisibilityOn();
    }
    else
    {
        ls->m_SurfaceActor->VisibilityOff();
    }

    // --- Part 2: Seed points ---
    const auto seedPoints = seg3d->GetSeedPoints();
    if (!seedPoints.empty())
    {
        auto appendFilter = vtkSmartPointer<vtkAppendPolyData>::New();

        for (const auto& pt : seedPoints)
        {
            auto sphere = vtkSmartPointer<vtkSphereSource>::New();
            sphere->SetCenter(pt[0], pt[1], pt[2]);
            sphere->SetRadius(props.seedRadius);
            sphere->SetThetaResolution(12);
            sphere->SetPhiResolution(12);
            sphere->Update();
            appendFilter->AddInputData(sphere->GetOutput());
        }

        appendFilter->Update();
        ls->m_SeedPointsMapper->SetInputData(appendFilter->GetOutput());

        ls->m_SeedPointsActor->GetProperty()->SetColor(1.0, 1.0, 0.0);
        ls->m_SeedPointsActor->VisibilityOn();
    }
    else
    {
        ls->m_SeedPointsActor->VisibilityOff();
    }

    ls->m_LastUpdateTime.Modified();
}
