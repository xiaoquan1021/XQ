#include "ui/panels/XQStageWidgets.h"

#include "ui/controllers/AiController.h"
#include "ui/controllers/CenterlineBController.h"
#include "ui/controllers/MeshingController.h"
#include "ui/controllers/ModelingController.h"
#include "ui/controllers/PathController.h"
#include "ui/controllers/PathModuleController.h"
#include "ui/controllers/SegmentationController.h"
#include "ui/controllers/VesselProfileController.h"

#include "core/NodeId.h"
#include "core/XQImageVolume.h"
#include "core/XQMemoryImageBufferHandle.h"
#include "core/source/ResidentVoxelSource.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFormLayout>
#include <QFileDialog>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QAbstractItemView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QString>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace xq {

// --- NodeComboBox: scene-node picker (replaces the raw NodeId spin boxes) ----

NodeComboBox::NodeComboBox(SceneNodeLister lister, XQDomainType domain, QWidget* parent)
    : QComboBox(parent)
    , lister_(std::move(lister))
    , domain_(domain)
{
    // Fill once at construction: the scene may already carry matching nodes when
    // the panel is (re)built after a project load.
    repopulate();
}

void NodeComboBox::repopulate()
{
    // Preserve the current selection across the rebuild: remember the selected
    // NodeId value and restore it if the node is still present.
    bool hadSelection = false;
    NodeId::ValueType selectedValue = 0;
    if (currentIndex() >= 0) {
        const QVariant data = currentData();
        if (data.isValid()) {
            hadSelection = true;
            selectedValue = data.value<qulonglong>();
        }
    }

    clear();
    if (lister_) {
        const std::vector<SceneNodeOption> options = lister_(domain_);
        for (const SceneNodeOption& option : options) {
            const QString text = option.name
                + QStringLiteral(" (#") + QString::number(option.id.value())
                + QStringLiteral(")");
            addItem(text, QVariant(static_cast<qulonglong>(option.id.value())));
        }
    }

    if (hadSelection) {
        const int index = findData(QVariant(static_cast<qulonglong>(selectedValue)));
        if (index >= 0) {
            setCurrentIndex(index);
        }
    }
}

void NodeComboBox::showPopup()
{
    // Refresh right before the list drops down so nodes created since the last
    // open (or since the panel was built) are always offered.
    repopulate();
    QComboBox::showPopup();
}

namespace {

// These stage pages are built from free functions (no QObject `this`), so user-
// facing strings are translated through QCoreApplication::translate under a
// shared context rather than tr(). The panel is built once after the translator
// is installed; it does not retranslate on a live language switch, which is
// acceptable here. Keep the TS entries under the literal "XQStageWidgets"
// context: treating xqTr as an lupdate tr() alias incorrectly classifies these
// free-function calls under the enclosing "xq" namespace.
QString xqTr(const char* source)
{
    return QCoreApplication::translate("XQStageWidgets", source);
}

// Allocates monotonically increasing node ids for newly created nodes so each
// "execute" click targets a fresh id. Starts at 1000 to stay clear of ids the
// reader / fixtures hand out. Shared across all stage pages via a shared_ptr so
// every page draws from the same sequence.
using IdCounter = std::shared_ptr<NodeId::ValueType>;

NodeId nextId(const IdCounter& counter)
{
    return NodeId((*counter)++);
}

// Builds a scene-node picker bound to `domain`, tagged with `objectName` so the
// window can find it (right-click preselect) and tests can drive it.
NodeComboBox* makeNodeCombo(QWidget* parent, const char* objectName,
                            SceneNodeLister lister, XQDomainType domain)
{
    NodeComboBox* combo = new NodeComboBox(std::move(lister), domain, parent);
    combo->setObjectName(QString::fromLatin1(objectName));
    return combo;
}

// The selected node's id, or NodeId(0) when nothing is selected (empty list or
// no pick). NodeId(0) preserves the previous "spin defaults to 0" invalid-source
// semantics, so each page's existing rejection path is unchanged.
NodeId idFromCombo(const QComboBox* combo)
{
    const QVariant data = combo->currentData();
    if (!data.isValid()) {
        return NodeId(0);
    }
    return NodeId(static_cast<NodeId::ValueType>(data.value<qulonglong>()));
}

// --- Page skeleton -------------------------------------------------------

// A stage page: title + a small grey wrapping subtitle at the top, a body
// layout (returned via `bodyOut`) the caller fills, plus a trailing execute
// button / status label / stretch the caller adds. Returns the page.
// `name` is the internal English identifier feeding the objectName suffixes;
// `titleText` is the (already translated) heading shown to the user;
// `subtitle` is a one-line, translated description of what the stage does.
QWidget* makeStagePage(QStackedWidget* panel,
                       const char* name,
                       const QString& titleText,
                       const QString& subtitle,
                       QVBoxLayout** bodyOut)
{
    QWidget* page = new QWidget(panel);
    page->setObjectName(QString::fromLatin1("xqStagePage_") + QString::fromLatin1(name));

    QVBoxLayout* layout = new QVBoxLayout(page);

    QLabel* title = new QLabel(titleText, page);
    title->setObjectName(QString::fromLatin1("xqStageTitle_") + QString::fromLatin1(name));
    QFont titleFont = title->font();
    titleFont.setPointSizeF(10.0);
    titleFont.setWeight(QFont::DemiBold);
    title->setFont(titleFont);
    layout->addWidget(title);

    // Stage description: small, muted, wrapping. Purely informational. A stable
    // neutral grey that reads well on the light theme.
    QLabel* desc = new QLabel(subtitle, page);
    desc->setObjectName(QString::fromLatin1("xqStageSubtitle_") + QString::fromLatin1(name));
    desc->setWordWrap(true);
    desc->setStyleSheet(QStringLiteral("color: #5F6B7A; font-size: 12px;"));
    layout->addWidget(desc);

    *bodyOut = layout;
    return page;
}

QLabel* makeStatusLabel(QWidget* page, const char* name)
{
    QLabel* status = new QLabel(xqTr("Idle."), page);
    status->setObjectName(QString::fromLatin1("xqStageStatus_") + QString::fromLatin1(name));
    status->setWordWrap(true);
    // Reserve a fixed two-line height so toggling between short and long status
    // text does not shift the surrounding layout.
    status->setMinimumHeight(status->fontMetrics().height() * 2 + 4);
    return status;
}

// Promotes a stage's execute button to the primary action: a "primary" style
// class (for an app-wide stylesheet) plus the default-button flag and the
// stage's tool icon.
void styleExecuteButton(QPushButton* run, const char* iconAlias)
{
    run->setProperty("class", "primary");
    run->setDefault(true);
    run->setIcon(QIcon(QString::fromLatin1(":/xq/") + QString::fromLatin1(iconAlias)));
}

// Builds an amber requirement hint bar for stages that need real project input.
// `name` only feeds the objectName; the caller owns the message text.
QLabel* makeHintBar(QWidget* page, const char* name, const QString& message)
{
    QLabel* hint = new QLabel(message, page);
    hint->setObjectName(QString::fromLatin1("xqStageHint_") + QString::fromLatin1(name));
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral(
        "border: 1px solid #E4B65B; background: #FFF7E6; color: #8A5A00;"
        " border-radius: 4px; padding: 6px;"));
    return hint;
}

// Applies consistent spacing / label alignment to a stage form.
void tidyForm(QFormLayout* form)
{
    form->setVerticalSpacing(8);
    form->setHorizontalSpacing(12);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
}

// --- Status -> text mappers ---------------------------------------------

QString text(PathController::Status s)
{
    switch (s) {
    case PathController::Status::Ok:
        return xqTr("OK");
    case PathController::Status::Rejected:
        return xqTr("Rejected: the path service rejected the input");
    case PathController::Status::NullScene:
        return xqTr("No scene / stack attached");
    }
    return xqTr("Unknown");
}

QString text(SegmentationController::Status s)
{
    switch (s) {
    case SegmentationController::Status::Ok:
        return xqTr("Automatic vessel mask prepared.");
    case SegmentationController::Status::Rejected:
        return xqTr("Automatic segmentation request is incomplete.");
    case SegmentationController::Status::PreprocessFailed:
        return xqTr("Vessel preprocessing failed.");
    case SegmentationController::Status::RoiPriorFailed:
        return xqTr("ROI import or alignment failed.");
    case SegmentationController::Status::SegmentationFailed:
        return xqTr("Automatic vessel segmentation failed.");
    case SegmentationController::Status::NullScene:
        return xqTr("No scene / stack attached");
    }
    return xqTr("Unknown");
}

