#include "app/XQMainWindow.h"

#include "adapters/gdcm/GdcmItkDicomSeriesReader.h"
#include "adapters/itk/ItkVascularSegmenter.h"
#include "adapters/vtk/MDLModelReader.h"
#include "adapters/vtk/MSHMeshReader.h"
#include "adapters/vtk/VtkImageAdapter.h"
#include "core/NodeId.h"
#include "core/XQContourGroup.h"
#include "core/XQContourGroupPayload.h"
#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQFlowResultPayload.h"
#include "core/XQImageVolume.h"
#include "core/XQImageVolumePayload.h"
#include "core/XQMemoryImageBufferHandle.h"
#include "core/XQMesh.h"
#include "core/XQMeshPayload.h"
#include "core/XQPathPayload.h"
#include "core/XQPayload.h"
#include "core/XQScene.h"
#include "core/XQSegmentationMaskPayload.h"
#include "core/XQSourcePayload.h"
#include "core/XQSurfaceModelPayload.h"
#include "core/source/IGeometrySource.h"
#include "core/source/IVoxelSource.h"
#include "core/command/XQCommandStack.h"
#include "core/command/XQProjectCommands.h"
#include "core/command/XQSceneCommands.h"
#include "core/meshing/ITetMesher.h"
#include "io/project/CTGRContourReader.h"
#include "io/project/SvProjectReader.h"
#include "services/resource/GeometryResourceManager.h"
#include "services/image/DicomImportService.h"
#include "services/image/ImageResourceResolver.h"
#include "services/segmentation/ContourExtractionService.h"
#include "services/resource/GeometrySourceResolver.h"
#include "ui/XQSceneModel.h"
#include "ui/controllers/AiController.h"
#include "ui/controllers/MeshingController.h"
#include "ui/controllers/ModelingController.h"
#include "ui/controllers/PathController.h"
#include "ui/controllers/SegmentationController.h"
#include "ui/panels/XQStageWidgets.h"
#include "app/XQAboutDialog.h"
#include "app/XQAppStartup.h"
#include "app/XQPreferencesDialog.h"
#include "app/XQWorkflowSession.h"
#include "io/project/XQProjectWriter.h"
#include "core/asset/AssetRegistry.h"
#include "visualization/XQCrossSectionViewWidget.h"
#include "visualization/XQDemoVolume.h"
#include "visualization/XQMprWidget.h"
#include "visualization/XQRenderScene.h"

#if defined(XQ_ENABLE_MMG)
#include "adapters/mmg/TetGenThenMmg.h"
#elif defined(XQ_ENABLE_TETGEN)
#include "adapters/tetgen/TetGenTetMesher.h"
#endif

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDebug>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QImage>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QModelIndex>
#include <QPixmap>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QSettings>
#include <QShortcut>
#include <QScreen>
#include <QShowEvent>
#include <QSortFilterProxyModel>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTranslator>
#include <QTreeView>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

// Windows process-memory query for the status bar (R4). Included last, guarded
// against min/max macro pollution (this file uses std::min/std::max).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>

namespace xq {
namespace {

const int kDefaultProductWindowWidth = 1680;
const int kDefaultProductWindowHeight = 900;
const int kDefaultDataManagerDockWidth = 320;
const int kDefaultStageDockWidth = 260;
const int kMinimumDockWidth = 260;
const int kMinimumStageDockWidth = 240;
const int kMaximumDataManagerDockWidth = 360;
const int kMaximumStageDockWidth = 280;
const int kNavigatorSpinWidth = 48;
const int kScreenMargin = 24;

// Default LOD for selection-driven surface rendering (R4): interactive
// vtkLODActor levels with a 2M-triangle budget for deterministic paths. The
// renderer's own default stays off -- library-level tests pass options
// explicitly and are unaffected.
const LodOptions kDefaultLodOptions = [] {
    LodOptions options;
    options.enabled = true;
    options.interactive = true;
    options.budgetTriangles = 2'000'000;
    return options;
}();

// Loads a themed icon from the compiled resource bundle (resources/*.qrc).
QIcon themeIcon(const char* file)
{
    return QIcon(QString(":/xq/%1").arg(QString::fromLatin1(file)));
}

AssetId allocateUnusedAssetId(const AssetRegistry& registry)
{
    AssetId::ValueType maximum = 0;
    registry.visit_assets([&maximum](const AssetRecord& record) {
        if (record.id.value() > maximum) {
            maximum = record.id.value();
        }
    });
    if (maximum == (std::numeric_limits<AssetId::ValueType>::max)()) {
        return AssetId::invalid();
    }
    return AssetId(maximum + 1);
}

std::string projectRelativeSourceDirectory(
    const std::filesystem::path& sourceDirectory,
    const QString& projectFilePath)
{
    if (projectFilePath.isEmpty()) {
        return std::string();
    }
    std::error_code error;
    const std::filesystem::path relative = std::filesystem::relative(
        sourceDirectory,
        std::filesystem::path(projectFilePath.toStdString()).parent_path(),
        error);
    if (error || relative.empty() || relative.is_absolute()
        || relative.has_root_name() || relative.has_root_directory()) {
        return std::string();
    }
    return relative.generic_string();
}

std::shared_ptr<XQDemoVolume> materializeDemoVolume(
    const XQImageVolume& image,
    const IVoxelSource& source)
{
    const VoxelMeta meta = source.meta();
    const ImageGeometry& geometry = image.geometry();
    if (!meta.valid || meta.type != image.scalarType()
        || meta.components != image.componentCount()
        || meta.dims[0] != geometry.dimensions[0]
        || meta.dims[1] != geometry.dimensions[1]
        || meta.dims[2] != geometry.dimensions[2]) {
        return std::shared_ptr<XQDemoVolume>();
    }
    VoxelLease whole = source.acquire_whole();
    if (!whole.view().valid || whole.view().bytes.empty()) {
        return std::shared_ptr<XQDemoVolume>();
    }
    std::vector<std::uint8_t> bytes(
        whole.view().bytes.begin(), whole.view().bytes.end());
    auto volume = std::make_shared<XQDemoVolume>();
    volume->image = image;
    volume->buffer = std::make_shared<XQMemoryImageBufferHandle>(
        meta.type, meta.dims, meta.components, std::move(bytes));
    if (!volume->buffer->is_valid()) {
        return std::shared_ptr<XQDemoVolume>();
    }
    return volume;
}

bool sourceMaterializationStillCurrent(
    const XQDataNode* node,
    XQDomainType expectedDomain,
    ContentRevision expectedRevision,
    const std::shared_ptr<const XQPayload>& expectedSourcePayload,
    const std::string& expectedSourcePath)
{
    if (node == nullptr || node->domainType() != expectedDomain
        || node->contentRevision() != expectedRevision
        || expectedSourcePayload == nullptr
        || node->payload().get() != expectedSourcePayload.get()) {
        return false;
    }

    const std::shared_ptr<XQSourcePayload> source =
        std::dynamic_pointer_cast<XQSourcePayload>(node->payload());
    return source != nullptr && source->domainType() == expectedDomain
        && source->sourcePath() == expectedSourcePath;
}

QRect availableScreenGeometryFor(const QWidget* widget)
{
    QScreen* screen = widget != nullptr ? widget->screen() : nullptr;
    if (screen == nullptr) {
        screen = QGuiApplication::primaryScreen();
    }
    return screen != nullptr ? screen->availableGeometry() : QRect();
}

} // namespace

XQMainWindow::XQMainWindow(const XQScene* scene, QWidget* parent)
    : XQMainWindow(WorkflowCapabilities::compiledDefaults(), scene, parent)
{
}

XQMainWindow::XQMainWindow(WorkflowCapabilities capabilities,
                           const XQScene* scene,
                           QWidget* parent)
    : QMainWindow(parent)
    , sceneModel_(new XQSceneModel(scene, this))
    , sceneTreeView_(new QTreeView(this))
    , centralStack_(new QStackedWidget(this))
    , renderScene_(new XQRenderScene())
    , session_(std::make_unique<XQWorkflowSession>(capabilities))
    , levelSetSegmenter_(std::make_unique<ItkVascularSegmenter>())
{
    setWindowTitle(QStringLiteral("XQ"));

    // Every successful command-stack mutation (push / undo / redo) routed through
    // the session refreshes the tree + re-syncs the render scene exactly once --
    // the single, unified render-sync point. Non-command paths (project load,
    // async parse landings) still call refreshSceneTree() explicitly.
    session_->setSceneChangedCallback([this]() { refreshSceneTree(); });

    // Runtime zh translator, loaded from the compiled resource bundle. Default
    // language is Chinese: if the .qm loads we install it now so the very first
    // painted frame is Chinese. The .qm is generated separately; a missing /
    // failed load leaves the English source strings in place and the app still
    // runs. Parented to this window for cleanup. This is the single canonical
    // translator instance -- the Language menu installs/removes this same object,
    // so English fully reverts to source strings.
    zhTranslator_ = new QTranslator(this);
    const QString zhResource = QStringLiteral(":/i18n/xq_zh_CN.qm");
    const bool zhResourceExists = QFile::exists(zhResource);
    const bool zhLoaded = zhTranslator_->load(zhResource);
    const char* const translationContext = XQMainWindow::staticMetaObject.className();
    const QString fileTranslation =
        zhTranslator_->translate(translationContext, "&File");
    zhTranslatorReady_ = zhResourceExists && zhLoaded && !zhTranslator_->isEmpty()
        && !fileTranslation.isEmpty()
        && fileTranslation != QStringLiteral("&File");
    if (!zhTranslatorReady_) {
        qWarning().noquote()
            << QStringLiteral("[xq-i18n] Chinese translator unavailable:"
                              " resourceExists=%1 load=%2 empty=%3 fileTranslation=\"%4\"")
                   .arg(zhResourceExists ? QStringLiteral("true") : QStringLiteral("false"),
                        zhLoaded ? QStringLiteral("true") : QStringLiteral("false"),
                        zhTranslator_->isEmpty() ? QStringLiteral("true") : QStringLiteral("false"),
                        fileTranslation);
    }
    if (zhTranslatorReady_) {
        qApp->installTranslator(zhTranslator_);
    }

    // The 2x2 four-view widget mounts the resident render scene's four renderers
    // (three slice views + the 3D view). The 3D cell keeps the "xqRenderWidget"
    // object name the tests depend on (set inside XQMprWidget).
    mprWidget_ = new XQMprWidget(renderScene_.get(), this);

    sceneTreeView_->setObjectName(QStringLiteral("xqSceneTreeView"));
    // A pass-through filter proxy sits between the tree and the scene model so
    // the Data Manager search box can filter by display name. With an empty
    // filter it forwards every row unchanged (rowCount stays == node count, the
    // contract test_main_window checks).
    sceneFilter_ = new QSortFilterProxyModel(this);
    sceneFilter_->setSourceModel(sceneModel_);
    sceneFilter_->setFilterCaseSensitivity(Qt::CaseInsensitive);
    sceneFilter_->setFilterKeyColumn(XQSceneModel::NameColumn);
    sceneFilter_->setRecursiveFilteringEnabled(true);
    sceneTreeView_->setModel(sceneFilter_);

    // The stack hosts the 2x2 MPR view (single workspace page). Kept as a
    // QStackedWidget so the object name and page-raising contract stay stable.
    centralStack_->setObjectName(QStringLiteral("xqCentralStack"));
    centralStack_->setMinimumSize(0, 0);
    centralStack_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    mprWidget_->setMinimumSize(0, 0);
    mprWidget_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    centralStack_->addWidget(mprWidget_);
    centralStack_->setCurrentWidget(mprWidget_);

    // Cross-section workbench: a second stack page (along-path slider + resident
    // reslice view) shown while the contour-extraction stage is active. Added
    // after the MPR page so mprWidget_ stays the default current widget.
    buildCrossSectionWorkbench();
    // Independent two-dimensional contour panel (P3-5b): built once here so it
    // outlives every attachWorkflow rebuild of stagePanel_. The ROI-v2 page owns
    // the explicit command that opens it.
    buildCrossSectionSegPanel();

    workspaceTabs_ = new QTabWidget(this);
    workspaceTabs_->setObjectName(QStringLiteral("xqWorkspaceTabs"));
    workspaceTabs_->setDocumentMode(true);
    workspaceTabs_->setMovable(false);
    workspaceTabs_->setTabsClosable(false);
    workspaceTabs_->addTab(centralStack_, themeIcon("node-image.svg"), QString());
    setCentralWidget(workspaceTabs_);

    buildMenus();
    buildMainToolBar();
    buildDataManagerDock();
    buildImageNavigatorDock();
    buildStatusBar();
    buildWindowMenu();

    // Search box filters the scene tree by display name.
    if (sceneSearchEdit_ != nullptr) {
        QObject::connect(sceneSearchEdit_, &QLineEdit::textChanged,
                         sceneFilter_, &QSortFilterProxyModel::setFilterFixedString);
    }

    // Navigator sliders re-slice the MPR views.
    QObject::connect(axialSlider_, &QSlider::valueChanged,
                     this, &XQMainWindow::onNavigatorSliderChanged);
    QObject::connect(sagittalSlider_, &QSlider::valueChanged,
                     this, &XQMainWindow::onNavigatorSliderChanged);
    QObject::connect(coronalSlider_, &QSlider::valueChanged,
                     this, &XQMainWindow::onNavigatorSliderChanged);

    QObject::connect(mprWidget_, &XQMprWidget::seedPicked, this,
                     [this](int i, int j, int k) {
        if (pickMode_ == PickMode::PathPoint) {
            // Path control-point pick: voxel -> world, append to the draft.
            // Shared with the Ctrl+A shortcut through addPathDraftVoxel.
            addPathDraftVoxel(i, j, k);
        }
    });

    // A pick that fell outside the image volume: transient status-bar hint so the
    // click no longer looks silently ignored.
    QObject::connect(mprWidget_, &XQMprWidget::pickOutOfBounds, this, [this]() {
        statusBar()->showMessage(tr("Click point is outside the image bounds"),
                                 3000);
    });

    // Wheel-driven slice changes in a slice view sync the matching navigator
    // slider/spin (signals blocked to avoid a re-render loop) and repaint the
    // other views (the shared crosshair + 3D planes moved too).
    QObject::connect(mprWidget_, &XQMprWidget::sliceChanged, this,
                     [this](int axis, int index) {
        QSlider* sliders[3] = {sagittalSlider_, coronalSlider_, axialSlider_};
        QSpinBox* spins[3] = {sagittalSpin_, coronalSpin_, axialSpin_};
        if (axis < 0 || axis > 2) {
            return;
        }
        if (QSlider* slider = sliders[axis]) {
            const QSignalBlocker blocker(slider);
            slider->setValue(index);
        }
        if (QSpinBox* spin = spins[axis]) {
            const QSignalBlocker blocker(spin);
            spin->setValue(index);
        }
        if (mprWidget_ != nullptr) {
            mprWidget_->renderAll();
        }
        // The scene already applied the new slice (XQSliceViewWidget sets it before
        // emitting), so refresh the Loc.(mm) read-out + status-bar Position here.
        syncLocReadout();
        updatePositionReadout();
    });

    // A window/level drag in a slice view (VTK applied it to the shared property)
    // re-reads the scene's values into the two navigator spins (blocked to avoid
    // pushing straight back into the scene).
    QObject::connect(mprWidget_, &XQMprWidget::windowLevelChanged, this, [this]() {
        if (renderScene_ == nullptr || windowSpin_ == nullptr
            || levelSpin_ == nullptr) {
            return;
        }
        double window = 0.0;
        double level = 0.0;
        renderScene_->windowLevel(&window, &level);
        const QSignalBlocker windowBlocker(windowSpin_);
        const QSignalBlocker levelBlocker(levelSpin_);
        windowSpin_->setValue(window);
        levelSpin_->setValue(level);
    });

    if (renderScene_ != nullptr) {
        renderScene_->clearVolume();
    }

    // Keyboard slice stepping on the Axial view (PageUp/PageDown).
    QShortcut* nextSlice = new QShortcut(QKeySequence(Qt::Key_PageUp), this);
    QObject::connect(nextSlice, &QShortcut::activated, this, [this]() { stepAxialSlice(+1); });
    QShortcut* prevSlice = new QShortcut(QKeySequence(Qt::Key_PageDown), this);
    QObject::connect(prevSlice, &QShortcut::activated, this, [this]() { stepAxialSlice(-1); });

    // Ctrl+A adds a path control point at the current crosshair while in path
    // picking mode (a keyboard alternative to clicking a slice). A no-op in any
    // other pick mode / with no volume.
    QShortcut* addAtCrosshair = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_A), this);
    QObject::connect(addAtCrosshair, &QShortcut::activated, this, [this]() {
        if (pickMode_ != PickMode::PathPoint || renderScene_ == nullptr) {
            return;
        }
        const int i = renderScene_->sliceIndex(0); // x / Sagittal
        const int j = renderScene_->sliceIndex(1); // y / Coronal
        const int k = renderScene_->sliceIndex(2); // z / Axial
        if (i < 0 || j < 0 || k < 0) {
            return; // no volume
        }
        addPathDraftVoxel(i, j, k);
    });

    // Esc leaves any active picking mode (seed or path).
    QShortcut* exitPick = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    QObject::connect(exitPick, &QShortcut::activated, this, [this]() { exitPicking(); });

    // Apply persisted preferences (crosshair, default layout, slice step).
    applyPreferences();

    // Selection-driven rendering. The selection model exists once setModel() has
    // run (above); setScene()/attachWorkflow() only swap the model's scene, not
    // the model, so this connection stays valid. With a null scene nothing is
    // selectable, so the slot is a no-op -- safe for the default-constructed
    // window that test_main_window builds without a workflow attached.
    QObject::connect(sceneTreeView_->selectionModel(),
                     &QItemSelectionModel::currentChanged,
                     this,
                     &XQMainWindow::onSceneSelectionChanged);

    // Visibility checkbox -> render scene. Toggling a node's checkbox in the Data
    // Manager shows/hides its actors in every view and repaints. The image node
    // does not live in the render scene's node table (it goes through setVolume),
    // so its visibility drives the dedicated setImageVisible path instead.
    QObject::connect(sceneModel_, &XQSceneModel::nodeVisibilityChanged, this,
                     [this](const NodeId& id, bool visible) {
        if (renderScene_ != nullptr) {
            const XQScene* scene =
                sceneModel_ != nullptr ? sceneModel_->scene() : nullptr;
            const XQDataNode* node = scene != nullptr ? scene->find(id) : nullptr;
            if (node != nullptr && node->domainType() == XQDomainType::Image) {
                renderScene_->setImageVisible(visible);
            } else {
                renderScene_->setNodeVisible(id, visible);
            }
        }
        if (mprWidget_ != nullptr) {
            mprWidget_->renderAll();
        }
    });

    // Background task runner drives the global busy matrix: it disables every
    // scene-mutating input while a heavy job runs and restores them after.
    QObject::connect(&session_->taskRunner(), &XQTaskRunner::taskStarted, this,
                     [this](const QString& label) { setWorkflowBusy(true, label); });
    QObject::connect(&session_->taskRunner(), &XQTaskRunner::taskFinished, this,
                     [this](const QString& label) {
                         setWorkflowBusy(false, label);
                         updateMemoryStatus();
                     });

    resizeToProductDefault();

    // Restore window geometry + dock layout from the previous session (no-op on
    // first run; the screen-fitting product default above stands in that case).
    restoreWindowState();

    // Apply all user-visible strings once everything is built. The constructor
    // installs the zh translator first when it is available, so the first frame
    // already resolves to Chinese.
    retranslateUi();
}

XQMainWindow::~XQMainWindow()
{
    // The MPR widget's QVTK render windows hold references to renderScene_'s four
    // vtkRenderers (AddRenderer). renderScene_ is a member (destroyed before the
    // base ~QMainWindow deletes child widgets), so unless we tear the widget down
    // first, the render windows would outlive the renderers they reference and
    // crash on destruction. Delete the widget tree here, before renderScene_'s
    // member destructor runs, so the renderers are unmounted while still alive.
    delete mprWidget_;
    mprWidget_ = nullptr;
    renderScene_.reset();
}

void XQMainWindow::setScene(const XQScene* scene)
{
    sceneModel_->setScene(scene);
    if (statusNodeCountLabel_ != nullptr) {
        const int rows = sceneModel_->rowCount(QModelIndex());
        statusNodeCountLabel_->setText(tr("Nodes: %1").arg(rows));
    }
}

void XQMainWindow::attachWorkflow(XQScene* scene, XQCommandStack* stack)
{
    // Rebind the scene/stack and rebuild the available controllers (+ mesh kernel) in
    // the resident session. The session itself (and its task runner) is
    // constructed once at window construction, so the taskRunner connections made
    // there stay valid across a re-attach.
    session_->attach(scene, stack);
    finishWorkflowAttach(scene);
}

void XQMainWindow::attachWorkflow(XQProject* project, XQCommandStack* stack)
{
    session_->attach(project, stack);
    finishWorkflowAttach(project != nullptr ? &project->scene() : nullptr);
}

void XQMainWindow::finishWorkflowAttach(XQScene* scene)
{

    // Display the (mutable) scene in the tree; the model reads it as const.
    setScene(scene);

    buildStagePanel();
    buildEditMenu();
    buildWindowMenu();
    applyProductDockLayout();
    QTimer::singleShot(0, this, [this]() {
        applyProductDockLayout();
    });
    retranslateUi();
}

void XQMainWindow::attachGeometryResources(const AssetRegistry* registry,
                                           const std::string& assetRootDir)
{
    geometryRegistry_ = registry;
    assetRootDir_ = assetRootDir;
    geometryResources_.reset();
    if (registry != nullptr && !assetRootDir.empty()) {
        geometryResources_.reset(new GeometryResourceManager(registry, assetRootDir));
        geometryResources_->setBudgetBytes(
            static_cast<std::size_t>(XQPreferencesDialog::geometryBudgetMiB()) << 20);
    }
}

const GeometryResourceManager* XQMainWindow::geometryResourceManager() const
{
    return geometryResources_.get();
}

bool XQMainWindow::renderSceneHasNode(const NodeId& id) const
{
    return renderScene_ != nullptr && renderScene_->hasNode(id);
}

long long XQMainWindow::renderSceneUploadedPointCount(const NodeId& id) const
{
    return renderScene_ != nullptr ? renderScene_->uploadedPointCount(id) : -1;
}

void XQMainWindow::setExternalStartupState(XQAppStartupState* state)
{
    externalProject_ = state != nullptr ? &state->project : nullptr;
    workspacePath_ = state != nullptr
        ? QString::fromStdString(state->projectFilePath)
        : QString();
}

XQProject* XQMainWindow::activeProject() const
{
    return ownedState_ != nullptr ? &ownedState_->project : externalProject_;
}

void XQMainWindow::reportWorkspaceError(const QString& title, const QString& text)
{
    // Tests drive the dialog-free entry points headlessly; a modal box would
    // deadlock offscreen. Suppressible via object property, default = show.
    if (property("xqSuppressDialogs").toBool()) {
        qWarning().noquote() << title << ":" << text;
        return;
    }
    QMessageBox::warning(this, title, text);
}

void XQMainWindow::openWorkspaceDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Workspace"), QString(), tr("XQ Workspace (*.xqproj)"));
    if (path.isEmpty()) {
        return; // user cancelled
    }
    openWorkspaceFromPath(path);
}

void XQMainWindow::saveWorkspaceDialog()
{
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save Workspace"), workspacePath_, tr("XQ Workspace (*.xqproj)"));
    if (path.isEmpty()) {
        return; // user cancelled
    }
    saveWorkspaceFile(path);
}

