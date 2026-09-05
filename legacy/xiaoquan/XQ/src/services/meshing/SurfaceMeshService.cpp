#include "services/meshing/SurfaceMeshService.h"

#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQMesh.h"
#include "core/XQMeshPayload.h"
#include "core/XQScene.h"
#include "core/XQSurfaceModel.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/command/XQSceneCommands.h"
#include "core/source/ResidentSurfaceSource.h"
#include "services/meshing/MeshQuality.h"

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace xq {
namespace {

SurfaceMeshService::Result surfaceFailure(SurfaceMeshService::Status status)
{
    SurfaceMeshService::Result result;
    result.status = status;
    result.mesh = nullptr;
    return result;
}

} // namespace

SurfaceMeshService::Result SurfaceMeshService::buildSurfaceMesh(const XQSurfaceModel& model,
                                                               const Params& /*params*/)
{
    if (!model.hasTriangleGeometry() || !model.triangleGeometry()->is_valid()) {
        return surfaceFailure(Status::InvalidModel);
    }
    const XQTriangleSurfaceGeometryHandle& src = *model.triangleGeometry();

    // First version: the surface mesh is the model's triangle geometry copied
    // verbatim (no remeshing yet). The copy carries the per-triangle face ids.
    auto triangles = std::make_shared<XQTriangleSurfaceGeometryHandle>(src);

    auto mesh = std::make_shared<XQMesh>();
    mesh->setSurfaceTriangles(triangles);

    // Keep the count-only handle populated so the reader/handle path stays
    // consistent for consumers that only look at counts.
    auto surfaceHandle = std::make_shared<SurfaceMeshHandle>();
    surfaceHandle->setCounts(triangles->pointCount(), triangles->triangleCount());
    mesh->setSurfaceMesh(surfaceHandle);

    // Consume the triangle geometry through the read-only Source contract (M9a):
    // one acquire for points and one for triangles+faceId, then all per-element
    // access goes through the contiguous spans.
    ResidentSurfaceSource surfaceSource(triangles);
    GeometryLease<Point3> pointLease = surfaceSource.acquire_points();
    TriangleLease triLease = surfaceSource.acquire_triangles();
    const ReadSpan<Point3>& pts = pointLease.span();
    const TriangleView& triView = triLease.view();
    const ReadSpan<SourceTriangle>& tris = triView.triangles;
    const ReadSpan<int>& faceIdSpan = triView.faceIds;

    // Boundary faces: one MeshBoundaryFace per ModelFace, cellIds = triangle
    // indices tagged with that faceId (set at generation time by ModelingService).
    const std::size_t triangleCount = tris.size();
    for (const ModelFace& face : model.faces()) {
        MeshBoundaryFace boundary;
        boundary.faceId = face.faceId;
        boundary.name = face.name;
        boundary.kind = face.kind;
        boundary.capId = face.capId;
        for (std::size_t t = 0; t < triangleCount; ++t) {
            if (faceIdSpan[t] == face.faceId) {
                boundary.cellIds.push_back(static_cast<int>(t));
            }
        }
        mesh->addBoundaryFace(boundary);
    }

    // Quality summary over all surface triangles.
    MeshQualitySummary quality;
    quality.elementCount = triangleCount;
    if (triangleCount > 0) {
        double minQ = 0.0;
        double maxQ = 0.0;
        double sumQ = 0.0;
        for (std::size_t t = 0; t < triangleCount; ++t) {
            const SourceTriangle& tri = tris[t];
            const double q = meshing::triangleQuality(
                pts[static_cast<std::size_t>(tri[0])],
                pts[static_cast<std::size_t>(tri[1])],
                pts[static_cast<std::size_t>(tri[2])]);
            if (t == 0) {
                minQ = q;
                maxQ = q;
            } else {
                if (q < minQ) {
                    minQ = q;
                }
                if (q > maxQ) {
                    maxQ = q;
                }
            }
            sumQ += q;
        }
        quality.minQuality = minQ;
        quality.maxQuality = maxQ;
        quality.meanQuality = sumQ / static_cast<double>(triangleCount);
    }
    mesh->setQuality(quality);

    return SurfaceMeshService::Result{Status::Ok, mesh};
}

SurfaceMeshService::CommandResult SurfaceMeshService::buildSurfaceMeshCommand(
    XQScene* scene,
    const NodeId& newMeshId,
    const std::string& name,
    const XQSurfaceModel& model,
    const NodeId& modelNodeId,
    const Params& params)
{
    CommandResult result;
    if (scene == nullptr) {
        result.status = Status::NullScene;
        result.command = nullptr;
        return result;
    }
    const Result built = buildSurfaceMesh(model, params);
    if (!built.ok()) {
        result.status = built.status;
        result.command = nullptr;
        return result;
    }

    XQMesh mesh = *built.mesh;
    if (modelNodeId.is_valid()) {
        mesh.setSourceModelNode(modelNodeId);
    }
    auto payload = std::make_shared<XQMeshPayload>(std::move(mesh));
    const XQDataNode node(newMeshId, XQDomainType::Mesh, name, payload);

    result.status = Status::Ok;
    if (modelNodeId.is_valid()) {
        result.command.reset(new AddNodeWithSourceRelationCommand(
            scene, node, modelNodeId, "Add surface mesh"));
    } else {
        result.command.reset(new AddNodeCommand(scene, node, "Add surface mesh"));
    }
    return result;
}

} // namespace xq
