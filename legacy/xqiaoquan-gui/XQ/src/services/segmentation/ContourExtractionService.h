#ifndef XQ_SERVICES_SEGMENTATION_CONTOUR_EXTRACTION_SERVICE_H
#define XQ_SERVICES_SEGMENTATION_CONTOUR_EXTRACTION_SERVICE_H

#include <vector>

namespace xq {

// A 2D point in the section's local (u, v) millimetre coordinates (the frame
// spanned by ContourFrame::xAxis / yAxis). Kept free of core geometry / VTK /
// Qt so this service stays pure domain code.
struct ContourPoint2D {
    double u;
    double v;
};

// Turns manual control points drawn on a cross-section into ordered 2D contour
// point loops, mirroring SimVascular's sv4gui_ContourCircle / _ContourPolygon
// sampling. Pure domain code: no VTK / Qt / core dependency, no throw; invalid
// input yields an empty result the caller can reject.
class ContourExtractionService {
public:
    // Circle from two control points: center and a boundary point. The radius is
    // dist(center, boundary). Samples `sampleCount` points evenly around the
    // circle: p_i = center + r*(cos a_i, sin a_i), a_i = i * 2pi / sampleCount,
    // i = 0..sampleCount-1 (a closed loop; the last point is not the first).
    // sampleCount is clamped up to a minimum of 8. Returns empty when the radius
    // is not positive.
    static std::vector<ContourPoint2D> circle(const ContourPoint2D& center,
                                              const ContourPoint2D& boundary,
                                              int sampleCount = 36);

    // Polygon from >= 3 vertices. Each edge v_i -> v_{i+1} -- wrapping the last
    // vertex back to the first, so the loop is closed -- is linearly subdivided
    // into `perEdge` segments: the edge start plus perEdge-1 interior points are
    // emitted, and the duplicated closing vertex is not. perEdge is clamped up to
    // a minimum of 1 (vertices only). Returns empty when fewer than 3 vertices.
    static std::vector<ContourPoint2D> polygon(const std::vector<ContourPoint2D>& vertices,
                                               int perEdge = 8);

    // Estimates a default threshold from the section's grayscale histogram,
    // biased to the high-signal band where contrast-filled vessel lumen sits
    // (percentile `pct` of the value range, default 0.90 -> P90). `gray` is a
    // row-major W*H sample buffer (idx = y*W + x). Returns the midpoint of the
    // min/max range when the buffer is empty or flat. NOT a hard 0/255 default.
    static double estimateThreshold(const std::vector<double>& gray,
                                    int width, int height, double pct = 0.90);

    // Extracts one closed 2D contour of the vessel lumen from a section by
    // tracing the `threshold` iso-line and keeping the connected iso-loop
    // nearest the seed (SV: vtkContourFilter + ClosestPointRegion). `gray` is a
    // row-major W*H grayscale buffer (idx = y*W + x). `pixelSizeMm` maps pixels
    // to section-local millimetres; the returned points are in section-local
    // (u, v) mm with the section CENTER at (0, 0):
    //   u = (x - (W-1)/2) * pixelSizeMm,  v = (y - (H-1)/2) * pixelSizeMm.
    // `seed` is the section-local (u,v) mm seed (the section center (0,0) is the
    // path lumen center -- a good default seed). Marching-squares tracing on the
    // regular grid; among the closed iso-loops, the one whose interior contains
    // the seed is chosen (fall back to the loop nearest the seed). Returns an
    // ordered, closed loop (last point != first). Returns empty when no closed
    // loop encloses/near the seed (caller rejects -> no contour added).
    static std::vector<ContourPoint2D> thresholdContour(const std::vector<double>& gray,
                                                        int width, int height,
                                                        double pixelSizeMm,
                                                        double threshold,
                                                        const ContourPoint2D& seed);

    // Extracts one closed 2D contour of the vessel lumen from a section by region
    // growing from the seed and tracing the grown region's boundary. The seed's
    // pixel value defines an inclusion band [seedValue - band, seedValue + band];
    // an 8-connected flood fill from the seed pixel keeps pixels whose value falls
    // in that band, yielding one connected mask (SV 3D ConnectedThreshold uses the
    // same lower/upper-bound + seed-connectivity semantics; anchoring the band on
    // the local seed value is what makes this robust to non-uniform brightness
    // where a single global threshold fails). The connected region's boundary is
    // then traced as a closed loop (reusing the marching-squares iso-tracer at
    // iso = 0.5 on the 0/1 mask) and the loop enclosing the seed is returned.
    // `gray` is a row-major W*H buffer (idx = y*W + x). `pixelSizeMm` maps pixels
    // to section-local mm; returned points are section-local (u, v) mm with the
    // section CENTER at (0, 0), same convention as thresholdContour. `seed` is the
    // section-local (u, v) mm seed (section center (0, 0) is a good default).
    // `band` is the inclusion half-width in intensity units (>= 0). Returns an
    // ordered, closed loop (last point != first); empty when the seed pixel is out
    // of range, the region is empty, or no closed boundary loop is found (caller
    // rejects -> no contour added).
    static std::vector<ContourPoint2D> regionGrowContour(const std::vector<double>& gray,
                                                         int width, int height,
                                                         double pixelSizeMm,
                                                         const ContourPoint2D& seed,
                                                         double band);
};

} // namespace xq

#endif // XQ_SERVICES_SEGMENTATION_CONTOUR_EXTRACTION_SERVICE_H
