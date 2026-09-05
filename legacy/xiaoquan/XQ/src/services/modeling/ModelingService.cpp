#include "services/modeling/ModelingService.h"

#include "core/GeometryTypes.h"
#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQScene.h"
#include "core/XQSurfaceModel.h"
#include "core/XQSurfaceModelPayload.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/command/XQSceneCommands.h"
#include "core/source/ResidentSurfaceSource.h"

#include <algorithm>
#include <cstddef>
#include <map>
#include <utility>
#include <vector>

namespace xq {
namespace {

ModelingService::Result modelFailure(ModelingService::Status status)
{
    ModelingService::Result result;
    result.status = status;
    result.model = nullptr;
    return result;
}

ModelingService::Result modelSuccess(std::shared_ptr<XQSurfaceModel> model)
{
    ModelingService::Result result;
    result.status = ModelingService::Status::Ok;
    result.model = std::move(model);
    return result;
}

// An undirected edge keyed by its sorted endpoint indices.
using Edge = std::pair<int, int>;

Edge makeEdge(int a, int b)
{
    return a < b ? Edge(a, b) : Edge(b, a);
}

// Extracts boundary loops from a triangle surface: a boundary edge is one used
// by exactly one triangle. The directed boundary edges are chained into ordered
// loops (each loop is a sequence of point indices). Orientation follows the
// triangles, so a centroid fan built per loop closes the surface consistently.
// Consumes the triangle connectivity through a contiguous Source span (M9a).
std::vector<std::vector<int>> extractBoundaryLoops(const ReadSpan<SourceTriangle>& tris)
{
    // Count undirected-edge usage and remember the directed boundary edge.
    std::map<Edge, int> useCount;
    std::map<int, int> nextOf; // directed: from -> to, for boundary edges only

    const std::size_t triangleCount = tris.size();
    for (std::size_t t = 0; t < triangleCount; ++t) {
        const SourceTriangle& tri = tris[t];
        for (int e = 0; e < 3; ++e) {
            const int a = tri[e];
            const int b = tri[(e + 1) % 3];
            ++useCount[makeEdge(a, b)];
        }
    }
    for (std::size_t t = 0; t < triangleCount; ++t) {
        const SourceTriangle& tri = tris[t];
        for (int e = 0; e < 3; ++e) {
            const int a = tri[e];
            const int b = tri[(e + 1) % 3];
            if (useCount[makeEdge(a, b)] == 1) {
                nextOf[a] = b; // directed boundary edge a -> b
            }
        }
    }

    // Chain directed boundary edges into loops.
    std::vector<std::vector<int>> loops;
    std::map<int, bool> visited;
    for (const auto& kv : nextOf) {
        const int start = kv.first;
        if (visited[start]) {
            continue;
        }
        std::vector<int> loop;
        int current = start;
        // Walk until we return to start or hit a missing/visited link.
        while (nextOf.count(current) && !visited[current]) {
            visited[current] = true;
            loop.push_back(current);
            current = nextOf[current];
        }
        if (loop.size() >= 3) {
            loops.push_back(std::move(loop));
        }
    }
    return loops;
}

Point3 loopCentroid(const ReadSpan<Point3>& points, const std::vector<int>& loop)
{
    Point3 centroid{0.0, 0.0, 0.0};
    for (int idx : loop) {
        centroid = add(centroid, points[static_cast<std::size_t>(idx)]);
    }
    const double inv = 1.0 / static_cast<double>(loop.size());
    return scale(centroid, inv);
}

} // namespace

int ModelingService::wallFaceId()
{
    return 1;
}

ModelingService::Result ModelingService::loftSurface(const XQContourLoftInput& input)
{
    const std::size_t ringCount = input.rings.size();
    if (ringCount < 2) {
        return modelFailure(Status::NotEnoughRings);
    }
    const std::size_t n = input.pointsPerContour;
    if (n < 3) {
        return modelFailure(Status::InconsistentRings);
    }
    for (const XQLoftRing& ring : input.rings) {
        if (ring.points.size() != n) {
            return modelFailure(Status::InconsistentRings);
        }
    }

    auto geometry = std::make_shared<XQTriangleSurfaceGeometryHandle>();
    // Lay out all ring points contiguously: point (ring i, vertex k) -> i*n + k.
    for (const XQLoftRing& ring : input.rings) {
        for (const Point3& p : ring.points) {
            geometry->addPoint(p);
        }
    }

    // Stitch each adjacent ring pair into a triangle strip. For corresponding
    // edges k -> k+1 (mod n) the quad (a0,a1,b1,b0) splits into two triangles,
    // wound consistently (a0,a1,b1) and (a0,b1,b0).
    for (std::size_t i = 0; i + 1 < ringCount; ++i) {
        const int baseA = static_cast<int>(i * n);
        const int baseB = static_cast<int>((i + 1) * n);
        for (std::size_t k = 0; k < n; ++k) {
            const int k1 = static_cast<int>((k + 1) % n);
            const int a0 = baseA + static_cast<int>(k);
            const int a1 = baseA + k1;
            const int b0 = baseB + static_cast<int>(k);
            const int b1 = baseB + k1;
            geometry->addTriangle(a0, a1, b1, wallFaceId());
            geometry->addTriangle(a0, b1, b0, wallFaceId());
        }
    }

    if (!geometry->is_valid()) {
        return modelFailure(Status::InconsistentRings);
    }

    auto model = std::make_shared<XQSurfaceModel>();
    model->setTriangleGeometry(geometry);
    model->setSource(ModelSource::Generated);
    if (input.sourceContourGroup.is_valid()) {
        model->setSourceContourGroupNode(input.sourceContourGroup);
    }

    ModelFace wall;
    wall.faceId = wallFaceId();
    wall.name = "wall";
    wall.kind = FaceKind::Wall;
    model->addFace(wall);

    return modelSuccess(std::move(model));
}

ModelingService::Result ModelingService::capModel(const XQSurfaceModel& model,
                                                  const CapOptions& options)
{
    if (!model.hasTriangleGeometry() || !model.triangleGeometry()->is_valid()) {
        return modelFailure(Status::InvalidModel);
    }
    const XQTriangleSurfaceGeometryHandle& src = *model.triangleGeometry();

    // Consume the input surface through the read-only Source contract (M9a):
    // acquire points + triangles once, then the boundary-loop extraction and the
    // centroid computation read the contiguous spans. The aliasing shared_ptr
    // borrows src without owning it.
    ResidentSurfaceSource srcSource(
        std::shared_ptr<const XQTriangleSurfaceGeometryHandle>(
            std::shared_ptr<const void>(), &src));
    GeometryLease<Point3> srcPointLease = srcSource.acquire_points();
    TriangleLease srcTriLease = srcSource.acquire_triangles();
    const ReadSpan<Point3>& srcPoints = srcPointLease.span();
    const ReadSpan<SourceTriangle>& srcTris = srcTriLease.view().triangles;

    std::vector<std::vector<int>> loops = extractBoundaryLoops(srcTris);
    if (loops.empty()) {
        return modelFailure(Status::AlreadyClosed);
    }

    // Copy existing geometry into the new model's handle, preserving point
    // indices so existing wall triangles stay valid.
    auto geometry = std::make_shared<XQTriangleSurfaceGeometryHandle>(src);

    // Order loops along the dominant extent of their centroids so inlet (first)
    // and outlet (last) are stable. We project centroids onto the line through
    // the two farthest-apart centroids (robust principal axis for a tube).
    std::vector<Point3> centroids;
    centroids.reserve(loops.size());
    for (const std::vector<int>& loop : loops) {
        centroids.push_back(loopCentroid(srcPoints, loop));
    }

    Vec3 axis{0.0, 0.0, 1.0};
    if (centroids.size() >= 2) {
        double bestDist = -1.0;
        std::size_t bi = 0;
        std::size_t bj = 1;
        for (std::size_t a = 0; a < centroids.size(); ++a) {
            for (std::size_t b = a + 1; b < centroids.size(); ++b) {
                const double d = distance(centroids[a], centroids[b]);
                if (d > bestDist) {
                    bestDist = d;
                    bi = a;
                    bj = b;
                }
            }
        }
        const Vec3 candidate = sub(centroids[bj], centroids[bi]);
        if (norm(candidate) > 1e-12) {
            axis = normalized(candidate);
        }
    }

    std::vector<std::size_t> order(loops.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return dot(centroids[a], axis) < dot(centroids[b], axis);
    });

