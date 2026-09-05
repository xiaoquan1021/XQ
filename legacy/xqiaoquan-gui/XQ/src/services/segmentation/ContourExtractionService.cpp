#include "services/segmentation/ContourExtractionService.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <vector>

namespace xq {

namespace {

// Converts a pixel index (x, y) to section-local (u, v) mm with the section
// center at (0, 0): u = (x - (W-1)/2) * px, v = (y - (H-1)/2) * px.
ContourPoint2D pixelToSection(double x, double y, int width, int height,
                              double pixelSizeMm)
{
    const double cx = 0.5 * static_cast<double>(width - 1);
    const double cy = 0.5 * static_cast<double>(height - 1);
    return ContourPoint2D{(x - cx) * pixelSizeMm, (y - cy) * pixelSizeMm};
}

// A marching-squares crossing on a grid edge, identified by an integer key so
// segments sharing an edge share the exact same node (no float matching). A
// crossing lies on either a horizontal edge (between (x,y) and (x+1,y)) or a
// vertical edge (between (x,y) and (x,y+1)); the key packs the edge's base
// pixel + orientation. `t` is the sub-pixel interpolation fraction along it.
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

// A directed segment of the iso-line, both endpoints given as edge keys so the
// stitching stage can chain them by shared node identity.
struct IsoSegment {
    EdgeKey a;
    EdgeKey b;
};

// Sample of `gray` at (x, y) (row-major, idx = y*W + x).
double sampleAt(const std::vector<double>& gray, int width, int x, int y)
{
    return gray[static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
                + static_cast<std::size_t>(x)];
}

// Interpolation fraction where `threshold` crosses between values va (at t=0)
// and vb (at t=1). Clamped to [0,1]; falls back to the midpoint when the two
// samples are equal (degenerate edge, should not occur once classified).
double crossingT(double va, double vb, double threshold)
{
    const double denom = vb - va;
    if (denom == 0.0) {
        return 0.5;
    }
    double t = (threshold - va) / denom;
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return t;
}

// World-ish (mm) point of an edge crossing, given its sub-pixel fraction.
ContourPoint2D edgePoint(const EdgeKey& e, double t, int width, int height,
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

// Signed area (shoelace) of a closed loop, in (u,v) units. Sign indicates
// winding; the absolute value is the enclosed area.
double signedArea(const std::vector<ContourPoint2D>& loop)
{
    double a = 0.0;
    const std::size_t n = loop.size();
    for (std::size_t i = 0; i < n; ++i) {
        const ContourPoint2D& p = loop[i];
        const ContourPoint2D& q = loop[(i + 1) % n];
        a += p.u * q.v - q.u * p.v;
    }
    return 0.5 * a;
}

// Centroid of a loop's vertices (plain average -- enough to rank loops by
// distance to the seed when none encloses it).
ContourPoint2D centroidOf(const std::vector<ContourPoint2D>& loop)
{
    ContourPoint2D c{0.0, 0.0};
    if (loop.empty()) {
        return c;
    }
    for (const ContourPoint2D& p : loop) {
        c.u += p.u;
        c.v += p.v;
    }
    const double inv = 1.0 / static_cast<double>(loop.size());
    c.u *= inv;
    c.v *= inv;
    return c;
}

// Point-in-polygon by winding via the ray-crossing rule (odd crossings ->
// inside). Robust to non-convex loops; the seed sits inside the vessel lumen
// loop, outside every other iso-loop.
bool pointInLoop(const std::vector<ContourPoint2D>& loop, const ContourPoint2D& p)
{
    bool inside = false;
    const std::size_t n = loop.size();
    for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
        const ContourPoint2D& a = loop[i];
        const ContourPoint2D& b = loop[j];
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

// Traces closed iso-loops of `field` at `iso` via marching squares on the
// regular W*H grid (idx = y*W + x), then returns the loop enclosing `seed`
// (smallest enclosing area when several nest) or, if none encloses it, the loop
// whose centroid is nearest the seed; empty when no closed loop is found. Shared
// by thresholdContour (field = grayscale, iso = threshold) and regionGrowContour
// (field = 0/1 mask, iso = 0.5) so both trace and select identically.
std::vector<ContourPoint2D> traceSeededIsoLoop(const std::vector<double>& field,
                                               int width, int height,
                                               double pixelSizeMm, double iso,
                                               const ContourPoint2D& seed)
{
    std::vector<ContourPoint2D> result;
    if (width <= 1 || height <= 1
        || field.size() != static_cast<std::size_t>(width)
                               * static_cast<std::size_t>(height)) {
        return result;
    }

    // --- 2D marching squares ------------------------------------------------
    // Each cell has 4 corners; classify each corner as above/below the iso value
    // to form a 4-bit case, then emit iso-line segment(s) crossing the cell's
    // edges (linear interpolation for sub-pixel crossings). Crossings are named
    // by EdgeKey so shared edges yield the exact same node.
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

            // Cell edges (as EdgeKeys) and their crossing fractions.
            const EdgeKey eBottom = edgeKeyHoriz(x, y);         // v00 -- v10
            const EdgeKey eRight = edgeKeyVert(x + 1, y);       // v10 -- v11
            const EdgeKey eTop = edgeKeyHoriz(x, y + 1);        // v01 -- v11
            const EdgeKey eLeft = edgeKeyVert(x, y);            // v00 -- v01
            crossT[eBottom] = crossingT(v00, v10, iso);
            crossT[eRight] = crossingT(v10, v11, iso);
            crossT[eTop] = crossingT(v01, v11, iso);
            crossT[eLeft] = crossingT(v00, v01, iso);

            // Marching-squares case table. Ambiguous saddles (5, 10) are split
            // with the standard asymptotic resolution using the cell mean; here
            // we resolve toward keeping the two segments that connect the two
            // "above" diagonal corners, matching the common convention.
            auto addSeg = [&segments](const EdgeKey& a, const EdgeKey& b) {
                segments.push_back(IsoSegment{a, b});
            };
            switch (code) {
                case 1:  addSeg(eLeft, eBottom); break;
                case 2:  addSeg(eBottom, eRight); break;
                case 3:  addSeg(eLeft, eRight); break;
                case 4:  addSeg(eRight, eTop); break;
                case 5: {
                    // Saddle: corners 0 and 2 above. Resolve by cell mean.
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
                    // Saddle: corners 1 and 3 above. Resolve by cell mean.
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
    // Build an adjacency map keyed by EdgeKey node identity. Each crossing node
    // borders at most two segments on a clean iso-line, so we can walk chains.
    std::map<EdgeKey, std::vector<std::size_t>> incident;   // node -> segment indices
    for (std::size_t i = 0; i < segments.size(); ++i) {
        incident[segments[i].a].push_back(i);
        incident[segments[i].b].push_back(i);
    }

    std::vector<bool> used(segments.size(), false);
    std::vector<std::vector<ContourPoint2D>> loops;

    auto emitPoint = [&](const EdgeKey& node) {
        const double t = crossT[node];
        return edgePoint(node, t, width, height, pixelSizeMm);
    };

    for (std::size_t s = 0; s < segments.size(); ++s) {
        if (used[s]) {
            continue;
        }
        // Walk a chain starting from segment s, following node adjacency until
        // we return to the start node (closed) or hit a dead end (open ->
        // discarded). `chain` accumulates the ordered crossing nodes; the start
        // node is not repeated at the end.
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
            // Find the next unused segment incident to `node`.
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
        std::vector<ContourPoint2D> loop;
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
    // The vessel lumen loop is the one whose interior contains the seed. When
    // none encloses the seed (partial section / seed off-lumen), fall back to
    // the loop whose centroid is nearest the seed. This is what keeps the
    // result from returning a solid block: only the seed's loop survives.
    int chosen = -1;
    for (std::size_t i = 0; i < loops.size(); ++i) {
        if (pointInLoop(loops[i], seed)) {
            // Prefer the enclosing loop with the smallest area (the innermost
            // one around the seed, not an outer envelope).
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
            const ContourPoint2D c = centroidOf(loops[i]);
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

} // namespace

std::vector<ContourPoint2D> ContourExtractionService::circle(const ContourPoint2D& center,
                                                             const ContourPoint2D& boundary,
                                                             int sampleCount)
{
    std::vector<ContourPoint2D> points;

    const double du = boundary.u - center.u;
    const double dv = boundary.v - center.v;
    const double radius = std::hypot(du, dv);
    if (radius <= 0.0) {
        return points;
    }

    // A degenerate sample count would leave the loop unusable for lofting, so
    // clamp up to a small minimum.
    if (sampleCount < 8) {
        sampleCount = 8;
    }

    points.reserve(static_cast<std::size_t>(sampleCount));
    const double twoPi = 2.0 * 3.14159265358979323846;
    for (int i = 0; i < sampleCount; ++i) {
        const double angle = static_cast<double>(i) * twoPi / static_cast<double>(sampleCount);
        points.push_back(ContourPoint2D{
            center.u + radius * std::cos(angle),
            center.v + radius * std::sin(angle),
        });
    }
    return points;
}

std::vector<ContourPoint2D> ContourExtractionService::polygon(
    const std::vector<ContourPoint2D>& vertices, int perEdge)
{
    std::vector<ContourPoint2D> points;
    if (vertices.size() < 3) {
        return points;
    }

    if (perEdge < 1) {
        perEdge = 1;
    }

    const std::size_t vertexCount = vertices.size();
    points.reserve(vertexCount * static_cast<std::size_t>(perEdge));
    for (std::size_t i = 0; i < vertexCount; ++i) {
        // Wrap the last edge back to the first vertex to close the loop.
        const ContourPoint2D& start = vertices[i];
        const ContourPoint2D& end = vertices[(i + 1) % vertexCount];

        // Emit the edge start, then perEdge-1 interior points. The edge end is
        // the next edge's start, so it (and the duplicated closing vertex) is
        // never pushed here.
        points.push_back(start);
        for (int k = 1; k < perEdge; ++k) {
            const double t = static_cast<double>(k) / static_cast<double>(perEdge);
            points.push_back(ContourPoint2D{
                start.u + t * (end.u - start.u),
                start.v + t * (end.v - start.v),
            });
        }
    }
    return points;
}

double ContourExtractionService::estimateThreshold(const std::vector<double>& gray,
                                                   int width, int height, double pct)
{
    // Scan for min/max; a percentile of the range biases the default to the
    // high-signal band (P90 by default) so the contrast-filled lumen -- not the
    // dark background -- is picked. No hard 0/255 default.
    (void)width;
    (void)height;
    if (gray.empty()) {
        return 0.0;
    }
    double lo = gray[0];
    double hi = gray[0];
    for (double v : gray) {
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    if (hi <= lo) {
        return 0.5 * (lo + hi);
    }
    if (pct < 0.0) pct = 0.0;
    if (pct > 1.0) pct = 1.0;
    return lo + pct * (hi - lo);
}

std::vector<ContourPoint2D> ContourExtractionService::thresholdContour(
    const std::vector<double>& gray, int width, int height, double pixelSizeMm,
    double threshold, const ContourPoint2D& seed)
{
    // Trace the grayscale's `threshold` iso-line and keep the seed's loop. The
    // marching-squares tracing + seed-loop selection is shared with the region
    // grower (traceSeededIsoLoop): here the field is the grayscale and the iso
    // value is the threshold.
    return traceSeededIsoLoop(gray, width, height, pixelSizeMm, threshold, seed);
}

std::vector<ContourPoint2D> ContourExtractionService::regionGrowContour(
    const std::vector<double>& gray, int width, int height, double pixelSizeMm,
    const ContourPoint2D& seed, double band)
{
    std::vector<ContourPoint2D> result;
    if (width <= 1 || height <= 1
        || gray.size() != static_cast<std::size_t>(width)
                              * static_cast<std::size_t>(height)) {
        return result;
    }
    if (band < 0.0) {
        band = 0.0;   // a negative half-width is meaningless; clamp to seed-only
    }

    // --- Locate the seed pixel ----------------------------------------------
    // Inverse of pixelToSection: sx = round(u/px + (W-1)/2), sy likewise. Clamp
    // into range so an off-image seed still starts at the nearest pixel.
    const double cx = 0.5 * static_cast<double>(width - 1);
    const double cy = 0.5 * static_cast<double>(height - 1);
    int sx = static_cast<int>(std::lround(seed.u / pixelSizeMm + cx));
    int sy = static_cast<int>(std::lround(seed.v / pixelSizeMm + cy));
    if (sx < 0) sx = 0;
    if (sx > width - 1) sx = width - 1;
    if (sy < 0) sy = 0;
    if (sy > height - 1) sy = height - 1;
    const double seedValue =
        gray[static_cast<std::size_t>(sy) * static_cast<std::size_t>(width)
             + static_cast<std::size_t>(sx)];

    // --- 8-connected flood fill from the seed -------------------------------
    // Keep a pixel when |value - seedValue| <= band, growing over the 8-
    // neighbourhood. The inclusion band is anchored on the LOCAL seed value (not
    // a global threshold), so a non-uniform background outside the band does not
    // get pulled in even where it is brighter than the seed elsewhere.
    std::vector<unsigned char> mask(static_cast<std::size_t>(width)
                                        * static_cast<std::size_t>(height),
                                    0);
    std::vector<int> stack;
    const std::size_t seedIdx =
        static_cast<std::size_t>(sy) * static_cast<std::size_t>(width)
        + static_cast<std::size_t>(sx);
    // The seed pixel is trivially in-band (|seedValue - seedValue| = 0 <= band),
    // so it always starts the fill.
    mask[seedIdx] = 1;
    stack.push_back(sy * width + sx);
    while (!stack.empty()) {
        const int idx = stack.back();
        stack.pop_back();
        const int px = idx % width;
        const int py = idx / width;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) {
                    continue;
                }
                const int nx = px + dx;
                const int ny = py + dy;
                if (nx < 0 || nx > width - 1 || ny < 0 || ny > height - 1) {
                    continue;
                }
                const std::size_t nIdx =
                    static_cast<std::size_t>(ny) * static_cast<std::size_t>(width)
                    + static_cast<std::size_t>(nx);
                if (mask[nIdx] != 0) {
                    continue;   // already visited
                }
                if (std::abs(gray[nIdx] - seedValue) <= band) {
                    mask[nIdx] = 1;
                    stack.push_back(ny * width + nx);
                }
            }
        }
    }

    // --- Trace the grown region's boundary ----------------------------------
    // Convert the 0/1 mask to a double field and trace the iso = 0.5 loop (which
    // sits exactly between kept and rejected pixels), then keep the seed's loop.
    // Same tracer + selection as thresholdContour: no second implementation.
    std::vector<double> maskField(mask.size(), 0.0);
    for (std::size_t i = 0; i < mask.size(); ++i) {
        maskField[i] = mask[i] != 0 ? 1.0 : 0.0;
    }
    return traceSeededIsoLoop(maskField, width, height, pixelSizeMm, 0.5, seed);
}

} // namespace xq
