#include <app/XQMainWindow.h>

#include <core/NodeId.h>
#include <core/XQDataNode.h>
#include <core/XQDomainType.h>
#include <core/XQImageVolume.h>
#include <core/XQPayload.h>
#include <core/XQScene.h>
#include <core/XQSourcePayload.h>
#include <core/command/XQCommandStack.h>
#include <ui/controllers/PathController.h>

#include <QAbstractItemModel>
#include <QApplication>
#include <QModelIndex>
#include <QStackedWidget>
#include <QTreeView>

#include <cstdio>
#include <memory>

namespace {

int fail(const char* message)
{
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

std::size_t count_nodes(const xq::XQScene& scene)
{
    std::size_t count = 0;
    scene.visit_nodes([&count](const xq::XQDataNode&) {
        ++count;
    });
    return count;
}

xq::ImageGeometry make_geometry()
{
    xq::ImageGeometry geometry = {};
    geometry.dimensions[0] = 40;
    geometry.dimensions[1] = 30;
    geometry.dimensions[2] = 8;
    geometry.spacing[0] = 0.8;
    geometry.spacing[1] = 0.8;
    geometry.spacing[2] = 1.5;
    geometry.origin[0] = -10.0;
    geometry.origin[1] = -6.0;
    geometry.origin[2] = 2.0;
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            geometry.direction[row][column] = row == column ? 1.0 : 0.0;
        }
    }
    geometry.coordinateSystem = xq::ImageCoordinateSystem::LPS;
    return geometry;
}

xq::XQImageVolume make_image()
{
    xq::XQImageVolume image;
    image.setGeometry(make_geometry());
    image.setScalarType(xq::ScalarType::Float32);
    image.setComponentCount(1);
    image.setIntensityRange({0.0, 2048.0});
    image.setBuffer(std::make_shared<xq::ImageBufferHandle>());
    image.setWindowCenter(512.0);
    image.setWindowWidth(1024.0);
    return image;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    xq::XQScene scene;
    if (scene.insert(xq::XQDataNode(xq::NodeId(1), "volume", "Synthetic Volume"))
        != xq::XQScene::InsertResult::Inserted) {
        return fail("insert volume node");
    }
    if (scene.insert(xq::XQDataNode(xq::NodeId(2), "mesh", "Derived Mesh"))
        != xq::XQScene::InsertResult::Inserted) {
        return fail("insert mesh node");
    }

    xq::XQMainWindow window;
    window.setScene(&scene);

    QTreeView* tree = window.findChild<QTreeView*>("xqSceneTreeView");
    if (tree == nullptr) {
        return fail("main window exposes scene tree view");
    }

    QAbstractItemModel* model = tree->model();
    if (model == nullptr) {
        return fail("scene tree view has a model");
    }
    if (model->rowCount(QModelIndex()) != static_cast<int>(count_nodes(scene))) {
        return fail("tree model rowCount matches scene node count");
    }

    xq::XQImageVolume image = make_image();
    const xq::ImageRenderResult result = window.showImage(image);
    if (!result.ok) {
        return fail("showImage renders a synthetic image");
    }

    const std::size_t expected_rgba_bytes =
        static_cast<std::size_t>(result.width) * static_cast<std::size_t>(result.height) * 4U;
    if (window.lastRgbaByteCount() != expected_rgba_bytes) {
        return fail("showImage produces one RGBA buffer with four bytes per pixel");
    }

    // --- workflow integration: attach a mutable scene + command stack ---
    xq::XQScene workflowScene;
    xq::XQCommandStack stack;
    const xq::NodeId imageId(1);
    workflowScene.insert(xq::XQDataNode(imageId, xq::XQDomainType::Image, "img",
                                        std::make_shared<xq::XQSourcePayload>(
                                            xq::XQDomainType::Image, "Images/x.vti")));

    xq::XQMainWindow workflowWindow;
    workflowWindow.attachWorkflow(&workflowScene, &stack);

    if (workflowWindow.findChild<QStackedWidget*>("xqStagePanel") == nullptr) {
        return fail("attachWorkflow builds the stage panel");
    }
    if (workflowWindow.pathController() == nullptr
        || workflowWindow.aiController() == nullptr) {
        return fail("attachWorkflow wires the stage controllers");
    }

    // Drive a stage controller, then exercise the window's undo/redo path.
    xq::PathController::AddPathIntent intent;
    intent.newPathId = xq::NodeId(2);
    intent.name = "centerline";
    intent.sourceImageNode = imageId;
    intent.controlPoints = {{{0.0, 0.0, 0.0}}, {{1.0, 0.0, 0.0}}, {{1.0, 1.0, 0.0}}};
    intent.spacing = 0.5;
    if (workflowWindow.pathController()->addPath(intent)
        != xq::PathController::Status::Ok) {
        return fail("path controller adds a path through the workflow");
    }
    if (workflowScene.find(xq::NodeId(2)) == nullptr) {
        return fail("workflow path node is in the scene");
    }

    workflowWindow.undo();
    if (workflowScene.find(xq::NodeId(2)) != nullptr) {
        return fail("window undo reverts the path command");
    }
    workflowWindow.redo();
    if (workflowScene.find(xq::NodeId(2)) == nullptr) {
        return fail("window redo restores the path command");
    }

    return 0;
}
