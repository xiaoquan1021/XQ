#ifndef XQ_VTK_UTILS_H
#define XQ_VTK_UTILS_H

#include <xqModuleCommonExports.h>

#include <mitkImage.h>
#include <mitkPoint.h>

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkImageData.h>

/// Static utility façade for common VTK mesh and image operations.
class XQMODULECOMMON_EXPORT xq_VtkUtils
{
public:
    /// Merge coincident vertices and remove degenerate cells.
    static vtkSmartPointer<vtkPolyData> ConsolidateTopology(
        vtkPolyData* mesh, double mergeTolerance = 1e-6);

    /// Convenience accessor for the underlying vtkImageData of a MITK image.
    static vtkImageData* ExtractVtkImageData(mitk::Image* image);

    /// Mark both the MITK image and its VTK pipeline as modified.
    static void RefreshImagePipeline(mitk::Image* image);

    /// Compute total surface area of a triangulated mesh.
    static double MeasureSurfaceArea(vtkPolyData* polydata);

    /// Compute enclosed volume of a closed triangulated mesh.
    static double MeasureEnclosedVolume(vtkPolyData* polydata);

    /// Compute the center-of-mass of a point cloud / mesh.
    static mitk::Point3D EvaluateCentroid(vtkPolyData* polydata,
                                           bool useScalarWeights = false);

    /// Generate a UV-sphere mesh centred at @p origin.
    static vtkSmartPointer<vtkPolyData> CreateUVSphere(
        const mitk::Point3D& origin, double radius,
        int thetaSteps = 20, int phiSteps = 20);
};

#endif // XQ_VTK_UTILS_H
