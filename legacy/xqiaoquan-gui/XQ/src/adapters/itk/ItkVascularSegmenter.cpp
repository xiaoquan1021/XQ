#include "adapters/itk/ItkVascularSegmenter.h"

#include "core/geometry/IsoContourTracer.h"

#include "itkImage.h"
#include "itkImageRegionIterator.h"
#include "itkGradientMagnitudeRecursiveGaussianImageFilter.h"

#include "sv3_VascularPhaseOneLevelSetImageFilter.h"
#include "sv3_VascularPhaseTwoLevelSetImageFilter.h"

#include <cmath>
#include <cstddef>
#include <vector>

namespace xq {

namespace {

using FloatImage = itk::Image<float, 2>;

// Builds an itk::Image<float,2> in index/pixel space (spacing 1, origin 0) from
// the row-major W*H gray buffer (idx = y*W + x -> ITK index (x, y)). Index space
// matches SV's UseImageSpacingOff() pipeline; see memory
// sv-vascular-levelset-propagation-zero-required.
FloatImage::Pointer makeImage(const std::vector<double>& gray, int width, int height)
{
    auto img = FloatImage::New();
    FloatImage::SizeType sz;
    sz[0] = static_cast<itk::SizeValueType>(width);
    sz[1] = static_cast<itk::SizeValueType>(height);
    FloatImage::IndexType start;
    start[0] = 0;
    start[1] = 0;
    FloatImage::RegionType region;
    region.SetSize(sz);
    region.SetIndex(start);
    img->SetRegions(region);
    img->Allocate();

    itk::ImageRegionIterator<FloatImage> it(img, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        const FloatImage::IndexType idx = it.GetIndex();
        const std::size_t flat = static_cast<std::size_t>(idx[1])
                                     * static_cast<std::size_t>(width)
                                 + static_cast<std::size_t>(idx[0]);
        it.Set(static_cast<float>(gray[flat]));
    }
    return img;
}

// Signed-distance seed circle centred at pixel (sx, sy) with pixel radius rPx:
// phi0 = hypot(dx, dy) - rPx (inside negative, outside positive), the sign
// convention SV's SetInsideIsPositive(false) produces.
FloatImage::Pointer makeSeed(int width, int height, double sx, double sy, double rPx)
{
    auto img = FloatImage::New();
    FloatImage::SizeType sz;
    sz[0] = static_cast<itk::SizeValueType>(width);
    sz[1] = static_cast<itk::SizeValueType>(height);
    FloatImage::IndexType start;
    start[0] = 0;
    start[1] = 0;
    FloatImage::RegionType region;
    region.SetSize(sz);
    region.SetIndex(start);
    img->SetRegions(region);
    img->Allocate();

    itk::ImageRegionIterator<FloatImage> it(img, region);
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        const FloatImage::IndexType idx = it.GetIndex();
        const double dx = static_cast<double>(idx[0]) - sx;
        const double dy = static_cast<double>(idx[1]) - sy;
        it.Set(static_cast<float>(std::sqrt(dx * dx + dy * dy) - rPx));
    }
    return img;
}

// GradientMagnitudeRecursiveGaussian feature image (the SV feature image the
// filter's internal ExpNegative turns into a speed image).
FloatImage::Pointer makeFeature(FloatImage* gray, double sigma)
{
    using GradFilter =
        itk::GradientMagnitudeRecursiveGaussianImageFilter<FloatImage, FloatImage>;
    auto f = GradFilter::New();
    f->SetInput(gray);
    f->SetSigma(sigma);
    f->Update();
    return f->GetOutput();
}

// Flattens an itk::Image<float,2> to a row-major W*H double buffer (idx = y*W+x),
// the layout traceSeededIsoLoop expects.
std::vector<double> flatten(FloatImage* img, int width, int height)
{
    std::vector<double> out(static_cast<std::size_t>(width)
                            * static_cast<std::size_t>(height));
    itk::ImageRegionIterator<FloatImage> it(img, img->GetLargestPossibleRegion());
    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        const FloatImage::IndexType idx = it.GetIndex();
        const std::size_t flat = static_cast<std::size_t>(idx[1])
                                     * static_cast<std::size_t>(width)
                                 + static_cast<std::size_t>(idx[0]);
        out[flat] = static_cast<double>(it.Get());
    }
    return out;
}

} // namespace