bool XQMainWindow::saveWorkspaceFile(const QString& path)
{
    if (workflowBusy_) {
        return false; // busy contract: no state swap while a task runs
    }
    if (session_->scene() == nullptr) {
        reportWorkspaceError(tr("Save Workspace"),
                             tr("No active workspace to save."));
        return false;
    }
    // The window renders a scene owned by a project; saving needs that project.
    // It is either the runtime-opened ownedState_ or the startup state that
    // attached this scene -- resolved through activeProject().
    XQProject* project = activeProject();
    if (project == nullptr) {
        reportWorkspaceError(tr("Save Workspace"),
                             tr("No active project to save."));
        return false;
    }

    namespace fs = std::filesystem;
    const std::string targetPathUtf8 = path.toStdString();
    fs::path targetPath(targetPathUtf8);
    const fs::path targetAssets =
        targetPath.parent_path() / (targetPath.stem().string() + ".assets");

    // Writer only derives blobs for nodes WITHOUT an asset id. Lazy-loaded
    // nodes already carry ids, so their blobs must be copied by hand whenever
    // the target assets dir differs from the current asset root -- otherwise
    // the new archive references files that were never written (dead archive).
    if (!assetRootDir_.empty() && fs::path(assetRootDir_) != targetAssets) {
        bool copyOk = true;
        std::string firstError;
        project->assetRegistry().visit_assets(
            [&](const AssetRecord& record) {
                for (std::size_t b = 0; copyOk && b < record.blobs.size(); ++b) {
                    const std::string& rel = record.blobs[b].second.relPath;
                    const fs::path from = fs::path(assetRootDir_) / rel;
                    const fs::path to = targetAssets / rel;
                    std::error_code ec;
                    fs::create_directories(to.parent_path(), ec);
                    if (ec) {
                        copyOk = false;
                        firstError = to.parent_path().string();
                        return;
                    }
                    const bool copied = fs::copy_file(
                        from, to, fs::copy_options::overwrite_existing, ec);
                    if (!copied || ec) {
                        copyOk = false;
                        firstError = from.string();
                    }
                }
            });
        if (!copyOk) {
            reportWorkspaceError(
                tr("Save Workspace"),
                tr("Could not copy asset blob: %1. The workspace was not saved.")
                    .arg(QString::fromStdString(firstError)));
            return false;
        }
    }

    const XQProjectWriter::Status status =
        XQProjectWriter::save(*project, targetPathUtf8);
    if (status != XQProjectWriter::Status::Ok) {
        const QString reason = status == XQProjectWriter::Status::FileOpenError
            ? QStringLiteral("FileOpenError")
            : QStringLiteral("WriteError");
        reportWorkspaceError(tr("Save Workspace"),
                             tr("Could not save the workspace: %1.").arg(reason));
        return false;
    }

    workspacePath_ = path;
    setWindowTitle(QStringLiteral("XQ - %1").arg(QFileInfo(path).fileName()));
    return true;
}

bool XQMainWindow::openWorkspaceFromPath(const QString& path)
{
    if (workflowBusy_) {
        return false; // busy contract: no state swap while a task runs
    }
    auto fresh = std::make_unique<XQAppStartupState>();
    XQAppStartupConfig config;
    config.projectPath = path.toStdString();
    const XQAppStartupStatus status = initializeAppStartup(config, fresh.get());
    if (status != XQAppStartupStatus::Ok) {
        // Old state stays fully attached; nothing was swapped yet.
        reportWorkspaceError(tr("Open Workspace"),
                             tr("Could not open the workspace: %1.").arg(path));
        return false;
    }

    // Take ownership, then re-attach everything to the new state. The previous
    // ownedState_ (if any) is destroyed after the swap; the startup-borrowed
    // state (main's) is simply no longer referenced.
    ownedState_ = std::move(fresh);
    externalProject_ = nullptr;
    attachWorkflow(&ownedState_->project, &ownedState_->commandStack);
    attachGeometryResources(&ownedState_->project.assetRegistry(),
                            ownedState_->assetRootDir);

    // Reset per-dataset UI state. The stage panel was rebuilt by
    // attachWorkflow above, so the Path page starts fresh; the draft must not
    // leak points picked against the previous dataset's voxel frame.
    activeImage_.reset();
    activeImageNodeId_ = NodeId::invalid();
    activeProjectDir_.clear();
    pathDraftPoints_.clear();
    pickMode_ = PickMode::None;
    if (pathDraftChanged_) {
        pathDraftChanged_();
    }
    workspacePath_ = path;
    if (renderScene_ != nullptr) {
        renderScene_->clearVolume();
        renderScene_->clearNodes();
        // The render side was just emptied; drop the incremental fingerprints so
        // the next sync rebuilds from scratch (kept in step with clearNodes()).
        syncedPayloads_.clear();
    }
    refreshSceneTree();
    setWindowTitle(QStringLiteral("XQ - %1").arg(QFileInfo(path).fileName()));
    return true;
}

void XQMainWindow::buildStagePanel()
{
    // Re-attach rebuilds the stage panel so every page lambda captures the new
    // controller pointers (the old ones are reset below in attachWorkflow).
    // The dock is reused; only its inner panel is swapped.
    QStackedWidget* oldPanel = stagePanel_;
    stagePanel_ = new QStackedWidget(this);
    stagePanel_->setObjectName(QStringLiteral("xqStagePanel"));

    // Rich operation forms per stage, each driving its controller through the
    // command stack (see XQStageWidgets). Pages keep the stable
    // xqStagePage_<Name> / xqStageTitle_<Name> object names the integration and
    // tests rely on.
    // Image provider: hands stage pages (segmentation) the currently loaded real
    // volume + buffer at execute time. The panels borrow these pointers only
    // during the call; the window owns activeImage_. Empty active image when no
    // real .vti has been opened, which keeps the data-driven pages disabled.
    ActiveImageProvider imageProvider = [this]() {
        ActiveImage active;
        if (activeImage_) {
            active.valid = true;
            active.image = &activeImage_->image;
            active.buffer = activeImage_->buffer.get();
            active.bufferShared = activeImage_->buffer;
            active.nodeId = activeImageNodeId_;
        }
        return active;
    };

    PathDraftProvider pathDraftProvider = [this]() {
        return pathDraftPoints_; // snapshot copy
    };

    PathDraftMutator pathDraftMutator = [this](int removeIndex) {
        if (removeIndex < 0) {
            pathDraftPoints_.clear();
        } else if (static_cast<std::size_t>(removeIndex) < pathDraftPoints_.size()) {
            pathDraftPoints_.erase(pathDraftPoints_.begin() + removeIndex);
        }
        if (pathDraftChanged_) {
            pathDraftChanged_();
        }
    };

    PathDraftFocuser pathDraftFocuser = [this](int index) {
        focusPathDraftPoint(index);
    };

    PathPickingSetter pathPickingSetter = [this](bool enabled, PathPageHooks hooks) {
        // The page (re)registers its hooks on every call; a rebuilt panel
        // hands in fresh closures, so no dangling captures survive. Wrap the
        // page's list refresher so every draft change (add / remove / clear)
        // also pushes the points to the render scene -> live slice-view markers.
        std::function<void()> refreshList = std::move(hooks.refreshList);
        pathDraftChanged_ = [this, refreshList]() {
            if (refreshList) {
                refreshList();
            }
            syncPathControlMarkers();
        };
        pathPickToggleSetter_ = std::move(hooks.setPickChecked);
        if (mprWidget_ == nullptr) {
            return;
        }
        pickMode_ = enabled ? PickMode::PathPoint : PickMode::None;
        // Reuses XQMprWidget's generic seed-picking click plumbing; the
        // seedPicked handler routes it to the Path draft while this mode is on.
        if (enabled) {
            mprWidget_->clearSeedMarker();
        }
        // Sync the markers to the current draft on both entry and exit: entering
        // shows any pre-existing draft points; exiting is left to clearVolume /
        // an explicit clear (the markers stay put so a finished path is visible).
        syncPathControlMarkers();
        mprWidget_->setSeedPickingEnabled(enabled);
        mprWidget_->setModeHint(enabled
            ? tr("Picking path point: click a slice to add a point "
                 "(Ctrl+A adds at the crosshair, Esc exits)")
            : QString());
    };

    ContourGroupProvider contourGroupProvider = [this](NodeId requested) {
        ActiveContourGroup active;
        const XQDataNode* node = nodeForRequestedOrSelected(requested);
        if (node == nullptr || node->domainType() != XQDomainType::ContourGroup
            || !node->payload()) {
            return active;
        }
        // An in-memory group (hand-drawn contours, P3-2) already carries a real
        // XQContourGroupPayload; use it directly so lofting does not need a .ctgr
        // file on disk. SV-loaded groups instead carry an unresolved source
        // payload (the .ctgr path), parsed on demand below.
        if (std::shared_ptr<XQContourGroupPayload> resolved =
                std::dynamic_pointer_cast<XQContourGroupPayload>(node->payload())) {
            active.valid = true;
            active.nodeId = node->id();
            active.contourGroup = resolved->group();
            return active;
        }
        std::shared_ptr<XQSourcePayload> source =
            std::dynamic_pointer_cast<XQSourcePayload>(node->payload());
        if (source == nullptr) {
            return active;
        }
        CTGRReadResult read = {};
        if (CTGRContourReader::read(resolveSourcePath(source->sourcePath()).toStdString(), &read)
            != CTGRContourReader::Status::Ok) {
            return active;
        }
        active.valid = true;
        active.nodeId = node->id();
        active.contourGroup = std::move(read.group);
        return active;
    };

    VesselProfileOutputIdProvider vesselProfileOutputIdProvider = [this]() {
        return allocateSceneObjectId();
    };

    CenterlineBOutputIdProvider centerlineBOutputIdProvider = [this]() {
        CenterlineBOutputIds ids;
        const NodeId pathNode = allocateSceneObjectId();
        if (!pathNode.is_valid()
            || pathNode.value()
                == std::numeric_limits<NodeId::ValueType>::max()) {
            return ids;
        }
        ids.valid = true;
        ids.pathNode = pathNode;
        ids.profileNode = NodeId(pathNode.value() + 1);
        return ids;
    };

    // Node lister: enumerates the scene's nodes of a given domain (id + name) so
    // the Modeling / Meshing / Modules / AI pages can offer a dropdown instead of a
    // raw NodeId spin box. Ascending by id (scene stores nodes in a NodeId-keyed
    // map). Empty when no scene is attached.
    SceneNodeLister nodeLister = [this](XQDomainType domain) {
        std::vector<SceneNodeOption> options;
        if (session_->scene() == nullptr) {
            return options;
        }
        session_->scene()->visit_nodes([&options, domain](const XQDataNode& node) {
            if (node.domainType() == domain) {
                options.push_back(
                    SceneNodeOption{node.id(), QString::fromStdString(node.display_name())});
            }
        });
        return options;
    };

    StageChangedCallback stageChanged = [this]() {
        refreshSceneTree();
    };

    AsyncCommandRunner asyncRunner = [this](const QString& label, StageCommandJob job,
                                            std::function<void(bool, const QString&)> done) {
        return session_->taskRunner().run(
            label,
            [job = std::move(job)]() -> std::shared_ptr<void> {
                return std::make_shared<StageCommandOutcome>(job());
            },
            [this, done = std::move(done)](std::shared_ptr<void> raw) {
                auto outcome = std::static_pointer_cast<StageCommandOutcome>(raw);
                bool ok = false;
                if (outcome->ownerCommit) {
                    ok = outcome->ownerCommit(&outcome->message);
                } else if (outcome->command != nullptr) {
                    // Command gateway: a successful push refreshes the tree +
                    // re-syncs the render scene through the session callback (the
                    // unified render-sync point), so no explicit refresh here.
                    ok = session_->pushCommand(std::move(outcome->command));
                }
                if (done) {
                    done(ok, outcome->message);
                }
            });
    };

    StagePanelContext stageContext;
    stageContext.path = session_->pathController();
    stageContext.segmentation = session_->segmentationController();
    stageContext.modeling = session_->modelingController();
    stageContext.meshing = session_->meshingController();
    stageContext.vesselProfile = session_->vesselProfileController();
    stageContext.pathModules = session_->pathModuleController();
    stageContext.centerlineB = session_->centerlineBController();
    stageContext.ai = session_->aiController();
    stageContext.imageProvider = imageProvider;
    stageContext.contourGroupProvider = contourGroupProvider;
    stageContext.vesselProfileOutputIdProvider = vesselProfileOutputIdProvider;
    stageContext.centerlineBOutputIdProvider = centerlineBOutputIdProvider;
    stageContext.nodeLister = nodeLister;
    stageContext.stageChanged = stageChanged;
    stageContext.openContourWorkbench = [this]() { showContourWorkbench(); };
    stageContext.pathDraftProvider = pathDraftProvider;
    stageContext.pathDraftMutator = pathDraftMutator;
    stageContext.pathDraftFocuser = pathDraftFocuser;
    stageContext.pathPickingSetter = pathPickingSetter;
    stageContext.asyncRunner = asyncRunner;
    populateStagePanels(stagePanel_, stageContext);

    if (stageDock_ == nullptr) {
        QDockWidget* stageDock = new QDockWidget(tr("Stages"), this);
        stageDock->setObjectName(QStringLiteral("xqStageDock"));
        stageDock->setMinimumWidth(kMinimumStageDockWidth);
        stageDock->setMaximumWidth(kMaximumStageDockWidth);
        addDockWidget(Qt::RightDockWidgetArea, stageDock);
        stageDock_ = stageDock;
        stageDock_->hide();
    }
    stageDock_->setWidget(stagePanel_);

    if (oldPanel != nullptr) {
        delete oldPanel;
    }
}

void XQMainWindow::buildMenus()
{
    QMenuBar* bar = menuBar();

    fileMenu_ = bar->addMenu(QString());
    openAction_ = fileMenu_->addAction(themeIcon("document-open.svg"), QString());
    openAction_->setObjectName(QStringLiteral("xqOpenWorkspaceAction"));
    openAction_->setShortcut(QKeySequence::Open);
    QObject::connect(openAction_, &QAction::triggered, this, &XQMainWindow::openWorkspaceDialog);
    openImageAction_ = fileMenu_->addAction(themeIcon("node-image.svg"), QString());
    openImageAction_->setObjectName(QStringLiteral("xqOpenImageAction"));
    QObject::connect(openImageAction_, &QAction::triggered, this, &XQMainWindow::openImageFile);
    openDicomAction_ = fileMenu_->addAction(themeIcon("node-image.svg"), QString());
    openDicomAction_->setObjectName(QStringLiteral("xqOpenDicomAction"));
    QObject::connect(openDicomAction_, &QAction::triggered,
                     this, &XQMainWindow::openDicomSeries);
    openSvProjectAction_ = fileMenu_->addAction(themeIcon("document-open.svg"), QString());
    openSvProjectAction_->setObjectName(QStringLiteral("xqOpenSvProjectAction"));
    QObject::connect(openSvProjectAction_, &QAction::triggered, this, &XQMainWindow::openSvProject);
    saveAction_ = fileMenu_->addAction(themeIcon("document-save.svg"), QString());
    saveAction_->setObjectName(QStringLiteral("xqSaveWorkspaceAction"));
    saveAction_->setShortcut(QKeySequence::Save);
    QObject::connect(saveAction_, &QAction::triggered, this, &XQMainWindow::saveWorkspaceDialog);
    fileMenu_->addSeparator();
    quitAction_ = fileMenu_->addAction(QString());
    quitAction_->setShortcut(QKeySequence::Quit);
    QObject::connect(quitAction_, &QAction::triggered, this, &QWidget::close);

    // Edit menu is created here so it keeps the conventional File | Edit | View
    // position; its undo/redo actions are filled in later by buildEditMenu()
    // (only once a workflow / command stack is attached). Creating it lazily in
    // attachWorkflow() would append it after Help instead.
    editMenu_ = bar->addMenu(QString());

    // View menu: MPR crosshair toggle + single/quad layout toggle + Language.
    viewMenu_ = bar->addMenu(QString());
    crosshairAction_ = viewMenu_->addAction(QString());
    crosshairAction_->setObjectName(QStringLiteral("xqCrosshairAction"));
    crosshairAction_->setCheckable(true);
    crosshairAction_->setChecked(true);
    QObject::connect(crosshairAction_, &QAction::toggled, this, [this](bool on) {
        if (mprWidget_ != nullptr) {
            mprWidget_->setCrosshairVisible(on);
        }
    });
    planes3dAction_ = viewMenu_->addAction(QString());
    planes3dAction_->setObjectName(QStringLiteral("xqAction3dPlanes"));
    planes3dAction_->setCheckable(true);
    planes3dAction_->setChecked(true);
    QObject::connect(planes3dAction_, &QAction::toggled, this, [this](bool on) {
        if (renderScene_ != nullptr) {
            renderScene_->setImagePlanesVisible3d(on);
        }
        if (mprWidget_ != nullptr) {
            mprWidget_->renderAll();
        }
    });
    layoutMenuAction_ = viewMenu_->addAction(QString());
    layoutMenuAction_->setObjectName(QStringLiteral("xqViewLayoutMenuAction"));
    layoutMenuAction_->setCheckable(true);
    QObject::connect(layoutMenuAction_, &QAction::toggled, this, [this](bool single) {
        if (viewLayoutAction_ != nullptr) {
            viewLayoutAction_->setChecked(single);
        }
        toggleViewLayout();
    });

    viewMenu_->addSeparator();
    languageMenu_ = viewMenu_->addMenu(QString());
    languageMenu_->setObjectName(QStringLiteral("xqLanguageMenu"));
    languageGroup_ = new QActionGroup(this);
    languageGroup_->setExclusive(true);
    langZhAction_ = languageMenu_->addAction(QString());
    langZhAction_->setObjectName(QStringLiteral("xqLanguageZhAction"));
    langZhAction_->setCheckable(true);
    langZhAction_->setChecked(true); // default: Chinese
    languageGroup_->addAction(langZhAction_);
    langEnAction_ = languageMenu_->addAction(QString());
    langEnAction_->setObjectName(QStringLiteral("xqLanguageEnAction"));
    langEnAction_->setCheckable(true);
    languageGroup_->addAction(langEnAction_);
    QObject::connect(langZhAction_, &QAction::triggered, this,
                     [this]() { setChineseLanguage(true); });
    QObject::connect(langEnAction_, &QAction::triggered, this,
                     [this]() { setChineseLanguage(false); });

    // Tools menu: preferences.
    toolsMenu_ = bar->addMenu(QString());
    prefsAction_ = toolsMenu_->addAction(QString());
    prefsAction_->setObjectName(QStringLiteral("xqPreferencesAction"));
    prefsAction_->setShortcut(QKeySequence::Preferences);
    QObject::connect(prefsAction_, &QAction::triggered, this, &XQMainWindow::showPreferencesDialog);

    // Window menu is created here (so it sits before Help) but populated by
    // buildWindowMenu() once the docks exist.
    windowMenu_ = bar->addMenu(QString());

    helpMenu_ = bar->addMenu(QString());
    aboutAction_ = helpMenu_->addAction(QString());
    aboutAction_->setObjectName(QStringLiteral("xqAboutAction"));
    QObject::connect(aboutAction_, &QAction::triggered, this, &XQMainWindow::showAboutDialog);
}

void XQMainWindow::buildMainToolBar()
{
    QToolBar* toolBar = addToolBar(QStringLiteral("Main"));
    toolBar->setObjectName(QStringLiteral("mainActionsToolBar"));
    toolBar->setMovable(false);
    // Dense desktop workbench layout: compact icon + text commands.
    toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    mainToolBar_ = toolBar;

    tbOpenAction_ = toolBar->addAction(themeIcon("document-open.svg"), QString());
    tbOpenAction_->setObjectName(QStringLiteral("xqToolbarOpenAction"));
    QObject::connect(tbOpenAction_, &QAction::triggered, this, &XQMainWindow::openWorkspaceDialog);
    tbSaveAction_ = toolBar->addAction(themeIcon("document-save.svg"), QString());
    tbSaveAction_->setObjectName(QStringLiteral("xqToolbarSaveAction"));
    QObject::connect(tbSaveAction_, &QAction::triggered, this, &XQMainWindow::saveWorkspaceDialog);
    toolBar->addSeparator();

    tbUndoAction_ = toolBar->addAction(themeIcon("edit-undo.svg"), QString());
    tbUndoAction_->setObjectName(QStringLiteral("xqToolbarUndoAction"));
    QObject::connect(tbUndoAction_, &QAction::triggered, this, &XQMainWindow::undo);
    tbRedoAction_ = toolBar->addAction(themeIcon("edit-redo.svg"), QString());
    tbRedoAction_->setObjectName(QStringLiteral("xqToolbarRedoAction"));
    QObject::connect(tbRedoAction_, &QAction::triggered, this, &XQMainWindow::redo);
    toolBar->addSeparator();

    // View-layout toggle stays available for the View menu + preferences, but is
    // no longer a toolbar button -- it lives as a standalone checkable action so
    // every existing reference (applyPreferences, toggleViewLayout) keeps working.
    viewLayoutAction_ = new QAction(themeIcon("node-image.svg"), QString(), this);
    viewLayoutAction_->setObjectName(QStringLiteral("xqViewLayoutAction"));
    viewLayoutAction_->setCheckable(true);
    QObject::connect(viewLayoutAction_, &QAction::triggered,
                     this, &XQMainWindow::toggleViewLayout);

    // Workflow tools (MITK-style): Image | Path | Seg | Model | Mesh | Modules.
    // Image raises the central MPR workspace; the stage tools raise
    // the matching right-hand stage page (no-op until attachWorkflow builds it).
    tbImageAction_ = toolBar->addAction(themeIcon("tool-image.svg"), QString());
    tbImageAction_->setObjectName(QStringLiteral("xqToolbarImageAction"));
    QObject::connect(tbImageAction_, &QAction::triggered, this, [this]() {
        if (mprWidget_ != nullptr) {
            centralStack_->setCurrentWidget(mprWidget_);
        }
        if (stageDock_ != nullptr) {
            stageDock_->hide();
        }
    });
    toolBar->addSeparator();
    tbPathAction_ = toolBar->addAction(themeIcon("tool-path.svg"), QString());
    tbPathAction_->setObjectName(QStringLiteral("xqToolbarPathAction"));
    QObject::connect(tbPathAction_, &QAction::triggered, this, [this]() { showStagePage(0); });
    tbSeg2dAction_ = toolBar->addAction(themeIcon("tool-seg-2d.svg"), QString());
    tbSeg2dAction_->setObjectName(QStringLiteral("xqToolbarSeg2dAction"));
    QObject::connect(tbSeg2dAction_, &QAction::triggered, this, [this]() { showStagePage(1); });
    tbModelAction_ = toolBar->addAction(themeIcon("tool-model.svg"), QString());
    tbModelAction_->setObjectName(QStringLiteral("xqToolbarModelAction"));
    QObject::connect(tbModelAction_, &QAction::triggered, this, [this]() { showStagePage(2); });
    tbMeshAction_ = toolBar->addAction(themeIcon("tool-mesh.svg"), QString());
    tbMeshAction_->setObjectName(QStringLiteral("xqToolbarMeshAction"));
    QObject::connect(tbMeshAction_, &QAction::triggered, this, [this]() { showStagePage(3); });
    tbModulesAction_ = toolBar->addAction(themeIcon("tool-path.svg"), QString());
    tbModulesAction_->setObjectName(QStringLiteral("xqToolbarModulesAction"));
    QObject::connect(tbModulesAction_, &QAction::triggered,
                     this, [this]() { showStagePage(4); });
}

