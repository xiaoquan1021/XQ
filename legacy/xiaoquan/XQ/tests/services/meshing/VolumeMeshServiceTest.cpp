#include <core/GeometryTypes.h>
#include <core/NodeId.h>
#include <core/XQContourGroup.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQMesh.h>
#include <core/XQMeshPayload.h>
#include <core/XQScene.h>
#include <core/XQSurfaceModel.h>
#include <core/XQTetVolumeMeshHandle.h>
#include <core/XQTriangleSurfaceGeometryHandle.h>
#include <core/command/XQCommandStack.h>
#include <core/meshing/ITetMesher.h>
#include <services/meshing/VolumeMeshService.h>
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

std::shared_ptr<xq::XQSurfaceModel> makeOpenTube(std::size_t N, std::size_t M,
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
    return lofted.model; // open: ends not capped
}

double tetSignedVolume(const xq::XQTetVolumeMeshHandle& g,
                       const xq::XQTetVolumeMeshHandle::Tet& t)
{
    const xq::Point3& a = g.point(static_cast<std::size_t>(t[0]));
    const xq::Point3& b = g.point(static_cast<std::size_t>(t[1]));
    const xq::Point3& c = g.point(static_cast<std::size_t>(t[2]));
    const xq::Point3& d = g.point(static_cast<std::size_t>(t[3]));
    return xq::dot(xq::cross(xq::sub(b, a), xq::sub(c, a)), xq::sub(d, a)) / 6.0;
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

// A kernel stand-in for the injected path. It ignores the input surface and
// returns a fixed, hand-built single-tet volume with two known boundary faces
// (faceId 2 -> tet 0 localFace 1; faceId 3 -> tet 0 localFace 2), so the test
// can assert the service fills cellIds/localFaces/faceId straight from here
// without depending on any real TetGen/MMG kernel.
class FakeTetMesher : public xq::ITetMesher {
public:
    xq::TetMeshResult tetrahedralize(const xq::IGeometrySource& /*surface*/,
                                     const xq::TetMeshParams& /*params*/) override
    {
        xq::TetMeshResult result;
        auto tets = std::make_shared<xq::XQTetVolumeMeshHandle>();
        tets->addPoint({0.0, 0.0, 0.0});
        tets->addPoint({1.0, 0.0, 0.0});
        tets->addPoint({0.0, 1.0, 0.0});
        tets->addPoint({0.0, 0.0, 1.0});
        tets->addTet(0, 1, 2, 3);

        xq::TetBoundaryFace inletFace;
        inletFace.tri = {0, 1, 3};
        inletFace.faceId = 2; // inlet in the tube fixture
        inletFace.tetIndex = 0;
        inletFace.localFace = 1;
        result.boundaryFaces.push_back(inletFace);

        xq::TetBoundaryFace outletFace;
        outletFace.tri = {0, 2, 3};
        outletFace.faceId = 3; // outlet in the tube fixture
        outletFace.tetIndex = 0;
        outletFace.localFace = 2;
        result.boundaryFaces.push_back(outletFace);

        result.ok = true;
        result.mesh = tets;
        return result;
    }
};

} // namespace

