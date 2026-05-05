#include "xq_ThresholdContour.h"
#include "xq_SegmentationUtils.h"

#include <mitkExtractSliceFilter.h>

#include <vtkImageThreshold.h>
#include <vtkMarchingCubes.h>
#include <vtkMarchingSquares.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkSmartPointer.h>
#include <vtkStripper.h>

#include <limits>

namespace
{

vtkSmartPointer<vtkPolyData> ConvertImageSurfaceToWorld(
    vtkPolyData* surface,
    const mitk::Image* image,
    vtkImageData* vtkImage)
{
    if (!surface || !image || !image->GetGeometry() || !vtkImage)
        return nullptr;

    auto worldSurface = vtkSmartPointer<vtkPolyData>::New();
    worldSurface->DeepCopy(surface);

    auto worldPoints = vtkSmartPointer<vtkPoints>::New();
    worldPoints->SetDataTypeToDouble();
    worldPoints->SetNumberOfPoints(surface->GetNumberOfPoints());

    double vtkOrigin[3] = {0.0, 0.0, 0.0};
    double vtkSpacing[3] = {1.0, 1.0, 1.0};
    vtkImage->GetOrigin(vtkOrigin);
    vtkImage->GetSpacing(vtkSpacing);

    for (vtkIdType i = 0; i < surface->GetNumberOfPoints(); ++i)
    {
        double rawPoint[3] = {0.0, 0.0, 0.0};
        surface->GetPoint(i, rawPoint);

        mitk::Point3D imageIndex;
        for (int d = 0; d < 3; ++d)
        {
            imageIndex[d] = vtkSpacing[d] != 0.0
                ? (rawPoint[d] - vtkOrigin[d]) / vtkSpacing[d]
                : rawPoint[d] - vtkOrigin[d];
        }

        mitk::Point3D worldPoint;
        image->GetGeometry()->IndexToWorld(imageIndex, worldPoint);
        worldPoints->SetPoint(i, worldPoint[0], worldPoint[1], worldPoint[2]);
    }

    worldSurface->SetPoints(worldPoints);
    return worldSurface;
}

mitk::Image::Pointer ExtractSliceFromImage(
    mitk::Image* image,
    const mitk::PlaneGeometry* contourPlane)
{
    if (!image || !contourPlane)
        return nullptr;

    auto extractor = mitk::ExtractSliceFilter::New();
    extractor->SetInput(image);
    extractor->SetTimeStep(0);
    extractor->SetWorldGeometry(contourPlane);
    extractor->SetOutputDimensionality(2);
    extractor->SetVtkOutputRequest(false);
    extractor->SetInterpolationMode(mitk::ExtractSliceFilter::RESLICE_LINEAR);
    extractor->Update();
    return extractor->GetOutput();
}

std::vector<mitk::Point3D> ExtractLargestSliceContour(
    vtkPolyData* sliceContour,
    const mitk::Image* sliceImage,
    vtkImageData* sliceVtk)
{
    if (!sliceContour || !sliceImage || !sliceVtk)
        return {};

    auto worldPolyData = ConvertImageSurfaceToWorld(sliceContour, sliceImage, sliceVtk);
    if (!worldPolyData || !worldPolyData->GetPoints() || !worldPolyData->GetLines())
        return {};

    auto stripper = vtkSmartPointer<vtkStripper>::New();
    stripper->SetInputData(worldPolyData);
    stripper->JoinContiguousSegmentsOn();
    stripper->Update();

    vtkPolyData* linesPolyData = stripper->GetOutput();
    vtkCellArray* lines = linesPolyData ? linesPolyData->GetLines() : nullptr;
    if (!lines)
        return {};

    lines->InitTraversal();
    vtkIdType npts = 0;
    const vtkIdType* ids = nullptr;

    std::vector<mitk::Point3D> bestContour;
    double bestArea = -std::numeric_limits<double>::max();

    while (lines->GetNextCell(npts, ids))
    {
        if (npts < 4)
            continue;

        std::vector<mitk::Point3D> contour;
        contour.reserve(static_cast<size_t>(npts));
        for (vtkIdType i = 0; i < npts; ++i)
        {
            double point[3];
            linesPolyData->GetPoint(ids[i], point);
            mitk::Point3D mitkPoint;
            mitkPoint[0] = point[0];
            mitkPoint[1] = point[1];
            mitkPoint[2] = point[2];
            contour.push_back(mitkPoint);
        }

        const auto dx = contour.front()[0] - contour.back()[0];
        const auto dy = contour.front()[1] - contour.back()[1];
        const auto dz = contour.front()[2] - contour.back()[2];
        const auto closingDistance = std::sqrt(dx * dx + dy * dy + dz * dz);

        if (contour.size() >= 2 &&
            xq_SegmentationUtils::CalculateContourPerimeter(contour) > 0.0 &&
            closingDistance <= 1e-6)
        {
            contour.pop_back();
        }

        if (contour.size() < 3)
            continue;

        const double area = xq_SegmentationUtils::CalculateContourArea(contour);
        if (area > bestArea)
        {
            bestArea = area;
            bestContour = std::move(contour);
        }
    }

    return bestContour;
}

std::vector<mitk::Point3D> ExtractContourFromThresholdedSurface(
    const mitk::Image* image,
    vtkImageData* vtkImage,
    const mitk::PlaneGeometry* planeGeometry,
    double thresholdValue)
{
    if (!image || !vtkImage || !planeGeometry)
        return {};

    auto threshold = vtkSmartPointer<vtkImageThreshold>::New();
    threshold->SetInputData(vtkImage);
    threshold->ThresholdByUpper(thresholdValue);
    threshold->SetInValue(1.0);
    threshold->SetOutValue(0.0);
    threshold->ReplaceInOn();
    threshold->ReplaceOutOn();
    threshold->SetOutputScalarTypeToDouble();
    threshold->Update();

    auto contourFilter = vtkSmartPointer<vtkMarchingCubes>::New();
    contourFilter->SetInputConnection(threshold->GetOutputPort());
    contourFilter->SetValue(0, 0.5);
    contourFilter->Update();

    auto* polyData = contourFilter->GetOutput();
    if (!polyData)
        return {};

    auto worldPolyData = ConvertImageSurfaceToWorld(polyData, image, vtkImage);
    if (!worldPolyData)
        return {};

    return xq_SegmentationUtils::ExtractSurfaceContourOnPlane(
        worldPolyData, planeGeometry);
}

} // namespace

