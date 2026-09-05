#include <itkBinaryThinningImageFilter3D.h>
#include <itkImage.h>
#include <itkImageRegionConstIterator.h>
#include <itkImageRegionIterator.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

int main()
{
    constexpr unsigned int Dimension = 3;
    using Image = itk::Image<unsigned char, Dimension>;

    Image::SizeType size;
    size.Fill(25);
    Image::IndexType start;
    start.Fill(0);
    Image::RegionType region;
    region.SetIndex(start);
    region.SetSize(size);

    auto image = Image::New();
    image->SetRegions(region);
    image->Allocate();
    image->FillBuffer(0);

    std::size_t inputForeground = 0;
    itk::ImageRegionIterator<Image> writer(image, region);
    for (writer.GoToBegin(); !writer.IsAtEnd(); ++writer) {
        const auto index = writer.GetIndex();
        const int dx = static_cast<int>(index[0]) - 12;
        const int dy = static_cast<int>(index[1]) - 12;
        const bool inside = index[2] >= 3 && index[2] <= 21 && dx * dx + dy * dy <= 16;
        if (inside) {
            writer.Set(1);
            ++inputForeground;
        }
    }

    using Thinning = itk::BinaryThinningImageFilter3D<Image, Image>;
    auto thinning = Thinning::New();
    thinning->SetInput(image);
    thinning->Update();

    std::size_t outputForeground = 0;
    long minimumZ = std::numeric_limits<long>::max();
    long maximumZ = std::numeric_limits<long>::lowest();
    double maximumRadialDistance = 0.0;
    itk::ImageRegionConstIterator<Image> reader(thinning->GetOutput(), region);
    for (reader.GoToBegin(); !reader.IsAtEnd(); ++reader) {
        if (reader.Get() == 0) {
            continue;
        }
        ++outputForeground;
        const auto index = reader.GetIndex();
        minimumZ = std::min(minimumZ, static_cast<long>(index[2]));
        maximumZ = std::max(maximumZ, static_cast<long>(index[2]));
        const double dx = static_cast<double>(index[0]) - 12.0;
        const double dy = static_cast<double>(index[1]) - 12.0;
        maximumRadialDistance = std::max(maximumRadialDistance, std::sqrt(dx * dx + dy * dy));
    }

    const long zSpan = outputForeground == 0 ? 0 : maximumZ - minimumZ;
    const bool oneVoxelScale = outputForeground > 0 && maximumRadialDistance <= 1.5;
    const bool spansVolume = zSpan >= 10;
    const bool reduced = outputForeground >= 5 && outputForeground < inputForeground / 4;
    if (!oneVoxelScale || !spansVolume || !reduced) {
        std::cerr << "3D thinning output failed topology/scale checks"
                  << " input=" << inputForeground
                  << " output=" << outputForeground
                  << " z_span=" << zSpan
                  << " radial_max=" << maximumRadialDistance << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "PASS itk_thickness3d_probe"
              << " input=" << inputForeground
              << " skeleton=" << outputForeground
              << " z_span=" << zSpan
              << " radial_max=" << maximumRadialDistance << '\n';
    return EXIT_SUCCESS;
}
