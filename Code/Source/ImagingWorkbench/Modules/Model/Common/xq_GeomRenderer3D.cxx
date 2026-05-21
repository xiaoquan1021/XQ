#include "xq_GeomRenderer3D.h"
#include "xq_Model.h"

#include <mitkBaseRenderer.h>
#include <mitkDataNode.h>
#include <mitkColorProperty.h>
#include <mitkProperties.h>

#include <vtkPolyData.h>
#include <vtkCellData.h>
#include <vtkIntArray.h>
#include <vtkProperty.h>
#include <vtkUnsignedCharArray.h>

#include <array>

xq_GeomRenderer3D::LocalStorage::LocalStorage()
{
    m_Actor = vtkSmartPointer<vtkActor>::New();
    m_Mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    m_FaceLUT = vtkSmartPointer<vtkLookupTable>::New();
    m_Actor->SetMapper(m_Mapper);
}

vtkProp* xq_GeomRenderer3D::GetVtkProp(mitk::BaseRenderer* renderer)
{
    return m_LSH.GetLocalStorage(renderer)->m_Actor;
}

void xq_GeomRenderer3D::GenerateDataForRenderer(mitk::BaseRenderer* renderer)
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

    ls->m_Mapper->SetInputData(polyData);

    auto* faceIds = vtkIntArray::SafeDownCast(
        polyData->GetCellData()->GetArray("FaceIds"));

    if (faceIds)
    {
        const int numFaces = element->GetFaceNumber();
        if (numFaces > 0)
        {
            ls->m_FaceLUT->SetNumberOfTableValues(numFaces + 1);
            ls->m_FaceLUT->SetRange(0, numFaces);
            ls->m_FaceLUT->Build();

            for (const auto& fi : element->GetAllFaceInfos())
            {
                if (fi.id >= 0 && fi.id <= numFaces)
                {
                    ls->m_FaceLUT->SetTableValue(fi.id,
                        fi.color[0], fi.color[1], fi.color[2], fi.opacity);
                }
            }

            ls->m_Mapper->SetScalarModeToUseCellFieldData();
            ls->m_Mapper->SelectColorArray("FaceIds");
            ls->m_Mapper->SetLookupTable(ls->m_FaceLUT);
            ls->m_Mapper->SetScalarRange(0, numFaces);
            ls->m_Mapper->ScalarVisibilityOn();
        }
        else
        {
            ls->m_Mapper->ScalarVisibilityOff();
        }
    }
    else
    {
        ls->m_Mapper->ScalarVisibilityOff();
    }

    // Face selection highlighting
    int selectedFaceId = -1;
    node->GetIntProperty("selectedFaceId", selectedFaceId, renderer);
    if (selectedFaceId >= 0 && faceIds)
    {
        auto colors = vtkSmartPointer<vtkUnsignedCharArray>::New();
        colors->SetNumberOfComponents(4);
        colors->SetName("FaceColors");
        colors->SetNumberOfTuples(polyData->GetNumberOfCells());

        constexpr std::array<unsigned char, 4> highlightColor = {255, 255, 0, 255};
        constexpr std::array<unsigned char, 4> defaultColor   = {200, 200, 200, 255};

        for (vtkIdType i = 0; i < polyData->GetNumberOfCells(); ++i)
        {
            const int fid = faceIds->GetValue(i);
            if (fid == selectedFaceId)
            {
                colors->SetTypedTuple(i, highlightColor.data());
            }
            else if (const auto* fi = element->GetFaceInfo(fid))
            {
                const std::array<unsigned char, 4> c = {
                    static_cast<unsigned char>(fi->color[0] * 255),
                    static_cast<unsigned char>(fi->color[1] * 255),
                    static_cast<unsigned char>(fi->color[2] * 255),
                    static_cast<unsigned char>(fi->opacity * 255)
                };
                colors->SetTypedTuple(i, c.data());
            }
            else
            {
                colors->SetTypedTuple(i, defaultColor.data());
            }
        }
        polyData->GetCellData()->SetScalars(colors);
        ls->m_Mapper->SetScalarModeToUseCellData();
        ls->m_Mapper->ScalarVisibilityOn();
    }

    bool wireframe = false;
    node->GetBoolProperty("wireframe", wireframe, renderer);
    ls->m_Actor->GetProperty()->SetRepresentationToSurface();
    if (wireframe)
    {
        ls->m_Actor->GetProperty()->SetRepresentationToWireframe();
    }

    ApplyColorAndOpacityProperties(renderer, ls->m_Actor);
    ls->m_Actor->VisibilityOn();
}

void xq_GeomRenderer3D::ApplyColorAndOpacityProperties(mitk::BaseRenderer* renderer,
                                                         vtkActor* actor)
{
    if (!actor)
        return;

    auto* node = this->GetDataNode();
    if (!node)
        return;

    float color[3] = {1.0f, 1.0f, 1.0f};
    node->GetColor(color, renderer);

    float opacity = 1.0f;
    node->GetOpacity(opacity, renderer);

    auto* polyData = vtkPolyData::SafeDownCast(actor->GetMapper()->GetInput());
    bool hasFaceColors = false;
    if (polyData && polyData->GetCellData())
    {
        hasFaceColors = polyData->GetCellData()->GetArray("FaceIds") != nullptr ||
                        polyData->GetCellData()->GetScalars() != nullptr;
    }

    if (!hasFaceColors)
    {
        actor->GetProperty()->SetColor(color[0], color[1], color[2]);
    }

    actor->GetProperty()->SetOpacity(opacity);
}