QString text(MeshingController::Status s)
{
    switch (s) {
    case MeshingController::Status::Ok:
        return xqTr("OK");
    case MeshingController::Status::Rejected:
        return xqTr("Rejected: the meshing service rejected the input");
    case MeshingController::Status::ModelNotFound:
        return xqTr("Model not found: the source node carries no surface model");
    case MeshingController::Status::NullScene:
        return xqTr("No scene / stack attached");
    }
    return xqTr("Unknown");
}

QString text(AiController::Status s)
{
    switch (s) {
    case AiController::Status::Ok:
        return xqTr("OK");
    case AiController::Status::Rejected:
        return xqTr("Rejected: the flow-metrics service rejected the flow");
    case AiController::Status::FlowNotFound:
        return xqTr("Flow not found: the source node carries no flow result");
    case AiController::Status::NullScene:
        return xqTr("No scene / stack attached");
    }
    return xqTr("Unknown");
}

QString text(ModelingController::Status s)
{
    switch (s) {
    case ModelingController::Status::Ok:
        return xqTr("OK");
    case ModelingController::Status::Rejected:
        return xqTr("Rejected: the modeling service rejected the contour group");
    case ModelingController::Status::NullScene:
        return xqTr("No scene / stack attached");
    }
    return xqTr("Unknown");
}

QString text(VesselProfileController::Status s)
{
    switch (s) {
    case VesselProfileController::Status::Ok:
        return xqTr("OK: Path input prepared");
    case VesselProfileController::Status::NullContext:
        return xqTr("No project / stack attached");
    case VesselProfileController::Status::ProjectNotOpen:
        return xqTr("Open a project before preparing a Path input");
    case VesselProfileController::Status::InvalidIntent:
        return xqTr("Invalid Path input request");
    case VesselProfileController::Status::TargetExists:
        return xqTr("The Path input output identifier is already in use");
    case VesselProfileController::Status::SourceNotFound:
        return xqTr("Path or ContourGroup source was not found");
    case VesselProfileController::Status::SourceTypeMismatch:
        return xqTr("Selected source has the wrong domain type");
    case VesselProfileController::Status::SourceStale:
        return xqTr("Path or ContourGroup is stale");
    case VesselProfileController::Status::SourcePayloadInvalid:
        return xqTr("Path or ContourGroup payload is invalid");
    case VesselProfileController::Status::SourceAssetInvalid:
        return xqTr("Source asset provenance is invalid");
    case VesselProfileController::Status::EvidenceAssetInvalid:
        return xqTr("Profile evidence asset is invalid");
    case VesselProfileController::Status::AssemblyFailed:
        return xqTr("Path input geometry validation failed");
    case VesselProfileController::Status::ImportFailed:
        return xqTr("Path input import failed");
    case VesselProfileController::Status::SourceChanged:
        return xqTr("Path, ContourGroup, or project changed while preparing the Path input");
    case VesselProfileController::Status::CommitRejected:
        return xqTr("Path input commit was rejected");
    }
    return xqTr("Unknown");
}

QString text(CenterlineBController::Status s)
{
    switch (s) {
    case CenterlineBController::Status::Ok:
        return xqTr("OK: Centerline B Path input built");
    case CenterlineBController::Status::NullContext:
        return xqTr("No project / stack / Centerline B backend attached");
    case CenterlineBController::Status::ProjectNotOpen:
        return xqTr("Open a project before building Centerline B");
    case CenterlineBController::Status::InvalidIntent:
        return xqTr("Invalid Centerline B request");
    case CenterlineBController::Status::TargetExists:
        return xqTr("Centerline B output identifiers are already in use");
    case CenterlineBController::Status::SourceNotFound:
        return xqTr("Selected mask or its source image was not found");
    case CenterlineBController::Status::SourceTypeMismatch:
        return xqTr("Selected Centerline B source has the wrong type");
    case CenterlineBController::Status::SourceStale:
        return xqTr("Selected mask or source image is stale");
    case CenterlineBController::Status::SourcePayloadInvalid:
        return xqTr("Mask geometry or source-image identity is invalid");
    case CenterlineBController::Status::SourceAssetInvalid:
        return xqTr("Mask or source-image asset provenance is invalid");
    case CenterlineBController::Status::UnsupportedScale:
        return xqTr("Centerline B requires an Organ-scale source image");
    case CenterlineBController::Status::AssetIdExhausted:
        return xqTr("Could not allocate Centerline B output assets");
    case CenterlineBController::Status::ComputeFailed:
        return xqTr("Centerline B computation failed");
    case CenterlineBController::Status::SourceChanged:
        return xqTr("Mask, source image, project, or output changed during computation");
    case CenterlineBController::Status::CommitRejected:
        return xqTr("Centerline B atomic commit was rejected");
    }
    return xqTr("Unknown");
}

QString text(PathModuleController::Status s)
{
    switch (s) {
    case PathModuleController::Status::Ok:
        return xqTr("OK: Path module completed");
    case PathModuleController::Status::NullContext:
        return xqTr("No project attached");
    case PathModuleController::Status::ProjectNotOpen:
        return xqTr("Open a project before running a Path module");
    case PathModuleController::Status::InvalidIntent:
        return xqTr("Select a Path input and Path module");
    case PathModuleController::Status::SourceNotFound:
        return xqTr("Path input source was not found");
    case PathModuleController::Status::SourceTypeMismatch:
        return xqTr("Selected source is not a Path input");
    case PathModuleController::Status::SourceStale:
        return xqTr("Selected Path input is stale and must be rebuilt");
    case PathModuleController::Status::SourcePayloadInvalid:
        return xqTr("Selected Path input payload is invalid");
    case PathModuleController::Status::SourceAssetInvalid:
        return xqTr("Selected Path input asset provenance is invalid");
    case PathModuleController::Status::SnapshotFailed:
        return xqTr("Path input could not produce a valid Path snapshot");
    case PathModuleController::Status::GeometrySmokeFailed:
        return xqTr("Path geometry-only smoke failed");
    case PathModuleController::Status::UnknownModule:
        return xqTr("Selected Path module is not registered");
    case PathModuleController::Status::ModuleFailed:
        return xqTr("Path module rejected the snapshot");
    }
    return xqTr("Unknown");
}

QString text(VesselPathSourceKind source)
{
    switch (source) {
    case VesselPathSourceKind::AutomaticCenterlineB:
        return xqTr("Automatic centerline B");
    case VesselPathSourceKind::SemiAutomatic:
        return xqTr("Semi-automatic");
    case VesselPathSourceKind::GoldFile:
        return xqTr("Gold Path file");
    case VesselPathSourceKind::Unknown:
        break;
    }
    return xqTr("Unknown");
}

// --- Path page (fully wired) --------------------------------------------

