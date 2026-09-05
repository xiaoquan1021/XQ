#ifndef XQ_CORE_GEOMETRY_ISO_CONTOUR_TRACER_H
#define XQ_CORE_GEOMETRY_ISO_CONTOUR_TRACER_H

#include "core/segmentation/ILevelSetSegmenter.h"  // SectionPoint2D

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <vector>

namespace xq {

// Header-only marching-squares iso-contour tracer, lifted verbatim (helpers +
// traceSeededIsoLoop) from ContourExtractionService.cpp's anonymous namespace so
// both xq_services (threshold / region-grow contours) and the ITK adapter
// (zero-level-set tracing) share the exact same, already field-validated tracer
// without either pulling the other's dependencies. Pure C++: no VTK / ITK / Qt.
//
// Point type is SectionPoint2D (core POD {u, v} mm). Everything is `inline` so
// multiple TUs can include this header without ODR conflicts.
namespace isocontour {

// Converts a pixel index (x, y) to section-local (u, v) mm with the section
// center at (0, 0): u = (x - (W-1)/2) * px, v = (y - (H-1)/2) * px.
inline SectionPoint2D pixelToSection(double x, double y, int width, int height,
                                     double pixelSizeMm)
{
    const double cx = 0.5 * static_cast<double>(width - 1);
    const double cy = 0.5 * static_cast<double>(height - 1);
    return SectionPoint2D{(x - cx) * pixelSizeMm, (y - cy) * pixelSizeMm};
}

// A marching-squares crossing on a grid edge, identified by an integer key so
// segments sharing an edge share the exact same node (no float matching).
struct EdgeKey {
    int x;
    int y;
    bool horizontal;   // true: edge to +x; false: edge to +y

    bool operator<(const EdgeKey& o) const
    {
        if (x != o.x) return x < o.x;
        if (y != o.y) return y < o.y;
        return horizontal < o.horizontal;
    }
    bool operator==(const EdgeKey& o) const
    {
        return x == o.x && y == o.y && horizontal == o.horizontal;
    }
};

// A directed segment of the iso-line, both endpoints given as edge keys.
struct IsoSegment {
    EdgeKey a;
    EdgeKey b;
};

// Sample of `field` at (x, y) (row-major, idx = y*W + x).
inline double sampleAt(const std::vector<double>& field, int width, int x, int y)
{
    return field[static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                 + static_cast<std::size_t>(x)];
}

// Interpolation fraction where `iso` crosses between va (t=0) and vb (t=1).
inline double crossingT(double va, double vb, double iso)
{
    const double denom = vb - va;
    if (denom == 0.0) {
        return 0.5;
    }
    double t = (iso - va) / denom;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return t;
}

// Section-local (mm) point of an edge crossing, given its sub-pixel fraction.
inline SectionPoint2D edgePoint(const EdgeKey& e, double t, int width, int height,
                                double pixelSizeMm)
{
    double px = static_cast<double>(e.x);
    double py = static_cast<double>(e.y);
    if (e.horizontal) {
        px += t;
    } else {
        py += t;
    }
    return pixelToSection(px, py, width, height, pixelSizeMm);
}

// Signed area (shoelace) of a closed loop, in (u,v) units.
inline double signedArea(const std::vector<SectionPoint2D>& loop)
{
    double a = 0.0;
    const std::size_t n = loop.size();
    for (std::size_t i = 0; i < n; ++i) {
        const SectionPoint2D& p = loop[i];
        const SectionPoint2D& q = loop[(i + 1) % n];
        a += p.u * q.v - q.u * p.v;
    }
    return 0.5 * a;
}

// Centroid of a loop's vertices (plain average).
inline SectionPoint2D centroidOf(const std::vector<SectionPoint2D>& loop)
{
    SectionPoint2D c{0.0, 0.0};
    if (loop.empty()) {
        return c;
    }
    for (const SectionPoint2D& p : loop) {
        c.u += p.u;
        c.v += p.v;
    }
    const double inv = 1.0 / static_cast<double>(loop.size());
    c.u *= inv;
    c.v *= inv;
    return c;
}

// Point-in-polygon by the ray-crossing rule (odd crossings -> inside).
inline bool pointInLoop(const std::vector<SectionPoint2D>& loop, const SectionPoint2D& p)
{
    bool inside = false;
    const std::size_t n = loop.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const SectionPoint2D& a = loop[i];
        const SectionPoint2D& b = loop[j];
        const bool straddles = (a.v > p.v) != (b.v > p.v);
        if (straddles) {
            const double xCross =
                (b.u - a.u) * (p.v - a.v) / (b.v - a.v) + a.u;
            if (p.u < xCross) {
                inside = !inside;
            }
        }
    }
    return inside;
}

} // namespace isocontour

