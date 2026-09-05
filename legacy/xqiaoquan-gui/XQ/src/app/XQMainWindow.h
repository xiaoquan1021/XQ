#ifndef XQ_APP_XQ_MAIN_WINDOW_H
#define XQ_APP_XQ_MAIN_WINDOW_H

#include "app/WorkflowCapabilities.h"
#include "app/XQTaskRunner.h"
#include "core/NodeId.h"
#include "core/XQDerivationStamp.h"
#include "core/XQDomainType.h"
#include "core/XQPath.h"
#include "core/segmentation/ILevelSetSegmenter.h"
#include "services/segmentation/ContourExtractionService.h"

#include <QMainWindow>
#include <QPointF>
#include <QString>
#include <QVector>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class QAction;
class QActionGroup;
class QComboBox;
class QDoubleSpinBox;
class QDockWidget;
class QEvent;
class QLabel;
class QLineEdit;
class QMenu;
class QModelIndex;
class QPushButton;
class QShowEvent;
class QSlider;
class QSpinBox;
class QSortFilterProxyModel;
class QStackedWidget;
class QTabWidget;
class QTimer;
class QToolBar;
class QToolButton;
class QTranslator;
class QTreeView;
class QWidget;

namespace xq {

class XQImageVolume;
class XQMemoryImageBufferHandle;
class XQPayload;
class XQScene;
class XQSceneModel;
class XQCommandStack;
class XQWorkflowSession;
class XQRenderScene;
class AssetRegistry;
class GeometryResourceManager;
class XQMprWidget;
class XQCrossSectionViewWidget;
enum class DrawMethod;
enum class ContourType;
struct XQDemoVolume;
struct XQAppStartupState;
class XQProject;
class PathController;
class SegmentationController;
class ModelingController;
class MeshingController;
class FlowController;
class FlowSmokeController;
class VesselProfileController;
class PathModuleController;
class CenterlineBController;
class AiController;
class ITetMesher;
class XQDataNode;
class NodeComboBox;
struct XQContour;

class XQMainWindow : public QMainWindow {
    // Q_OBJECT is required for runtime i18n: tr() resolves to the namespaced
    // class translation context ("xq::XQMainWindow"). AUTOMOC (xq_app_shell)
    // handles the moc step.
    Q_OBJECT
public:
    explicit XQMainWindow(const XQScene* scene = nullptr, QWidget* parent = nullptr);
    explicit XQMainWindow(WorkflowCapabilities capabilities,
                          const XQScene* scene = nullptr,
                          QWidget* parent = nullptr);
    ~XQMainWindow() override;

    void setScene(const XQScene* scene);

    // Attaches a mutable scene + command stack and wires the available stage
    // controllers + the right-hand stage panel + Edit undo/redo. The const
    // setScene() path (used by test_main_window) is unaffected: callers that
    // only display a scene need not attach a workflow. The window does not own
    // the scene or the stack; the caller keeps them alive.
    void attachWorkflow(XQScene* scene, XQCommandStack* stack);
    void attachWorkflow(XQProject* project, XQCommandStack* stack);

    // Attaches the project asset registry/root needed to resolve lazy geometry
    // payloads stamped with geometryAssetId. The window borrows the registry;
    // callers keep the owning project alive, matching attachWorkflow().
    void attachGeometryResources(const AssetRegistry* registry,
                                 const std::string& assetRootDir);

    // Test/UI-readonly view of the geometry residency manager (null until
    // attachGeometryResources with a non-empty asset root).
    const GeometryResourceManager* geometryResourceManager() const;

    // Test-only probes into the resident render scene, exposing the incremental
    // sync outcome (B4b) that has no UI surface otherwise: whether a node has an
    // assembled actor, and its uploaded point count (-1 for an unknown node). Used
    // by test_main_window to assert that an unrelated command does not evict a
    // resident node and that a removed node leaves no actor.
    bool renderSceneHasNode(const NodeId& id) const;
    long long renderSceneUploadedPointCount(const NodeId& id) const;

    // Controller accessors (valid only after attachWorkflow). The shell widgets
    // gather input and drive these; tests can drive them headlessly too.
    PathController* pathController() const;
    SegmentationController* segmentationController() const;
    ModelingController* modelingController() const;
    MeshingController* meshingController() const;
    VesselProfileController* vesselProfileController() const;
    PathModuleController* pathModuleController() const;
    CenterlineBController* centerlineBController() const;
    FlowController* flowController() const;
    FlowSmokeController* flowSmokeController() const;
    bool hasFlowCapability() const;
    AiController* aiController() const;

