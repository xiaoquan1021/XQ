#ifndef XQ_APP_XQ_MAIN_WINDOW_H
#define XQ_APP_XQ_MAIN_WINDOW_H

#include "visualization/XQImageViewer.h"

#include <QMainWindow>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class QAction;
class QLabel;
class QModelIndex;
class QStackedWidget;
class QTreeView;

namespace xq {

class XQImageVolume;
class XQScene;
class XQSceneModel;
class XQCommandStack;
class XQRenderWidget;
class AssetRegistry;
class GeometryResourceManager;
class PathController;
class SegmentationController;
class ModelingController;
class MeshingController;
class FlowController;
class AiController;
class ITetMesher;

class XQMainWindow : public QMainWindow {
public:
    explicit XQMainWindow(const XQScene* scene = nullptr, QWidget* parent = nullptr);
    ~XQMainWindow() override;

    void setScene(const XQScene* scene);
    ImageRenderResult showImage(const XQImageVolume& image);
    std::size_t lastRgbaByteCount() const;

    // Attaches a mutable scene + command stack and wires the six stage
    // controllers + the right-hand stage panel + Edit undo/redo. The const
    // setScene() path (used by test_main_window) is unaffected: callers that
    // only display a scene need not attach a workflow. The window does not own
    // the scene or the stack; the caller keeps them alive.
    void attachWorkflow(XQScene* scene, XQCommandStack* stack);

    // Attaches the project asset registry/root needed to resolve lazy geometry
    // payloads stamped with geometryAssetId. The window borrows the registry;
    // callers keep the owning project alive, matching attachWorkflow().
    void attachGeometryResources(const AssetRegistry* registry,
                                 const std::string& assetRootDir);

    // Controller accessors (valid only after attachWorkflow). The shell widgets
    // gather input and drive these; tests can drive them headlessly too.
    PathController* pathController() const;
    SegmentationController* segmentationController() const;
    ModelingController* modelingController() const;
    MeshingController* meshingController() const;
    FlowController* flowController() const;
    AiController* aiController() const;

    // Edit actions exposed for testing and menu wiring.
    void undo();
    void redo();

private:
    void buildStagePanel();
    void buildEditMenu();
    void refreshSceneTree();

    // Selection-driven rendering: maps the current scene-tree selection to its
    // node payload and renders it in the interactive view. Pure dispatch -- all
    // rendering logic lives in XQSceneRenderer.
    void onSceneSelectionChanged(const QModelIndex& current, const QModelIndex& previous);

    XQSceneModel* sceneModel_;
    QTreeView* sceneTreeView_;
    QStackedWidget* centralStack_;
    QLabel* imageLabel_;
    XQRenderWidget* renderWidget_;
    XQImageViewer imageViewer_;
    std::vector<unsigned char> lastRgba_;

    // Workflow wiring (null until attachWorkflow).
    XQScene* workflowScene_ = nullptr;
    XQCommandStack* commandStack_ = nullptr;
    const AssetRegistry* geometryRegistry_ = nullptr;
    QStackedWidget* stagePanel_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;

    std::unique_ptr<PathController> pathController_;
    std::unique_ptr<SegmentationController> segmentationController_;
    std::unique_ptr<ModelingController> modelingController_;
    // Declared before meshingController_: destruction is reverse-declaration
    // order, so the controller (which borrows this kernel) is destroyed first,
    // then the kernel -- no dangling borrow.
    std::unique_ptr<ITetMesher> volumeMeshKernel_;
    std::unique_ptr<MeshingController> meshingController_;
    std::unique_ptr<FlowController> flowController_;
    std::unique_ptr<AiController> aiController_;
    std::unique_ptr<GeometryResourceManager> geometryResources_;
};

} // namespace xq

#endif // XQ_APP_XQ_MAIN_WINDOW_H
