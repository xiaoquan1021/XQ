#include "xq_GridLegacyIO.h"
#include "xq_Grid.h"
#include "xq_GridFactory.h"

#include <vtkSmartPointer.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkXMLUnstructuredGridWriter.h>
#include <vtkUnstructuredGrid.h>
#include <vtkGeometryFilter.h>

#include <string>

namespace xq::grid
{

xq_MitkGrid::Pointer ReadMesh(std::string_view filename)
{
    const std::string fname(filename);

    auto reader = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
    reader->SetFileName(fname.c_str());
    reader->Update();

    auto* ugrid = reader->GetOutput();
    if (!ugrid || ugrid->GetNumberOfPoints() == 0)
    {
        return nullptr;
    }

    auto mesh = xq_GridFactory::CreateMesh("TetGen");
    if (!mesh)
    {
        return nullptr;
    }

    mesh->GetVolumeMesh()->DeepCopy(ugrid);

    auto geomFilter = vtkSmartPointer<vtkGeometryFilter>::New();
    geomFilter->SetInputData(ugrid);
    geomFilter->Update();
    mesh->GetSurfaceMesh()->DeepCopy(geomFilter->GetOutput());

    auto mitkMesh = xq_MitkGrid::New();
    mitkMesh->SetMesh(mesh.release(), 0);

    return mitkMesh;
}

bool WriteMesh(std::string_view filename, const xq_MitkGrid* mitkMesh)
{
    if (!mitkMesh)
    {
        return false;
    }

    auto* mesh = mitkMesh->GetMesh(0);
    if (!mesh)
    {
        return false;
    }

    auto* ugrid = mesh->GetVolumeMesh();
    if (!ugrid || ugrid->GetNumberOfCells() == 0)
    {
        return false;
    }

    const std::string fname(filename);

    auto writer = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
    writer->SetFileName(fname.c_str());
    writer->SetInputData(ugrid);
    writer->Write();

    return true;
}

} // namespace xq::grid