    // Test probe: the resident task runner (valid only after attachWorkflow).
    // Tests connect its taskStarted signal to count background parse turns and
    // assert the B4c batch-merge (one turn per same-kind batch, not per file).
    XQTaskRunner& taskRunner();

    // Edit actions exposed for testing and menu wiring.
    void undo();
    void redo();

    // Dialog-free load entry points used by tests and automation. They execute
    // the same real-reader + command-stack paths as the File menu actions.
    bool loadImageFromPath(const QString& path);
    // Queues production GDCM/ITK DICOM import for one explicitly selected
    // SeriesInstanceUID. The resulting metadata-only Image node/ExternalSource
    // asset and project-scoped voxel residency are committed through the same
    // service/command path used by the interactive File action.
    bool loadDicomSeriesFromDirectory(const QString& directory,
                                      const QString& seriesInstanceUid);
    bool loadSvProjectFromDirectory(const QString& dir);

    // Test-only: appends a world-space path control point exactly like an MPR
    // pick in PathPoint mode would (draft + page refresh). Dialog-free tests
    // drive the path stage through this.
    void appendPathDraftPointForTest(const Point3& world);

    // Test-only entry into the exact semantic contour-append tail used by the
    // section workbench. It avoids offscreen drawing gestures while preserving
    // command-stack revision, stale, undo, and redo behavior.
    NodeId appendContourToGroupForTest(
        const NodeId& contourGroupId,
        XQContour contour);

    // Test-only: runs exactly what the Ctrl+A shortcut does -- reads the current
    // crosshair voxel (per-axis sliceIndex) and adds a path draft point there.
    // Offscreen ctest cannot reliably route a window shortcut, so tests exercise
    // this same code path directly. Returns the resulting draft point count (0 if
    // no volume / not applicable).
    std::size_t addPathDraftAtCrosshairForTest();

    // Test-only: runs exactly what the Esc shortcut does -- leaves any active
    // picking mode. Offscreen ctest cannot reliably route a window shortcut.
    void exitPickingForTest();

    // Test-only: runs the same jump the scene right-click "Model from" / "Mesh
    // from" actions do -- raise the consuming stage page and preselect `node`.
    // Modeling for a ContourGroup, Meshing for a SurfaceModel; a no-op for any
    // other domain. Dialog-free tests use this because the right-click QMenu's
    // exec() cannot be driven offscreen.
    void activateStageFromNodeForTest(const NodeId& id, XQDomainType domain);

    // Dialog-free workspace entry points (same real reader/writer paths as the
    // File menu actions; tests drive these directly).
    // Opens a .xqproj (lazy geometry), replacing the current workflow state on
    // success; on failure the current state is left fully intact.
    bool openWorkspaceFromPath(const QString& path);
    // Saves the live project to `path`. When saving into a different assets
    // directory, first copies every registry-referenced blob so the archive
    // stays self-contained (lazy-loaded nodes keep their asset ids and are
    // never re-derived by the writer).
    bool saveWorkspaceFile(const QString& path);

    // Records the startup state whose scene main() attached, so save/open can
    // reach the owning project without owning it. Pass nullptr to clear.
    void setExternalStartupState(XQAppStartupState* state);

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    // Re-applies all user-visible strings when the active translator changes
    // (QEvent::LanguageChange), so the Language menu can switch zh/en at runtime.
    void changeEvent(QEvent* event) override;

private:
    void buildMenus();
    void buildMainToolBar();
    void buildDataManagerDock();
    void buildImageNavigatorDock();
    void buildStatusBar();
    void updateMemoryStatus();
    void buildStagePanel();
    void buildEditMenu();
    void buildWindowMenu();
    void applyProductDockLayout();
    void refreshSceneTree();
    // Visibility-driven scene composition: walks the live scene and upserts every
    // renderable node into the resident render scene (removing nodes that left),
    // then repaints. Replaces the old "render the single selected node" model.
    // Called from refreshSceneTree() (the single integration point covering load
    // / command push / undo / redo).
    void syncRenderScene();
    // Background geometry pre-parsing (B2a/B3): after an SV project loads, its
    // surface-model (.mdl) and contour-group (.ctgr) nodes carry an unresolved
    // XQSourcePayload. startNextParse pops one job off pendingParses_ and runs the
    // matching reader (MDLModelReader / CTGRContourReader) on the task runner's
    // worker thread; the commit swaps in the parsed payload (if the node still
    // exists / is still unresolved) so the geometry appears with no user action,
    // then kicks the next one. One shared queue keeps the runner strictly single
    // task, so .mdl and .ctgr jobs never race each other for the idle window.
    void startNextParse();
    // Global busy matrix: while a background task runs, disables the stage panel
    // and every action that could start work or mutate the scene, and shows a
    // running indicator; restores everything when it finishes.
    void setWorkflowBusy(bool busy, const QString& label);