QWidget* buildPathPage(QStackedWidget* panel,
                       PathController* path,
                       const IdCounter& counter,
                       const ActiveImageProvider& imageProvider,
                       const PathDraftProvider& draftProvider,
                       const PathDraftMutator& draftMutator,
                       const PathDraftFocuser& draftFocuser,
                       const PathPickingSetter& pickingSetter,
                       const StageChangedCallback& stageChanged,
                       const AsyncCommandRunner& asyncRunner)
{
    QVBoxLayout* body = nullptr;
    QWidget* page = makeStagePage(
        panel, "Path",
        xqTr("Path"),
        xqTr("Pick control points in the MPR slices, then generate a centerline."),
        &body);
    QLabel* status = makeStatusLabel(page, "Path");

    QGroupBox* group = new QGroupBox(xqTr("Path parameters"), page);
    QFormLayout* form = new QFormLayout(group);
    tidyForm(form);

    QLineEdit* nameEdit = new QLineEdit(QStringLiteral("path-1"), group);
    nameEdit->setObjectName(QStringLiteral("xqPathNameEdit"));
    nameEdit->setToolTip(xqTr("Name for the generated centerline node (e.g. path-1)."));

    QPushButton* pickToggle = new QPushButton(xqTr("Pick Control Points"), group);
    pickToggle->setObjectName(QStringLiteral("xqPathPickButton"));
    pickToggle->setCheckable(true);
    pickToggle->setToolTip(xqTr(
        "Toggle picking, then click points in any MPR slice to place them along "
        "the vessel. Ctrl+A adds a point at the current crosshair; Esc exits."));

    QListWidget* pointList = new QListWidget(group);
    pointList->setObjectName(QStringLiteral("xqPathPointList"));
    pointList->setSelectionMode(QAbstractItemView::SingleSelection);
    pointList->setToolTip(xqTr(
        "The ordered control points picked so far. At least 2 are needed to "
        "generate a path."));

    QPushButton* removeButton = new QPushButton(xqTr("Remove Selected"), group);
    removeButton->setObjectName(QStringLiteral("xqPathRemoveButton"));
    removeButton->setToolTip(xqTr("Remove the point selected in the list above."));
    QPushButton* clearButton = new QPushButton(xqTr("Clear"), group);
    clearButton->setObjectName(QStringLiteral("xqPathClearButton"));
    clearButton->setToolTip(xqTr("Discard all picked control points."));

    QDoubleSpinBox* spacingSpin = new QDoubleSpinBox(group);
    spacingSpin->setObjectName(QStringLiteral("xqPathSpacingSpin"));
    spacingSpin->setRange(0.01, 100.0);
    spacingSpin->setValue(0.5);
    spacingSpin->setSuffix(QStringLiteral(" mm"));
    spacingSpin->setToolTip(xqTr(
        "Distance between resampled centerline points, in mm. Smaller is smoother "
        "but heavier; 0.5 mm suits most vessels."));

    form->addRow(xqTr("Name"), nameEdit);
    form->addRow(pickToggle);
    form->addRow(pointList);
    form->addRow(removeButton);
    form->addRow(clearButton);
    form->addRow(xqTr("Sample spacing"), spacingSpin);

    QPushButton* run = new QPushButton(xqTr("Generate Path"), page);
    run->setObjectName(QStringLiteral("xqPathRunButton"));
    styleExecuteButton(run, "tool-path.svg");
    run->setEnabled(false); // needs >= 2 draft points

    QLabel* hint = makeHintBar(
        page, "Path",
        xqTr("Open an image, toggle picking, then click points in an MPR slice."));

    body->addWidget(group);
    body->addWidget(run);
    body->addWidget(hint);
    body->addWidget(status);
    body->addStretch();

    if (path == nullptr || !draftProvider || !draftMutator || !pickingSetter) {
        group->setEnabled(false);
        run->setEnabled(false);
        status->setText(xqTr("Controller not attached"));
        return page;
    }

    // Step-by-step guidance: rewrites the hint bar to the next action the user
    // should take, gated on exactly the same state the run button is (image
    // present, picking on, draft point count). Driven from the same points that
    // update the run button (toggle, list refresh, run done) -- no polling.
    auto updateGuidance = [hint, pickToggle, imageProvider, draftProvider]() {
        const bool hasImage = imageProvider && imageProvider().valid;
        if (!hasImage) {
            hint->setText(xqTr("Step 1/3: Open an image (File > Open Image)."));
            return;
        }
        const std::size_t count = draftProvider ? draftProvider().size() : 0;
        if (count >= 2) {
            // Enough points already: the next action is naming + generating,
            // regardless of whether picking is still toggled on.
            hint->setText(xqTr("Step 3/3: Name the path and press Generate Path."));
            return;
        }
        if (!pickToggle->isChecked()) {
            hint->setText(xqTr("Step 2/3: Toggle point picking, then click in a "
                               "slice view (Ctrl+A adds at the crosshair)."));
            return;
        }
        hint->setText(xqTr("Step 2/3: Add at least 2 points (%1 so far).")
                          .arg(count));
    };

    // List refresh: re-renders the draft into the list and gates the run
    // button (>= 2 points). Registered with the window via pickingSetter so
    // MPR picks refresh the page too.
    auto refreshList = [pointList, run, draftProvider, updateGuidance]() {
        const std::vector<PathControlPoint> draft = draftProvider();
        pointList->clear();
        for (std::size_t i = 0; i < draft.size(); ++i) {
            pointList->addItem(QString::fromLatin1("#%1 (%2, %3, %4)")
                                   .arg(i + 1)
                                   .arg(draft[i].position.x, 0, 'f', 1)
                                   .arg(draft[i].position.y, 0, 'f', 1)
                                   .arg(draft[i].position.z, 0, 'f', 1));
        }
        run->setEnabled(draft.size() >= 2);
        updateGuidance();
    };
    // Toggle setter the window uses to force the pick toggle off (mutual
    // exclusion with seed picking). QSignalBlocker keeps the forced state
    // change from re-entering pickingSetter through the toggled handler.
    auto setPickChecked = [pickToggle](bool on) {
        const QSignalBlocker blocker(pickToggle);
        pickToggle->setChecked(on);
    };
    // Register both hooks immediately (picking disabled) so window-side draft
    // changes (test hook / MPR picks) reach the list from the start.
    pickingSetter(false, PathPageHooks{refreshList, setPickChecked});
    refreshList();

    QObject::connect(pickToggle, &QPushButton::toggled, page,
                     [pickingSetter, refreshList, setPickChecked, status,
                      updateGuidance](bool checked) {
        pickingSetter(checked, PathPageHooks{refreshList, setPickChecked});
        status->setText(checked
                            ? xqTr("Click a voxel in an MPR slice to add a control point.")
                            : xqTr("Picking off."));
        updateGuidance();
    });

    QObject::connect(removeButton, &QPushButton::clicked, page,
                     [pointList, draftMutator]() {
        const int row = pointList->currentRow();
        if (row >= 0) {
            draftMutator(row);
        }
    });

    // Double-click a list row -> jump the MPR crosshair to that control point.
    QObject::connect(pointList, &QListWidget::itemDoubleClicked, page,
                     [pointList, draftFocuser](QListWidgetItem* item) {
        if (draftFocuser && item != nullptr) {
            draftFocuser(pointList->row(item));
        }
    });

    QObject::connect(clearButton, &QPushButton::clicked, page,
                     [draftMutator]() {
        draftMutator(-1);
    });

    QObject::connect(run, &QPushButton::clicked, page,
                     [path, counter, nameEdit, spacingSpin, status, imageProvider,
                      draftProvider, draftMutator, setPickChecked,
                      stageChanged, asyncRunner, updateGuidance]() {
        const std::vector<PathControlPoint> draft = draftProvider();
        if (draft.size() < 2) {
            status->setText(xqTr("Pick at least two control points first."));
            return;
        }
        // The path node binds to its source image via a derived relation;
        // link_derived fails on a missing source, so gate on a live image.
        const ActiveImage active = imageProvider ? imageProvider() : ActiveImage{};
        if (!active.valid) {
            status->setText(xqTr("Open an image first."));
            return;
        }

        PathController::AddPathIntent intent;
        intent.newPathId = nextId(counter);
        intent.name = nameEdit->text().toStdString();
        intent.sourceImageNode = active.nodeId;
        intent.controlPoints = draft;
        intent.spacing = spacingSpin->value();

        StageCommandJob job = [path, intent]() {
            StageCommandOutcome outcome;
            PathController::PreparedCommand prepared = path->prepareAddPath(intent);
            outcome.message = prepared.ok()
                ? xqTr("Path created: %1").arg(QString::fromStdString(intent.name))
                : text(prepared.status);
            outcome.command = std::move(prepared.command);
            return outcome;
        };

        auto done = [status, draftMutator, setPickChecked, stageChanged,
                     nameEdit, updateGuidance](bool ok, const QString& message) {
            status->setText(message);
            if (ok) {
                draftMutator(-1); // clear the committed draft (refreshes list)
                setPickChecked(false);
                // setPickChecked blocks the toggled signal, so re-run guidance to
                // reflect the now-off picking state.
                updateGuidance();
                // Auto-increment the default name suffix: path-1 -> path-2.
                const QString name = nameEdit->text();
                const int dash = name.lastIndexOf(QLatin1Char('-'));
                bool numeric = false;
                const int n = name.mid(dash + 1).toInt(&numeric);
                if (dash > 0 && numeric) {
                    nameEdit->setText(name.left(dash + 1) + QString::number(n + 1));
                }
                if (stageChanged) {
                    stageChanged();
                }
            }
        };

        if (asyncRunner) {
            if (!asyncRunner(xqTr("Path"), job, done)) {
                status->setText(xqTr("Another task is still running."));
            }
            return;
        }
        StageCommandOutcome outcome = job();
        const bool ok = outcome.command != nullptr
            && path->commitPrepared(std::move(outcome.command));
        done(ok, outcome.message);
    });

    return page;
}

struct NiftiFilePicker {
    QWidget* row = nullptr;
    QLineEdit* edit = nullptr;
    QPushButton* browse = nullptr;
};