// Traces closed iso-loops of `field` at `iso` via marching squares on the
// regular W*H grid (idx = y*W + x), then returns the loop enclosing `seed`
// (smallest enclosing area when several nest) or, if none encloses it, the loop
// whose centroid is nearest the seed; empty when no closed loop is found.
inline std::vector<SectionPoint2D> traceSeededIsoLoop(const std::vector<double>& field,
                                                      int width, int height,
                                                      double pixelSizeMm, double iso,
                                                      const SectionPoint2D& seed)
{
    using namespace isocontour;

    std::vector<SectionPoint2D> result;
    if (width <= 1 || height <= 1
        || field.size() != static_cast<std::size_t>(width)
                               * static_cast<std::size_t>(height)) {
        return result;
    }

    // --- 2D marching squares ------------------------------------------------
    std::map<EdgeKey, double> crossT;   // edge -> interpolation fraction
    std::vector<IsoSegment> segments;

    auto edgeKeyHoriz = [](int x, int y) { return EdgeKey{x, y, true}; };
    auto edgeKeyVert = [](int x, int y) { return EdgeKey{x, y, false}; };

    for (int y = 0; y + 1 < height; ++y) {
        for (int x = 0; x + 1 < width; ++x) {
            const double v00 = sampleAt(field, width, x, y);         // bottom-left
            const double v10 = sampleAt(field, width, x + 1, y);     // bottom-right
            const double v11 = sampleAt(field, width, x + 1, y + 1); // top-right
            const double v01 = sampleAt(field, width, x, y + 1);     // top-left

            int code = 0;
            if (v00 >= iso) code |= 1;
            if (v10 >= iso) code |= 2;
            if (v11 >= iso) code |= 4;
            if (v01 >= iso) code |= 8;
            if (code == 0 || code == 15) {
                continue;   // wholly inside/outside -> no crossing
            }

            const EdgeKey eBottom = edgeKeyHoriz(x, y);         // v00 -- v10
            const EdgeKey eRight = edgeKeyVert(x + 1, y);       // v10 -- v11
            const EdgeKey eTop = edgeKeyHoriz(x, y + 1);        // v01 -- v11
            const EdgeKey eLeft = edgeKeyVert(x, y);            // v00 -- v01
            crossT[eBottom] = crossingT(v00, v10, iso);
            crossT[eRight] = crossingT(v10, v11, iso);
            crossT[eTop] = crossingT(v01, v11, iso);
            crossT[eLeft] = crossingT(v00, v01, iso);

            auto addSeg = [&segments](const EdgeKey& a, const EdgeKey& b) {
                segments.push_back(IsoSegment{a, b});
            };
            switch (code) {
                case 1:  addSeg(eLeft, eBottom); break;
                case 2:  addSeg(eBottom, eRight); break;
                case 3:  addSeg(eLeft, eRight); break;
                case 4:  addSeg(eRight, eTop); break;
                case 5: {
                    const double mean = 0.25 * (v00 + v10 + v11 + v01);
                    if (mean >= iso) {
                        addSeg(eLeft, eTop);
                        addSeg(eBottom, eRight);
                    } else {
                        addSeg(eLeft, eBottom);
                        addSeg(eRight, eTop);
                    }
                    break;
                }
                case 6:  addSeg(eBottom, eTop); break;
                case 7:  addSeg(eLeft, eTop); break;
                case 8:  addSeg(eTop, eLeft); break;
                case 9:  addSeg(eTop, eBottom); break;
                case 10: {
                    const double mean = 0.25 * (v00 + v10 + v11 + v01);
                    if (mean >= iso) {
                        addSeg(eBottom, eLeft);
                        addSeg(eTop, eRight);
                    } else {
                        addSeg(eBottom, eRight);
                        addSeg(eTop, eLeft);
                    }
                    break;
                }
                case 11: addSeg(eTop, eRight); break;
                case 12: addSeg(eRight, eLeft); break;
                case 13: addSeg(eRight, eBottom); break;
                case 14: addSeg(eBottom, eLeft); break;
                default: break;
            }
        }
    }

    if (segments.empty()) {
        return result;
    }

    // --- Stitch segments into closed loops ----------------------------------
    std::map<EdgeKey, std::vector<std::size_t>> incident;   // node -> segment indices
    for (std::size_t i = 0; i < segments.size(); ++i) {
        incident[segments[i].a].push_back(i);
        incident[segments[i].b].push_back(i);
    }

    std::vector<bool> used(segments.size(), false);
    std::vector<std::vector<SectionPoint2D>> loops;

    auto emitPoint = [&](const EdgeKey& node) {
        const double t = crossT[node];
        return edgePoint(node, t, width, height, pixelSizeMm);
    };

    for (std::size_t s = 0; s < segments.size(); ++s) {
        if (used[s]) {
            continue;
        }
        const EdgeKey startNode = segments[s].a;
        std::vector<EdgeKey> chain;
        chain.push_back(startNode);
        EdgeKey node = segments[s].b;   // the far end of the first segment
        used[s] = true;

        bool closed = false;
        while (true) {
            if (node == startNode) {
                closed = true;
                break;
            }
            chain.push_back(node);
            std::size_t advance = segments.size();
            for (std::size_t idx : incident[node]) {
                if (!used[idx]) {
                    advance = idx;
                    break;
                }
            }
            if (advance == segments.size()) {
                break;   // dead end: open chain
            }
            used[advance] = true;
            const IsoSegment& seg = segments[advance];
            node = (seg.a == node) ? seg.b : seg.a;   // step to the far endpoint
        }

        if (!closed || chain.size() < 3) {
            continue;   // only keep genuine closed loops
        }
        std::vector<SectionPoint2D> loop;
        loop.reserve(chain.size());
        for (const EdgeKey& n : chain) {
            loop.push_back(emitPoint(n));
        }
        loops.push_back(std::move(loop));
    }

    if (loops.empty()) {
        return result;
    }

    // --- Seed-nearest loop selection ----------------------------------------
    int chosen = -1;
    for (std::size_t i = 0; i < loops.size(); ++i) {
        if (pointInLoop(loops[i], seed)) {
            if (chosen < 0
                || std::abs(signedArea(loops[i]))
                       < std::abs(signedArea(loops[static_cast<std::size_t>(chosen)]))) {
                chosen = static_cast<int>(i);
            }
        }
    }
    if (chosen < 0) {
        double best = 0.0;
        for (std::size_t i = 0; i < loops.size(); ++i) {
            const SectionPoint2D c = centroidOf(loops[i]);
            const double d = std::hypot(c.u - seed.u, c.v - seed.v);
            if (chosen < 0 || d < best) {
                best = d;
                chosen = static_cast<int>(i);
            }
        }
    }

    if (chosen >= 0) {
        result = std::move(loops[static_cast<std::size_t>(chosen)]);
    }
    return result;
}

} // namespace xq

#endif // XQ_CORE_GEOMETRY_ISO_CONTOUR_TRACER_H
