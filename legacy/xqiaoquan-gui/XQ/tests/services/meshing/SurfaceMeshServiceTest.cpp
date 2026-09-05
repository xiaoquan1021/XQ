#include <core/NodeId.h>
#include <core/XQContourGroup.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQMesh.h>
#include <core/XQMeshPayload.h>
#include <core/XQScene.h>
#include <core/XQSurfaceModel.h>
#include <core/command/XQCommandStack.h>
#include <services/meshing/SurfaceMeshService.h>
#include <services/modeling/ContourLoftInputBuilder.h>
#include <services/modeling/ModelingService.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <vector>

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

const double kPi = 3.14159265358979323846;

xq::XQContour makeCircle(int contourId, std::size_t n, double r, double z)
{
    xq::XQContour c;
    c.contourId = xq::NodeId(contourId);
    c.pathArcLength = z;
    c.frame.origin = {0.0, 0.0, z};
    c.frame.normal = {0.0, 0.0, 1.0};
    c.frame.xAxis = {1.0, 0.0, 0.0};
    c.frame.yAxis = {0.0, 1.0, 0.0};
    c.type = xq::ContourType::Circle;
    c.closed = true;
    for (std::size_t k = 0; k < n; ++k) {
        const double a = 2.0 * kPi * static_cast<double>(k) / static_cast<double>(n);
        xq::Point3 p;
        p.x = r * std::cos(a);
        p.y = r * std::sin(a);
        p.z = z;
        c.points.push_back(p);
    }
    return c;
}

// Builds a capped (closed) tube model bound to a source contour group.
std::shared_ptr<xq::XQSurfaceModel> makeCappedTube(std::size_t N, std::size_t M,
                                                   const xq::NodeId& groupId)
{
    xq::XQContourGroup group;
    group.setId(groupId);
    for (std::size_t i = 0; i < M; ++i) {
        group.addContour(makeCircle(static_cast<int>(i + 1), N, 2.0,
                                    static_cast<double>(i) * 2.0));
    }
    xq::ContourLoftInputBuilder::Options opt;
    opt.pointsPerContour = N;
    const xq::ContourLoftInputBuilder::Result built =
        xq::ContourLoftInputBuilder::buildLoftInput(group, opt);
    if (!built.ok()) {
        return nullptr;
    }
    const xq::ModelingService::Result lofted = xq::ModelingService::loftSurface(built.input);
    if (!lofted.ok()) {
        return nullptr;
    }
    const xq::ModelingService::Result capped =
        xq::ModelingService::capModel(*lofted.model, xq::ModelingService::CapOptions{});
    if (!capped.ok()) {
        return nullptr;
    }
    return capped.model;
}

std::size_t node_count(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes([&count](const xq::XQDataNode&) { ++count; });
    return count;
}

std::size_t relation_count(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_derived_relations([&count](const xq::NodeId&, const xq::NodeId&) { ++count; });
    return count;
}

} // namespace