    // Single point that (re)sets every user-facing string: menu titles, action
    // text, dock titles, toolbar button text, placeholders and status text.
    // Called once at construction and again on every LanguageChange event.
    void retranslateUi();
    // Switches the active language by installing/removing the zh translator and
    // keeps the Language menu radio state in sync.
    void setChineseLanguage(bool chinese);

    // Opens a real .vti image (R1) via file dialog, then delegates to
    // loadImageFromPath().
    void openImageFile();
    // Selects a DICOM directory, discovers safe series descriptors on the
    // resident worker, asks the user to choose one, then delegates to the
    // explicit-UID dialog-free import entry point above.
    void openDicomSeries();
    // Resolves a persisted metadata-only DICOM Image node through its bound
    // ExternalSource asset and project-scoped ImageResourceResolver.
    void queuePersistedDicomImage(const XQDataNode& node);
    // Opens a SimVascular project (R1/AC2) via directory dialog, then delegates
    // to loadSvProjectFromDirectory().
    void openSvProject();
    // Points the MPR view + navigator sliders at the given volume/buffer and
    // raises the MPR workspace. Shared by demo + real-image paths.
    void useVolume(const XQImageVolume& image, const XQMemoryImageBufferHandle* buffer);
    // Re-points one MPR axis to a navigator slider value.
    void onNavigatorSliderChanged();
    // Refreshes the three Loc.(mm) spins from the current slice-plane world
    // coordinates (signals blocked to avoid a feedback loop). No-op with no volume.
    void syncLocReadout();
    // A Loc.(mm) spin edit: maps the three world values to voxel indices and drives
    // the matching slice sliders (which re-slice the views); clamps back on a
    // read-back when the world point falls outside the volume.
    void onNavLocEdited();
    // Writes the status-bar "Position: <x, y, z> mm" read-out from the current
    // slice-plane world coordinates. Only while idle (statusShowingIdle_): busy /
    // loading messages own the label then. Does not itself change the idle flag --
    // the position is the resident idle read-out. No-op with no volume.
    void updatePositionReadout();
    void showAboutDialog();
    void showPreferencesDialog();
    // Restores the default dock arrangement (re-shows and re-docks all panels).
    void resetDockLayout();
    // Fits the default product window to the current screen before it is shown.
    void resizeToProductDefault();
    // Persists / restores window geometry across sessions.
    void saveWindowState();
    void restoreWindowState();
    // Applies persisted preferences (default layout, crosshair, slice step) to
    // the live widgets. Called at construction and after the dialog accepts.
    void applyPreferences();
    // Steps the Axial slice by +/- the configured step (keyboard shortcuts).
    void stepAxialSlice(int delta);
    // Appends a path control point at voxel (i, j, k): maps it to world space and
    // pushes it onto the draft, then refreshes the Path page. Shared by the MPR
    // seedPicked handler (PathPoint mode) and the Ctrl+A shortcut so the two never
    // drift. A no-op with no active image or on a failed voxel->world transform.
    void addPathDraftVoxel(int i, int j, int k);
    // Pushes the current draft control points (world space) to the render scene so
    // the slice views show a marker per point, then repaints. Called on every
    // draft change (add / remove / clear) so the markers track the list live. A
    // no-op with no render scene.
    void syncPathControlMarkers();
    // Centres the three MPR slice planes on draft control point `index` (double-
    // click a list row to jump to it). Out-of-range index / no volume is a no-op.
    void focusPathDraftPoint(int index);
    // Leaves any active picking mode (seed or path): flips pickMode_ back to None
    // via the matching setter so the MPR stops routing clicks and the page toggle
    // resets. A no-op when not picking. Bound to Esc.
    void exitPicking();
    // Raises the matching stage panel page (index into the six workflow stages);
    // a no-op until attachWorkflow has built the panel.
    void showStagePage(int index);
    // Opens the independent two-dimensional path-contour workbench. The primary
    // Segmentation toolbar action remains the three-dimensional ROI-v2 entry.
    void showContourWorkbench();
    // Refreshes a stage page's scene-node picker and preselects `id` (if present).
    // pageIndex is the stage index (2 = Modeling, 3 = Meshing, ...); used by the
    // scene right-click "operate from this node" actions and by selection follow.
    // A no-op if the page has no matching NodeComboBox.
    void preselectStageSource(int pageIndex, const NodeId& id);
    // Raises stage page `pageIndex` and preselects `id` in it: the single
    // implementation behind both the right-click "Model/Mesh from" actions and
    // activateStageFromNodeForTest, so the two never drift.
    void activateStageFromNode(int pageIndex, const NodeId& id);
    // Toggles the central MPR view between 2x2 and single-view layout.
    void toggleViewLayout();
    // Builds the cross-section workbench page (along-path slider + section view)
    // and adds it to centralStack_. Called once at construction.
    void buildCrossSectionWorkbench();
    // Builds crossSectionSegPanel_ (the independent two-dimensional contour
    // method panel) and wires its control signals. Called once at construction,
    // right after buildCrossSectionWorkbench(). The panel is parented to the
    // window so it never rides the stagePanel_ rebuild.
    void buildCrossSectionSegPanel();
    // Points the section view + along-path slider at a bound path: walks the
    // scene for the first Path node (P3-1 has no path picker yet), caches its
    // sample points, ranges the slider to [0, M-1], and reslices at the current
    // index. Clears to the empty state when there is no path / no volume. Called
    // when the contour-extraction stage is entered.
    void bindCrossSectionPath();
    // Reslices the section view at sample index `index` and updates the position
    // read-out ("point N / M, arc length X mm"). A no-op when the index is out of
    // range for the cached samples.
    void updateCrossSectionAtSample(int index);
    // Creates a new contour group node bound to the path selected in the section
    // top bar's path picker, sets it active, and rebinds the section to it. A
    // no-op (status hint) when no path is selected / no mutable scene.
    void createContourGroupFromPicker();
    // Enables/disables the method + edit controls based on whether a bound path
    // and an active contour group exist, and refreshes the path picker list.
    void updateContourWorkbenchState();
    // Repaints the section's committed-contour overlay from the active group's
    // contours at (near) the current arc length. A no-op with no active group.
    void refreshSectionContourOverlay();
    // Maps section-local 2D control points (from the section view's contourDrawn
    // signal) to world space and adds an XQContour to the active group at the
    // current arc length: circle control points -> ContourExtractionService
    // circle, polygon control points -> polygon, then unprojectFromFrame. A no-op
    // with no active group / degenerate input.
    void addDrawnContour(DrawMethod method, const QVector<QPointF>& points);
    // Copy-on-write semantic edit shared by manual/automatic section contours
    // and the dialog-free regression entry point.
    NodeId appendContourToGroup(
        const NodeId& contourGroupId,
        XQContour contour);
    // Adds an XQContour built from a section-local 2D (u, v) mm loop to the
    // active group at the current arc length: takes lastFrame() as the contour
    // frame, unprojectFromFrame each point, tags it `type`, then refreshes the
    // tree + overlay. Shared tail of addDrawnContour and
    // runThresholdOnCurrentSection. A no-op with no active group / fewer than 3
    // points.
    void addContourFromSection2D(const std::vector<ContourPoint2D>& pts2d,
                                 ContourType type);
    // Auto threshold segmentation on the current section: reads the section
    // grayscale, maps the threshold slider to an intensity, traces the vessel
    // lumen iso-contour constrained to the section-center seed via
    // ContourExtractionService::thresholdContour, and adds it as a
    // ThresholdResult contour. A no-op (status hint) with no section / no group /
    // no closed loop.
    void runThresholdOnCurrentSection();
    // Computes (but does not add) the threshold contour for the current section
    // at the current slider intensity. Fills `pts2d` (section-local 2D loop) and
    // reports the intensity actually used in `usedThreshold`. Returns false with
    // no resliced section. Shared by the live preview and the commit path so both
    // trace at exactly the same intensity. The slider (0..1000) maps linearly to
    // the section's actual [min, max] value range (SV uses the image intensity
    // range, not a fixed percentile), so the label can show a real intensity.
    bool previewThresholdContour(std::vector<ContourPoint2D>& pts2d,
                                 double& usedThreshold);
    // Slider-drag slot: re-traces the threshold contour live and shows it as a
    // preview overlay on the section (not added to the group until the Threshold
    // button is clicked), and updates the label to the real intensity. Lets the
    // user scrub the slider and watch the lumen contour grow/shrink to find a
    // good threshold -- the fixed global threshold is otherwise hard to guess on
    // non-uniform medical images.
    void updateThresholdPreview();
    // Auto region-grow segmentation on the current section: reads the section
    // grayscale, region-grows from the current seed with the band slider's
    // half-width, traces the region boundary via
    // ContourExtractionService::regionGrowContour, and adds it as a LevelSetResult
    // contour. A no-op (status hint) with no section / no group / no closed loop.
    void runRegionGrowOnCurrentSection();
    // Computes (but does not add) the region-grow contour for the current section
    // at the current band. Fills `pts2d` (section-local 2D loop) and reports the
    // band actually used. Returns false with no resliced section. Shared by the
    // live preview and the commit path so both trace identically.
    bool previewRegionGrowContour(std::vector<ContourPoint2D>& pts2d, double& usedBand);
    // Slider-drag slot: re-traces the region-grow contour live and shows it as a
    // preview overlay (not added until the Region grow button is clicked), and
    // updates the band label. Lets the user scrub the band and watch the region
    // grow/shrink.
    void updateRegionGrowPreview();
    // Auto level-set segmentation on the current section (P3-5): reads the section
    // grayscale, runs the SimVascular ITK vascular two-phase level set from the
    // current seed with the slider's iteration count (via the injected
    // ILevelSetSegmenter), traces the zero level set, and adds it as a
    // LevelSetResult contour. A no-op (status hint) with no section / no group /
    // no closed loop.
    void runLevelSetOnCurrentSection();
    // Computes (but does not add) the level-set contour for the current section at
    // the current iteration count. Fills `pts2d` and reports the iteration count
    // used. Returns false with no resliced section. Shared by the (release-driven)
    // preview and the commit path so both evolve identically.
    bool previewLevelSetContour(std::vector<ContourPoint2D>& pts2d, int& usedIterations);
    // Slider-RELEASE slot (not valueChanged -- one evolution is expensive): runs a
    // full level-set evolution once and shows the result as a preview overlay (not
    // added until the Level set button is clicked), and updates the iteration
    // label. Dragging the slider does NOT re-evolve; only release does.
    void updateLevelSetPreview();
    // Stores a clicked seed point (section-local (u,v) mm) as the current
    // segmentation seed and refreshes both auto-method previews so the user sees
    // the contour re-trace from the new seed. Leaves seed-pick mode.
    void onSeedPicked(double u, double v);
    // Returns one id past every Scene node id and every nested contour id (or a
    // fixed base when none exist). Workbench-created nodes and contour evidence
    // share this allocator so neither namespace can collide later.
    NodeId allocateSceneObjectId() const;
    // Scene-tree right-click menu (Delete via the command stack).
    void onSceneContextMenu(const QPoint& pos);