void XQMainWindow::buildDataManagerDock()
{
    QDockWidget* dock = new QDockWidget(QStringLiteral("Data Manager"), this);
    dock->setObjectName(QStringLiteral("xqDataManagerDock"));

    QWidget* panel = new QWidget(dock);
    QVBoxLayout* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    sceneSearchEdit_ = new QLineEdit(panel);
    sceneSearchEdit_->setObjectName(QStringLiteral("xqSceneSearch"));
    sceneSearchEdit_->setClearButtonEnabled(true);
    layout->addWidget(sceneSearchEdit_);

    // The tree view is created in the constructor (object name xqSceneTreeView,
    // which test_main_window looks up); here we just give it its home.
    sceneTreeView_->setParent(panel);
    // Single-column tree (MITK-style, no header): the sole name column carries the
    // tree indent, the visibility checkbox, the icon, and the full display name,
    // stretching to fill the dock. Domain type and staleness live in the icon,
    // amber tint, and tooltip -- so dropping the old Type/Status columns frees the
    // width they stole and full names never truncate.
    sceneTreeView_->setHeaderHidden(true);
    sceneTreeView_->setAlternatingRowColors(true);
    sceneTreeView_->setContextMenuPolicy(Qt::CustomContextMenu);
    sceneTreeView_->header()->setStretchLastSection(false);
    sceneTreeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    QObject::connect(sceneTreeView_, &QWidget::customContextMenuRequested,
                     this, &XQMainWindow::onSceneContextMenu);
    layout->addWidget(sceneTreeView_, 1);

    // Presentation controls for the selected node: an opacity slider (0-100 %) and
    // a colour button (QColorDialog). Disabled until a renderable node is selected
    // (updatePresentationControls back-fills + enables them). Object names are the
    // ones test_main_window looks up.
    // A container widget (parented to panel) hosts the presentation grid, so the
    // grid's ownership is unambiguous and it tears down cleanly with the panel.
    QWidget* presPanel = new QWidget(panel);
    QGridLayout* presGrid = new QGridLayout(presPanel);
    presGrid->setContentsMargins(0, 0, 0, 0);
    presGrid->setHorizontalSpacing(6);
    presGrid->setVerticalSpacing(4);

    opacityLabel_ = new QLabel(presPanel);
    opacitySlider_ = new QSlider(Qt::Horizontal, presPanel);
    opacitySlider_->setObjectName(QStringLiteral("xqOpacitySlider"));
    opacitySlider_->setRange(0, 100);
    opacitySlider_->setValue(100);
    opacitySlider_->setEnabled(false);
    presGrid->addWidget(opacityLabel_, 0, 0);
    presGrid->addWidget(opacitySlider_, 0, 1);

    colorLabel_ = new QLabel(presPanel);
    colorButton_ = new QPushButton(presPanel);
    colorButton_->setObjectName(QStringLiteral("xqColorButton"));
    colorButton_->setEnabled(false);
    presGrid->addWidget(colorLabel_, 1, 0);
    presGrid->addWidget(colorButton_, 1, 1);
    presGrid->setColumnStretch(1, 1);
    layout->addWidget(presPanel);

    // Opacity edit -> render scene + repaint (acts on presentationNodeId_).
    QObject::connect(opacitySlider_, &QSlider::valueChanged, this, [this](int value) {
        if (renderScene_ == nullptr || !presentationNodeId_.is_valid()) {
            return;
        }
        renderScene_->setNodeOpacity(presentationNodeId_,
                                     static_cast<double>(value) / 100.0);
        if (mprWidget_ != nullptr) {
            mprWidget_->renderAll();
        }
    });
    // Colour button -> QColorDialog -> render scene + repaint.
    QObject::connect(colorButton_, &QPushButton::clicked, this, [this]() {
        if (renderScene_ == nullptr || !presentationNodeId_.is_valid()) {
            return;
        }
        double rgb[3] = {1.0, 1.0, 1.0};
        renderScene_->nodeColor(presentationNodeId_, rgb);
        const QColor initial =
            QColor::fromRgbF(rgb[0], rgb[1], rgb[2]);
        const QColor picked =
            QColorDialog::getColor(initial, this, tr("Select Color"));
        if (!picked.isValid()) {
            return;
        }
        renderScene_->setNodeColor(presentationNodeId_, picked.redF(),
                                   picked.greenF(), picked.blueF());
        colorButton_->setStyleSheet(
            QStringLiteral("background-color: %1").arg(picked.name()));
        if (mprWidget_ != nullptr) {
            mprWidget_->renderAll();
        }
    });

    panel->setLayout(layout);
    dock->setWidget(panel);
    dock->setMinimumWidth(kMinimumDockWidth);
    dock->setMaximumWidth(kMaximumDataManagerDockWidth);
    dock->setFixedWidth(kDefaultDataManagerDockWidth);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    dataManagerDock_ = dock;
    resizeDocks(QList<QDockWidget*>{dataManagerDock_},
                QList<int>{kDefaultDataManagerDockWidth},
                Qt::Horizontal);
}

void XQMainWindow::buildImageNavigatorDock()
{
    QDockWidget* dock = new QDockWidget(QStringLiteral("Image Navigator"), this);
    dock->setObjectName(QStringLiteral("xqImageNavigatorDock"));

    QWidget* panel = new QWidget(dock);
    QGridLayout* grid = new QGridLayout(panel);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 1);

    int row = 0;

    // Axial / Sagittal / Coronal slice sliders (object names the tests rely on)
    // and compact spin boxes mirroring the current slice index.
    QLabel** rowLabels[] = {&navAxialLabel_, &navSagittalLabel_, &navCoronalLabel_};
    const char* const sliderKeys[] = {"Axial", "Sagittal", "Coronal"};
    QSlider** sliders[] = {&axialSlider_, &sagittalSlider_, &coronalSlider_};
    QSpinBox** spins[] = {&axialSpin_, &sagittalSpin_, &coronalSpin_};
    for (int i = 0; i < 3; ++i) {
        QLabel* label = new QLabel(panel);
        *rowLabels[i] = label;
        QSlider* slider = new QSlider(Qt::Horizontal, panel);
        slider->setObjectName(QString(QStringLiteral("xqNav%1Slider"))
                                  .arg(QString::fromLatin1(sliderKeys[i])));
        slider->setRange(0, 100);
        slider->setValue(0);
        slider->setEnabled(false);
        *sliders[i] = slider;
        QSpinBox* spin = new QSpinBox(panel);
        spin->setObjectName(QString(QStringLiteral("xqNav%1Spin"))
                                .arg(QString::fromLatin1(sliderKeys[i])));
        spin->setRange(0, 0);
        spin->setValue(0);
        spin->setEnabled(false);
        spin->setMinimumWidth(0);
        spin->setFixedWidth(kNavigatorSpinWidth);
        *spins[i] = spin;
        QObject::connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
        QObject::connect(spin, qOverload<int>(&QSpinBox::valueChanged),
                         slider, &QSlider::setValue);
        grid->addWidget(label, row, 0);
        grid->addWidget(slider, row, 1, 1, 2);
        grid->addWidget(spin, row, 3);
        ++row;
    }

    // Window / Level numeric boxes, two-way bound to the render scene's shared
    // image property. Disabled until a real image loads (useVolume enables +
    // seeds them). A value edit pushes to the scene + repaints; a window/level
    // drag in a slice view pushes back here (windowLevelChanged), read below.
    navWindowLabel_ = new QLabel(panel);
    windowSpin_ = new QDoubleSpinBox(panel);
    windowSpin_->setObjectName(QStringLiteral("xqNavWindowSpin"));
    windowSpin_->setRange(1.0, 65535.0);
    windowSpin_->setDecimals(1);
    windowSpin_->setEnabled(false);
    grid->addWidget(navWindowLabel_, row, 0);
    grid->addWidget(windowSpin_, row, 1, 1, 3);
    ++row;

    navLevelLabel_ = new QLabel(panel);
    levelSpin_ = new QDoubleSpinBox(panel);
    levelSpin_->setObjectName(QStringLiteral("xqNavLevelSpin"));
    levelSpin_->setRange(-32768.0, 32767.0);
    levelSpin_->setDecimals(1);
    levelSpin_->setEnabled(false);
    grid->addWidget(navLevelLabel_, row, 0);
    grid->addWidget(levelSpin_, row, 1, 1, 3);
    ++row;

    // A spin edit -> push window/level into the scene + repaint.
    auto pushWindowLevel = [this]() {
        if (renderScene_ == nullptr || windowSpin_ == nullptr
            || levelSpin_ == nullptr) {
            return;
        }
        renderScene_->setWindowLevel(windowSpin_->value(), levelSpin_->value());
        if (mprWidget_ != nullptr) {
            mprWidget_->renderAll();
        }
    };
    QObject::connect(windowSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
                     this, pushWindowLevel);
    QObject::connect(levelSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged),
                     this, pushWindowLevel);

    // Loc.(mm): three world-space coordinate spins (x/y/z = render-scene axis
    // 0/1/2), two-way bound to the slice sliders. A slice change refreshes them
    // (syncLocReadout); an edit maps world -> voxel index and drives the sliders
    // (onNavLocEdited). Disabled until a real image loads (useVolume enables +
    // ranges them to the volume's world bounding box).
    navLocLabel_ = new QLabel(panel);
    grid->addWidget(navLocLabel_, row, 0);
    QDoubleSpinBox** locSpins[] = {&navLocXSpin_, &navLocYSpin_, &navLocZSpin_};
    const char* const locKeys[] = {"X", "Y", "Z"};
    QWidget* locRow = new QWidget(panel);
    QHBoxLayout* locLayout = new QHBoxLayout(locRow);
    locLayout->setContentsMargins(0, 0, 0, 0);
    locLayout->setSpacing(4);
    for (int i = 0; i < 3; ++i) {
        QDoubleSpinBox* spin = new QDoubleSpinBox(locRow);
        spin->setObjectName(QString(QStringLiteral("xqNavLoc%1"))
                                .arg(QString::fromLatin1(locKeys[i])));
        spin->setDecimals(2);
        // Wide provisional range; useVolume tightens it to the world bounding box.
        spin->setRange(-1.0e6, 1.0e6);
        spin->setValue(0.0);
        spin->setEnabled(false);
        *locSpins[i] = spin;
        locLayout->addWidget(spin);
        QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged),
                         this, &XQMainWindow::onNavLocEdited);
    }
    locRow->setLayout(locLayout);
    grid->addWidget(locRow, row, 1, 1, 3);
    ++row;

    panel->setLayout(grid);
    dock->setWidget(panel);
    dock->setMinimumWidth(kMinimumDockWidth);
    dock->setMaximumWidth(kMaximumDataManagerDockWidth);
    dock->setFixedWidth(kDefaultDataManagerDockWidth);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
    imageNavigatorDock_ = dock;
    imageNavigatorDock_->show();
    resizeDocks(QList<QDockWidget*>{imageNavigatorDock_},
                QList<int>{kDefaultDataManagerDockWidth},
                Qt::Horizontal);
}

void XQMainWindow::buildStatusBar()
{
    QStatusBar* bar = statusBar();
    statusPositionLabel_ = new QLabel(QString(), bar);
    statusPositionLabel_->setObjectName(QStringLiteral("xqStatusPosition"));
    bar->addWidget(statusPositionLabel_, 1);

    // Permanent right-aligned segments: memory usage, then node count.
    statusMemLabel_ = new QLabel(QString(), bar);
    statusMemLabel_->setObjectName(QStringLiteral("xqStatusMem"));
    bar->addPermanentWidget(statusMemLabel_);

    statusNodeCountLabel_ = new QLabel(QString(), bar);
    statusNodeCountLabel_->setObjectName(QStringLiteral("xqStatusNodeCount"));
    bar->addPermanentWidget(statusNodeCountLabel_);

    memoryTimer_ = new QTimer(this);
    memoryTimer_->setInterval(2000);
    QObject::connect(memoryTimer_, &QTimer::timeout,
                     this, &XQMainWindow::updateMemoryStatus);
    memoryTimer_->start();
    updateMemoryStatus();
}

void XQMainWindow::updateMemoryStatus()
{
    if (statusMemLabel_ == nullptr) {
        return;
    }
    PROCESS_MEMORY_COUNTERS pmc = {};
    std::size_t workingSetMiB = 0;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        workingSetMiB = static_cast<std::size_t>(pmc.WorkingSetSize >> 20);
    }
    if (geometryResources_ != nullptr) {
        const std::size_t residentMiB = geometryResources_->residentBytes() >> 20;
        statusMemLabel_->setText(tr("Mem %1 MB | Geometry %2 MB")
                                     .arg(workingSetMiB)
                                     .arg(residentMiB));
    } else {
        statusMemLabel_->setText(tr("Mem %1 MB").arg(workingSetMiB));
    }
}

void XQMainWindow::buildWindowMenu()
{
    if (windowMenu_ == nullptr) {
        return;
    }
    windowMenu_->clear();
    // Each dock contributes its built-in toggle action (checked = visible), so
    // the Window menu shows/hides panels and stays in sync automatically.
    if (dataManagerDock_ != nullptr) {
        windowMenu_->addAction(dataManagerDock_->toggleViewAction());
    }
    if (imageNavigatorDock_ != nullptr) {
        windowMenu_->addAction(imageNavigatorDock_->toggleViewAction());
    }
    if (stageDock_ != nullptr) {
        windowMenu_->addAction(stageDock_->toggleViewAction());
    }
    if (mainToolBar_ != nullptr) {
        windowMenu_->addAction(mainToolBar_->toggleViewAction());
    }
    windowMenu_->addSeparator();

    fullScreenAction_ = windowMenu_->addAction(QString());
    fullScreenAction_->setObjectName(QStringLiteral("xqFullScreenAction"));
    fullScreenAction_->setCheckable(true);
    fullScreenAction_->setShortcut(QKeySequence::FullScreen);
    QObject::connect(fullScreenAction_, &QAction::toggled, this, [this](bool on) {
        if (on) {
            showFullScreen();
        } else {
            showNormal();
        }
    });

    resetLayoutAction_ = windowMenu_->addAction(QString());
    resetLayoutAction_->setObjectName(QStringLiteral("xqResetLayoutAction"));
    QObject::connect(resetLayoutAction_, &QAction::triggered, this, &XQMainWindow::resetDockLayout);
}

void XQMainWindow::applyProductDockLayout()
{
    if (dataManagerDock_ != nullptr) {
        dataManagerDock_->setFloating(false);
        addDockWidget(Qt::LeftDockWidgetArea, dataManagerDock_);
        dataManagerDock_->show();
        dataManagerDock_->setMinimumWidth(kMinimumDockWidth);
        dataManagerDock_->setMaximumWidth(kMaximumDataManagerDockWidth);
        dataManagerDock_->setFixedWidth(kDefaultDataManagerDockWidth);
        resizeDocks(QList<QDockWidget*>{dataManagerDock_},
                    QList<int>{kDefaultDataManagerDockWidth},
                    Qt::Horizontal);
    }
    if (imageNavigatorDock_ != nullptr) {
        imageNavigatorDock_->setFloating(false);
        addDockWidget(Qt::LeftDockWidgetArea, imageNavigatorDock_);
        imageNavigatorDock_->show();
        imageNavigatorDock_->setMinimumWidth(kMinimumDockWidth);
        imageNavigatorDock_->setMaximumWidth(kMaximumDataManagerDockWidth);
        imageNavigatorDock_->setFixedWidth(kDefaultDataManagerDockWidth);
        if (dataManagerDock_ != nullptr) {
            splitDockWidget(dataManagerDock_, imageNavigatorDock_, Qt::Vertical);
        }
        resizeDocks(QList<QDockWidget*>{imageNavigatorDock_},
                    QList<int>{kDefaultDataManagerDockWidth},
                    Qt::Horizontal);
    }
    if (stageDock_ != nullptr) {
        stageDock_->setFloating(false);
        addDockWidget(Qt::RightDockWidgetArea, stageDock_);
        stageDock_->setMinimumWidth(kMinimumStageDockWidth);
        stageDock_->setMaximumWidth(kMaximumStageDockWidth);
        stageDock_->hide();
        resizeDocks(QList<QDockWidget*>{stageDock_},
                    QList<int>{kDefaultStageDockWidth},
                    Qt::Horizontal);
    }
}

void XQMainWindow::resetDockLayout()
{
    // Restore the MITK-like product docking arrangement: data and image
    // navigator stacked on the left; workflow stage is opened by tool buttons.
    if (dataManagerDock_ != nullptr) {
        dataManagerDock_->setFloating(false);
        dataManagerDock_->show();
        addDockWidget(Qt::LeftDockWidgetArea, dataManagerDock_);
    }
    if (imageNavigatorDock_ != nullptr) {
        imageNavigatorDock_->setFloating(false);
        addDockWidget(Qt::LeftDockWidgetArea, imageNavigatorDock_);
        imageNavigatorDock_->show();
    }
    if (stageDock_ != nullptr) {
        stageDock_->hide();
    }
    if (mainToolBar_ != nullptr) {
        mainToolBar_->show();
    }
    applyProductDockLayout();
}

void XQMainWindow::resizeToProductDefault()
{
    QSize target(kDefaultProductWindowWidth, kDefaultProductWindowHeight);
    const QRect available = availableScreenGeometryFor(this);
    if (available.isValid()) {
        target.setWidth(std::min(target.width(), std::max(640, available.width() - kScreenMargin)));
        target.setHeight(std::min(target.height(), std::max(520, available.height() - kScreenMargin)));
    }
    resize(target);
    if (available.isValid()) {
        move(available.x() + (available.width() - width()) / 2,
             available.y() + (available.height() - height()) / 2);
    }
}

void XQMainWindow::saveWindowState()
{
    QSettings settings(QStringLiteral("XQ"), QStringLiteral("XQ"));
    settings.setValue(QStringLiteral("window/productGeometryV4"), saveGeometry());
}

void XQMainWindow::restoreWindowState()
{
    QSettings settings(QStringLiteral("XQ"), QStringLiteral("XQ"));
    const QByteArray geometry =
        settings.value(QStringLiteral("window/productGeometryV4")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
        const QRect available = availableScreenGeometryFor(this);
        if (available.isValid() && !available.intersects(frameGeometry())) {
            resizeToProductDefault();
        }
    }
    // Dock state is not restored here. The workflow panel is attached after the
    // constructor, and stale saved dock state can hide newly added product panels
    // on a real Windows desktop. applyProductDockLayout() owns the default docks.
}

void XQMainWindow::closeEvent(QCloseEvent* event)
{
    saveWindowState();
    QMainWindow::closeEvent(event);
}

void XQMainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    if (stageDock_ == nullptr) {
        return;
    }
    applyProductDockLayout();
    QTimer::singleShot(50, this, [this]() {
        // Do not let the delayed platform-layout pass override a stage action
        // triggered immediately after the window becomes visible.
        if (stageDock_ != nullptr && stageDock_->isHidden()) {
            applyProductDockLayout();
        }
    });
}

