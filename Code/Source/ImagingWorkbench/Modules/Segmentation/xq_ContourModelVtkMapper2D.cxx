#include "xq_ContourModelVtkMapper2D.h"
#include "xq_ThresholdContour.h"

#include <mitkDataNode.h>
#include <mitkProperties.h>
#include <mitkBaseRenderer.h>
#include <mitkPlaneGeometry.h>
#include <mitkContourModel.h>

#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkLine.h>

#include <cmath>
#include <array>

// ---------------------------------------------------------------------------
// LocalStorage
// ---------------------------------------------------------------------------

xq_ContourModelVtkMapper2D::LocalStorage::LocalStorage()
{
    m_Actor    = vtkSmartPointer<vtkActor>::New();
    m_Mapper   = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_PolyData = vtkSmartPointer<vtkPolyData>::New();
    m_Points   = vtkSmartPointer<vtkPoints>::New();
    m_Lines    = vtkSmartPointer<vtkCellArray>::New();

    m_Actor->SetMapper(m_Mapper);
}

// ---------------------------------------------------------------------------
// Mapper lifecycle
// ---------------------------------------------------------------------------

vtkProp* xq_ContourModelVtkMapper2D::GetVtkProp(mitk::BaseRenderer* renderer)
{
    return m_LSH.GetLocalStorage(renderer)->m_Actor;
}

void xq_ContourModelVtkMapper2D::ResetMapper(mitk::BaseRenderer* renderer)
{
    m_LSH.GetLocalStorage(renderer)->m_Actor->VisibilityOff();
}

// ---------------------------------------------------------------------------
// SetDefaultProperties
// ---------------------------------------------------------------------------

void xq_ContourModelVtkMapper2D::SetDefaultProperties(mitk::DataNode* node,
                                                       mitk::BaseRenderer* renderer,
                                                       bool overwrite)
{
    node->AddProperty("color", mitk::ColorProperty::New(0.0f, 1.0f, 0.0f), renderer, overwrite);
    node->AddProperty("opacity", mitk::FloatProperty::New(1.0f), renderer, overwrite);
    node->AddProperty("contour.line width", mitk::FloatProperty::New(2.0f), renderer, overwrite);

    Superclass::SetDefaultProperties(node, renderer, overwrite);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

xq_ContourModelVtkMapper2D::RenderProperties
xq_ContourModelVtkMapper2D::fetchRenderProperties(const mitk::DataNode* node,
                                                   mitk::BaseRenderer* renderer)
{
    RenderProperties props;
    node->GetColor(props.color.data(), renderer);
    node->GetOpacity(props.opacity, renderer);
    node->GetFloatProperty("contour.line width", props.lineWidth, renderer);
    return props;
}

void xq_ContourModelVtkMapper2D::buildClosedPolyLine(const std::vector<vtkIdType>& ids,
                                                     vtkCellArray* lines,
                                                     bool closed)
{
    for (size_t i = 0; i + 1 < ids.size(); ++i)
    {
        auto line = vtkSmartPointer<vtkLine>::New();
        line->GetPointIds()->SetId(0, ids[i]);
        line->GetPointIds()->SetId(1, ids[i + 1]);
        lines->InsertNextCell(line);
    }
    if (closed && ids.size() >= 2)
    {
        auto line = vtkSmartPointer<vtkLine>::New();
        line->GetPointIds()->SetId(0, ids.back());
        line->GetPointIds()->SetId(1, ids.front());
        lines->InsertNextCell(line);
    }
}

// ---------------------------------------------------------------------------
// GenerateDataForRenderer
// ---------------------------------------------------------------------------

void xq_ContourModelVtkMapper2D::GenerateDataForRenderer(mitk::BaseRenderer* renderer)
{
    auto* ls = m_LSH.GetLocalStorage(renderer);

    const auto* node = GetDataNode();
    if (!node)
        return;

    auto* contourModel = dynamic_cast<xq_ThresholdContour*>(node->GetData());
    if (!contourModel)
    {
        ls->m_Actor->VisibilityOff();
        return;
    }

    bool visible = true;
    node->GetVisibility(visible, renderer);
    if (!visible)
    {
        ls->m_Actor->VisibilityOff();
        return;
    }

    const auto* planeGeometry = renderer->GetCurrentWorldPlaneGeometry();
    if (!planeGeometry)
    {
        ls->m_Actor->VisibilityOff();
        return;
    }

    const auto origin = planeGeometry->GetOrigin();
    auto planeNormal = planeGeometry->GetNormal();
    planeNormal.Normalize();

    const unsigned int timeStep = renderer->GetTimeStep(contourModel);
    const int numVertices = contourModel->GetNumberOfVertices(timeStep);

    if (numVertices == 0)
    {
        ls->m_Actor->VisibilityOff();
        return;
    }

    constexpr double tolerance = 1.0; // mm

    // Lambda: signed distance from a point to the slice plane
    auto signedDistance = [&](const mitk::Point3D& pt) -> double {
        return (pt[0] - origin[0]) * planeNormal[0]
             + (pt[1] - origin[1]) * planeNormal[1]
             + (pt[2] - origin[2]) * planeNormal[2];
    };

    // Lambda: project a point onto the plane
    auto projectToPlane = [&](const mitk::Point3D& pt) -> std::array<double, 3> {
        const double d = signedDistance(pt);
        return {pt[0] - d * planeNormal[0],
                pt[1] - d * planeNormal[1],
                pt[2] - d * planeNormal[2]};
    };

    ls->m_Points->Reset();
    ls->m_Lines->Reset();

    std::vector<vtkIdType> projectedIds;

    for (auto it = contourModel->Begin(timeStep), end = contourModel->End(timeStep);
         it != end; ++it)
    {
        const auto& coords = (*it)->Coordinates;
        if (std::abs(signedDistance(coords)) > tolerance)
            continue;

        const auto projected = projectToPlane(coords);
        projectedIds.push_back(ls->m_Points->InsertNextPoint(projected.data()));
    }

    buildClosedPolyLine(projectedIds, ls->m_Lines, contourModel->IsClosed(timeStep));

    ls->m_PolyData->SetPoints(ls->m_Points);
    ls->m_PolyData->SetLines(ls->m_Lines);
    ls->m_PolyData->Modified();

    ls->m_Mapper->SetInputData(ls->m_PolyData);

    const auto props = fetchRenderProperties(node, renderer);
    auto* actorProp = ls->m_Actor->GetProperty();
    actorProp->SetColor(props.color[0], props.color[1], props.color[2]);
    actorProp->SetOpacity(props.opacity);
    actorProp->SetLineWidth(props.lineWidth);
    ls->m_Actor->VisibilityOn();

    ls->m_LastUpdateTime.Modified();
}