    // File > Open/Save Workspace dialog slots (delegate to the dialog-free
    // entry points once a path is chosen).
    void openWorkspaceDialog();
    void saveWorkspaceDialog();
    // Resolves the project backing the live workflow scene (owned runtime state
    // or the borrowed startup state).
    XQProject* activeProject() const;
    // Shows a workspace error, suppressible headlessly via the object property
    // "xqSuppressDialogs" (tests set it to avoid modal deadlock offscreen).
    void reportWorkspaceError(const QString& title, const QString& text);
    void finishWorkflowAttach(XQScene* scene);

    // Selection-driven rendering: maps the current scene-tree selection to its
    // node payload and renders it in the interactive view. Pure dispatch -- all
    // rendering logic lives in XQSceneRenderer.
    void onSceneSelectionChanged(const QModelIndex& current, const QModelIndex& previous);
    // Enables + back-fills the Data Manager opacity/colour controls for the given
    // node (disabled + reset when null or not a renderable node). Sets
    // presentationNodeId_ to the acted-on node (invalid when disabled).
    void updatePresentationControls(const XQDataNode* node);
    const XQDataNode* selectedSceneNode() const;
    const XQDataNode* nodeForRequestedOrSelected(const NodeId& requested) const;
    QString resolveSourcePath(const std::string& sourcePath) const;