void XQMainWindow::changeEvent(QEvent* event)
{
    if (event != nullptr && event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void XQMainWindow::setChineseLanguage(bool chinese)
{
    if (zhTranslator_ == nullptr) {
        return;
    }
    if (chinese && !zhTranslatorReady_) {
        qWarning().noquote()
            << QStringLiteral("[xq-i18n] Chinese language requested, but translator is not ready");
        if (langZhAction_ != nullptr) {
            const QSignalBlocker blocker(langZhAction_);
            langZhAction_->setChecked(false);
        }
        if (langEnAction_ != nullptr) {
            const QSignalBlocker blocker(langEnAction_);
            langEnAction_->setChecked(true);
        }
        return;
    }
    // Installing / removing the translator triggers a LanguageChange event that
    // re-runs retranslateUi() across every widget; we only flip the active one.
    if (chinese) {
        qApp->installTranslator(zhTranslator_);
    } else {
        qApp->removeTranslator(zhTranslator_);
    }
    if (langZhAction_ != nullptr) {
        langZhAction_->setChecked(chinese);
    }
    if (langEnAction_ != nullptr) {
        langEnAction_->setChecked(!chinese);
    }
}

void XQMainWindow::retranslateUi()
{
    // Menu titles.
    if (fileMenu_ != nullptr) {
        fileMenu_->setTitle(tr("&File"));
    }
    if (editMenu_ != nullptr) {
        editMenu_->setTitle(tr("&Edit"));
    }
    if (viewMenu_ != nullptr) {
        viewMenu_->setTitle(tr("&View"));
    }
    if (toolsMenu_ != nullptr) {
        toolsMenu_->setTitle(tr("&Tools"));
    }
    if (windowMenu_ != nullptr) {
        windowMenu_->setTitle(tr("&Window"));
    }
    if (helpMenu_ != nullptr) {
        helpMenu_->setTitle(tr("&Help"));
    }
    if (languageMenu_ != nullptr) {
        languageMenu_->setTitle(tr("&Language"));
    }
    if (workspaceTabs_ != nullptr && workspaceTabs_->count() > 0) {
        workspaceTabs_->setTabText(0, tr("Standard Display"));
    }

    // File / Tools / Help / Edit actions.
    if (openAction_ != nullptr) {
        openAction_->setText(tr("&Open Workspace..."));
    }
    if (openImageAction_ != nullptr) {
        openImageAction_->setText(tr("Open &Image (.vti)..."));
    }
    if (openDicomAction_ != nullptr) {
        openDicomAction_->setText(tr("Open &DICOM Series..."));
    }
    if (openSvProjectAction_ != nullptr) {
        openSvProjectAction_->setText(tr("Open Sim&Vascular Project..."));
    }
    if (saveAction_ != nullptr) {
        saveAction_->setText(tr("&Save Workspace"));
    }
    if (quitAction_ != nullptr) {
        quitAction_->setText(tr("&Quit"));
    }
    if (prefsAction_ != nullptr) {
        prefsAction_->setText(tr("&Preferences..."));
    }
    if (aboutAction_ != nullptr) {
        aboutAction_->setText(tr("&About XQ..."));
    }
    if (undoAction_ != nullptr) {
        undoAction_->setText(tr("&Undo"));
    }
    if (redoAction_ != nullptr) {
        redoAction_->setText(tr("&Redo"));
    }

    // View / Window actions.
    if (crosshairAction_ != nullptr) {
        crosshairAction_->setText(tr("Show MPR &Crosshair"));
    }
    if (planes3dAction_ != nullptr) {
        planes3dAction_->setText(tr("Show 3D Slice &Planes"));
    }
    const bool single = (viewLayoutAction_ != nullptr && viewLayoutAction_->isChecked());
    if (layoutMenuAction_ != nullptr) {
        layoutMenuAction_->setText(single ? tr("Quad &View") : tr("Single &View"));
    }
    if (viewLayoutAction_ != nullptr) {
        viewLayoutAction_->setText(single ? tr("Quad View") : tr("Single View"));
    }
    if (fullScreenAction_ != nullptr) {
        fullScreenAction_->setText(tr("&Full Screen"));
    }
    if (resetLayoutAction_ != nullptr) {
        resetLayoutAction_->setText(tr("&Reset Layout"));
    }
    if (langZhAction_ != nullptr) {
        langZhAction_->setText(QStringLiteral("中文"));
    }
    if (langEnAction_ != nullptr) {
        langEnAction_->setText(QStringLiteral("English"));
    }

    // Main toolbar buttons.
    if (tbOpenAction_ != nullptr) {
        tbOpenAction_->setText(tr("Open"));
        tbOpenAction_->setToolTip(tr("Open Workspace"));
    }
    if (tbSaveAction_ != nullptr) {
        tbSaveAction_->setText(tr("Save"));
        tbSaveAction_->setToolTip(tr("Save Workspace"));
    }
    if (tbUndoAction_ != nullptr) {
        tbUndoAction_->setText(tr("Undo"));
    }
    if (tbRedoAction_ != nullptr) {
        tbRedoAction_->setText(tr("Redo"));
    }
    if (tbImageAction_ != nullptr) {
        tbImageAction_->setText(tr("Image"));
    }
    if (tbPathAction_ != nullptr) {
        tbPathAction_->setText(tr("Path"));
    }
    if (tbSeg2dAction_ != nullptr) {
        tbSeg2dAction_->setText(tr("Seg"));
    }
    if (tbModelAction_ != nullptr) {
        tbModelAction_->setText(tr("Model"));
    }
    if (tbMeshAction_ != nullptr) {
        tbMeshAction_->setText(tr("Mesh"));
    }
    if (tbModulesAction_ != nullptr) {
        tbModulesAction_->setText(tr("Modules"));
    }

    // Toolbar title (shown as its Window-menu toggle action text).
    if (mainToolBar_ != nullptr) {
        mainToolBar_->setWindowTitle(tr("Main Toolbar"));
    }

    // Docks.
    if (dataManagerDock_ != nullptr) {
        dataManagerDock_->setWindowTitle(tr("Data Manager"));
    }
    if (imageNavigatorDock_ != nullptr) {
        imageNavigatorDock_->setWindowTitle(tr("Image Navigator"));
    }
    if (stageDock_ != nullptr) {
        stageDock_->setWindowTitle(tr("Stages"));
    }

    // Data Manager controls.
    if (sceneSearchEdit_ != nullptr) {
        sceneSearchEdit_->setPlaceholderText(tr("Search nodes..."));
    }

    // Image Navigator labels.
    if (navAxialLabel_ != nullptr) {
        navAxialLabel_->setText(tr("Axial"));
    }
    if (navSagittalLabel_ != nullptr) {
        navSagittalLabel_->setText(tr("Sagittal"));
    }
    if (navCoronalLabel_ != nullptr) {
        navCoronalLabel_->setText(tr("Coronal"));
    }
    if (navWindowLabel_ != nullptr) {
        navWindowLabel_->setText(tr("Window"));
    }
    if (navLevelLabel_ != nullptr) {
        navLevelLabel_->setText(tr("Level"));
    }
    if (navLocLabel_ != nullptr) {
        navLocLabel_->setText(tr("Loc. (mm)"));
    }

    // Data Manager presentation controls.
    if (opacityLabel_ != nullptr) {
        opacityLabel_->setText(tr("Opacity"));
    }
    if (colorLabel_ != nullptr) {
        colorLabel_->setText(tr("Color"));
    }
    if (colorButton_ != nullptr) {
        colorButton_->setText(tr("Select Color..."));
    }

    // Status bar: position placeholder + memory + node count. Only relabel the
    // position cell when it is showing the idle text; a live slice read-out is
    // left untouched (and will pick up the new language on its next update).
    if (statusPositionLabel_ != nullptr && statusShowingIdle_) {
        statusPositionLabel_->setText(tr("Ready"));
    }
    if (statusMemLabel_ != nullptr) {
        updateMemoryStatus();
    }
    if (statusNodeCountLabel_ != nullptr) {
        const int rows = (sceneModel_ != nullptr) ? sceneModel_->rowCount(QModelIndex()) : 0;
        statusNodeCountLabel_->setText(tr("Nodes: %1").arg(rows));
    }
}

void XQMainWindow::buildEditMenu()
{
    // editMenu_ was already created in buildMenus() (to hold its File | Edit |
    // View slot); here we only populate it, once a command stack is attached.
    // Idempotent: the undo/redo slots route through the resident session, so a
    // workflow re-attach needs no re-wiring -- populate only once.
    if (undoAction_ != nullptr) {
        return;
    }
    if (editMenu_ == nullptr) {
        editMenu_ = menuBar()->addMenu(QString());
    }

    undoAction_ = editMenu_->addAction(QString());
    undoAction_->setObjectName(QStringLiteral("xqUndoAction"));
    undoAction_->setShortcut(QKeySequence::Undo);
    QObject::connect(undoAction_, &QAction::triggered, this, &XQMainWindow::undo);

    redoAction_ = editMenu_->addAction(QString());
    redoAction_->setObjectName(QStringLiteral("xqRedoAction"));
    redoAction_->setShortcut(QKeySequence::Redo);
    QObject::connect(redoAction_, &QAction::triggered, this, &XQMainWindow::redo);

    // The Edit menu is built lazily by attachWorkflow, after the constructor's
    // first retranslateUi(); refresh so its strings honour the active language.
    retranslateUi();
}

void XQMainWindow::useVolume(const XQImageVolume& image,
                             const XQMemoryImageBufferHandle* buffer)
{
    // Size the navigator sliders to the volume's per-axis extent, defaulting to
    // the mid-slice. Block signals so wiring them doesn't trigger 3 re-renders
    // before the image is set.
    QSlider* sliders[3] = {sagittalSlider_, coronalSlider_, axialSlider_}; // axis 0,1,2
    QSpinBox* spins[3] = {sagittalSpin_, coronalSpin_, axialSpin_};
    if (mprWidget_ != nullptr) {
        mprWidget_->setSeedPickingEnabled(false);
    }
    renderScene_->setVolume(image, buffer);
    for (int axis = 0; axis < 3; ++axis) {
        QSlider* slider = sliders[axis];
        if (slider == nullptr) {
            continue;
        }
        const int count = renderScene_->sliceCount(axis);
        const int index = renderScene_->sliceIndex(axis);
        const QSignalBlocker blocker(slider);
        slider->setRange(0, count > 0 ? count - 1 : 0);
        slider->setValue(index >= 0 ? index : 0);
        slider->setEnabled(count > 0);
        QSpinBox* spin = spins[axis];
        if (spin != nullptr) {
            const QSignalBlocker spinBlocker(spin);
            spin->setRange(0, count > 0 ? count - 1 : 0);
            spin->setValue(index >= 0 ? index : 0);
            spin->setEnabled(count > 0);
        }
    }

    // Seed the window/level spins from the scene's initial (full-window) values
    // and enable them (blocked so seeding does not push straight back).
    if (windowSpin_ != nullptr && levelSpin_ != nullptr) {
        double window = 0.0;
        double level = 0.0;
        renderScene_->windowLevel(&window, &level);
        const QSignalBlocker windowBlocker(windowSpin_);
        const QSignalBlocker levelBlocker(levelSpin_);
        windowSpin_->setValue(window);
        levelSpin_->setValue(level);
        windowSpin_->setEnabled(true);
        levelSpin_->setEnabled(true);
    }

    // Loc.(mm): range each spin to the volume's world bounding box (axis-aligned
    // assumption, same as worldToSliceIndex), enable them, and seed them from the
    // current slice-plane world coordinates. An out-of-box edit is still clamped
    // by worldToVoxelIndex and read back in-range.
    {
        const ImageGeometry& geom = image.geometry();
        QDoubleSpinBox* locSpins[3] = {navLocXSpin_, navLocYSpin_, navLocZSpin_};
        for (int axis = 0; axis < 3; ++axis) {
            QDoubleSpinBox* spin = locSpins[axis];
            if (spin == nullptr) {
                continue;
            }
            const int count = geom.dimensions[axis];
            const double a = geom.origin[axis];
            const double b = geom.origin[axis]
                + (count > 0 ? (count - 1) : 0) * geom.spacing[axis]
                      * geom.direction[axis][axis];
            const QSignalBlocker blocker(spin);
            spin->setRange(std::min(a, b), std::max(a, b));
            spin->setValue(renderScene_->sliceWorldCoord(axis));
            spin->setEnabled(count > 0);
        }
    }

    // Push the image node's current checkbox state onto the freshly built volume
    // so the tree stays authoritative over image visibility (default Checked ->
    // this is a no-op; but if the user had unchecked the image, a volume rebuild
    // must not silently show it again).
    if (renderScene_ != nullptr && sceneModel_ != nullptr) {
        if (const XQScene* scene = sceneModel_->scene()) {
            scene->visit_nodes([this](const XQDataNode& node) {
                if (node.domainType() == XQDomainType::Image) {
                    renderScene_->setImageVisible(sceneModel_->nodeVisible(node.id()));
                }
            });
        }
    }

    // Re-point the section view at the newly resident volume so the next entry
    // into the contour-extraction stage reslices the current image.
    if (crossSectionView_ != nullptr) {
        crossSectionView_->refreshImage();
    }

    centralStack_->setCurrentWidget(mprWidget_);
    if (mprWidget_ != nullptr) {
        mprWidget_->renderAll();
    }

    // An image just became available: nudge the Path page so its step-by-step
    // guidance advances off "open an image" without waiting for a draft edit.
    if (pathDraftChanged_) {
        pathDraftChanged_();
    }
}

void XQMainWindow::openImageFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Image"), QString(), tr("VTK Image (*.vti)"));
    if (path.isEmpty()) {
        return; // user cancelled
    }

    loadImageFromPath(path);
}

void XQMainWindow::openDicomSeries()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this, tr("Open DICOM Series"));
    if (directory.isEmpty()) {
        return;
    }

    struct DiscoveryOutcome {
        DicomSeriesDiscoveryResult discovery;
    };
    const std::string directoryUtf8 = directory.toStdString();
    const bool queued = session_->taskRunner().run(
        tr("Discovering DICOM series..."),
        [directoryUtf8]() -> std::shared_ptr<void> {
            auto outcome = std::make_shared<DiscoveryOutcome>();
            GdcmItkDicomSeriesReader reader;
            outcome->discovery = reader.discover(directoryUtf8);
            return outcome;
        },
        [this, directory](std::shared_ptr<void> raw) {
            const auto outcome =
                std::static_pointer_cast<DiscoveryOutcome>(raw);
            if (!outcome->discovery.ok()
                || outcome->discovery.series.empty()) {
                const QString status = QString::fromLatin1(
                    dicomSeriesStatusToken(outcome->discovery.status));
                reportWorkspaceError(
                    tr("Open DICOM Series"),
                    tr("Could not discover DICOM series: %1.").arg(status));
                return;
            }

            QStringList labels;
            for (const DicomSeriesDescriptor& descriptor
                 : outcome->discovery.series) {
                // The safe label contains only technical metadata, but two
                // series can still collide after UID abbreviation. Add a
                // deterministic ordinal so text-based selection never falls
                // back to the first matching series.
                labels.push_back(QStringLiteral("%1  [#%2]")
                    .arg(QString::fromStdString(descriptor.safeDisplayName))
                    .arg(labels.size() + 1));
            }
            bool accepted = false;
            const QString selected = QInputDialog::getItem(
                this, tr("Open DICOM Series"), tr("Series"), labels,
                0, false, &accepted);
            if (!accepted) {
                return;
            }
            const int index = labels.indexOf(selected);
            if (index < 0
                || index >= static_cast<int>(outcome->discovery.series.size())) {
                return;
            }
            const QString seriesUid = QString::fromStdString(
                outcome->discovery.series[static_cast<std::size_t>(index)]
                    .identity.seriesInstanceUid);
            QTimer::singleShot(0, this, [this, directory, seriesUid]() {
                loadDicomSeriesFromDirectory(directory, seriesUid);
            });
        });
    if (!queued && statusPositionLabel_ != nullptr) {
        statusShowingIdle_ = false;
        statusPositionLabel_->setText(
            tr("Another task is still running."));
    }
}

void XQMainWindow::appendPathDraftPointForTest(const Point3& world)
{
    pathDraftPoints_.push_back(PathControlPoint{world});
    if (pathDraftChanged_) {
        pathDraftChanged_();
    }
}

NodeId XQMainWindow::appendContourToGroupForTest(
    const NodeId& contourGroupId,
    XQContour contour)
{
    return appendContourToGroup(contourGroupId, std::move(contour));
}

std::size_t XQMainWindow::addPathDraftAtCrosshairForTest()
{
    // Mirror the Ctrl+A shortcut exactly (crosshair voxel -> draft point).
    if (pickMode_ != PickMode::PathPoint || renderScene_ == nullptr) {
        return pathDraftPoints_.size();
    }
    const int i = renderScene_->sliceIndex(0);
    const int j = renderScene_->sliceIndex(1);
    const int k = renderScene_->sliceIndex(2);
    if (i >= 0 && j >= 0 && k >= 0) {
        addPathDraftVoxel(i, j, k);
    }
    return pathDraftPoints_.size();
}

void XQMainWindow::addPathDraftVoxel(int i, int j, int k)
{
    if (!activeImage_) {
        return;
    }
    const double voxel[3] = {static_cast<double>(i),
                             static_cast<double>(j),
                             static_cast<double>(k)};
    double world[3] = {0.0, 0.0, 0.0};
    if (activeImage_->image.voxelToWorld(voxel, world)
        != XQImageVolume::TransformStatus::Ok) {
        return;
    }
    pathDraftPoints_.push_back(PathControlPoint{{world[0], world[1], world[2]}});
    if (pathDraftChanged_) {
        pathDraftChanged_();
    }
}

void XQMainWindow::syncPathControlMarkers()
{
    if (renderScene_ == nullptr) {
        return;
    }
    std::vector<std::array<double, 3>> world;
    world.reserve(pathDraftPoints_.size());
    for (const PathControlPoint& point : pathDraftPoints_) {
        world.push_back({point.position.x, point.position.y, point.position.z});
    }
    renderScene_->setPathControlPoints(world);
    if (mprWidget_ != nullptr) {
        mprWidget_->renderAll();
    }
}

void XQMainWindow::focusPathDraftPoint(int index)
{
    if (renderScene_ == nullptr || !renderScene_->hasVolume()) {
        return;
    }
    if (index < 0 || static_cast<std::size_t>(index) >= pathDraftPoints_.size()) {
        return;
    }
    const Point3& p = pathDraftPoints_[static_cast<std::size_t>(index)].position;
    const double world[3] = {p.x, p.y, p.z};
    int ijk[3] = {0, 0, 0};
    if (!renderScene_->worldToVoxelIndex(world, &ijk[0], &ijk[1], &ijk[2])) {
        return;
    }
    // Same slider/spin drive as onNavLocEdited: block both so we re-slice + refresh
    // once via onNavigatorSliderChanged (not three times), and keep the spins in
    // step with the sliders (blocking the slider alone would starve its spin). The
    // slider order is {Sagittal, Coronal, Axial} = axis 0/1/2. Centres all three
    // slice planes on the point so its marker lands on each view's current slice.
    QSlider* sliders[3] = {sagittalSlider_, coronalSlider_, axialSlider_};
    QSpinBox* spins[3] = {sagittalSpin_, coronalSpin_, axialSpin_};
    for (int axis = 0; axis < 3; ++axis) {
        if (QSlider* slider = sliders[axis]) {
            const QSignalBlocker blocker(slider);
            slider->setValue(ijk[axis]);
        }
        if (QSpinBox* spin = spins[axis]) {
            const QSignalBlocker blocker(spin);
            spin->setValue(ijk[axis]);
        }
    }
    onNavigatorSliderChanged();
}

void XQMainWindow::exitPicking()
{
    if (pickMode_ == PickMode::None) {
        return;
    }
    // Reset the Path page toggle (signal-blocked on the page side), then drop
    // pick mode + MPR routing ourselves so the state is consistent.
    if (pathPickToggleSetter_) {
        pathPickToggleSetter_(false);
    }
    pickMode_ = PickMode::None;
    if (mprWidget_ != nullptr) {
        mprWidget_->setSeedPickingEnabled(false);
    }
    if (mprWidget_ != nullptr) {
        mprWidget_->setModeHint(QString()); // hide the picking-mode overlay
    }
}

void XQMainWindow::exitPickingForTest()
{
    exitPicking();
}

bool XQMainWindow::loadImageFromPath(const QString& path)
{
    // Fast-fail on a missing file synchronously (keeps the "file not found" modal
    // semantics). The heavy .vti read + decompress now rides the background parse
    // queue so it never freezes the GUI thread; a corrupt / unreadable image is
    // reported from the commit (status bar), matching the model/contour parse
    // failure path. Returns true = the load was queued.
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("Open Image"),
                             tr("Could not open the image: %1.").arg(tr("file not found")));
        return false;
    }

    // Queue an image-decode job that, on the GUI commit, seeds a fresh Image node
    // (File > Open Image) bound to the decoded volume. displayName is the file
    // name; nodeId is assigned at commit time (maxId+1) so it never collides with
    // nodes another queued job may add first.
    PendingParse job;
    job.path = path;
    job.kind = ParseKind::Image;
    job.seedImageNode = (session_->scene() != nullptr
                         && session_->commandStack() != nullptr);
    job.displayName = QFileInfo(path).fileName();
    pendingParses_.push_back(std::move(job));
    startNextParse();
    return true;
}

bool XQMainWindow::loadDicomSeriesFromDirectory(
    const QString& directory,
    const QString& seriesInstanceUid)
{
    namespace fs = std::filesystem;
    if (workflowBusy_ || directory.isEmpty() || seriesInstanceUid.isEmpty()
        || !QFileInfo(directory).isDir()) {
        return false;
    }
    XQProject* project = activeProject();
    if (project == nullptr
        || project->state() != XQProject::LifecycleState::Open
        || session_->scene() != &project->scene()
        || session_->commandStack() == nullptr) {
        reportWorkspaceError(
            tr("Open DICOM Series"),
            tr("No active project is available for DICOM import."));
        return false;
    }

    const NodeId nodeId = allocateSceneObjectId();
    const AssetId assetId = allocateUnusedAssetId(project->assetRegistry());
    if (!nodeId.is_valid() || !assetId.is_valid()) {
        reportWorkspaceError(
            tr("Open DICOM Series"),
            tr("Could not allocate stable DICOM project identifiers."));
        return false;
    }

    DicomImportRequest request;
    request.sourceDirectory = fs::absolute(
        fs::path(directory.toStdString())).lexically_normal().string();
    request.sourceRelPath = projectRelativeSourceDirectory(
        fs::path(request.sourceDirectory), workspacePath_);
    request.seriesInstanceUid = seriesInstanceUid.toStdString();
    request.nodeId = nodeId;
    request.assetId = assetId;
    XQProject* const capturedProject = project;
    const std::uint64_t capturedEpoch = project->lifecycleEpoch();

    struct ImportOutcome {
        DicomImportResult import;
        std::shared_ptr<XQDemoVolume> displayVolume;
    };

    const bool queued = session_->taskRunner().run(
        tr("Loading DICOM series..."),
        [request]() -> std::shared_ptr<void> {
            auto outcome = std::make_shared<ImportOutcome>();
            GdcmItkDicomSeriesReader reader;
            outcome->import = DicomImportService::prepare(reader, request);
            if (outcome->import.ok()) {
                const auto image = std::dynamic_pointer_cast<
                    XQImageVolumePayload>(
                    outcome->import.batchSpec->node.payload());
                if (image != nullptr) {
                    outcome->displayVolume = materializeDemoVolume(
                        image->volume(), *outcome->import.residentSource);
                }
            }
            return outcome;
        },
        [this, capturedProject, capturedEpoch, nodeId, assetId, directory](
            std::shared_ptr<void> raw) {
            auto outcome = std::static_pointer_cast<ImportOutcome>(raw);
            if (!outcome->import.ok() || outcome->displayVolume == nullptr) {
                if (statusPositionLabel_ != nullptr) {
                    statusShowingIdle_ = false;
                    statusPositionLabel_->setText(
                        tr("DICOM import failed: %1")
                            .arg(QString::fromLatin1(dicomSeriesStatusToken(
                                outcome->import.status))));
                }
                return;
            }
            XQProject* project = activeProject();
            if (project != capturedProject
                || project == nullptr
                || project->lifecycleEpoch() != capturedEpoch
                || project->state() != XQProject::LifecycleState::Open
                || project->scene().find(nodeId) != nullptr
                || project->assetRegistry().find(assetId) != nullptr) {
                if (statusPositionLabel_ != nullptr) {
                    statusShowingIdle_ = false;
                    statusPositionLabel_->setText(
                        tr("The project changed while DICOM was loading."));
                }
                return;
            }

            if (geometryResources_ == nullptr
                || !geometryResources_->usesAssetRegistry(
                    &project->assetRegistry())) {
                geometryRegistry_ = &project->assetRegistry();
                geometryResources_ = std::make_unique<GeometryResourceManager>(
                    geometryRegistry_, assetRootDir_);
                geometryResources_->setBudgetBytes(
                    static_cast<std::size_t>(
                        XQPreferencesDialog::geometryBudgetMiB()) << 20);
            }

            std::shared_ptr<const IVoxelSource> resident =
                outcome->import.residentSource;
            if (!session_->pushCommand(
                    std::make_unique<ProjectNodeBatchCommand>(
                        project,
                        std::move(outcome->import.batchSpec.value()),
                        "Import DICOM series"))) {
                if (statusPositionLabel_ != nullptr) {
                    statusShowingIdle_ = false;
                    statusPositionLabel_->setText(
                        tr("The DICOM project commit was rejected."));
                }
                return;
            }
            const GeometryResourceManager::VoxelSourceHandle installed =
                geometryResources_->installResidentVoxelSource(
                    assetId, std::move(resident));
            if (!installed.valid()) {
                if (statusPositionLabel_ != nullptr) {
                    statusShowingIdle_ = false;
                    statusPositionLabel_->setText(
                        tr("DICOM metadata was imported, but voxel residency failed."));
                }
                return;
            }

            activeImage_ = std::make_unique<XQDemoVolume>(
                std::move(*outcome->displayVolume));
            activeImageNodeId_ = nodeId;
            activeProjectDir_ = directory;
            useVolume(activeImage_->image, activeImage_->buffer.get());
            refreshSceneTree();
        });
    return queued;
}

void XQMainWindow::openSvProject()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open SimVascular Project"));
    if (dir.isEmpty()) {
        return; // user cancelled
    }

    loadSvProjectFromDirectory(dir);
}

