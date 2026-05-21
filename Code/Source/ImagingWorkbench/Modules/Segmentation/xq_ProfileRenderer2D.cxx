#include "xq_ProfileRenderer2D.h"
#include "xq_ProfileGroup.h"
#include "xq_LumenProfile.h"

#include <mitkDataNode.h>
#include <mitkProperties.h>
#include <mitkBaseRenderer.h>
#include <mitkPlaneGeometry.h>

#include <vtkActor.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolyData.h>
#include <vtkPropAssembly.h>
#include <vtkProperty.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkLine.h>
#include <vtkSphereSource.h>
#include <vtkAppendPolyData.h>

#include <cmath>
#include <array>

// ---------------------------------------------------------------------------
// LocalStorage
// ---------------------------------------------------------------------------

xq_ProfileRenderer2D::LocalStorage::LocalStorage()
{
    m_Assembly = vtkSmartPointer<vtkPropAssembly>::New();

    m_ContourActor = vtkSmartPointer<vtkActor>::New();
    m_ContourMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_ContourActor->SetMapper(m_ContourMapper);

    m_PointsActor = vtkSmartPointer<vtkActor>::New();
    m_PointsMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_PointsActor->SetMapper(m_PointsMapper);

    m_Assembly->AddPart(m_ContourActor);
    m_Assembly->AddPart(m_PointsActor);
}

// ---------------------------------------------------------------------------
// Mapper lifecycle
// ---------------------------------------------------------------------------

vtkProp* xq_ProfileRenderer2D::GetVtkProp(mitk::BaseRenderer* renderer)
{
    return m_LSH.GetLocalStorage(renderer)->m_Assembly;
}

void xq_ProfileRenderer2D::ResetMapper(mitk::BaseRenderer* renderer)
{
    auto* ls = m_LSH.GetLocalStorage(renderer);
    ls->m_ContourActor->VisibilityOff();
    ls->m_PointsActor->VisibilityOff();
}

// ---------------------------------------------------------------------------
// SetDefaultProperties
// ---------------------------------------------------------------------------

