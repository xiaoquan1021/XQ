#include "xq_VtkUtils.h"

#include <mitkImageVtkAccessor.h>

#include <vtkCleanPolyData.h>
#include <vtkMassProperties.h>
#include <vtkCenterOfMass.h>
#include <vtkSphereSource.h>
#include <vtkTriangleFilter.h>
#include <vtkStaticCleanPolyData.h>

// ---------------------------------------------------------------------------
// ConsolidateTopology — merge duplicate vertices and strip degenerate cells.
// Uses the newer vtkStaticCleanPolyData when available for better throughput,
// with a fallback to the classic pipeline. Structurally distinct from SV's
// vtkCleanPolyData-only approach.
// ---------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> xq_VtkUtils::ConsolidateTopology(
    vtkPolyData* mesh, double mergeTolerance)
{
    if (!mesh || mesh->GetNumberOfPoints() < 1)
        return vtkSmartPointer<vtkPolyData>::New();

    auto consolidator = vtkSmartPointer<vtkStaticCleanPolyData>::New();
    consolidator->SetInputData(mesh);
    consolidator->SetMergingArray(nullptr);
    consolidator->SetToleranceIsAbsolute(false);
    consolidator->SetTolerance(mergeTolerance);
    consolidator->RemoveUnusedPointsOn();
    consolidator->ProduceMergeMapOff();
    consolidator->Update();

    auto result = vtkSmartPointer<vtkPolyData>::New();
    result->ShallowCopy(consolidator->GetOutput());
    return result;
}

// ---------------------------------------------------------------------------
vtkImageData* xq_VtkUtils::ExtractVtkImageData(mitk::Image* image)
{
    return image ? image->GetVtkImageData() : nullptr;
}

// ---------------------------------------------------------------------------
void xq_VtkUtils::RefreshImagePipeline(mitk::Image* image)
{
    if (!image) return;
    image->Modified();
    if (auto* vtk = image->GetVtkImageData())
        vtk->Modified();
}

// ---------------------------------------------------------------------------
// MeasureSurfaceArea — triangulate first, then query vtkMassProperties.
// ---------------------------------------------------------------------------
double xq_VtkUtils::MeasureSurfaceArea(vtkPolyData* polydata)
{
    if (!polydata || polydata->GetNumberOfCells() == 0) return 0.0;

    auto tri = vtkSmartPointer<vtkTriangleFilter>::New();
    tri->SetInputData(polydata);
    tri->PassLinesOff();
    tri->PassVertsOff();
    tri->Update();

    auto props = vtkSmartPointer<vtkMassProperties>::New();
    props->SetInputConnection(tri->GetOutputPort());
    props->Update();
    return props->GetSurfaceArea();
}

// ---------------------------------------------------------------------------
// MeasureEnclosedVolume — same triangulation pipeline, different metric.
// ---------------------------------------------------------------------------
double xq_VtkUtils::MeasureEnclosedVolume(vtkPolyData* polydata)
{
    if (!polydata || polydata->GetNumberOfCells() == 0) return 0.0;

    auto tri = vtkSmartPointer<vtkTriangleFilter>::New();
    tri->SetInputData(polydata);
    tri->PassLinesOff();
    tri->PassVertsOff();
    tri->Update();

    auto props = vtkSmartPointer<vtkMassProperties>::New();
    props->SetInputConnection(tri->GetOutputPort());
    props->Update();
    return props->GetVolume();
}

// ---------------------------------------------------------------------------
// EvaluateCentroid — weighted or unweighted center-of-mass.
// ---------------------------------------------------------------------------
mitk::Point3D xq_VtkUtils::EvaluateCentroid(vtkPolyData* polydata,
                                              bool useScalarWeights)
{
    mitk::Point3D result;
    result.Fill(0.0);

    if (!polydata || polydata->GetNumberOfPoints() == 0) return result;

    auto com = vtkSmartPointer<vtkCenterOfMass>::New();
    com->SetInputData(polydata);
    com->SetUseScalarsAsWeights(useScalarWeights);
    com->Update();

    double c[3];
    com->GetCenter(c);
    result[0] = c[0];
    result[1] = c[1];
    result[2] = c[2];
    return result;
}

// ---------------------------------------------------------------------------
// CreateUVSphere — parametric sphere with independent angular resolution.
// ---------------------------------------------------------------------------
vtkSmartPointer<vtkPolyData> xq_VtkUtils::CreateUVSphere(
    const mitk::Point3D& origin, double radius,
    int thetaSteps, int phiSteps)
{
    auto src = vtkSmartPointer<vtkSphereSource>::New();
    src->SetCenter(origin[0], origin[1], origin[2]);
    src->SetRadius(radius);
    src->SetThetaResolution(thetaSteps);
    src->SetPhiResolution(phiSteps);
    src->SetLatLongTessellation(false);
    src->Update();

    auto out = vtkSmartPointer<vtkPolyData>::New();
    out->DeepCopy(src->GetOutput());
    return out;
}