    XQSceneModel* sceneModel_;
    QSortFilterProxyModel* sceneFilter_ = nullptr;
    QTreeView* sceneTreeView_;
    QLineEdit* sceneSearchEdit_ = nullptr;
    // Data Manager presentation controls, acting on the currently selected node:
    // opacity slider (0-100 %) + colour button (QColorDialog). Enabled only when a
    // renderable node is selected. (B3: 07-03 B2 deleted these; this restores them
    // wired to the render scene's setNodeOpacity/setNodeColor.)
    QSlider* opacitySlider_ = nullptr;
    QPushButton* colorButton_ = nullptr;
    QLabel* opacityLabel_ = nullptr;
    QLabel* colorLabel_ = nullptr;
    // The node the presentation controls currently act on (invalid when none).
    NodeId presentationNodeId_;
    QTabWidget* workspaceTabs_ = nullptr;
    QStackedWidget* centralStack_;
    // Resident render scene (data uploaded once, four persistent renderers) +
    // the 2x2 four-view widget mounting them. renderScene_ is declared before
    // mprWidget_: the widget's cells mount the scene's renderers at construction,
    // so the scene must outlive the widget (members destroy in reverse order).
    std::unique_ptr<XQRenderScene> renderScene_;
    XQMprWidget* mprWidget_ = nullptr;