void xq_ProfileRenderer2D::SetDefaultProperties(mitk::DataNode* node,
                                                 mitk::BaseRenderer* renderer,
                                                 bool overwrite)
{
    node->AddProperty("color", mitk::ColorProperty::New(0.0f, 1.0f, 0.0f), renderer, overwrite);
    node->AddProperty("opacity", mitk::FloatProperty::New(1.0f), renderer, overwrite);
    node->AddProperty("contour.line width", mitk::FloatProperty::New(2.0f), renderer, overwrite);
    node->AddProperty("contour.point size", mitk::FloatProperty::New(4.0f), renderer, overwrite);
    node->AddProperty("contour.selected color", mitk::ColorProperty::New(1.0f, 0.0f, 0.0f), renderer, overwrite);

    Superclass::SetDefaultProperties(node, renderer, overwrite);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

xq_ProfileRenderer2D::RenderProperties
xq_ProfileRenderer2D::fetchRenderProperties(const mitk::DataNode* node,
                                             mitk::BaseRenderer* renderer) const
{
    RenderProperties props;
    node->GetColor(props.color.data(), renderer);
    node->GetOpacity(props.opacity, renderer);
    node->GetFloatProperty("contour.line width", props.lineWidth, renderer);
    node->GetFloatProperty("contour.point size", props.pointSize, renderer);

    mitk::ColorProperty* selColorProp = nullptr;
    if (node->GetProperty(selColorProp, "contour.selected color", renderer) && selColorProp)
    {
        const auto sc = selColorProp->GetColor();
        props.selectedColor = {sc.GetRed(), sc.GetGreen(), sc.GetBlue()};
    }
    return props;
}

void xq_ProfileRenderer2D::buildClosedPolyLine(const std::vector<vtkIdType>& ids,
                                                vtkCellArray* lines)
{
    for (size_t i = 0; i + 1 < ids.size(); ++i)
    {
        auto line = vtkSmartPointer<vtkLine>::New();
        line->GetPointIds()->SetId(0, ids[i]);
        line->GetPointIds()->SetId(1, ids[i + 1]);
        lines->InsertNextCell(line);
    }
    if (ids.size() >= 3)
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

void xq_ProfileRenderer2D::GenerateDataForRenderer(mitk::BaseRenderer* renderer)
{
    auto* ls = m_LSH.GetLocalStorage(renderer);

    const auto* node = GetDataNode();
    if (!node)
        return;

    auto* group = dynamic_cast<xq_ProfileGroup*>(node->GetData());
    if (!group)
    {
        ls->m_ContourActor->VisibilityOff();
        ls->m_PointsActor->VisibilityOff();
        return;
    }

    bool visible = true;
    node->GetVisibility(visible, renderer);
    if (!visible)
    {
        ls->m_ContourActor->VisibilityOff();
        ls->m_PointsActor->VisibilityOff();
        return;
    }

    const auto* planeGeometry = renderer->GetCurrentWorldPlaneGeometry();
    if (!planeGeometry)
    {
        ls->m_ContourActor->VisibilityOff();
        ls->m_PointsActor->VisibilityOff();
        return;
    }

    const auto planeOrigin = planeGeometry->GetOrigin();
    auto planeNormal = planeGeometry->GetNormal();
    planeNormal.Normalize();

    const auto props = fetchRenderProperties(node, renderer);

    // Lambda: signed distance from a point to the display plane
    auto signedDistance = [&](const mitk::Point3D& pt) -> double {
        return (pt[0] - planeOrigin[0]) * planeNormal[0]
             + (pt[1] - planeOrigin[1]) * planeNormal[1]
             + (pt[2] - planeOrigin[2]) * planeNormal[2];
    };

    // Lambda: project a point onto the display plane
    auto projectToPlane = [&](const mitk::Point3D& pt) -> std::array<double, 3> {
        const double d = signedDistance(pt);
        return {pt[0] - d * planeNormal[0],
                pt[1] - d * planeNormal[1],
                pt[2] - d * planeNormal[2]};
    };

    const unsigned int timeStep = renderer->GetTimeStep(group);
    const auto indices = group->GetProfilePathIndices(timeStep);

    constexpr double angleThreshold = 0.1;      // radians (~5.7 degrees)
    constexpr double distanceTolerance = 1.0;    // mm
    bool showControlPoints = true;
    node->GetBoolProperty("contour.show.control.points", showControlPoints, renderer);

    auto contourPoints = vtkSmartPointer<vtkPoints>::New();
    auto contourLines = vtkSmartPointer<vtkCellArray>::New();
    auto pointsAppend = vtkSmartPointer<vtkAppendPolyData>::New();
    bool hasControlPoints = false;

    for (const int pathPosIndex : indices)
    {
        auto* contour = group->FetchProfile(pathPosIndex, timeStep);
        if (!contour)
            continue;

        auto contourPlane = contour->GetSlicePlane();
        if (contourPlane.IsNull())
            continue;

        auto contourNormal = contourPlane->GetNormal();
        contourNormal.Normalize();

        const double dotProduct = planeNormal[0] * contourNormal[0]
                                + planeNormal[1] * contourNormal[1]
                                + planeNormal[2] * contourNormal[2];
        const double angle = std::acos(std::min(std::abs(dotProduct), 1.0));
        if (angle > angleThreshold)
            continue;

        const auto contourOrigin = contourPlane->GetOrigin();
        const double dist = signedDistance(contourOrigin);
        if (std::abs(dist) > distanceTolerance)
            continue;

        const auto pts = contour->GetProfilePoints();
        std::vector<vtkIdType> projectedIds;

        for (const auto& pt : pts)
        {
            if (std::abs(signedDistance(pt)) > distanceTolerance)
                continue;

            const auto projected = projectToPlane(pt);
            projectedIds.push_back(contourPoints->InsertNextPoint(projected.data()));
        }

        buildClosedPolyLine(projectedIds, contourLines);

        // Control-point spheres
        if (showControlPoints)
        {
          const int cpCount = contour->GetAnchorPointCount();
          for (int ci = 0; ci < cpCount; ++ci)
          {
              const auto cp = contour->GetAnchorPoint(ci);
              if (std::abs(signedDistance(cp)) > distanceTolerance)
                  continue;

              const auto projected = projectToPlane(cp);
              auto sphere = vtkSmartPointer<vtkSphereSource>::New();
              sphere->SetCenter(projected.data());
              sphere->SetRadius(props.pointSize * 0.1);
              sphere->SetPhiResolution(8);
              sphere->SetThetaResolution(8);
              sphere->Update();

              pointsAppend->AddInputData(sphere->GetOutput());
              hasControlPoints = true;
          }
        }
    }

    // Wire up contour-line actor
    auto contourPolyData = vtkSmartPointer<vtkPolyData>::New();
    contourPolyData->SetPoints(contourPoints);
    contourPolyData->SetLines(contourLines);
    ls->m_ContourMapper->SetInputData(contourPolyData);

    auto* contourProp = ls->m_ContourActor->GetProperty();
    contourProp->SetColor(props.color[0], props.color[1], props.color[2]);
    contourProp->SetOpacity(props.opacity);
    contourProp->SetLineWidth(props.lineWidth);

    ls->m_ContourActor->SetVisibility(contourPoints->GetNumberOfPoints() > 0);

    // Wire up control-points actor
    if (hasControlPoints)
    {
        pointsAppend->Update();
        ls->m_PointsMapper->SetInputData(pointsAppend->GetOutput());
        auto* ptsProp = ls->m_PointsActor->GetProperty();
        ptsProp->SetColor(props.selectedColor[0], props.selectedColor[1], props.selectedColor[2]);
        ptsProp->SetOpacity(props.opacity);
        ls->m_PointsActor->VisibilityOn();
    }
    else
    {
        ls->m_PointsActor->VisibilityOff();
    }

    ls->m_LastUpdateTime.Modified();
}
