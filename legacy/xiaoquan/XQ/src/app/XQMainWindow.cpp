#include "app/XQMainWindow.h"

#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQFlowResultPayload.h"
#include "core/XQMeshPayload.h"
#include "core/XQPathPayload.h"
#include "core/XQPayload.h"
#include "core/XQSegmentationMaskPayload.h"
#include "core/XQSurfaceModelPayload.h"
#include "core/source/IGeometrySource.h"
#include "core/command/XQCommandStack.h"
#include "core/meshing/ITetMesher.h"
#include "services/resource/GeometryResourceManager.h"
#include "services/resource/GeometrySourceResolver.h"
#include "ui/XQSceneModel.h"
#include "ui/controllers/AiController.h"
#include "ui/controllers/FlowController.h"
#include "ui/controllers/MeshingController.h"
#include "ui/controllers/ModelingController.h"
#include "ui/controllers/PathController.h"
#include "ui/controllers/SegmentationController.h"
#include "visualization/XQRenderWidget.h"

#if defined(XQ_ENABLE_MMG)
#include "adapters/mmg/TetGenThenMmg.h"
#elif defined(XQ_ENABLE_TETGEN)
#include "adapters/tetgen/TetGenTetMesher.h"
#endif

#include <QAction>
#include <QDockWidget>
#include <QImage>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QModelIndex>
#include <QPixmap>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStringList>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <memory>
#include <string>

