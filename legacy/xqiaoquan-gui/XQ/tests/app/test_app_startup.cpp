#include <app/XQAppStartup.h>
#include <app/XQMainWindow.h>
#include <app/XQPreferencesDialog.h>
#include <services/resource/GeometryResourceManager.h>

#include <core/NodeId.h>
#include <core/asset/AssetRecord.h>
#include <core/asset/AssetRegistry.h>
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
#include <QAction>
#include <QApplication>
#include <QSettings>
#include <QDockWidget>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListWidget>
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
    // The scene model is a two-level tree: root rows are type groups, node rows
    // are their children. Walk group rows then their children.
    for (int g = 0; g < model->rowCount(QModelIndex()); ++g) {
        const QModelIndex groupIndex =
            model->index(g, xq::XQSceneModel::NameColumn, QModelIndex());
        for (int n = 0; n < model->rowCount(groupIndex); ++n) {
            const QModelIndex index =
                model->index(n, xq::XQSceneModel::NameColumn, groupIndex);
            if (index.data(Qt::DisplayRole).toString().toStdString() == displayName) {
                return index;
            }
        }
    }
    return QModelIndex();
}

// Total node (leaf) rows across all group rows -- the tree equivalent of the old
// flat root rowCount.
int tree_node_count(QAbstractItemModel* model)
{
    if (model == nullptr) {
        return 0;
    }
    int total = 0;
    for (int g = 0; g < model->rowCount(QModelIndex()); ++g) {
        total += model->rowCount(model->index(g, 0, QModelIndex()));
    }
    return total;
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
    CHECK(state.assetRootDir.empty());
    CHECK(state.projectFilePath.empty());

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
    CHECK(state.projectFilePath == files.path.string());

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
    QWidget* renderWidget = window.findChild<QWidget*>("xqRenderWidget");
    QTreeView* tree = window.findChild<QTreeView*>("xqSceneTreeView");
    CHECK(stack != nullptr);
    CHECK(renderWidget != nullptr);
    CHECK(tree != nullptr);
    CHECK(tree->selectionModel() != nullptr);

    // Two central pages now: the MPR four-view workspace (page 0, default) and
    // the P3 cross-section workbench (page 1, shown only in the contour stage).
    CHECK(stack->count() == 2);
    QWidget* mprHost = window.findChild<QWidget*>("xqMprView");
    CHECK(mprHost != nullptr);
    QWidget* crossSectionHost =
        window.findChild<QWidget*>("xqCrossSectionWorkbench");
    CHECK(crossSectionHost != nullptr);
    // The MPR workspace is the default page; the section workbench is present
    // but not raised until the contour stage activates it.
    CHECK(stack->currentWidget() == mprHost);

    QModelIndex surfaceIndex = find_display_name(tree->model(), "Lazy Surface");
    CHECK(surfaceIndex.isValid());
    tree->selectionModel()->setCurrentIndex(
        surfaceIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QApplication::processEvents();
    // The 3D render widget lives inside the MPR workspace page; selecting lazy
    // geometry must keep that hosting page current and not crash.
    CHECK(stack->currentWidget() != nullptr);
    CHECK(stack->currentWidget()->isAncestorOf(renderWidget));
    CHECK(renderWidget->isVisibleTo(&window));

    QModelIndex meshIndex = find_display_name(tree->model(), "Lazy Mesh");
    CHECK(meshIndex.isValid());
    tree->selectionModel()->setCurrentIndex(
        meshIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    QApplication::processEvents();
    CHECK(stack->currentWidget() != nullptr);
    CHECK(stack->currentWidget()->isAncestorOf(renderWidget));
    CHECK(renderWidget->isVisibleTo(&window));

    // Empty-scene hint: hidden after a successful geometry selection.
    QLabel* emptyHint = window.findChild<QLabel*>("xqEmptySceneHint");
    CHECK(emptyHint != nullptr);
    CHECK(!emptyHint->isVisibleTo(renderWidget->parentWidget()));

    return 0;
}

int test_attach_workflow_is_reentrant()
{
    xq::XQMainWindow window;

    xq::XQScene sceneA;
    xq::XQCommandStack stackA;
    window.attachWorkflow(&sceneA, &stackA);

    // Baseline after the first attach: what a re-attach must not grow.
    const int dockCountAfterFirst =
        static_cast<int>(window.findChildren<QDockWidget*>().size());
    CHECK(window.findChildren<QStackedWidget*>("xqStagePanel").size() == 1);
    CHECK(window.findChildren<QAction*>("xqUndoAction").size() == 1);

    xq::XQScene sceneB;
    sceneB.insert(xq::XQDataNode(xq::NodeId(11), "volume", "B-node-1"));
    sceneB.insert(xq::XQDataNode(xq::NodeId(12), "mesh", "B-node-2"));
    xq::XQCommandStack stackB;
    window.attachWorkflow(&sceneB, &stackB);
    QApplication::processEvents();

    // Re-attach must not duplicate: same dock count, one stage panel, one
    // Undo/Redo pair, and the tree shows the second scene's rows.
    CHECK(static_cast<int>(window.findChildren<QDockWidget*>().size())
          == dockCountAfterFirst);
    CHECK(window.findChildren<QStackedWidget*>("xqStagePanel").size() == 1);
    CHECK(window.findChildren<QAction*>("xqUndoAction").size() == 1);
    CHECK(window.findChildren<QAction*>("xqRedoAction").size() == 1);
    QTreeView* tree = window.findChild<QTreeView*>("xqSceneTreeView");
    CHECK(tree != nullptr);
    // Two nodes (both legacy/Unknown -> one Ungrouped group). Assert on the node
    // (leaf) count, which the tree change preserves.
    CHECK(tree_node_count(tree->model()) == 2);

    return 0;
}

int test_workspace_save_reopen_roundtrip()
{
    const xq::NodeId surfaceId(401);
    const xq::NodeId meshId(402);
    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    add_lazy_geometry_fixture_nodes(&project, surfaceId, meshId);

    TempProjectFiles files("xq_ws_roundtrip");
    const xq::XQProjectWriter::Status saved =
        xq::XQProjectWriter::save(project, files.path.string());
    CHECK(saved == xq::XQProjectWriter::Status::Ok);

    xq::XQAppStartupConfig config;
    config.projectPath = files.path.string();
    xq::XQAppStartupState state;
    const xq::XQAppStartupStatus status = xq::initializeAppStartup(config, &state);
    CHECK(status == xq::XQAppStartupStatus::Ok);
    CHECK(count_nodes(state.project.scene()) == 2);
    CHECK(state.assetRootDir == files.assets.string());
    CHECK(state.project.scene().find(surfaceId) != nullptr);
    CHECK(state.project.scene().find(meshId) != nullptr);
    return 0;
}

int test_workspace_save_as_carries_lazy_blobs()
{
    const xq::NodeId surfaceId(501);
    const xq::NodeId meshId(502);
    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    add_lazy_geometry_fixture_nodes(&project, surfaceId, meshId);

    TempProjectFiles filesA("xq_ws_saveas_a");
    CHECK(xq::XQProjectWriter::save(project, filesA.path.string())
          == xq::XQProjectWriter::Status::Ok);

    // Lazy-open A (nodes now carry asset ids + empty handles).
    xq::XQAppStartupConfig config;
    config.projectPath = filesA.path.string();
    xq::XQAppStartupState state;
    CHECK(xq::initializeAppStartup(config, &state) == xq::XQAppStartupStatus::Ok);

    xq::XQMainWindow window;
    window.setProperty("xqSuppressDialogs", true);
    xq::attachMainWindowWorkflow(&window, &state);

    // Save-as into a different directory pair.
    TempProjectFiles filesB("xq_ws_saveas_b");
    const bool savedAs = window.saveWorkspaceFile(
        QString::fromStdString(filesB.path.string()));
    CHECK(savedAs);

    // Every registry-referenced blob must exist under B.assets.
    std::size_t missing = 0;
    state.project.assetRegistry().visit_assets(
        [&](const xq::AssetRecord& record) {
            for (std::size_t b = 0; b < record.blobs.size(); ++b) {
                const std::filesystem::path blob =
                    filesB.assets / record.blobs[b].second.relPath;
                std::error_code ec;
                const bool exists = std::filesystem::exists(blob, ec);
                if (!exists) {
                    ++missing;
                }
            }
        });
    CHECK(missing == 0);

    // And B must load: node count conserved, both nodes present.
    xq::XQAppStartupConfig configB;
    configB.projectPath = filesB.path.string();
    xq::XQAppStartupState stateB;
    CHECK(xq::initializeAppStartup(configB, &stateB) == xq::XQAppStartupStatus::Ok);
    CHECK(count_nodes(stateB.project.scene()) == 2);
    CHECK(stateB.project.scene().find(surfaceId) != nullptr);
    CHECK(stateB.project.scene().find(meshId) != nullptr);
    return 0;
}

int test_window_open_workspace_swaps_state()
{
    // Startup: empty workflow.
    xq::XQAppStartupState startupState;
    CHECK(xq::initializeAppStartup(xq::XQAppStartupConfig{}, &startupState)
          == xq::XQAppStartupStatus::Ok);
    xq::XQMainWindow window;
    window.setProperty("xqSuppressDialogs", true);
    xq::attachMainWindowWorkflow(&window, &startupState);

    // A real project on disk to open at runtime.
    const xq::NodeId surfaceId(601);
    const xq::NodeId meshId(602);
    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    add_lazy_geometry_fixture_nodes(&project, surfaceId, meshId);
    TempProjectFiles files("xq_ws_window_open");
    CHECK(xq::XQProjectWriter::save(project, files.path.string())
          == xq::XQProjectWriter::Status::Ok);

    const bool opened = window.openWorkspaceFromPath(
        QString::fromStdString(files.path.string()));
    CHECK(opened);
    QApplication::processEvents();

    QTreeView* tree = window.findChild<QTreeView*>("xqSceneTreeView");
    CHECK(tree != nullptr);
    // A surface model + a mesh -> two nodes across the Models and Meshes groups.
    CHECK(tree_node_count(tree->model()) == 2);
    // Re-attach happened through the reentrant path: still one stage panel.
    CHECK(window.findChildren<QStackedWidget*>("xqStagePanel").size() == 1);

    // A pending path draft must not survive a workspace swap: its points were
    // picked against the previous dataset's frame.
    window.appendPathDraftPointForTest({1.0, 2.0, 3.0});
    window.appendPathDraftPointForTest({4.0, 5.0, 6.0});
    QApplication::processEvents();
    QListWidget* pathPoints = window.findChild<QListWidget*>("xqPathPointList");
    CHECK(pathPoints != nullptr);
    CHECK(pathPoints->count() == 2);
    CHECK(window.openWorkspaceFromPath(QString::fromStdString(files.path.string())));
    QApplication::processEvents();
    pathPoints = window.findChild<QListWidget*>("xqPathPointList");
    CHECK(pathPoints != nullptr);
    CHECK(pathPoints->count() == 0);

    // Open failure leaves the swapped-in state intact.
    const bool openedBad = window.openWorkspaceFromPath(
        QStringLiteral("Z:/definitely/missing.xqproj"));
    CHECK(!openedBad);
    CHECK(tree_node_count(tree->model()) == 2);
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
    CHECK(state.assetRootDir.empty());
    CHECK(state.projectFilePath.empty());

    return 0;
}

int test_geometry_budget_flows_from_settings()
{
    // Pin the setting to a known value, then attach and assert传导.
    QSettings settings(QStringLiteral("XQ"), QStringLiteral("XQ"));
    const QVariant saved = settings.value(QStringLiteral("memory/geometryBudgetMiB"));
    settings.setValue(QStringLiteral("memory/geometryBudgetMiB"), 512);

    const xq::NodeId surfaceId(701);
    const xq::NodeId meshId(702);
    xq::XQProject project;
    CHECK(project.open() == xq::XQProject::LifecycleResult::Ok);
    add_lazy_geometry_fixture_nodes(&project, surfaceId, meshId);
    TempProjectFiles files("xq_budget_flow");
    CHECK(xq::XQProjectWriter::save(project, files.path.string())
          == xq::XQProjectWriter::Status::Ok);

    xq::XQAppStartupConfig config;
    config.projectPath = files.path.string();
    xq::XQAppStartupState state;
    CHECK(xq::initializeAppStartup(config, &state) == xq::XQAppStartupStatus::Ok);

    xq::XQMainWindow window;
    xq::attachMainWindowWorkflow(&window, &state);

    const xq::GeometryResourceManager* manager = window.geometryResourceManager();
    CHECK(manager != nullptr);
    CHECK(manager->budgetBytes() == (std::size_t(512) << 20));

    // Restore the user's persisted value (don't pollute the real settings).
    if (saved.isValid()) {
        settings.setValue(QStringLiteral("memory/geometryBudgetMiB"), saved);
    } else {
        settings.remove(QStringLiteral("memory/geometryBudgetMiB"));
    }
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
    result = test_attach_workflow_is_reentrant();
    if (result != 0) {
        return result;
    }
    result = test_workspace_save_reopen_roundtrip();
    if (result != 0) {
        return result;
    }
    result = test_workspace_save_as_carries_lazy_blobs();
    if (result != 0) {
        return result;
    }
    result = test_window_open_workspace_swaps_state();
    if (result != 0) {
        return result;
    }
    result = test_geometry_budget_flows_from_settings();
    if (result != 0) {
        return result;
    }
    return test_invalid_project_path_fails_closed();
}
