#include <app/XQAppStartup.h>
#include <app/XQMainWindow.h>

#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQMesh.h>
#include <core/XQMeshPayload.h>
#include <core/XQProject.h>
#include <core/XQScene.h>
#include <core/XQSurfaceModel.h>
#include <core/XQSurfaceModelPayload.h>
#include <core/XQTetVolumeMeshHandle.h>
#include <core/XQTriangleSurfaceGeometryHandle.h>
#include <io/project/XQProjectWriter.h>
#include <ui/XQSceneModel.h>
#include <ui/controllers/PathController.h>

#include <QAbstractItemModel>
#include <QApplication>
#include <QItemSelectionModel>
#include <QStackedWidget>
#include <QTreeView>
#include <QWidget>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

namespace {

int fail(const char* message, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", message, line);
    return 1;
}

#define CHECK(cond)                       \
    do {                                  \
        if (!(cond)) {                    \
            return fail(#cond, __LINE__); \
        }                                 \
    } while (0)

std::size_t count_nodes(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes([&count](const xq::XQDataNode&) {
        ++count;
    });
    return count;
}

std::filesystem::path temp_project_path(const char* stem)
{
    return std::filesystem::temp_directory_path() / (std::string(stem) + ".xqproj");
}

std::filesystem::path asset_dir_for_project_path(const std::filesystem::path& path)
{
    return path.parent_path() / (path.stem().string() + ".assets");
}

struct TempProjectFiles {
    explicit TempProjectFiles(const char* stem)
        : path(temp_project_path(stem))
        , assets(asset_dir_for_project_path(path))
    {
        std::filesystem::remove(path);
        std::filesystem::remove_all(assets);
    }

    ~TempProjectFiles()
    {
        std::filesystem::remove(path);
        std::filesystem::remove_all(assets);
    }

    std::filesystem::path path;
    std::filesystem::path assets;
};

std::shared_ptr<xq::XQSurfaceModelPayload> surface_payload(const xq::XQProject& project,
                                                           const xq::NodeId& id)
{
    const xq::XQDataNode* node = project.scene().find(id);
    if (node == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<xq::XQSurfaceModelPayload>(node->payload());
}

std::shared_ptr<xq::XQMeshPayload> mesh_payload(const xq::XQProject& project,
                                                const xq::NodeId& id)
{
    const xq::XQDataNode* node = project.scene().find(id);
    if (node == nullptr) {
        return nullptr;
    }
    return std::dynamic_pointer_cast<xq::XQMeshPayload>(node->payload());
}

void add_lazy_geometry_fixture_nodes(xq::XQProject* project,
                                     const xq::NodeId& surfaceId,
                                     const xq::NodeId& meshId)
{
    auto surfaceGeometry = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>();
    surfaceGeometry->addPoint({0.0, 0.0, 0.0});
    surfaceGeometry->addPoint({1.0, 0.0, 0.0});
    surfaceGeometry->addPoint({0.0, 1.0, 0.0});
    surfaceGeometry->addPoint({0.0, 0.0, 1.0});
    surfaceGeometry->addTriangle(0, 1, 2, 10);
    surfaceGeometry->addTriangle(0, 2, 3, 20);

    xq::XQSurfaceModel model;
    model.setId(surfaceId);
    model.setTriangleGeometry(surfaceGeometry);
    project->scene().insert(xq::XQDataNode(
        surfaceId, xq::XQDomainType::SurfaceModel, "Lazy Surface",
        std::make_shared<xq::XQSurfaceModelPayload>(std::move(model))));

    auto meshSurface = std::make_shared<xq::XQTriangleSurfaceGeometryHandle>();
    meshSurface->addPoint({0.0, 0.0, 0.0});
    meshSurface->addPoint({1.0, 0.0, 0.0});
    meshSurface->addPoint({0.0, 1.0, 0.0});
    meshSurface->addTriangle(0, 1, 2, 30);

    auto meshVolume = std::make_shared<xq::XQTetVolumeMeshHandle>();
    meshVolume->addPoint({0.0, 0.0, 0.0});
    meshVolume->addPoint({1.0, 0.0, 0.0});
    meshVolume->addPoint({0.0, 1.0, 0.0});
    meshVolume->addPoint({0.0, 0.0, 1.0});
    meshVolume->addTet(0, 1, 2, 3);

    xq::XQMesh mesh;
    mesh.setId(meshId);
    mesh.setSurfaceTriangles(meshSurface);
    mesh.setVolumeTets(meshVolume);
    project->scene().insert(xq::XQDataNode(
        meshId, xq::XQDomainType::Mesh, "Lazy Mesh",
        std::make_shared<xq::XQMeshPayload>(std::move(mesh))));
}

QModelIndex find_display_name(QAbstractItemModel* model, const char* displayName)
{
    if (model == nullptr) {
        return QModelIndex();
    }
    for (int row = 0; row < model->rowCount(QModelIndex()); ++row) {
        const QModelIndex index = model->index(row, xq::XQSceneModel::DisplayNameColumn);
        if (index.data(Qt::DisplayRole).toString().toStdString() == displayName) {
            return index;
        }
    }
    return QModelIndex();
}

int test_empty_startup_attaches_workflow()
{
    xq::XQAppStartupState state;
    xq::XQAppStartupConfig config;
    const xq::XQAppStartupStatus status = xq::initializeAppStartup(config, &state);

    CHECK(status == xq::XQAppStartupStatus::Ok);
    CHECK(state.project.state() == xq::XQProject::LifecycleState::Open);
    CHECK(count_nodes(state.project.scene()) == 0);
    CHECK(state.project.scene().find(xq::NodeId(1)) == nullptr);

    xq::XQMainWindow window;
    xq::attachMainWindowWorkflow(&window, &state);
    CHECK(window.pathController() != nullptr);
    CHECK(window.findChild<QStackedWidget*>("xqStagePanel") != nullptr);

    return 0;
}

int test_native_project_startup_loads_real_scene()
{
    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    CHECK(project.scene().insert(xq::XQDataNode(xq::NodeId(101), "image.volume", "Real Volume"))
          == xq::XQScene::InsertResult::Inserted);
    CHECK(project.scene().insert(xq::XQDataNode(xq::NodeId(102), "vascular.path", "Real Path"))
          == xq::XQScene::InsertResult::Inserted);
    CHECK(project.scene().link_derived(xq::NodeId(101), xq::NodeId(102))
          == xq::XQScene::RelationResult::Linked);

    TempProjectFiles files("xq_app_startup_real_scene");
    CHECK(xq::XQProjectWriter::save(project, files.path.string())
          == xq::XQProjectWriter::Status::Ok);

    xq::XQAppStartupConfig config;
    config.projectPath = files.path.string();
    xq::XQAppStartupState state;
    const xq::XQAppStartupStatus status = xq::initializeAppStartup(config, &state);

    CHECK(status == xq::XQAppStartupStatus::Ok);
    CHECK(state.project.state() == xq::XQProject::LifecycleState::Open);
    CHECK(count_nodes(state.project.scene()) == 2);
    CHECK(state.project.scene().find(xq::NodeId(101)) != nullptr);
    CHECK(state.project.scene().find(xq::NodeId(102)) != nullptr);
    CHECK(state.project.scene().find(xq::NodeId(1)) == nullptr);

    xq::XQMainWindow window;
    xq::attachMainWindowWorkflow(&window, &state);
    CHECK(window.pathController() != nullptr);

    return 0;
}

int test_lazy_project_startup_renders_lazy_geometry_selection()
{
    const xq::NodeId surfaceId(301);
    const xq::NodeId meshId(302);

    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    add_lazy_geometry_fixture_nodes(&project, surfaceId, meshId);

    TempProjectFiles files("xq_app_startup_lazy_geometry");
    CHECK(xq::XQProjectWriter::save(project, files.path.string())
          == xq::XQProjectWriter::Status::Ok);

    xq::XQAppStartupConfig config;
    config.projectPath = files.path.string();
    xq::XQAppStartupState state;
    const xq::XQAppStartupStatus status = xq::initializeAppStartup(config, &state);

    CHECK(status == xq::XQAppStartupStatus::Ok);
    CHECK(state.assetRootDir == files.assets.string());

    std::shared_ptr<xq::XQSurfaceModelPayload> surface =
        surface_payload(state.project, surfaceId);
    std::shared_ptr<xq::XQMeshPayload> mesh = mesh_payload(state.project, meshId);
    CHECK(surface != nullptr);
    CHECK(mesh != nullptr);
    CHECK(surface->hasGeometryAssetId());
    CHECK(!surface->model().hasTriangleGeometry());
    CHECK(mesh->hasGeometryAssetId());
    CHECK(!mesh->mesh().hasVolumeTets());
    CHECK(!mesh->mesh().hasSurfaceTriangles());

    xq::XQMainWindow window;
    xq::attachMainWindowWorkflow(&window, &state);

    QStackedWidget* stack = window.findChild<QStackedWidget*>("xqCentralStack");
    QWidget* imageLabel = window.findChild<QWidget*>("xqImageLabel");
    QWidget* renderWidget = window.findChild<QWidget*>("xqRenderWidget");
    QTreeView* tree = window.findChild<QTreeView*>("xqSceneTreeView");
    CHECK(stack != nullptr);
    CHECK(imageLabel != nullptr);
    CHECK(renderWidget != nullptr);
    CHECK(tree != nullptr);
    CHECK(tree->selectionModel() != nullptr);

    QModelIndex surfaceIndex = find_display_name(tree->model(), "Lazy Surface");
    CHECK(surfaceIndex.isValid());
    stack->setCurrentWidget(imageLabel);
    tree->selectionModel()->setCurrentIndex(
        surfaceIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QApplication::processEvents();
    CHECK(stack->currentWidget() == renderWidget);

    QModelIndex meshIndex = find_display_name(tree->model(), "Lazy Mesh");
    CHECK(meshIndex.isValid());
    stack->setCurrentWidget(imageLabel);
    tree->selectionModel()->setCurrentIndex(
        meshIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QApplication::processEvents();
    CHECK(stack->currentWidget() == renderWidget);

    return 0;
}

int test_invalid_project_path_fails_closed()
{
    xq::XQAppStartupConfig config;
    config.projectPath = temp_project_path("xq_app_startup_missing").string();

    xq::XQAppStartupState state;
    const xq::XQAppStartupStatus status = xq::initializeAppStartup(config, &state);

    CHECK(status == xq::XQAppStartupStatus::ProjectLoadFailed);
    CHECK(state.project.state() == xq::XQProject::LifecycleState::Created);
    CHECK(count_nodes(state.project.scene()) == 0);

    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    int result = test_empty_startup_attaches_workflow();
    if (result != 0) {
        return result;
    }
    result = test_native_project_startup_loads_real_scene();
    if (result != 0) {
        return result;
    }
    result = test_lazy_project_startup_renders_lazy_geometry_selection();
    if (result != 0) {
        return result;
    }
    return test_invalid_project_path_fails_closed();
}