namespace xq {
namespace {

const int kDefaultRenderWidth = 256;
const int kDefaultRenderHeight = 256;

// The six workflow stages, in main-line order.
const char* const kStageNames[] = {
    "Path", "Segmentation", "Modeling", "Meshing", "Flow", "AI",
};

} // namespace

XQMainWindow::XQMainWindow(const XQScene* scene, QWidget* parent)
    : QMainWindow(parent)
    , sceneModel_(new XQSceneModel(scene, this))
    , sceneTreeView_(new QTreeView(this))
    , centralStack_(new QStackedWidget(this))
    , imageLabel_(new QLabel(this))
    , renderWidget_(new XQRenderWidget(this))
    , imageViewer_()
    , lastRgba_()
{
    setWindowTitle(QStringLiteral("XQ"));

    sceneTreeView_->setObjectName(QStringLiteral("xqSceneTreeView"));
    sceneTreeView_->setModel(sceneModel_);

    QDockWidget* sceneDock = new QDockWidget(QStringLiteral("Scene"), this);
    sceneDock->setObjectName(QStringLiteral("xqSceneDock"));
    sceneDock->setWidget(sceneTreeView_);
    addDockWidget(Qt::LeftDockWidgetArea, sceneDock);

    imageLabel_->setObjectName(QStringLiteral("xqImageLabel"));
    imageLabel_->setAlignment(Qt::AlignCenter);
    imageLabel_->setMinimumSize(kDefaultRenderWidth, kDefaultRenderHeight);
    imageLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Central area stacks the legacy offscreen-RGBA label (showImage path,
    // test_main_window depends on it) and the interactive 3D view. showImage
    // raises the label; selecting a scene node raises the render widget.
    renderWidget_->setObjectName(QStringLiteral("xqRenderWidget"));
    centralStack_->setObjectName(QStringLiteral("xqCentralStack"));
    centralStack_->addWidget(renderWidget_);
    centralStack_->addWidget(imageLabel_);
    centralStack_->setCurrentWidget(renderWidget_);
    setCentralWidget(centralStack_);

    // Selection-driven rendering. The selection model exists once setModel() has
    // run (above); setScene()/attachWorkflow() only swap the model's scene, not
    // the model, so this connection stays valid. With a null scene nothing is
    // selectable, so the slot is a no-op -- safe for the default-constructed
    // window that test_main_window builds before showImage().
    QObject::connect(sceneTreeView_->selectionModel(),
                     &QItemSelectionModel::currentChanged,
                     this,
                     &XQMainWindow::onSceneSelectionChanged);

    resize(900, 600);
}

XQMainWindow::~XQMainWindow() = default;

void XQMainWindow::setScene(const XQScene* scene)
{
    sceneModel_->setScene(scene);
}

ImageRenderResult XQMainWindow::showImage(const XQImageVolume& image)
{
    const int width = std::max(kDefaultRenderWidth, imageLabel_->width());
    const int height = std::max(kDefaultRenderHeight, imageLabel_->height());

    ImageRenderResult result = imageViewer_.renderToRgba(image, width, height, &lastRgba_);
    if (!result.ok) {
        imageLabel_->clear();
        return result;
    }

    QImage rendered(lastRgba_.data(),
                    result.width,
                    result.height,
                    result.width * 4,
                    QImage::Format_RGBA8888);
    imageLabel_->setPixmap(QPixmap::fromImage(rendered.copy()));
    centralStack_->setCurrentWidget(imageLabel_);

    return result;
}

std::size_t XQMainWindow::lastRgbaByteCount() const
{
    return lastRgba_.size();
}

void XQMainWindow::attachWorkflow(XQScene* scene, XQCommandStack* stack)
{
    workflowScene_ = scene;
    commandStack_ = stack;

    // Display the (mutable) scene in the tree; the model reads it as const.
    setScene(scene);

    pathController_.reset(new PathController(scene, stack));
    segmentationController_.reset(new SegmentationController(scene, stack));
    modelingController_.reset(new ModelingController(scene, stack));
#if defined(XQ_ENABLE_MMG)
    volumeMeshKernel_.reset(new TetGenThenMmg());
#elif defined(XQ_ENABLE_TETGEN)
    volumeMeshKernel_.reset(new TetGenTetMesher());
#endif
    meshingController_.reset(new MeshingController(scene, stack, volumeMeshKernel_.get()));
    flowController_.reset(new FlowController(scene, stack));
    aiController_.reset(new AiController(scene, stack));

    buildStagePanel();
    buildEditMenu();
}

void XQMainWindow::attachGeometryResources(const AssetRegistry* registry,
                                           const std::string& assetRootDir)
{
    geometryRegistry_ = registry;
    geometryResources_.reset();
    if (registry != nullptr && !assetRootDir.empty()) {
        geometryResources_.reset(new GeometryResourceManager(registry, assetRootDir));
    }
}

void XQMainWindow::buildStagePanel()
{
    stagePanel_ = new QStackedWidget(this);
    stagePanel_->setObjectName(QStringLiteral("xqStagePanel"));

    // First version: each stage is a thin placeholder page (the controllers hold
    // the click->service->command->scene logic, which the headless tests drive).
    // The pages give the panel a stable structure the integration can grow into.
    for (const char* name : kStageNames) {
        QWidget* page = new QWidget(stagePanel_);
        page->setObjectName(QString::fromLatin1("xqStagePage_") + QString::fromLatin1(name));
        QVBoxLayout* layout = new QVBoxLayout(page);
        QLabel* title = new QLabel(QString::fromLatin1(name), page);
        title->setObjectName(QString::fromLatin1("xqStageTitle_") + QString::fromLatin1(name));
        layout->addWidget(title);
        layout->addStretch();
        stagePanel_->addWidget(page);
    }

    QDockWidget* stageDock = new QDockWidget(QStringLiteral("Stages"), this);
    stageDock->setObjectName(QStringLiteral("xqStageDock"));
    stageDock->setWidget(stagePanel_);
    addDockWidget(Qt::RightDockWidgetArea, stageDock);
}

void XQMainWindow::buildEditMenu()
{
    QMenu* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));

    undoAction_ = editMenu->addAction(QStringLiteral("&Undo"));
    undoAction_->setObjectName(QStringLiteral("xqUndoAction"));
    undoAction_->setShortcut(QKeySequence::Undo);
    QObject::connect(undoAction_, &QAction::triggered, this, &XQMainWindow::undo);

    redoAction_ = editMenu->addAction(QStringLiteral("&Redo"));
    redoAction_->setObjectName(QStringLiteral("xqRedoAction"));
    redoAction_->setShortcut(QKeySequence::Redo);
    QObject::connect(redoAction_, &QAction::triggered, this, &XQMainWindow::redo);
}

void XQMainWindow::refreshSceneTree()
{
    sceneModel_->refresh();
}

void XQMainWindow::undo()
{
    if (commandStack_ != nullptr && commandStack_->undo()) {
        refreshSceneTree();
    }
}

void XQMainWindow::redo()
{
    if (commandStack_ != nullptr && commandStack_->redo()) {
        refreshSceneTree();
    }
}

PathController* XQMainWindow::pathController() const
{
    return pathController_.get();
}

