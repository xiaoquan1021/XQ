#include "xq_GeometryLegacyIO.h"
#include "xq_PolyGeometry.h"

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkCellData.h>

#include <memory>

namespace xq::legacy_io
{

xq_Model::Pointer readModel(const std::string& filename)
{
    auto reader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
    reader->SetFileName(filename.c_str());
    reader->Update();

    auto* polyData = reader->GetOutput();
    if (!polyData || polyData->GetNumberOfPoints() == 0)
    {
        return nullptr;
    }

    auto element = std::make_unique<xq_PolyGeometry>();
    element->SetWholeVtkPolyData(polyData);

    // Create a default face if no FaceIds exist
    if (!polyData->GetCellData() ||
        !polyData->GetCellData()->GetArray("FaceIds"))
    {
        FaceInfo fi;
        fi.id = 1;
        fi.name = "surface";
        fi.type = "wall";
        element->SetFaceInfo(fi.id, fi);
    }

    auto model = xq_Model::New();
    model->SetModelElement(std::move(element), 0);
    model->SetType("PolyData");

    return model;
}

void writeModel(const std::string& filename, const xq_Model* model)
{
    if (!model)
        return;

    auto* element = model->GetModelElement(0);
    if (!element)
        return;

    auto polyData = element->GetWholeVtkPolyData();
    if (!polyData)
        return;

    auto writer = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
    writer->SetFileName(filename.c_str());
    writer->SetInputData(polyData);
    writer->Write();
}

} // namespace xq::legacy_io
