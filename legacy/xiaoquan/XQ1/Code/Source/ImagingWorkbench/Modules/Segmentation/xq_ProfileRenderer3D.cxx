#include "xq_ProfileRenderer3D.h"
#include "xq_ProfileGroup.h"
#include "xq_LumenProfile.h"

#include <mitkDataNode.h>
#include <mitkProperties.h>
#include <mitkBaseRenderer.h>

#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyData.h>
#include <vtkPropAssembly.h>
#include <vtkProperty.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkLine.h>
#include <vtkAppendPolyData.h>

// ---------------------------------------------------------------------------
// LocalStorage
// ---------------------------------------------------------------------------

xq_ProfileRenderer3D::LocalStorage::LocalStorage()
{
    m_Assembly = vtkSmartPointer<vtkPropAssembly>::New();

    m_ContourMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_ContourActor = vtkSmartPointer<vtkActor>::New();
    m_ContourActor->SetMapper(m_ContourMapper);

    m_SurfaceMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_SurfaceActor = vtkSmartPointer<vtkActor>::New();
    m_SurfaceActor->SetMapper(m_SurfaceMapper);

    m_Assembly->AddPart(m_ContourActor);
    m_Assembly->AddPart(m_SurfaceActor);
}

// ---------------------------------------------------------------------------
// Mapper lifecycle
// ---------------------------------------------------------------------------

vtkProp* xq_ProfileRenderer3D::GetVtkProp(mitk::BaseRenderer* renderer)
{
    return m_LSH.GetLocalStorage(renderer)->m_Assembly;
}

void xq_ProfileRenderer3D::ResetMapper(mitk::BaseRenderer* renderer)
{
    auto* ls = m_LSH.GetLocalStorage(renderer);
    ls->m_ContourActor->VisibilityOff();
    ls->m_SurfaceActor->VisibilityOff();
}

// ---------------------------------------------------------------------------
// SetDefaultProperties
// ---------------------------------------------------------------------------

void xq_ProfileRenderer3D::SetDefaultProperties(mitk::DataNode* node,
                                                 mitk::BaseRenderer* renderer,
                                                 bool overwrite)
{
    node->AddProperty("color", mitk::ColorProperty::New(0.0f, 1.0f, 0.0f), renderer, overwrite);
    node->AddProperty("opacity", mitk::FloatProperty::New(1.0f), renderer, overwrite);
    node->AddProperty("contour.line width", mitk::FloatProperty::New(2.0f), renderer, overwrite);
    node->AddProperty("contour.show surface", mitk::BoolProperty::New(false), renderer, overwrite);
    node->AddProperty("contour.surface opacity", mitk::FloatProperty::New(0.3f), renderer, overwrite);

    Superclass::SetDefaultProperties(node, renderer, overwrite);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

xq_ProfileRenderer3D::RenderProperties
xq_ProfileRenderer3D::fetchRenderProperties(const mitk::DataNode* node,
                                             mitk::BaseRenderer* renderer) const
{
    RenderProperties props;
    node->GetColor(props.color.data(), renderer);
    node->GetOpacity(props.opacity, renderer);
    node->GetFloatProperty("contour.line width", props.lineWidth, renderer);
    node->GetBoolProperty("contour.show surface", props.showSurface, renderer);
    node->GetFloatProperty("contour.surface opacity", props.surfaceOpacity, renderer);
    return props;
}

void xq_ProfileRenderer3D::buildClosedPolyLine(vtkPoints* vtkPts, vtkCellArray* lines)
{
    if (!vtkPts) return;
    const auto numPts = vtkPts->GetNumberOfPoints();
    if (numPts < 2) return;

    for (vtkIdType i = 0; i < numPts - 1; ++i)
    {
        auto line = vtkSmartPointer<vtkLine>::New();
        line->GetPointIds()->SetId(0, i);
        line->GetPointIds()->SetId(1, i + 1);
        lines->InsertNextCell(line);
    }
    // Close the contour
    auto closingLine = vtkSmartPointer<vtkLine>::New();
    closingLine->GetPointIds()->SetId(0, numPts - 1);
    closingLine->GetPointIds()->SetId(1, 0);
    lines->InsertNextCell(closingLine);
}

// ---------------------------------------------------------------------------
// GenerateDataForRenderer
// ---------------------------------------------------------------------------

void xq_ProfileRenderer3D::GenerateDataForRenderer(mitk::BaseRenderer* renderer)
{
    auto* ls = m_LSH.GetLocalStorage(renderer);

    const auto* node = GetDataNode();
    if (!node)
        return;

    auto* group = dynamic_cast<xq_ProfileGroup*>(node->GetData());
    if (!group)
        return;

    bool visible = true;
    node->GetVisibility(visible, renderer);
    if (!visible)
    {
        ls->m_ContourActor->VisibilityOff();
        ls->m_SurfaceActor->VisibilityOff();
        return;
    }

    const unsigned int timeStep = renderer->GetTimeStep(group);
    const auto props = fetchRenderProperties(node, renderer);

    // --- Part 1: Contour wireframes ---
    const auto indices = group->GetProfilePathIndices(timeStep);
    if (!indices.empty())
    {
        auto appendFilter = vtkSmartPointer<vtkAppendPolyData>::New();

        for (const int pathPosIndex : indices)
        {
            auto* contour = group->FetchProfile(pathPosIndex, timeStep);
            if (!contour)
                continue;

            const auto pts = contour->GetProfilePoints();
            const auto numPts = static_cast<vtkIdType>(pts.size());
            if (numPts < 2)
                continue;

            auto vtkPts = vtkSmartPointer<vtkPoints>::New();
            vtkPts->SetNumberOfPoints(numPts);
            for (vtkIdType i = 0; i < numPts; ++i)
                vtkPts->SetPoint(i, pts[i][0], pts[i][1], pts[i][2]);

            auto lines = vtkSmartPointer<vtkCellArray>::New();
            buildClosedPolyLine(vtkPts, lines);

            auto contourPoly = vtkSmartPointer<vtkPolyData>::New();
            contourPoly->SetPoints(vtkPts);
            contourPoly->SetLines(lines);
            appendFilter->AddInputData(contourPoly);
        }

        if (appendFilter->GetNumberOfInputConnections(0) > 0)
        {
            appendFilter->Update();
            ls->m_ContourMapper->SetInputData(appendFilter->GetOutput());

            auto* prop = ls->m_ContourActor->GetProperty();
            prop->SetColor(props.color[0], props.color[1], props.color[2]);
            prop->SetOpacity(props.opacity);
            prop->SetLineWidth(props.lineWidth);

            ls->m_ContourActor->VisibilityOn();
        }
        else
        {
            ls->m_ContourActor->VisibilityOff();
        }
    }
    else
    {
        ls->m_ContourActor->VisibilityOff();
    }

    // --- Part 2: Loft surface (optional) ---
    auto loftSurface = group->GetLoftedMesh(timeStep);
    if (props.showSurface && loftSurface && loftSurface->GetNumberOfPoints() > 0)
    {
        ls->m_SurfaceMapper->SetInputData(loftSurface);

        auto* surfProp = ls->m_SurfaceActor->GetProperty();
        surfProp->SetColor(props.color[0], props.color[1], props.color[2]);
        surfProp->SetOpacity(props.surfaceOpacity);
        surfProp->SetRepresentationToWireframe();

        ls->m_SurfaceActor->VisibilityOn();
    }
    else
    {
        ls->m_SurfaceActor->VisibilityOff();
    }

    ls->m_LastUpdateTime.Modified();
}
