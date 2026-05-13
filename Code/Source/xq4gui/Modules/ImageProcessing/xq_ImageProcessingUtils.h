#pragma once

#include <xqModuleImageProcessingExports.h>

#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <itkImage.h>

#include <array>
#include <string>
#include <vector>

// Structured result for all image processing operations.
// Caller checks `ok`; if false, `diagnostic` carries a human-readable reason.
struct XQMODULEIMAGEPROCESSING_EXPORT xq_ImageResult
{
    bool ok = false;
    std::string diagnostic;

    vtkSmartPointer<vtkImageData> image;
    vtkSmartPointer<vtkPolyData> surface;
};

class XQMODULEIMAGEPROCESSING_EXPORT xq_ImageProcessingUtils
{
public:
    using ItkFloat3D = itk::Image<float, 3>;

    // Conversion between VTK image data and ITK float-3D image.
    // Returns nullptr on failure (null input, incompatible data).
    static ItkFloat3D::Pointer VtkImageToItkFloat3D(vtkImageData* vtkImage);
    static vtkSmartPointer<vtkImageData> ItkFloat3DToVtkImage(ItkFloat3D::Pointer itkImage);

    // Binary threshold: pixels in [lower, upper] → insideValue; else → outsideValue.
    static xq_ImageResult BinaryThreshold(
        vtkImageData* input,
        double lower, double upper,
        double insideValue, double outsideValue);

    // Region-growing connected threshold. Seed indices are (x,y,z) in voxel space.
    // Only takes the connected component reachable from the provided seeds.
    static xq_ImageResult ConnectedThreshold(
        vtkImageData* input,
        double lower, double upper,
        const std::vector<std::array<int, 3>>& seeds);

    // Recursive Gaussian smoothing.
    static xq_ImageResult SmoothGaussian(vtkImageData* input, double sigma);

    // Binary morphological opening followed by closing with a ball of given radius.
    // Input must already be a binary image (0/1 or 0/nonzero).
    static xq_ImageResult MorphologicalOpenClose(vtkImageData* input, int radius);

    // Crop to a sub-region: (ox,oy,oz) origin, (sx,sy,sz) size in voxels.
    static xq_ImageResult Crop(
        vtkImageData* input,
        int ox, int oy, int oz,
        int sx, int sy, int sz);

    // Resample to a new physical spacing. Preserves origin and direction.
    static xq_ImageResult Resample(
        vtkImageData* input,
        double spacingX, double spacingY, double spacingZ);

    // Marching cubes isosurface extraction at the given isovalue.
    // Returns result.surface as vtkPolyData.
    static xq_ImageResult MarchingCubes(vtkImageData* input, double isovalue);
};
