#include "app/XQWorkflowSession.h"

#include "adapters/itk/ItkAutomaticVesselSegmenter.h"
#include "adapters/itk/ItkCenterlineSkeletonizer3D.h"
#include "adapters/itk/ItkVascularPreprocessor.h"
#include "adapters/itk/ItkVascularRoiPriorReader.h"
#include "core/command/XQCommand.h"
#include "core/command/XQCommandStack.h"
#include "core/meshing/ITetMesher.h"
#include "core/XQProject.h"
#include "ui/controllers/AiController.h"
#include "ui/controllers/CenterlineBController.h"
#if XQ_ENABLE_FLOW
#include "ui/controllers/FlowController.h"
#include "ui/controllers/FlowSmokeController.h"
#endif
#include "ui/controllers/MeshingController.h"
#include "ui/controllers/ModelingController.h"
#include "ui/controllers/PathController.h"
#include "ui/controllers/PathModuleController.h"
#include "ui/controllers/SegmentationController.h"
#include "ui/controllers/VesselProfileController.h"

#if defined(XQ_ENABLE_MMG)
#include "adapters/mmg/TetGenThenMmg.h"
#elif defined(XQ_ENABLE_TETGEN)
#include "adapters/tetgen/TetGenTetMesher.h"
#endif

#include <utility>

namespace xq {

XQWorkflowSession::XQWorkflowSession(WorkflowCapabilities capabilities)
    : capabilities_(capabilities.constrainedToBuild())
    , vascularPreprocessor_(new ItkVascularPreprocessor())
    , vascularRoiPriorReader_(new ItkVascularRoiPriorReader())
    , automaticVesselSegmenter_(new ItkAutomaticVesselSegmenter())
    , centerlineSkeletonizer_(new ItkCenterlineSkeletonizer3D())
{
}

// Out-of-line dtor: the unique_ptr members hold forward-declared types, so the
// deleter must see the full definitions (included above).
XQWorkflowSession::~XQWorkflowSession() = default;

void XQWorkflowSession::attach(XQScene* scene, XQCommandStack* stack)
{
    project_ = nullptr;
    attachControllers(scene, stack);
}

void XQWorkflowSession::attach(XQProject* project, XQCommandStack* stack)
{
    project_ = project;
    attachControllers(project != nullptr ? &project->scene() : nullptr, stack);
}

void XQWorkflowSession::attachControllers(
    XQScene* scene,
    XQCommandStack* stack)
{
    scene_ = scene;
    stack_ = stack;

    path_.reset(new PathController(scene, stack));
    segmentation_.reset(new SegmentationController(
        scene, stack, vascularPreprocessor_.get(), vascularRoiPriorReader_.get(),
        automaticVesselSegmenter_.get()));
    modeling_.reset(new ModelingController(scene, stack));
#if defined(XQ_ENABLE_MMG)
    volumeMeshKernel_.reset(new TetGenThenMmg());
#elif defined(XQ_ENABLE_TETGEN)
    volumeMeshKernel_.reset(new TetGenTetMesher());
#endif
    meshing_.reset(new MeshingController(scene, stack, volumeMeshKernel_.get()));
    if (project_ != nullptr) {
        vesselProfile_.reset(new VesselProfileController(project_, stack));
        pathModules_.reset(new PathModuleController(project_));
        centerlineB_.reset(new CenterlineBController(
            project_, stack, centerlineSkeletonizer_.get()));
    } else {
        vesselProfile_.reset();
        pathModules_.reset();
        centerlineB_.reset();
    }
#if XQ_ENABLE_FLOW
    if (hasFlowCapability()) {
        flow_.reset(new FlowController(scene, stack));
        if (project_ != nullptr) {
            flowSmoke_.reset(new FlowSmokeController(project_, stack));
        } else {
            flowSmoke_.reset();
        }
    } else {
        flow_.reset();
        flowSmoke_.reset();
    }
#endif
    ai_.reset(new AiController(scene, stack));
}

FlowController* XQWorkflowSession::flowController() const
{
#if XQ_ENABLE_FLOW
    return flow_.get();
#else
    return nullptr;
#endif
}

FlowSmokeController* XQWorkflowSession::flowSmokeController() const
{
#if XQ_ENABLE_FLOW
    return flowSmoke_.get();
#else
    return nullptr;
#endif
}

bool XQWorkflowSession::pushCommand(std::unique_ptr<XQCommand> command)
{
    if (stack_ == nullptr) {
        return false;
    }
    const bool ok = stack_->push(std::move(command));
    if (ok && sceneChanged_) {
        sceneChanged_();
    }
    return ok;
}

bool XQWorkflowSession::undo()
{
    if (stack_ == nullptr) {
        return false;
    }
    const bool ok = stack_->undo();
    if (ok && sceneChanged_) {
        sceneChanged_();
    }
    return ok;
}

bool XQWorkflowSession::redo()
{
    if (stack_ == nullptr) {
        return false;
    }
    const bool ok = stack_->redo();
    if (ok && sceneChanged_) {
        sceneChanged_();
    }
    return ok;
}

void XQWorkflowSession::setSceneChangedCallback(std::function<void()> cb)
{
    sceneChanged_ = std::move(cb);
}

} // namespace xq
