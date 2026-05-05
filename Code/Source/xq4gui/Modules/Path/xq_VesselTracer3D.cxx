#include "xq_VesselTracer3D.h"
#include "xq_VesselCenterline.h"

#include <mitkDataNode.h>
#include <mitkProperties.h>
#include <mitkColorProperty.h>
#include <mitkBaseRenderer.h>

#include <vtkSphereSource.h>
#include <vtkTubeFilter.h>
#include <vtkAppendPolyData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkPropAssembly.h>
#include <vtkProperty.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkLine.h>
#include <vtkUnsignedCharArray.h>
#include <vtkPointData.h>

#include <algorithm>
#include <cmath>

// XQ composes VTK filters into pipelines; no deep mapper class hierarchies (differs from SV)

xq_VesselTracer3D::LocalStorage::LocalStorage()
{
    // Wire up the composed VTK pipeline
    m_PathActor->SetMapper(m_PathMapper);
    m_PointsActor->SetMapper(m_PointsMapper);
    m_Assembly->AddPart(m_PathActor);
    m_Assembly->AddPart(m_PointsActor);
}

vtkProp* xq_VesselTracer3D::GetVtkProp(mitk::BaseRenderer* renderer)
{
    return m_LSH.GetLocalStorage(renderer)->m_Assembly;
}

void xq_VesselTracer3D::GenerateDataForRenderer(mitk::BaseRenderer* renderer)
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

    // Read rendering properties
    float lineWidth = 1.0f;
    node->GetFloatProperty("line width", lineWidth, renderer);

    float tubeRadius = 0.3f;
    node->GetFloatProperty("tube radius", tubeRadius, renderer);

    float pointSize = 1.0f;
    node->GetFloatProperty("point size", pointSize, renderer);
    bool showControlPoints = true;
    node->GetBoolProperty("path.show.control.points", showControlPoints, renderer);

    float color[3] = {0.0f, 1.0f, 0.0f};
    node->GetColor(color, renderer);

    float opacity = 1.0f;
    node->GetOpacity(opacity, renderer);

    float selectedColor[3] = {1.0f, 0.0f, 0.0f};
    node->GetFloatProperty("selected color r", selectedColor[0], renderer);
    node->GetFloatProperty("selected color g", selectedColor[1], renderer);
    node->GetFloatProperty("selected color b", selectedColor[2], renderer);

    bool useTube = true;
    node->GetBoolProperty("use tube", useTube, renderer);

    auto* elem = path->GetSegment(0);
    auto pathPts = elem->GetTracePositions();
    auto ctrlPts = elem->GetAnchorNodes();

    // Lambda: build a vtkPolyData polyline from a vector of 3D points
    auto buildPolylineData = [](const auto& pts) {
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

        auto polyData = vtkSmartPointer<vtkPolyData>::New();
        polyData->SetPoints(points);
        polyData->SetLines(lines);
        return polyData;
    };

    // Lambda: color a sphere's points based on selection state
    auto colorSphereForAnchor = [](vtkPolyData* pd, bool selected,
                                          const float* baseColor,
                                          const float* selColor) {
        auto sphereColors = vtkSmartPointer<vtkUnsignedCharArray>::New();
        sphereColors->SetNumberOfComponents(3);
        sphereColors->SetNumberOfTuples(pd->GetNumberOfPoints());

        const auto* c = selected ? selColor : baseColor;
        const auto r = static_cast<unsigned char>(c[0] * 255);
        const auto g = static_cast<unsigned char>(c[1] * 255);
        const auto b = static_cast<unsigned char>(c[2] * 255);

        for (vtkIdType j = 0; j < pd->GetNumberOfPoints(); ++j)
            sphereColors->SetTuple3(j, r, g, b);

        pd->GetPointData()->SetScalars(sphereColors);
    };

    // Build path polyline, optionally with tube filter
    if (pathPts.size() >= 2)
    {
        auto polyData = buildPolylineData(pathPts);

        if (useTube && tubeRadius > 0.0f)
        {
            auto tubeFilter = vtkSmartPointer<vtkTubeFilter>::New();
            tubeFilter->SetInputData(polyData);
            tubeFilter->SetRadius(tubeRadius);
            tubeFilter->SetNumberOfSides(12);
            tubeFilter->CappingOn();
            tubeFilter->Update();
            ls->m_PathMapper->SetInputData(tubeFilter->GetOutput());
        }
        else
        {
            ls->m_PathMapper->SetInputData(polyData);
        }

        ls->m_PathActor->GetProperty()->SetColor(color[0], color[1], color[2]);
        ls->m_PathActor->GetProperty()->SetOpacity(opacity);
        ls->m_PathActor->GetProperty()->SetLineWidth(lineWidth);
        ls->m_PathActor->VisibilityOn();
    }
    else
    {
        ls->m_PathActor->VisibilityOff();
    }

    // Render control points as colored spheres
    if (showControlPoints && !ctrlPts.empty())
    {
        auto appendFilter = vtkSmartPointer<vtkAppendPolyData>::New();
        constexpr double minSphereRadius = 0.2;
        const double sphereRadius = std::max(static_cast<double>(pointSize) * 0.5,
                                             minSphereRadius);

        std::for_each(ctrlPts.cbegin(), ctrlPts.cend(),
            [&](const auto& cp) {
                auto sphere = vtkSmartPointer<vtkSphereSource>::New();
                sphere->SetCenter(cp.point[0], cp.point[1], cp.point[2]);
                sphere->SetRadius(sphereRadius);
                sphere->SetPhiResolution(12);
                sphere->SetThetaResolution(12);
                sphere->Update();

                colorSphereForAnchor(sphere->GetOutput(), cp.selected,
                                           color, selectedColor);
                appendFilter->AddInputData(sphere->GetOutput());
            });

        appendFilter->Update();
        ls->m_PointsMapper->SetInputData(appendFilter->GetOutput());
        ls->m_PointsMapper->ScalarVisibilityOn();
        ls->m_PointsMapper->SetScalarModeToUsePointData();
        ls->m_PointsActor->GetProperty()->SetOpacity(opacity);
        ls->m_PointsActor->VisibilityOn();
    }
    else
    {
        ls->m_PointsActor->VisibilityOff();
    }

    ls->m_LastUpdateTime.Modified();
}

void xq_VesselTracer3D::ResetMapper(mitk::BaseRenderer* renderer)
{
    auto* ls = m_LSH.GetLocalStorage(renderer);
    ls->m_PathActor->VisibilityOff();
    ls->m_PointsActor->VisibilityOff();
}