SegmentationController* XQMainWindow::segmentationController() const
{
    return segmentationController_.get();
}

ModelingController* XQMainWindow::modelingController() const
{
    return modelingController_.get();
}

MeshingController* XQMainWindow::meshingController() const
{
    return meshingController_.get();
}

FlowController* XQMainWindow::flowController() const
{
    return flowController_.get();
}

AiController* XQMainWindow::aiController() const
{
    return aiController_.get();
}

void XQMainWindow::onSceneSelectionChanged(const QModelIndex& current,
                                           const QModelIndex& previous)
{
    Q_UNUSED(previous);

    const XQDataNode* node = sceneModel_->nodeForIndex(current);
    if (node == nullptr || !node->payload()) {
        return;
    }

    // Pure dispatch: map the node's payload to the matching renderer entry. All
    // rendering logic lives in XQSceneRenderer; this only selects which add*()
    // to call. The domainType() switch guarantees the static_pointer_cast below
    // matches the payload's concrete type.
    XQSceneRenderer& renderer = renderWidget_->renderer();
    renderer.clear();

    RenderStats stats;
    switch (node->domainType()) {
    case XQDomainType::SurfaceModel: {
        const auto payload =
            std::static_pointer_cast<XQSurfaceModelPayload>(node->payload());
        const XQSurfaceModel& model = payload->model();
        if (model.hasTriangleGeometry()) {
            stats = renderer.addSurface(*model.triangleGeometry());
        } else if (geometryResources_ != nullptr && geometryRegistry_ != nullptr) {
            GeometryResourceManager::GeometrySourceHandle lazy =
                resolveLazyGeometrySource(*payload,
                                          *geometryResources_,
                                          *geometryRegistry_,
                                          LazyGeometrySourceMode::SurfaceOnly);
            if (lazy.valid() && lazy->meta().triangleCount > 0) {
                stats = renderer.addSurfaceProgressive(lazy.source());
            }
        }
        break;
    }
    case XQDomainType::Mesh: {
        const auto payload = std::static_pointer_cast<XQMeshPayload>(node->payload());
        const XQMesh& mesh = payload->mesh();
        if (mesh.hasVolumeTets()) {
            stats = renderer.addVolumeMesh(*mesh.volumeTets());
        } else if (mesh.hasSurfaceTriangles()) {
            stats = renderer.addSurface(*mesh.surfaceTriangles());
        } else if (geometryResources_ != nullptr && geometryRegistry_ != nullptr) {
            GeometryResourceManager::GeometrySourceHandle lazy =
                resolveLazyGeometrySource(*payload,
                                          *geometryResources_,
                                          *geometryRegistry_,
                                          LazyGeometrySourceMode::TetOnly);
            if (lazy.valid()) {
                const GeometryMeta meta = lazy->meta();
                if (meta.tetCount > 0) {
                    stats = renderer.addVolumeMeshProgressive(lazy.source());
                }
            }
            if (!stats.ok) {
                lazy = resolveLazyGeometrySource(*payload,
                                                 *geometryResources_,
                                                 *geometryRegistry_,
                                                 LazyGeometrySourceMode::SurfaceOnly);
                if (lazy.valid() && lazy->meta().triangleCount > 0) {
                    stats = renderer.addSurfaceProgressive(lazy.source());
                }
            }
        }
        break;
    }
    case XQDomainType::Path: {
        const auto payload = std::static_pointer_cast<XQPathPayload>(node->payload());
        stats = renderer.addPath(*payload);
        break;
    }
    case XQDomainType::SegmentationMask: {
        const auto payload =
            std::static_pointer_cast<XQSegmentationMaskPayload>(node->payload());
        stats = renderer.addSegmentationMask(*payload);
        break;
    }
    case XQDomainType::FlowResult: {
        const auto payload =
            std::static_pointer_cast<XQFlowResultPayload>(node->payload());
        stats = renderer.addFlowResult(*payload);
        break;
    }
    default:
        // Image nodes carry only a source-path payload (no XQImageVolume /
        // scalar buffer in the scene), and other domains have no geometry to
        // show. Leave the cleared (empty) scene; the render widget shows blank.
        break;
    }

    if (stats.ok) {
        centralStack_->setCurrentWidget(renderWidget_);
        renderWidget_->render();
    }
}

} // namespace xq
