// XQ Seg3DUtils: composable VTK pipeline builder for volumetric surface extraction
#pragma once

#include <xqModuleSegmentationExports.h>
#include <mitkImage.h>
#include <mitkPoint.h>
#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <vector>

class XQMODULESEGMENTATION_EXPORT xq_Seg3DUtils
{
public:
    static vtkSmartPointer<vtkPolyData> ThresholdSegmentation(
        vtkImageData* imageData,
        double lowerThreshold,
        double upperThreshold);

    static vtkSmartPointer<vtkPolyData> RegionGrowingSegmentation(
        vtkImageData* imageData,
        const std::vector<mitk::Point3D>& seedPoints,
        double lowerThreshold,
        double upperThreshold);

    static vtkSmartPointer<vtkPolyData> MarchingCubes(
        vtkImageData* imageData,
        double isoValue);

    static vtkSmartPointer<vtkPolyData> SmoothSurface(
        vtkSmartPointer<vtkPolyData> input,
        int iterations = 20,
        double relaxationFactor = 0.1);

    static vtkSmartPointer<vtkPolyData> DecimateSurface(
        vtkSmartPointer<vtkPolyData> input,
        double targetReduction = 0.5);

    static vtkSmartPointer<vtkPolyData> FillHoles(
        vtkSmartPointer<vtkPolyData> input,
        double holeSize = 100.0);

    static vtkSmartPointer<vtkPolyData> ComputeNormals(
        vtkSmartPointer<vtkPolyData> input);
};
