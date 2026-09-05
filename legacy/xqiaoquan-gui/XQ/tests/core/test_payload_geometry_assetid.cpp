// M9b-E 阶段1: payload geometryAssetId 字段 + clone 语义(core, no io/services/vtk).
// AC1 字段加不破:既有构造/model()/mesh()/domainType() 不变,默认 hasGeometryAssetId()==false。
// AC2 clone 三分支:持 handle 深拷不共享;仅 assetId 无 handle 只拷引用零几何分配;
//      二者都有以 handle 为准深拷 + 带 assetId。CHECK-macro style.

#include <core/XQMeshPayload.h>
#include <core/XQSurfaceModelPayload.h>
#include <core/asset/AssetId.h>

#include <cstdio>
#include <memory>

namespace {

int g_failures = 0;
void check(bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

std::shared_ptr<xq::XQTriangleSurfaceGeometryHandle> makeSurfaceHandle()
{
    auto h = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>();
    h->addPoint({0.0, 0.0, 0.0});
    h->addPoint({1.0, 0.0, 0.0});
    h->addPoint({0.0, 1.0, 0.0});
    h->addTriangle(0, 1, 2, 7);
    return h;
}

xq::XQSurfaceModel makeSurfaceModel(bool withGeometry)
{
    xq::XQSurfaceModel model;
    model.setId(xq::NodeId(6001));
    model.setSource(xq::ModelSource::Generated);
    if (withGeometry) {
        model.setTriangleGeometry(makeSurfaceHandle());
    }
    return model;
}

// AC1: field is additive; absent by default; existing accessors unchanged.
void test_field_additive_default_absent()
{
    xq::XQSurfaceModelPayload surf(makeSurfaceModel(true));
    check(surf.domainType() == xq::XQDomainType::SurfaceModel, "surface domainType unchanged");
    check(!surf.hasGeometryAssetId(), "surface hasGeometryAssetId() false by default");
    check(surf.model().hasTriangleGeometry(), "surface model() still exposes geometry");

    surf.setGeometryAssetId(xq::AssetId(42));
    check(surf.hasGeometryAssetId(), "surface hasGeometryAssetId() true after set");
    check(surf.geometryAssetId() == xq::AssetId(42), "surface geometryAssetId() round-trips value");

    xq::XQMesh emptyMesh;
    xq::XQMeshPayload mesh(std::move(emptyMesh));
    check(mesh.domainType() == xq::XQDomainType::Mesh, "mesh domainType unchanged");
    check(!mesh.hasGeometryAssetId(), "mesh hasGeometryAssetId() false by default");
    mesh.setGeometryAssetId(xq::AssetId(99));
    check(mesh.hasGeometryAssetId() && mesh.geometryAssetId() == xq::AssetId(99),
          "mesh geometryAssetId() round-trips");
}

// AC2 branch 1: payload with a resident handle clones a deep copy (not shared).
void test_clone_handle_deep_copy_not_shared()
{
    xq::XQSurfaceModelPayload surf(makeSurfaceModel(true));
    const auto* origGeom = surf.model().triangleGeometry().get();

    auto cloned = surf.clone();
    auto* clonedSurf = dynamic_cast<xq::XQSurfaceModelPayload*>(cloned.get());
    check(clonedSurf != nullptr, "clone is an XQSurfaceModelPayload");
    if (clonedSurf == nullptr) {
        return;
    }
    check(clonedSurf->model().hasTriangleGeometry(), "clone has its own geometry");
    const auto* clonedGeom = clonedSurf->model().triangleGeometry().get();
    check(clonedGeom != nullptr && clonedGeom != origGeom,
          "clone geometry is a distinct handle (deep copy, not shared)");
    check(!clonedSurf->hasGeometryAssetId(), "handle-only clone carries no assetId");
}

// AC2 branch 2: payload with only an assetId (no handle) clones the assetId
// reference and allocates no geometry.
void test_clone_assetid_only_no_geometry_alloc()
{
    xq::XQSurfaceModelPayload surf(makeSurfaceModel(false)); // no geometry
    surf.setGeometryAssetId(xq::AssetId(7));
    check(!surf.model().hasTriangleGeometry(), "assetId-only payload has no resident geometry");

    auto cloned = surf.clone();
    auto* clonedSurf = dynamic_cast<xq::XQSurfaceModelPayload*>(cloned.get());
    check(clonedSurf != nullptr, "assetId-only clone is an XQSurfaceModelPayload");
    if (clonedSurf == nullptr) {
        return;
    }
    check(!clonedSurf->model().hasTriangleGeometry(),
          "assetId-only clone allocates no geometry");
    check(clonedSurf->hasGeometryAssetId()
              && clonedSurf->geometryAssetId() == xq::AssetId(7),
          "assetId-only clone carries the assetId reference forward");
}

// AC2 branch 3: payload with both a handle and an assetId clones a deep handle
// copy AND carries the assetId forward.
void test_clone_handle_and_assetid_both()
{
    xq::XQSurfaceModelPayload surf(makeSurfaceModel(true));
    surf.setGeometryAssetId(xq::AssetId(13));
    const auto* origGeom = surf.model().triangleGeometry().get();

    auto cloned = surf.clone();
    auto* clonedSurf = dynamic_cast<xq::XQSurfaceModelPayload*>(cloned.get());
    check(clonedSurf != nullptr, "both-path clone is an XQSurfaceModelPayload");
    if (clonedSurf == nullptr) {
        return;
    }
    const auto* clonedGeom = clonedSurf->model().triangleGeometry().get();
    check(clonedGeom != nullptr && clonedGeom != origGeom,
          "both-path clone deep-copies the handle");
    check(clonedSurf->hasGeometryAssetId()
              && clonedSurf->geometryAssetId() == xq::AssetId(13),
          "both-path clone carries the assetId forward");
}

// AC2 (mesh): assetId carried forward through mesh clone alongside handle deep copy.
void test_mesh_clone_carries_assetid()
{
    auto surf = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>();
    surf->addPoint({0.0, 0.0, 0.0});
    surf->addPoint({1.0, 0.0, 0.0});
    surf->addPoint({0.0, 1.0, 0.0});
    surf->addTriangle(0, 1, 2, 5);
    xq::XQMesh mesh;
    mesh.setSurfaceTriangles(surf);

    xq::XQMeshPayload payload(std::move(mesh));
    payload.setGeometryAssetId(xq::AssetId(21));
    const auto* origSurf = payload.mesh().surfaceTriangles().get();

    auto cloned = payload.clone();
    auto* clonedMesh = dynamic_cast<xq::XQMeshPayload*>(cloned.get());
    check(clonedMesh != nullptr, "mesh clone is an XQMeshPayload");
    if (clonedMesh == nullptr) {
        return;
    }
    const auto* clonedSurf = clonedMesh->mesh().surfaceTriangles().get();
    check(clonedSurf != nullptr && clonedSurf != origSurf,
          "mesh clone deep-copies surface handle");
    check(clonedMesh->hasGeometryAssetId()
              && clonedMesh->geometryAssetId() == xq::AssetId(21),
          "mesh clone carries the assetId forward");
}

} // namespace

int main()
{
    test_field_additive_default_absent();
    test_clone_handle_deep_copy_not_shared();
    test_clone_assetid_only_no_geometry_alloc();
    test_clone_handle_and_assetid_both();
    test_mesh_clone_carries_assetid();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all payload geometryAssetId checks passed\n");
    return 0;
}