bool XQMainWindow::loadSvProjectFromDirectory(const QString& dir)
{
    XQProjectReadResult result;
    const SvProjectReader::Status status = SvProjectReader::load(dir.toStdString(), &result);
    if (status != SvProjectReader::Status::Ok) {
        const QString reason = (status == SvProjectReader::Status::ProjectFileNotFound)
            ? tr("no .sv project file was found in that folder")
            : tr("the project could not be parsed");
        QMessageBox::warning(this, tr("Open SimVascular Project"),
                             tr("Could not open the project: %1.").arg(reason));
        return false;
    }

    if (session_->scene() == nullptr || session_->commandStack() == nullptr) {
        QMessageBox::warning(this, tr("Open SimVascular Project"),
                             tr("No active workspace to load into."));
        return false;
    }
    activeProjectDir_ = dir;

    // Merge the loaded project's nodes into the live scene (undoable). Allocate
    // fresh ids past the current max so they never collide with existing nodes;
    // clone each payload so the live scene owns its own copies. Remember the
    // first image node + its source path to display its volume in the MPR.
    NodeId::ValueType maxId = 0;
    session_->scene()->visit_nodes([&maxId](const XQDataNode& n) {
        if (n.id().value() > maxId) {
            maxId = n.id().value();
        }
    });

    int seeded = 0;
    QString imagePath;
    NodeId imageNodeId;
    // Reset the background-parse queue: this load owns it (a previous project's
    // pending entries are stale).
    pendingParses_.clear();
    result.project.scene().visit_nodes(
        [this, &maxId, &seeded, &imagePath, &imageNodeId](const XQDataNode& src) {
            const NodeId newId(++maxId);
            std::shared_ptr<XQPayload> payload =
                src.payload() ? src.payload()->clone() : nullptr;
            XQDataNode node(newId, src.domainType(), src.display_name(), payload);
            const std::shared_ptr<XQSourcePayload> sourcePayload =
                std::dynamic_pointer_cast<XQSourcePayload>(payload);
            if (src.domainType() == XQDomainType::Image && imagePath.isEmpty()) {
                if (sourcePayload != nullptr) {
                    imagePath = QString::fromStdString(sourcePayload->sourcePath());
                    imageNodeId = newId;
                }
            }
            // A surface-model (.mdl) or contour-group (.ctgr) node loaded from an
            // SV project carries an unresolved XQSourcePayload (the file path,
            // relative to the project). Queue it for background parsing so the
            // geometry appears without a user click. Resolve to an absolute path
            // now (activeProjectDir_ is already set above).
            if ((src.domainType() == XQDomainType::SurfaceModel
                 || src.domainType() == XQDomainType::ContourGroup)
                && sourcePayload != nullptr) {
                PendingParse parse;
                parse.nodeId = newId;
                parse.path = resolveSourcePath(sourcePayload->sourcePath());
                parse.kind = src.domainType() == XQDomainType::SurfaceModel
                    ? ParseKind::Model
                    : ParseKind::Contour;
                parse.expectedRevision = node.contentRevision();
                parse.expectedSourcePayload = payload;
                parse.expectedSourcePath = sourcePayload->sourcePath();
                pendingParses_.push_back(std::move(parse));
            }
            session_->commandStack()->push(std::make_unique<AddNodeCommand>(
                session_->scene(), std::move(node), "Open SimVascular project"));
            ++seeded;
        });
    refreshSceneTree();

    // If the project carries an image, queue its decode at the FRONT of the parse
    // queue so the underlay appears before the surface/contour geometry (the user
    // sees the base image first). The image node already exists (seeded above), so
    // the commit binds the decoded volume to imageNodeId rather than seeding a new
    // node. A large .vti read no longer freezes the GUI thread.
    if (!imagePath.isEmpty()) {
        PendingParse imageJob;
        imageJob.nodeId = imageNodeId;
        imageJob.path = resolveSourcePath(imagePath.toStdString());
        imageJob.kind = ParseKind::Image;
        imageJob.seedImageNode = false;
        imageJob.displayName = QFileInfo(imageJob.path).fileName();
        pendingParses_.insert(pendingParses_.begin(), std::move(imageJob));
    }

    // Kick off background image / .mdl / .ctgr parsing (no-op when the queue is
    // empty).
    startNextParse();

    if (statusPositionLabel_ != nullptr) {
        statusPositionLabel_->setText(tr("Loaded project: %1 node(s)").arg(seeded));
    }
    return true;
}

void XQMainWindow::startNextParse()
{
    if (pendingParses_.empty() || session_->scene() == nullptr) {
        return;
    }
    // Take a BATCH off the front: all consecutive jobs of the same kind (B4c).
    // A large SV project queues one .ctgr job per contour group (143 for
    // 0080_H_PULM_H); running them one-at-a-time meant 143 task turns, each
    // flipping the global busy state and rebuilding the whole scene tree in
    // refreshSceneTree(). Merging same-kind jobs into a single runner task (one
    // busy flip, one tree rebuild for the batch) is what SV effectively does with
    // its synchronous single-pass load. Image jobs stay a batch of one: each binds
    // / seeds a node with its own commit logic, and there is normally only one.
    const ParseKind kind = pendingParses_.front().kind;
    std::vector<PendingParse> batch;
    if (kind == ParseKind::Image) {
        batch.push_back(pendingParses_.front());
        pendingParses_.erase(pendingParses_.begin());
    } else {
        while (!pendingParses_.empty() && pendingParses_.front().kind == kind) {
            batch.push_back(pendingParses_.front());
            pendingParses_.erase(pendingParses_.begin());
        }
    }
    const PendingParse job = batch.front();
    const NodeId nodeId = job.nodeId;
    const std::string pathStd = job.path.toStdString();

    // Defers the next job to the next event-loop turn: the runner clears busy_
    // *after* this commit returns, so a direct run() here would be refused.
    auto kickNext = [this]() {
        QMetaObject::invokeMethod(
            this, [this]() { startNextParse(); }, Qt::QueuedConnection);
    };

    struct SourceMaterializationBatchItem {
        NodeId nodeId;
        std::string path;
        ContentRevision expectedRevision = 0;
        std::shared_ptr<const XQPayload> expectedSourcePayload;
        std::string expectedSourcePath;
    };

    if (job.kind == ParseKind::Image) {
        // Image (.vti): decode the volume on the worker thread (the heavy read +
        // decompress that used to freeze the GUI), then bind it on the commit.
        const bool seedImageNode = job.seedImageNode;
        const QString displayName = job.displayName;
        struct ImageLoadOutcome {
            VtkImageAdapter::LoadStatus status =
                VtkImageAdapter::LoadStatus::ReadFailed;
            std::shared_ptr<XQDemoVolume> volume;
        };
        session_->taskRunner().run(
            tr("Loading image..."),
            [pathStd]() -> std::shared_ptr<void> {
                // Worker thread: read + decode the file only. The XQDemoVolume
                // (image + scalar buffer) is a plain data holder with no scene /
                // widget / VTK-pipeline coupling, so building it here is safe; the
                // GPU upload (useVolume -> setVolume) stays on the GUI thread.
                auto outcome = std::make_shared<ImageLoadOutcome>();
                outcome->volume = std::make_shared<XQDemoVolume>();
                outcome->status = VtkImageAdapter::loadVtiWithBuffer(
                    pathStd, &outcome->volume->image, &outcome->volume->buffer);
                return outcome;
            },
            [this, nodeId, seedImageNode, displayName, pathStd, kickNext](
                std::shared_ptr<void> raw) {
                auto outcome = std::static_pointer_cast<ImageLoadOutcome>(raw);
                if (outcome->status == VtkImageAdapter::LoadStatus::Ok) {
                    // Hold the decoded volume + buffer so the MPR view's borrowed
                    // pointers stay valid; release any previously loaded image.
                    activeImage_ = std::make_unique<XQDemoVolume>(
                        std::move(*outcome->volume));
                    // GPU upload + navigator wiring on the GUI thread.
                    useVolume(activeImage_->image, activeImage_->buffer.get());

                    if (seedImageNode && session_->scene() != nullptr
                        && session_->commandStack() != nullptr) {
                        // File > Open Image: seed a fresh Image node (undoable)
                        // carrying a source-path payload, matching the old
                        // synchronous path. Its id is maxId+1 as of now.
                        NodeId::ValueType maxId = 0;
                        session_->scene()->visit_nodes([&maxId](const XQDataNode& n) {
                            if (n.id().value() > maxId) {
                                maxId = n.id().value();
                            }
                        });
                        const NodeId imageId(maxId + 1);
                        XQDataNode node(
                            imageId, XQDomainType::Image, displayName.toStdString(),
                            std::make_shared<XQSourcePayload>(
                                XQDomainType::Image, pathStd));
                        session_->commandStack()->push(
                            std::make_unique<AddNodeCommand>(
                                session_->scene(), std::move(node), "Open image"));
                        activeImageNodeId_ = imageId;
                        refreshSceneTree();
                    } else {
                        // SV project / node reload: bind to the existing node.
                        activeImageNodeId_ = nodeId;
                    }

                    const ImageGeometry& g = activeImage_->image.geometry();
                    if (statusPositionLabel_ != nullptr) {
                        statusShowingIdle_ = false;
                        statusPositionLabel_->setText(
                            tr("Loaded %1  (%2 x %3 x %4)")
                                .arg(displayName)
                                .arg(g.dimensions[0])
                                .arg(g.dimensions[1])
                                .arg(g.dimensions[2]));
                    }
                } else if (statusPositionLabel_ != nullptr) {
                    statusShowingIdle_ = false;
                    QString reason;
                    switch (outcome->status) {
                    case VtkImageAdapter::LoadStatus::FileNotFound:
                        reason = tr("file not found");
                        break;
                    case VtkImageAdapter::LoadStatus::ReadFailed:
                        reason = tr("the file could not be read");
                        break;
                    case VtkImageAdapter::LoadStatus::InvalidImage:
                        reason = tr("the file is not a valid image");
                        break;
                    case VtkImageAdapter::LoadStatus::Ok:
                        break;
                    }
                    statusPositionLabel_->setText(
                        tr("Could not open the image: %1.").arg(reason));
                }
                kickNext();
            });
    } else if (job.kind == ParseKind::Model) {
        // Batch: (nodeId, path) for every .mdl in this run. Captured by value into
        // the worker thunk; the worker reads every file and hands the GUI commit a
        // per-file result vector so a single tree rebuild covers the whole batch.
        auto items =
            std::make_shared<std::vector<SourceMaterializationBatchItem>>();
        for (const PendingParse& p : batch) {
            SourceMaterializationBatchItem item;
            item.nodeId = p.nodeId;
            item.path = p.path.toStdString();
            item.expectedRevision = p.expectedRevision;
            item.expectedSourcePayload = p.expectedSourcePayload;
            item.expectedSourcePath = p.expectedSourcePath;
            items->push_back(std::move(item));
        }
        struct ModelFileOutcome {
            SourceMaterializationBatchItem item;
            MDLModelReader::Status status = MDLModelReader::Status::MdlParseError;
            MDLReadResult result;
        };
        session_->taskRunner().run(
            tr("Parsing models (%1)...").arg(static_cast<int>(items->size())),
            [items]() -> std::shared_ptr<void> {
                // Worker thread: read every file in the batch. No scene / widget /
                // render / QSettings access; the reader objects are local to read().
                auto outcomes = std::make_shared<std::vector<ModelFileOutcome>>();
                outcomes->reserve(items->size());
                for (const SourceMaterializationBatchItem& item : *items) {
                    ModelFileOutcome fo;
                    fo.item = item;
                    fo.status = MDLModelReader::read(item.path, &fo.result);
                    outcomes->push_back(std::move(fo));
                }
                return outcomes;
            },
            [this, kickNext](std::shared_ptr<void> raw) {
                auto outcomes =
                    std::static_pointer_cast<std::vector<ModelFileOutcome>>(raw);
                int applied = 0;
                int failed = 0;
                QString firstFailName;
                for (ModelFileOutcome& fo : *outcomes) {
                    if (fo.status == MDLModelReader::Status::Ok) {
                        // Commit only against the exact unresolved source state
                        // captured when queued. A semantic edit may leave another
                        // XQSourcePayload in place, so type checking alone is not a
                        // sufficient concurrency guard.
                        XQDataNode* node =
                            session_->scene()->find(fo.item.nodeId);
                        if (sourceMaterializationStillCurrent(
                                node,
                                XQDomainType::SurfaceModel,
                                fo.item.expectedRevision,
                                fo.item.expectedSourcePayload,
                                fo.item.expectedSourcePath)) {
                            node->setPayload(
                                XQDomainType::SurfaceModel,
                                std::make_shared<XQSurfaceModelPayload>(
                                    std::move(fo.result.model)));
                            ++applied;
                        }
                    } else {
                        ++failed;
                        if (firstFailName.isEmpty()) {
                            firstFailName =
                                QString::fromStdString(fo.result.modelName);
                        }
                    }
                }
                // One tree rebuild + render sync for the whole batch, not one per
                // file, so the surfaces appear with no user action.
                if (applied > 0) {
                    refreshSceneTree();
                }
                // Failures are summarised in a single status line (per-file lines
                // would just overwrite each other in a large batch).
                if (failed > 0 && statusPositionLabel_ != nullptr) {
                    statusPositionLabel_->setText(
                        failed == 1
                            ? tr("Model parse failed: %1").arg(firstFailName)
                            : tr("Model parse failed: %1 (and %2 more)")
                                  .arg(firstFailName)
                                  .arg(failed - 1));
                }
                kickNext();
            });
    } else {
        // Contour (.ctgr) batch: same shape as the model batch above. Parse each
        // into an XQContourGroup, then swap every unresolved source payload for a
        // real XQContourGroupPayload with one tree rebuild for the whole run.
        auto items =
            std::make_shared<std::vector<SourceMaterializationBatchItem>>();
        for (const PendingParse& p : batch) {
            SourceMaterializationBatchItem item;
            item.nodeId = p.nodeId;
            item.path = p.path.toStdString();
            item.expectedRevision = p.expectedRevision;
            item.expectedSourcePayload = p.expectedSourcePayload;
            item.expectedSourcePath = p.expectedSourcePath;
            items->push_back(std::move(item));
        }
        struct ContourFileOutcome {
            SourceMaterializationBatchItem item;
            CTGRContourReader::Status status = CTGRContourReader::Status::ParseError;
            CTGRReadResult result;
        };
        session_->taskRunner().run(
            tr("Parsing contours (%1)...").arg(static_cast<int>(items->size())),
            [items]() -> std::shared_ptr<void> {
                auto outcomes =
                    std::make_shared<std::vector<ContourFileOutcome>>();
                outcomes->reserve(items->size());
                for (const SourceMaterializationBatchItem& item : *items) {
                    ContourFileOutcome fo;
                    fo.item = item;
                    fo.status = CTGRContourReader::read(item.path, &fo.result);
                    outcomes->push_back(std::move(fo));
                }
                return outcomes;
            },
            [this, kickNext](std::shared_ptr<void> raw) {
                auto outcomes =
                    std::static_pointer_cast<std::vector<ContourFileOutcome>>(raw);
                int applied = 0;
                int failed = 0;
                QString firstFailName;
                for (ContourFileOutcome& fo : *outcomes) {
                    if (fo.status == CTGRContourReader::Status::Ok) {
                        XQDataNode* node =
                            session_->scene()->find(fo.item.nodeId);
                        if (sourceMaterializationStillCurrent(
                                node,
                                XQDomainType::ContourGroup,
                                fo.item.expectedRevision,
                                fo.item.expectedSourcePayload,
                                fo.item.expectedSourcePath)) {
                            node->setPayload(
                                XQDomainType::ContourGroup,
                                std::make_shared<XQContourGroupPayload>(
                                    std::move(fo.result.group)));
                            ++applied;
                        }
                    } else {
                        ++failed;
                        if (firstFailName.isEmpty()) {
                            firstFailName =
                                QString::fromStdString(fo.result.groupName);
                        }
                    }
                }
                if (applied > 0) {
                    refreshSceneTree();
                }
                if (failed > 0 && statusPositionLabel_ != nullptr) {
                    statusPositionLabel_->setText(
                        failed == 1
                            ? tr("Contour parse failed: %1").arg(firstFailName)
                            : tr("Contour parse failed: %1 (and %2 more)")
                                  .arg(firstFailName)
                                  .arg(failed - 1));
                }
                kickNext();
            });
    }
}

void XQMainWindow::onNavigatorSliderChanged()
{
    if (renderScene_ == nullptr || renderScene_->sliceCount(0) <= 0
        || renderScene_->sliceCount(1) <= 0 || renderScene_->sliceCount(2) <= 0) {
        return;
    }
    // Map each slider to its slice axis (0=Sagittal/x,1=Coronal/y,2=Axial/z).
    if (sagittalSlider_ != nullptr) {
        renderScene_->setSliceIndex(0, sagittalSlider_->value());
    }
    if (coronalSlider_ != nullptr) {
        renderScene_->setSliceIndex(1, coronalSlider_->value());
    }
    if (axialSlider_ != nullptr) {
        renderScene_->setSliceIndex(2, axialSlider_->value());
    }
    centralStack_->setCurrentWidget(mprWidget_);
    if (mprWidget_ != nullptr) {
        mprWidget_->renderAll();
    }

    // Reflect the new slice position: refresh the Loc.(mm) read-out and the
    // status-bar Position (both driven off the current slice-plane world coords).
    syncLocReadout();
    updatePositionReadout();
}

void XQMainWindow::syncLocReadout()
{
    if (renderScene_ == nullptr || !renderScene_->hasVolume()) {
        return;
    }
    QDoubleSpinBox* locSpins[3] = {navLocXSpin_, navLocYSpin_, navLocZSpin_};
    for (int axis = 0; axis < 3; ++axis) {
        QDoubleSpinBox* spin = locSpins[axis];
        if (spin == nullptr) {
            continue;
        }
        const QSignalBlocker blocker(spin);
        spin->setValue(renderScene_->sliceWorldCoord(axis));
    }
}

void XQMainWindow::onNavLocEdited()
{
    if (renderScene_ == nullptr || !renderScene_->hasVolume()
        || navLocXSpin_ == nullptr || navLocYSpin_ == nullptr
        || navLocZSpin_ == nullptr) {
        return;
    }
    // World point from the three Loc spins -> nearest voxel index (clamped into
    // the volume). i/j/k follow the image index axes, which equal render-scene
    // slice axes 0/1/2 = Sagittal/Coronal/Axial.
    const double world[3] = {navLocXSpin_->value(), navLocYSpin_->value(),
                             navLocZSpin_->value()};
    int ijk[3] = {0, 0, 0};
    if (!renderScene_->worldToVoxelIndex(world, &ijk[0], &ijk[1], &ijk[2])) {
        return;
    }
    // Drive the three slice sliders (blocked here so we re-slice + refresh once
    // via the explicit onNavigatorSliderChanged() below, not three times). The
    // slider order is {Sagittal, Coronal, Axial} = axis 0/1/2.
    QSlider* sliders[3] = {sagittalSlider_, coronalSlider_, axialSlider_};
    QSpinBox* spins[3] = {sagittalSpin_, coronalSpin_, axialSpin_};
    for (int axis = 0; axis < 3; ++axis) {
        if (QSlider* slider = sliders[axis]) {
            const QSignalBlocker blocker(slider);
            slider->setValue(ijk[axis]);
        }
        if (QSpinBox* spin = spins[axis]) {
            const QSignalBlocker blocker(spin);
            spin->setValue(ijk[axis]);
        }
    }
    // Apply the new slice indices + repaint, and refresh the Loc spins from the
    // resulting slice-plane world coords -- an out-of-range edit thus reads back
    // as the clamped in-volume position.
    onNavigatorSliderChanged();
}

void XQMainWindow::updatePositionReadout()
{
    if (!showStatusCoordinates_ || statusPositionLabel_ == nullptr
        || renderScene_ == nullptr || !renderScene_->hasVolume()) {
        return;
    }
    // Position is the resident idle read-out: only write it while idle so a busy /
    // loading message is never clobbered, and do not flip statusShowingIdle_ here.
    if (!statusShowingIdle_) {
        return;
    }
    statusPositionLabel_->setText(tr("Position: <%1, %2, %3> mm")
                                      .arg(renderScene_->sliceWorldCoord(0), 0, 'f', 2)
                                      .arg(renderScene_->sliceWorldCoord(1), 0, 'f', 2)
                                      .arg(renderScene_->sliceWorldCoord(2), 0, 'f', 2));
}

void XQMainWindow::showAboutDialog()
{
    XQAboutDialog dialog(this);
    dialog.exec();
}

void XQMainWindow::showPreferencesDialog()
{
    XQPreferencesDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        applyPreferences();
    }
}

void XQMainWindow::applyPreferences()
{
    sliceStep_ = XQPreferencesDialog::sliceStep();
    showStatusCoordinates_ = XQPreferencesDialog::showStatusCoordinates();

    const bool crosshair = XQPreferencesDialog::crosshairEnabled();
    if (crosshairAction_ != nullptr) {
        crosshairAction_->setChecked(crosshair); // toggled() applies it to the view
    }
    if (mprWidget_ != nullptr) {
        mprWidget_->setCrosshairVisible(crosshair);

        const bool single = XQPreferencesDialog::defaultSingleView();
        if (viewLayoutAction_ != nullptr && viewLayoutAction_->isChecked() != single) {
            viewLayoutAction_->setChecked(single);
            toggleViewLayout();
        }
    }

    if (geometryResources_ != nullptr) {
        geometryResources_->setBudgetBytes(
            static_cast<std::size_t>(XQPreferencesDialog::geometryBudgetMiB()) << 20);
    }
}

void XQMainWindow::stepAxialSlice(int delta)
{
    if (mprWidget_ == nullptr || axialSlider_ == nullptr) {
        return;
    }
    // Drive through the navigator slider so the slider, the MPR slice and the
    // info overlay all stay in sync.
    axialSlider_->setValue(axialSlider_->value() + delta * sliceStep_);
}

void XQMainWindow::showStagePage(int index)
{
    if (stagePanel_ == nullptr) {
        return;
    }
    if (index < 0 || index >= stagePanel_->count()) {
        return;
    }
    stagePanel_->setCurrentIndex(index);

    // Every primary stage opens against the four-view MPR. The independent
    // two-dimensional contour command switches to the cross-section workbench
    // explicitly, so it cannot replace the ROI-v2 product entry by accident.
    if (mprWidget_ != nullptr) {
        centralStack_->setCurrentWidget(mprWidget_);
    }

    if (stageDock_ != nullptr && stageDock_->widget() != stagePanel_) {
        stageDock_->setWidget(stagePanel_);
    }

    if (stageDock_ != nullptr) {
        stageDock_->setFloating(false);
        addDockWidget(Qt::RightDockWidgetArea, stageDock_);
        stageDock_->setMinimumWidth(kMinimumStageDockWidth);
        stageDock_->setMaximumWidth(kMaximumStageDockWidth);
        stageDock_->show();
        stageDock_->raise();
        resizeDocks(QList<QDockWidget*>{stageDock_},
                    QList<int>{kDefaultStageDockWidth},
                    Qt::Horizontal);
    }
}

void XQMainWindow::showContourWorkbench()
{
    if (workflowBusy_ || stagePanel_ == nullptr
        || crossSectionWorkbench_ == nullptr || crossSectionSegPanel_ == nullptr) {
        return;
    }

    // Reuse the primary stage path to restore stable dock sizing and keep the
    // Segmentation page selected, then swap only the dedicated 2-D surfaces.
    showStagePage(1);
    centralStack_->setCurrentWidget(crossSectionWorkbench_);
    bindCrossSectionPath();
    if (stageDock_ != nullptr && stageDock_->widget() != crossSectionSegPanel_) {
        stageDock_->setWidget(crossSectionSegPanel_);
        stageDock_->show();
        stageDock_->raise();
        resizeDocks(QList<QDockWidget*>{stageDock_},
                    QList<int>{kDefaultStageDockWidth},
                    Qt::Horizontal);
    }
}

