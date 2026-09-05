#ifndef XQ_UI_PANELS_STAGE_WIDGETS_H
#define XQ_UI_PANELS_STAGE_WIDGETS_H

#include "core/NodeId.h"
#include "core/XQContourGroup.h"
#include "core/XQDomainType.h"
#include "core/XQPath.h"
#include "core/command/XQCommand.h"

#include <QComboBox>
#include <QString>

#include <functional>
#include <memory>
#include <vector>

class QStackedWidget;
class QWidget;

namespace xq {

class PathController;
class SegmentationController;
class ModelingController;
class MeshingController;
class VesselProfileController;
class PathModuleController;
class CenterlineBController;
class AiController;
class XQImageVolume;
class XQMemoryImageBufferHandle;

// Supplies the currently active image to stage pages that need real voxel data
// (e.g. segmentation). The main window owns the decoded volume + buffer; the
// panels only borrow them at execute time via this provider, so they never hold
// large data or depend on the main window's concrete type. valid == false means
// no image is loaded (the page keeps its execute control disabled + prompts the
// user to open / select an image).
struct ActiveImage {
    bool valid = false;
    const XQImageVolume* image = nullptr;
    const XQMemoryImageBufferHandle* buffer = nullptr;
    // Shared keepalive for the scalar buffer (async jobs copy this so the
    // buffer outlives an activeImage_ swap mid-task). Null in legacy callers;
    // then `buffer` must only be used synchronously.
    std::shared_ptr<XQMemoryImageBufferHandle> bufferShared;
    NodeId nodeId; // the image node bound to this volume (for mask provenance)
};
using ActiveImageProvider = std::function<ActiveImage()>;

// Path-stage draft plumbing (B4). The window owns the draft (world-space
// control points picked in the MPR); the page renders it and mutates it.
using PathDraftProvider = std::function<std::vector<PathControlPoint>()>;
// removeIndex >= 0 removes that point; removeIndex < 0 clears the draft.
using PathDraftMutator = std::function<void(int removeIndex)>;
// Centres the MPR crosshair on draft point `index` (double-click the list to jump
// to a control point). Out-of-range index is ignored.
using PathDraftFocuser = std::function<void(int index)>;
// Closures the Path page registers with the window on (re)build.
struct PathPageHooks {
    std::function<void()> refreshList;        // draft changed -> re-render list
    std::function<void(bool)> setPickChecked; // window forces the toggle state
};
// enabled: toggle MPR path picking. hooks: re-registered on every call.
using PathPickingSetter = std::function<void(bool enabled, PathPageHooks hooks)>;

struct ActiveContourGroup {
    bool valid = false;
    NodeId nodeId;
    XQContourGroup contourGroup;
};
using ContourGroupProvider = std::function<ActiveContourGroup(NodeId requestedNode)>;

using VesselProfileOutputIdProvider = std::function<NodeId()>;

struct CenterlineBOutputIds {
    bool valid = false;
    NodeId pathNode;
    NodeId profileNode;
};
using CenterlineBOutputIdProvider = std::function<CenterlineBOutputIds()>;

// One scene node offered in a stage-page node picker: its id plus display name.
struct SceneNodeOption {
    NodeId id;
    QString name;
};
// Enumerates the scene nodes of a given domain (id + name), ascending by id.
// Empty vector when no scene is attached. Lets the stage pages populate a node
// dropdown instead of asking the user to type an internal NodeId.
using SceneNodeLister = std::function<std::vector<SceneNodeOption>(XQDomainType)>;

// A scene-node picker for a single domain, used by the Modeling / Meshing /
// Modules / AI pages instead of a raw NodeId spin box. It re-queries the lister on
// every showPopup() (and on demand via repopulate()) so newly created nodes
// appear without a panel rebuild; the current selection is preserved when its id
// is still present. Each item's userData holds the NodeId value (qulonglong).
// No Q_OBJECT: it only overrides the virtual showPopup(), adding no new
// signals/slots, so it needs no moc.
class NodeComboBox : public QComboBox {
public:
    NodeComboBox(SceneNodeLister lister, XQDomainType domain, QWidget* parent = nullptr);

    // Re-reads the lister and rebuilds the item list, keeping the current node
    // selected if it still exists. Public so the window can refresh + preselect
    // a page's picker on a right-click "operate from this node" action.
    void repopulate();

    void showPopup() override;

private:
    SceneNodeLister lister_;
    XQDomainType domain_;
};

using StageChangedCallback = std::function<void()>;
using ContourWorkbenchOpener = std::function<void()>;

// A stage command computed off the GUI thread: the job runs controller
// prepare*() and maps the status to the page's user-facing message; the shell
// commits the command on the GUI thread and reports back through `done`.
struct StageCommandOutcome {
    std::unique_ptr<XQCommand> command; // null on failure
    // Optional guarded commit for prepared results that must be revalidated on
    // the GUI/owner thread rather than reduced to a raw command on the worker.
    std::function<bool(QString* message)> ownerCommit;
    QString message;                    // status text for the page label
};
using StageCommandJob = std::function<StageCommandOutcome()>;
// Returns false when a task is already running (the page then reports "busy").
using AsyncCommandRunner =
    std::function<bool(const QString& label, StageCommandJob job,
                       std::function<void(bool ok, const QString& message)> done)>;

// Everything a stage page needs from the shell, bundled: the stage controllers
// plus the provider/callback hooks the window used to pass as ten trailing
// std::function parameters. A pure ui-layer aggregate (no app-layer types), so
// the panels stay decoupled from XQMainWindow / XQWorkflowSession. Controllers
// may be null (e.g. before attachWorkflow); a page with a null controller shows
// its controls disabled. Providers may be empty; a page whose data provider is
// empty keeps its execute control disabled (back-compat).
struct StagePanelContext {
    PathController* path = nullptr;
    SegmentationController* segmentation = nullptr;
    ModelingController* modeling = nullptr;
    MeshingController* meshing = nullptr;
    VesselProfileController* vesselProfile = nullptr;
    PathModuleController* pathModules = nullptr;
    CenterlineBController* centerlineB = nullptr;
    AiController* ai = nullptr;

    ActiveImageProvider imageProvider;
    ContourGroupProvider contourGroupProvider;
    VesselProfileOutputIdProvider vesselProfileOutputIdProvider;
    CenterlineBOutputIdProvider centerlineBOutputIdProvider;
    SceneNodeLister nodeLister;
    StageChangedCallback stageChanged;
    ContourWorkbenchOpener openContourWorkbench;
    PathDraftProvider pathDraftProvider;
    PathDraftMutator pathDraftMutator;
    PathDraftFocuser pathDraftFocuser;
    PathPickingSetter pathPickingSetter;
    AsyncCommandRunner asyncRunner;
};

// Builds the six workflow stage pages into `panel` (a QStackedWidget),
// in this exact order: Path, Segmentation, Modeling, Meshing, Modules, AI.
// Each page is a rich operation form whose controls drive the matching
// controller (which routes through the command stack). Adds exactly 6 pages to
// `panel`. See StagePanelContext for the null-controller / empty-provider
// contract.
void populateStagePanels(QStackedWidget* panel, const StagePanelContext& context);

} // namespace xq

#endif // XQ_UI_PANELS_STAGE_WIDGETS_H