NiftiFilePicker makeNiftiFilePicker(
    QWidget* parent,
    const char* editObjectName,
    const char* browseObjectName,
    const QString& placeholder,
    const QString& dialogTitle)
{
    NiftiFilePicker picker;
    picker.row = new QWidget(parent);
    QHBoxLayout* layout = new QHBoxLayout(picker.row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    picker.edit = new QLineEdit(picker.row);
    picker.edit->setObjectName(QString::fromLatin1(editObjectName));
    picker.edit->setPlaceholderText(placeholder);
    picker.edit->setClearButtonEnabled(true);
    layout->addWidget(picker.edit, 1);

    picker.browse = new QPushButton(
        QIcon(QStringLiteral(":/xq/document-open.svg")), xqTr("Browse..."),
        picker.row);
    picker.browse->setObjectName(QString::fromLatin1(browseObjectName));
    picker.browse->setToolTip(dialogTitle);
    layout->addWidget(picker.browse);

    QObject::connect(picker.browse, &QPushButton::clicked, picker.row,
                     [row = picker.row, edit = picker.edit, dialogTitle]() {
        const QString path = QFileDialog::getOpenFileName(
            row, dialogTitle, edit->text(),
            xqTr("NIfTI files (*.nii *.nii.gz);;All files (*)"));
        if (!path.isEmpty()) {
            edit->setText(path);
        }
    });
    return picker;
}

// --- Segmentation page: the single three-dimensional ROI-v2 product entry ---

QWidget* buildSegmentationPage(QStackedWidget* panel, SegmentationController* segmentation,
                               const IdCounter& counter,
                               const ActiveImageProvider& imageProvider,
                               const StageChangedCallback& stageChanged,
                               const ContourWorkbenchOpener& openContourWorkbench,
                               const AsyncCommandRunner& asyncRunner)
{
    QVBoxLayout* body = nullptr;
    QWidget* page = makeStagePage(
        panel, "Segmentation",
        xqTr("Segmentation"),
        xqTr("Run the fixed ROI-guided automatic vessel segmentation pipeline."),
        &body);
    QLabel* status = makeStatusLabel(page, "Segmentation");

    QGroupBox* group = new QGroupBox(xqTr("Automatic vessel segmentation"), page);
    QFormLayout* form = new QFormLayout(group);
    tidyForm(form);

    QLineEdit* nameEdit = new QLineEdit(xqTr("Vessel mask 1"), group);
    nameEdit->setObjectName(QStringLiteral("xqSegNameEdit"));
    nameEdit->setToolTip(xqTr("Name for the segmentation mask node."));

    const NiftiFilePicker liverPicker = makeNiftiFilePicker(
        group, "xqSegLiverRoiPath", "xqSegLiverRoiBrowse",
        xqTr("Select liver.nii.gz"), xqTr("Select liver ROI"));
    liverPicker.edit->setToolTip(xqTr(
        "Offline TotalSegmentator 2.15.0 liver NIfTI mask."));
    const NiftiFilePicker coarsePicker = makeNiftiFilePicker(
        group, "xqSegCoarseVesselRoiPath", "xqSegCoarseVesselRoiBrowse",
        xqTr("Select portal_vein_and_splenic_vein.nii.gz"),
        xqTr("Select coarse-vessel ROI"));
    coarsePicker.edit->setToolTip(xqTr(
        "Offline TotalSegmentator 2.15.0 portal-vein and splenic-vein NIfTI mask."));

    form->addRow(xqTr("Name"), nameEdit);
    form->addRow(xqTr("Liver ROI"), liverPicker.row);
    form->addRow(xqTr("Coarse-vessel ROI"), coarsePicker.row);

    QPushButton* run = new QPushButton(xqTr("Automatic Segmentation"), page);
    run->setObjectName(QStringLiteral("xqSegAutomaticRun"));
    styleExecuteButton(run, "tool-seg-2d.svg");

    QPushButton* openContours = new QPushButton(
        QIcon(QStringLiteral(":/xq/tool-seg-2d.svg")),
        xqTr("2D Contour Workbench"), page);
    openContours->setObjectName(QStringLiteral("xqSegOpenContourWorkbench"));
    openContours->setEnabled(static_cast<bool>(openContourWorkbench));
    if (openContourWorkbench) {
        QObject::connect(openContours, &QPushButton::clicked, page,
                         [openContourWorkbench]() { openContourWorkbench(); });
    }

    QLabel* hint = makeHintBar(
        page, "Segmentation",
        xqTr("Open an image and select both offline ROI masks."));

    body->addWidget(group);
    body->addWidget(run);
    body->addWidget(openContours);
    body->addWidget(hint);
    body->addWidget(status);
    body->addStretch();

    // The page borrows the current image only while gathering the intent. The
    // worker owns the image metadata and scalar-buffer keepalive for the full
    // preprocess -> ROI read -> v2 segmenter operation.
    if (segmentation == nullptr || !imageProvider) {
        run->setEnabled(false);
        liverPicker.browse->setEnabled(false);
        coarsePicker.browse->setEnabled(false);
        return page;
    }

    auto updateGuidance = [hint, imageProvider, liverEdit = liverPicker.edit,
                           coarseEdit = coarsePicker.edit]() {
        if (!imageProvider || !imageProvider().valid) {
            hint->setText(xqTr("Step 1/3: Open an image first."));
        } else if (liverEdit->text().trimmed().isEmpty()
                   || coarseEdit->text().trimmed().isEmpty()) {
            hint->setText(xqTr("Step 2/3: Select the liver and coarse-vessel ROI files."));
        } else {
            hint->setText(xqTr("Step 3/3: Run the fixed automatic segmentation pipeline."));
        }
    };
    updateGuidance();
    QObject::connect(liverPicker.edit, &QLineEdit::textChanged, page,
                     [updateGuidance](const QString&) { updateGuidance(); });
    QObject::connect(coarsePicker.edit, &QLineEdit::textChanged, page,
                     [updateGuidance](const QString&) { updateGuidance(); });

    QObject::connect(run, &QPushButton::clicked, panel,
                     [segmentation, imageProvider, counter, nameEdit,
                      liverEdit = liverPicker.edit, coarseEdit = coarsePicker.edit,
                      status, stageChanged, asyncRunner, updateGuidance]() {
        const ActiveImage active = imageProvider();
        if (!active.valid || active.image == nullptr || active.buffer == nullptr) {
            status->setText(xqTr("Please select an image node first."));
            updateGuidance();
            return;
        }
        const QString liverPath = liverEdit->text().trimmed();
        const QString coarsePath = coarseEdit->text().trimmed();
        if (liverPath.isEmpty() || coarsePath.isEmpty()) {
            status->setText(xqTr("Select both ROI files before running segmentation."));
            updateGuidance();
            return;
        }

        auto imageCopy = std::make_shared<XQImageVolume>(*active.image);
        std::shared_ptr<const XQMemoryImageBufferHandle> bufferKeepalive =
            active.bufferShared;
        if (bufferKeepalive == nullptr) {
            if (asyncRunner) {
                status->setText(xqTr(
                    "Active image source cannot be retained for background processing."));
                return;
            }
            bufferKeepalive = std::shared_ptr<const XQMemoryImageBufferHandle>(
                active.buffer, [](const XQMemoryImageBufferHandle*) {});
        }

        SegmentationController::AutomaticVesselIntent capturedIntent;
        capturedIntent.newMaskId = nextId(counter);
        capturedIntent.name = nameEdit->text().toStdString();
        capturedIntent.sourceImageNode = active.nodeId;
        capturedIntent.liverRoiPath = liverPath.toStdString();
        capturedIntent.coarseVesselRoiPath = coarsePath.toStdString();

        StageCommandJob job = [segmentation, capturedIntent, imageCopy,
                               bufferKeepalive]() mutable {
            StageCommandOutcome outcome;
            ResidentVoxelSource source(bufferKeepalive);
            capturedIntent.image = imageCopy.get();
            capturedIntent.source = &source;
            SegmentationController::PreparedCommand prepared =
                segmentation->prepareAutomaticVesselSegmentation(capturedIntent);
            outcome.message = text(prepared.status);
            outcome.command = std::move(prepared.command);
            return outcome;
        };

        auto done = [status, stageChanged](bool ok, const QString& message) {
            if (ok) {
                status->setText(xqTr("Automatic vessel mask created."));
                if (stageChanged) {
                    stageChanged();
                }
            } else if (message == xqTr("Automatic vessel mask prepared.")) {
                status->setText(xqTr("Automatic vessel mask commit was rejected."));
            } else {
                status->setText(message);
            }
        };

        if (asyncRunner) {
            if (!asyncRunner(xqTr("Segmentation"), job, done)) {
                status->setText(xqTr("Another task is still running."));
            }
            return;
        }

        // Headless / no-runner fallback: same job + done, run synchronously.
        StageCommandOutcome outcome = job();
        const bool ok = outcome.command != nullptr
            && segmentation->commitPrepared(std::move(outcome.command));
        done(ok, outcome.message);
    });

    return page;
}

// --- Modeling page (controls only; needs a contour group value) ----------

QWidget* buildModelingPage(QStackedWidget* panel,
                           ModelingController* modeling,
                           const IdCounter& counter,
                           const ContourGroupProvider& contourGroupProvider,
                           const SceneNodeLister& nodeLister,
                           const StageChangedCallback& stageChanged,
                           const AsyncCommandRunner& asyncRunner)
{
    QVBoxLayout* body = nullptr;
    QWidget* page = makeStagePage(
        panel, "Modeling",
        xqTr("Modeling"),
        xqTr("Loft contour rings into a closed surface model."),
        &body);
    QLabel* status = makeStatusLabel(page, "Modeling");

    QGroupBox* group = new QGroupBox(xqTr("Loft parameters"), page);
    QFormLayout* form = new QFormLayout(group);
    tidyForm(form);

    QLineEdit* nameEdit = new QLineEdit(xqTr("Model 1"), group);
    nameEdit->setToolTip(xqTr("Name for the lofted surface model node."));
    NodeComboBox* sourceCombo = makeNodeCombo(group, "xqModelingSourceCombo",
                                              nodeLister, XQDomainType::ContourGroup);
    sourceCombo->setToolTip(xqTr(
        "The contour group to loft. Create one in the Segmentation stage; the "
        "list refreshes as new groups appear."));
    QCheckBox* capEnds = new QCheckBox(xqTr("Cap ends"), group);
    capEnds->setChecked(true);
    capEnds->setToolTip(xqTr(
        "Close the two open ends of the lofted tube so the surface is watertight "
        "(recommended for meshing)."));

    form->addRow(xqTr("Name"), nameEdit);
    form->addRow(xqTr("Contour group"), sourceCombo);
    form->addRow(capEnds);

    QPushButton* run = new QPushButton(xqTr("Loft"), page);
    styleExecuteButton(run, "tool-model.svg");

    QLabel* hint = makeHintBar(
        page, "Modeling",
        xqTr("Select a contour group node in the scene or pick one here."));

    body->addWidget(group);
    body->addWidget(run);
    body->addWidget(hint);
    body->addWidget(status);
    body->addStretch();

    if (modeling == nullptr || !contourGroupProvider) {
        group->setEnabled(false);
        run->setEnabled(false);
        status->setText(xqTr("Please select a contour group first."));
        return page;
    }

    // Step-by-step guidance: needs a contour group to exist before lofting.
    // count()==0 means the scene has no contour group yet (the combo was
    // repopulated from the lister at build and on every currentIndexChanged).
    auto updateGuidance = [hint, sourceCombo]() {
        if (sourceCombo->count() == 0) {
            hint->setText(xqTr("Step 1/2: Create a contour group first "
                               "(Segmentation stage)."));
        } else {
            hint->setText(xqTr("Step 2/2: Pick a contour group and press Loft."));
        }
    };
    updateGuidance();
    QObject::connect(sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     page, [updateGuidance](int) { updateGuidance(); });

    QObject::connect(run, &QPushButton::clicked, page,
                     [modeling, contourGroupProvider, counter, nameEdit, sourceCombo,
                      capEnds, status, stageChanged, asyncRunner, updateGuidance]() {
        const ActiveContourGroup active = contourGroupProvider(idFromCombo(sourceCombo));
        if (!active.valid) {
            status->setText(xqTr("Please select a contour group first."));
            return;
        }
        ModelingController::LoftIntent intent;
        intent.newModelId = nextId(counter);
        intent.name = nameEdit->text().toStdString();
        intent.contourGroupNode = active.nodeId;
        intent.contourGroup = active.contourGroup;
        intent.capEnds = capEnds->isChecked();

        StageCommandJob job = [modeling, intent]() {
            StageCommandOutcome outcome;
            ModelingController::PreparedCommand prepared = modeling->prepareLoft(intent);
            outcome.message = text(prepared.status);
            outcome.command = std::move(prepared.command);
            return outcome;
        };

        auto done = [status, stageChanged, sourceCombo, updateGuidance](
                        bool ok, const QString& message) {
            status->setText(message);
            if (ok && stageChanged) {
                stageChanged();
            }
            // A new model may have shifted upstream availability; refresh the
            // picker + guidance so the next step is shown.
            sourceCombo->repopulate();
            updateGuidance();
        };

        if (asyncRunner) {
            if (!asyncRunner(xqTr("Modeling"), job, done)) {
                status->setText(xqTr("Another task is still running."));
            }
            return;
        }

        StageCommandOutcome outcome = job();
        const bool ok = outcome.command != nullptr
            && modeling->commitPrepared(std::move(outcome.command));
        done(ok, outcome.message);
    });

    return page;
}

// --- Meshing page (fully wired) ------------------------------------------

QWidget* buildMeshingPage(QStackedWidget* panel,
                          MeshingController* meshing,
                          const IdCounter& counter,
                          const SceneNodeLister& nodeLister,
                          const StageChangedCallback& stageChanged,
                          const AsyncCommandRunner& asyncRunner)
{
    QVBoxLayout* body = nullptr;
    QWidget* page = makeStagePage(
        panel, "Meshing",
        xqTr("Meshing"),
        xqTr("Generate a surface or volume mesh from the model."),
        &body);
    QLabel* status = makeStatusLabel(page, "Meshing");

    QGroupBox* group = new QGroupBox(xqTr("Mesh parameters"), page);
    QFormLayout* form = new QFormLayout(group);
    tidyForm(form);

    QLineEdit* nameEdit = new QLineEdit(xqTr("Mesh 1"), group);
    nameEdit->setToolTip(xqTr("Name for the generated mesh node."));
    NodeComboBox* sourceCombo = makeNodeCombo(group, "xqMeshingSourceCombo",
                                              nodeLister, XQDomainType::SurfaceModel);
    sourceCombo->setToolTip(xqTr(
        "The surface model to mesh. Create one in the Modeling stage; the list "
        "refreshes as new models appear."));

    QRadioButton* surfaceRadio = new QRadioButton(xqTr("Surface mesh"), group);
    QRadioButton* volumeRadio = new QRadioButton(xqTr("Volume mesh"), group);
    surfaceRadio->setChecked(true);
    surfaceRadio->setToolTip(xqTr(
        "Triangulate the model surface only (fast; for visualization or as a "
        "volume-mesh input)."));
    volumeRadio->setToolTip(xqTr(
        "Fill the model interior with tetrahedra (required for the flow solver)."));

    form->addRow(xqTr("Name"), nameEdit);
    form->addRow(xqTr("Model source"), sourceCombo);
    form->addRow(surfaceRadio);
    form->addRow(volumeRadio);

    QPushButton* run = new QPushButton(xqTr("Build Mesh"), page);
    styleExecuteButton(run, "tool-mesh.svg");

    QLabel* hint = makeHintBar(
        page, "Meshing",
        xqTr("Select a surface model node in the scene or pick one here."));

    body->addWidget(group);
    body->addWidget(run);
    body->addWidget(hint);
    body->addWidget(status);
    body->addStretch();

    if (!meshing) {
        group->setEnabled(false);
        run->setEnabled(false);
        status->setText(xqTr("Controller not attached"));
        return page;
    }

    // Step-by-step guidance: needs a surface model to exist before meshing.
    auto updateGuidance = [hint, sourceCombo]() {
        if (sourceCombo->count() == 0) {
            hint->setText(xqTr("Step 1/2: Create a surface model first "
                               "(Modeling stage)."));
        } else {
            hint->setText(xqTr("Step 2/2: Pick a model, choose surface/volume, "
                               "press Build Mesh."));
        }
    };
    updateGuidance();
    QObject::connect(sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     page, [updateGuidance](int) { updateGuidance(); });

    QObject::connect(run, &QPushButton::clicked, page,
                     [meshing, counter, nameEdit, sourceCombo, surfaceRadio,
                      status, stageChanged, asyncRunner, updateGuidance]() {
        const NodeId modelNode = idFromCombo(sourceCombo);
        const std::string name = nameEdit->text().toStdString();
        const bool isSurface = surfaceRadio->isChecked();
        MeshingController::SurfaceMeshIntent surfaceIntent;
        MeshingController::VolumeMeshIntent volumeIntent;
        if (isSurface) {
            surfaceIntent.newMeshId = nextId(counter);
            surfaceIntent.name = name;
            surfaceIntent.modelNode = modelNode;
        } else {
            volumeIntent.newMeshId = nextId(counter);
            volumeIntent.name = name;
            volumeIntent.modelNode = modelNode;
            volumeIntent.sourceNode = modelNode;
        }

        StageCommandJob job = [meshing, isSurface, surfaceIntent, volumeIntent]() {
            StageCommandOutcome outcome;
            MeshingController::PreparedCommand prepared =
                isSurface ? meshing->prepareSurfaceMesh(surfaceIntent)
                          : meshing->prepareVolumeMesh(volumeIntent);
            outcome.message = text(prepared.status);
            outcome.command = std::move(prepared.command);
            return outcome;
        };

        auto done = [status, stageChanged, sourceCombo, updateGuidance](
                        bool ok, const QString& message) {
            status->setText(message);
            if (ok && stageChanged) {
                stageChanged();
            }
            sourceCombo->repopulate();
            updateGuidance();
        };

        if (asyncRunner) {
            if (!asyncRunner(xqTr("Meshing"), job, done)) {
                status->setText(xqTr("Another task is still running."));
            }
            return;
        }

        StageCommandOutcome outcome = job();
        const bool ok = outcome.command != nullptr
            && meshing->commitPrepared(std::move(outcome.command));
        done(ok, outcome.message);
    });

    return page;
}

// --- Modules page -------------------------------------------------------

QWidget* buildModulesPage(
    QStackedWidget* panel,
    VesselProfileController* vesselProfile,
    const VesselProfileOutputIdProvider& profileOutputIdProvider,
    PathModuleController* pathModules,
    CenterlineBController* centerlineB,
    const CenterlineBOutputIdProvider& centerlineBOutputIdProvider,
    const ActiveImageProvider& imageProvider,
    const SceneNodeLister& nodeLister,
    const StageChangedCallback& stageChanged,
    const AsyncCommandRunner& asyncRunner)
{
    QVBoxLayout* body = nullptr;
    QWidget* page = makeStagePage(
        panel, "Modules",
        xqTr("Path modules"),
        xqTr("Prepare a Path input, inspect its contract, and run a Path-only module."),
        &body);
    QLabel* status = makeStatusLabel(page, "Modules");

    QGroupBox* workflowGroup = new QGroupBox(xqTr("Path input and module"), page);
    QFormLayout* workflowForm = new QFormLayout(workflowGroup);
    tidyForm(workflowForm);

    NodeComboBox* pathCombo = makeNodeCombo(
        workflowGroup, "xqPathInputPathCombo", nodeLister, XQDomainType::Path);
    NodeComboBox* contourCombo = makeNodeCombo(
        workflowGroup, "xqPathInputContourCombo", nodeLister,
        XQDomainType::ContourGroup);
    QLabel* frameSource = new QLabel(
        xqTr("Selected DICOM image (patient LPS/mm)"), workflowGroup);
    frameSource->setWordWrap(true);
    QPushButton* preparePathInput = new QPushButton(
        xqTr("Prepare Path input"), workflowGroup);
    preparePathInput->setObjectName(QStringLiteral("xqPathInputBuild"));
    preparePathInput->setIcon(QIcon(QStringLiteral(":/xq/tool-path.svg")));

    NodeComboBox* centerlineMask = makeNodeCombo(
        workflowGroup, "xqCenterlineBMaskCombo", nodeLister,
        XQDomainType::SegmentationMask);
    QPushButton* buildCenterlineB = new QPushButton(
        xqTr("Build Centerline B"), workflowGroup);
    buildCenterlineB->setObjectName(QStringLiteral("xqCenterlineBBuild"));
    buildCenterlineB->setIcon(QIcon(QStringLiteral(":/xq/tool-path.svg")));

    NodeComboBox* pathModuleSource = makeNodeCombo(
        workflowGroup, "xqPathModuleSourceCombo", nodeLister,
        XQDomainType::VesselProfile);
    QComboBox* pathModuleCombo = new QComboBox(workflowGroup);
    pathModuleCombo->setObjectName(QStringLiteral("xqPathModuleCombo"));
    QLabel* pathSource = new QLabel(
        xqTr("Run a module to inspect source."), workflowGroup);
    pathSource->setObjectName(QStringLiteral("xqPathModuleSource"));
    pathSource->setWordWrap(true);
    QLabel* pathGeometry = new QLabel(
        xqTr("No geometry-only result yet."), workflowGroup);
    pathGeometry->setObjectName(QStringLiteral("xqPathModuleGeometry"));
    pathGeometry->setWordWrap(true);

    workflowForm->addRow(xqTr("Path"), pathCombo);
    workflowForm->addRow(xqTr("ContourGroup"), contourCombo);
    workflowForm->addRow(xqTr("Frame / units"), frameSource);
    workflowForm->addRow(QString(), preparePathInput);
    workflowForm->addRow(xqTr("SegmentationMask"), centerlineMask);
    workflowForm->addRow(QString(), buildCenterlineB);
    workflowForm->addRow(xqTr("Path input"), pathModuleSource);
    workflowForm->addRow(xqTr("Module"), pathModuleCombo);
    workflowForm->addRow(xqTr("Path source"), pathSource);
    workflowForm->addRow(xqTr("Geometry-only"), pathGeometry);

    QPushButton* runPathModule = new QPushButton(xqTr("Run Path module"), page);
    runPathModule->setObjectName(QStringLiteral("xqPathModuleRun"));
    styleExecuteButton(runPathModule, "tool-path.svg");
    QPlainTextEdit* pathDump = new QPlainTextEdit(page);
    pathDump->setObjectName(QStringLiteral("xqPathContractDump"));
    pathDump->setReadOnly(true);
    pathDump->setPlaceholderText(xqTr("Validated Path dump appears here."));
    pathDump->setMinimumHeight(110);
    pathDump->setMaximumHeight(180);
    QPushButton* copyPathDump = new QPushButton(xqTr("Copy Path dump"), page);
    copyPathDump->setObjectName(QStringLiteral("xqPathDumpCopy"));
    copyPathDump->setEnabled(false);
    QLabel* guidance = makeHintBar(
        page, "PathModule",
        xqTr("Prepare or select a Path input, then run Noop or PathValidate."));

    body->addWidget(workflowGroup);
    body->addWidget(runPathModule);
    body->addWidget(pathDump);
    body->addWidget(copyPathDump);
    body->addWidget(guidance);
    body->addWidget(status);
    body->addStretch();

    const bool profileAvailable = vesselProfile != nullptr
        && profileOutputIdProvider && imageProvider && nodeLister;
    const bool moduleAvailable = pathModules != nullptr && nodeLister;
    const bool centerlineAvailable = centerlineB != nullptr
        && centerlineBOutputIdProvider && nodeLister;
    page->setProperty("xqCapabilityState",
                      (moduleAvailable || centerlineAvailable)
                          ? "available" : "unavailable");

    std::vector<PathModuleDescriptor> modules;
    if (moduleAvailable) {
        modules = pathModules->modules();
        for (const PathModuleDescriptor& module : modules) {
            pathModuleCombo->addItem(
                QString::fromStdString(module.name)
                    + QStringLiteral(" v")
                    + QString::fromStdString(module.version),
                QString::fromStdString(module.id));
        }
    } else {
        pathModuleSource->setEnabled(false);
        pathModuleCombo->setEnabled(false);
    }
    if (!centerlineAvailable) {
        centerlineMask->setEnabled(false);
        buildCenterlineB->setEnabled(false);
    }

    auto updateGuidance = [guidance, preparePathInput, buildCenterlineB,
                            runPathModule,
                            pathCombo, contourCombo, pathModuleSource,
                            pathModuleCombo, centerlineMask,
                            profileAvailable, moduleAvailable,
                            centerlineAvailable]() {
        const bool canPrepare = profileAvailable
            && pathCombo->count() > 0 && contourCombo->count() > 0;
        preparePathInput->setEnabled(canPrepare);

        const bool canBuildCenterline = centerlineAvailable
            && centerlineMask->count() > 0;
        buildCenterlineB->setEnabled(canBuildCenterline);

        const bool canRun = moduleAvailable
            && pathModuleSource->count() > 0 && pathModuleCombo->count() > 0;
        runPathModule->setEnabled(canRun);

        if (!moduleAvailable) {
            guidance->setText(xqTr(
                "Attach an open project before running Path modules."));
        } else if (pathModuleSource->count() > 0
                   && pathModuleCombo->count() == 0) {
            guidance->setText(xqTr("No Path modules are registered."));
        } else if (canRun) {
            guidance->setText(xqTr("Run a Path-only module."));
        } else if (canBuildCenterline) {
            guidance->setText(xqTr(
                "Build Centerline B or prepare a contour-derived Path input."));
        } else if (!profileAvailable) {
            guidance->setText(xqTr(
                "Create or load a validated Path input first."));
        } else if (pathCombo->count() == 0) {
            guidance->setText(xqTr(
                "Create or load a patient-space Path first."));
        } else if (contourCombo->count() == 0) {
            guidance->setText(xqTr(
                "Create at least three measured contours first."));
        } else {
            guidance->setText(xqTr(
                "Prepare the LPS/mm geometry source used to rebuild the Path snapshot."));
        }
    };

    auto clearModuleResult = [pathSource, pathGeometry, pathDump, copyPathDump]() {
        pathSource->setText(xqTr("Run a module to inspect source."));
        pathGeometry->setText(xqTr("No geometry-only result yet."));
        pathDump->clear();
        copyPathDump->setEnabled(false);
    };

    QObject::connect(copyPathDump, &QPushButton::clicked, page,
                     [pathDump, status]() {
                         const QString dump = pathDump->toPlainText();
                         if (dump.isEmpty()) {
                             return;
                         }
                         QGuiApplication::clipboard()->setText(dump);
                         status->setText(xqTr("Path dump copied."));
                     });

    QObject::connect(pathCombo,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     page, [updateGuidance](int) { updateGuidance(); });
    QObject::connect(contourCombo,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     page, [updateGuidance](int) { updateGuidance(); });
    QObject::connect(centerlineMask,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     page, [status, updateGuidance](int) {
                         status->setText(xqTr("Idle."));
                         updateGuidance();
                     });
    QObject::connect(pathModuleSource,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     page, [status, clearModuleResult, updateGuidance](int) {
                         clearModuleResult();
                         status->setText(xqTr("Idle."));
                         updateGuidance();
                     });
    QObject::connect(pathModuleCombo,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     page, [status, clearModuleResult, updateGuidance](int) {
                         clearModuleResult();
                         status->setText(xqTr("Idle."));
                         updateGuidance();
                     });

    if (centerlineAvailable) {
        QObject::connect(
            buildCenterlineB, &QPushButton::clicked, page,
            [centerlineB, centerlineBOutputIdProvider, centerlineMask,
             pathCombo, contourCombo, pathModuleSource, status, stageChanged,
             asyncRunner, clearModuleResult, updateGuidance]() {
                clearModuleResult();
                const NodeId maskNode = idFromCombo(centerlineMask);
                if (!maskNode.is_valid()) {
                    status->setText(xqTr("Select a SegmentationMask."));
                    return;
                }
                const CenterlineBOutputIds ids =
                    centerlineBOutputIdProvider();
                if (!ids.valid || !ids.pathNode.is_valid()
                    || !ids.profileNode.is_valid()
                    || ids.pathNode == ids.profileNode) {
                    status->setText(xqTr(
                        "Could not allocate Centerline B outputs."));
                    return;
                }

                CenterlineBController::Intent intent;
                intent.sourceMaskNode = maskNode;
                intent.output.pathNode = ids.pathNode;
                intent.output.pathName = "Centerline B Path";
                intent.output.profileNode = ids.profileNode;
                intent.output.profileName = "Centerline B Path input";
                CenterlineBController::CapturedInput captured =
                    centerlineB->capture(intent);
                if (!captured.ok()) {
                    status->setText(text(captured.status));
                    return;
                }

                StageCommandJob job = [centerlineB,
                                       captured = std::move(captured)]() mutable {
                    StageCommandOutcome outcome;
                    auto prepared = std::make_shared<
                        CenterlineBController::PreparedCommand>(
                        centerlineB->compute(std::move(captured)));
                    outcome.message = text(prepared->status);
                    if (prepared->status
                        == CenterlineBController::Status::ComputeFailed) {
                        outcome.message += QStringLiteral(" (%1 / %2 / %3)")
                            .arg(QString::fromLatin1(
                                centerlineBServiceStatusToken(
                                    prepared->serviceStatus)))
                            .arg(QString::fromLatin1(
                                centerlineSkeletonizationStatusToken(
                                    prepared->skeletonizationStatus)))
                            .arg(QString::fromLatin1(
                                centerlineBGraphStatusToken(
                                    prepared->graphStatus)));
                    }
                    if (prepared->ok()) {
                        outcome.ownerCommit =
                            [centerlineB, prepared](QString* message) mutable {
                                const CenterlineBController::Status commitStatus =
                                    centerlineB->commitPrepared(
                                        std::move(*prepared));
                                if (message != nullptr) {
                                    *message = text(commitStatus);
                                }
                                return commitStatus
                                    == CenterlineBController::Status::Ok;
                            };
                    }
                    return outcome;
                };

                auto done = [status, stageChanged, centerlineMask,
                             pathCombo, contourCombo, pathModuleSource,
                             profileNode = ids.profileNode,
                             clearModuleResult, updateGuidance](
                                bool ok, const QString& message) {
                    if (ok && stageChanged) {
                        stageChanged();
                    }
                    centerlineMask->repopulate();
                    pathCombo->repopulate();
                    contourCombo->repopulate();
                    pathModuleSource->repopulate();
                    if (ok) {
                        const int profileIndex = pathModuleSource->findData(
                            QVariant(static_cast<qulonglong>(
                                profileNode.value())));
                        if (profileIndex >= 0) {
                            pathModuleSource->setCurrentIndex(profileIndex);
                        }
                    }
                    clearModuleResult();
                    updateGuidance();
                    status->setText(message);
                };

                if (asyncRunner) {
                    if (!asyncRunner(xqTr("Centerline B"),
                                     std::move(job), done)) {
                        status->setText(xqTr(
                            "Another task is still running."));
                    }
                    return;
                }

                StageCommandOutcome outcome = job();
                bool ok = false;
                if (outcome.ownerCommit) {
                    ok = outcome.ownerCommit(&outcome.message);
                }
                done(ok, outcome.message);
            });
    }

    if (profileAvailable) {
        QObject::connect(
            preparePathInput, &QPushButton::clicked, page,
            [vesselProfile, profileOutputIdProvider, imageProvider,
             pathCombo, contourCombo, pathModuleSource, status, stageChanged,
             asyncRunner, clearModuleResult, updateGuidance]() {
                clearModuleResult();
                const NodeId pathNode = idFromCombo(pathCombo);
                const NodeId contourNode = idFromCombo(contourCombo);
                if (!pathNode.is_valid() || !contourNode.is_valid()) {
                    status->setText(xqTr(
                        "Select both a Path and ContourGroup."));
                    return;
                }
                const ActiveImage active = imageProvider();
                if (!active.valid || active.image == nullptr
                    || !active.image->hasDicomIdentity()
                    || active.image->dicomIdentity().frameOfReferenceUid.empty()) {
                    status->setText(xqTr(
                        "Select the source DICOM image before preparing the Path input."));
                    return;
                }
                const NodeId profileNode = profileOutputIdProvider();
                if (!profileNode.is_valid()) {
                    status->setText(xqTr(
                        "Could not allocate a Path input output."));
                    return;
                }

                VesselProfileController::ContourIntent intent;
                intent.output.newProfileId = profileNode;
                intent.output.name = "Path input";
                intent.pathNode = pathNode;
                intent.contourGroupNode = contourNode;
                intent.frameOfReferenceId =
                    active.image->dicomIdentity().frameOfReferenceUid;
                VesselProfileController::CapturedContourInput captured =
                    vesselProfile->captureFromContours(intent);
                if (!captured.ok()) {
                    status->setText(text(captured.status));
                    return;
                }

                StageCommandJob job = [vesselProfile,
                                       captured = std::move(captured)]() mutable {
                    StageCommandOutcome outcome;
                    auto prepared = std::make_shared<
                        VesselProfileController::PreparedCommand>(
                        VesselProfileController::computeFromContours(
                            std::move(captured)));
                    outcome.message = text(prepared->status);
                    if (prepared->ok()) {
                        outcome.ownerCommit =
                            [vesselProfile, prepared](QString* message) mutable {
                                const VesselProfileController::Status commitStatus =
                                    vesselProfile->commitPrepared(
                                        std::move(*prepared));
                                if (message != nullptr) {
                                    *message = text(commitStatus);
                                }
                                return commitStatus
                                    == VesselProfileController::Status::Ok;
                            };
                    }
                    return outcome;
                };

                auto done = [status, stageChanged, pathModuleSource,
                              pathCombo, contourCombo, profileNode,
                              clearModuleResult, updateGuidance](
                                 bool ok, const QString& message) {
                    if (ok && stageChanged) {
                        stageChanged();
                    }
                    pathCombo->repopulate();
                    contourCombo->repopulate();
                    pathModuleSource->repopulate();
                    if (ok) {
                        const int preparedIndex = pathModuleSource->findData(
                            QVariant(static_cast<qulonglong>(profileNode.value())));
                        if (preparedIndex >= 0) {
                            pathModuleSource->setCurrentIndex(preparedIndex);
                        }
                    }
                    clearModuleResult();
                    updateGuidance();
                    status->setText(message);
                };

                if (asyncRunner) {
                    if (!asyncRunner(xqTr("Path input preparation"),
                                     std::move(job), done)) {
                        status->setText(xqTr("Another task is still running."));
                    }
                    return;
                }

                StageCommandOutcome outcome = job();
                bool ok = false;
                if (outcome.ownerCommit) {
                    ok = outcome.ownerCommit(&outcome.message);
                }
                done(ok, outcome.message);
            });
    }

    if (moduleAvailable) {
        QObject::connect(
            runPathModule, &QPushButton::clicked, page,
            [pathModules, pathModuleSource, pathModuleCombo, pathSource,
              pathGeometry, pathDump, copyPathDump, guidance, status,
              updateGuidance]() {
                PathModuleController::Intent intent;
                intent.sourceProfileNode = idFromCombo(pathModuleSource);
                intent.moduleId = pathModuleCombo->currentData().toString().toStdString();
                const PathModuleController::Result result = pathModules->run(intent);
                status->setText(text(result.status));
                if (!result.ok()) {
                    qWarning().noquote()
                        << "Path module failed: module="
                        << QString::fromStdString(intent.moduleId)
                        << "status=" << text(result.status)
                        << "diagnostic="
                        << QString::fromStdString(result.diagnostic);
                    pathSource->setText(xqTr("Path source unavailable."));
                    pathGeometry->setText(xqTr("No geometry-only result yet."));
                    pathDump->clear();
                    updateGuidance();
                    return;
                }

                pathSource->setText(text(result.sourceKind));
                pathGeometry->setText(
                    xqTr("%1 stations, %2 mm arc span, radius %3-%4 mm")
                        .arg(static_cast<qulonglong>(result.geometry.stationCount))
                        .arg(result.geometry.arcSpanMm, 0, 'g', 8)
                        .arg(result.geometry.minimumRadiusMm, 0, 'g', 8)
                        .arg(result.geometry.maximumRadiusMm, 0, 'g', 8));
                pathDump->setPlainText(
                    QString::fromStdString(result.geometry.canonicalDump));
                copyPathDump->setEnabled(true);
                guidance->setText(
                    xqTr("Module %1 completed.")
                        .arg(QString::fromStdString(result.module.name)));
                qInfo().noquote()
                    << "Path module completed: module="
                    << QString::fromStdString(result.module.id)
                    << "source="
                    << QString::fromLatin1(vesselPathSourceToken(result.sourceKind))
                    << "stations="
                    << static_cast<qulonglong>(result.geometry.stationCount)
                    << "radius_mm="
                    << QStringLiteral("%1..%2")
                           .arg(result.geometry.minimumRadiusMm, 0, 'g', 8)
                           .arg(result.geometry.maximumRadiusMm, 0, 'g', 8);
            });
    }

    updateGuidance();
    return page;
}

// --- AI page (fully wired) -----------------------------------------------

QWidget* buildAiPage(QStackedWidget* panel,
                     AiController* ai,
                     const IdCounter& counter,
                     const SceneNodeLister& nodeLister,
                     const StageChangedCallback& stageChanged)
{
    QVBoxLayout* body = nullptr;
    QWidget* page = makeStagePage(
        panel, "AI",
        xqTr("AI"),
        xqTr("Compute hemodynamic metrics (FFR) from a flow result."),
        &body);
    QLabel* status = makeStatusLabel(page, "AI");

    QGroupBox* group = new QGroupBox(xqTr("Flow-metrics parameters"), page);
    QFormLayout* form = new QFormLayout(group);
    tidyForm(form);

    QLineEdit* nameEdit = new QLineEdit(xqTr("Analysis 1"), group);
    nameEdit->setToolTip(xqTr("Name for the flow-metrics analysis node."));
    NodeComboBox* sourceCombo = makeNodeCombo(group, "xqAiSourceCombo",
                                              nodeLister, XQDomainType::FlowResult);
    sourceCombo->setToolTip(xqTr(
        "The flow result to analyze. Load one or create one through a later "
        "flow module; the list refreshes as results appear."));

    QDoubleSpinBox* muSpin = new QDoubleSpinBox(group);
    muSpin->setRange(0.0, 100.0);
    muSpin->setDecimals(4);
    muSpin->setValue(0.04);
    muSpin->setToolTip(xqTr(
        "Blood dynamic viscosity in poise. Typical value ~0.04 poise "
        "(4 cP)."));
    QDoubleSpinBox* ffrSpin = new QDoubleSpinBox(group);
    ffrSpin->setRange(0.0, 1.0);
    ffrSpin->setDecimals(3);
    ffrSpin->setValue(0.8);
    ffrSpin->setToolTip(xqTr(
        "FFR below this value flags a lesion as significant. Clinical cutoff is "
        "0.80."));
    QDoubleSpinBox* refPressureSpin = new QDoubleSpinBox(group);
    refPressureSpin->setRange(-1000000.0, 1000000.0);
    refPressureSpin->setValue(0.0);
    refPressureSpin->setToolTip(xqTr(
        "Reference (outlet) pressure offset applied to the computed field, in "
        "the solver's pressure units. Leave 0 unless calibrating."));

    form->addRow(xqTr("Name"), nameEdit);
    form->addRow(xqTr("Flow source"), sourceCombo);
    form->addRow(xqTr("Viscosity mu [poise]"), muSpin);
    form->addRow(xqTr("FFR risk threshold"), ffrSpin);
    form->addRow(xqTr("Reference pressure"), refPressureSpin);

    QPushButton* run = new QPushButton(xqTr("Analyze"), page);
    styleExecuteButton(run, "tool-flow.svg");

    QLabel* hint = makeHintBar(
        page, "AI",
        xqTr("Select a flow result node in the scene or pick one here."));

    body->addWidget(group);
    body->addWidget(run);
    body->addWidget(hint);
    body->addWidget(status);
    body->addStretch();

    if (!ai) {
        group->setEnabled(false);
        run->setEnabled(false);
        status->setText(xqTr("Controller not attached"));
        return page;
    }

    // Step-by-step guidance: needs a flow result to exist before analysis.
    auto updateGuidance = [hint, sourceCombo]() {
        if (sourceCombo->count() == 0) {
            hint->setText(xqTr("Step 1/2: Load or create a FlowResult first."));
        } else {
            hint->setText(xqTr("Step 2/2: Pick a flow result and press Analyze."));
        }
    };
    updateGuidance();
    QObject::connect(sourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                     page, [updateGuidance](int) { updateGuidance(); });

    QObject::connect(run, &QPushButton::clicked, page, [=]() {
        AiController::AnalyzeFlowIntent intent;
        intent.newAnalysisId = nextId(counter);
        intent.name = nameEdit->text().toStdString();
        intent.flowNode = idFromCombo(sourceCombo);
        intent.request.mu = muSpin->value();
        intent.request.ffrRiskThreshold = ffrSpin->value();
        intent.request.referencePressure = refPressureSpin->value();
        const AiController::Status st = ai->analyzeFlow(intent);
        status->setText(text(st));
        if (st == AiController::Status::Ok && stageChanged) {
            stageChanged();
        }
        sourceCombo->repopulate();
        updateGuidance();
    });

    return page;
}

} // namespace

