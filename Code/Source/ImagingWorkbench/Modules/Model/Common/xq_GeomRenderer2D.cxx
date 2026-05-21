#include "xq_GeomRenderer2D.h"
#include "xq_Model.h"

#include <mitkBaseRenderer.h>
#include <mitkDataNode.h>
#include <mitkPlaneGeometry.h>
#include <mitkColorProperty.h>
#include <mitkProperties.h>

#include <vtkProperty.h>
#include <vtkPolyData.h>

xq_GeomRenderer2D::LocalStorage::LocalStorage()
{
    m_Actor = vtkSmartPointer<vtkActor>::New();
    m_Mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_Cutter = vtkSmartPointer<vtkCutter>::New();
    m_CuttingPlane = vtkSmartPointer<vtkPlane>::New();
    m_Actor->SetMapper(m_Mapper);
}

vtkProp* xq_GeomRenderer2D::GetVtkProp(mitk::BaseRenderer* renderer)
{
    return m_LSH.GetLocalStorage(renderer)->m_Actor;
}

void xq_GeomRenderer2D::GenerateDataForRenderer(mitk::BaseRenderer* renderer)
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

    auto* model = dynamic_cast<xq_Model*>(node->GetData());
    if (!model)
    {
        ls->m_Actor->VisibilityOff();
        return;
    }

    auto* element = model->GetModelElement(0);
    if (!element)
    {
        ls->m_Actor->VisibilityOff();
        return;
    }

    auto polyData = element->GetWholeVtkPolyData();
    if (!polyData || polyData->GetNumberOfPoints() == 0)
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
    const auto normal = planeGeometry->GetNormal();

    ls->m_CuttingPlane->SetOrigin(origin[0], origin[1], origin[2]);
    ls->m_CuttingPlane->SetNormal(normal[0], normal[1], normal[2]);

    ls->m_Cutter->SetInputData(polyData);
    ls->m_Cutter->SetCutFunction(ls->m_CuttingPlane);
    ls->m_Cutter->Update();

    ls->m_Mapper->SetInputConnection(ls->m_Cutter->GetOutputPort());

    float color[3] = {1.0f, 1.0f, 1.0f};
    node->GetColor(color, renderer);
    ls->m_Actor->GetProperty()->SetColor(color[0], color[1], color[2]);

    float opacity = 1.0f;
    node->GetOpacity(opacity, renderer);
    ls->m_Actor->GetProperty()->SetOpacity(opacity);

    ls->m_Actor->GetProperty()->SetLineWidth(2.0);
    ls->m_Actor->VisibilityOn();
}