int main()
{
    const std::size_t N = 16;
    const std::size_t M = 5;
    const xq::NodeId groupId(300);
    std::shared_ptr<xq::XQSurfaceModel> tube = makeCappedTube(N, M, groupId);
    CHECK(tube != nullptr);
    CHECK(tube->hasTriangleGeometry());
    const auto& surface = *tube->triangleGeometry();
    const std::size_t surfacePoints = surface.pointCount();
    const std::size_t surfaceTris = surface.triangleCount();

    // ===================================================================
    // 1. buildVolumeMesh: star tetrahedralization of a closed tube
    // ===================================================================
    {
        const xq::VolumeMeshService::Result r =
            xq::VolumeMeshService::buildVolumeMesh(surface, tube->faces(),
                                                   xq::VolumeMeshService::Params{});
        CHECK(r.ok());
        CHECK(r.mesh != nullptr);
        CHECK(r.mesh->hasVolumeTets());

        const auto& tets = *r.mesh->volumeTets();
        CHECK(tets.is_valid());
        // one tet per surface triangle; one extra point (the centroid)
        CHECK(tets.tetCount() == surfaceTris);
        CHECK(tets.pointCount() == surfacePoints + 1);

        // count-only handle stays consistent
        CHECK(r.mesh->volumeGrid() != nullptr);
        CHECK(r.mesh->volumeGrid()->is_valid());
        CHECK(r.mesh->volumeGrid()->pointCount() == surfacePoints + 1);
        CHECK(r.mesh->volumeGrid()->cellCount() == surfaceTris);

        // all tets have the same signed-volume sign (no folds in a star domain)
        bool firstPositive = false;
        for (std::size_t t = 0; t < tets.tetCount(); ++t) {
            const double v = tetSignedVolume(tets, tets.tet(t));
            CHECK(std::fabs(v) > 0.0); // no degenerate tet
            if (t == 0) {
                firstPositive = (v > 0.0);
            } else {
                CHECK((v > 0.0) == firstPositive);
            }
        }

        // boundary faces preserved: wall(1)/inlet(2)/outlet(3) all present,
        // cellIds partition all tets exactly once
        CHECK(r.mesh->boundaryFaces().size() == tube->faces().size());
        std::size_t totalCells = 0;
        for (const xq::ModelFace& mf : tube->faces()) {
            xq::MeshBoundaryFace bf = {};
            const bool found = r.mesh->boundaryFaceById(mf.faceId, &bf);
            CHECK(found);
            CHECK(bf.kind == mf.kind);
            CHECK(bf.capId.has_value() == mf.capId.has_value());
            if (mf.capId.has_value()) {
                CHECK(*bf.capId == *mf.capId);
            }
            CHECK(!bf.cellIds.empty());
            totalCells += bf.cellIds.size();
        }
        CHECK(totalCells == surfaceTris);

        // quality summary
        const xq::MeshQualitySummary& q = r.mesh->quality();
        CHECK(q.elementCount == surfaceTris);
        CHECK(q.minQuality <= q.meanQuality);
        CHECK(q.meanQuality <= q.maxQuality);
        CHECK(q.maxQuality <= 1.0 + 1e-9);
        CHECK(q.minQuality >= 0.0);
    }

    // ===================================================================
    // 2. open surface -> NotClosed; nothing produced
    // ===================================================================
    {
        std::shared_ptr<xq::XQSurfaceModel> open = makeOpenTube(N, M, xq::NodeId(301));
        CHECK(open != nullptr);
        const xq::VolumeMeshService::Result r =
            xq::VolumeMeshService::buildVolumeMesh(*open->triangleGeometry(), open->faces(),
                                                   xq::VolumeMeshService::Params{});
        CHECK(r.status == xq::VolumeMeshService::Status::NotClosed);
        CHECK(r.mesh == nullptr);
    }

    // empty surface -> InvalidSurface
    {
        xq::XQTriangleSurfaceGeometryHandle emptySurface;
        std::vector<xq::ModelFace> noFaces;
        const xq::VolumeMeshService::Result r =
            xq::VolumeMeshService::buildVolumeMesh(emptySurface, noFaces,
                                                   xq::VolumeMeshService::Params{});
        CHECK(r.status == xq::VolumeMeshService::Status::InvalidSurface);
        CHECK(r.mesh == nullptr);
    }

    // ===================================================================
    // 3. buildVolumeMeshCommand: into scene w/ source relation + undo/redo
    // ===================================================================
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId sourceNodeId(310);
        const xq::NodeId meshId(311);
        scene.insert(xq::XQDataNode(sourceNodeId, xq::XQDomainType::SurfaceModel, "aorta-model",
                                    std::shared_ptr<xq::XQPayload>()));

        xq::VolumeMeshService::CommandResult cmd =
            xq::VolumeMeshService::buildVolumeMeshCommand(&scene, meshId, "aorta-volume-mesh",
                                                          surface, tube->faces(), sourceNodeId,
                                                          xq::VolumeMeshService::Params{});
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
        CHECK(payload->mesh().hasVolumeTets());
        CHECK(payload->mesh().volumeTets()->tetCount() == surfaceTris);
        CHECK(payload->mesh().hasSourceModelNode());
        CHECK(payload->mesh().sourceModelNode() == sourceNodeId);

        // payload clone deep-copies the volume handle
        const std::shared_ptr<xq::XQPayload> cloned = payload->clone();
        const auto* clonedMesh = dynamic_cast<const xq::XQMeshPayload*>(cloned.get());
        CHECK(clonedMesh != nullptr);
        CHECK(clonedMesh->mesh().volumeTets().get() != payload->mesh().volumeTets().get());
        CHECK(clonedMesh->mesh().volumeTets()->tetCount() == surfaceTris);

        const bool undone = stack.undo();
        CHECK(undone);
        CHECK(node_count(scene) == nodesBefore);
        CHECK(relation_count(scene) == 0);

        const bool redone = stack.redo();
        CHECK(redone);
        CHECK(node_count(scene) == nodesBefore + 1);
        CHECK(relation_count(scene) == 1);
    }

    // null scene rejected
    {
        xq::VolumeMeshService::CommandResult cmd =
            xq::VolumeMeshService::buildVolumeMeshCommand(nullptr, xq::NodeId(1), "m", surface,
                                                          tube->faces(), xq::NodeId(2),
                                                          xq::VolumeMeshService::Params{});
        CHECK(cmd.status == xq::VolumeMeshService::Status::NullScene);
        CHECK(cmd.command == nullptr);
    }

    // ===================================================================
    // 4. transferBoundaryFaces: faceId/kind/capId carried; cellIds not
    // ===================================================================
    {
        const xq::VolumeMeshService::Result r =
            xq::VolumeMeshService::buildVolumeMesh(surface, tube->faces(),
                                                   xq::VolumeMeshService::Params{});
        CHECK(r.ok());
        xq::XQMesh dest;
        xq::VolumeMeshService::transferBoundaryFaces(*r.mesh, dest);
        CHECK(dest.boundaryFaces().size() == r.mesh->boundaryFaces().size());
        for (const xq::MeshBoundaryFace& src : r.mesh->boundaryFaces()) {
            xq::MeshBoundaryFace got = {};
            const bool found = dest.boundaryFaceById(src.faceId, &got);
            CHECK(found);
            CHECK(got.kind == src.kind);
            CHECK(got.capId.has_value() == src.capId.has_value());
            // cellIds are not transferred across meshes
            CHECK(got.cellIds.empty());
        }
    }

    // ===================================================================
    // 5. injected ITetMesher path: mesh + boundary faces come from the kernel
    //    (cellIds == tetIndex, localFaces == localFace), and faceId/kind map
    //    from `faces`. The star path (mesher == nullptr) leaves localFaces empty.
    // ===================================================================
    {
        FakeTetMesher fake;
        const xq::VolumeMeshService::Result r =
            xq::VolumeMeshService::buildVolumeMesh(surface, tube->faces(),
                                                   xq::VolumeMeshService::Params{}, &fake);
        CHECK(r.ok());
        CHECK(r.mesh != nullptr);
        CHECK(r.mesh->hasVolumeTets());

        // tets come straight from the fake kernel (one tet, four points)
        const auto& tets = *r.mesh->volumeTets();
        CHECK(tets.tetCount() == 1);
        CHECK(tets.pointCount() == 4);
        CHECK(r.mesh->volumeGrid() != nullptr);
        CHECK(r.mesh->volumeGrid()->cellCount() == 1);
        CHECK(r.mesh->volumeGrid()->pointCount() == 4);

        // inlet (faceId 2): cellIds/localFaces parallel and from the kernel
        xq::MeshBoundaryFace inlet = {};
        const bool foundInlet = r.mesh->boundaryFaceById(2, &inlet);
        CHECK(foundInlet);
        CHECK(inlet.cellIds.size() == 1);
        CHECK(inlet.localFaces.size() == inlet.cellIds.size());
        CHECK(inlet.cellIds[0] == 0);
        CHECK(inlet.localFaces[0] == 1);
        xq::ModelFace inletModel = {};
        const bool inletInModel = tube->faceById(2, &inletModel);
        CHECK(inletInModel);
        CHECK(inlet.kind == inletModel.kind); // kind mapped from `faces`

        // outlet (faceId 3): different local face, same single owning tet
        xq::MeshBoundaryFace outlet = {};
        const bool foundOutlet = r.mesh->boundaryFaceById(3, &outlet);
        CHECK(foundOutlet);
        CHECK(outlet.cellIds.size() == 1);
        CHECK(outlet.localFaces.size() == 1);
        CHECK(outlet.cellIds[0] == 0);
        CHECK(outlet.localFaces[0] == 2);
        xq::ModelFace outletModel = {};
        const bool outletInModel = tube->faceById(3, &outletModel);
        CHECK(outletInModel);
        CHECK(outlet.kind == outletModel.kind);

        // star path (no mesher) leaves localFaces empty for every boundary face
        const xq::VolumeMeshService::Result star =
            xq::VolumeMeshService::buildVolumeMesh(surface, tube->faces(),
                                                   xq::VolumeMeshService::Params{});
        CHECK(star.ok());
        for (const xq::MeshBoundaryFace& bf : star.mesh->boundaryFaces()) {
            CHECK(bf.localFaces.empty());
            CHECK(!bf.cellIds.empty()); // star path still fills cellIds
        }
    }

    std::printf("OK: volume mesh service\n");
    return 0;
}