void populateStagePanels(QStackedWidget* panel, const StagePanelContext& context)
{
    if (!panel) {
        return;
    }

    IdCounter counter = std::make_shared<NodeId::ValueType>(1000);

    panel->addWidget(buildPathPage(panel, context.path, counter, context.imageProvider,
                                   context.pathDraftProvider, context.pathDraftMutator,
                                   context.pathDraftFocuser, context.pathPickingSetter,
                                   context.stageChanged, context.asyncRunner));
    panel->addWidget(buildSegmentationPage(panel, context.segmentation, counter,
                                           context.imageProvider, context.stageChanged,
                                           context.openContourWorkbench,
                                           context.asyncRunner));
    panel->addWidget(buildModelingPage(panel, context.modeling, counter,
                                       context.contourGroupProvider, context.nodeLister,
                                       context.stageChanged, context.asyncRunner));
    panel->addWidget(buildMeshingPage(panel, context.meshing, counter, context.nodeLister,
                                      context.stageChanged, context.asyncRunner));
    panel->addWidget(buildModulesPage(
        panel, context.vesselProfile,
        context.vesselProfileOutputIdProvider,
        context.pathModules,
        context.centerlineB,
        context.centerlineBOutputIdProvider,
        context.imageProvider, context.nodeLister,
        context.stageChanged, context.asyncRunner));
    panel->addWidget(buildAiPage(panel, context.ai, counter, context.nodeLister,
                                 context.stageChanged));
}

} // namespace xq