void XQMainWindow::preselectStageSource(int pageIndex, const NodeId& id)
{
    if (stagePanel_ == nullptr) {
        return;
    }
    // Map the stage index to its source picker's object name. Only the pages that
    // consume an upstream node (Modeling <- ContourGroup, Meshing <- SurfaceModel)
    // carry a picker; anything else is a no-op.
    const char* objectName = nullptr;
    switch (pageIndex) {
    case 2:
        objectName = "xqModelingSourceCombo";
        break;
    case 3:
        objectName = "xqMeshingSourceCombo";
        break;
    default:
        return;
    }
    NodeComboBox* combo =
        stagePanel_->findChild<NodeComboBox*>(QString::fromLatin1(objectName));
    if (combo == nullptr) {
        return;
    }
    // Refresh from the live scene first so a just-created node is offered, then
    // select the requested id (leaves the current selection when it is absent).
    combo->repopulate();
    const int index =
        combo->findData(QVariant(static_cast<qulonglong>(id.value())));
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

void XQMainWindow::activateStageFromNode(int pageIndex, const NodeId& id)
{
    // Raise the target stage page, then preselect the node in its picker. Shared
    // by the right-click actions and the test entry point so both behave alike.
    showStagePage(pageIndex);
    preselectStageSource(pageIndex, id);
}

void XQMainWindow::activateStageFromNodeForTest(const NodeId& id, XQDomainType domain)
{
    if (domain == XQDomainType::ContourGroup) {
        activateStageFromNode(2, id); // Modeling
    } else if (domain == XQDomainType::SurfaceModel) {
        activateStageFromNode(3, id); // Meshing
    }
}

void XQMainWindow::toggleViewLayout()
{
    if (mprWidget_ == nullptr) {
        return;
    }
    const bool single = (viewLayoutAction_ != nullptr && viewLayoutAction_->isChecked());
    mprWidget_->setLayoutMode(single ? XQMprWidget::LayoutMode::Single
                                     : XQMprWidget::LayoutMode::Quad);
    if (viewLayoutAction_ != nullptr) {
        viewLayoutAction_->setText(single ? tr("Quad View") : tr("Single View"));
    }
    if (layoutMenuAction_ != nullptr) {
        layoutMenuAction_->setText(single ? tr("Quad &View") : tr("Single &View"));
    }
    centralStack_->setCurrentWidget(mprWidget_);
    mprWidget_->renderAll();
}

void XQMainWindow::buildCrossSectionWorkbench()
{
    crossSectionWorkbench_ = new QWidget(this);
    crossSectionWorkbench_->setObjectName(QStringLiteral("xqCrossSectionWorkbench"));
    crossSectionWorkbench_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    QVBoxLayout* layout = new QVBoxLayout(crossSectionWorkbench_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Top bar: a connected operation band -- pick a path + new group, then the
    // along-path slider, then the drawing method + edit controls.
    QWidget* topBar = new QWidget(crossSectionWorkbench_);
    QVBoxLayout* topLayout = new QVBoxLayout(topBar);
    topLayout->setContentsMargins(8, 6, 8, 6);
    topLayout->setSpacing(6);

    // Row 1: bind-path picker + "new contour group" (fixes P3-1's hardcoded
    // "first Path" pick). The lister reads the live scene at popup time.
    QHBoxLayout* bindRow = new QHBoxLayout();
    bindRow->setSpacing(8);
    SceneNodeLister pathLister = [this](XQDomainType domain) {
        std::vector<SceneNodeOption> options;
        const XQScene* scene = sceneModel_ != nullptr ? sceneModel_->scene() : nullptr;
        if (scene == nullptr) {
            return options;
        }
        scene->visit_nodes([&options, domain](const XQDataNode& node) {
            if (node.domainType() == domain) {
                options.push_back(
                    SceneNodeOption{node.id(), QString::fromStdString(node.display_name())});
            }
        });
        return options;
    };
    NodeComboBox* pathCombo = new NodeComboBox(pathLister, XQDomainType::Path, topBar);
    pathCombo->setObjectName(QStringLiteral("xqContourPathCombo"));
    contourPathCombo_ = pathCombo;
    contourPathCombo_->setToolTip(QCoreApplication::translate(
        "XQStageWidgets", "The centerline path this contour group follows."));

    contourGroupCreateBtn_ = new QPushButton(topBar);
    contourGroupCreateBtn_->setObjectName(QStringLiteral("xqContourGroupCreateBtn"));
    contourGroupCreateBtn_->setText(
        QCoreApplication::translate("XQStageWidgets", "New contour group"));

    bindRow->addWidget(new QLabel(
        QCoreApplication::translate("XQStageWidgets", "Path:"), topBar), 0);
    bindRow->addWidget(contourPathCombo_, 1);
    bindRow->addWidget(contourGroupCreateBtn_, 0);
    topLayout->addLayout(bindRow);

    // Row 2: along-path slider + "point N / M, arc length X mm" read-out
    // (SV sv4guiResliceSlider). Disabled until a path is bound.
    QHBoxLayout* sliderRow = new QHBoxLayout();
    sliderRow->setSpacing(8);
    alongPathSlider_ = new QSlider(Qt::Horizontal, topBar);
    alongPathSlider_->setObjectName(QStringLiteral("xqAlongPathSlider"));
    alongPathSlider_->setRange(0, 0);
    alongPathSlider_->setEnabled(false);

    alongPathLabel_ = new QLabel(topBar);
    alongPathLabel_->setObjectName(QStringLiteral("xqAlongPathLabel"));
    alongPathLabel_->setMinimumWidth(180);
    alongPathLabel_->setText(QCoreApplication::translate("XQStageWidgets",
                                                         "No path bound"));
    sliderRow->addWidget(alongPathSlider_, 1);
    sliderRow->addWidget(alongPathLabel_, 0);
    topLayout->addLayout(sliderRow);

    // The segmentation method / parameter controls (threshold / region-grow /
    // level-set + the manual drawing tools) no longer live in this top bar: they
    // were moved to the vertical crossSectionSegPanel_ docked on the right
    // (buildCrossSectionSegPanel, P3-5b). The top bar now carries only the section
    // navigation (path picker + new group, along-path slider + arc read-out).

    layout->addWidget(topBar, 0);

    // Center: the resident-reslice section view.
    crossSectionView_ = new XQCrossSectionViewWidget(renderScene_.get(),
                                                     crossSectionWorkbench_);
    crossSectionView_->setObjectName(QStringLiteral("xqCrossSectionView"));
    layout->addWidget(crossSectionView_, 1);

    QObject::connect(alongPathSlider_, &QSlider::valueChanged, this,
                     [this](int value) { updateCrossSectionAtSample(value); });

    // New contour group: bind the picked path, set active, rebind the section.
    QObject::connect(contourGroupCreateBtn_, &QPushButton::clicked, this,
                     [this]() { createContourGroupFromPicker(); });

    // The segmentation method controls + their signal connections now live in
    // buildCrossSectionSegPanel() (P3-5b). Only the section-view connections stay
    // here, wired to crossSectionView_ which is created above.

    // A clicked seed becomes the current segmentation seed for both auto methods.
    QObject::connect(crossSectionView_, &XQCrossSectionViewWidget::seedPicked, this,
                     [this](double u, double v) { onSeedPicked(u, v); });

    // Finished drawing: map section 2D control points to world and add a contour.
    QObject::connect(crossSectionView_, &XQCrossSectionViewWidget::contourDrawn, this,
                     [this](DrawMethod method, const QVector<QPointF>& points) {
                         addDrawnContour(method, points);
                     });

    centralStack_->addWidget(crossSectionWorkbench_);
    updateContourWorkbenchState();
}

void XQMainWindow::buildCrossSectionSegPanel()
{
    // The independent two-dimensional contour method panel (P3-5b). Parented to
    // the window so it outlives every attachWorkflow rebuild of stagePanel_. The
    // ROI-v2 page opens it explicitly; toolbar Segmentation never routes here.
    crossSectionSegPanel_ = new QWidget(this);
    crossSectionSegPanel_->setObjectName(QStringLiteral("xqCrossSectionSegPanel"));

    QVBoxLayout* panelLayout = new QVBoxLayout(crossSectionSegPanel_);
    panelLayout->setContentsMargins(8, 8, 8, 8);
    panelLayout->setSpacing(8);

    QLabel* segTitle = new QLabel(
        QCoreApplication::translate("XQStageWidgets", "Segmentation"),
        crossSectionSegPanel_);
    panelLayout->addWidget(segTitle, 0);

    // --- Automatic methods (primary workflow) ---
    QLabel* autoTitle = new QLabel(
        QCoreApplication::translate("XQStageWidgets", "Automatic"),
        crossSectionSegPanel_);
    panelLayout->addWidget(autoTitle, 0);

    // Automatic threshold method: a plain (non-checkable) button that executes on
    // click; it does NOT join the manual-drawing method group / draw mode.
    contourMethodThresholdBtn_ = new QToolButton(crossSectionSegPanel_);
    contourMethodThresholdBtn_->setObjectName(QStringLiteral("xqContourMethodThreshold"));
    contourMethodThresholdBtn_->setText(
        QCoreApplication::translate("XQStageWidgets", "Threshold"));
    panelLayout->addWidget(contourMethodThresholdBtn_, 0);

    thresholdLabel_ = new QLabel(crossSectionSegPanel_);
    thresholdLabel_->setObjectName(QStringLiteral("xqThresholdLabel"));
    thresholdLabel_->setText(
        QCoreApplication::translate("XQStageWidgets", "Threshold:"));
    panelLayout->addWidget(thresholdLabel_, 0);

    thresholdSlider_ = new QSlider(Qt::Horizontal, crossSectionSegPanel_);
    thresholdSlider_->setObjectName(QStringLiteral("xqThresholdSlider"));
    thresholdSlider_->setRange(0, 1000);
    thresholdSlider_->setValue(900);   // P90 default position (see estimateThreshold)
    thresholdSlider_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    panelLayout->addWidget(thresholdSlider_, 0);

    // Automatic region-grow method (P3-4): like Threshold, a plain (non-checkable)
    // button that runs on click. The band slider maps to a fraction of the
    // section's value range (previewRegionGrowContour); its label previews the
    // band. "Region grow" reuses the existing XQStageWidgets translation.
    contourMethodRegionGrowBtn_ = new QToolButton(crossSectionSegPanel_);
    contourMethodRegionGrowBtn_->setObjectName(
        QStringLiteral("xqContourMethodRegionGrow"));
    contourMethodRegionGrowBtn_->setText(
        QCoreApplication::translate("XQStageWidgets", "Region grow"));
    panelLayout->addWidget(contourMethodRegionGrowBtn_, 0);

    regionGrowBandLabel_ = new QLabel(crossSectionSegPanel_);
    regionGrowBandLabel_->setObjectName(QStringLiteral("xqRegionGrowBandLabel"));
    regionGrowBandLabel_->setText(
        QCoreApplication::translate("XQStageWidgets", "Band:"));
    panelLayout->addWidget(regionGrowBandLabel_, 0);

    regionGrowBandSlider_ = new QSlider(Qt::Horizontal, crossSectionSegPanel_);
    regionGrowBandSlider_->setObjectName(QStringLiteral("xqRegionGrowBandSlider"));
    regionGrowBandSlider_->setRange(0, 1000);
    regionGrowBandSlider_->setValue(300);   // a moderate inclusion band by default
    regionGrowBandSlider_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    panelLayout->addWidget(regionGrowBandSlider_, 0);

    // Automatic level-set method (P3-5): like Threshold / Region grow, a plain
    // (non-checkable) button that runs on click. The iteration slider maps to the
    // Chan-Vese evolution step count (previewLevelSetContour); its label previews
    // the count. Because one evolution is expensive, the slider re-evolves on
    // RELEASE only (connected below), unlike the threshold / band sliders.
    contourMethodLevelSetBtn_ = new QToolButton(crossSectionSegPanel_);
    contourMethodLevelSetBtn_->setObjectName(
        QStringLiteral("xqContourMethodLevelSet"));
    contourMethodLevelSetBtn_->setText(
        QCoreApplication::translate("XQStageWidgets", "Level set"));
    panelLayout->addWidget(contourMethodLevelSetBtn_, 0);

    levelSetIterLabel_ = new QLabel(crossSectionSegPanel_);
    levelSetIterLabel_->setObjectName(QStringLiteral("xqLevelSetIterLabel"));
    levelSetIterLabel_->setText(
        QCoreApplication::translate("XQStageWidgets", "Iterations:"));
    panelLayout->addWidget(levelSetIterLabel_, 0);

    levelSetIterSlider_ = new QSlider(Qt::Horizontal, crossSectionSegPanel_);
    levelSetIterSlider_->setObjectName(QStringLiteral("xqLevelSetIterSlider"));
    levelSetIterSlider_->setRange(0, 1000);
    levelSetIterSlider_->setValue(400);   // a moderate iteration count by default
    levelSetIterSlider_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    panelLayout->addWidget(levelSetIterSlider_, 0);

    // Manual seed pick (P3-4): a checkable button that toggles the section view
    // into SeedPick draw mode, letting the user click a seed for both auto
    // methods (region-grow / threshold both anchor on sectionSeed_).
    contourSeedPickBtn_ = new QToolButton(crossSectionSegPanel_);
    contourSeedPickBtn_->setObjectName(QStringLiteral("xqContourSeedPick"));
    contourSeedPickBtn_->setCheckable(true);
    contourSeedPickBtn_->setText(
        QCoreApplication::translate("XQStageWidgets", "Seed"));
    panelLayout->addWidget(contourSeedPickBtn_, 0);

    // --- Manual drawing tools ---
    QLabel* manualTitle = new QLabel(
        QCoreApplication::translate("XQStageWidgets", "Manual"),
        crossSectionSegPanel_);
    panelLayout->addWidget(manualTitle, 0);

    QButtonGroup* methodGroup = new QButtonGroup(crossSectionSegPanel_);
    methodGroup->setExclusive(true);

    contourMethodCircleBtn_ = new QToolButton(crossSectionSegPanel_);
    contourMethodCircleBtn_->setObjectName(QStringLiteral("xqContourMethodCircle"));
    contourMethodCircleBtn_->setCheckable(true);
    contourMethodCircleBtn_->setText(
        QCoreApplication::translate("XQStageWidgets", "Circle"));

    contourMethodPolygonBtn_ = new QToolButton(crossSectionSegPanel_);
    contourMethodPolygonBtn_->setObjectName(QStringLiteral("xqContourMethodPolygon"));
    contourMethodPolygonBtn_->setCheckable(true);
    contourMethodPolygonBtn_->setText(
        QCoreApplication::translate("XQStageWidgets", "Polygon"));

    methodGroup->addButton(contourMethodCircleBtn_);
    methodGroup->addButton(contourMethodPolygonBtn_);

    contourEditToggle_ = new QToolButton(crossSectionSegPanel_);
    contourEditToggle_->setObjectName(QStringLiteral("xqContourEditToggle"));
    contourEditToggle_->setCheckable(true);
    contourEditToggle_->setText(
        QCoreApplication::translate("XQStageWidgets", "Draw"));

    // Circle / Polygon / Draw share one horizontal row.
    QHBoxLayout* manualRow = new QHBoxLayout();
    manualRow->setSpacing(8);
    manualRow->addWidget(contourMethodCircleBtn_, 0);
    manualRow->addWidget(contourMethodPolygonBtn_, 0);
    manualRow->addWidget(contourEditToggle_, 0);
    manualRow->addStretch(1);
    panelLayout->addLayout(manualRow);

    panelLayout->addStretch(1);

    // --- Signal connections (moved verbatim from buildCrossSectionWorkbench) ---
    // Method buttons engage drawing (auto-checking the edit toggle); the toggle
    // enters/leaves the drawing mode.
    QObject::connect(contourMethodCircleBtn_, &QToolButton::clicked, this, [this]() {
        if (crossSectionView_ != nullptr) {
            crossSectionView_->setDrawMethod(DrawMethod::Circle);
        }
        if (contourEditToggle_ != nullptr) {
            contourEditToggle_->setChecked(true);
        }
    });
    QObject::connect(contourMethodPolygonBtn_, &QToolButton::clicked, this, [this]() {
        if (crossSectionView_ != nullptr) {
            crossSectionView_->setDrawMethod(DrawMethod::Polygon);
        }
        if (contourEditToggle_ != nullptr) {
            contourEditToggle_->setChecked(true);
        }
    });
    QObject::connect(contourEditToggle_, &QToolButton::toggled, this, [this](bool on) {
        if (crossSectionView_ == nullptr) {
            return;
        }
        if (!on) {
            crossSectionView_->setDrawMethod(DrawMethod::None);
            return;
        }
        // Entering edit with no method chosen defaults to Circle.
        if (contourMethodCircleBtn_ != nullptr && contourMethodPolygonBtn_ != nullptr
            && !contourMethodCircleBtn_->isChecked()
            && !contourMethodPolygonBtn_->isChecked()) {
            contourMethodCircleBtn_->setChecked(true);
            crossSectionView_->setDrawMethod(DrawMethod::Circle);
        } else if (contourMethodPolygonBtn_ != nullptr
                   && contourMethodPolygonBtn_->isChecked()) {
            crossSectionView_->setDrawMethod(DrawMethod::Polygon);
        } else {
            crossSectionView_->setDrawMethod(DrawMethod::Circle);
        }
    });

    // Threshold method: the button commits the current preview contour to the
    // group. Dragging the slider re-traces a live preview on the section so the
    // user can watch the lumen contour grow/shrink and pick a good threshold
    // (a single global threshold is hard to guess on non-uniform images).
    QObject::connect(contourMethodThresholdBtn_, &QToolButton::clicked, this,
                     [this]() { runThresholdOnCurrentSection(); });
    QObject::connect(thresholdSlider_, &QSlider::valueChanged, this,
                     [this](int) { updateThresholdPreview(); });

    // Region grow: the button commits the current preview; the band slider drives
    // a live preview so the user can scrub the inclusion band and watch the region
    // grow/shrink (same automatic-first affordance as threshold).
    QObject::connect(contourMethodRegionGrowBtn_, &QToolButton::clicked, this,
                     [this]() { runRegionGrowOnCurrentSection(); });
    QObject::connect(regionGrowBandSlider_, &QSlider::valueChanged, this,
                     [this](int) { updateRegionGrowPreview(); });

    // Level set: the button commits the current preview contour to the group. A
    // single Chan-Vese evolution is expensive, so -- unlike the threshold / band
    // sliders which re-trace on every valueChanged -- the iteration slider only
    // re-evolves on RELEASE (sliderReleased). Dragging updates just the label
    // text (valueChanged), so the count is visible while scrubbing without paying
    // for an evolution per tick.
    QObject::connect(contourMethodLevelSetBtn_, &QToolButton::clicked, this,
                     [this]() { runLevelSetOnCurrentSection(); });
    QObject::connect(levelSetIterSlider_, &QSlider::sliderReleased, this,
                     [this]() { updateLevelSetPreview(); });
    QObject::connect(levelSetIterSlider_, &QSlider::valueChanged, this,
                     [this](int value) {
                         // Only the label follows the drag; no evolution here.
                         if (levelSetIterLabel_ == nullptr) {
                             return;
                         }
                         const double frac =
                             static_cast<double>(value) / 1000.0;
                         const int iterations =
                             30 + static_cast<int>(std::lround(frac * 270.0));
                         levelSetIterLabel_->setText(
                             QCoreApplication::translate("XQStageWidgets",
                                                         "Iterations: %1")
                                 .arg(iterations));
                     });
    // Seed pick: toggling on puts the section view into SeedPick draw mode; off
    // returns to view-only. Turning it on clears the manual draw toggle so a
    // single mode is active at a time (SeedPick is its own mode, not in the
    // manual method group).
    QObject::connect(contourSeedPickBtn_, &QToolButton::toggled, this, [this](bool on) {
        if (crossSectionView_ == nullptr) {
            return;
        }
        // Clear the manual draw toggle FIRST: its toggled(false) slot would set
        // the draw method back to None and clobber SeedPick if run afterwards.
        if (on && contourEditToggle_ != nullptr) {
            contourEditToggle_->setChecked(false);
        }
        crossSectionView_->setDrawMethod(on ? DrawMethod::SeedPick : DrawMethod::None);
    });

    updateContourWorkbenchState();
}

void XQMainWindow::bindCrossSectionPath()
{
    sectionPathSamples_.clear();

    const XQScene* scene = sceneModel_ != nullptr ? sceneModel_->scene() : nullptr;

    // Resolve the path to drive the section from: the active contour group's
    // bound sourcePathNode (P3-2). With no active group / no bound path, fall
    // back to the first Path node so the workbench still shows a section before a
    // group is created.
    NodeId pathNode;
    if (scene != nullptr && activeContourGroup_.is_valid()) {
        if (const XQDataNode* groupNode = scene->find(activeContourGroup_)) {
            if (std::shared_ptr<XQContourGroupPayload> groupPayload =
                    std::dynamic_pointer_cast<XQContourGroupPayload>(groupNode->payload())) {
                if (groupPayload->group().hasSourcePathNode()) {
                    pathNode = groupPayload->group().sourcePathNode();
                }
            }
        }
    }

    if (scene != nullptr) {
        if (pathNode.is_valid()) {
            if (const XQDataNode* node = scene->find(pathNode)) {
                if (std::shared_ptr<XQPathPayload> payload =
                        std::dynamic_pointer_cast<XQPathPayload>(node->payload())) {
                    sectionPathSamples_ = payload->path().samplePoints();
                }
            }
        } else {
            // No active group yet: drive from the first Path node in the scene.
            scene->visit_nodes([this](const XQDataNode& node) {
                if (!sectionPathSamples_.empty()) {
                    return;
                }
                if (node.domainType() != XQDomainType::Path) {
                    return;
                }
                const auto payload =
                    std::dynamic_pointer_cast<XQPathPayload>(node.payload());
                if (payload == nullptr) {
                    return;
                }
                const std::vector<PathSamplePoint>& samples =
                    payload->path().samplePoints();
                if (!samples.empty()) {
                    sectionPathSamples_ = samples;
                }
            });
        }
    }

    // Re-attach the current resident volume before reslicing.
    if (crossSectionView_ != nullptr) {
        crossSectionView_->refreshImage();
    }

    updateContourWorkbenchState();

    const int count = static_cast<int>(sectionPathSamples_.size());
    if (alongPathSlider_ != nullptr) {
        const QSignalBlocker blocker(alongPathSlider_);
        alongPathSlider_->setRange(0, count > 0 ? count - 1 : 0);
        alongPathSlider_->setValue(0);
        alongPathSlider_->setEnabled(count > 0);
    }

    if (count > 0) {
        updateCrossSectionAtSample(0);
    } else if (alongPathLabel_ != nullptr) {
        alongPathLabel_->setText(
            QCoreApplication::translate("XQStageWidgets", "No path bound"));
    }
}

void XQMainWindow::updateCrossSectionAtSample(int index)
{
    if (crossSectionView_ == nullptr || index < 0
        || index >= static_cast<int>(sectionPathSamples_.size())) {
        return;
    }
    const PathSamplePoint& sample = sectionPathSamples_[static_cast<std::size_t>(index)];

    XQCrossSectionPose pose{};
    pose.origin[0] = sample.position.x;
    pose.origin[1] = sample.position.y;
    pose.origin[2] = sample.position.z;
    pose.tangent[0] = sample.tangent.x;
    pose.tangent[1] = sample.tangent.y;
    pose.tangent[2] = sample.tangent.z;
    pose.normal[0] = sample.normal.x;
    pose.normal[1] = sample.normal.y;
    pose.normal[2] = sample.normal.z;
    pose.binormal[0] = sample.binormal.x;
    pose.binormal[1] = sample.binormal.y;
    pose.binormal[2] = sample.binormal.z;

    crossSectionView_->setSampleFrame(pose);
    refreshSectionContourOverlay();
    // The section changed, so any threshold preview from the previous plane is
    // stale; clear it (the user re-drags the slider to preview on the new plane).
    crossSectionView_->setPreviewContour(QVector<QVector<QPointF>>());
    // The seed is section-local; a seed picked on the previous plane no longer
    // maps to the vessel here, so reset it to the section center default.
    sectionSeed_ = ContourPoint2D{0.0, 0.0};

    if (alongPathLabel_ != nullptr) {
        const int count = static_cast<int>(sectionPathSamples_.size());
        alongPathLabel_->setText(
            QCoreApplication::translate("XQStageWidgets", "Point %1 / %2 \xC2\xB7 arc %3 mm")
                .arg(index + 1)
                .arg(count)
                .arg(sample.arcLength, 0, 'f', 1));
    }
}

NodeId XQMainWindow::allocateSceneObjectId() const
{
    // One past the largest Scene node or nested contour id (base 2000 when
    // empty), clear of reader/fixture ids and the stage panel's counter.
    NodeId::ValueType maxValue = 1999;
    const XQScene* scene = sceneModel_ != nullptr ? sceneModel_->scene() : nullptr;
    if (scene != nullptr) {
        scene->visit_nodes([&maxValue](const XQDataNode& node) {
            if (node.id().value() > maxValue) {
                maxValue = node.id().value();
            }
            const std::shared_ptr<XQContourGroupPayload> contourPayload =
                std::dynamic_pointer_cast<XQContourGroupPayload>(node.payload());
            if (contourPayload == nullptr) {
                return;
            }
            for (const XQContour& contour : contourPayload->group().contours()) {
                if (contour.contourId.is_valid()
                    && contour.contourId.value() > maxValue) {
                    maxValue = contour.contourId.value();
                }
            }
        });
    }
    if (maxValue == std::numeric_limits<NodeId::ValueType>::max()) {
        return NodeId::invalid();
    }
    return NodeId(maxValue + 1);
}

void XQMainWindow::createContourGroupFromPicker()
{
    XQScene* scene = session_ != nullptr ? session_->scene() : nullptr;
    if (scene == nullptr) {
        return;
    }
    if (contourPathCombo_ == nullptr) {
        return;
    }
    // The picker stores each node's id as item userData (qulonglong); an invalid
    // data means nothing is selected.
    const QVariant pathData = contourPathCombo_->currentData();
    if (!pathData.isValid()) {
        if (statusPositionLabel_ != nullptr) {
            statusPositionLabel_->setText(
                tr("Select a centerline path before creating a contour group."));
        }
        return;
    }
    const NodeId pathId(static_cast<NodeId::ValueType>(pathData.value<qulonglong>()));
    const XQDataNode* pathNode = scene->find(pathId);
    if (pathNode == nullptr || pathNode->domainType() != XQDomainType::Path) {
        if (statusPositionLabel_ != nullptr) {
            statusPositionLabel_->setText(
                tr("Select a centerline path before creating a contour group."));
        }
        return;
    }

    // Build an empty in-memory group bound to the picked path, mounted as a real
    // XQContourGroupPayload node (no .ctgr file). Name after the path node.
    const NodeId groupId = allocateSceneObjectId();
    XQContourGroup group;
    group.setId(groupId);
    group.setSourcePathNode(pathId);

    const std::string name = pathNode->display_name() + " contours";
    XQDataNode node(groupId, XQDomainType::ContourGroup, name,
                    std::make_shared<XQContourGroupPayload>(std::move(group)));
    if (pathNode->hasScaleSlot()) {
        node.setScaleSlot(pathNode->scaleSlot().value());
    }
    if (session_ == nullptr
        || !session_->pushCommand(
            std::make_unique<AddNodeWithSourceRelationCommand>(
                scene, std::move(node), pathId,
                "Add contour group"))) {
        return;
    }

    activeContourGroup_ = groupId;
    refreshSceneTree();
    bindCrossSectionPath();
}

void XQMainWindow::updateContourWorkbenchState()
{
    // Refresh the path picker from the live scene so newly created paths appear.
    if (contourPathCombo_ != nullptr) {
        contourPathCombo_->repopulate();
    }

    const bool hasGroup = activeContourGroup_.is_valid();
    const bool hasPath = !sectionPathSamples_.empty();
    const bool canDraw = hasGroup && hasPath;

    if (contourMethodCircleBtn_ != nullptr) {
        contourMethodCircleBtn_->setEnabled(canDraw);
    }
    if (contourMethodPolygonBtn_ != nullptr) {
        contourMethodPolygonBtn_->setEnabled(canDraw);
    }
    if (contourEditToggle_ != nullptr) {
        contourEditToggle_->setEnabled(canDraw);
        if (!canDraw && contourEditToggle_->isChecked()) {
            const QSignalBlocker blocker(contourEditToggle_);
            contourEditToggle_->setChecked(false);
            if (crossSectionView_ != nullptr) {
                crossSectionView_->setDrawMethod(DrawMethod::None);
            }
        }
    }
}

void XQMainWindow::refreshSectionContourOverlay()
{
    if (crossSectionView_ == nullptr) {
        return;
    }
    QVector<QVector<QPointF>> loops;

    // Only overlay the contour(s) that sit on the CURRENT section plane, i.e.
    // whose pathArcLength matches the slider's current sample position (SV shows
    // just the contour at the current path point; scrubbing to another position
    // switches / clears it). A stale overlay of every contour at once would pile
    // up as more are drawn and never clear when scrubbing away.
    double currentArc = 0.0;
    bool haveArc = false;
    if (alongPathSlider_ != nullptr) {
        const int index = alongPathSlider_->value();
        if (index >= 0 && index < static_cast<int>(sectionPathSamples_.size())) {
            currentArc = sectionPathSamples_[static_cast<std::size_t>(index)].arcLength;
            haveArc = true;
        }
    }

    const XQScene* scene = sceneModel_ != nullptr ? sceneModel_->scene() : nullptr;
    if (haveArc && scene != nullptr && activeContourGroup_.is_valid()) {
        if (const XQDataNode* groupNode = scene->find(activeContourGroup_)) {
            if (std::shared_ptr<XQContourGroupPayload> payload =
                    std::dynamic_pointer_cast<XQContourGroupPayload>(groupNode->payload())) {
                // Match tolerance well under the path sample spacing: contours are
                // stored with the exact arcLength of the sample they were added
                // at (same source value), so this is a near-exact match guarding
                // only float noise.
                const double kArcTol = 1e-3;
                const std::vector<XQContour>& contours = payload->group().contours();
                for (const XQContour& contour : contours) {
                    if (std::abs(contour.pathArcLength - currentArc) > kArcTol) {
                        continue;   // not on the current section plane
                    }
                    QVector<QPointF> loop;
                    loop.reserve(static_cast<int>(contour.points.size()));
                    for (const Point3& p : contour.points) {
                        double u = 0.0;
                        double v = 0.0;
                        XQContourGroup::projectToFrame(contour.frame, p, &u, &v);
                        loop.push_back(QPointF(u, v));
                    }
                    if (!loop.isEmpty()) {
                        loops.push_back(loop);
                    }
                }
            }
        }
    }
    crossSectionView_->setDisplayedContours(loops);
}

void XQMainWindow::addDrawnContour(DrawMethod method, const QVector<QPointF>& points)
{
    XQScene* scene = session_ != nullptr ? session_->scene() : nullptr;
    if (scene == nullptr || crossSectionView_ == nullptr
        || !activeContourGroup_.is_valid()) {
        return;
    }
    XQDataNode* groupNode = scene->find(activeContourGroup_);
    if (groupNode == nullptr) {
        return;
    }
    std::shared_ptr<XQContourGroupPayload> payload =
        std::dynamic_pointer_cast<XQContourGroupPayload>(groupNode->payload());
    if (payload == nullptr) {
        return;
    }

    // Turn the control points into a 2D contour loop via the extraction service.
    std::vector<ContourPoint2D> pts2d;
    if (method == DrawMethod::Circle) {
        if (points.size() < 2) {
            return;
        }
        pts2d = ContourExtractionService::circle(
            ContourPoint2D{points[0].x(), points[0].y()},
            ContourPoint2D{points[1].x(), points[1].y()});
    } else if (method == DrawMethod::Polygon) {
        if (points.size() < 3) {
            return;
        }
        std::vector<ContourPoint2D> vertices;
        vertices.reserve(static_cast<std::size_t>(points.size()));
        for (const QPointF& p : points) {
            vertices.push_back(ContourPoint2D{p.x(), p.y()});
        }
        pts2d = ContourExtractionService::polygon(vertices);
    } else {
        return;
    }

    const ContourType type =
        (method == DrawMethod::Circle) ? ContourType::Circle : ContourType::Manual;
    addContourFromSection2D(pts2d, type);
}

void XQMainWindow::addContourFromSection2D(const std::vector<ContourPoint2D>& pts2d,
                                           ContourType type)
{
    XQScene* scene = session_ != nullptr ? session_->scene() : nullptr;
    if (scene == nullptr || crossSectionView_ == nullptr
        || !activeContourGroup_.is_valid()) {
        return;
    }
    if (pts2d.size() < 3) {
        return;
    }

    // Build the world-space contour frame from the section's last reslice frame.
    const XQCrossSectionFrame sectionFrame = crossSectionView_->lastFrame();
    ContourFrame frame;
    frame.origin = {sectionFrame.origin[0], sectionFrame.origin[1], sectionFrame.origin[2]};
    frame.normal = {sectionFrame.normal[0], sectionFrame.normal[1], sectionFrame.normal[2]};
    frame.xAxis = {sectionFrame.xAxis[0], sectionFrame.xAxis[1], sectionFrame.xAxis[2]};
    frame.yAxis = {sectionFrame.yAxis[0], sectionFrame.yAxis[1], sectionFrame.yAxis[2]};

    // Current arc length from the slider position (0 when no path is bound).
    double arcLength = 0.0;
    if (alongPathSlider_ != nullptr) {
        const int index = alongPathSlider_->value();
        if (index >= 0 && index < static_cast<int>(sectionPathSamples_.size())) {
            arcLength = sectionPathSamples_[static_cast<std::size_t>(index)].arcLength;
        }
    }

    XQContour contour;
    // The shared append tail allocates against both Scene-node and contour ids.
    contour.contourId = ContourId::invalid();
    contour.pathArcLength = arcLength;
    contour.frame = frame;
    contour.type = type;
    contour.closed = true;
    contour.points.reserve(pts2d.size());
    for (const ContourPoint2D& p : pts2d) {
        contour.points.push_back(XQContourGroup::unprojectFromFrame(frame, p.u, p.v));
    }

    if (appendContourToGroup(activeContourGroup_, std::move(contour)).is_valid()) {
        refreshSectionContourOverlay();
    }
}

NodeId XQMainWindow::appendContourToGroup(
    const NodeId& contourGroupId,
    XQContour contour)
{
    XQScene* scene = session_ != nullptr ? session_->scene() : nullptr;
    if (scene == nullptr || session_ == nullptr || !contourGroupId.is_valid()) {
        return NodeId::invalid();
    }
    const XQDataNode* groupNode = scene->find(contourGroupId);
    if (groupNode == nullptr
        || groupNode->domainType() != XQDomainType::ContourGroup) {
        return NodeId::invalid();
    }
    const std::shared_ptr<XQContourGroupPayload> payload =
        std::dynamic_pointer_cast<XQContourGroupPayload>(groupNode->payload());
    if (payload == nullptr) {
        return NodeId::invalid();
    }

    XQContourGroup updatedGroup = payload->group();
    if (!contour.contourId.is_valid()) {
        contour.contourId = allocateSceneObjectId();
    } else {
        bool idInUse = false;
        scene->visit_nodes([&idInUse, &contour](const XQDataNode& node) {
            if (idInUse || node.id() == contour.contourId) {
                idInUse = true;
                return;
            }
            const std::shared_ptr<XQContourGroupPayload> otherPayload =
                std::dynamic_pointer_cast<XQContourGroupPayload>(node.payload());
            if (otherPayload == nullptr) {
                return;
            }
            for (const XQContour& existing : otherPayload->group().contours()) {
                if (existing.contourId == contour.contourId) {
                    idInUse = true;
                    return;
                }
            }
        });
        if (idInUse) {
            return NodeId::invalid();
        }
    }
    if (!contour.contourId.is_valid()) {
        return NodeId::invalid();
    }
    const ContourId appendedId = contour.contourId;
    updatedGroup.addContour(contour);
    const bool pushed = session_->pushCommand(
        std::make_unique<SemanticReplacePayloadCommand>(
            scene,
            contourGroupId,
            XQDomainType::ContourGroup,
            std::make_shared<XQContourGroupPayload>(std::move(updatedGroup)),
            "Add contour"));
    return pushed ? appendedId : NodeId::invalid();
}

bool XQMainWindow::previewThresholdContour(std::vector<ContourPoint2D>& pts2d,
                                           double& usedThreshold)
{
    if (crossSectionView_ == nullptr) {
        return false;
    }

    // Read the current section grayscale (row-major, plus dimensions + mm/px).
    std::vector<double> gray;
    int width = 0;
    int height = 0;
    double pixelSizeMm = 0.0;
    if (!crossSectionView_->sectionPixels(gray, width, height, pixelSizeMm)) {
        return false;   // no resliced section yet
    }

    // Map the slider (0..1000) linearly onto the section's actual [min, max]
    // intensity range, so the threshold tracks real image intensity (SV drives
    // its slider by the image range rather than a fixed percentile). With no
    // slider, fall back to the P90 auto estimate (the high-signal band where a
    // contrast-filled lumen sits).
    double threshold = 0.0;
    if (thresholdSlider_ != nullptr) {
        double lo = 0.0;
        double hi = 0.0;
        bool haveRange = false;
        for (double value : gray) {
            if (!haveRange) {
                lo = value;
                hi = value;
                haveRange = true;
            } else {
                if (value < lo) lo = value;
                if (value > hi) hi = value;
            }
        }
        const double frac =
            static_cast<double>(thresholdSlider_->value()) / 1000.0;
        threshold = lo + frac * (hi - lo);
    } else {
        threshold = ContourExtractionService::estimateThreshold(gray, width, height);
    }
    usedThreshold = threshold;

    // Seed the trace at the current segmentation seed (the section center (0, 0)
    // == pose.origin == the path lumen center by default; a manual seed pick in
    // SeedPick mode overrides it). One seed drives both auto methods.
    pts2d = ContourExtractionService::thresholdContour(gray, width, height,
                                                       pixelSizeMm, threshold,
                                                       sectionSeed_);
    return true;
}

void XQMainWindow::updateThresholdPreview()
{
    // Label always reflects the current slider intensity, even before a section
    // exists, so the user sees what the slider means.
    std::vector<ContourPoint2D> pts2d;
    double threshold = 0.0;
    const bool haveSection = previewThresholdContour(pts2d, threshold);

    if (thresholdLabel_ != nullptr) {
        if (haveSection) {
            thresholdLabel_->setText(
                QCoreApplication::translate("XQStageWidgets", "Threshold: %1")
                    .arg(threshold, 0, 'f', 0));
        } else {
            thresholdLabel_->setText(
                QCoreApplication::translate("XQStageWidgets", "Threshold:"));
        }
    }

    if (crossSectionView_ == nullptr) {
        return;
    }
    // Live preview: draw the candidate loop on the section (over the committed
    // overlay) so scrubbing the slider shows the lumen contour grow/shrink. Not
    // added to the group -- that happens only when the Threshold button is
    // clicked. An empty result clears the preview back to the committed overlay.
    QVector<QVector<QPointF>> preview;
    if (haveSection && pts2d.size() >= 3) {
        QVector<QPointF> loop;
        loop.reserve(static_cast<int>(pts2d.size()));
        for (const ContourPoint2D& p : pts2d) {
            loop.push_back(QPointF(p.u, p.v));
        }
        preview.push_back(loop);
    }
    crossSectionView_->setPreviewContour(preview);
}

void XQMainWindow::runThresholdOnCurrentSection()
{
    if (session_ == nullptr || session_->scene() == nullptr
        || crossSectionView_ == nullptr || !activeContourGroup_.is_valid()) {
        return;
    }

    std::vector<ContourPoint2D> pts2d;
    double threshold = 0.0;
    if (!previewThresholdContour(pts2d, threshold)) {
        return;   // no resliced section yet
    }
    if (pts2d.size() < 3) {
        statusBar()->showMessage(
            QCoreApplication::translate(
                "xq::XQMainWindow",
                "No closed contour was traced at this threshold."),
            4000);
        return;
    }

    addContourFromSection2D(pts2d, ContourType::ThresholdResult);
    // The committed contour now shows through the arc-matched overlay; drop the
    // transient preview so it does not double-draw.
    crossSectionView_->setPreviewContour(QVector<QVector<QPointF>>());
}

bool XQMainWindow::previewRegionGrowContour(std::vector<ContourPoint2D>& pts2d,
                                            double& usedBand)
{
    if (crossSectionView_ == nullptr) {
        return false;
    }

    // Read the current section grayscale (row-major, plus dimensions + mm/px).
    std::vector<double> gray;
    int width = 0;
    int height = 0;
    double pixelSizeMm = 0.0;
    if (!crossSectionView_->sectionPixels(gray, width, height, pixelSizeMm)) {
        return false;   // no resliced section yet
    }

    // Map the band slider (0..1000) to a fraction of the section's actual value
    // span: band = (slider/1000) * (max - min). band = 0 keeps only pixels equal
    // to the seed value; band = full spans the whole range and would flood the
    // section from the seed (the falsifiable failure mode the tests pin down).
    double band = 0.0;
    if (regionGrowBandSlider_ != nullptr && !gray.empty()) {
        double lo = gray[0];
        double hi = gray[0];
        for (double value : gray) {
            if (value < lo) lo = value;
            if (value > hi) hi = value;
        }
        const double frac =
            static_cast<double>(regionGrowBandSlider_->value()) / 1000.0;
        band = frac * (hi - lo);
    }
    usedBand = band;

    // Region-grow from the current seed and trace the grown region's boundary.
    pts2d = ContourExtractionService::regionGrowContour(gray, width, height,
                                                        pixelSizeMm, sectionSeed_,
                                                        band);
    return true;
}

void XQMainWindow::updateRegionGrowPreview()
{
    // Label always reflects the current band, even before a section exists.
    std::vector<ContourPoint2D> pts2d;
    double band = 0.0;
    const bool haveSection = previewRegionGrowContour(pts2d, band);

    if (regionGrowBandLabel_ != nullptr) {
        if (haveSection) {
            regionGrowBandLabel_->setText(
                QCoreApplication::translate("XQStageWidgets", "Band: %1")
                    .arg(band, 0, 'f', 0));
        } else {
            regionGrowBandLabel_->setText(
                QCoreApplication::translate("XQStageWidgets", "Band:"));
        }
    }

    if (crossSectionView_ == nullptr) {
        return;
    }
    // Live preview on the same orange overlay the threshold preview uses -- the
    // two auto methods never preview at once (the later drag overwrites the
    // earlier), which matches the single-active-preview affordance.
    QVector<QVector<QPointF>> preview;
    if (haveSection && pts2d.size() >= 3) {
        QVector<QPointF> loop;
        loop.reserve(static_cast<int>(pts2d.size()));
        for (const ContourPoint2D& p : pts2d) {
            loop.push_back(QPointF(p.u, p.v));
        }
        preview.push_back(loop);
    }
    crossSectionView_->setPreviewContour(preview);
}

void XQMainWindow::runRegionGrowOnCurrentSection()
{
    if (session_ == nullptr || session_->scene() == nullptr
        || crossSectionView_ == nullptr || !activeContourGroup_.is_valid()) {
        return;
    }

    std::vector<ContourPoint2D> pts2d;
    double band = 0.0;
    if (!previewRegionGrowContour(pts2d, band)) {
        return;   // no resliced section yet
    }
    if (pts2d.size() < 3) {
        statusBar()->showMessage(
            QCoreApplication::translate(
                "xq::XQMainWindow",
                "No connected region was grown from this seed."),
            4000);
        return;
    }

    addContourFromSection2D(pts2d, ContourType::LevelSetResult);
    // The committed contour now shows through the arc-matched overlay; drop the
    // transient preview so it does not double-draw.
    crossSectionView_->setPreviewContour(QVector<QVector<QPointF>>());
}

bool XQMainWindow::previewLevelSetContour(std::vector<ContourPoint2D>& pts2d,
                                          int& usedIterations)
{
    if (crossSectionView_ == nullptr) {
        return false;
    }

    // Read the current section grayscale (row-major, plus dimensions + mm/px).
    std::vector<double> gray;
    int width = 0;
    int height = 0;
    double pixelSizeMm = 0.0;
    if (!crossSectionView_->sectionPixels(gray, width, height, pixelSizeMm)) {
        return false;   // no resliced section yet
    }

    // Map the iteration slider (0..1000) linearly to [200, 1200] phase-one
    // evolution steps: even the low end lets the seed circle inflate to a small
    // lumen, the high end lets the front travel across a larger vessel before the
    // gradient-stop criterion holds. The ITK two-phase level set is heavier than
    // the old Chan-Vese, so the preview is release-driven (see the sliderReleased
    // connect in buildCrossSectionWorkbench).
    int iterations = 600;
    if (levelSetIterSlider_ != nullptr) {
        const double frac =
            static_cast<double>(levelSetIterSlider_->value()) / 1000.0;
        iterations = 200 + static_cast<int>(std::lround(frac * 1000.0));
    }
    usedIterations = iterations;

    // Run the SimVascular ITK vascular two-phase level set from the current seed
    // and trace the zero level set. Robust where region-grow / a global threshold
    // leak, AND where the old self-written Chan-Vese collapsed to a smooth circle
    // on low-contrast MR (region means c1 ~= c2). ITK types live in the adapter;
    // here we do a trivial ContourPoint2D <-> SectionPoint2D copy at the boundary.
    LevelSetParams params;
    params.maxIterations1 = iterations;
    const SectionPoint2D seed{sectionSeed_.u, sectionSeed_.v};
    const std::vector<SectionPoint2D> loop =
        levelSetSegmenter_->segment(gray, width, height, pixelSizeMm, seed, params);
    pts2d.clear();
    pts2d.reserve(loop.size());
    for (const SectionPoint2D& p : loop) {
        pts2d.push_back(ContourPoint2D{p.u, p.v});
    }
    return true;
}

void XQMainWindow::updateLevelSetPreview()
{
    // Label always reflects the current iteration count, even before a section
    // exists. Only a slider RELEASE reaches here (the evolution is expensive), so
    // dragging never re-evolves; see the connect in buildCrossSectionWorkbench.
    std::vector<ContourPoint2D> pts2d;
    int iterations = 0;
    const bool haveSection = previewLevelSetContour(pts2d, iterations);

    if (levelSetIterLabel_ != nullptr) {
        if (haveSection) {
            levelSetIterLabel_->setText(
                QCoreApplication::translate("XQStageWidgets", "Iterations: %1")
                    .arg(iterations));
        } else {
            levelSetIterLabel_->setText(
                QCoreApplication::translate("XQStageWidgets", "Iterations:"));
        }
    }

    if (crossSectionView_ == nullptr) {
        return;
    }
    // Live preview on the same orange overlay the other auto methods use -- the
    // later action overwrites the earlier, matching the single-active-preview
    // affordance.
    QVector<QVector<QPointF>> preview;
    if (haveSection && pts2d.size() >= 3) {
        QVector<QPointF> loop;
        loop.reserve(static_cast<int>(pts2d.size()));
        for (const ContourPoint2D& p : pts2d) {
            loop.push_back(QPointF(p.u, p.v));
        }
        preview.push_back(loop);
    }
    crossSectionView_->setPreviewContour(preview);
}

void XQMainWindow::runLevelSetOnCurrentSection()
{
    if (session_ == nullptr || session_->scene() == nullptr
        || crossSectionView_ == nullptr || !activeContourGroup_.is_valid()) {
        return;
    }

    std::vector<ContourPoint2D> pts2d;
    int iterations = 0;
    if (!previewLevelSetContour(pts2d, iterations)) {
        return;   // no resliced section yet
    }
    if (pts2d.size() < 3) {
        statusBar()->showMessage(
            QCoreApplication::translate(
                "xq::XQMainWindow",
                "No closed contour was found by the level set."),
            4000);
        return;
    }

    addContourFromSection2D(pts2d, ContourType::LevelSetResult);
    // The committed contour now shows through the arc-matched overlay; drop the
    // transient preview so it does not double-draw.
    crossSectionView_->setPreviewContour(QVector<QVector<QPointF>>());
}

void XQMainWindow::onSeedPicked(double u, double v)
{
    sectionSeed_ = ContourPoint2D{u, v};
    // Leave seed-pick mode: unchecking the toggle drives its toggled slot, which
    // switches the section view back to view-only (DrawMethod::None).
    if (contourSeedPickBtn_ != nullptr) {
        contourSeedPickBtn_->setChecked(false);
    }
    // Re-trace both auto previews from the new seed so the user sees the effect
    // immediately (whichever method they last scrubbed is what shows).
    updateThresholdPreview();
    updateRegionGrowPreview();
    statusBar()->showMessage(
        QCoreApplication::translate("xq::XQMainWindow", "Seed set at (%1, %2) mm.")
            .arg(u, 0, 'f', 1)
            .arg(v, 0, 'f', 1),
        4000);
}

void XQMainWindow::onSceneContextMenu(const QPoint& pos)
{
    // Right-click a scene node to delete it through the command stack (undoable).
    // Only available once a workflow (mutable scene + stack) is attached.
    const QModelIndex viewIndex = sceneTreeView_->indexAt(pos);
    if (workflowBusy_ || !viewIndex.isValid() || session_->scene() == nullptr
        || session_->commandStack() == nullptr) {
        return;
    }
    const QModelIndex sourceIndex =
        (sceneFilter_ != nullptr) ? sceneFilter_->mapToSource(viewIndex) : viewIndex;
    const XQDataNode* node = sceneModel_->nodeForIndex(sourceIndex);
    if (node == nullptr) {
        return;
    }

    const NodeId id = node->id();
    const QString name = QString::fromStdString(node->display_name());
    const XQDomainType domain = node->domainType();

    QMenu menu(this);
    // "Operate from this node" shortcuts jump straight to the consuming stage page
    // and preselect this node, so the user never types an internal NodeId. Placed
    // above Delete. A contour group feeds Modeling; a surface model feeds Meshing.
    QAction* modelFromAction = nullptr;
    QAction* meshFromAction = nullptr;
    if (domain == XQDomainType::ContourGroup) {
        modelFromAction = menu.addAction(tr("Model from \"%1\"").arg(name));
    } else if (domain == XQDomainType::SurfaceModel) {
        meshFromAction = menu.addAction(tr("Mesh from \"%1\"").arg(name));
    }
    if (modelFromAction != nullptr || meshFromAction != nullptr) {
        menu.addSeparator();
    }
    QAction* deleteAction =
        menu.addAction(tr("Delete \"%1\"").arg(name));
    const QAction* chosen = menu.exec(sceneTreeView_->viewport()->mapToGlobal(pos));
    if (chosen == nullptr) {
        return;
    }
    if (chosen == modelFromAction) {
        activateStageFromNode(2, id); // Modeling
    } else if (chosen == meshFromAction) {
        activateStageFromNode(3, id); // Meshing
    } else if (chosen == deleteAction) {
        // Command gateway: the session callback refreshes the tree + re-syncs the
        // render scene on success (the unified render-sync point).
        session_->pushCommand(
            std::make_unique<RemoveNodeCommand>(session_->scene(), id));
    }
}

void XQMainWindow::refreshSceneTree()
{
    sceneModel_->refresh();
    // The tree is now two-level (type groups -> nodes); expand it so every node is
    // visible without a manual disclosure click.
    if (sceneTreeView_ != nullptr) {
        sceneTreeView_->expandAll();
    }
    // Keep the status-bar node count in step with every scene change (load /
    // command push / undo / redo). Without this the right-hand "Nodes: N" cell
    // stayed at its attach-time value (0 for a workspace attached empty, then
    // loaded), contradicting the left-hand "Loaded project: N node(s)" readout.
    // Both now report the same scene node count. Root rowCount is the group count
    // after the tree change, so count the scene's nodes directly.
    if (statusNodeCountLabel_ != nullptr) {
        int nodeCount = 0;
        if (const XQScene* scene = sceneModel_->scene()) {
            scene->visit_nodes([&nodeCount](const XQDataNode&) { ++nodeCount; });
        }
        statusNodeCountLabel_->setText(tr("Nodes: %1").arg(nodeCount));
    }
    syncRenderScene();
}

void XQMainWindow::syncRenderScene()
{
    if (renderScene_ == nullptr) {
        return;
    }
    const XQScene* scene = sceneModel_ != nullptr ? sceneModel_->scene() : nullptr;
    if (scene == nullptr) {
        renderScene_->clearNodes();
        syncedPayloads_.clear();
        if (mprWidget_ != nullptr) {
            mprWidget_->renderAll();
        }
        return;
    }

    // Default LOD for surface/mesh rendering (R4): interactive vtkLODActor levels
    // with a 2M-triangle budget. Same constant the old selection path used.
    ChunkUploadSpec lazySpec;
    lazySpec.lod = kDefaultLodOptions;

    // Incremental sync (B4b): the old contract cleared every actor and re-upserted
    // every renderable node on each scene change, so a large surface re-ran its
    // decimation build even when its payload had not changed (a big real-file
    // stall on undo/redo/parse-landing). Now each node's live payload pointer is
    // compared against syncedPayloads_ (the identity already assembled on the
    // render side); a match + a resident actor -> skip the detach/rebuild. Nodes
    // whose payload changed (a payload change always swaps the shared_ptr:
    // setPayload / clone) or that are seen for the first time are (re)upserted;
    // nodes that left the scene are removed after the walk. This is safe because
    // presentation attributes (visible / opacity / color) live on the render side
    // and survive the skip, and the visibility reconcile below still runs for
    // every node. `visited` tracks the ids that should remain resident this pass.
    std::unordered_set<NodeId> visited;

    scene->visit_nodes([&](const XQDataNode& node) {
        const NodeId id = node.id();
        if (!node.payload()) {
            return;
        }
        // Payload identity fingerprint: a match against the last synced pointer +
        // an existing resident actor means nothing to rebuild for this node.
        const XQPayload* payloadPtr = node.payload().get();
        const auto syncedIt = syncedPayloads_.find(id);
        const bool skipUpsert = (syncedIt != syncedPayloads_.end()
                                 && syncedIt->second == payloadPtr
                                 && renderScene_->hasNode(id));
        if (!skipUpsert) {
        switch (node.domainType()) {
        case XQDomainType::SurfaceModel: {
            // A SurfaceModel node's payload is not always an XQSurfaceModelPayload:
            // the SV project reader leaves .mdl model nodes with an unresolved
            // XQSourcePayload (same "sitting in the list" state as contours). A
            // static_cast to XQSurfaceModelPayload there reads garbage geometry and
            // crashes -- so resolve the concrete type with dynamic_pointer_cast and
            // skip a node that carries no real surface payload.
            const auto payload =
                std::dynamic_pointer_cast<XQSurfaceModelPayload>(node.payload());
            if (payload == nullptr) {
                break;
            }
            if (payload->model().hasTriangleGeometry()) {
                renderScene_->upsertNode(id, node);
            } else if (geometryResources_ != nullptr && geometryRegistry_ != nullptr) {
                GeometryResourceManager::GeometrySourceHandle lazy =
                    resolveLazyGeometrySource(*payload, *geometryResources_,
                                              *geometryRegistry_,
                                              LazyGeometrySourceMode::SurfaceOnly);
                if (lazy.valid() && lazy->meta().triangleCount > 0) {
                    renderScene_->upsertNodeProgressive(id, lazy.source(),
                                                        GeoKind::Surface, lazySpec);
                }
            }
            break;
        }
        case XQDomainType::Mesh: {
            // Same guard as SurfaceModel: a Mesh node may carry an unresolved
            // XQSourcePayload rather than an XQMeshPayload.
            const auto payload =
                std::dynamic_pointer_cast<XQMeshPayload>(node.payload());
            if (payload == nullptr) {
                break;
            }
            const XQMesh& mesh = payload->mesh();
            if (mesh.hasVolumeTets() || mesh.hasSurfaceTriangles()) {
                renderScene_->upsertNode(id, node);
            } else if (geometryResources_ != nullptr && geometryRegistry_ != nullptr) {
                GeometryResourceManager::GeometrySourceHandle lazy =
                    resolveLazyGeometrySource(*payload, *geometryResources_,
                                              *geometryRegistry_,
                                              LazyGeometrySourceMode::TetOnly);
                bool ok = false;
                if (lazy.valid() && lazy->meta().tetCount > 0) {
                    ok = renderScene_
                             ->upsertNodeProgressive(id, lazy.source(), GeoKind::TetMesh,
                                                     lazySpec)
                             .ok;
                }
                if (!ok) {
                    lazy = resolveLazyGeometrySource(*payload, *geometryResources_,
                                                     *geometryRegistry_,
                                                     LazyGeometrySourceMode::SurfaceOnly);
                    if (lazy.valid() && lazy->meta().triangleCount > 0) {
                        renderScene_->upsertNodeProgressive(id, lazy.source(),
                                                            GeoKind::Surface, lazySpec);
                    }
                }
            }
            break;
        }
        case XQDomainType::Path:
            // Confirm the concrete payload before upsert: the render kernel
            // static_casts by domain, so a mislabelled / unresolved payload would
            // crash there. dynamic_cast makes the sync path robust to it.
            if (std::dynamic_pointer_cast<XQPathPayload>(node.payload()) != nullptr) {
                renderScene_->upsertNode(id, node);
            }
            break;
        case XQDomainType::SegmentationMask:
            if (std::dynamic_pointer_cast<XQSegmentationMaskPayload>(node.payload())
                != nullptr) {
                renderScene_->upsertNode(id, node);
            }
            break;
        case XQDomainType::FlowResult:
            if (std::dynamic_pointer_cast<XQFlowResultPayload>(node.payload())
                != nullptr) {
                renderScene_->upsertNode(id, node);
            }
            break;
        case XQDomainType::ContourGroup:
            // A ContourGroup node is unresolved (XQSourcePayload) until the
            // background parse swaps in an XQContourGroupPayload; only the resolved
            // form has contours to composite. dynamic_cast keeps the sync robust to
            // the unresolved state (a static_cast there would read garbage).
            if (std::dynamic_pointer_cast<XQContourGroupPayload>(node.payload())
                != nullptr) {
                renderScene_->upsertNode(id, node);
            }
            break;
        default:
            // Image / SimulationCase / AiAnalysis / Unknown: no 3D geometry to
            // composite.
            break;
        }
        } // if (!skipUpsert)

        // Record the fingerprint + mark this node as still resident this pass. A
        // node that assembled an actor (freshly upserted, or skipped because it was
        // already current) is stamped with its live payload pointer; a node whose
        // upsert failed (lazy resolve miss / non-renderable payload) leaves no
        // actor, so it is neither stamped nor kept -- the leave-scene sweep below
        // then drops any stale fingerprint for it.
        if (renderScene_->hasNode(id)) {
            syncedPayloads_[id] = payloadPtr;
            visited.insert(id);
        }

        // Reconcile the tree checkbox state with the render scene for any node that
        // now has assembled actors. This runs for every node on every sync
        // (whether or not its geometry was rebuilt), so it must not let the sync
        // silently reset a user's choice:
        //   - known UI state  -> the checkbox is authoritative: push it to render.
        //   - first sight     -> the render-side default is authoritative (a mesh
        //                        mounts hidden); mirror it back into the tree
        //                        silently so the checkbox matches without emitting.
        if (renderScene_->hasNode(id) && sceneModel_ != nullptr) {
            if (sceneModel_->hasVisibilityState(id)) {
                renderScene_->setNodeVisible(id, sceneModel_->nodeVisible(id));
            } else {
                sceneModel_->setNodeVisibleSilently(id,
                                                    renderScene_->nodeVisible(id));
            }
        }
    });

    // Leave-scene sweep: any node the render side still holds but the walk did not
    // visit (deleted node, undone AddNode, or a payload that regressed to a
    // non-renderable state) is removed here -- this replaces the old blanket
    // clearNodes() as the "no lingering actor" guarantee.
    if (!syncedPayloads_.empty()) {
        std::vector<NodeId> departed;
        departed.reserve(syncedPayloads_.size());
        for (const auto& entry : syncedPayloads_) {
            if (visited.find(entry.first) == visited.end()) {
                departed.push_back(entry.first);
            }
        }
        for (const NodeId& id : departed) {
            renderScene_->removeNode(id);
            syncedPayloads_.erase(id);
        }
    }

    if (mprWidget_ != nullptr) {
        mprWidget_->renderAll();
    }

    // Release the memory-mapped geometry blocks the lazy upserts above pinned:
    // the render scene copies geometry into its own VTK arrays (copy-on-upload),
    // so once every source lease has gone out of scope the manager's cached mmap
    // of the .xqproj asset blobs is dead weight -- and, worse, keeps the project
    // files open, so the OS refuses to delete/move them. All leases were local to
    // this function and are already destroyed, so a budget-0 eviction pass frees
    // every now-unpinned block; the budget is then restored.
    if (geometryResources_ != nullptr) {
        const std::size_t budget = geometryResources_->budgetBytes();
        geometryResources_->setBudgetBytes(0);
        geometryResources_->evictToBudget();
        geometryResources_->setBudgetBytes(budget);
    }
}

void XQMainWindow::setWorkflowBusy(bool busy, const QString& label)
{
    workflowBusy_ = busy;
    if (stagePanel_ != nullptr) {
        stagePanel_->setEnabled(!busy);
    }
    QAction* const actions[] = {undoAction_, redoAction_, tbUndoAction_, tbRedoAction_,
                                openAction_, saveAction_, tbOpenAction_, tbSaveAction_,
                                openImageAction_, openDicomAction_,
                                openSvProjectAction_};
    for (QAction* action : actions) {
        if (action != nullptr) {
            action->setEnabled(!busy);
        }
    }
    if (busy) {
        if (statusPositionLabel_ != nullptr) {
            statusShowingIdle_ = false;
            statusPositionLabel_->setText(tr("Running: %1...").arg(label));
        }
        QApplication::setOverrideCursor(Qt::BusyCursor);
    } else {
        if (statusPositionLabel_ != nullptr) {
            statusShowingIdle_ = true;
            statusPositionLabel_->setText(tr("Ready"));
        }
        QApplication::restoreOverrideCursor();
    }
}

void XQMainWindow::updatePresentationControls(const XQDataNode* node)
{
    // A node is adjustable only once the render scene has assembled actors for it
    // (hasNode). That covers surface / mesh / path / mask / flow / contour nodes
    // and excludes image / unresolved-source nodes -- exactly the ones with no
    // opacity/colour to tweak.
    const bool adjustable = node != nullptr && renderScene_ != nullptr
                            && renderScene_->hasNode(node->id());
    if (!adjustable) {
        presentationNodeId_ = NodeId::invalid();
        if (opacitySlider_ != nullptr) {
            opacitySlider_->setEnabled(false);
        }
        if (colorButton_ != nullptr) {
            colorButton_->setEnabled(false);
            colorButton_->setStyleSheet(QString());
        }
        return;
    }

    presentationNodeId_ = node->id();
    // Back-fill the colour swatch from the node's current colour; opacity has no
    // render-scene getter (it defaults to fully opaque on upsert), so the slider
    // resets to 100 % without pushing a value back (blocked).
    if (opacitySlider_ != nullptr) {
        const QSignalBlocker blocker(opacitySlider_);
        opacitySlider_->setEnabled(true);
        opacitySlider_->setValue(100);
    }
    if (colorButton_ != nullptr) {
        colorButton_->setEnabled(true);
        double rgb[3] = {1.0, 1.0, 1.0};
        if (renderScene_->nodeColor(presentationNodeId_, rgb)) {
            const QColor c = QColor::fromRgbF(rgb[0], rgb[1], rgb[2]);
            colorButton_->setStyleSheet(
                QStringLiteral("background-color: %1").arg(c.name()));
        } else {
            colorButton_->setStyleSheet(QString());
        }
    }
}

const XQDataNode* XQMainWindow::selectedSceneNode() const
{
    if (sceneTreeView_ == nullptr || sceneModel_ == nullptr) {
        return nullptr;
    }
    const QModelIndex current = sceneTreeView_->currentIndex();
    if (!current.isValid()) {
        return nullptr;
    }
    const QModelIndex sourceIndex =
        (sceneFilter_ != nullptr) ? sceneFilter_->mapToSource(current) : current;
    return sceneModel_->nodeForIndex(sourceIndex);
}

const XQDataNode* XQMainWindow::nodeForRequestedOrSelected(const NodeId& requested) const
{
    if (requested.is_valid() && session_->scene() != nullptr) {
        if (const XQDataNode* node = session_->scene()->find(requested)) {
            return node;
        }
    }
    return selectedSceneNode();
}

QString XQMainWindow::resolveSourcePath(const std::string& sourcePath) const
{
    const QString path = QString::fromStdString(sourcePath);
    const QFileInfo info(path);
    if (info.isAbsolute() || activeProjectDir_.isEmpty()) {
        return info.filePath();
    }
    return QDir(activeProjectDir_).filePath(path);
}

void XQMainWindow::undo()
{
    // The session gateway fires the scene-changed callback (refreshSceneTree) on
    // a successful undo, so there is no explicit refresh here.
    session_->undo();
}

void XQMainWindow::redo()
{
    session_->redo();
}

PathController* XQMainWindow::pathController() const
{
    return session_->pathController();
}

XQTaskRunner& XQMainWindow::taskRunner()
{
    return session_->taskRunner();
}

SegmentationController* XQMainWindow::segmentationController() const
{
    return session_->segmentationController();
}

ModelingController* XQMainWindow::modelingController() const
{
    return session_->modelingController();
}

MeshingController* XQMainWindow::meshingController() const
{
    return session_->meshingController();
}

VesselProfileController* XQMainWindow::vesselProfileController() const
{
    return session_->vesselProfileController();
}

PathModuleController* XQMainWindow::pathModuleController() const
{
    return session_->pathModuleController();
}

CenterlineBController* XQMainWindow::centerlineBController() const
{
    return session_->centerlineBController();
}

FlowController* XQMainWindow::flowController() const
{
    return session_->flowController();
}

FlowSmokeController* XQMainWindow::flowSmokeController() const
{
    return session_->flowSmokeController();
}

bool XQMainWindow::hasFlowCapability() const
{
    return session_->hasFlowCapability();
}

AiController* XQMainWindow::aiController() const
{
    return session_->aiController();
}

void XQMainWindow::queuePersistedDicomImage(const XQDataNode& node)
{
    const auto payload = std::dynamic_pointer_cast<XQImageVolumePayload>(
        node.payload());
    XQProject* project = activeProject();
    if (payload == nullptr || project == nullptr || !node.hasAssetId()
        || project->state() != XQProject::LifecycleState::Open) {
        return;
    }
    const AssetRecord* asset = project->assetRegistry().find(node.assetId());
    if (asset == nullptr || asset->kind != AssetKind::Image
        || asset->category != AssetCategory::ExternalSource || !asset->hasDicom) {
        return;
    }

    if (geometryResources_ == nullptr
        || !geometryResources_->usesAssetRegistry(&project->assetRegistry())) {
        geometryRegistry_ = &project->assetRegistry();
        geometryResources_ = std::make_unique<GeometryResourceManager>(
            geometryRegistry_, assetRootDir_);
        geometryResources_->setBudgetBytes(
            static_cast<std::size_t>(
                XQPreferencesDialog::geometryBudgetMiB()) << 20);
    }

    const NodeId nodeId = node.id();
    const AssetId assetId = node.assetId();
    const ContentRevision revision = node.contentRevision();
    const std::shared_ptr<XQPayload> payloadIdentity = node.payload();
    XQProject* const capturedProject = project;
    const std::uint64_t capturedEpoch = project->lifecycleEpoch();
    GeometryResourceManager* const manager = geometryResources_.get();
    const std::string projectFilePath = workspacePath_.toStdString();
    const XQImageVolume image = payload->volume();

    struct ResolveOutcome {
        ImageResourceResolveResult resolved;
        std::shared_ptr<XQDemoVolume> displayVolume;
    };
    const bool queued = session_->taskRunner().run(
        tr("Loading DICOM image..."),
        [capturedProject, manager, assetId, image,
         projectFilePath]() -> std::shared_ptr<void> {
            auto outcome = std::make_shared<ResolveOutcome>();
            GdcmItkDicomSeriesReader reader;
            outcome->resolved = ImageResourceResolver::acquire(
                assetId, image, projectFilePath, *manager,
                capturedProject->assetRegistry(), reader);
            if (outcome->resolved.ok()) {
                outcome->displayVolume = materializeDemoVolume(
                    image, outcome->resolved.source.source());
            }
            return outcome;
        },
        [this, capturedProject, capturedEpoch, nodeId, assetId, revision,
         payloadIdentity](std::shared_ptr<void> raw) {
            const auto outcome = std::static_pointer_cast<ResolveOutcome>(raw);
            if (!outcome->resolved.ok() || outcome->displayVolume == nullptr) {
                if (statusPositionLabel_ != nullptr) {
                    statusShowingIdle_ = false;
                    statusPositionLabel_->setText(
                        tr("DICOM source could not be restored: %1")
                            .arg(QString::fromLatin1(dicomSeriesStatusToken(
                                outcome->resolved.status))));
                }
                return;
            }
            XQProject* project = activeProject();
            const XQDataNode* live = project != nullptr
                ? project->scene().find(nodeId)
                : nullptr;
            if (project != capturedProject || project == nullptr
                || project->lifecycleEpoch() != capturedEpoch
                || live == nullptr || live->contentRevision() != revision
                || live->payload() != payloadIdentity || !live->hasAssetId()
                || live->assetId() != assetId) {
                return;
            }
            activeImage_ = std::make_unique<XQDemoVolume>(
                std::move(*outcome->displayVolume));
            activeImageNodeId_ = nodeId;
            useVolume(activeImage_->image, activeImage_->buffer.get());
        });
    if (!queued && statusPositionLabel_ != nullptr) {
        statusShowingIdle_ = false;
        statusPositionLabel_->setText(
            tr("Another task is still running."));
    }
}

void XQMainWindow::onSceneSelectionChanged(const QModelIndex& current,
                                           const QModelIndex& previous)
{
    Q_UNUSED(previous);

    // The tree view is driven through the filter proxy, so map proxy -> source
    // before resolving the node behind the index.
    const QModelIndex sourceIndex =
        (sceneFilter_ != nullptr) ? sceneFilter_->mapToSource(current) : current;

    const XQDataNode* node = sceneModel_->nodeForIndex(sourceIndex);
    if (node == nullptr) {
        updatePresentationControls(nullptr);
        return;
    }

    // Enable/back-fill the opacity/colour controls for the selected node (a no-op
    // for image / unresolved nodes, which have no assembled actors).
    updatePresentationControls(node);

    if (node->domainType() == XQDomainType::Image) {
        // Fast path: the volume is already decoded in memory -> just re-point the
        // MPR (no I/O, stays synchronous).
        if (activeImage_ && node->id() == activeImageNodeId_) {
            useVolume(activeImage_->image, activeImage_->buffer.get());
            return;
        }
        if (std::dynamic_pointer_cast<XQImageVolumePayload>(node->payload())) {
            queuePersistedDicomImage(*node);
            return;
        }
        // Otherwise queue a background decode (same serial queue as project loads)
        // so selecting a different image never freezes the GUI thread. A failed
        // read is reported from the commit (status bar).
        if (std::shared_ptr<XQSourcePayload> source =
                std::dynamic_pointer_cast<XQSourcePayload>(node->payload())) {
            PendingParse job;
            job.nodeId = node->id();
            job.path = resolveSourcePath(source->sourcePath());
            job.kind = ParseKind::Image;
            job.seedImageNode = false;
            job.displayName = QString::fromStdString(node->display_name());
            pendingParses_.push_back(std::move(job));
            startNextParse();
        }
        return;
    }

    // Visibility-driven composition (design §2/§3.4): selecting a node no longer
    // rebuilds the renderer. Every renderable node is already a resident actor in
    // the render scene (built by syncRenderScene on scene changes); selection is
    // purely for the property panel / highlight (M3). Geometry rendering is not
    // driven from here anymore.

    // Convenience: pre-fill the consuming stage page's source picker with the
    // selected node so the user can just open that page and run. Selecting is not
    // a request to model/mesh, so this never raises or switches the stage page.
    if (node->domainType() == XQDomainType::ContourGroup) {
        preselectStageSource(2, node->id()); // Modeling
    } else if (node->domainType() == XQDomainType::SurfaceModel) {
        preselectStageSource(3, node->id()); // Meshing
    }
}

} // namespace xq