    auto capped = std::make_shared<XQSurfaceModel>();
    capped->setTriangleGeometry(geometry);
    capped->setSource(ModelSource::Generated);
    if (model.hasSourceContourGroupNode()) {
        capped->setSourceContourGroupNode(model.sourceContourGroupNode());
    }
    // Preserve existing (wall) faces.
    for (const ModelFace& f : model.faces()) {
        capped->addFace(f);
    }

    // Centroid-fan triangulate each boundary loop. Each loop edge (a -> b)
    // becomes a triangle (centroid, a, b); the loop is directed so the fan winds
    // consistently with the wall, and each loop edge ends up shared by exactly
    // two triangles (its wall triangle + its cap triangle) -> closed manifold.
    int nextCapId = 1;
    for (std::size_t oi = 0; oi < order.size(); ++oi) {
        const std::vector<int>& loop = loops[order[oi]];
        const Point3 centroid = centroids[order[oi]];
        const int centroidIndex = geometry->addPoint(centroid);

        ModelFace cap;
        const bool isInlet = (oi == 0);
        cap.faceId = isInlet ? options.inletFaceId : options.outletFaceId;
        // Intermediate loops (shouldn't occur for a simple tube) reuse outlet id
        // space by offset to keep ids unique and stable.
        if (!isInlet && oi + 1 < order.size()) {
            cap.faceId = options.outletFaceId + static_cast<int>(oi) - 1;
        }
        cap.name = isInlet ? "inlet" : "outlet";
        cap.kind = isInlet ? FaceKind::Inlet : FaceKind::Outlet;
        cap.capId = nextCapId++;

        // Tag the fan triangles with the cap's face id so the boundary-face
        // association survives end-to-end into the surface / volume mesh.
        for (std::size_t k = 0; k < loop.size(); ++k) {
            const int a = loop[k];
            const int b = loop[(k + 1) % loop.size()];
            geometry->addTriangle(centroidIndex, a, b, cap.faceId);
        }

        capped->addFace(cap);
    }

