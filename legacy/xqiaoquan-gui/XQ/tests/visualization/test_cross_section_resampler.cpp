// P3-1 cross-section resampler discrete-invariant tests. Pure VTK, no GL /
// offscreen context (only vtkImageData / vtkImageReslice are exercised, never a
// render window). Bare main + fail() style, mirroring test_render_scene.
//
// IRON RULE (memory: no-sideeffect-in-assert): side-effecting calls (setImage /
// setPose / ...) are made first and their results stored, THEN asserted --
// never inside an assert/condition expression, so a /DNDEBUG build cannot delete
// them into a false green.
//
// The correctness gate is the reslice frame <-> ContourFrame correspondence:
// origin == pose.origin, normal == tangent, xAxis == normal, yAxis == binormal,
// AND the section pixels actually sample the world positions that
// unprojectFromFrame(frame, u, v) names. Swapping the reslice axis order
// (normal <-> binormal) must turn the off-center-pixel assertions red.

#include <visualization/XQCrossSectionResampler.h>
#include <visualization/XQRenderScene.h>

#include <core/GeometryTypes.h>
#include <core/XQContourGroup.h>
#include <core/XQImageVolume.h>

#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

void check(bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

bool nearlyEqual(double a, double b, double tol)
{
    return std::fabs(a - b) <= tol;
}

bool vecNear(const double a[3], const double b[3], double tol)
{
    return nearlyEqual(a[0], b[0], tol) && nearlyEqual(a[1], b[1], tol)
        && nearlyEqual(a[2], b[2], tol);
}

// A single-component float volume whose scalar at world point (x,y,z) is a known
// affine function of world position: f = ax*x + ay*y + az*z + a0. Cubic reslice
// interpolation is exact for an affine field, so a resliced pixel's value equals
// f evaluated at that pixel's world position -- which lets the test assert the
// section pixel -> world mapping (and thus the reslice axis order) to tight
// tolerance. The volume is axis-aligned (identity direction), 1 mm spacing,
// origin at 0, large enough to contain the section.
vtkSmartPointer<vtkImageData> makeAffineVolume(double ax, double ay, double az,
                                               double a0)
{
    const int n = 81; // covers -40..+40 mm about the origin at 1 mm spacing
    vtkSmartPointer<vtkImageData> image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(n, n, n);
    image->SetSpacing(1.0, 1.0, 1.0);
    image->SetOrigin(-40.0, -40.0, -40.0);
    image->AllocateScalars(VTK_FLOAT, 1);
    float* ptr = static_cast<float*>(image->GetScalarPointer());
    for (int k = 0; k < n; ++k) {
        const double z = -40.0 + k;
        for (int j = 0; j < n; ++j) {
            const double y = -40.0 + j;
            for (int i = 0; i < n; ++i) {
                const double x = -40.0 + i;
                *ptr++ = static_cast<float>(ax * x + ay * y + az * z + a0);
            }
        }
    }
    return image;
}

// Reads the scalar of a 2D section image at pixel (px, py).
double sectionValue(vtkImageData* section, int px, int py)
{
    if (section == nullptr) {
        return 0.0;
    }
    const int* ext = section->GetExtent();
    if (px < ext[0] || px > ext[1] || py < ext[2] || py > ext[3]) {
        return 0.0;
    }
    return section->GetScalarComponentAsDouble(px, py, ext[4], 0);
}

// 1. Frame correspondence: the resampler's reported frame mirrors ContourFrame
//    (origin == pose.origin, normal == tangent, xAxis == normal,
//    yAxis == binormal). Independent of image presence.
void testFrameCorrespondence()
{
    xq::XQCrossSectionResampler resampler;

    xq::XQCrossSectionPose pose{};
    pose.origin[0] = 3.0;
    pose.origin[1] = -2.0;
    pose.origin[2] = 5.0;
    // A right-handed frame (not axis-aligned) so an axis mix-up is visible.
    pose.tangent[0] = 0.0;
    pose.tangent[1] = 0.0;
    pose.tangent[2] = 1.0;
    pose.normal[0] = 1.0;
    pose.normal[1] = 0.0;
    pose.normal[2] = 0.0;
    pose.binormal[0] = 0.0;
    pose.binormal[1] = 1.0;
    pose.binormal[2] = 0.0;

    resampler.setPose(pose);
    const xq::XQCrossSectionFrame frame = resampler.lastFrame();

    check(vecNear(frame.origin, pose.origin, 1e-6), "1: frame origin == pose origin");
    check(vecNear(frame.normal, pose.tangent, 1e-6), "1: frame normal == tangent");
    check(vecNear(frame.xAxis, pose.normal, 1e-6), "1: frame xAxis == normal");
    check(vecNear(frame.yAxis, pose.binormal, 1e-6), "1: frame yAxis == binormal");
}

// 2. Section pixel -> world sampling: with an affine intensity field, a resliced
//    pixel's value equals f evaluated at the world position that
//    unprojectFromFrame(frame, u, v) names. Covers the center pixel (u=v=0 ->
//    pose.origin) AND off-center pixels along +u (normal) and +v (binormal), so
//    swapping the reslice axes turns it red.
void testPixelWorldSampling()
{
    // f = 2x + 3y + 5z + 7. Distinct coefficients so any axis swap changes the
    // predicted value.
    const double ax = 2.0;
    const double ay = 3.0;
    const double az = 5.0;
    const double a0 = 7.0;
    vtkSmartPointer<vtkImageData> image = makeAffineVolume(ax, ay, az, a0);

    xq::XQCrossSectionResampler resampler;
    resampler.setImage(image.GetPointer());

    // Odd N so the section has an exact center pixel: 20.5 mm at 0.5 mm/px ->
    // round(41) = 41 px, centered on the pose origin.
    xq::XQCrossSectionOutputSpec spec{};
    spec.sizeMm = 20.5;
    spec.pixelSizeMm = 0.5;
    resampler.setOutputSpec(spec);

    // A tilted (still orthonormal) frame so u and v mix world axes -- an axis
    // swap cannot accidentally still match.
    const double inv = 1.0 / std::sqrt(2.0);
    xq::XQCrossSectionPose pose{};
    pose.origin[0] = 1.0;
    pose.origin[1] = 2.0;
    pose.origin[2] = -1.0;
    pose.tangent[0] = 0.0;
    pose.tangent[1] = 0.0;
    pose.tangent[2] = 1.0;
    pose.normal[0] = inv;   // u axis mixes x and y
    pose.normal[1] = inv;
    pose.normal[2] = 0.0;
    pose.binormal[0] = -inv; // v axis mixes x and y, orthogonal to normal
    pose.binormal[1] = inv;
    pose.binormal[2] = 0.0;

    resampler.setPose(pose);
    vtkImageData* section =
        static_cast<vtkImageData*>(resampler.outputImageHandle());
    check(section != nullptr, "2: section output non-null");
    if (section == nullptr) {
        return;
    }

    const int* ext = section->GetExtent();
    const int nx = ext[1] - ext[0] + 1;
    const int ny = ext[3] - ext[2] + 1;
    check(nx == 41 && ny == 41, "2: section is 41x41 for 20mm @ 0.5mm/px");
    const int cx = ext[0] + nx / 2; // center pixel index
    const int cy = ext[2] + ny / 2;

    const xq::XQCrossSectionFrame frame = resampler.lastFrame();
    xq::ContourFrame cf{};
    cf.origin = {frame.origin[0], frame.origin[1], frame.origin[2]};
    cf.normal = {frame.normal[0], frame.normal[1], frame.normal[2]};
    cf.xAxis = {frame.xAxis[0], frame.xAxis[1], frame.xAxis[2]};
    cf.yAxis = {frame.yAxis[0], frame.yAxis[1], frame.yAxis[2]};

    auto predict = [&](double u, double v) {
        const xq::Point3 w = xq::XQContourGroup::unprojectFromFrame(cf, u, v);
        return ax * w.x + ay * w.y + az * w.z + a0;
    };

    // Center pixel: u = v = 0 -> world == pose.origin.
    {
        const double got = sectionValue(section, cx, cy);
        const double want = predict(0.0, 0.0);
        check(nearlyEqual(got, want, 1e-3), "2: center pixel samples pose.origin");
        // Explicitly: unproject(0,0) must be the origin (guards the frame too).
        const xq::Point3 c = xq::XQContourGroup::unprojectFromFrame(cf, 0.0, 0.0);
        const double originVec[3] = {c.x, c.y, c.z};
        check(vecNear(originVec, pose.origin, 1e-6),
              "2: center-pixel world == pose.origin");
    }

    // Off-center along +u (normal direction): 8 px * 0.5 mm = +4 mm.
    {
        const double u = 8 * spec.pixelSizeMm;
        const double got = sectionValue(section, cx + 8, cy);
        const double want = predict(u, 0.0);
        check(nearlyEqual(got, want, 1e-3),
              "2: +u pixel samples origin + u*normal (axis order)");
    }

    // Off-center along +v (binormal direction): 8 px * 0.5 mm = +4 mm.
    {
        const double v = 8 * spec.pixelSizeMm;
        const double got = sectionValue(section, cx, cy + 8);
        const double want = predict(0.0, v);
        check(nearlyEqual(got, want, 1e-3),
              "2: +v pixel samples origin + v*binormal (axis order)");
    }
}

// 3. No image -> null output, no crash. Also exercised through XQRenderScene's
//    handle (null when no volume was set).
void testNoImage()
{
    xq::XQCrossSectionResampler resampler;
    const bool hasBefore = resampler.hasImage();
    check(!hasBefore, "3: no image initially");

    xq::XQCrossSectionPose pose{};
    pose.tangent[2] = 1.0;
    pose.normal[0] = 1.0;
    pose.binormal[1] = 1.0;
    resampler.setPose(pose);
    void* output = resampler.outputImageHandle();
    check(output == nullptr, "3: null image -> null output");

    // The frame is still reported (frame math is image-independent).
    const xq::XQCrossSectionFrame frame = resampler.lastFrame();
    check(nearlyEqual(frame.normal[2], 1.0, 1e-6), "3: frame reported without image");

    // XQRenderScene with no volume hands out a null image handle; feeding it to
    // the resampler keeps the output null.
    xq::XQRenderScene scene;
    void* sceneImage = scene.vtkImageDataHandle();
    check(sceneImage == nullptr, "3: no-volume scene image handle is null");
    resampler.setImage(sceneImage);
    resampler.setPose(pose);
    void* output2 = resampler.outputImageHandle();
    check(output2 == nullptr, "3: null scene handle -> null output");
}

} // namespace

int main()
{
    testFrameCorrespondence();
    testPixelWorldSampling();
    testNoImage();

    if (g_failures != 0) {
        std::fprintf(stderr, "test_cross_section_resampler: %d failure(s)\n",
                     g_failures);
        return 1;
    }
    std::fprintf(stderr, "test_cross_section_resampler: all checks passed\n");
    return 0;
}