xq_ThresholdContour::xq_ThresholdContour() = default;

xq_ThresholdContour::xq_ThresholdContour(const xq_ThresholdContour& other)
    : mitk::ContourModel(other)
    , m_ThresholdValue(other.m_ThresholdValue)
    , m_ImageData(other.m_ImageData)
    , m_PlaneGeometry(other.m_PlaneGeometry)
    , m_ContourPoints(other.m_ContourPoints)
{
}

void xq_ThresholdContour::SetThresholdValue(double val)
{
    m_ThresholdValue = val;
}

double xq_ThresholdContour::GetThresholdValue() const
{
    return m_ThresholdValue;
}

void xq_ThresholdContour::SetImageData(mitk::Image::Pointer image)
{
    m_ImageData = image;
}

mitk::Image::Pointer xq_ThresholdContour::GetImageData() const
{
    return m_ImageData;
}

void xq_ThresholdContour::SetPlaneGeometry(const mitk::PlaneGeometry* planeGeometry)
{
    m_PlaneGeometry = planeGeometry;
}

const std::vector<mitk::Point3D>& xq_ThresholdContour::GetContourPoints() const
{
    return m_ContourPoints;
}

void xq_ThresholdContour::GenerateContourPoints()
{
    m_ContourPoints.clear();

    if (!m_ImageData || !m_PlaneGeometry)
        return;

    // Extract contours from the orthogonal reslice itself rather than building
    // a 3D isosurface first. Using the 3D route here made lumen slices look
    // voxel-cross-like and destabilized downstream lofting.
    auto sliceImage = ExtractSliceFromImage(m_ImageData, m_PlaneGeometry);
    if (sliceImage.IsNull())
        return;

    auto* vtkImage = sliceImage->GetVtkImageData();
    if (!vtkImage)
        return;

    auto threshold = vtkSmartPointer<vtkImageThreshold>::New();
    threshold->SetInputData(vtkImage);
    threshold->ThresholdByUpper(m_ThresholdValue);
    threshold->SetInValue(1.0);
    threshold->SetOutValue(0.0);
    threshold->ReplaceInOn();
    threshold->ReplaceOutOn();
    threshold->SetOutputScalarTypeToDouble();
    threshold->Update();

    auto contourFilter = vtkSmartPointer<vtkMarchingSquares>::New();
    contourFilter->SetInputConnection(threshold->GetOutputPort());
    contourFilter->SetValue(0, 0.5);
    contourFilter->Update();

    auto sliceContour = ExtractLargestSliceContour(
        contourFilter->GetOutput(), sliceImage, vtkImage);
    auto bestContour = sliceContour;
    auto bestArea = xq_SegmentationUtils::CalculateContourArea(bestContour);

    auto* inputVtkImage = m_ImageData->GetVtkImageData();
    if (inputVtkImage)
    {
        auto fallbackContour = ExtractContourFromThresholdedSurface(
            m_ImageData, inputVtkImage, m_PlaneGeometry, m_ThresholdValue);
        const auto fallbackArea = xq_SegmentationUtils::CalculateContourArea(fallbackContour);
        if (fallbackArea > bestArea)
        {
            bestContour = std::move(fallbackContour);
            bestArea = fallbackArea;
        }
    }

    if (bestContour.size() < 8 && bestArea < 1.0)
        return;

    m_ContourPoints = std::move(bestContour);
}
