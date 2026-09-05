// M9b-E 阶段2: reader lazyGeometry opt-in (io, no services/vtk).
// AC3 惰性不强制常驻: lazyGeometry==true 载入带几何 asset 后 payload
//   hasGeometryAssetId()==true 且几何 handle 未物化;且未对几何 blob 调
//   store.get —— 以"删掉几何 blob 文件后 lazy 仍成功、eager 失败"反证。
// AC5 resident 旧路径不退化: lazyGeometry==false(默认)与今天逐字节一致。
// AC6 向后兼容: 无 assetId / 无几何 blob 的工程不受影响。CHECK-macro style.

#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQMesh.h>
#include <core/XQMeshPayload.h>
#include <core/XQProject.h>
#include <core/XQScene.h>
#include <core/XQSurfaceModel.h>
#include <core/XQSurfaceModelPayload.h>
#include <core/XQTetVolumeMeshHandle.h>
#include <core/XQTriangleSurfaceGeometryHandle.h>
#include <io/project/XQProjectReader.h>
#include <io/project/XQProjectWriter.h>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

namespace {

int g_failures = 0;
void check(bool ok, const char* what)
{
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

const xq::NodeId kSurfNode(7201);
const xq::NodeId kMeshNode(7202);

std::filesystem::path temp_dir()
{
    return std::filesystem::temp_directory_path() / "xq_m9be_lazy_geometry";
}

void build_surface(xq::XQProject* project)
{
    auto handle = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>();
    handle->addPoint({0.0, 0.0, 0.0});
    handle->addPoint({1.0, 0.0, 0.0});
    handle->addPoint({0.0, 1.0, 0.0});
    handle->addPoint({0.0, 0.0, 1.0});
    handle->addTriangle(0, 1, 2, 7);
    handle->addTriangle(0, 1, 3, 7);
    handle->addTriangle(0, 2, 3, 8);

    xq::XQSurfaceModel model;
    model.setId(xq::NodeId(8001));
    model.setSource(xq::ModelSource::Generated);
    model.setTriangleGeometry(handle);
    xq::ModelFace f = {};
    f.faceId = 7;
    f.name = "wall";
    f.kind = xq::FaceKind::Wall;
    model.addFace(f);
    xq::ModelFace f2 = {};
    f2.faceId = 8;
    f2.name = "outlet";
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
    vol->addTet(0, 1, 2, 3);

    xq::XQMesh mesh;
    mesh.setId(xq::NodeId(8002));
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
    std::filesystem::remove_all(temp_dir());
    std::filesystem::create_directories(temp_dir());
    const std::filesystem::path path = temp_dir() / (std::string(stem) + ".xqproj");
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

// Deletes every blob file under <stem>.assets/blobs so that any geometry
// store.get would fail; lazy load must still succeed (it never reads them).
void delete_all_blobs(const char* stem)
{
    const std::filesystem::path blobs = temp_dir() / (std::string(stem) + ".assets") / "blobs";
    if (!std::filesystem::exists(blobs)) {
        return;
    }
    for (std::filesystem::recursive_directory_iterator it(blobs), end; it != end; ++it) {
        if (it->is_regular_file()) {
            std::filesystem::remove(it->path());
        }
    }
}

// AC5: default (eager) load materializes resident handles, byte-for-byte as today.
void test_eager_default_materializes()
{
    const std::string path = save_project("eager");
    if (path.empty()) {
        return;
    }
    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path, &result);
    check(status == xq::XQProjectReader::Status::Ok, "eager load ok");

    auto surf = payload_of<xq::XQSurfaceModelPayload>(result.project, kSurfNode);
    check(surf != nullptr, "eager surface payload restored");
    if (surf) {
        check(surf->model().hasTriangleGeometry(), "eager surface materialized geometry");
        check(!surf->hasGeometryAssetId(), "eager surface has no geometryAssetId");
        if (surf->model().hasTriangleGeometry()) {
            const auto& g = *surf->model().triangleGeometry();
            check(g.pointCount() == 4 && g.triangleCount() == 3, "eager surface counts");
        }
    }
    auto mesh = payload_of<xq::XQMeshPayload>(result.project, kMeshNode);
    check(mesh != nullptr, "eager mesh payload restored");
    if (mesh) {
        check(mesh->mesh().hasSurfaceTriangles() && mesh->mesh().hasVolumeTets(),
              "eager mesh materialized surf + vol");
        check(!mesh->hasGeometryAssetId(), "eager mesh has no geometryAssetId");
    }
}

// AC3: lazy load stamps geometryAssetId and does NOT materialize the handle.
void test_lazy_stamps_assetid_no_materialize()
{
    const std::string path = save_project("lazy");
    if (path.empty()) {
        return;
    }
    xq::XQProjectReadOptions opts;
    opts.lazyGeometry = true;
    xq::XQProjectReadResult result = {};
    const xq::XQProjectReader::Status status = xq::XQProjectReader::load(path, &result, opts);
    check(status == xq::XQProjectReader::Status::Ok, "lazy load ok");

    auto surf = payload_of<xq::XQSurfaceModelPayload>(result.project, kSurfNode);
    check(surf != nullptr, "lazy surface payload restored");
    if (surf) {
        check(surf->hasGeometryAssetId(), "lazy surface has geometryAssetId (AC3)");
        check(!surf->model().hasTriangleGeometry(),
              "lazy surface did NOT materialize geometry (AC3)");
        // metadata (faces) still parsed from the text block, not the blob.
        check(surf->model().faces().size() == 2, "lazy surface metadata still parsed");
    }
    auto mesh = payload_of<xq::XQMeshPayload>(result.project, kMeshNode);
    check(mesh != nullptr, "lazy mesh payload restored");
    if (mesh) {
        check(mesh->hasGeometryAssetId(), "lazy mesh has geometryAssetId (AC3)");
        check(!mesh->mesh().hasSurfaceTriangles() && !mesh->mesh().hasVolumeTets(),
              "lazy mesh did NOT materialize surf/vol (AC3)");
    }
}

// AC3 (decisive): with every blob file deleted, lazy load still succeeds (it
// never reads the geometry blobs) while eager load fails. Proves lazy does not
// call store.get on the geometry blobs.
void test_lazy_skips_blob_get()
{
    const std::string path = save_project("noblob");
    if (path.empty()) {
        return;
    }
    delete_all_blobs("noblob");

    // Eager: must fail because the geometry blob is gone.
    xq::XQProjectReadResult eager = {};
    const xq::XQProjectReader::Status eagerStatus = xq::XQProjectReader::load(path, &eager);
    check(eagerStatus != xq::XQProjectReader::Status::Ok,
          "eager load fails with blobs deleted (it reads them)");

    // Lazy: must succeed because it never touches the geometry blobs.
    xq::XQProjectReadOptions opts;
    opts.lazyGeometry = true;
    xq::XQProjectReadResult lazy = {};
    const xq::XQProjectReader::Status lazyStatus = xq::XQProjectReader::load(path, &lazy, opts);
    check(lazyStatus == xq::XQProjectReader::Status::Ok,
          "lazy load succeeds with blobs deleted (never reads them, AC3)");
    auto surf = payload_of<xq::XQSurfaceModelPayload>(lazy.project, kSurfNode);
    check(surf != nullptr && surf->hasGeometryAssetId(),
          "lazy-no-blob surface still stamped with assetId");
}

} // namespace

int main()
{
    test_eager_default_materializes();
    test_lazy_stamps_assetid_no_materialize();
    test_lazy_skips_blob_get();

    if (g_failures != 0) {
        std::fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    std::printf("all lazy-geometry reader checks passed\n");
    return 0;
}
