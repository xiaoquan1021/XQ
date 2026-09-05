// M9b-E phase 3: services lazy geometry resolver equivalence checks (AC4/AC8).
// The same project is loaded two ways: eager mode materializes resident handles,
// while lazy mode stamps asset ids and resolves MappedGeometrySource through
// GeometryResourceManager. points/tris/faceId/tets must match element-for-element.
// Side-effecting acquire_* calls happen before assertions (no-sideeffect-in-assert).

#include "core/XQMesh.h"
#include "core/XQMeshPayload.h"
#include "core/XQProject.h"
#include "core/XQScene.h"
#include "core/XQSurfaceModel.h"
#include "core/XQSurfaceModelPayload.h"
#include "core/XQTetVolumeMeshHandle.h"
#include "core/XQTriangleSurfaceGeometryHandle.h"
#include "core/asset/AssetRegistry.h"
#include "core/source/IGeometrySource.h"
#include "core/source/ResidentSurfaceSource.h"
#include "core/source/ResidentTetSource.h"
#include "io/source/MappedGeometrySource.h"
#include "io/project/XQProjectReader.h"
#include "io/project/XQProjectWriter.h"
#include "services/resource/GeometryResourceManager.h"
#include "services/resource/GeometrySourceResolver.h"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;
void check(bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

const xq::NodeId kSurfNode(7301);
const xq::NodeId kMeshNode(7302);

fs::path temp_dir()
{
    return fs::temp_directory_path() / "xq_m9be_resolver";
}

void build_surface(xq::XQProject* project)
{
    auto handle = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>();
    for (int i = 0; i < 6; ++i) {
        handle->addPoint({static_cast<double>(i), static_cast<double>(i * 2),
                          static_cast<double>(i * 3) + 0.5});
    }
    handle->addTriangle(0, 1, 2, 11);
    handle->addTriangle(1, 2, 3, 11);
    handle->addTriangle(2, 3, 4, 22);
    handle->addTriangle(3, 4, 5, 22);

    xq::XQSurfaceModel model;
    model.setId(xq::NodeId(8101));
    model.setSource(xq::ModelSource::Generated);
    model.setTriangleGeometry(handle);
    xq::ModelFace f = {};
    f.faceId = 11;
    f.kind = xq::FaceKind::Wall;
    model.addFace(f);
    xq::ModelFace f2 = {};
    f2.faceId = 22;
    f2.kind = xq::FaceKind::Outlet;
    model.addFace(f2);

    project->scene().insert(xq::XQDataNode(
        kSurfNode, xq::XQDomainType::SurfaceModel, "Surface",
        std::make_shared<xq::XQSurfaceModelPayload>(std::move(model))));
}

void build_mesh(xq::XQProject* project)
{
    auto surf = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>();
    surf->addPoint({0.0, 0.0, 0.0});
    surf->addPoint({1.0, 0.0, 0.0});
    surf->addPoint({0.0, 1.0, 0.0});
    surf->addTriangle(0, 1, 2, 5);

    auto vol = std::make_shared<xq::XQTetVolumeMeshHandle>();
    vol->addPoint({0.0, 0.0, 0.0});
    vol->addPoint({1.0, 0.0, 0.0});
    vol->addPoint({0.0, 1.0, 0.0});
    vol->addPoint({0.0, 0.0, 1.0});
    vol->addPoint({0.25, 0.25, 0.25});
    vol->addTet(0, 1, 2, 4);
    vol->addTet(1, 2, 3, 4);

    xq::XQMesh mesh;
    mesh.setId(xq::NodeId(8102));
    mesh.setSurfaceTriangles(surf);
    mesh.setVolumeTets(vol);

    project->scene().insert(xq::XQDataNode(
        kMeshNode, xq::XQDomainType::Mesh, "Mesh",
        std::make_shared<xq::XQMeshPayload>(std::move(mesh))));
}

template <typename PayloadT>
std::shared_ptr<PayloadT> payload_of(const xq::XQProject& project, const xq::NodeId& id)
{
    const xq::XQDataNode* node = project.scene().find(id);
    if (node == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<PayloadT>(node->payload());
}

std::string save_project(const char* stem)
{
    fs::remove_all(temp_dir());
    fs::create_directories(temp_dir());
    const fs::path path = temp_dir() / (std::string(stem) + ".xqproj");
    xq::XQProject project;
    project.open();
    build_surface(&project);
    build_mesh(&project);
    if (xq::XQProjectWriter::save(project, path.string()) != xq::XQProjectWriter::Status::Ok) {
        check(false, "save project");
        return std::string();
    }
    return path.string();
}

fs::path assets_dir(const char* stem)
{
    return temp_dir() / (std::string(stem) + ".assets");
}

int remove_merkle_sidecars(const fs::path& root)
{
    int removed = 0;
    if (!fs::exists(root)) {
        return removed;
    }
    for (fs::recursive_directory_iterator it(root), end; it != end; ++it) {
        if (it->is_regular_file() && it->path().extension() == ".merkle") {
            std::error_code ec;
            fs::remove(it->path(), ec);
            if (!ec) {
                ++removed;
            }
        }
    }
    return removed;
}

// Compares selected aspects of two geometry sources element-for-element.
// Side-effecting acquire_* calls are made first, then the spans are asserted
// (no-sideeffect-in-assert). A combined surf+vol MappedGeometrySource exposes
// SURFACE points via acquire_points (hasSurface_ ? points_), so points/tris are
// compared against the eager surface handle and tets against the eager tet
// handle -- never points across a surface vs a tet source.
void comparePointsTris(const xq::IGeometrySource& a, const xq::IGeometrySource& b,
                       const char* tag)
{
    xq::GeometryLease<xq::Point3> pa = a.acquire_points();
    xq::GeometryLease<xq::Point3> pb = b.acquire_points();
    const xq::ReadSpan<xq::Point3>& sa = pa.span();
    const xq::ReadSpan<xq::Point3>& sb = pb.span();
    bool pointsEqual = sa.size() == sb.size();
    for (std::size_t i = 0; pointsEqual && i < sa.size(); ++i) {
        if (sa[i].x != sb[i].x || sa[i].y != sb[i].y || sa[i].z != sb[i].z) {
            pointsEqual = false;
        }
    }
    check(pointsEqual, tag);

    xq::TriangleLease ta = a.acquire_triangles();
    xq::TriangleLease tb = b.acquire_triangles();
    const xq::TriangleView& va = ta.view();
    const xq::TriangleView& vb = tb.view();
    bool trisEqual = va.triangles.size() == vb.triangles.size()
        && va.faceIds.size() == vb.faceIds.size();
    for (std::size_t i = 0; trisEqual && i < va.triangles.size(); ++i) {
        if (va.triangles[i] != vb.triangles[i] || va.faceIds[i] != vb.faceIds[i]) {
            trisEqual = false;
        }
    }
    check(trisEqual, tag);
}

void compareTets(const xq::IGeometrySource& a, const xq::IGeometrySource& b, const char* tag)
{
    xq::GeometryLease<xq::SourceTet> xa = a.acquire_tetrahedra();
    xq::GeometryLease<xq::SourceTet> xb = b.acquire_tetrahedra();
    const xq::ReadSpan<xq::SourceTet>& za = xa.span();
    const xq::ReadSpan<xq::SourceTet>& zb = xb.span();
    bool tetsEqual = za.size() == zb.size();
    for (std::size_t i = 0; tetsEqual && i < za.size(); ++i) {
        if (za[i] != zb[i]) {
            tetsEqual = false;
        }
    }
    check(tetsEqual, tag);
}

// AC4: lazy-resolved mapped source == eager resident handle, element for element.
void test_lazy_resolver_equivalent_to_eager()
{
    const std::string path = save_project("equiv");
    if (path.empty()) {
        return;
    }

    // Eager: resident handles.
    xq::XQProjectReadResult eager = {};
    check(xq::XQProjectReader::load(path, &eager) == xq::XQProjectReader::Status::Ok,
          "eager load ok");
    auto eagerSurf = payload_of<xq::XQSurfaceModelPayload>(eager.project, kSurfNode);
    auto eagerMesh = payload_of<xq::XQMeshPayload>(eager.project, kMeshNode);
    check(eagerSurf && eagerSurf->model().hasTriangleGeometry(), "eager surface handle");
    check(eagerMesh && eagerMesh->mesh().hasVolumeTets(), "eager mesh handles");
    if (!eagerSurf || !eagerMesh) {
        return;
    }

    // Lazy: stamped assetId; resolve through the manager.
    xq::XQProjectReadOptions opts;
    opts.lazyGeometry = true;
    xq::XQProjectReadResult lazy = {};
    check(xq::XQProjectReader::load(path, &lazy, opts) == xq::XQProjectReader::Status::Ok,
          "lazy load ok");
    auto lazySurf = payload_of<xq::XQSurfaceModelPayload>(lazy.project, kSurfNode);
    auto lazyMesh = payload_of<xq::XQMeshPayload>(lazy.project, kMeshNode);
    check(lazySurf && lazySurf->hasGeometryAssetId(), "lazy surface stamped");
    check(lazyMesh && lazyMesh->hasGeometryAssetId(), "lazy mesh stamped");
    if (!lazySurf || !lazyMesh) {
        return;
    }

    xq::GeometryResourceManager mgr(&lazy.project.assetRegistry(),
                                    assets_dir("equiv").string());

    // Surface: eager handle source vs lazy mapped source (points + tris).
    {
        xq::ResidentSurfaceSource eagerSrc(eagerSurf->model().triangleGeometry());
        xq::GeometryResourceManager::GeometrySourceHandle lazyH =
            xq::resolveLazyGeometrySource(*lazySurf, mgr, lazy.project.assetRegistry());
        check(lazyH.valid(), "surface lazy resolve valid");
        if (lazyH.valid()) {
            comparePointsTris(eagerSrc, lazyH.source(), "surface eager==lazy (AC4)");
        }
    }

    // Mesh: the asset carries both surf + vol. The mapped source exposes surface
    // points/tris via acquire_points/acquire_triangles and tets via
    // acquire_tetrahedra; compare each aspect against the matching eager handle.
    {
        xq::ResidentSurfaceSource eagerSurfSrc(eagerMesh->mesh().surfaceTriangles());
        xq::ResidentTetSource eagerTetSrc(eagerMesh->mesh().volumeTets());
        xq::GeometryResourceManager::GeometrySourceHandle lazyH =
            xq::resolveLazyGeometrySource(*lazyMesh, mgr, lazy.project.assetRegistry());
        check(lazyH.valid(), "mesh lazy resolve valid");
        if (lazyH.valid()) {
            comparePointsTris(eagerSurfSrc, lazyH.source(), "mesh surf eager==lazy (AC4)");
            compareTets(eagerTetSrc, lazyH.source(), "mesh tet eager==lazy (AC4)");
        }
    }
}

// A payload with no geometryAssetId resolves to an invalid handle (caller falls
// back to the resident path). Guards the "coexists, no surprise" contract.
void test_no_assetid_resolves_invalid()
{
    const std::string path = save_project("noid");
    if (path.empty()) {
        return;
    }
    xq::XQProjectReadResult eager = {};
    check(xq::XQProjectReader::load(path, &eager) == xq::XQProjectReader::Status::Ok,
          "noid eager load ok");
    auto surf = payload_of<xq::XQSurfaceModelPayload>(eager.project, kSurfNode);
    check(surf && !surf->hasGeometryAssetId(), "eager payload has no assetId");
    if (!surf) {
        return;
    }
    xq::GeometryResourceManager mgr(&eager.project.assetRegistry(),
                                    assets_dir("noid").string());
    xq::GeometryResourceManager::GeometrySourceHandle h =
        xq::resolveLazyGeometrySource(*surf, mgr, eager.project.assetRegistry());
    check(!h.valid(), "no-assetId payload resolves to invalid handle");
}

void test_writer_sidecars_drive_segmented_resolver()
{
    const std::string path = save_project("segmented");
    if (path.empty()) {
        return;
    }

    xq::XQProjectReadOptions opts;
    opts.lazyGeometry = true;
    xq::XQProjectReadResult lazy = {};
    check(xq::XQProjectReader::load(path, &lazy, opts) == xq::XQProjectReader::Status::Ok,
          "segmented lazy load ok");
    auto lazySurf = payload_of<xq::XQSurfaceModelPayload>(lazy.project, kSurfNode);
    check(lazySurf && lazySurf->hasGeometryAssetId(), "segmented lazy surface stamped");
    if (!lazySurf) {
        return;
    }

    xq::GeometryResourceManager mgr(&lazy.project.assetRegistry(),
                                    assets_dir("segmented").string());
    xq::GeometryResourceManager::GeometrySourceHandle h =
        xq::resolveLazyGeometrySource(*lazySurf, mgr, lazy.project.assetRegistry(),
                                      xq::LazyGeometrySourceMode::SurfaceOnly);
    check(h.valid(), "segmented sidecar resolve valid");
    if (!h.valid()) {
        return;
    }

    const xq::MappedGeometrySource* mapped =
        dynamic_cast<const xq::MappedGeometrySource*>(&h.source());
    check(mapped != nullptr, "segmented resolver returns mapped geometry source");
    if (mapped == nullptr) {
        return;
    }

    xq::GeometryLease<xq::Point3> points = h.source().acquire_points();
    xq::TriangleLease tris = h.source().acquire_triangles();
    check(!points.span().empty(), "segmented resolver points acquire valid");
    check(!tris.view().triangles.empty(), "segmented resolver triangles acquire valid");

    const xq::MappedGeometrySource::IntegrityStats stats = mapped->stats();
    check(stats.segmentsChecked > 0,
          "resolver uses SegmentedMerkle when writer-produced sidecars exist");
}

void test_missing_sidecars_fall_back_to_full_verify()
{
    const std::string path = save_project("fullfallback");
    if (path.empty()) {
        return;
    }
    check(remove_merkle_sidecars(assets_dir("fullfallback")) > 0,
          "fallback fixture removed writer sidecars");

    xq::XQProjectReadOptions opts;
    opts.lazyGeometry = true;
    xq::XQProjectReadResult lazy = {};
    check(xq::XQProjectReader::load(path, &lazy, opts) == xq::XQProjectReader::Status::Ok,
          "fallback lazy load ok");
    auto lazySurf = payload_of<xq::XQSurfaceModelPayload>(lazy.project, kSurfNode);
    check(lazySurf && lazySurf->hasGeometryAssetId(), "fallback lazy surface stamped");
    if (!lazySurf) {
        return;
    }

    xq::GeometryResourceManager mgr(&lazy.project.assetRegistry(),
                                    assets_dir("fullfallback").string());
    xq::GeometryResourceManager::GeometrySourceHandle h =
        xq::resolveLazyGeometrySource(*lazySurf, mgr, lazy.project.assetRegistry(),
                                      xq::LazyGeometrySourceMode::SurfaceOnly);
    check(h.valid(), "missing sidecars fall back to FullVerify");
    if (!h.valid()) {
        return;
    }

    const xq::MappedGeometrySource* mapped =
        dynamic_cast<const xq::MappedGeometrySource*>(&h.source());
    check(mapped != nullptr, "fallback resolver returns mapped geometry source");
    if (mapped == nullptr) {
        return;
    }

    xq::GeometryLease<xq::Point3> points = h.source().acquire_points();
    xq::TriangleLease tris = h.source().acquire_triangles();
    check(!points.span().empty(), "fallback points acquire valid");
    check(!tris.view().triangles.empty(), "fallback triangles acquire valid");

    const xq::MappedGeometrySource::IntegrityStats stats = mapped->stats();
    check(stats.segmentsChecked == 0, "fallback does not check merkle segments");
    check(stats.bytesHashed > 0, "fallback verifies whole blobs by sha256 anchor");
}

// TetOnly mode resolves the mesh asset to a tet-bearing mapped source, and its
// tets match the eager resident tet handle element for element. Complements the
// SurfaceOnly path already covered above so all three modes have a resolve test.
void test_tet_only_mode_resolves_tets()
{
    const std::string path = save_project("tetonly");
    if (path.empty()) {
        return;
    }

    xq::XQProjectReadResult eager = {};
    check(xq::XQProjectReader::load(path, &eager) == xq::XQProjectReader::Status::Ok,
          "tetonly eager load ok");
    auto eagerMesh = payload_of<xq::XQMeshPayload>(eager.project, kMeshNode);
    check(eagerMesh && eagerMesh->mesh().hasVolumeTets(), "tetonly eager mesh handle");
    if (!eagerMesh) {
        return;
    }

    xq::XQProjectReadOptions opts;
    opts.lazyGeometry = true;
    xq::XQProjectReadResult lazy = {};
    check(xq::XQProjectReader::load(path, &lazy, opts) == xq::XQProjectReader::Status::Ok,
          "tetonly lazy load ok");
    auto lazyMesh = payload_of<xq::XQMeshPayload>(lazy.project, kMeshNode);
    check(lazyMesh && lazyMesh->hasGeometryAssetId(), "tetonly lazy mesh stamped");
    if (!lazyMesh) {
        return;
    }

    xq::GeometryResourceManager mgr(&lazy.project.assetRegistry(),
                                    assets_dir("tetonly").string());
    xq::GeometryResourceManager::GeometrySourceHandle lazyH =
        xq::resolveLazyGeometrySource(*lazyMesh, mgr, lazy.project.assetRegistry(),
                                      xq::LazyGeometrySourceMode::TetOnly);
    check(lazyH.valid(), "tet-only lazy resolve valid");
    if (!lazyH.valid()) {
        return;
    }
    xq::ResidentTetSource eagerTetSrc(eagerMesh->mesh().volumeTets());
    compareTets(eagerTetSrc, lazyH.source(), "tet-only eager==lazy tets");
}

// A payload that carries a geometryAssetId which the registry does not know
// resolves to an invalid handle without crashing (registry.find == nullptr
// branch). Built in-memory, no project on disk.
void test_unknown_assetid_resolves_invalid()
{
    xq::AssetRegistry registry;
    xq::XQSurfaceModelPayload payload{xq::XQSurfaceModel{}};
    payload.setGeometryAssetId(xq::AssetId(999999));
    check(payload.hasGeometryAssetId(), "payload carries a geometry assetId");

    xq::GeometryResourceManager mgr(&registry, assets_dir("unknownid").string());
    xq::GeometryResourceManager::GeometrySourceHandle h =
        xq::resolveLazyGeometrySource(payload, mgr, registry);
    check(!h.valid(), "unknown assetId resolves to invalid handle without crash");
}

} // namespace

int main()
{
    test_lazy_resolver_equivalent_to_eager();
    test_no_assetid_resolves_invalid();
    test_tet_only_mode_resolves_tets();
    test_unknown_assetid_resolves_invalid();
    test_writer_sidecars_drive_segmented_resolver();
    test_missing_sidecars_fall_back_to_full_verify();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all geometry source resolver checks passed\n");
    return 0;
}