    // Cross-section workbench (P3-1): a second centralStack_ page shown while the
    // contour-extraction stage is active. Top = along-path slider + position
    // read-out; center = the resident-reslice section view. Building it never
    // touches the four-view render path. crossSectionView_ is borrowed by
    // crossSectionWorkbench_ (a child of the stack), destroyed with the window.
    QWidget* crossSectionWorkbench_ = nullptr;
    // Vertical panel holding the independent two-dimensional contour controls
    // (P3-5b). Owned by the window (parent = this) and NOT part of stagePanel_,
    // so it survives every attachWorkflow rebuild. showContourWorkbench() swaps
    // it into stageDock_; the Segmentation toolbar action restores stagePanel_.
    QWidget* crossSectionSegPanel_ = nullptr;
    XQCrossSectionViewWidget* crossSectionView_ = nullptr;
    QSlider* alongPathSlider_ = nullptr;
    QLabel* alongPathLabel_ = nullptr;
    // Sample points of the path currently driving the section view (empty when no
    // path is bound). Cached so the slider maps an index to a pose without a
    // scene walk per tick.
    std::vector<PathSamplePoint> sectionPathSamples_;

    // Contour-extraction workbench controls (P3-2). The path picker + "new group"
    // button live in the section top bar (app layer so they reach the scene /
    // node lister directly); the method / edit toggles drive the section view's
    // drawing layer. Kept for enable/disable + retranslate.
    NodeComboBox* contourPathCombo_ = nullptr;
    QPushButton* contourGroupCreateBtn_ = nullptr;
    QToolButton* contourEditToggle_ = nullptr;
    QToolButton* contourMethodCircleBtn_ = nullptr;
    QToolButton* contourMethodPolygonBtn_ = nullptr;
    // Auto threshold segmentation (P3-3): the "Threshold" method sits first in the
    // method row (automatic-first over the manual drawing tools) and runs on
    // click (not a persistent draw mode). The slider maps to the section's value
    // range; its value label previews the current threshold.
    QToolButton* contourMethodThresholdBtn_ = nullptr;
    QSlider* thresholdSlider_ = nullptr;
    QLabel* thresholdLabel_ = nullptr;
    // Auto region-grow segmentation (P3-4): "Region grow" method sits after
    // Threshold in the automatic-first method row and runs on click. The band
    // slider controls the seed-value inclusion half-width; its label previews
    // the current band. The Seed button toggles the section into seed-pick mode.
    QToolButton* contourMethodRegionGrowBtn_ = nullptr;   // objectName xqContourMethodRegionGrow
    QSlider* regionGrowBandSlider_ = nullptr;             // objectName xqRegionGrowBandSlider
    QLabel* regionGrowBandLabel_ = nullptr;
    QToolButton* contourSeedPickBtn_ = nullptr;           // objectName xqContourSeedPick, checkable
    // Auto level-set segmentation (P3-5): "Level set" method sits after Seed in
    // the automatic-first method row and runs on click. The iteration slider
    // controls the Chan-Vese evolution step count; its label previews the count.
    // Because one evolution is expensive, the slider re-evolves on RELEASE only
    // (not on every drag), unlike the threshold / band sliders.
    QToolButton* contourMethodLevelSetBtn_ = nullptr;   // objectName xqContourMethodLevelSet
    QSlider* levelSetIterSlider_ = nullptr;             // objectName xqLevelSetIterSlider
    QLabel* levelSetIterLabel_ = nullptr;
    // Current segmentation seed in section-local (u, v) mm (section center (0,0)
    // by default; set by clicking in seed-pick mode). Used by both threshold and
    // region-grow. Reset to (0,0) when the section plane changes.
    ContourPoint2D sectionSeed_{0.0, 0.0};
    // Injected ITK vascular two-phase level-set segmenter (adapter, batch 3):
    // backs the level-set contour button. Owns no Qt/VTK state; ITK lives inside
    // the adapter. Constructed in the ctor; previewLevelSetContour drives it.
    std::unique_ptr<ILevelSetSegmenter> levelSetSegmenter_;
    // The contour group hand-drawn contours are added to (invalid until a group
    // is created / selected in the workbench). Drives bindCrossSectionPath's
    // source-path lookup and the contourDrawn add path.
    NodeId activeContourGroup_;

