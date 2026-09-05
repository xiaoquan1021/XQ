#include <core/NodeId.h>
#include <core/XQContourGroup.h>
#include <core/XQMesh.h>
#include <core/XQSurfaceModel.h>
#include <core/XQTetVolumeMeshHandle.h>
#include <core/XQTriangleSurfaceGeometryHandle.h>
#include <io/project/CTGRContourReader.h>
#include <services/meshing/SurfaceMeshService.h>
#include <services/meshing/VolumeMeshService.h>
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
    CHECK(read.group.contours().size() >= 2);

    xq::XQContourGroup group = read.group;
    group.setId(xq::NodeId(7000));

    // Loft + cap -> closed surface model.
    xq::ContourLoftInputBuilder::Options opt; // pointsPerContour = 0 -> largest
    const xq::ContourLoftInputBuilder::Result built =
        xq::ContourLoftInputBuilder::buildLoftInput(group, opt);
    CHECK(built.ok());

    const xq::ModelingService::Result lofted = xq::ModelingService::loftSurface(built.input);
    CHECK(lofted.ok());
    const xq::ModelingService::Result capped =
        xq::ModelingService::capModel(*lofted.model, xq::ModelingService::CapOptions{});
    CHECK(capped.ok());
    CHECK(capped.model != nullptr);
    const auto& closedSurface = *capped.model->triangleGeometry();
    CHECK(isClosedManifold(closedSurface));

    // wall + inlet + outlet faces present
    CHECK(capped.model->faces().size() >= 3);

    // ---- surface mesh ----
    const xq::SurfaceMeshService::Result surfaceMesh =
        xq::SurfaceMeshService::buildSurfaceMesh(*capped.model, xq::SurfaceMeshService::Params{});
    CHECK(surfaceMesh.ok());
    CHECK(surfaceMesh.mesh->hasSurfaceTriangles());
    CHECK(surfaceMesh.mesh->surfaceTriangles()->triangleCount()
          == closedSurface.triangleCount());
    // face id full chain: model faces -> surface-mesh boundary faces
    CHECK(surfaceMesh.mesh->boundaryFaces().size() == capped.model->faces().size());
    for (const xq::ModelFace& mf : capped.model->faces()) {
        xq::MeshBoundaryFace bf = {};
        const bool found = surfaceMesh.mesh->boundaryFaceById(mf.faceId, &bf);
        CHECK(found);
        CHECK(bf.kind == mf.kind);
    }
    CHECK(surfaceMesh.mesh->quality().elementCount == closedSurface.triangleCount());

    // ---- volume mesh ----
    const xq::VolumeMeshService::Result volumeMesh =
        xq::VolumeMeshService::buildVolumeMesh(closedSurface, capped.model->faces(),
                                               xq::VolumeMeshService::Params{});
    CHECK(volumeMesh.ok());
    CHECK(volumeMesh.mesh->hasVolumeTets());
    const auto& tets = *volumeMesh.mesh->volumeTets();
    CHECK(tets.is_valid());
    CHECK(tets.tetCount() == closedSurface.triangleCount());
    CHECK(tets.pointCount() == closedSurface.pointCount() + 1);

    // face id full chain end-to-end: model -> volume-mesh boundary faces
    CHECK(volumeMesh.mesh->boundaryFaces().size() == capped.model->faces().size());
    for (const xq::ModelFace& mf : capped.model->faces()) {
        xq::MeshBoundaryFace bf = {};
        const bool found = volumeMesh.mesh->boundaryFaceById(mf.faceId, &bf);
        CHECK(found);
        CHECK(bf.kind == mf.kind);
        CHECK(!bf.cellIds.empty());
    }

    // quality summary non-empty (the real curved aorta may contain degenerate /
    // inverted tets from the centroid star scheme -- the summary must expose
    // them rather than hide them; tech debt: TetGen kernel later).
    const xq::MeshQualitySummary& q = volumeMesh.mesh->quality();
    CHECK(q.elementCount == tets.tetCount());
    CHECK(q.minQuality <= q.meanQuality);
    CHECK(q.meanQuality <= q.maxQuality);

    std::printf("OK: aorta end-to-end -> surface mesh %zu tris, volume mesh %zu tets "
                "(tet quality min=%.4f mean=%.4f max=%.4f)\n",
                surfaceMesh.mesh->surfaceTriangles()->triangleCount(), tets.tetCount(),
                q.minQuality, q.meanQuality, q.maxQuality);
    return 0;
}
