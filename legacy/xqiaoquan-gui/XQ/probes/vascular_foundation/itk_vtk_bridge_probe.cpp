#include <itkImage.h>
#include <itkImageRegionIterator.h>
#include <itkImageToVTKImageFilter.h>
#include <itkVTKImageToImageFilter.h>
#include <itkVersion.h>

#include <vtkDataArray.h>
#include <vtkImageData.h>
#include <vtkMatrix3x3.h>
#include <vtkPointData.h>
#include <vtkVersion.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

bool close(double actual, double expected, double tolerance = 1.0e-10)
{
    return std::abs(actual - expected) <= tolerance;
}

} // namespace

int main()
{
    constexpr unsigned int Dimension = 3;
    using Image = itk::Image<float, Dimension>;

    Image::SizeType size;
    size[0] = 7;
    size[1] = 8;
    size[2] = 9;
    Image::IndexType start;
    start.Fill(0);
    Image::RegionType region;
    region.SetIndex(start);
    region.SetSize(size);

    auto image = Image::New();
    image->SetRegions(region);
    Image::SpacingType spacing;
    spacing[0] = 0.7;
    spacing[1] = 1.1;
    spacing[2] = 2.3;
    image->SetSpacing(spacing);
    Image::PointType origin;
    origin[0] = 12.5;
    origin[1] = -8.75;
    origin[2] = 43.0;
    image->SetOrigin(origin);

    const double angle = 0.37;
    Image::DirectionType direction;
    direction.SetIdentity();
    direction[0][0] = std::cos(angle);
    direction[0][1] = -std::sin(angle);
    direction[1][0] = std::sin(angle);
    direction[1][1] = std::cos(angle);
    image->SetDirection(direction);
    image->Allocate();

    itk::ImageRegionIterator<Image> writer(image, region);
    for (writer.GoToBegin(); !writer.IsAtEnd(); ++writer) {
        const auto index = writer.GetIndex();
        writer.Set(static_cast<float>(index[0] + 10 * index[1] + 100 * index[2]) + 0.25F);
    }

    using ToVtk = itk::ImageToVTKImageFilter<Image>;
    auto toVtk = ToVtk::New();
    toVtk->SetInput(image);
    toVtk->Update();
    vtkImageData* vtkImage = toVtk->GetOutput();
    if (vtkImage == nullptr) {
        std::cerr << "ITK-to-VTK bridge returned null\n";
        return EXIT_FAILURE;
    }

    int dimensions[3] = {};
    vtkImage->GetDimensions(dimensions);
    double vtkSpacing[3] = {};
    vtkImage->GetSpacing(vtkSpacing);
    double vtkOrigin[3] = {};
    vtkImage->GetOrigin(vtkOrigin);
    vtkMatrix3x3* vtkDirection = vtkImage->GetDirectionMatrix();
    for (int axis = 0; axis < 3; ++axis) {
        if (dimensions[axis] != static_cast<int>(size[axis])
            || !close(vtkSpacing[axis], spacing[axis])
            || !close(vtkOrigin[axis], origin[axis])) {
            std::cerr << "ITK-to-VTK geometry mismatch on axis " << axis << '\n';
            return EXIT_FAILURE;
        }
        for (int component = 0; component < 3; ++component) {
            if (!close(vtkDirection->GetElement(axis, component), direction[axis][component])) {
                std::cerr << "ITK-to-VTK direction mismatch\n";
                return EXIT_FAILURE;
            }
        }
    }

    Image::IndexType selected;
    selected[0] = 2;
    selected[1] = 3;
    selected[2] = 4;
    Image::PointType itkPoint;
    image->TransformIndexToPhysicalPoint(selected, itkPoint);
    double vtkPoint[3] = {};
    vtkImage->TransformIndexToPhysicalPoint(selected[0], selected[1], selected[2], vtkPoint);
    for (int axis = 0; axis < 3; ++axis) {
        if (!close(vtkPoint[axis], itkPoint[axis])) {
            std::cerr << "ITK-to-VTK physical point mismatch\n";
            return EXIT_FAILURE;
        }
    }
    const float expectedValue = image->GetPixel(selected);
    const float vtkValue = static_cast<float>(vtkImage->GetScalarComponentAsDouble(
        selected[0], selected[1], selected[2], 0));
    if (!close(vtkValue, expectedValue, 1.0e-6)) {
        std::cerr << "ITK-to-VTK scalar mismatch\n";
        return EXIT_FAILURE;
    }

    using ToItk = itk::VTKImageToImageFilter<Image>;
    auto toItk = ToItk::New();
    toItk->SetInput(vtkImage);
    toItk->Update();
    Image* roundTrip = toItk->GetOutput();
    if (roundTrip == nullptr || roundTrip->GetLargestPossibleRegion().GetSize() != size) {
        std::cerr << "VTK-to-ITK bridge returned invalid dimensions\n";
        return EXIT_FAILURE;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (!close(roundTrip->GetSpacing()[axis], spacing[axis])
            || !close(roundTrip->GetOrigin()[axis], origin[axis])) {
            std::cerr << "VTK-to-ITK geometry mismatch\n";
            return EXIT_FAILURE;
        }
        for (int component = 0; component < 3; ++component) {
            if (!close(roundTrip->GetDirection()[axis][component], direction[axis][component])) {
                std::cerr << "VTK-to-ITK direction mismatch\n";
                return EXIT_FAILURE;
            }
        }
    }
    Image::PointType roundTripPoint;
    roundTrip->TransformIndexToPhysicalPoint(selected, roundTripPoint);
    for (int axis = 0; axis < 3; ++axis) {
        if (!close(roundTripPoint[axis], itkPoint[axis])) {
            std::cerr << "round-trip physical point mismatch\n";
            return EXIT_FAILURE;
        }
    }
    if (!close(roundTrip->GetPixel(selected), expectedValue, 1.0e-6)) {
        std::cerr << "round-trip scalar mismatch\n";
        return EXIT_FAILURE;
    }

    std::cout << "PASS itk_vtk_bridge_probe"
              << " itk=" << ITK_VERSION_MAJOR << '.' << ITK_VERSION_MINOR << '.' << ITK_VERSION_PATCH
              << " vtk=" << VTK_VERSION
              << " selected_physical=" << itkPoint[0] << ',' << itkPoint[1] << ',' << itkPoint[2]
              << " value=" << expectedValue << '\n';
    return EXIT_SUCCESS;
}