    if (!geometry->is_valid()) {
        return modelFailure(Status::InvalidModel);
    }
    return modelSuccess(std::move(capped));
}

ModelingService::CommandResult ModelingService::createModelNodeCommand(
    XQScene* scene,
    const NodeId& newModelId,
    const std::string& name,
    const std::shared_ptr<XQSurfaceModel>& model)
{
    CommandResult result;
    if (scene == nullptr) {
        result.status = Status::NullScene;
        result.command = nullptr;
        return result;
    }
    if (model == nullptr || !model->hasTriangleGeometry()
        || !model->triangleGeometry()->is_valid()) {
        result.status = Status::InvalidModel;
        result.command = nullptr;
        return result;
    }

    auto payload = std::make_shared<XQSurfaceModelPayload>(*model);
    const XQDataNode node(newModelId, XQDomainType::SurfaceModel, name, payload);

    result.status = Status::Ok;
    if (model->hasSourceContourGroupNode()) {
        result.command.reset(new AddNodeWithSourceRelationCommand(
            scene, node, model->sourceContourGroupNode(), "Add surface model"));
    } else {
        result.command.reset(new AddNodeCommand(scene, node, "Add surface model"));
    }
    return result;
}

ModelingService::CommandResult ModelingService::loftSurfaceCommand(
    XQScene* scene,
    const NodeId& newModelId,
    const std::string& name,
    const XQContourLoftInput& input)
{
    CommandResult result;
    if (scene == nullptr) {
        result.status = Status::NullScene;
        result.command = nullptr;
        return result;
    }
    const Result lofted = loftSurface(input);
    if (!lofted.ok()) {
        result.status = lofted.status;
        result.command = nullptr;
        return result;
    }
    return createModelNodeCommand(scene, newModelId, name, lofted.model);
}

} // namespace xq
