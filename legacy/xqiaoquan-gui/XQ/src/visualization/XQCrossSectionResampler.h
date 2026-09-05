#ifndef XQ_VISUALIZATION_CROSS_SECTION_RESAMPLER_H
#define XQ_VISUALIZATION_CROSS_SECTION_RESAMPLER_H

#include <memory>

namespace xq {

// Sampling pose along a centerline path (POD, VTK-free). Origin is the world
// position of the section center; the three axes are the PathSamplePoint frame:
// tangent (path direction), normal, binormal. The resampled section plane is
// perpendicular to the tangent.
struct XQCrossSectionPose {
    double origin[3];
    double tangent[3];
    double normal[3];
    double binormal[3];
};

// Output section geometry (POD). The section spans sizeMm x sizeMm millimetres
// centered on the pose origin, sampled at pixelSizeMm per pixel. With the
// defaults (40 mm, 0.2 mm/px) the output image is 200 x 200 pixels.
struct XQCrossSectionOutputSpec {
    double sizeMm;        // physical extent of the square section (mm)
    double pixelSizeMm;   // sampling resolution (mm per pixel)
};

// Default section extent / resolution (SV-comparable): 40 mm square at
// 0.2 mm/px -> 200 x 200 output image.
inline XQCrossSectionOutputSpec xqDefaultCrossSectionSpec()
{
    return XQCrossSectionOutputSpec{40.0, 0.2};
}

// The world-space frame the last reslice produced, mirroring core's
// ContourFrame: origin == pose.origin, normal == tangent (the section's out-of
// plane axis), xAxis == normal, yAxis == binormal. Reported as POD so headless
// tests can assert the reslice axes match what an XQContour would store, which
// is the P3-1 correctness gate (section 2D pixel -> unprojectFromFrame -> world).
struct XQCrossSectionFrame {
    double origin[3];
    double normal[3];
    double xAxis[3];
    double yAxis[3];
};

// Wraps a vtkImageReslice that cuts a 2D cross-section out of a resident
// vtkImageData along a path sample pose. The pipeline is built once; each
// setPose call only updates the reslice axes + spec and re-executes, so slider
// scrubbing never rebuilds the pipeline. VTK-free header (pimpl); VTK lives in
// the .cpp only.
class XQCrossSectionResampler {
public:
    XQCrossSectionResampler();
    ~XQCrossSectionResampler();

    XQCrossSectionResampler(const XQCrossSectionResampler&) = delete;
    XQCrossSectionResampler& operator=(const XQCrossSectionResampler&) = delete;

    // Sets the resident image to reslice. `image` is an opaque vtkImageData*
    // (obtained from XQRenderScene::vtkImageDataHandle); null clears it. The
    // resampler borrows the image; it must outlive the resampler / any setPose.
    void setImage(void* image);
    bool hasImage() const;

    // Section extent / resolution. Defaults to xqDefaultCrossSectionSpec().
    void setOutputSpec(const XQCrossSectionOutputSpec& spec);
    XQCrossSectionOutputSpec outputSpec() const;

    // Reslices at the given pose: sets the reslice axes (x = normal,
    // y = binormal, z = tangent), origin, and centered output extent, then
    // executes. No-op (output stays null) when there is no image.
    void setPose(const XQCrossSectionPose& pose);

    // Opaque vtkImageData* of the last resliced 2D section (null when there is
    // no image / setPose was never called). The view mounts a vtkImageSlice on
    // this. Stable across setPose calls (the same output object is re-executed).
    void* outputImageHandle() const;

    // The world frame the last setPose produced (see XQCrossSectionFrame). All
    // zero when setPose was never called. Independent of whether an image is
    // present, so tests can assert axis mapping without volume data.
    XQCrossSectionFrame lastFrame() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace xq

#endif // XQ_VISUALIZATION_CROSS_SECTION_RESAMPLER_H
