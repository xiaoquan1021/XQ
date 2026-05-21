#include "xq_MitkGridMapper2D.h"
#include "xq_MitkGrid.h"
#include "xq_Grid.h"

#include <mitkBaseRenderer.h>
#include <mitkDataNode.h>
#include <mitkPlaneGeometry.h>

#include <vtkProperty.h>
#include <vtkPolyData.h>
#include <vtkUnstructuredGrid.h>

xq_MitkGridMapper2D::LocalStorage::LocalStorage()
    : m_Actor(vtkSmartPointer<vtkActor>::New())
    , m_Mapper(vtkSmartPointer<vtkPolyDataMapper>::New())
    , m_Cutter(vtkSmartPointer<vtkCutter>::New())
    , m_CuttingPlane(vtkSmartPointer<vtkPlane>::New())
    , m_SurfaceFilter(vtkSmartPointer<vtkDataSetSurfaceFilter>::New())
{
    m_Actor->SetMapper(m_Mapper);
    m_Actor->GetProperty()->SetRepresentationToWireframe();
    m_Actor->GetProperty()->SetLineWidth(1.0);
}

vtkProp* xq_MitkGridMapper2D::GetVtkProp(mitk::BaseRenderer* renderer)
{
    return m_LSH.GetLocalStorage(renderer)->m_Actor;
}

void xq_MitkGridMapper2D::GenerateDataForRenderer(mitk::BaseRenderer* renderer)
{
    auto* ls = m_LSH.GetLocalStorage(renderer);

    auto* node = this->GetDataNode();
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

    auto* mitkMesh = dynamic_cast<xq_MitkGrid*>(node->GetData());
    if (!mitkMesh)
    {
        ls->m_Actor->VisibilityOff();
        return;
    }

    const auto timeStep = this->GetTimestep();
    auto* mesh = mitkMesh->GetMesh(timeStep);
    if (!mesh)
    {
        ls->m_Actor->VisibilityOff();
        return;
    }

    auto* volumeMesh = mesh->GetVolumeMesh();
    if (!volumeMesh || volumeMesh->GetNumberOfCells() == 0)
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

    // Configure slice cutting plane
    const auto origin = planeGeometry->GetOrigin();
    const auto normal = planeGeometry->GetNormal();
    ls->m_CuttingPlane->SetOrigin(origin[0], origin[1], origin[2]);
    ls->m_CuttingPlane->SetNormal(normal[0], normal[1], normal[2]);

    ls->m_SurfaceFilter->SetInputData(volumeMesh);
    ls->m_SurfaceFilter->Update();

    ls->m_Cutter->SetInputConnection(ls->m_SurfaceFilter->GetOutputPort());
    ls->m_Cutter->SetCutFunction(ls->m_CuttingPlane);
    ls->m_Cutter->Update();

    ls->m_Mapper->SetInputConnection(ls->m_Cutter->GetOutputPort());

    // Apply visual properties
    float color[3] = {0.0f, 1.0f, 0.0f};
    node->GetColor(color, renderer);
    ls->m_Actor->GetProperty()->SetColor(color[0], color[1], color[2]);

    float opacity = 1.0f;
    node->GetOpacity(opacity, renderer);
    ls->m_Actor->GetProperty()->SetOpacity(opacity);

    ls->m_Actor->VisibilityOn();
    ls->m_LastUpdateTime.Modified();
}
