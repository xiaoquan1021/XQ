#include <core/NodeId.h>
#include <core/XQContourGroup.h>
#include <core/XQSurfaceModel.h>
#include <core/XQTriangleSurfaceGeometryHandle.h>
#include <io/project/CTGRContourReader.h>
#include <services/modeling/ContourLoftInputBuilder.h>
#include <services/modeling/ModelingService.h>

#include <cstddef>
#include <cstdio>
#include <map>
#include <string>
#include <utility>

namespace {

int fail(const char* what, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", what, line);
    return 1;
}

} // namespace

// Explicit-failure checks (no assert): survive Release /DNDEBUG. Calls with side
// effects are evaluated into a variable before being checked.
#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

namespace {

bool isClosedManifold(const xq::XQTriangleSurfaceGeometryHandle& g)
{
    std::map<std::pair<int, int>, int> edgeUse;
    const std::size_t triCount = g.triangleCount();
    for (std::size_t t = 0; t < triCount; ++t) {
        const auto& tri = g.triangle(t);
        for (int e = 0; e < 3; ++e) {
            int a = tri[e];
            int b = tri[(e + 1) % 3];
            if (a > b) {
                const int tmp = a;
                a = b;
                b = tmp;
            }
            ++edgeUse[std::make_pair(a, b)];
        }
    }
    for (const auto& kv : edgeUse) {
        if (kv.second != 2) {
            return false;
        }
    }
    return !edgeUse.empty();
}

} // namespace

int main()
{
    // Read the real 0007 aorta contour group from its .ctgr.
    const std::string ctgrPath = std::string(XQ_CTGR_DIR) + "/aorta_final.ctgr";
    xq::CTGRReadResult read;
    const xq::CTGRContourReader::Status status =
        xq::CTGRContourReader::read(ctgrPath, &read);
    CHECK(status == xq::CTGRContourReader::Status::Ok);

    // The aorta group has many stacked contours along the path.
    CHECK(read.group.contours().size() >= 2);
    // Give the group a stable id so the lofted model can bind to it as source.
    xq::XQContourGroup group = read.group;
    group.setId(xq::NodeId(7000));

    // Build the loft input: order + uniform resample + anti-twist alignment.
    xq::ContourLoftInputBuilder::Options opt; // pointsPerContour = 0 -> largest
    const xq::ContourLoftInputBuilder::Result built =
        xq::ContourLoftInputBuilder::buildLoftInput(group, opt);
    CHECK(built.ok());
    CHECK(built.input.rings.size() >= 2);
    CHECK(built.input.pointsPerContour >= 3);

    // Loft an open tube surface.
    const xq::ModelingService::Result lofted =
        xq::ModelingService::loftSurface(built.input);
    CHECK(lofted.ok());
    CHECK(lofted.model != nullptr);
    CHECK(lofted.model->hasTriangleGeometry());
    const auto& openGeometry = *lofted.model->triangleGeometry();
    CHECK(openGeometry.is_valid());
    CHECK(openGeometry.pointCount() > 0);
    CHECK(openGeometry.triangleCount() > 0);
    // open tube is not yet closed
    CHECK(!isClosedManifold(openGeometry));
    // bound to the source contour group
    CHECK(lofted.model->hasSourceContourGroupNode());
    CHECK(lofted.model->sourceContourGroupNode() == xq::NodeId(7000));

    // Cap the open ends: the surface must become a closed manifold.
    const xq::ModelingService::Result capped =
        xq::ModelingService::capModel(*lofted.model, xq::ModelingService::CapOptions{});
    CHECK(capped.ok());
    CHECK(capped.model != nullptr);
    const auto& closedGeometry = *capped.model->triangleGeometry();
    CHECK(closedGeometry.is_valid());
    CHECK(isClosedManifold(closedGeometry)); // every edge shared by exactly 2 triangles

    // ModelFace metadata: wall + at least one cap, stable wall id preserved.
    xq::ModelFace wall = {};
    const bool hasWall = capped.model->faceById(xq::ModelingService::wallFaceId(), &wall);
    CHECK(hasWall);
    CHECK(wall.kind == xq::FaceKind::Wall);
    CHECK(capped.model->faces().size() >= 3); // wall + inlet + outlet
    CHECK(capped.model->hasSourceContourGroupNode());
    CHECK(capped.model->sourceContourGroupNode() == xq::NodeId(7000));

    std::printf("OK: aorta loft %zu points / %zu tris -> capped %zu points / %zu tris, %zu faces\n",
                openGeometry.pointCount(), openGeometry.triangleCount(),
                closedGeometry.pointCount(), closedGeometry.triangleCount(),
                capped.model->faces().size());
    return 0;
}
