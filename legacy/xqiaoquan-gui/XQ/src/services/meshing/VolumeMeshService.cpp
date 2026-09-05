#include "services/meshing/VolumeMeshService.h"

#include "core/GeometryTypes.h"
#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQMesh.h"
#include "core/XQMeshPayload.h"
#include "core/XQScene.h"
#include "core/XQTetVolumeMeshHandle.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/command/XQSceneCommands.h"
#include "core/source/ResidentSurfaceSource.h"
#include "core/meshing/ITetMesher.h"
#include "services/meshing/MeshQuality.h"

#include <cstddef>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace xq {
namespace {

VolumeMeshService::Result volumeFailure(VolumeMeshService::Status status)
{
    VolumeMeshService::Result result;
    result.status = status;
    result.mesh = nullptr;
    return result;
}

using Edge = std::pair<int, int>;

Edge makeEdge(int a, int b)
{
    return a < b ? Edge(a, b) : Edge(b, a);
}

// A closed 2-manifold: every undirected edge is shared by exactly two
// triangles. Mirrors the closedness check used by the M3 modeling tests.
// Consumes the triangle connectivity through a contiguous Source span (M9a).
bool isClosedManifold(const ReadSpan<SourceTriangle>& tris)
{
    std::map<Edge, int> edgeUse;
    for (std::size_t t = 0; t < tris.size(); ++t) {
        const SourceTriangle& tri = tris[t];
        for (int e = 0; e < 3; ++e) {
            ++edgeUse[makeEdge(tri[e], tri[(e + 1) % 3])];
        }
    }
    if (edgeUse.empty()) {
        return false;
    }
    for (const auto& kv : edgeUse) {
        if (kv.second != 2) {
            return false;
        }
    }
    return true;
}

// Quality summary over all tets (signed volume -> normalized shape). Shared by
// the star-fallback and injected-kernel paths so the statistics stay identical.
MeshQualitySummary summarizeTetQuality(const XQTetVolumeMeshHandle& tets)
{
    MeshQualitySummary quality;
    quality.elementCount = tets.tetCount();
    if (tets.tetCount() > 0) {
        double minQ = 0.0;
        double maxQ = 0.0;
        double sumQ = 0.0;
        for (std::size_t t = 0; t < tets.tetCount(); ++t) {
            const XQTetVolumeMeshHandle::Tet& cell = tets.tet(t);
            const Point3& a = tets.point(static_cast<std::size_t>(cell[0]));
            const Point3& b = tets.point(static_cast<std::size_t>(cell[1]));
            const Point3& c = tets.point(static_cast<std::size_t>(cell[2]));
            const Point3& d = tets.point(static_cast<std::size_t>(cell[3]));
            const double q = meshing::tetQuality(a, b, c, d);
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
        quality.meanQuality = sumQ / static_cast<double>(tets.tetCount());
    }
    return quality;
}

} // namespace

VolumeMeshService::Result VolumeMeshService::buildVolumeMesh(
    const XQTriangleSurfaceGeometryHandle& surface,
    const std::vector<ModelFace>& faces,
    const Params& /*params*/,
    ITetMesher* mesher)
{
    if (!surface.is_valid()) {
        return volumeFailure(Status::InvalidSurface);
    }

    // Consume the surface through the read-only Source contract (M9a): one
    // acquire each for points and triangles+faceId, then all access is via the
    // contiguous spans (no per-element handle calls). The aliasing shared_ptr
    // borrows the caller's handle without owning it.
    ResidentSurfaceSource surfaceSource(
        std::shared_ptr<const XQTriangleSurfaceGeometryHandle>(
            std::shared_ptr<const void>(), &surface));
    GeometryLease<Point3> pointLease = surfaceSource.acquire_points();
    TriangleLease triLease = surfaceSource.acquire_triangles();
    const ReadSpan<Point3>& points = pointLease.span();
    const TriangleView& triView = triLease.view();
    const ReadSpan<SourceTriangle>& tris = triView.triangles;
    const ReadSpan<int>& faceIdSpan = triView.faceIds;

    if (!isClosedManifold(tris)) {
        return volumeFailure(Status::NotClosed);
    }

    // Lookup table for boundary-face metadata (kind/capId/name) by faceId,
    // shared by both paths. A triangle/boundary face tagged with a faceId not in
    // `faces` is still exposed (kind = Unknown) so the association is not lost.
    std::map<int, MeshBoundaryFace> byFaceId;
    for (const ModelFace& face : faces) {
        MeshBoundaryFace boundary;
        boundary.faceId = face.faceId;
        boundary.name = face.name;
        boundary.kind = face.kind;
        boundary.capId = face.capId;
        byFaceId[face.faceId] = boundary;
    }

    if (mesher != nullptr) {
        // Injected kernel path: delegate the tetrahedralization, then fill the
        // mesh + boundary faces (incl. cell-level connectivity) from the result.
        // The surface is presented to the kernel through the read-only Source
        // contract (M9a); the aliasing shared_ptr borrows the caller's handle
        // without owning it (buildVolumeMesh does not extend its lifetime).
        ResidentSurfaceSource surfaceSource(
            std::shared_ptr<const XQTriangleSurfaceGeometryHandle>(
                std::shared_ptr<const void>(), &surface));
        const TetMeshResult tetResult = mesher->tetrahedralize(surfaceSource, TetMeshParams{});
        if (!tetResult.ok || tetResult.mesh == nullptr) {
            return volumeFailure(Status::InvalidSurface);
        }
        const std::shared_ptr<XQTetVolumeMeshHandle>& tets = tetResult.mesh;

        auto mesh = std::make_shared<XQMesh>();
        mesh->setVolumeTets(tets);

        // Count-only handle for handle-path consumers.
        auto volumeHandle = std::make_shared<VolumeMeshHandle>();
        volumeHandle->setCounts(tets->pointCount(), tets->tetCount());
        mesh->setVolumeGrid(volumeHandle);

        // Boundary faces: group the kernel's boundary records by faceId. cellIds
        // come from each record's tetIndex, localFaces from its localFace (kept
        // parallel); kind/capId/name come from `faces` (or Unknown if absent).
        for (const TetBoundaryFace& bf : tetResult.boundaryFaces) {
            auto it = byFaceId.find(bf.faceId);
            if (it == byFaceId.end()) {
                MeshBoundaryFace boundary;
                boundary.faceId = bf.faceId;
                boundary.kind = FaceKind::Unknown;
                it = byFaceId.emplace(bf.faceId, boundary).first;
            }
            it->second.cellIds.push_back(bf.tetIndex);
            it->second.localFaces.push_back(bf.localFace);
        }
        for (const auto& kv : byFaceId) {
            mesh->addBoundaryFace(kv.second);
        }

        mesh->setQuality(summarizeTetQuality(*tets));
        return VolumeMeshService::Result{Status::Ok, mesh};
    }

    const std::size_t surfacePointCount = points.size();
    const std::size_t triangleCount = tris.size();

    // Centroid star tetrahedralization: copy the surface points, add the vertex
    // centroid, and emit one tet per surface triangle fanning to the centroid.
    auto tets = std::make_shared<XQTetVolumeMeshHandle>();
    for (std::size_t i = 0; i < surfacePointCount; ++i) {
        tets->addPoint(points[i]);
    }
    Point3 centroid{0.0, 0.0, 0.0};
    for (std::size_t i = 0; i < surfacePointCount; ++i) {
        centroid = add(centroid, points[i]);
    }
    centroid = scale(centroid, 1.0 / static_cast<double>(surfacePointCount));
    const int centroidIndex = tets->addPoint(centroid);

    for (std::size_t t = 0; t < triangleCount; ++t) {
        const SourceTriangle& tri = tris[t];
        // Emit each tet with a consistent positive orientation. The source
        // surface's wall and cap triangles do not share a single global winding
        // convention (the M3 closedness check only requires every edge to be
        // shared by two triangles, not a globally consistent normal), so a fixed
        // (tri,C) vertex order would yield mixed-sign volumes that look like
        // folds. Since C is the interior centroid of a star-shaped domain, every
        // (tri,C) tet is geometrically valid; we just swap two vertices when the
        // signed volume is negative so all cells are positively oriented (as
        // TetGen / VTK guarantee for their output).
        const Point3& a = points[static_cast<std::size_t>(tri[0])];
        const Point3& b = points[static_cast<std::size_t>(tri[1])];
        const Point3& c = points[static_cast<std::size_t>(tri[2])];
        if (meshing::tetSignedVolume(a, b, c, centroid) >= 0.0) {
            tets->addTet(tri[0], tri[1], tri[2], centroidIndex);
        } else {
            tets->addTet(tri[0], tri[2], tri[1], centroidIndex);
        }
    }

    auto mesh = std::make_shared<XQMesh>();
    mesh->setVolumeTets(tets);

    // Count-only handle for handle-path consumers.
    auto volumeHandle = std::make_shared<VolumeMeshHandle>();
    volumeHandle->setCounts(tets->pointCount(), tets->tetCount());
    mesh->setVolumeGrid(volumeHandle);

    // Boundary faces: cellId == tet index == triangle index. Each tet inherits
    // the surface triangle's tagged faceId; kind/capId come from `faces`. The
    // star path leaves localFaces empty (no cell-level face connectivity).
    for (std::size_t t = 0; t < triangleCount; ++t) {
        const int faceId = faceIdSpan[t];
        auto it = byFaceId.find(faceId);
        if (it == byFaceId.end()) {
            // Triangle tagged with a faceId not in `faces`: still expose it as a
            // boundary face so the association is not silently dropped.
            MeshBoundaryFace boundary;
            boundary.faceId = faceId;
            boundary.kind = FaceKind::Unknown;
            it = byFaceId.emplace(faceId, boundary).first;
        }
        it->second.cellIds.push_back(static_cast<int>(t));
    }
    for (const auto& kv : byFaceId) {
        mesh->addBoundaryFace(kv.second);
    }

    mesh->setQuality(summarizeTetQuality(*tets));

    return VolumeMeshService::Result{Status::Ok, mesh};
}

VolumeMeshService::CommandResult VolumeMeshService::buildVolumeMeshCommand(
    XQScene* scene,
    const NodeId& newMeshId,
    const std::string& name,
    const XQTriangleSurfaceGeometryHandle& surface,
    const std::vector<ModelFace>& faces,
    const NodeId& sourceNodeId,
    const Params& params,
    ITetMesher* mesher)
{
    CommandResult result;
    if (scene == nullptr) {
        result.status = Status::NullScene;
        result.command = nullptr;
        return result;
    }
    const Result built = buildVolumeMesh(surface, faces, params, mesher);
    if (!built.ok()) {
        result.status = built.status;
        result.command = nullptr;
        return result;
    }

    XQMesh mesh = *built.mesh;
    if (sourceNodeId.is_valid()) {
        mesh.setSourceModelNode(sourceNodeId);
    }
    auto payload = std::make_shared<XQMeshPayload>(std::move(mesh));
    const XQDataNode node(newMeshId, XQDomainType::Mesh, name, payload);

    result.status = Status::Ok;
    if (sourceNodeId.is_valid()) {
        result.command.reset(new AddNodeWithSourceRelationCommand(
            scene, node, sourceNodeId, "Add volume mesh"));
    } else {
        result.command.reset(new AddNodeCommand(scene, node, "Add volume mesh"));
    }
    return result;
}

void VolumeMeshService::transferBoundaryFaces(const XQMesh& from, XQMesh& to)
{
    for (const MeshBoundaryFace& src : from.boundaryFaces()) {
        MeshBoundaryFace existing;
        if (to.boundaryFaceById(src.faceId, &existing)) {
            // `to` already carries this face (with its own cellIds); leave it.
            continue;
        }
        MeshBoundaryFace transferred;
        transferred.faceId = src.faceId;
        transferred.name = src.name;
        transferred.kind = src.kind;
        transferred.capId = src.capId;
        // cellIds intentionally left empty: they index `from`'s cells, not `to`'s.
        to.addBoundaryFace(transferred);
    }
}

} // namespace xq