std::vector<SectionPoint2D> ItkVascularSegmenter::segment(const std::vector<double>& gray,
                                                          int width, int height,
                                                          double pixelSizeMm,
                                                          const SectionPoint2D& seed,
                                                          const LevelSetParams& params)
{
    std::vector<SectionPoint2D> empty;
    if (width <= 1 || height <= 1 || pixelSizeMm <= 0.0
        || gray.size() != static_cast<std::size_t>(width)
                              * static_cast<std::size_t>(height)) {
        return empty;
    }

    try {
        // Seed (u,v) mm -> pixel (inverse of IsoContourTracer::pixelToSection):
        //   u = (x - (W-1)/2) * px  =>  x = u/px + (W-1)/2.
        const double cx = 0.5 * static_cast<double>(width - 1);
        const double cy = 0.5 * static_cast<double>(height - 1);
        const double sx = seed.u / pixelSizeMm + cx;
        const double sy = seed.v / pixelSizeMm + cy;
        double rPx = params.seedRadiusMm / pixelSizeMm;
        if (rPx < 1.0) {
            rPx = 1.0;   // at least a 1-pixel seed so the front has room to move
        }

        FloatImage::Pointer grayImg = makeImage(gray, width, height);

        // ---- Phase one (coarse localisation) -------------------------------
        FloatImage::Pointer feature1 = makeFeature(grayImg, params.sigmaFeature1);
        FloatImage::Pointer seedImg = makeSeed(width, height, sx, sy, rPx);

        using P1 = itk::VascularPhaseOneLevelSetImageFilter<FloatImage, FloatImage>;
        auto p1 = P1::New();
        p1->SetFeatureImage(feature1);
        p1->SetInitialImage(seedImg);
        p1->UseImageSpacingOff();
        p1->SetAdvectionDerivativeSigma(0.0f);
        p1->SetRisingVelocityDecayModifier(static_cast<float>(params.expFactorRising));
        p1->SetFallingVelocityDecayModifier(static_cast<float>(params.expFactorFalling));
        // MUST be prop=0 (see memory sv-vascular-levelset-propagation-zero-required):
        // the filter only allocates the gradient/speed image on this path.
        p1->SetPropagationScaling(0.0);
        p1->SetCurvatureScaling(1.0);
        p1->SetAdvectionScaling(0.0);
        p1->SetEquilibriumCurvature(static_cast<float>(params.kc));
        p1->SetMaximumRMSError(params.maxRmsError1);
        p1->SetNumberOfIterations(static_cast<unsigned int>(params.maxIterations1 > 0
                                                                ? params.maxIterations1
                                                                : 1));
        p1->Update();
        FloatImage::Pointer front1 = p1->GetOutput();

        // ---- Phase two (curvature-threshold refinement, seeded by front1) --
        FloatImage::Pointer feature2 = makeFeature(grayImg, params.sigmaFeature2);

        using P2 = itk::VascularPhaseTwoLevelSetImageFilter<FloatImage, FloatImage>;
        auto p2 = P2::New();
        p2->SetFeatureImage(feature2);
        p2->SetInitialImage(front1);
        p2->UseImageSpacingOff();
        p2->SetAdvectionDerivativeSigma(0.0f);
        p2->SetPropagationScaling(0.0);
        p2->SetCurvatureScaling(1.0);
        p2->SetAdvectionScaling(0.0);
        p2->SetCurvataureLowerThreshold(params.kLower);
        p2->SetCurvataureUpperThreshold(params.kUpper);
        p2->SetMaximumRMSError(params.maxRmsError2);
        p2->SetNumberOfIterations(static_cast<unsigned int>(params.maxIterations2 > 0
                                                                ? params.maxIterations2
                                                                : 1));
        p2->Update();
        FloatImage::Pointer front2 = p2->GetOutput();

        // ---- Trace the zero level set --------------------------------------
        // Level-set inside is negative, so the vessel interior sits below iso=0;
        // traceSeededIsoLoop keeps the closed loop enclosing the seed.
        std::vector<double> field = flatten(front2, width, height);
        return traceSeededIsoLoop(field, width, height, pixelSizeMm, 0.0, seed);
    } catch (...) {
        return empty;
    }
}

} // namespace xq
