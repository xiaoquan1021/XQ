#ifndef XQ_CORE_SEGMENTATION_I_LEVEL_SET_SEGMENTER_H
#define XQ_CORE_SEGMENTATION_I_LEVEL_SET_SEGMENTER_H

#include <vector>

namespace xq {

// A 2D point in the section's local (u, v) millimetre coordinates (the frame
// spanned by ContourFrame::xAxis / yAxis), section CENTER at (0, 0). Same POD
// shape as services' ContourPoint2D, redeclared here so xq_core never depends on
// xq_services (the app does a trivial field-for-field copy at the boundary).
struct SectionPoint2D {
    double u;
    double v;
};

// Parameters for the SimVascular two-phase vascular level set. Defaults mirror
// SV's cvITKLevelSet construction (sv3_ITKLevelSet.cxx) + sv3_Contour.h golden
// values. NOTE (see memory sv-vascular-levelset-propagation-zero-required):
// the SV filter REQUIRES propagation scaling == 0 (it only allocates the
// gradient/speed image on that path); the adapter keeps prop = 0 and does NOT
// expose it as a knob. Advection likewise stays 0 (SV cvITKLevelSet default),
// curvature 1.0 -- fixed inside the adapter, not here.
struct LevelSetParams {
    // Seed circle radius in millimetres; the adapter converts to pixels via
    // rPx = seedRadiusMm / pixelSizeMm and builds the signed-distance init image.
    double seedRadiusMm = 2.0;

    // Phase one (coarse localisation).
    double sigmaFeature1 = 2.5;       // GradientMagnitudeRecursiveGaussian sigma (feature image)
    double kc = 2.0;                  // EquilibriumCurvature: the front's expansion drive.
                                      // SV's data-scale default is 0.6, but on XQ-scale
                                      // sections a small kc leaves the front stalled near the
                                      // seed when internal intensity gradients depress the
                                      // speed image (batch-2 spike: kc=0.6 -> 1% fill on a
                                      // c1~=c2 ramp; kc>=1.5 -> full ~8% fill, contrast-robust).
                                      // Re-tuned on real sections in batch 4.
    double expFactorRising = 0.25;    // RisingVelocityDecayModifier
    double expFactorFalling = 0.5;    // FallingVelocityDecayModifier
    int maxIterations1 = 600;         // phase-one iteration cap (SV 2000; smaller for interactive preview)
    double maxRmsError1 = 0.001;

    // Phase two (curvature-threshold refinement, seeded by phase-one front).
    double sigmaFeature2 = 1.5;
    double kUpper = 0.8;              // CurvataureUpperThreshold (SV spelling)
    double kLower = 0.09;             // CurvataureLowerThreshold
    int maxIterations2 = 400;
    double maxRmsError2 = 0.0005;
};

// Extracts one closed 2D contour of a vessel lumen from a cross-section grayscale
// section, using SimVascular's ITK-based two-phase vascular level set (GAC:
// gradient-magnitude speed image + curvature). Replaces the failed self-written
// Chan-Vese: robust to low-contrast MR sections where region means c1 ~= c2 make
// a region model collapse to a smooth circle.
//
// Pure XQ POD types in and out; the ITK types live entirely inside the adapter
// (ItkVascularSegmenter), which is injected into the app. `gray` is a row-major
// W*H grayscale buffer (idx = y*W + x), raw voxel intensity. `pixelSizeMm` maps
// pixels to section-local millimetres; returned points are section-local (u, v)
// mm with the section CENTER at (0, 0):
//   u = (x - (W-1)/2) * pixelSizeMm,  v = (y - (H-1)/2) * pixelSizeMm.
// `seed` is the section-local (u, v) mm seed (section center (0,0) = path lumen
// center is a good default). Returns an ordered, closed loop (last point !=
// first); empty when the section is degenerate or no closed zero-level-set loop
// encloses/near the seed (caller rejects -> no contour added). No throw.
class ILevelSetSegmenter {
public:
    virtual ~ILevelSetSegmenter() = default;

    virtual std::vector<SectionPoint2D> segment(const std::vector<double>& gray,
                                                int width, int height,
                                                double pixelSizeMm,
                                                const SectionPoint2D& seed,
                                                const LevelSetParams& params) = 0;
};

} // namespace xq

#endif // XQ_CORE_SEGMENTATION_I_LEVEL_SET_SEGMENTER_H
