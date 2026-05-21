#include "xq_VesselTracer2D.h"
#include "xq_VesselCenterline.h"

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
#include <vtkPlane.h>
#include <vtkCutter.h>
#include <vtkSphereSource.h>
#include <vtkAppendPolyData.h>
#include <vtkUnsignedCharArray.h>
#include <vtkPointData.h>
#include <vtkStripper.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTransform.h>

#include <algorithm>
#include <cmath>

// XQ composes VTK filters into pipelines; no deep mapper class hierarchies (differs from SV)

xq_VesselTracer2D::LocalStorage::LocalStorage()
{
    // Wire up the composed VTK pipeline
    m_PathActor->SetMapper(m_PathMapper);
    m_PointsActor->SetMapper(m_PointsMapper);
    m_Assembly->AddPart(m_PathActor);
    m_Assembly->AddPart(m_PointsActor);
}

vtkProp* xq_VesselTracer2D::GetVtkProp(mitk::BaseRenderer* renderer)
{
    return m_LSH.GetLocalStorage(renderer)->m_Assembly;
}

void xq_VesselTracer2D::GenerateDataForRenderer(mitk::BaseRenderer* renderer)
{
    auto* ls = m_LSH.GetLocalStorage(renderer);

    auto* node = GetDataNode();
    if (!node)
        return;

    auto* path = dynamic_cast<xq_VesselCenterline*>(node->GetData());
    if (!path || path->IsEmptyTimeStep(0))
    {
        ls->m_PathActor->VisibilityOff();
        ls->m_PointsActor->VisibilityOff();
        return;
    }

    bool visible = true;
    node->GetVisibility(visible, renderer);
    if (!visible)
    {
        ls->m_PathActor->VisibilityOff();
        ls->m_PointsActor->VisibilityOff();
        return;
    }

    const auto* planeGeometry = renderer->GetCurrentWorldPlaneGeometry();
    if (!planeGeometry)
    {
        ls->m_PathActor->VisibilityOff();
        ls->m_PointsActor->VisibilityOff();
        return;
    }

    auto origin = planeGeometry->GetOrigin();
    auto planeNormal = planeGeometry->GetNormal();
    planeNormal.Normalize();

    // Read rendering properties
    float sliceThickness = 1.0f;
    node->GetFloatProperty("slice thickness", sliceThickness, renderer);

    float lineWidth = 2.0f;
    node->GetFloatProperty("line width", lineWidth, renderer);
    float pointSize = 1.0f;
    node->GetFloatProperty("point size", pointSize, renderer);

    float color[3] = {0.0f, 1.0f, 0.0f};
    node->GetColor(color, renderer);

    float opacity = 1.0f;
    node->GetOpacity(opacity, renderer);

    float selectedColor[3] = {1.0f, 0.0f, 0.0f};
    node->GetFloatProperty("selected color r", selectedColor[0], renderer);
    node->GetFloatProperty("selected color g", selectedColor[1], renderer);
    node->GetFloatProperty("selected color b", selectedColor[2], renderer);
    bool showControlPoints = true;
    node->GetBoolProperty("path.show.control.points", showControlPoints, renderer);

    auto* elem = path->GetSegment(0);

    // Lambda: build a vtkPolyData polyline from a vector of 3D points
    auto buildPolylineFromPoints = [](const auto& pts) {
        auto points = vtkSmartPointer<vtkPoints>::New();
        auto lines  = vtkSmartPointer<vtkCellArray>::New();

        for (const auto& pt : pts)
            points->InsertNextPoint(pt[0], pt[1], pt[2]);

        for (vtkIdType i = 0; i + 1 < static_cast<vtkIdType>(pts.size()); ++i)
        {
            auto line = vtkSmartPointer<vtkLine>::New();
            line->GetPointIds()->SetId(0, i);
            line->GetPointIds()->SetId(1, i + 1);
            lines->InsertNextCell(line);
        }

        auto polyLine = vtkSmartPointer<vtkPolyData>::New();
        polyLine->SetPoints(points);
        polyLine->SetLines(lines);
        return polyLine;
    };

    // Lambda: compute signed distance from a point to the slice plane
    auto computeSliceDistance = [&](const auto& pt) {
        return std::abs((pt[0] - origin[0]) * planeNormal[0] +
                        (pt[1] - origin[1]) * planeNormal[1] +
                        (pt[2] - origin[2]) * planeNormal[2]);
    };

    auto buildCrossForAnchor = [](const mitk::Point3D& point,
                                  double size,
                                  bool selected,
                                  const float* baseColor,
                                  const float* selColor) {
        auto points = vtkSmartPointer<vtkPoints>::New();
        auto lines = vtkSmartPointer<vtkCellArray>::New();
        auto polyData = vtkSmartPointer<vtkPolyData>::New();
        auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();

        const auto* c = selected ? selColor : baseColor;
        const auto r = static_cast<unsigned char>(c[0] * 255);
        const auto g = static_cast<unsigned char>(c[1] * 255);
        const auto b = static_cast<unsigned char>(c[2] * 255);

        const vtkIdType p0 = points->InsertNextPoint(point[0] - size, point[1], point[2]);
        const vtkIdType p1 = points->InsertNextPoint(point[0] + size, point[1], point[2]);
        const vtkIdType p2 = points->InsertNextPoint(point[0], point[1] - size, point[2]);
        const vtkIdType p3 = points->InsertNextPoint(point[0], point[1] + size, point[2]);

        vtkIdType lineA[2] = {p0, p1};
        vtkIdType lineB[2] = {p2, p3};
        lines->InsertNextCell(2, lineA);
        lines->InsertNextCell(2, lineB);

        colors->SetNumberOfComponents(3);
        colors->SetNumberOfTuples(4);
        for (vtkIdType i = 0; i < 4; ++i)
            colors->SetTuple3(i, r, g, b);

        polyData->SetPoints(points);
        polyData->SetLines(lines);
        polyData->GetPointData()->SetScalars(colors);
        return polyData;
    };

    // Build and slice the path polyline
    if (auto pathPts = elem->GetTracePositions(); pathPts.size() >= 2)
    {
        auto polyLine = buildPolylineFromPoints(pathPts);

        auto plane = vtkSmartPointer<vtkPlane>::New();
        plane->SetOrigin(origin[0], origin[1], origin[2]);
        plane->SetNormal(planeNormal[0], planeNormal[1], planeNormal[2]);

        auto cutter = vtkSmartPointer<vtkCutter>::New();
        cutter->SetCutFunction(plane);
        cutter->SetInputData(polyLine);
        cutter->Update();

        ls->m_PathMapper->SetInputData(cutter->GetOutput());
        ls->m_PathActor->GetProperty()->SetColor(color[0], color[1], color[2]);
        ls->m_PathActor->GetProperty()->SetOpacity(opacity);
        ls->m_PathActor->GetProperty()->SetLineWidth(lineWidth);
        ls->m_PathActor->GetProperty()->SetPointSize(lineWidth + 2.0f);
        ls->m_PathActor->VisibilityOn();
    }
    else
    {
        ls->m_PathActor->VisibilityOff();
    }

    // Render control points as colored spheres near the current slice
    auto ctrlPts = elem->GetAnchorNodes();
    auto appendFilter = vtkSmartPointer<vtkAppendPolyData>::New();
    const double crossHalfSize = std::max(0.2, static_cast<double>(pointSize) * 0.5);
    bool hasVisiblePoints = false;

    if (showControlPoints)
    {
        std::for_each(ctrlPts.cbegin(), ctrlPts.cend(),
            [&](const auto& cp) {
                if (computeSliceDistance(cp.point) > sliceThickness)
                    return;

                auto cross = buildCrossForAnchor(cp.point, crossHalfSize,
                                                 cp.selected, color, selectedColor);
                appendFilter->AddInputData(cross);
                hasVisiblePoints = true;
            });
    }

    if (hasVisiblePoints)
    {
        appendFilter->Update();
        ls->m_PointsMapper->SetInputData(appendFilter->GetOutput());
        ls->m_PointsMapper->ScalarVisibilityOn();
        ls->m_PointsMapper->SetScalarModeToUsePointData();
        ls->m_PointsActor->GetProperty()->SetOpacity(opacity);
        ls->m_PointsActor->GetProperty()->SetLineWidth(std::max(1.0f, lineWidth));
        ls->m_PointsActor->VisibilityOn();
    }
    else
    {
        ls->m_PointsActor->VisibilityOff();
    }

    ls->m_LastUpdateTime.Modified();
}

void xq_VesselTracer2D::ResetMapper(mitk::BaseRenderer* renderer)
{
    auto* ls = m_LSH.GetLocalStorage(renderer);
    ls->m_PathActor->VisibilityOff();
    ls->m_PointsActor->VisibilityOff();
}
