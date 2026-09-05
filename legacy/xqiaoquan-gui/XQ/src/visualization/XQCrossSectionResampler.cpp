#include "visualization/XQCrossSectionResampler.h"

#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>

#include <algorithm>
#include <cmath>

namespace xq {

// Cross-section resampler pipeline. A single vtkImageReslice is configured once
// and re-executed per pose: scrubbing the slider only touches the reslice axes
// and re-runs Update, never rebuilding the pipeline (the resident vtkImageData
// is never copied). The output is a centered 2D section perpendicular to the
// path tangent.
class XQCrossSectionResampler::Impl {
public:
    Impl()
        : spec_(xqDefaultCrossSectionSpec())
    {
        reslice_->SetInterpolationModeToCubic();
        reslice_->SetOutputDimensionality(2);
        // Voxels outside the input are clamped/black rather than mirrored, so a
        // section near the volume border does not fold data back in.
        reslice_->SetBackgroundLevel(0.0);
    }

    void setImage(vtkImageData* image)
    {
        image_ = image;
        if (image_ != nullptr) {
            reslice_->SetInputData(image_);
        } else {
            reslice_->SetInputData(nullptr);
            hasOutput_ = false;
        }
    }

    bool hasImage() const { return image_ != nullptr; }

    void setOutputSpec(const XQCrossSectionOutputSpec& spec) { spec_ = spec; }
    XQCrossSectionOutputSpec outputSpec() const { return spec_; }

    void setPose(const XQCrossSectionPose& pose)
    {
        // The frame is recorded regardless of image presence so tests can assert
        // the axis mapping without volume data.
        for (int i = 0; i < 3; ++i) {
            frame_.origin[i] = pose.origin[i];
            frame_.normal[i] = pose.tangent[i];   // section out-of-plane axis
            frame_.xAxis[i] = pose.normal[i];      // section in-plane u axis
            frame_.yAxis[i] = pose.binormal[i];    // section in-plane v axis
        }
        frameValid_ = true;

        if (image_ == nullptr) {
            hasOutput_ = false;
            return;
        }

        // Reslice axes: x = normal (u), y = binormal (v), z = tangent (section
        // normal). SetResliceAxesDirectionCosines takes the x, y, z direction
        // vectors as its three arguments.
        reslice_->SetResliceAxesDirectionCosines(pose.normal, pose.binormal, pose.tangent);
        reslice_->SetResliceAxesOrigin(pose.origin[0], pose.origin[1], pose.origin[2]);

        // Centered square section: N x N pixels at pixelSizeMm, with the reslice
        // origin (pose.origin) at the section center. Pixel p maps to the local
        // coordinate outputOrigin + p * spacing; choosing outputOrigin =
        // -(N-1)/2 * spacing puts local (0,0) -- i.e. pose.origin -- at pixel
        // ((N-1)/2, (N-1)/2). For an odd N that is exactly the center pixel.
        const double px = spec_.pixelSizeMm > 0.0 ? spec_.pixelSizeMm : 0.2;
        const double sizeMm = spec_.sizeMm > 0.0 ? spec_.sizeMm : 40.0;
        int n = static_cast<int>(std::lround(sizeMm / px));
        n = std::max(n, 1);
        const double half = 0.5 * static_cast<double>(n - 1) * px;

        reslice_->SetOutputSpacing(px, px, 1.0);
        reslice_->SetOutputExtent(0, n - 1, 0, n - 1, 0, 0);
        reslice_->SetOutputOrigin(-half, -half, 0.0);

        reslice_->Update();
        hasOutput_ = true;
    }

    void* outputImageHandle() const
    {
        if (!hasOutput_) {
            return nullptr;
        }
        return reslice_->GetOutput();
    }

    XQCrossSectionFrame lastFrame() const
    {
        if (!frameValid_) {
            return XQCrossSectionFrame{};
        }
        return frame_;
    }

private:
    vtkNew<vtkImageReslice> reslice_;
    vtkImageData* image_ = nullptr;  // borrowed; owned by XQRenderScene
    XQCrossSectionOutputSpec spec_;
    XQCrossSectionFrame frame_{};
    bool frameValid_ = false;
    bool hasOutput_ = false;
};

XQCrossSectionResampler::XQCrossSectionResampler()
    : impl_(std::make_unique<Impl>())
{
}

XQCrossSectionResampler::~XQCrossSectionResampler() = default;

void XQCrossSectionResampler::setImage(void* image)
{
    impl_->setImage(static_cast<vtkImageData*>(image));
}

bool XQCrossSectionResampler::hasImage() const
{
    return impl_->hasImage();
}

void XQCrossSectionResampler::setOutputSpec(const XQCrossSectionOutputSpec& spec)
{
    impl_->setOutputSpec(spec);
}

XQCrossSectionOutputSpec XQCrossSectionResampler::outputSpec() const
{
    return impl_->outputSpec();
}

void XQCrossSectionResampler::setPose(const XQCrossSectionPose& pose)
{
    impl_->setPose(pose);
}

void* XQCrossSectionResampler::outputImageHandle() const
{
    return impl_->outputImageHandle();
}

XQCrossSectionFrame XQCrossSectionResampler::lastFrame() const
{
    return impl_->lastFrame();
}

} // namespace xq