    // Incremental render-sync fingerprint (B4b): the payload identity currently
    // assembled on the render side, per node. syncRenderScene compares each
    // renderable node's live payload pointer against this map and skips the
    // expensive upsert (detach + rebuild, which re-runs surface decimation) when
    // it is unchanged, removing nodes that left the scene. The pointers are used
    // ONLY for identity comparison -- never dereferenced -- so a stale pointer is
    // harmless: a payload change always swaps the shared_ptr (setPayload / clone),
    // so a matching pointer proves the assembled geometry is still current.
    std::unordered_map<NodeId, const XQPayload*> syncedPayloads_;

    // Image Navigator slice sliders and synchronized read-outs.
    QSlider* axialSlider_ = nullptr;
    QSlider* sagittalSlider_ = nullptr;
    QSlider* coronalSlider_ = nullptr;
    QSpinBox* axialSpin_ = nullptr;
    QSpinBox* sagittalSpin_ = nullptr;
    QSpinBox* coronalSpin_ = nullptr;
    QLabel* statusPositionLabel_ = nullptr;
    QLabel* statusNodeCountLabel_ = nullptr;
    QLabel* statusMemLabel_ = nullptr;
    QTimer* memoryTimer_ = nullptr;

    QLabel* navAxialLabel_ = nullptr;
    QLabel* navSagittalLabel_ = nullptr;
    QLabel* navCoronalLabel_ = nullptr;

    // Window/level numeric boxes, two-way bound to the render scene's shared
    // image property.
    QDoubleSpinBox* windowSpin_ = nullptr;
    QDoubleSpinBox* levelSpin_ = nullptr;
    QLabel* navWindowLabel_ = nullptr;
    QLabel* navLevelLabel_ = nullptr;

    // World-space location (mm) of the current crosshair / slice intersection, two
    // way bound to the three slice sliders. X/Y/Z map to render-scene axis 0/1/2
    // (Sagittal/Coronal/Axial). Disabled until a real image loads.
    QDoubleSpinBox* navLocXSpin_ = nullptr;
    QDoubleSpinBox* navLocYSpin_ = nullptr;
    QDoubleSpinBox* navLocZSpin_ = nullptr;
    QLabel* navLocLabel_ = nullptr;

    // Docks, stage side panel + main toolbar, kept for the Window menu
    // (show/hide, reset) and session save/restore.
    QToolBar* mainToolBar_ = nullptr;
    QDockWidget* dataManagerDock_ = nullptr;
    QDockWidget* imageNavigatorDock_ = nullptr;
    QDockWidget* stageDock_ = nullptr;

    // Menus (kept so retranslateUi() can reset their titles).
    QMenu* fileMenu_ = nullptr;
    QMenu* editMenu_ = nullptr;
    QMenu* viewMenu_ = nullptr;
    QMenu* toolsMenu_ = nullptr;
    QMenu* windowMenu_ = nullptr;
    QMenu* helpMenu_ = nullptr;
    QMenu* languageMenu_ = nullptr;

    // Menu actions (kept for retranslateUi()).
    QAction* openAction_ = nullptr;
    QAction* openImageAction_ = nullptr;
    QAction* openDicomAction_ = nullptr;
    QAction* openSvProjectAction_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* quitAction_ = nullptr;
    QAction* prefsAction_ = nullptr;
    QAction* aboutAction_ = nullptr;
    QAction* layoutMenuAction_ = nullptr;
    QAction* fullScreenAction_ = nullptr;
    QAction* resetLayoutAction_ = nullptr;
    QAction* langZhAction_ = nullptr;
    QAction* langEnAction_ = nullptr;
    QActionGroup* languageGroup_ = nullptr;

    // Main toolbar actions (kept for retranslateUi()).
    QAction* tbOpenAction_ = nullptr;
    QAction* tbSaveAction_ = nullptr;
    QAction* tbUndoAction_ = nullptr;
    QAction* tbRedoAction_ = nullptr;
    QAction* tbImageAction_ = nullptr;
    QAction* tbPathAction_ = nullptr;
    QAction* tbSeg2dAction_ = nullptr;
    QAction* tbModelAction_ = nullptr;
    QAction* tbMeshAction_ = nullptr;
    QAction* tbModulesAction_ = nullptr;

    // Real image loaded from disk (R1). Holds the decoded volume + scalar buffer;
    // the MPR view borrows pointers into this, so it must outlive the view's use.
    // Reuses the XQDemoVolume struct shape (image + buffer); null until a real
    // .vti is opened.
    std::unique_ptr<XQDemoVolume> activeImage_;
    // Scene node id bound to activeImage_ (for mask/result provenance). Valid
    // only while activeImage_ is set.
    NodeId activeImageNodeId_;
    QString activeProjectDir_;