int main()
{
    const std::size_t N = 16;
    const std::size_t M = 5;
    const xq::NodeId groupId(200);
    std::shared_ptr<xq::XQSurfaceModel> tube = makeCappedTube(N, M, groupId);
    CHECK(tube != nullptr);
    CHECK(tube->hasTriangleGeometry());
    const std::size_t modelPointCount = tube->triangleGeometry()->pointCount();
    const std::size_t modelTriCount = tube->triangleGeometry()->triangleCount();

    // ===================================================================
    // 1. buildSurfaceMesh: geometry copied verbatim; faces preserved
    // ===================================================================
    {
        const xq::SurfaceMeshService::Result r =
            xq::SurfaceMeshService::buildSurfaceMesh(*tube, xq::SurfaceMeshService::Params{});
        CHECK(r.ok());
        CHECK(r.mesh != nullptr);
        CHECK(r.mesh->hasSurfaceTriangles());

        const auto& surf = *r.mesh->surfaceTriangles();
        CHECK(surf.is_valid());
        // surface mesh == model triangle geometry (no remeshing yet)
        CHECK(surf.pointCount() == modelPointCount);
        CHECK(surf.triangleCount() == modelTriCount);

        // count-only handle stays consistent for handle-path consumers
        CHECK(r.mesh->surfaceMesh() != nullptr);
        CHECK(r.mesh->surfaceMesh()->is_valid());
        CHECK(r.mesh->surfaceMesh()->pointCount() == modelPointCount);
        CHECK(r.mesh->surfaceMesh()->cellCount() == modelTriCount);

        // one MeshBoundaryFace per ModelFace, faceId/kind/capId identical
        CHECK(r.mesh->boundaryFaces().size() == tube->faces().size());
        for (const xq::ModelFace& mf : tube->faces()) {
            xq::MeshBoundaryFace bf = {};
            const bool found = r.mesh->boundaryFaceById(mf.faceId, &bf);
            CHECK(found);
            CHECK(bf.faceId == mf.faceId);
            CHECK(bf.name == mf.name);
            CHECK(bf.kind == mf.kind);
            CHECK(bf.capId.has_value() == mf.capId.has_value());
            if (mf.capId.has_value()) {
                CHECK(*bf.capId == *mf.capId);
            }
            // every cell tagged with that faceId belongs to the face
            CHECK(!bf.cellIds.empty());
        }

        // boundary-face cellIds partition all triangles exactly once
        std::size_t totalCells = 0;
        for (const xq::MeshBoundaryFace& bf : r.mesh->boundaryFaces()) {
            totalCells += bf.cellIds.size();
        }
        CHECK(totalCells == modelTriCount);

        // quality summary: count correct, min <= mean <= max, all in (0,1]
        const xq::MeshQualitySummary& q = r.mesh->quality();
        CHECK(q.elementCount == modelTriCount);
        CHECK(q.minQuality <= q.meanQuality);
        CHECK(q.meanQuality <= q.maxQuality);
        CHECK(q.minQuality > 0.0);
        CHECK(q.maxQuality <= 1.0 + 1e-9);
    }

    // ===================================================================
    // 2. Invalid model -> InvalidModel; no mesh
    // ===================================================================
    {
        xq::XQSurfaceModel empty;
        const xq::SurfaceMeshService::Result r =
            xq::SurfaceMeshService::buildSurfaceMesh(empty, xq::SurfaceMeshService::Params{});
        CHECK(r.status == xq::SurfaceMeshService::Status::InvalidModel);
        CHECK(r.mesh == nullptr);
    }

    // ===================================================================
    // 3. buildSurfaceMeshCommand: into scene w/ source relation + undo/redo
    // ===================================================================
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId modelNodeId(201);
        const xq::NodeId meshId(202);
        // a model node to be the source of the derived relation
        scene.insert(xq::XQDataNode(modelNodeId, xq::XQDomainType::SurfaceModel, "aorta-model",
                                    std::shared_ptr<xq::XQPayload>()));

        xq::SurfaceMeshService::CommandResult cmd =
            xq::SurfaceMeshService::buildSurfaceMeshCommand(&scene, meshId, "aorta-surface-mesh",
                                                            *tube, modelNodeId,
                                                            xq::SurfaceMeshService::Params{});
        CHECK(cmd.ok());
        CHECK(cmd.command != nullptr);

        const std::size_t nodesBefore = node_count(scene);
        stack.push(std::move(cmd.command));
        CHECK(node_count(scene) == nodesBefore + 1);
        CHECK(relation_count(scene) == 1);

        const xq::XQDataNode* node = scene.find(meshId);
        CHECK(node != nullptr);
        CHECK(node->domainType() == xq::XQDomainType::Mesh);
        const auto* payload = dynamic_cast<const xq::XQMeshPayload*>(node->payload().get());
        CHECK(payload != nullptr);
        CHECK(payload->mesh().hasSurfaceTriangles());
        CHECK(payload->mesh().surfaceTriangles()->triangleCount() == modelTriCount);
        CHECK(payload->mesh().hasSourceModelNode());
        CHECK(payload->mesh().sourceModelNode() == modelNodeId);

        // payload clone is a deep copy (distinct surface handle, same content)
        const std::shared_ptr<xq::XQPayload> cloned = payload->clone();
        const auto* clonedMesh = dynamic_cast<const xq::XQMeshPayload*>(cloned.get());
        CHECK(clonedMesh != nullptr);
        CHECK(clonedMesh->mesh().surfaceTriangles().get()
              != payload->mesh().surfaceTriangles().get());
        CHECK(clonedMesh->mesh().surfaceTriangles()->triangleCount() == modelTriCount);

        // undo removes node + relation; redo restores both
        const bool undone = stack.undo();
        CHECK(undone);
        CHECK(node_count(scene) == nodesBefore);
        CHECK(relation_count(scene) == 0);
        CHECK(scene.find(meshId) == nullptr);

        const bool redone = stack.redo();
        CHECK(redone);
        CHECK(node_count(scene) == nodesBefore + 1);
        CHECK(relation_count(scene) == 1);
        CHECK(scene.find(meshId) != nullptr);
    }

    // buildSurfaceMeshCommand: null scene rejected
    {
        xq::SurfaceMeshService::CommandResult cmd =
            xq::SurfaceMeshService::buildSurfaceMeshCommand(nullptr, xq::NodeId(1), "m", *tube,
                                                            xq::NodeId(2),
                                                            xq::SurfaceMeshService::Params{});
        CHECK(cmd.status == xq::SurfaceMeshService::Status::NullScene);
        CHECK(cmd.command == nullptr);
    }

    std::printf("OK: surface mesh service\n");
    return 0;
}
