#ifndef XQ_APP_XQ_WORKFLOW_SESSION_H
#define XQ_APP_XQ_WORKFLOW_SESSION_H

#include "app/WorkflowCapabilities.h"
#include "app/XQTaskRunner.h"

#include <functional>
#include <memory>

namespace xq {

class XQScene;
class XQCommand;
class XQCommandStack;
class PathController;
class SegmentationController;
class ItkVascularPreprocessor;
class ItkVascularRoiPriorReader;
class ItkAutomaticVesselSegmenter;
class ItkCenterlineSkeletonizer3D;
class ModelingController;
class MeshingController;
class FlowController;
class FlowSmokeController;
class VesselProfileController;
class PathModuleController;
class CenterlineBController;
class AiController;
class ITetMesher;
class XQProject;

// Bundles the workflow wiring that XQMainWindow used to hold loose: the attached
// scene + command stack, the stage controllers (Flow optional), the mesh kernel,
// and the resident task runner. Owns the single gateway for command-stack mutations so
// every push / undo / redo triggers exactly one scene-changed notification (the
// unified render-sync point).
//
// A plain (non-QObject) class: the notification callback is a std::function, so
// no moc / signals are needed. The window constructs it once (the task runner
// lives here for the window's whole life) and only rebinds scene/stack + rebuilds
// controllers on attach(); the taskRunner connections the window made at
// construction therefore stay valid across re-attach.
class XQWorkflowSession {
public:
    explicit XQWorkflowSession(
        WorkflowCapabilities capabilities = WorkflowCapabilities::compiledDefaults());
    ~XQWorkflowSession();

    XQWorkflowSession(const XQWorkflowSession&) = delete;
    XQWorkflowSession& operator=(const XQWorkflowSession&) = delete;

    // Rebinds scene/stack and rebuilds the controllers (old ones destroyed).
    void attach(XQScene* scene, XQCommandStack* stack);
    void attach(XQProject* project, XQCommandStack* stack);

    // Command gateway: on success runs the scene-changed callback (set by the
    // window to refreshSceneTree). Returns the stack's result; false when
    // unattached.
    bool pushCommand(std::unique_ptr<XQCommand> command);
    bool undo();
    bool redo();
    void setSceneChangedCallback(std::function<void()> cb);

    // Accessors (valid only after attach()).
    XQScene* scene() const { return scene_; }
    XQCommandStack* commandStack() const { return stack_; }
    PathController* pathController() const { return path_.get(); }
    SegmentationController* segmentationController() const { return segmentation_.get(); }
    ModelingController* modelingController() const { return modeling_.get(); }
    MeshingController* meshingController() const { return meshing_.get(); }
    VesselProfileController* vesselProfileController() const
    {
        return vesselProfile_.get();
    }
    PathModuleController* pathModuleController() const
    {
        return pathModules_.get();
    }
    CenterlineBController* centerlineBController() const
    {
        return centerlineB_.get();
    }
    FlowController* flowController() const;
    FlowSmokeController* flowSmokeController() const;
    bool hasFlowCapability() const { return capabilities_.flowSolver1D; }
    const WorkflowCapabilities& capabilities() const { return capabilities_; }
    AiController* aiController() const { return ai_.get(); }
    XQTaskRunner& taskRunner() { return taskRunner_; }

private:
    void attachControllers(XQScene* scene, XQCommandStack* stack);

    XQProject* project_ = nullptr;
    XQScene* scene_ = nullptr;
    XQCommandStack* stack_ = nullptr;
    WorkflowCapabilities capabilities_;
    std::function<void()> sceneChanged_;

    std::unique_ptr<PathController> path_;
    // The controller borrows these three stateless adapters. They are declared
    // before segmentation_ so reverse-order destruction releases the controller
    // first; taskRunner_ still joins before either side is destroyed.
    std::unique_ptr<ItkVascularPreprocessor> vascularPreprocessor_;
    std::unique_ptr<ItkVascularRoiPriorReader> vascularRoiPriorReader_;
    std::unique_ptr<ItkAutomaticVesselSegmenter> automaticVesselSegmenter_;
    std::unique_ptr<ItkCenterlineSkeletonizer3D> centerlineSkeletonizer_;
    std::unique_ptr<SegmentationController> segmentation_;
    std::unique_ptr<ModelingController> modeling_;
    // Declared before meshing_: destruction is reverse-declaration order, so the
    // controller (which borrows this kernel) is destroyed first, then the kernel
    // -- no dangling borrow.
    std::unique_ptr<ITetMesher> volumeMeshKernel_;
    std::unique_ptr<MeshingController> meshing_;
    std::unique_ptr<VesselProfileController> vesselProfile_;
    std::unique_ptr<PathModuleController> pathModules_;
    std::unique_ptr<CenterlineBController> centerlineB_;
#if XQ_ENABLE_FLOW
    std::unique_ptr<FlowController> flow_;
    std::unique_ptr<FlowSmokeController> flowSmoke_;
#endif
    std::unique_ptr<AiController> ai_;

    // Resident worker thread for heavy stage jobs. Declared LAST on purpose:
    // members destroy in reverse declaration order, so the destructor joins the
    // worker thread while the controllers/scene the running job captured are
    // still alive.
    XQTaskRunner taskRunner_;
};

} // namespace xq

#endif // XQ_APP_XQ_WORKFLOW_SESSION_H