    // Pending background parse jobs (B2a/B3/B4b): surface-model (.mdl),
    // contour-group (.ctgr) and image (.vti) nodes queued after a project / file
    // load. Drained one at a time by startNextParse via the task runner
    // (single-task discipline), so the kinds share one serial queue and never
    // contend for the runner. Image decoding (large .vti read + decompress) used
    // to run synchronously on the GUI thread and froze the UI for hundreds of MB;
    // it now rides this same queue so the window stays responsive with a busy
    // indicator.
    enum class ParseKind { Model, Contour, Image };
    struct PendingParse {
        NodeId nodeId;
        QString path; // absolute path to the .mdl / .ctgr / .vti file
        ParseKind kind;
        // Model/contour jobs capture the unresolved source state when queued.
        // The GUI-thread commit rechecks all three fields so a worker started
        // from an old source cannot overwrite a later semantic edit, node
        // replacement, or source-path change. Image jobs do not replace the
        // node payload and leave these fields at their defaults.
        ContentRevision expectedRevision = 0;
        std::shared_ptr<const XQPayload> expectedSourcePayload;
        std::string expectedSourcePath;
        // Image jobs only: when true the commit seeds a fresh Image node into the
        // scene (File > Open Image) using displayName; when false it binds the
        // decoded volume to the already-present nodeId (SV project / node reload).
        bool seedImageNode = false;
        QString displayName; // Image seed jobs: the new node's display name
    };
    std::vector<PendingParse> pendingParses_;

    // MPR click routing for Path control-point picking. XQMprWidget retains its
    // generic seedPicked signal name, but the old 3-D region-grow consumer has
    // left the product entry.
    enum class PickMode { None, PathPoint };
    PickMode pickMode_ = PickMode::None;

    // Path-stage draft: world-space control points picked in the MPR. Owned by
    // the window (outlives stage-panel rebuilds); the Path page reads/mutates
    // it through the provider pair below.
    std::vector<PathControlPoint> pathDraftPoints_;
    // Hooks the Path page registers on (re)build: list refresher + toggle setter.
    std::function<void()> pathDraftChanged_;
    std::function<void(bool)> pathPickToggleSetter_;

    // Runtime zh<->en translator. Loaded from :/i18n/xq_zh_CN.qm and owned by
    // this window so the Language menu can install/remove one canonical instance.
    QTranslator* zhTranslator_ = nullptr;
    bool zhTranslatorReady_ = false;

    bool workflowBusy_ = false;

    const AssetRegistry* geometryRegistry_ = nullptr;
    // Owned startup state for workspaces opened through File > Open Workspace.
    // The state main() builds at launch stays owned by main(); the window only
    // borrows it (attachWorkflow contract). Opening a new workspace at runtime
    // creates a fresh state the window must own -- it replaces (and destroys)
    // the previous owned one.
    std::unique_ptr<XQAppStartupState> ownedState_;
    // Borrowed startup state main() attached (null once a runtime workspace is
    // owned). Used by activeProject() to reach the project for saving.
    XQProject* externalProject_ = nullptr;
    // Asset root dir of the live project (recorded by attachGeometryResources),
    // used by save-as to copy lazy-loaded blobs into a new archive.
    std::string assetRootDir_;
    QString workspacePath_;
    QStackedWidget* stagePanel_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;
    QAction* viewLayoutAction_ = nullptr;
    QAction* crosshairAction_ = nullptr;
    QAction* planes3dAction_ = nullptr;
    int sliceStep_ = 1;
    bool showStatusCoordinates_ = true;
    // True while the position label shows the idle "Ready" text (not a live
    // slice read-out). Lets retranslateUi() relabel it on a language switch
    // without clobbering a live coordinate string.
    bool statusShowingIdle_ = true;

    std::unique_ptr<GeometryResourceManager> geometryResources_;

    // Workflow wiring: scene + command stack + stage controllers (Flow backend optional)
    // + mesh kernel
    // + resident task runner, all bundled and lifetime-managed by the session.
    // Constructed once (taskRunner lives for the window's whole life); attach()
    // only rebinds scene/stack + rebuilds controllers. Command-stack mutations go
    // through session_->pushCommand/undo/redo so each one fires exactly one
    // refreshSceneTree (the unified render-sync point).
    //
    // Declared LAST on purpose: members destroy in reverse declaration order, so
    // the session (whose task runner joins its worker thread in the destructor)
    // is destroyed FIRST -- before ownedState_ (which owns the scene/stack a
    // running job may have captured) and before geometryResources_. This
    // preserves the pre-B4 contract the loose taskRunner_ member carried.
    std::unique_ptr<XQWorkflowSession> session_;
};

} // namespace xq

#endif // XQ_APP_XQ_MAIN_WINDOW_H
