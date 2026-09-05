#include <core/NodeId.h>
#include <core/XQContourGroup.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQScene.h>
#include <core/XQSurfaceModel.h>
#include <core/XQSurfaceModelPayload.h>
#include <core/XQTriangleSurfaceGeometryHandle.h>
#include <core/command/XQCommandStack.h>
#include <services/modeling/ContourLoftInputBuilder.h>
#include <services/modeling/ModelingService.h>

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <map>
#include <memory>
#include <utility>
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

// A circular contour of `n` points (counter-clockwise in the XY plane) at height
// z, radius r, centered on the z-axis. pathArcLength is set to z so ordering by
// path position matches stacking order.
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

// Returns true when the triangle surface is a closed 2-manifold: every
// undirected edge is shared by exactly two triangles. Also reports V, E, F.
bool isClosedManifold(const xq::XQTriangleSurfaceGeometryHandle& g,
                      std::size_t* outV, std::size_t* outE, std::size_t* outF)
{
    std::map<std::pair<int, int>, int> edgeUse;
    const std::size_t triCount = g.triangleCount();
    for (std::size_t t = 0; t < triCount; ++t) {
        const auto& tri = g.triangle(t);
        for (int e = 0; e < 3; ++e) {
            int a = tri[e];
            int b = tri[(e + 1) % 3];
            if (a > b) {
                std::swap(a, b);
            }
            ++edgeUse[std::make_pair(a, b)];
        }
    }
    bool closed = true;
    for (const auto& kv : edgeUse) {
        if (kv.second != 2) {
            closed = false;
            break;
        }
    }
    if (outV != nullptr) {
        *outV = g.pointCount();
    }
    if (outE != nullptr) {
        *outE = edgeUse.size();
    }
    if (outF != nullptr) {
        *outF = triCount;
    }
    return closed;
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
    // ===================================================================
    // 1. XQTriangleSurfaceGeometryHandle: build + validity
    // ===================================================================
    {
        xq::XQTriangleSurfaceGeometryHandle g;
        CHECK(!g.is_valid()); // empty -> invalid
        const int i0 = g.addPoint({0.0, 0.0, 0.0});
        const int i1 = g.addPoint({1.0, 0.0, 0.0});
        const int i2 = g.addPoint({0.0, 1.0, 0.0});
        CHECK(i0 == 0);
        CHECK(i1 == 1);
        CHECK(i2 == 2);
        g.addTriangle(i0, i1, i2);
        CHECK(g.pointCount() == 3);
        CHECK(g.triangleCount() == 1);
        CHECK(g.is_valid());
        CHECK(g.triangle(0)[0] == 0);
        CHECK(g.triangle(0)[2] == 2);
    }
    {
        // out-of-range index -> invalid
        xq::XQTriangleSurfaceGeometryHandle g;
        g.addPoint({0.0, 0.0, 0.0});
        g.addPoint({1.0, 0.0, 0.0});
        g.addPoint({0.0, 1.0, 0.0});
        g.addTriangle(0, 1, 5); // 5 out of range
        CHECK(!g.is_valid());
    }
    {
        // degenerate (repeated index) -> invalid
        xq::XQTriangleSurfaceGeometryHandle g;
        g.addPoint({0.0, 0.0, 0.0});
        g.addPoint({1.0, 0.0, 0.0});
        g.addTriangle(0, 1, 1);
        CHECK(!g.is_valid());
    }

    // ===================================================================
    // 2. ContourLoftInputBuilder: order, uniform resample, validation
    // ===================================================================
    {
        // Three stacked circles, intentionally out of path order and with
        // different point counts, to exercise ordering + uniform resampling.
        xq::XQContourGroup group;
        group.setId(xq::NodeId(100));
        group.addContour(makeCircle(1, 12, 2.0, 0.0));
        group.addContour(makeCircle(3, 20, 2.0, 4.0)); // farthest
        group.addContour(makeCircle(2, 16, 2.0, 2.0)); // middle

        xq::ContourLoftInputBuilder::Options opt;
        opt.pointsPerContour = 16;
        const xq::ContourLoftInputBuilder::Result r =
            xq::ContourLoftInputBuilder::buildLoftInput(group, opt);
        CHECK(r.ok());
        CHECK(r.input.pointsPerContour == 16);
        CHECK(r.input.rings.size() == 3);
        // ordered by path position (z = 0, 2, 4)
        CHECK(r.input.rings[0].pathArcLength == 0.0);
        CHECK(r.input.rings[1].pathArcLength == 2.0);
        CHECK(r.input.rings[2].pathArcLength == 4.0);
        // every ring resampled to 16 points
        for (const auto& ring : r.input.rings) {
            CHECK(ring.points.size() == 16);
        }
        CHECK(r.input.sourceContourGroup == xq::NodeId(100));
    }
    {
        // fewer than 2 usable contours -> NotEnoughContours
        xq::XQContourGroup group;
        group.addContour(makeCircle(1, 12, 2.0, 0.0));
        const xq::ContourLoftInputBuilder::Result r =
            xq::ContourLoftInputBuilder::buildLoftInput(group, {});
        CHECK(r.status == xq::ContourLoftInputBuilder::Status::NotEnoughContours);
    }
    {
        // pointsPerContour < 3 -> InvalidSampleCount
        xq::XQContourGroup group;
        group.addContour(makeCircle(1, 12, 2.0, 0.0));
        group.addContour(makeCircle(2, 12, 2.0, 2.0));
        xq::ContourLoftInputBuilder::Options opt;
        opt.pointsPerContour = 2;
        const xq::ContourLoftInputBuilder::Result r =
            xq::ContourLoftInputBuilder::buildLoftInput(group, opt);
        CHECK(r.status == xq::ContourLoftInputBuilder::Status::InvalidSampleCount);
    }

    // ===================================================================
    // 3. loftSurface: triangle count is exactly 2*N*(M-1); open tube
    // ===================================================================
    const std::size_t N = 16; // points per ring
    const std::size_t M = 5;  // rings
    xq::XQContourLoftInput tubeInput;
    {
        xq::XQContourGroup group;
        group.setId(xq::NodeId(200));
        for (std::size_t i = 0; i < M; ++i) {
            group.addContour(makeCircle(static_cast<int>(i + 1), N, 2.0,
                                        static_cast<double>(i) * 2.0));
        }
        xq::ContourLoftInputBuilder::Options opt;
        opt.pointsPerContour = N;
        const xq::ContourLoftInputBuilder::Result built =
            xq::ContourLoftInputBuilder::buildLoftInput(group, opt);
        CHECK(built.ok());
        tubeInput = built.input;
    }

    std::shared_ptr<xq::XQSurfaceModel> tube;
    {
        const xq::ModelingService::Result r = xq::ModelingService::loftSurface(tubeInput);
        CHECK(r.ok());
        CHECK(r.model != nullptr);
        CHECK(r.model->hasTriangleGeometry());
        const auto& g = *r.model->triangleGeometry();
        CHECK(g.is_valid());
        // M rings * N points
        CHECK(g.pointCount() == N * M);
        // 2 triangles per quad, N quads per adjacent pair, (M-1) pairs
        CHECK(g.triangleCount() == 2 * N * (M - 1));
        // exactly one Wall face with the stable id
        CHECK(r.model->faces().size() == 1);
        CHECK(r.model->faces()[0].kind == xq::FaceKind::Wall);
        CHECK(r.model->faces()[0].faceId == xq::ModelingService::wallFaceId());
        CHECK(r.model->source() == xq::ModelSource::Generated);

        // every lofted (wall) triangle is tagged with the wall face id
        for (std::size_t t = 0; t < g.triangleCount(); ++t) {
            CHECK(g.triangleFaceId(t) == xq::ModelingService::wallFaceId());
        }
        CHECK(r.model->hasSourceContourGroupNode());
        CHECK(r.model->sourceContourGroupNode() == xq::NodeId(200));

        // open tube: NOT closed (the two end loops are open boundaries)
        std::size_t V = 0;
        std::size_t E = 0;
        std::size_t F = 0;
        const bool closed = isClosedManifold(g, &V, &E, &F);
        CHECK(!closed);
        tube = r.model;
    }

    // loft validation: a single ring -> NotEnoughRings
    {
        xq::XQContourLoftInput one;
        one.pointsPerContour = N;
        one.rings.push_back(tubeInput.rings[0]);
        const xq::ModelingService::Result r = xq::ModelingService::loftSurface(one);
        CHECK(r.status == xq::ModelingService::Status::NotEnoughRings);
        CHECK(r.model == nullptr);
    }

    // ===================================================================
    // 4. capModel: closes the tube; closed 2-manifold; Euler V-E+F = 2
    // ===================================================================
    {
        const xq::ModelingService::Result r =
            xq::ModelingService::capModel(*tube, xq::ModelingService::CapOptions{});
        CHECK(r.ok());
        CHECK(r.model != nullptr);
        const auto& g = *r.model->triangleGeometry();
        CHECK(g.is_valid());

        // two centroid points added (one per open end)
        CHECK(g.pointCount() == N * M + 2);
        // wall triangles + N triangles per cap (fan over N boundary edges) * 2 caps
        CHECK(g.triangleCount() == 2 * N * (M - 1) + 2 * N);

        // per-triangle face ids: the first 2*N*(M-1) are wall triangles (==1);
        // then the inlet fan (N tris, ==2) then the outlet fan (N tris, ==3).
        const std::size_t wallTriCount = 2 * N * (M - 1);
        for (std::size_t t = 0; t < wallTriCount; ++t) {
            CHECK(g.triangleFaceId(t) == xq::ModelingService::wallFaceId());
        }
        std::size_t inletFanCount = 0;
        std::size_t outletFanCount = 0;
        for (std::size_t t = wallTriCount; t < g.triangleCount(); ++t) {
            const int fid = g.triangleFaceId(t);
            CHECK(fid == 2 || fid == 3);
            if (fid == 2) {
                ++inletFanCount;
            } else {
                ++outletFanCount;
            }
        }
        CHECK(inletFanCount == N);
        CHECK(outletFanCount == N);

        std::size_t V = 0;
        std::size_t E = 0;
        std::size_t F = 0;
        const bool closed = isClosedManifold(g, &V, &E, &F);
        CHECK(closed); // every edge shared by exactly 2 triangles
        // Euler characteristic of a closed sphere-topology surface: V - E + F = 2
        const long euler = static_cast<long>(V) - static_cast<long>(E) + static_cast<long>(F);
        CHECK(euler == 2);

        // faces: wall + inlet + outlet, stable ids, distinct cap ids
        CHECK(r.model->faces().size() == 3);
        xq::ModelFace wall = {};
        xq::ModelFace inlet = {};
        xq::ModelFace outlet = {};
        const bool hasWall = r.model->faceById(xq::ModelingService::wallFaceId(), &wall);
        const bool hasInlet = r.model->faceById(2, &inlet);
        const bool hasOutlet = r.model->faceById(3, &outlet);
        CHECK(hasWall);
        CHECK(hasInlet);
        CHECK(hasOutlet);
        CHECK(wall.kind == xq::FaceKind::Wall);
        CHECK(inlet.kind == xq::FaceKind::Inlet);
        CHECK(outlet.kind == xq::FaceKind::Outlet);
        CHECK(inlet.capId.has_value());
        CHECK(outlet.capId.has_value());
        CHECK(*inlet.capId != *outlet.capId);
        // capping preserves the source contour group binding
        CHECK(r.model->hasSourceContourGroupNode());
        CHECK(r.model->sourceContourGroupNode() == xq::NodeId(200));
    }

    // capModel on an already-closed model -> AlreadyClosed
    {
        const xq::ModelingService::Result capped =
            xq::ModelingService::capModel(*tube, xq::ModelingService::CapOptions{});
        CHECK(capped.ok());
        const xq::ModelingService::Result again =
            xq::ModelingService::capModel(*capped.model, xq::ModelingService::CapOptions{});
        CHECK(again.status == xq::ModelingService::Status::AlreadyClosed);
        CHECK(again.model == nullptr);
    }

    // capModel on a model with no usable triangle geometry -> InvalidModel
    {
        xq::XQSurfaceModel empty;
        const xq::ModelingService::Result r =
            xq::ModelingService::capModel(empty, xq::ModelingService::CapOptions{});
        CHECK(r.status == xq::ModelingService::Status::InvalidModel);
        CHECK(r.model == nullptr);
    }

    // ===================================================================
    // 5. ModelFace id stability: re-lofting + re-capping yields identical ids
    // ===================================================================
    {
        const xq::ModelingService::Result loft1 = xq::ModelingService::loftSurface(tubeInput);
        const xq::ModelingService::Result loft2 = xq::ModelingService::loftSurface(tubeInput);
        CHECK(loft1.ok());
        CHECK(loft2.ok());
        const xq::ModelingService::Result cap1 =
            xq::ModelingService::capModel(*loft1.model, xq::ModelingService::CapOptions{});
        const xq::ModelingService::Result cap2 =
            xq::ModelingService::capModel(*loft2.model, xq::ModelingService::CapOptions{});
        CHECK(cap1.ok());
        CHECK(cap2.ok());
        CHECK(cap1.model->faces().size() == cap2.model->faces().size());
        for (std::size_t i = 0; i < cap1.model->faces().size(); ++i) {
            CHECK(cap1.model->faces()[i].faceId == cap2.model->faces()[i].faceId);
            CHECK(cap1.model->faces()[i].kind == cap2.model->faces()[i].kind);
        }
    }

    // ===================================================================
    // 6. XQSurfaceModelPayload: domain + deep-copy clone of triangle geometry
    // ===================================================================
    {
        xq::XQSurfaceModelPayload payload(*tube);
        CHECK(payload.domainType() == xq::XQDomainType::SurfaceModel);
        const std::shared_ptr<xq::XQPayload> cloned = payload.clone();
        CHECK(cloned != nullptr);
        CHECK(cloned->domainType() == xq::XQDomainType::SurfaceModel);
        auto* clonedModel = dynamic_cast<xq::XQSurfaceModelPayload*>(cloned.get());
        CHECK(clonedModel != nullptr);
        // deep copy: clone holds a different geometry handle instance
        CHECK(clonedModel->model().triangleGeometry().get()
              != payload.model().triangleGeometry().get());
        // same content
        CHECK(clonedModel->model().triangleGeometry()->pointCount()
              == payload.model().triangleGeometry()->pointCount());
    }

    // ===================================================================
    // 7. loftSurfaceCommand: into scene w/ source relation + undo/redo
    // ===================================================================
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        const xq::NodeId groupId(200);
        const xq::NodeId modelId(201);
        // contour-group node to be the source
        scene.insert(xq::XQDataNode(groupId, xq::XQDomainType::ContourGroup, "aorta-ctgr",
                                    std::shared_ptr<xq::XQPayload>()));

        xq::ModelingService::CommandResult cmd =
            xq::ModelingService::loftSurfaceCommand(&scene, modelId, "aorta-model", tubeInput);
        CHECK(cmd.ok());
        CHECK(cmd.command != nullptr);

        const std::size_t nodesBefore = node_count(scene);
        stack.push(std::move(cmd.command));
        CHECK(node_count(scene) == nodesBefore + 1);
        CHECK(relation_count(scene) == 1); // ContourGroupToModel relation

        const xq::XQDataNode* node = scene.find(modelId);
        CHECK(node != nullptr);
        CHECK(node->domainType() == xq::XQDomainType::SurfaceModel);
        const auto* payload =
            dynamic_cast<const xq::XQSurfaceModelPayload*>(node->payload().get());
        CHECK(payload != nullptr);
        CHECK(payload->model().hasTriangleGeometry());
        CHECK(payload->model().triangleGeometry()->pointCount() == N * M);
        CHECK(payload->model().sourceContourGroupNode() == groupId);

        // undo removes node + relation; redo restores both
        const bool undone = stack.undo();
        CHECK(undone);
        CHECK(node_count(scene) == nodesBefore);
        CHECK(relation_count(scene) == 0);
        CHECK(scene.find(modelId) == nullptr);

        const bool redone = stack.redo();
        CHECK(redone);
        CHECK(node_count(scene) == nodesBefore + 1);
        CHECK(relation_count(scene) == 1);
        CHECK(scene.find(modelId) != nullptr);
    }

    // loftSurfaceCommand: null scene rejected
    {
        xq::ModelingService::CommandResult cmd =
            xq::ModelingService::loftSurfaceCommand(nullptr, xq::NodeId(1), "m", tubeInput);
        CHECK(cmd.status == xq::ModelingService::Status::NullScene);
        CHECK(cmd.command == nullptr);
    }

    // createModelNodeCommand on a capped model (no source) -> plain AddNodeCommand
    {
        xq::XQScene scene;
        xq::XQCommandStack stack;
        xq::ModelingService::Result loft = xq::ModelingService::loftSurface(tubeInput);
        CHECK(loft.ok());
        // strip source by re-building a model without a source group
        xq::XQContourLoftInput noSource = tubeInput;
        noSource.sourceContourGroup = xq::NodeId::invalid();
        xq::ModelingService::Result loftNoSrc = xq::ModelingService::loftSurface(noSource);
        CHECK(loftNoSrc.ok());
        CHECK(!loftNoSrc.model->hasSourceContourGroupNode());

        xq::ModelingService::CommandResult cmd =
            xq::ModelingService::createModelNodeCommand(&scene, xq::NodeId(301), "m",
                                                        loftNoSrc.model);
        CHECK(cmd.ok());
        stack.push(std::move(cmd.command));
        CHECK(node_count(scene) == 1);
        CHECK(relation_count(scene) == 0);
    }

    return 0;
}
