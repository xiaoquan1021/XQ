#include "xq_MitkGridMapper3D.h"
#include "xq_MitkGrid.h"
#include "xq_Grid.h"

#include <mitkBaseRenderer.h>
#include <mitkDataNode.h>

#include <vtkProperty.h>
#include <vtkUnstructuredGrid.h>

xq_MitkGridMapper3D::LocalStorage::LocalStorage()
    : m_Actor(vtkSmartPointer<vtkActor>::New())
    , m_Mapper(vtkSmartPointer<vtkDataSetMapper>::New())
{
    m_Actor->SetMapper(m_Mapper);
}

vtkProp* xq_MitkGridMapper3D::GetVtkProp(mitk::BaseRenderer* renderer)
{
    return m_LSH.GetLocalStorage(renderer)->m_Actor;
}

void xq_MitkGridMapper3D::GenerateDataForRenderer(mitk::BaseRenderer* renderer)
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

    ls->m_Mapper->SetInputData(volumeMesh);

    bool wireframe = false;
    node->GetBoolProperty("mesh.wireframe", wireframe, renderer);

    if (wireframe)
    {
        ls->m_Actor->GetProperty()->SetRepresentationToWireframe();
    }
    else
    {
        ls->m_Actor->GetProperty()->SetRepresentationToSurface();
        ls->m_Actor->GetProperty()->EdgeVisibilityOn();
    }

    ApplyColorAndOpacityProperties(renderer, ls->m_Actor);

    ls->m_Actor->VisibilityOn();
    ls->m_LastUpdateTime.Modified();
}

void xq_MitkGridMapper3D::ApplyColorAndOpacityProperties(
    mitk::BaseRenderer* renderer, vtkActor* actor)
{
    auto* node = this->GetDataNode();
    if (!node || !actor)
    {
        return;
    }

    float color[3] = {0.0f, 0.8f, 0.2f};
    node->GetColor(color, renderer);
    actor->GetProperty()->SetColor(color[0], color[1], color[2]);

    float opacity = 1.0f;
    node->GetOpacity(opacity, renderer);
    actor->GetProperty()->SetOpacity(opacity);

    // Dimmed edge color for depth contrast
    actor->GetProperty()->SetEdgeColor(color[0] * 0.5, color[1] * 0.5, color[2] * 0.5);
}
