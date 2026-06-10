#include "xq_MainWindow.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportCommand.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataManagementService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_MeasurementService.h"
#include "Core/xq_PreferencesService.h"
#include "Core/xq_ProjectFilePathProvider.h"
#include "Core/xq_ProjectSessionService.h"
#include "Core/xq_ProjectService.h"
#include "Core/xq_SceneExportService.h"
#include "Core/xq_SceneFilePathProvider.h"
#include "Core/xq_ScreenshotFilePathProvider.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowContextService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowRegistry.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Core/xq_TaskRunner.h"
#include "xq_DataHierarchyModel.h"
#include "xq_PreferencesDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QPushButton>
#include <QPixmap>
#include <QResource>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QSize>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTableView>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QVariantList>
#include <QWidget>

#include <mitkDataStorage.h>
#include <mitkBaseProperty.h>
#include <mitkPropertyList.h>
#include <mitkProperties.h>
#include <mitkRenderingManager.h>
#include <mitkSurface.h>

#include <utility>

static void InitXqApplicationResources()
{
    Q_INIT_RESOURCE(xqApplication);
}

namespace xq::presentation
{

namespace
{

QString RoleDisplayName(xq::core::DataWorkflowRole role)
{
    switch (role)
    {
    case xq::core::DataWorkflowRole::DICOMSeries:
        return QStringLiteral("DICOM Series");
    case xq::core::DataWorkflowRole::Image:
        return QStringLiteral("Image");
    case xq::core::DataWorkflowRole::Path:
        return QStringLiteral("Path");
    case xq::core::DataWorkflowRole::Segmentation:
        return QStringLiteral("Segmentation");
    case xq::core::DataWorkflowRole::Model:
        return QStringLiteral("Model");
    case xq::core::DataWorkflowRole::Mesh:
        return QStringLiteral("Mesh");
    case xq::core::DataWorkflowRole::SimulationPrep:
        return QStringLiteral("Simulation Prep");
    case xq::core::DataWorkflowRole::SimulationResult:
        return QStringLiteral("Simulation Result");
    case xq::core::DataWorkflowRole::ROMSimulation:
        return QStringLiteral("ROM Simulation");
    case xq::core::DataWorkflowRole::MultiPhysics:
        return QStringLiteral("MultiPhysics");
    case xq::core::DataWorkflowRole::Unknown:
        break;
    }

    return QStringLiteral("Unknown");
}

bool ParseIntegerPointList(const QString& text,
                           QVariantList& points,
                           QString* message)
{
    points.clear();
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
    {
        if (message)
            *message = QStringLiteral(
                "Point list must contain at least one x,y,z triplet.");
        return false;
    }

    const QStringList pointTexts =
        trimmed.split(QStringLiteral(";"), Qt::SkipEmptyParts);
    for (const auto& pointText : pointTexts)
    {
        const QStringList coordinates =
            pointText.trimmed().split(QStringLiteral(","));
        if (coordinates.size() != 3)
        {
            if (message)
                *message = QStringLiteral(
                    "Point list entries must use x,y,z format.");
            return false;
        }

        QVariantList point;
        for (const auto& coordinateText : coordinates)
        {
            bool ok = false;
            const int coordinate = coordinateText.trimmed().toInt(&ok);
            if (!ok)
            {
                if (message)
                    *message = QStringLiteral(
                        "Point list coordinates must be integers.");
                return false;
            }
            point.push_back(coordinate);
        }
        points.push_back(point);
    }

    if (points.isEmpty())
    {
        if (message)
            *message = QStringLiteral(
                "Point list must contain at least one x,y,z triplet.");
        return false;
    }

    if (message)
        message->clear();
    return true;
}

QString FormatIntegerPointList(const QVariant& value)
{
    QStringList pointTexts;
    const QVariantList points = value.toList();
    for (const auto& pointValue : points)
    {
        const QVariantList point = pointValue.toList();
        if (point.size() != 3)
            continue;

        pointTexts.push_back(QStringLiteral("%1,%2,%3")
                                 .arg(point.at(0).toInt())
                                 .arg(point.at(1).toInt())
                                 .arg(point.at(2).toInt()));
    }

    return pointTexts.join(QStringLiteral("; "));
}

QString FormatMitkColor(const float rgb[3])
{
    return QStringLiteral("(%1, %2, %3)")
        .arg(QString::number(rgb[0], 'f', 2),
             QString::number(rgb[1], 'f', 2),
             QString::number(rgb[2], 'f', 2));
}

QString ColorButtonStyle(const float rgb[3])
{
    QColor color;
    color.setRgbF(rgb[0], rgb[1], rgb[2]);
    return QStringLiteral(
               "QPushButton { background-color: %1; border: 1px solid #555; }")
        .arg(color.name());
}

QString DataManagerPropertyValue(const mitk::BaseProperty* property)
{
    if (!property)
        return QStringLiteral("<null>");

    if (const auto* boolProperty =
            dynamic_cast<const mitk::BoolProperty*>(property))
    {
        return boolProperty->GetValue() ? QStringLiteral("true")
                                        : QStringLiteral("false");
    }

    return QString::fromStdString(property->GetValueAsString());
}

bool ShouldShowDataManagerProperty(const std::string& key)
{
    if (key == "name" || key == "visible" || key == "opacity" ||
        key == "color")
    {
        return false;
    }

    return key.rfind("xq.", 0) == 0 || key == "binary" ||
           key == "volumerendering" || key == "material.representation" ||
           key == "layer" || key == "show contour" ||
           key == "levelwindow" || key.find("DICOM") != std::string::npos ||
           key.find("dicom") != std::string::npos;
}

struct WorkflowToolDescriptor
{
    const char* WorkflowId;
    const char* Label;
    const char* IconPath;
};

const WorkflowToolDescriptor kWorkflowTools[] = {
    {"image-preprocessing", "Image", ":/xq/tool-process.svg"},
    {"path", "Path", ":/xq/tool-path.svg"},
    {"segmentation-2d", "2D Seg", ":/xq/tool-seg-2d.svg"},
    {"segmentation-3d", "3D Seg", ":/xq/tool-seg-3d.svg"},
    {"modeling", "Model", ":/xq/tool-model.svg"},
    {"meshing", "Mesh", ":/xq/tool-mesh.svg"},
    {"flow-simulation", "Simulation", ":/xq/tool-flow.svg"},
};

} // namespace

MainWindow::MainWindow(xq::core::ApplicationContext& context, QWidget* parent)
    : QMainWindow(parent)
    , m_Context(context)
{
    setWindowTitle(QStringLiteral("XQ"));
    InitXqApplicationResources();
    resize(1440, 960);
    statusBar()->setObjectName(QStringLiteral("xqProjectStatusBar"));
    statusBar()->showMessage(QStringLiteral("No project"));

    menuBar()->setNativeMenuBar(false);
    auto* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));
    fileMenu->setObjectName(QStringLiteral("FileMenu"));
    auto* editMenu = menuBar()->addMenu(QStringLiteral("&Edit"));
    editMenu->setObjectName(QStringLiteral("EditMenu"));
    auto* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));
    viewMenu->setObjectName(QStringLiteral("ViewMenu"));
    auto* toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));
    toolsMenu->setObjectName(QStringLiteral("ToolsMenu"));

    auto* mainToolbar = new QToolBar(QStringLiteral("Main Actions"), this);
    mainToolbar->setObjectName(QStringLiteral("mainActionsToolBar"));
    mainToolbar->setMovable(false);
    mainToolbar->setFloatable(false);
    mainToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    mainToolbar->setIconSize(QSize(24, 24));

    auto* newProjectAction =
        new QAction(QIcon(QStringLiteral(":/xq/document-new.svg")),
                    QStringLiteral("New Project"),
                    this);
    newProjectAction->setObjectName(QStringLiteral("xqNewProjectAction"));
    newProjectAction->setShortcut(QKeySequence::New);
    fileMenu->addAction(newProjectAction);

    auto* openProjectAction =
        new QAction(QIcon(QStringLiteral(":/xq/document-open.svg")),
                    QStringLiteral("Open Project"),
                    this);
    openProjectAction->setObjectName(QStringLiteral("xqOpenProjectAction"));
    openProjectAction->setShortcut(QKeySequence::Open);
    fileMenu->addAction(openProjectAction);

    m_SaveProjectAction =
        new QAction(QIcon(QStringLiteral(":/xq/document-save.svg")),
                    QStringLiteral("Save"),
                    this);
    m_SaveProjectAction->setObjectName(
        QStringLiteral("xqSaveProjectAction"));
    m_SaveProjectAction->setShortcut(QKeySequence::Save);
    m_SaveProjectAction->setEnabled(false);
    fileMenu->addAction(m_SaveProjectAction);

    auto* saveAsProjectAction =
        new QAction(QStringLiteral("Save As..."), this);
    saveAsProjectAction->setObjectName(
        QStringLiteral("xqSaveAsProjectAction"));
    saveAsProjectAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    fileMenu->addAction(saveAsProjectAction);

    auto* closeProjectAction =
        new QAction(QStringLiteral("Close Workspace"), this);
    closeProjectAction->setObjectName(QStringLiteral("xqCloseProjectAction"));
    closeProjectAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    fileMenu->addAction(closeProjectAction);
    fileMenu->addSeparator();

    m_ImportDataAction =
        new QAction(QStringLiteral("Open Data File..."), this);
    m_ImportDataAction->setObjectName(QStringLiteral("xqImportDataAction"));
    fileMenu->addAction(m_ImportDataAction);

    auto* importDicomAction =
        new QAction(QStringLiteral("Import DICOM..."), this);
    importDicomAction->setObjectName(QStringLiteral("xqImportDicomAction"));
    fileMenu->addAction(importDicomAction);

    auto* saveSceneAction =
        new QAction(QStringLiteral("Save All as MITK Scene..."), this);
    saveSceneAction->setObjectName(QStringLiteral("xqSaveSceneAction"));
    fileMenu->addAction(saveSceneAction);
    fileMenu->addSeparator();

    auto* recentProjectsMenu =
        fileMenu->addMenu(QStringLiteral("Recent Projects"));
    recentProjectsMenu->setObjectName(
        QStringLiteral("xqRecentProjectsMenu"));
    auto* noRecentProjectsAction =
        recentProjectsMenu->addAction(QStringLiteral("(No recent projects)"));
    noRecentProjectsAction->setEnabled(false);
    fileMenu->addSeparator();

    auto* exitAction = new QAction(QStringLiteral("Exit"), this);
    exitAction->setObjectName(QStringLiteral("xqExitAction"));
    exitAction->setShortcut(QKeySequence::Quit);
    fileMenu->addAction(exitAction);

    auto* undoAction =
        new QAction(QIcon(QStringLiteral(":/xq/edit-undo.svg")),
                    QStringLiteral("Undo"),
                    this);
    undoAction->setObjectName(QStringLiteral("xqUndoAction"));
    undoAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Z")));
    undoAction->setEnabled(false);
    undoAction->setStatusTip(QStringLiteral(
        "Undo will be enabled when monolith edit history is available."));
    editMenu->addAction(undoAction);

    auto* redoAction =
        new QAction(QIcon(QStringLiteral(":/xq/edit-redo.svg")),
                    QStringLiteral("Redo"),
                    this);
    redoAction->setObjectName(QStringLiteral("xqRedoAction"));
    redoAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Y")));
    redoAction->setEnabled(false);
    redoAction->setStatusTip(QStringLiteral(
        "Redo will be enabled when monolith edit history is available."));
    editMenu->addAction(redoAction);

    auto* screenshotAction =
        new QAction(QIcon(QStringLiteral(":/xq/camera-photo.svg")),
                    QStringLiteral("Screenshot..."),
                    this);
    screenshotAction->setObjectName(QStringLiteral("xqScreenshotAction"));
    screenshotAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    viewMenu->addAction(screenshotAction);

    auto* volumeRenderingAction =
        new QAction(QStringLiteral("Volume Rendering"), this);
    volumeRenderingAction->setObjectName(
        QStringLiteral("xqVolumeRenderingAction"));
    volumeRenderingAction->setCheckable(true);
    viewMenu->addAction(volumeRenderingAction);

    auto* crosshairAction =
        new QAction(QStringLiteral("Crosshair"), this);
    crosshairAction->setObjectName(QStringLiteral("xqCrosshairAction"));
    crosshairAction->setCheckable(true);
    crosshairAction->setChecked(
        m_Context.Preferences()->BoolValue(
            QStringLiteral("view.crosshair.enabled"),
            true));
    viewMenu->addAction(crosshairAction);
    viewMenu->addSeparator();

    auto* viewPresetMenu = viewMenu->addMenu(QStringLiteral("View Presets"));
    viewPresetMenu->setObjectName(QStringLiteral("xqViewPresetsMenu"));
    for (const auto& presetName :
         {QStringLiteral("Default"),
          QStringLiteral("Viewer"),
          QStringLiteral("Analysis")})
    {
        auto* presetAction = viewPresetMenu->addAction(presetName);
        presetAction->setObjectName(
            QStringLiteral("xqViewPreset_%1").arg(presetName));
    }

    auto* preferencesAction =
        new QAction(QStringLiteral("Preferences..."), this);
    preferencesAction->setObjectName(
        QStringLiteral("xqOpenPreferencesAction"));
    toolsMenu->addAction(preferencesAction);
    toolsMenu->addSeparator();

    auto* measureDistanceAction =
        new QAction(QStringLiteral("Measure Distance"), this);
    measureDistanceAction->setObjectName(
        QStringLiteral("xqMeasureDistanceAction"));
    measureDistanceAction->setEnabled(false);
    measureDistanceAction->setStatusTip(QStringLiteral(
        "Interactive distance measurement will be enabled after monolith picking is available."));
    toolsMenu->addAction(measureDistanceAction);

    auto* measureAngleAction =
        new QAction(QStringLiteral("Measure Angle"), this);
    measureAngleAction->setObjectName(
        QStringLiteral("xqMeasureAngleAction"));
    measureAngleAction->setEnabled(false);
    measureAngleAction->setStatusTip(QStringLiteral(
        "Interactive angle measurement will be enabled after monolith picking is available."));
    toolsMenu->addAction(measureAngleAction);

    m_MeasureAreaAction =
        new QAction(QStringLiteral("Measure Surface Area"), this);
    m_MeasureAreaAction->setObjectName(
        QStringLiteral("xqMeasureAreaAction"));
    m_MeasureAreaAction->setEnabled(false);
    toolsMenu->addAction(m_MeasureAreaAction);

    m_MeasureVolumeAction =
        new QAction(QStringLiteral("Measure Volume"), this);
    m_MeasureVolumeAction->setObjectName(
        QStringLiteral("xqMeasureVolumeAction"));
    m_MeasureVolumeAction->setEnabled(false);
    toolsMenu->addAction(m_MeasureVolumeAction);

    mainToolbar->addAction(openProjectAction);
    mainToolbar->addAction(m_SaveProjectAction);
    mainToolbar->addSeparator();
    mainToolbar->addAction(undoAction);
    mainToolbar->addAction(redoAction);
    mainToolbar->addSeparator();
    mainToolbar->addAction(screenshotAction);
    mainToolbar->addSeparator();
    mainToolbar->addAction(m_ImportDataAction);

    m_RemoveDataAction =
        new QAction(QStringLiteral("Remove Data"), this);
    m_RemoveDataAction->setObjectName(QStringLiteral("xqRemoveDataAction"));
    m_RemoveDataAction->setEnabled(false);
    mainToolbar->addAction(m_RemoveDataAction);
    addToolBar(Qt::TopToolBarArea, mainToolbar);

    connect(newProjectAction,
            &QAction::triggered,
            this,
            [this]() {
                CreateProjectFromProvider();
            });
    connect(openProjectAction,
            &QAction::triggered,
            this,
            [this]() {
                OpenProjectFromProvider();
            });
    connect(saveAsProjectAction,
            &QAction::triggered,
            this,
            [this]() {
                SaveProjectAsFromProvider();
            });
    connect(closeProjectAction,
            &QAction::triggered,
            this,
            [this]() {
                CloseWorkspace();
            });
    connect(importDicomAction,
            &QAction::triggered,
            this,
            [this]() {
                ImportDicomData();
            });
    connect(saveSceneAction,
            &QAction::triggered,
            this,
            [this]() {
                SaveMitkScene();
            });
    connect(exitAction, &QAction::triggered, this, [this]() { close(); });
    connect(screenshotAction,
            &QAction::triggered,
            this,
            [this]() {
                CaptureScreenshot();
            });
    connect(volumeRenderingAction,
            &QAction::triggered,
            this,
            [this](bool checked) {
                SetSelectedDataVolumeRendering(checked);
            });
    connect(crosshairAction,
            &QAction::triggered,
            this,
            [this](bool checked) {
                SetCrosshairEnabled(checked);
            });
    connect(preferencesAction,
            &QAction::triggered,
            this,
            [this]() {
                OpenPreferencesDialog();
            });
    connect(m_MeasureAreaAction,
            &QAction::triggered,
            this,
            [this]() {
                RunSurfaceMeasurement(
                    xq::core::SurfaceMeasurementKind::Area);
            });
    connect(m_MeasureVolumeAction,
            &QAction::triggered,
            this,
            [this]() {
                RunSurfaceMeasurement(
                    xq::core::SurfaceMeasurementKind::Volume);
            });

    auto* viewToolbar = new QToolBar(QStringLiteral("XQ Views"), this);
    viewToolbar->setObjectName(QStringLiteral("xqViewToolBar"));
    viewToolbar->setMovable(false);
    viewToolbar->setFloatable(false);
    viewToolbar->setAllowedAreas(Qt::TopToolBarArea);
    viewToolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    viewToolbar->setIconSize(QSize(36, 36));
    m_WorkflowToolbarActionGroup = new QActionGroup(this);
    m_WorkflowToolbarActionGroup->setExclusive(true);
    for (const auto& tool : kWorkflowTools)
    {
        const QString workflowId = QString::fromLatin1(tool.WorkflowId);
        auto* action = new QAction(QIcon(QString::fromLatin1(tool.IconPath)),
                                   QString::fromLatin1(tool.Label),
                                   this);
        action->setObjectName(
            QStringLiteral("xqToolAction_%1").arg(workflowId));
        action->setCheckable(true);
        action->setActionGroup(m_WorkflowToolbarActionGroup);
        connect(action,
                &QAction::triggered,
                this,
                [this, workflowId]() {
                    m_Context.WorkflowSelection()->SelectWorkflow(workflowId);
        });
        m_WorkflowToolbarActions.insert(workflowId, action);
        viewToolbar->addAction(action);
        if (auto* button =
                qobject_cast<QToolButton*>(viewToolbar->widgetForAction(action)))
        {
            button->setObjectName(
                QStringLiteral("xqToolButton_%1").arg(workflowId));
            button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            button->setIconSize(QSize(36, 36));
            button->setFocusPolicy(Qt::NoFocus);
            button->setMinimumWidth(82);
            button->setMaximumWidth(90);
            button->setMinimumHeight(58);
        }
    }
    addToolBar(Qt::TopToolBarArea, viewToolbar);

    m_RenderHostContainer = new QWidget(this);
    m_RenderHostContainer->setObjectName(
        QStringLiteral("xqRenderHostContainer"));
    auto* renderHostLayout = new QVBoxLayout(m_RenderHostContainer);
    renderHostLayout->setContentsMargins(0, 0, 0, 0);

    m_RenderHost = new QFrame(m_RenderHostContainer);
    m_RenderHost->setObjectName(QStringLiteral("xqRenderPlaceholder"));
    renderHostLayout->addWidget(m_RenderHost);
    setCentralWidget(m_RenderHostContainer);

    auto* dataManagerDock =
        new QDockWidget(QStringLiteral("Data Manager"), this);
    dataManagerDock->setObjectName(QStringLiteral("xqDataManagerDock"));
    dataManagerDock->setAllowedAreas(Qt::LeftDockWidgetArea |
                                     Qt::RightDockWidgetArea);
    m_DataHierarchyModel =
        new DataHierarchyModel(*m_Context.DataHierarchy(), dataManagerDock);
    m_DataManagerDock = dataManagerDock;

    auto* dataManagerPanel = new QWidget(dataManagerDock);
    dataManagerPanel->setObjectName(QStringLiteral("xqDataManagerPanel"));
    auto* dataManagerLayout = new QVBoxLayout(dataManagerPanel);
    dataManagerLayout->setContentsMargins(2, 2, 2, 2);
    dataManagerLayout->setSpacing(4);

    auto* dataSearchBox = new QLineEdit(dataManagerPanel);
    dataSearchBox->setObjectName(QStringLiteral("xqDataManagerSearchBox"));
    dataSearchBox->setPlaceholderText(QStringLiteral("Search nodes..."));
    dataSearchBox->setClearButtonEnabled(true);
    dataManagerLayout->addWidget(dataSearchBox);

    m_DataHierarchyView = new QTreeView(dataManagerPanel);
    m_DataHierarchyView->setObjectName(
        QStringLiteral("xqDataHierarchyView"));
    m_DataHierarchyView->setHeaderHidden(true);
    m_DataHierarchyView->setMinimumHeight(160);
    m_DataHierarchyView->setModel(m_DataHierarchyModel);
    m_DataHierarchyView->setContextMenuPolicy(Qt::ActionsContextMenu);
    dataManagerLayout->addWidget(m_DataHierarchyView, 1);

    m_RenameDataAction =
        new QAction(QIcon::fromTheme(QStringLiteral("edit-rename")),
                    QStringLiteral("Rename..."),
                    m_DataHierarchyView);
    m_RenameDataAction->setObjectName(QStringLiteral("xqRenameDataAction"));
    m_RenameDataAction->setShortcut(QKeySequence(Qt::Key_F2));
    m_RenameDataAction->setEnabled(false);
    m_DataHierarchyView->addAction(m_RenameDataAction);

    m_ToggleDataVisibilityAction =
        new QAction(QStringLiteral("Toggle Visibility"), m_DataHierarchyView);
    m_ToggleDataVisibilityAction->setObjectName(
        QStringLiteral("xqToggleDataVisibilityAction"));
    m_ToggleDataVisibilityAction->setShortcut(Qt::Key_Space);
    m_ToggleDataVisibilityAction->setEnabled(false);
    m_DataHierarchyView->addAction(m_ToggleDataVisibilityAction);

    m_RemoveSelectedDataAction =
        new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")),
                    QStringLiteral("Remove"),
                    m_DataHierarchyView);
    m_RemoveSelectedDataAction->setObjectName(
        QStringLiteral("xqRemoveSelectedDataAction"));
    m_RemoveSelectedDataAction->setShortcut(QKeySequence::Delete);
    m_RemoveSelectedDataAction->setEnabled(false);
    m_DataHierarchyView->addAction(m_RemoveSelectedDataAction);

    auto* dataManagerSeparator = new QAction(m_DataHierarchyView);
    dataManagerSeparator->setSeparator(true);
    m_DataHierarchyView->addAction(dataManagerSeparator);

    m_ShowOnlySelectedDataAction =
        new QAction(QStringLiteral("Show Only Selected"), m_DataHierarchyView);
    m_ShowOnlySelectedDataAction->setObjectName(
        QStringLiteral("xqShowOnlySelectedDataAction"));
    m_ShowOnlySelectedDataAction->setEnabled(false);
    m_DataHierarchyView->addAction(m_ShowOnlySelectedDataAction);

    auto* makeAllVisibleAction =
        new QAction(QStringLiteral("Make All Visible"), m_DataHierarchyView);
    makeAllVisibleAction->setObjectName(
        QStringLiteral("xqMakeAllDataVisibleAction"));
    m_DataHierarchyView->addAction(makeAllVisibleAction);

    auto* makeAllInvisibleAction =
        new QAction(QStringLiteral("Make All Invisible"), m_DataHierarchyView);
    makeAllInvisibleAction->setObjectName(
        QStringLiteral("xqMakeAllDataInvisibleAction"));
    m_DataHierarchyView->addAction(makeAllInvisibleAction);

    auto* reinitializeSeparator = new QAction(m_DataHierarchyView);
    reinitializeSeparator->setSeparator(true);
    m_DataHierarchyView->addAction(reinitializeSeparator);

    m_ReinitializeSelectedDataAction =
        new QAction(QStringLiteral("Reinitialize Node"), m_DataHierarchyView);
    m_ReinitializeSelectedDataAction->setObjectName(
        QStringLiteral("xqReinitializeSelectedDataAction"));
    m_ReinitializeSelectedDataAction->setEnabled(false);
    m_DataHierarchyView->addAction(m_ReinitializeSelectedDataAction);

    m_GlobalReinitializeDataAction =
        new QAction(QStringLiteral("Global Reinit"), m_DataHierarchyView);
    m_GlobalReinitializeDataAction->setObjectName(
        QStringLiteral("xqGlobalReinitializeDataAction"));
    m_DataHierarchyView->addAction(m_GlobalReinitializeDataAction);

    auto* representationSeparator = new QAction(m_DataHierarchyView);
    representationSeparator->setSeparator(true);
    m_DataHierarchyView->addAction(representationSeparator);

    m_SurfaceRepresentationAction =
        new QAction(QStringLiteral("Representation: Surface"),
                    m_DataHierarchyView);
    m_SurfaceRepresentationAction->setObjectName(
        QStringLiteral("xqSetDataRepresentationSurfaceAction"));
    m_SurfaceRepresentationAction->setEnabled(false);
    m_DataHierarchyView->addAction(m_SurfaceRepresentationAction);

    m_WireframeRepresentationAction =
        new QAction(QStringLiteral("Representation: Wireframe"),
                    m_DataHierarchyView);
    m_WireframeRepresentationAction->setObjectName(
        QStringLiteral("xqSetDataRepresentationWireframeAction"));
    m_WireframeRepresentationAction->setEnabled(false);
    m_DataHierarchyView->addAction(m_WireframeRepresentationAction);

    m_PointsRepresentationAction =
        new QAction(QStringLiteral("Representation: Points"),
                    m_DataHierarchyView);
    m_PointsRepresentationAction->setObjectName(
        QStringLiteral("xqSetDataRepresentationPointsAction"));
    m_PointsRepresentationAction->setEnabled(false);
    m_DataHierarchyView->addAction(m_PointsRepresentationAction);

    auto* dataControlLayout = new QHBoxLayout();
    dataControlLayout->setContentsMargins(0, 0, 0, 0);
    dataControlLayout->setSpacing(6);
    auto* opacityLabel = new QLabel(QStringLiteral("Opacity:"), dataManagerPanel);
    m_DataOpacitySlider = new QSlider(Qt::Horizontal, dataManagerPanel);
    m_DataOpacitySlider->setObjectName(QStringLiteral("xqDataOpacitySlider"));
    m_DataOpacitySlider->setRange(0, 100);
    m_DataOpacitySlider->setValue(100);
    m_DataOpacityValueLabel = new QLabel(QStringLiteral("100%"), dataManagerPanel);
    m_DataOpacityValueLabel->setObjectName(
        QStringLiteral("xqDataOpacityValueLabel"));
    m_DataOpacityValueLabel->setMinimumWidth(40);
    m_DataOpacityValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_DataColorButton = new QPushButton(QStringLiteral("Color"), dataManagerPanel);
    m_DataColorButton->setObjectName(QStringLiteral("xqDataColorButton"));
    m_DataColorButton->setMaximumWidth(60);
    dataControlLayout->addWidget(opacityLabel);
    dataControlLayout->addWidget(m_DataOpacitySlider, 1);
    dataControlLayout->addWidget(m_DataOpacityValueLabel);
    dataControlLayout->addWidget(m_DataColorButton);
    dataManagerLayout->addLayout(dataControlLayout);

    auto* propertiesToggle =
        new QPushButton(QStringLiteral("Properties"), dataManagerPanel);
    propertiesToggle->setObjectName(QStringLiteral("xqDataPropertiesToggle"));
    propertiesToggle->setFlat(true);
    propertiesToggle->setCheckable(true);
    dataManagerLayout->addWidget(propertiesToggle);

    m_DataPropertiesTable = new QTableWidget(dataManagerPanel);
    m_DataPropertiesTable->setObjectName(QStringLiteral("xqDataPropertiesTable"));
    m_DataPropertiesTable->setColumnCount(2);
    m_DataPropertiesTable->setHorizontalHeaderLabels(
        {QStringLiteral("Property"), QStringLiteral("Value")});
    m_DataPropertiesTable->setAlternatingRowColors(true);
    m_DataPropertiesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_DataPropertiesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_DataPropertiesTable->setMaximumHeight(200);
    m_DataPropertiesTable->setVisible(false);
    m_DataPropertiesTable->horizontalHeader()->setStretchLastSection(true);
    dataManagerLayout->addWidget(m_DataPropertiesTable);

    connect(dataSearchBox,
            &QLineEdit::textChanged,
            this,
            [this](const QString& text) {
                ApplyDataManagerSearch(text);
            });
    connect(m_DataOpacitySlider,
            &QSlider::valueChanged,
            this,
            [this](int value) {
                ApplySelectedDataOpacity(value);
            });
    connect(m_RenameDataAction,
            &QAction::triggered,
            this,
            [this]() {
                RenameSelectedData();
            });
    connect(m_ToggleDataVisibilityAction,
            &QAction::triggered,
            this,
            [this]() {
                ToggleSelectedDataVisibility();
            });
    connect(m_ShowOnlySelectedDataAction,
            &QAction::triggered,
            this,
            [this]() {
                ShowOnlySelectedData();
            });
    connect(makeAllVisibleAction,
            &QAction::triggered,
            this,
            [this]() {
                SetAllDataVisibility(true);
            });
    connect(makeAllInvisibleAction,
            &QAction::triggered,
            this,
            [this]() {
                SetAllDataVisibility(false);
            });
    connect(m_RemoveSelectedDataAction,
            &QAction::triggered,
            this,
            [this]() {
                RemoveSelectedData();
            });
    connect(m_ReinitializeSelectedDataAction,
            &QAction::triggered,
            this,
            [this]() {
                ReinitializeSelectedData();
            });
    connect(m_GlobalReinitializeDataAction,
            &QAction::triggered,
            this,
            [this]() {
                GlobalReinitializeData();
            });
    connect(m_SurfaceRepresentationAction,
            &QAction::triggered,
            this,
            [this]() {
                SetSelectedDataRepresentation(2, false, true);
            });
    connect(m_WireframeRepresentationAction,
            &QAction::triggered,
            this,
            [this]() {
                SetSelectedDataRepresentation(1, true, false);
            });
    connect(m_PointsRepresentationAction,
            &QAction::triggered,
            this,
            [this]() {
                SetSelectedDataRepresentation(0, false, false);
            });
    connect(propertiesToggle,
            &QPushButton::toggled,
            this,
            [this](bool visible) {
                if (m_DataPropertiesTable)
                    m_DataPropertiesTable->setVisible(visible);
                if (visible)
                    UpdateDataManagerPropertiesTable();
            });

    dataManagerDock->setWidget(dataManagerPanel);
    addDockWidget(Qt::LeftDockWidgetArea, dataManagerDock);

    auto* imageNavigatorDock =
        new QDockWidget(QStringLiteral("Image Navigator"), this);
    imageNavigatorDock->setObjectName(QStringLiteral("xqImageNavigatorDock"));
    imageNavigatorDock->setAllowedAreas(Qt::LeftDockWidgetArea |
                                        Qt::RightDockWidgetArea);
    m_ImageNavigatorDock = imageNavigatorDock;
    auto* imageNavigatorPlaceholder = new QWidget(imageNavigatorDock);
    imageNavigatorPlaceholder->setObjectName(
        QStringLiteral("xqImageNavigatorPlaceholder"));
    imageNavigatorDock->setWidget(imageNavigatorPlaceholder);
    addDockWidget(Qt::LeftDockWidgetArea, imageNavigatorDock);
    splitDockWidget(dataManagerDock,
                    imageNavigatorDock,
                    Qt::Vertical);

    auto* workflowDock =
        new QDockWidget(QStringLiteral("Tools"), this);
    workflowDock->setObjectName(QStringLiteral("xqWorkflowToolsDock"));
    workflowDock->setAllowedAreas(Qt::RightDockWidgetArea |
                                  Qt::LeftDockWidgetArea);
    m_WorkflowToolsDock = workflowDock;

    auto* workflowPanel = new QWidget(workflowDock);
    workflowPanel->setObjectName(QStringLiteral("xqWorkflowToolsPanel"));
    workflowPanel->setMinimumWidth(360);
    auto* workflowLayout = new QVBoxLayout(workflowPanel);
    workflowLayout->setContentsMargins(0, 0, 0, 0);
    workflowLayout->setSpacing(0);

    m_Navigation = new QListWidget(workflowPanel);
    m_Navigation->setObjectName(QStringLiteral("xqWorkflowNavigation"));
    m_Navigation->setVisible(false);

    m_Pages = new QStackedWidget(workflowPanel);
    m_Pages->setObjectName(QStringLiteral("xqWorkflowPages"));
    m_Pages->setMinimumWidth(340);
    workflowLayout->addWidget(m_Pages);

    workflowDock->setWidget(workflowPanel);
    addDockWidget(Qt::RightDockWidgetArea, workflowDock);
    resizeDocks({workflowDock}, {380}, Qt::Horizontal);

    auto addDockToggleAction = [viewMenu](QDockWidget* dock,
                                          const QString& objectName) {
        auto* action = dock->toggleViewAction();
        action->setObjectName(objectName);
        viewMenu->addAction(action);
        return action;
    };
    addDockToggleAction(m_DataManagerDock,
                        QStringLiteral("xqToggleDataManagerDockAction"));
    addDockToggleAction(m_ImageNavigatorDock,
                        QStringLiteral("xqToggleImageNavigatorDockAction"));
    addDockToggleAction(m_WorkflowToolsDock,
                        QStringLiteral("xqToggleWorkflowToolsDockAction"));

    for (const auto& workflow : xq::core::DefaultWorkflowRegistry())
        AddWorkflowPage(workflow.Id, workflow.Title);

    connect(m_Navigation, &QListWidget::currentRowChanged,
            this,
            [this](int row) {
                if (row < 0)
                    return;

                m_Pages->setCurrentIndex(row);
                auto* item = m_Navigation->item(row);
                if (!item)
                    return;

                m_Context.WorkflowSelection()->SelectWorkflow(
                    item->data(Qt::UserRole).toString());
            });
    connect(m_Context.WorkflowSelection(),
            &xq::core::WorkflowSelectionService::WorkflowChanged,
            this,
            [this](const QString& workflowId) {
                SyncWorkflowNavigationFromCore(workflowId);
                UpdateWorkflowToolbarSelection(workflowId);
            });
    connect(m_Context.WorkflowContext(),
            &xq::core::WorkflowContextService::ContextChanged,
            this,
            [this]() {
                UpdateWorkflowContextStatusPage();
            });
    connect(m_Context.WorkflowOperations(),
            &xq::core::WorkflowOperationService::SelectedOperationChanged,
            this,
            [this](const QString&, const QString&) {
                UpdateWorkflowOperationControls();
            });
    connect(m_Context.WorkflowOperations(),
            &xq::core::WorkflowOperationService::ParameterValueChanged,
            this,
            [this](const QString& workflowId,
                   const QString&,
                   const QString& parameterId,
                   const QVariant& value) {
                UpdateWorkflowParameterEditorValue(workflowId,
                                                   parameterId,
                                                   value);
            });
    SyncWorkflowNavigationFromCore(
        m_Context.WorkflowSelection()->SelectedWorkflowId());
    UpdateWorkflowToolbarSelection(
        m_Context.WorkflowSelection()->SelectedWorkflowId());
    UpdateWorkflowContextStatusPage();
    UpdateWorkflowOperationControls();

    connect(m_DataHierarchyView->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            [this](const QModelIndex& current, const QModelIndex&) {
                if (m_SyncingSelectionFromCore)
                    return;

                const QString nodeId =
                    current.data(DataHierarchyModel::NodeIdRole).toString();
                const auto* node = m_Context.DataHierarchy()->FindNode(nodeId);
                if (!node ||
                    node->Kind != xq::core::DataHierarchyNodeKind::DataEntry)
                {
                    return;
                }

                QString errorMessage;
                if (!m_Context.DataSelection()->SelectHierarchyNode(
                        nodeId,
                        &errorMessage))
                {
                    m_Context.PostDiagnostic(errorMessage);
                }
            });
    connect(m_RemoveDataAction,
            &QAction::triggered,
            this,
            [this]() {
                RemoveSelectedData();
            });
    connect(m_ImportDataAction,
            &QAction::triggered,
            this,
            [this]() {
                ImportData();
            });
    connect(m_SaveProjectAction,
            &QAction::triggered,
            this,
            [this]() {
                SaveProject();
            });

    connect(m_Context.DataSelection(),
            &xq::core::DataSelectionService::SelectionChanged,
            this,
            [this](const QString& hierarchyNodeId, const QString&) {
                SyncTreeSelectionFromCore(hierarchyNodeId);
                UpdateDataWorkflowPage();
                UpdateDataManagerSelection();
                UpdateDataActions();
            });

    UpdateDataActions();

    connect(m_Context.DataCatalog(),
            &xq::core::DataCatalogService::EntriesChanged,
            this,
            [this]() {
                UpdateDataWorkflowPage();
                UpdateDataManagerSelection();
                UpdateDataActions();
                UpdateProjectPageDataCount();
                UpdateProjectStructureTree();
            });
    connect(m_Context.DataHierarchy(),
            &xq::core::DataHierarchyService::NodesChanged,
            this,
            [this]() {
                UpdateProjectStructureTree();
            });
    connect(m_Context.DataNodes(),
            &xq::core::DataNodeRegistryService::BindingsChanged,
            this,
            [this]() {
                UpdateDataManagerSelection();
                UpdateDataActions();
            });

    connect(m_Context.Projects(),
            &xq::core::ProjectService::ProjectChanged,
            this,
            [this](const xq::core::ProjectMetadata& project) {
                UpdateProjectPage(&project);
                UpdateProjectWindowState(project);
                UpdateProjectActions();
                UpdateProjectStructureTree();
            });
    if (const auto* project = m_Context.Projects()->CurrentProject())
    {
        UpdateDataWorkflowPage();
        UpdateProjectPage(project);
        UpdateProjectWindowState(*project);
    }
    else
    {
        UpdateDataWorkflowPage();
        UpdateProjectPage(nullptr);
    }
    UpdateDataManagerSelection();
    UpdateProjectActions();

    m_Diagnostics = new QTextEdit(this);
    m_Diagnostics->setObjectName(QStringLiteral("xqDiagnosticsLog"));
    m_Diagnostics->setReadOnly(true);
    auto* diagnosticsDock = new QDockWidget(QStringLiteral("Diagnostics"), this);
    diagnosticsDock->setObjectName(QStringLiteral("xqDiagnosticsDock"));
    diagnosticsDock->setWidget(m_Diagnostics);
    m_DiagnosticsDock = diagnosticsDock;
    addDockWidget(Qt::BottomDockWidgetArea, diagnosticsDock);
    viewMenu->addSeparator();
    addDockToggleAction(m_DiagnosticsDock,
                        QStringLiteral("xqToggleDiagnosticsDockAction"));

    connect(&m_Context, &xq::core::ApplicationContext::DiagnosticPosted,
            this, [this](const QString& message) {
                m_Diagnostics->append(message);
            });

    m_TaskHistoryTable = new QTableWidget(this);
    m_TaskHistoryTable->setObjectName(QStringLiteral("xqTaskHistoryTable"));
    m_TaskHistoryTable->setColumnCount(3);
    m_TaskHistoryTable->setHorizontalHeaderLabels(
        {QStringLiteral("Task"),
         QStringLiteral("Status"),
         QStringLiteral("Message")});
    m_TaskHistoryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_TaskHistoryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_TaskHistoryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_TaskHistoryTable->horizontalHeader()->setStretchLastSection(true);

    auto* taskHistoryDock =
        new QDockWidget(QStringLiteral("Task History"), this);
    taskHistoryDock->setObjectName(QStringLiteral("xqTaskHistoryDock"));
    taskHistoryDock->setWidget(m_TaskHistoryTable);
    m_TaskHistoryDock = taskHistoryDock;
    addDockWidget(Qt::BottomDockWidgetArea, taskHistoryDock);
    addDockToggleAction(m_TaskHistoryDock,
                        QStringLiteral("xqToggleTaskHistoryDockAction"));

    for (const auto& task : m_Context.Tasks()->History())
        AppendTaskHistoryRow(task);
    connect(m_Context.Tasks(),
            &xq::core::TaskRunner::TaskFinished,
            this,
            [this](const QString& taskName,
                   bool succeeded,
                   const QString& message) {
                xq::core::TaskRecord task;
                task.Name = taskName;
                task.Succeeded = succeeded;
                task.Message = message;
                AppendTaskHistoryRow(task);
            });
}

void MainWindow::SetDataImportCommand(xq::core::DataImportCommand* command)
{
    m_DataImportCommand = command;
}

void MainWindow::SetDicomImportCommand(xq::core::DataImportCommand* command)
{
    m_DicomImportCommand = command;
}

void MainWindow::SetProjectFilePathProvider(
    xq::core::ProjectFilePathProvider* provider)
{
    m_ProjectFilePathProvider = provider;
}

void MainWindow::SetSceneFilePathProvider(
    xq::core::SceneFilePathProvider* provider)
{
    m_SceneFilePathProvider = provider;
}

void MainWindow::SetSceneExportService(
    xq::core::SceneExportService* service)
{
    m_SceneExportService = service;
}

void MainWindow::SetScreenshotFilePathProvider(
    xq::core::ScreenshotFilePathProvider* provider)
{
    m_ScreenshotFilePathProvider = provider;
}

void MainWindow::SetMeasurementService(
    xq::core::MeasurementService* service)
{
    m_MeasurementService = service;
    UpdateDataActions();
}

void MainWindow::SetRenderHost(QWidget* renderHost)
{
    if (!renderHost || !m_RenderHostContainer)
        return;

    auto* layout = qobject_cast<QVBoxLayout*>(m_RenderHostContainer->layout());
    if (!layout)
        return;

    if (m_RenderHost)
    {
        layout->removeWidget(m_RenderHost);
        m_RenderHost->deleteLater();
    }

    m_RenderHost = renderHost;
    m_RenderHost->setParent(m_RenderHostContainer);
    layout->addWidget(m_RenderHost);
}

void MainWindow::SetImageNavigatorWidget(QWidget* imageNavigator)
{
    if (!imageNavigator || !m_ImageNavigatorDock)
        return;

    if (auto* currentWidget = m_ImageNavigatorDock->widget())
    {
        currentWidget->setParent(nullptr);
        currentWidget->deleteLater();
    }

    imageNavigator->setParent(m_ImageNavigatorDock);
    m_ImageNavigatorDock->setWidget(imageNavigator);
}

void MainWindow::AppendTaskHistoryRow(const xq::core::TaskRecord& task)
{
    if (!m_TaskHistoryTable)
        return;

    const int row = m_TaskHistoryTable->rowCount();
    m_TaskHistoryTable->insertRow(row);
    m_TaskHistoryTable->setItem(row, 0, new QTableWidgetItem(task.Name));
    m_TaskHistoryTable->setItem(
        row,
        1,
        new QTableWidgetItem(task.Succeeded ? QStringLiteral("Succeeded")
                                            : QStringLiteral("Failed")));
    m_TaskHistoryTable->setItem(row, 2, new QTableWidgetItem(task.Message));
    m_TaskHistoryTable->scrollToBottom();
}

void MainWindow::UpdateDataWorkflowPage()
{
    if (!m_DataSelectionLabel || !m_DataCatalogIdLabel ||
        !m_DataDisplayNameLabel || !m_DataSourcePathLabel ||
        !m_DataWorkflowRoleLabel)
    {
        return;
    }

    const QString catalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    const auto* entry = m_Context.DataCatalog()->FindById(catalogEntryId);
    if (!entry)
    {
        m_DataSelectionLabel->setText(QStringLiteral("No data selected"));
        m_DataCatalogIdLabel->clear();
        m_DataDisplayNameLabel->clear();
        m_DataSourcePathLabel->clear();
        m_DataWorkflowRoleLabel->clear();
        UpdateDataManagerSelection();
        return;
    }

    m_DataSelectionLabel->setText(
        QStringLiteral("Selected data: %1").arg(entry->DisplayName));
    m_DataCatalogIdLabel->setText(QStringLiteral("ID: %1").arg(entry->Id));
    m_DataDisplayNameLabel->setText(
        QStringLiteral("Name: %1").arg(entry->DisplayName));
    m_DataSourcePathLabel->setText(
        QStringLiteral("Source: %1").arg(entry->SourcePath));
    m_DataWorkflowRoleLabel->setText(
        QStringLiteral("Role: %1").arg(RoleDisplayName(entry->WorkflowRole)));
    UpdateDataManagerSelection();
}

void MainWindow::SaveProject()
{
    if (!m_Context.Projects()->HasActiveProject())
    {
        m_Context.PostDiagnostic(QStringLiteral("No active project to save."));
        UpdateProjectActions();
        return;
    }

    QString errorMessage;
    if (!m_Context.ProjectSession()->Save(&errorMessage))
    {
        m_Context.PostDiagnostic(
            errorMessage.trimmed().isEmpty()
                ? QStringLiteral("Unable to save project.")
                : errorMessage);
        UpdateProjectActions();
        return;
    }

    m_Context.PostDiagnostic(QStringLiteral("Project saved."));
    UpdateProjectActions();
}

void MainWindow::SaveProjectAsFromProvider()
{
    const auto* currentProject = m_Context.Projects()->CurrentProject();
    if (!currentProject)
    {
        m_Context.PostDiagnostic(QStringLiteral(
            "Save As failed: No active project to save."));
        UpdateProjectActions();
        return;
    }

    if (!m_ProjectFilePathProvider)
    {
        m_Context.PostDiagnostic(QStringLiteral(
            "Save As failed: No project file path provider is configured."));
        return;
    }

    const auto projectFile =
        m_ProjectFilePathProvider->SaveAsProjectFilePath(*currentProject);
    if (projectFile.ProjectFilePath.trimmed().isEmpty() ||
        projectFile.ProjectName.trimmed().isEmpty())
    {
        return;
    }

    QString errorMessage;
    if (!m_Context.ProjectSession()->SaveAs(projectFile.ProjectName,
                                            projectFile.ProjectFilePath,
                                            &errorMessage))
    {
        m_Context.PostDiagnostic(
            QStringLiteral("Save As failed: %1").arg(errorMessage));
        UpdateProjectActions();
        return;
    }

    m_Context.PostDiagnostic(QStringLiteral("Project saved as %1.")
                                 .arg(projectFile.ProjectFilePath));
    UpdateProjectActions();
    UpdateProjectPage(m_Context.Projects()->CurrentProject());
    UpdateProjectPageDataCount();
    UpdateProjectStructureTree();
}

void MainWindow::CaptureScreenshot()
{
    if (!m_ScreenshotFilePathProvider)
    {
        m_Context.PostDiagnostic(QStringLiteral(
            "Screenshot failed: No screenshot file path provider is configured."));
        return;
    }

    QString filePath = m_ScreenshotFilePathProvider->ScreenshotFilePath()
                           .trimmed();
    if (filePath.isEmpty())
        return;

    if (!filePath.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive))
        filePath += QStringLiteral(".png");

    const QPixmap screenshot = grab();
    if (screenshot.isNull())
    {
        m_Context.PostDiagnostic(
            QStringLiteral("Screenshot failed: Unable to capture the window."));
        return;
    }

    if (!screenshot.save(filePath, "PNG"))
    {
        m_Context.PostDiagnostic(
            QStringLiteral("Screenshot failed: Unable to write %1.")
                .arg(filePath));
        return;
    }

    m_Context.PostDiagnostic(QStringLiteral("Screenshot saved."));
}

void MainWindow::SaveMitkScene()
{
    if (!m_SceneFilePathProvider)
    {
        m_Context.PostDiagnostic(QStringLiteral(
            "MITK scene export failed: No scene file path provider is configured."));
        return;
    }
    if (!m_SceneExportService)
    {
        m_Context.PostDiagnostic(QStringLiteral(
            "MITK scene export failed: No scene export service is configured."));
        return;
    }

    QString filePath = m_SceneFilePathProvider->SceneFilePath().trimmed();
    if (filePath.isEmpty())
        return;

    if (!filePath.endsWith(QStringLiteral(".mitk"), Qt::CaseInsensitive))
        filePath += QStringLiteral(".mitk");

    QString errorMessage;
    if (!m_SceneExportService->SaveScene(m_Context.DataStorage(),
                                         filePath,
                                         &errorMessage))
    {
        m_Context.PostDiagnostic(
            errorMessage.trimmed().isEmpty()
                ? QStringLiteral("MITK scene export failed.")
                : QStringLiteral("MITK scene export failed: %1")
                      .arg(errorMessage));
        return;
    }

    m_Context.PostDiagnostic(QStringLiteral("MITK scene saved."));
}

void MainWindow::CloseWorkspace()
{
    QString errorMessage;
    const bool closed = m_Context.ProjectSession()->Close(&errorMessage);
    if (!errorMessage.trimmed().isEmpty())
        m_Context.PostDiagnostic(errorMessage);
    if (!closed)
    {
        UpdateProjectActions();
        return;
    }

    setWindowTitle(QStringLiteral("XQ"));
    statusBar()->showMessage(QStringLiteral("No project"));
    UpdateProjectActions();
    UpdateProjectPage(nullptr);
    UpdateProjectPageDataCount();
    UpdateProjectStructureTree();
    UpdateDataWorkflowPage();
    UpdateDataManagerSelection();
    UpdateDataActions();
    if (auto* renderingManager = mitk::RenderingManager::GetInstance())
        renderingManager->RequestUpdateAll();
}

void MainWindow::SyncWorkflowNavigationFromCore(const QString& workflowId)
{
    if (!m_Navigation || !m_Pages)
        return;

    for (int row = 0; row < m_Navigation->count(); ++row)
    {
        auto* item = m_Navigation->item(row);
        if (!item)
            continue;

        if (item->data(Qt::UserRole).toString() != workflowId)
            continue;

        m_Navigation->setCurrentRow(row);
        m_Pages->setCurrentIndex(row);
        return;
    }
}

void MainWindow::UpdateWorkflowToolbarSelection(const QString& workflowId)
{
    for (auto it = m_WorkflowToolbarActions.begin();
         it != m_WorkflowToolbarActions.end();
         ++it)
    {
        it.value()->setChecked(it.key() == workflowId);
    }
}

void MainWindow::UpdateWorkflowContextStatusPage()
{
    const xq::core::WorkflowContextSnapshot snapshot =
        m_Context.WorkflowContext()->Snapshot();
    auto* label = m_WorkflowContextStatusLabels.value(snapshot.WorkflowId,
                                                      nullptr);
    for (auto* actionButton : std::as_const(m_WorkflowPrimaryActionButtons))
    {
        if (actionButton)
            actionButton->setEnabled(false);
    }

    auto* button = m_WorkflowPrimaryActionButtons.value(snapshot.WorkflowId,
                                                       nullptr);
    if (button)
        button->setEnabled(snapshot.HasCompatibleSelection);

    if (!label)
        return;

    if (!snapshot.RequiresSelectedData)
    {
        label->setText(QStringLiteral("Ready."));
        return;
    }

    if (!snapshot.HasSelectedData)
    {
        label->setText(QStringLiteral("Select compatible data to continue."));
        return;
    }

    if (!snapshot.HasCompatibleSelection)
    {
        label->setText(
            QStringLiteral("Selected data is not compatible with %1.")
                .arg(snapshot.WorkflowTitle));
        return;
    }

    const QString displayName =
        snapshot.SelectedDataDisplayName.trimmed().isEmpty()
            ? snapshot.SelectedCatalogEntryId
            : snapshot.SelectedDataDisplayName;
    label->setText(QStringLiteral("Using %1.").arg(displayName));
}

void MainWindow::UpdateWorkflowOperationControls()
{
    for (auto it = m_WorkflowPrimaryActionButtons.begin();
         it != m_WorkflowPrimaryActionButtons.end();
         ++it)
    {
        const QString workflowId = it.key();
        auto* button = it.value();
        if (!button)
            continue;

        const auto operations =
            m_Context.WorkflowOperations()->OperationsForWorkflow(workflowId);
        if (operations.isEmpty())
        {
            button->setText(QStringLiteral("Run"));
            continue;
        }

        QString selectedOperationId =
            m_Context.WorkflowOperations()->SelectedOperationId(workflowId);
        QString operationTitle;
        for (const auto& operation : operations)
        {
            if (operation.Id == selectedOperationId)
            {
                operationTitle = operation.Title;
                break;
            }
        }
        if (operationTitle.trimmed().isEmpty())
            operationTitle = operations.front().Title;

        button->setText(QStringLiteral("Run %1").arg(operationTitle));
        RebuildWorkflowParameterPanel(workflowId);
    }

    for (auto it = m_WorkflowOperationSelectors.begin();
         it != m_WorkflowOperationSelectors.end();
         ++it)
    {
        auto* selector = it.value();
        if (!selector)
            continue;

        const QString selectedOperationId =
            m_Context.WorkflowOperations()->SelectedOperationId(it.key());
        const int index = selector->findData(selectedOperationId);
        if (index >= 0 && selector->currentIndex() != index)
        {
            QSignalBlocker blocker(selector);
            selector->setCurrentIndex(index);
        }
    }

    UpdateFlowSimulationToolButtons();
    UpdateModelingToolButtons();
    UpdateMeshingToolButtons();
    UpdateMultiPhysicsToolButtons();
    UpdatePathToolButtons();
    UpdateRomSimulationToolButtons();
    UpdateSegmentation2DToolButtons();
    UpdateSegmentation3DToolButtons();
}

void MainWindow::UpdateFlowSimulationToolButtons()
{
    const QString selectedOperationId =
        m_Context.WorkflowOperations()->SelectedOperationId(
            QStringLiteral("flow-simulation"));
    const QStringList buttonObjectNames = {
        QStringLiteral("xqFlowConfigureJobButton"),
        QStringLiteral("xqFlowSteadyFlowButton"),
        QStringLiteral("xqFlowReviewResultsButton"),
    };

    for (const auto& objectName : buttonObjectNames)
    {
        auto* button = findChild<QPushButton*>(objectName);
        if (!button)
            continue;

        QSignalBlocker blocker(button);
        button->setChecked(
            button->property("xqOperationId").toString() ==
            selectedOperationId);
    }
}

void MainWindow::UpdateMeshingToolButtons()
{
    const QString selectedOperationId =
        m_Context.WorkflowOperations()->SelectedOperationId(
            QStringLiteral("meshing"));
    const QStringList buttonObjectNames = {
        QStringLiteral("xqMeshingGenerateSurfaceMeshButton"),
        QStringLiteral("xqMeshingGenerateVolumeMeshButton"),
        QStringLiteral("xqMeshingBoundaryLayersButton"),
    };

    for (const auto& objectName : buttonObjectNames)
    {
        auto* button = findChild<QPushButton*>(objectName);
        if (!button)
            continue;

        QSignalBlocker blocker(button);
        button->setChecked(
            button->property("xqOperationId").toString() ==
            selectedOperationId);
    }
}

void MainWindow::UpdateMultiPhysicsToolButtons()
{
    const QString selectedOperationId =
        m_Context.WorkflowOperations()->SelectedOperationId(
            QStringLiteral("multiphysics"));
    const QStringList buttonObjectNames = {
        QStringLiteral("xqMultiPhysicsConfigureCouplingButton"),
        QStringLiteral("xqMultiPhysicsReviewResultsButton"),
    };

    for (const auto& objectName : buttonObjectNames)
    {
        auto* button = findChild<QPushButton*>(objectName);
        if (!button)
            continue;

        QSignalBlocker blocker(button);
        button->setChecked(
            button->property("xqOperationId").toString() ==
            selectedOperationId);
    }
}

void MainWindow::UpdateRomSimulationToolButtons()
{
    const QString selectedOperationId =
        m_Context.WorkflowOperations()->SelectedOperationId(
            QStringLiteral("rom-simulation"));
    const QStringList buttonObjectNames = {
        QStringLiteral("xqRomBuildNetworkButton"),
        QStringLiteral("xqRomCalibrateBoundaryButton"),
    };

    for (const auto& objectName : buttonObjectNames)
    {
        auto* button = findChild<QPushButton*>(objectName);
        if (!button)
            continue;

        QSignalBlocker blocker(button);
        button->setChecked(
            button->property("xqOperationId").toString() ==
            selectedOperationId);
    }
}

void MainWindow::UpdateModelingToolButtons()
{
    const QString selectedOperationId =
        m_Context.WorkflowOperations()->SelectedOperationId(
            QStringLiteral("modeling"));
    const QStringList buttonObjectNames = {
        QStringLiteral("xqModelingLoftSurfaceButton"),
        QStringLiteral("xqModelingBuildSolidModelButton"),
        QStringLiteral("xqModelingTrimBranchesButton"),
    };

    for (const auto& objectName : buttonObjectNames)
    {
        auto* button = findChild<QPushButton*>(objectName);
        if (!button)
            continue;

        QSignalBlocker blocker(button);
        button->setChecked(
            button->property("xqOperationId").toString() ==
            selectedOperationId);
    }
}

void MainWindow::UpdatePathToolButtons()
{
    const QString selectedOperationId =
        m_Context.WorkflowOperations()->SelectedOperationId(
            QStringLiteral("path"));
    const QStringList buttonObjectNames = {
        QStringLiteral("xqPathPlanningAddPathButton"),
        QStringLiteral("xqPathPlanningEditControlPointsButton"),
        QStringLiteral("xqPathPlanningSmoothPathButton"),
    };

    for (const auto& objectName : buttonObjectNames)
    {
        auto* button = findChild<QPushButton*>(objectName);
        if (!button)
            continue;

        QSignalBlocker blocker(button);
        button->setChecked(
            button->property("xqOperationId").toString() ==
            selectedOperationId);
    }
}

void MainWindow::UpdateSegmentation2DToolButtons()
{
    const QString selectedOperationId =
        m_Context.WorkflowOperations()->SelectedOperationId(
            QStringLiteral("segmentation-2d"));
    const QStringList buttonObjectNames = {
        QStringLiteral("xqSegmentation2DThresholdContourButton"),
        QStringLiteral("xqSegmentation2DManualContourButton"),
        QStringLiteral("xqSegmentation2DLoftProfilesButton"),
    };

    for (const auto& objectName : buttonObjectNames)
    {
        auto* button = findChild<QPushButton*>(objectName);
        if (!button)
            continue;

        QSignalBlocker blocker(button);
        button->setChecked(
            button->property("xqOperationId").toString() ==
            selectedOperationId);
    }
}

void MainWindow::UpdateSegmentation3DToolButtons()
{
    const QString selectedOperationId =
        m_Context.WorkflowOperations()->SelectedOperationId(
            QStringLiteral("segmentation-3d"));
    const QStringList buttonObjectNames = {
        QStringLiteral("xqSegmentation3DThresholdButton"),
        QStringLiteral("xqSegmentation3DRegionGrowButton"),
        QStringLiteral("xqSegmentation3DSurfacePreviewButton"),
    };

    for (const auto& objectName : buttonObjectNames)
    {
        auto* button = findChild<QPushButton*>(objectName);
        if (!button)
            continue;

        QSignalBlocker blocker(button);
        button->setChecked(
            button->property("xqOperationId").toString() ==
            selectedOperationId);
    }
}

void MainWindow::RebuildWorkflowParameterPanel(const QString& workflowId)
{
    auto* panel = m_WorkflowParameterPanels.value(workflowId, nullptr);
    if (!panel)
        return;

    auto* formLayout = qobject_cast<QFormLayout*>(panel->layout());
    if (!formLayout)
        return;

    while (formLayout->rowCount() > 0)
        formLayout->removeRow(0);

    const auto operations =
        m_Context.WorkflowOperations()->OperationsForWorkflow(workflowId);
    const QString selectedOperationId =
        m_Context.WorkflowOperations()->SelectedOperationId(workflowId);
    const xq::core::WorkflowOperationDescriptor* selectedOperation = nullptr;
    for (const auto& operation : operations)
    {
        if (operation.Id == selectedOperationId)
        {
            selectedOperation = &operation;
            break;
        }
    }
    if (!selectedOperation && !operations.isEmpty())
        selectedOperation = &operations.front();
    if (!selectedOperation)
        return;

    for (const auto& parameter : selectedOperation->Parameters)
    {
        QWidget* editor = nullptr;
        const QVariantMap values =
            m_Context.WorkflowOperations()->ParameterValues(
                workflowId,
                selectedOperation->Id);
        const QVariant value = values.value(parameter.Id);
        switch (parameter.Type)
        {
        case xq::core::WorkflowOperationParameterValueType::NumericScalar:
        {
            auto* spinBox = new QDoubleSpinBox(panel);
            if (workflowId == QStringLiteral("image-preprocessing"))
            {
                spinBox->setObjectName(
                    QStringLiteral("xqImagePreprocessingParameter_%1")
                        .arg(parameter.Id));
            }
            else
            {
                spinBox->setObjectName(
                    QStringLiteral("xqWorkflowParameter_%1")
                        .arg(parameter.Id));
            }
            spinBox->setDecimals(3);
            spinBox->setRange(-1000000.0, 1000000.0);
            spinBox->setValue(value.toDouble());
            connect(spinBox,
                    qOverload<double>(&QDoubleSpinBox::valueChanged),
                    this,
                    [this, workflowId, operationId = selectedOperation->Id,
                     parameterId = parameter.Id](double newValue) {
                        QString message;
                        if (!m_Context.WorkflowOperations()->SetParameterValue(
                                workflowId,
                                operationId,
                                parameterId,
                                newValue,
                                &message))
                        {
                            m_Context.PostDiagnostic(message);
                        }
                    });
            editor = spinBox;
            break;
        }
        case xq::core::WorkflowOperationParameterValueType::IntegerScalar:
        {
            auto* spinBox = new QSpinBox(panel);
            if (workflowId == QStringLiteral("image-preprocessing"))
            {
                spinBox->setObjectName(
                    QStringLiteral("xqImagePreprocessingParameter_%1")
                        .arg(parameter.Id));
            }
            else
            {
                spinBox->setObjectName(
                    QStringLiteral("xqWorkflowParameter_%1")
                        .arg(parameter.Id));
            }
            spinBox->setRange(-1000000, 1000000);
            spinBox->setValue(value.toInt());
            connect(spinBox,
                    qOverload<int>(&QSpinBox::valueChanged),
                    this,
                    [this, workflowId, operationId = selectedOperation->Id,
                     parameterId = parameter.Id](int newValue) {
                        QString message;
                        if (!m_Context.WorkflowOperations()->SetParameterValue(
                                workflowId,
                                operationId,
                                parameterId,
                                newValue,
                                &message))
                        {
                            m_Context.PostDiagnostic(message);
                        }
                    });
            editor = spinBox;
            break;
        }
        case xq::core::WorkflowOperationParameterValueType::IntegerPointList:
        {
            auto* lineEdit = new QLineEdit(panel);
            if (workflowId == QStringLiteral("image-preprocessing"))
            {
                lineEdit->setObjectName(
                    QStringLiteral("xqImagePreprocessingParameter_%1")
                        .arg(parameter.Id));
            }
            else
            {
                lineEdit->setObjectName(
                    QStringLiteral("xqWorkflowParameter_%1")
                        .arg(parameter.Id));
            }
            lineEdit->setText(FormatIntegerPointList(value));
            connect(lineEdit,
                    &QLineEdit::editingFinished,
                    this,
                    [this, lineEdit, workflowId,
                     operationId = selectedOperation->Id,
                     parameterId = parameter.Id]() {
                        StoreWorkflowPointListParameter(workflowId,
                                                        operationId,
                                                        parameterId,
                                                        lineEdit->text());
                    });
            editor = lineEdit;
            break;
        }
        case xq::core::WorkflowOperationParameterValueType::Option:
        {
            auto* comboBox = new QComboBox(panel);
            if (workflowId == QStringLiteral("image-preprocessing"))
            {
                comboBox->setObjectName(
                    QStringLiteral("xqImagePreprocessingParameter_%1")
                        .arg(parameter.Id));
            }
            else
            {
                comboBox->setObjectName(
                    QStringLiteral("xqWorkflowParameter_%1")
                        .arg(parameter.Id));
            }
            for (const auto& option : parameter.Options)
                comboBox->addItem(option.Title, option.Id);
            const int index = comboBox->findData(value.toString());
            if (index >= 0)
                comboBox->setCurrentIndex(index);
            connect(comboBox,
                    &QComboBox::currentIndexChanged,
                    this,
                    [this, comboBox, workflowId,
                     operationId = selectedOperation->Id,
                     parameterId = parameter.Id](int) {
                        QString message;
                        if (!m_Context.WorkflowOperations()->SetParameterValue(
                                workflowId,
                                operationId,
                                parameterId,
                                comboBox->currentData().toString(),
                                &message))
                        {
                            m_Context.PostDiagnostic(message);
                        }
                    });
            editor = comboBox;
            break;
        }
        }

        formLayout->addRow(parameter.Title, editor);
    }
}

void MainWindow::UpdateWorkflowParameterEditorValue(const QString& workflowId,
                                                    const QString& parameterId,
                                                    const QVariant& value)
{
    auto* panel = m_WorkflowParameterPanels.value(workflowId, nullptr);
    if (!panel)
        return;

    QString objectName =
        QStringLiteral("xqWorkflowParameter_%1").arg(parameterId);
    if (workflowId == QStringLiteral("image-preprocessing"))
    {
        objectName = QStringLiteral("xqImagePreprocessingParameter_%1")
                         .arg(parameterId);
    }

    if (auto* doubleEditor = panel->findChild<QDoubleSpinBox*>(objectName))
    {
        QSignalBlocker blocker(doubleEditor);
        doubleEditor->setValue(value.toDouble());
        return;
    }

    if (auto* integerEditor = panel->findChild<QSpinBox*>(objectName))
    {
        QSignalBlocker blocker(integerEditor);
        integerEditor->setValue(value.toInt());
        return;
    }

    if (auto* optionEditor = panel->findChild<QComboBox*>(objectName))
    {
        const int index = optionEditor->findData(value.toString());
        if (index >= 0)
        {
            QSignalBlocker blocker(optionEditor);
            optionEditor->setCurrentIndex(index);
        }
        return;
    }

    if (auto* pointListEditor = panel->findChild<QLineEdit*>(objectName))
    {
        QSignalBlocker blocker(pointListEditor);
        pointListEditor->setText(FormatIntegerPointList(value));
    }
}

void MainWindow::StoreWorkflowPointListParameter(const QString& workflowId,
                                                 const QString& operationId,
                                                 const QString& parameterId,
                                                 const QString& text)
{
    QVariantList points;
    QString parseMessage;
    if (!ParseIntegerPointList(text, points, &parseMessage))
    {
        m_Context.PostDiagnostic(parseMessage);
        return;
    }

    QString message;
    if (!m_Context.WorkflowOperations()->SetParameterValue(workflowId,
                                                           operationId,
                                                           parameterId,
                                                           points,
                                                           &message))
    {
        m_Context.PostDiagnostic(message);
    }
}

void MainWindow::RunActiveWorkflowAction()
{
    QString message;
    if (!m_Context.WorkflowActions()->RunActiveWorkflowAction(&message))
        m_Context.PostDiagnostic(message);
}

void MainWindow::CreateProjectFromProvider()
{
    if (!m_ProjectFilePathProvider)
    {
        m_Context.PostDiagnostic(QStringLiteral(
            "No project file path provider is configured."));
        return;
    }

    const auto projectFile = m_ProjectFilePathProvider->NewProjectFilePath();
    if (projectFile.ProjectFilePath.trimmed().isEmpty() ||
        projectFile.ProjectName.trimmed().isEmpty())
    {
        return;
    }

    QString message;
    if (!m_Context.Projects()->CreateProject(projectFile.ProjectName,
                                             projectFile.ProjectFilePath,
                                             &message))
    {
        m_Context.PostDiagnostic(
            QStringLiteral("New Project failed: %1").arg(message));
        UpdateProjectActions();
        return;
    }

    UpdateProjectActions();
}

void MainWindow::OpenProjectFromProvider()
{
    if (!m_ProjectFilePathProvider)
    {
        m_Context.PostDiagnostic(QStringLiteral(
            "No project file path provider is configured."));
        return;
    }

    const QString projectFilePath =
        m_ProjectFilePathProvider->OpenProjectFilePath();
    if (projectFilePath.trimmed().isEmpty())
        return;

    QString message;
    if (!m_Context.ProjectSession()->Open(projectFilePath, &message))
    {
        m_Context.PostDiagnostic(
            QStringLiteral("Open Project failed: %1").arg(message));
        UpdateProjectActions();
        return;
    }

    UpdateDataWorkflowPage();
    UpdateProjectActions();
    UpdateProjectPageDataCount();
    UpdateProjectStructureTree();
}

void MainWindow::OpenPreferencesDialog()
{
    auto* existingDialog =
        findChild<PreferencesDialog*>(QStringLiteral("xqPreferencesDialog"));
    if (existingDialog)
    {
        existingDialog->raise();
        existingDialog->activateWindow();
        return;
    }

    auto* dialog = new PreferencesDialog(*m_Context.Preferences(), this);
    dialog->show();
}

void MainWindow::UpdateProjectPage(const xq::core::ProjectMetadata* project)
{
    if (!m_ProjectNameLabel || !m_ProjectPathLabel || !m_ProjectSchemaLabel)
        return;

    if (!project)
    {
        m_ProjectNameLabel->setText(QStringLiteral("No project loaded"));
        m_ProjectPathLabel->setText(QStringLiteral("No project file"));
        m_ProjectSchemaLabel->setText(QStringLiteral("Schema: -"));
        UpdateProjectPageDataCount();
        UpdateProjectStructureTree();
        return;
    }

    m_ProjectNameLabel->setText(project->Name);
    m_ProjectPathLabel->setText(project->ProjectFilePath);
    m_ProjectSchemaLabel->setText(
        QStringLiteral("Schema: %1").arg(project->SchemaVersion));
    UpdateProjectPageDataCount();
    UpdateProjectStructureTree();
}

void MainWindow::UpdateProjectPageDataCount()
{
    if (!m_ProjectDataCountLabel)
        return;

    m_ProjectDataCountLabel->setText(
        QStringLiteral("Data items: %1")
            .arg(m_Context.DataCatalog()->Entries().size()));
}

void MainWindow::UpdateProjectStructureTree()
{
    if (!m_ProjectStructureTree)
        return;

    m_ProjectStructureTree->clear();

    const auto* project = m_Context.Projects()->CurrentProject();
    if (!project)
    {
        auto* rootItem = new QTreeWidgetItem(m_ProjectStructureTree);
        rootItem->setText(0, QStringLiteral("(No project loaded)"));
        return;
    }

    auto* rootItem = new QTreeWidgetItem(m_ProjectStructureTree);
    rootItem->setText(0, project->Name);
    QFont rootFont = rootItem->font(0);
    rootFont.setBold(true);
    rootItem->setFont(0, rootFont);

    const auto rootChildren =
        m_Context.DataHierarchy()->ChildrenOf(m_Context.DataHierarchy()->RootId());
    for (const auto& folder : rootChildren)
    {
        if (folder.Kind != xq::core::DataHierarchyNodeKind::Folder)
            continue;

        const auto dataChildren =
            m_Context.DataHierarchy()->ChildrenOf(folder.Id);
        if (dataChildren.isEmpty())
            continue;

        auto* folderItem = new QTreeWidgetItem(rootItem);
        folderItem->setText(
            0,
            QStringLiteral("%1 [%2]").arg(folder.DisplayName).arg(
                dataChildren.size()));

        for (const auto& dataNode : dataChildren)
        {
            auto* dataItem = new QTreeWidgetItem(folderItem);
            dataItem->setText(0, dataNode.DisplayName);
        }
    }

    m_ProjectStructureTree->expandAll();
}

void MainWindow::UpdateProjectWindowState(
    const xq::core::ProjectMetadata& project)
{
    setWindowTitle(QStringLiteral("XQ - %1").arg(project.Name));
    statusBar()->showMessage(
        QStringLiteral("%1 | %2").arg(project.Name, project.ProjectFilePath));
}

void MainWindow::UpdateProjectActions()
{
    if (!m_SaveProjectAction)
        return;

    m_SaveProjectAction->setEnabled(m_Context.Projects()->HasActiveProject());
    const bool hasProject = m_Context.Projects()->HasActiveProject();
    if (m_ProjectRefreshButton)
        m_ProjectRefreshButton->setEnabled(hasProject);
    if (m_ProjectOpenFolderButton)
        m_ProjectOpenFolderButton->setEnabled(hasProject);
}

void MainWindow::ImportData()
{
    if (!m_DataImportCommand)
    {
        m_Context.PostDiagnostic(
            QStringLiteral("No data import command is configured."));
        return;
    }

    const auto result = m_DataImportCommand->RunImport(m_Context);
    if (!result.Message.trimmed().isEmpty())
        m_Context.PostDiagnostic(result.Message);

    UpdateDataWorkflowPage();
    UpdateProjectPageDataCount();
    UpdateDataActions();
}

void MainWindow::ImportDicomData()
{
    if (!m_DicomImportCommand)
    {
        m_Context.PostDiagnostic(
            QStringLiteral("No DICOM import command is configured."));
        return;
    }

    const auto result = m_DicomImportCommand->RunImport(m_Context);
    if (!result.Message.trimmed().isEmpty())
        m_Context.PostDiagnostic(result.Message);

    UpdateDataWorkflowPage();
    UpdateProjectPageDataCount();
    UpdateProjectStructureTree();
    UpdateDataActions();
}

void MainWindow::RemoveSelectedData()
{
    const QString selectedCatalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    if (selectedCatalogEntryId.trimmed().isEmpty())
    {
        m_Context.PostDiagnostic(QStringLiteral("No data selected."));
        UpdateDataActions();
        return;
    }

    QString errorMessage;
    if (!m_Context.DataManagement()->RemoveEntry(selectedCatalogEntryId,
                                                 &errorMessage))
    {
        m_Context.PostDiagnostic(errorMessage);
        UpdateDataActions();
        return;
    }

    if (!errorMessage.trimmed().isEmpty())
        m_Context.PostDiagnostic(errorMessage);

    if (auto* renderingManager = mitk::RenderingManager::GetInstance())
        renderingManager->RequestUpdateAll();

    UpdateDataActions();
}

void MainWindow::RenameSelectedData()
{
    const QString selectedCatalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    const auto* entry = m_Context.DataCatalog()->FindById(selectedCatalogEntryId);
    if (!entry)
    {
        m_Context.PostDiagnostic(QStringLiteral("No data selected."));
        UpdateDataActions();
        return;
    }

    bool accepted = false;
    const QString displayName =
        QInputDialog::getText(this,
                              QStringLiteral("Rename Node"),
                              QStringLiteral("New name:"),
                              QLineEdit::Normal,
                              entry->DisplayName,
                              &accepted)
            .trimmed();
    if (!accepted)
        return;

    if (displayName.isEmpty())
    {
        m_Context.PostDiagnostic(
            QStringLiteral("Data display name is required."));
        return;
    }

    QString errorMessage;
    if (!m_Context.DataManagement()->RenameEntry(selectedCatalogEntryId,
                                                 displayName,
                                                 &errorMessage))
    {
        m_Context.PostDiagnostic(errorMessage);
        UpdateDataActions();
        return;
    }

    auto node = m_Context.DataNodes()->FindNode(selectedCatalogEntryId);
    if (node.IsNotNull())
    {
        node->SetName(displayName.toStdString());
        if (auto* renderingManager = mitk::RenderingManager::GetInstance())
            renderingManager->RequestUpdateAll();
    }

    if (!errorMessage.trimmed().isEmpty())
        m_Context.PostDiagnostic(errorMessage);
    UpdateDataWorkflowPage();
    UpdateDataManagerSelection();
    UpdateDataActions();
}

void MainWindow::SyncTreeSelectionFromCore(const QString& hierarchyNodeId)
{
    if (!m_DataHierarchyView || !m_DataHierarchyModel ||
        !m_DataHierarchyView->selectionModel())
    {
        return;
    }

    m_SyncingSelectionFromCore = true;
    const auto resetSyncing = qScopeGuard([this]() {
        m_SyncingSelectionFromCore = false;
    });

    QSignalBlocker selectionBlocker(m_DataHierarchyView->selectionModel());
    if (hierarchyNodeId.trimmed().isEmpty())
    {
        m_DataHierarchyView->selectionModel()->clear();
        m_DataHierarchyView->selectionModel()->clearCurrentIndex();
        return;
    }

    const QModelIndex index =
        m_DataHierarchyModel->IndexForNodeId(hierarchyNodeId);
    if (!index.isValid())
        return;

    m_DataHierarchyView->expand(index.parent());
    m_DataHierarchyView->setCurrentIndex(index);
    m_DataHierarchyView->scrollTo(index);
}

void MainWindow::UpdateDataActions()
{
    const QString selectedCatalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    const bool hasSelection = !selectedCatalogEntryId.isEmpty();
    const bool hasSelectedNode =
        m_Context.DataNodes()->FindNode(selectedCatalogEntryId).IsNotNull();
    const bool canMeasureSurface =
        m_MeasurementService && SelectedDataIsSurface();

    if (m_RemoveDataAction)
        m_RemoveDataAction->setEnabled(hasSelection);
    if (m_RenameDataAction)
        m_RenameDataAction->setEnabled(hasSelection);
    if (m_RemoveSelectedDataAction)
        m_RemoveSelectedDataAction->setEnabled(hasSelection);
    if (m_DataPageRenameButton)
        m_DataPageRenameButton->setEnabled(hasSelection);
    if (m_DataPageRemoveButton)
        m_DataPageRemoveButton->setEnabled(hasSelection);
    if (m_ReinitializeSelectedDataAction)
        m_ReinitializeSelectedDataAction->setEnabled(hasSelectedNode);
    if (m_ToggleDataVisibilityAction)
        m_ToggleDataVisibilityAction->setEnabled(hasSelectedNode);
    if (m_ShowOnlySelectedDataAction)
        m_ShowOnlySelectedDataAction->setEnabled(hasSelectedNode);
    if (m_DataPageShowOnlyButton)
        m_DataPageShowOnlyButton->setEnabled(hasSelectedNode);
    if (m_DataPageReinitializeButton)
        m_DataPageReinitializeButton->setEnabled(hasSelectedNode);
    if (m_SurfaceRepresentationAction)
        m_SurfaceRepresentationAction->setEnabled(hasSelectedNode);
    if (m_WireframeRepresentationAction)
        m_WireframeRepresentationAction->setEnabled(hasSelectedNode);
    if (m_PointsRepresentationAction)
        m_PointsRepresentationAction->setEnabled(hasSelectedNode);
    if (m_MeasureAreaAction)
        m_MeasureAreaAction->setEnabled(canMeasureSurface);
    if (m_MeasureVolumeAction)
        m_MeasureVolumeAction->setEnabled(canMeasureSurface);
}

void MainWindow::ApplyDataManagerSearch(const QString& text)
{
    if (!m_DataHierarchyView || !m_DataHierarchyModel)
        return;

    ApplyDataManagerSearch(QModelIndex(), text.trimmed().toLower());
}

bool MainWindow::ApplyDataManagerSearch(const QModelIndex& parent,
                                        const QString& normalizedText)
{
    if (!m_DataHierarchyView || !m_DataHierarchyModel)
        return true;

    bool anyVisibleChild = false;
    const int rows = m_DataHierarchyModel->rowCount(parent);
    for (int row = 0; row < rows; ++row)
    {
        const QModelIndex index = m_DataHierarchyModel->index(row, 0, parent);
        const QString displayText =
            m_DataHierarchyModel->data(index, Qt::DisplayRole)
                .toString()
                .toLower();
        const bool selfMatches = normalizedText.isEmpty() ||
                                 displayText.contains(normalizedText);
        const bool childMatches =
            ApplyDataManagerSearch(index, normalizedText);
        const bool visible = selfMatches || childMatches;
        m_DataHierarchyView->setRowHidden(row, parent, !visible);
        anyVisibleChild = anyVisibleChild || visible;
    }

    return anyVisibleChild;
}

void MainWindow::UpdateDataManagerSelection()
{
    if (!m_DataOpacitySlider || !m_DataOpacityValueLabel)
        return;

    const QString catalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    auto node = m_Context.DataNodes()->FindNode(catalogEntryId);

    m_InternalDataManagerUpdate = true;
    const auto resetInternalUpdate = qScopeGuard([this]() {
        m_InternalDataManagerUpdate = false;
    });

    float opacity = 1.0f;
    if (node.IsNotNull())
        node->GetFloatProperty("opacity", opacity);
    const int opacityValue =
        qBound(0, static_cast<int>(opacity * 100.0f + 0.5f), 100);
    m_DataOpacitySlider->setValue(opacityValue);
    m_DataOpacityValueLabel->setText(
        QStringLiteral("%1%").arg(opacityValue));

    if (m_DataColorButton)
    {
        if (node.IsNotNull())
        {
            float rgb[3] = {1.0f, 1.0f, 1.0f};
            node->GetColor(rgb);
            m_DataColorButton->setStyleSheet(ColorButtonStyle(rgb));
        }
        else
        {
            m_DataColorButton->setStyleSheet(QString());
        }
    }

    UpdateDataManagerPropertiesTable();
}

void MainWindow::UpdateDataManagerPropertiesTable()
{
    if (!m_DataPropertiesTable || !m_DataPropertiesTable->isVisible())
        return;

    m_DataPropertiesTable->setRowCount(0);

    const QString catalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    const auto* entry = m_Context.DataCatalog()->FindById(catalogEntryId);
    auto node = m_Context.DataNodes()->FindNode(catalogEntryId);
    if (!entry && node.IsNull())
        return;

    auto addRow = [this](const QString& key, const QString& value) {
        const int row = m_DataPropertiesTable->rowCount();
        m_DataPropertiesTable->insertRow(row);
        m_DataPropertiesTable->setItem(row,
                                       0,
                                       new QTableWidgetItem(key));
        m_DataPropertiesTable->setItem(row,
                                       1,
                                       new QTableWidgetItem(value));
    };

    if (node.IsNotNull())
    {
        addRow(QStringLiteral("Name"),
               QString::fromStdString(node->GetName()));
    }
    else if (entry)
    {
        addRow(QStringLiteral("Name"), entry->DisplayName);
    }

    if (entry)
    {
        addRow(QStringLiteral("Catalog Id"), entry->Id);
        addRow(QStringLiteral("Source"), entry->SourcePath);
        if (!entry->Modality.trimmed().isEmpty())
            addRow(QStringLiteral("Modality"), entry->Modality);
        addRow(QStringLiteral("Role"), RoleDisplayName(entry->WorkflowRole));
    }

    if (node.IsNotNull())
    {
        if (node->GetData())
        {
            addRow(QStringLiteral("Data Type"),
                   QString::fromStdString(node->GetData()->GetNameOfClass()));
        }

        bool visible = true;
        node->GetBoolProperty("visible", visible);
        addRow(QStringLiteral("Visible"),
               visible ? QStringLiteral("true") : QStringLiteral("false"));

        float opacity = 1.0f;
        node->GetFloatProperty("opacity", opacity);
        addRow(QStringLiteral("Opacity"),
               QString::number(opacity, 'f', 2));

        float rgb[3] = {1.0f, 1.0f, 1.0f};
        node->GetColor(rgb);
        addRow(QStringLiteral("Color"), FormatMitkColor(rgb));

        if (auto* propertyList = node->GetPropertyList())
        {
            if (auto* propertyMap = propertyList->GetMap())
            {
                for (auto it = propertyMap->begin();
                     it != propertyMap->end();
                     ++it)
                {
                    if (!ShouldShowDataManagerProperty(it->first))
                        continue;

                    addRow(QString::fromStdString(it->first),
                           DataManagerPropertyValue(it->second.GetPointer()));
                }
            }
        }
    }

    m_DataPropertiesTable->resizeColumnsToContents();
}

void MainWindow::ApplySelectedDataOpacity(int value)
{
    if (m_DataOpacityValueLabel)
    {
        m_DataOpacityValueLabel->setText(
            QStringLiteral("%1%").arg(value));
    }

    if (m_InternalDataManagerUpdate)
        return;

    const QString catalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    auto node = m_Context.DataNodes()->FindNode(catalogEntryId);
    if (node.IsNull())
        return;

    node->SetFloatProperty("opacity", static_cast<float>(value) / 100.0f);
    UpdateDataManagerPropertiesTable();
    if (auto* renderingManager = mitk::RenderingManager::GetInstance())
        renderingManager->RequestUpdateAll();
}

void MainWindow::ToggleSelectedDataVisibility()
{
    const QString catalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    auto node = m_Context.DataNodes()->FindNode(catalogEntryId);
    if (node.IsNull())
        return;

    bool visible = true;
    node->GetBoolProperty("visible", visible);
    node->SetBoolProperty("visible", !visible);
    RefreshDataManagerAfterVisibilityChange();
}

void MainWindow::SetSelectedDataVolumeRendering(bool enabled)
{
    const QString catalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    auto node = m_Context.DataNodes()->FindNode(catalogEntryId);
    if (node.IsNull())
    {
        m_Context.PostDiagnostic(QStringLiteral(
            "Select image data before changing Volume Rendering."));
        return;
    }

    node->SetBoolProperty("volumerendering", enabled);
    UpdateDataManagerSelection();
    if (auto* renderingManager = mitk::RenderingManager::GetInstance())
        renderingManager->RequestUpdateAll();
}

bool MainWindow::SelectedDataIsSurface() const
{
    const QString catalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    auto node = m_Context.DataNodes()->FindNode(catalogEntryId);
    return node.IsNotNull() &&
           dynamic_cast<mitk::Surface*>(node->GetData()) != nullptr;
}

void MainWindow::RunSurfaceMeasurement(
    xq::core::SurfaceMeasurementKind kind)
{
    if (!m_MeasurementService)
    {
        m_Context.PostDiagnostic(
            QStringLiteral("Measurement service is not configured."));
        return;
    }

    const QString catalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    auto node = m_Context.DataNodes()->FindNode(catalogEntryId);
    const auto result = m_MeasurementService->MeasureSurface(node, kind);
    if (!result.Message.trimmed().isEmpty())
        m_Context.PostDiagnostic(result.Message);
}

void MainWindow::SetCrosshairEnabled(bool enabled)
{
    m_Context.Preferences()->SetBoolValue(
        QStringLiteral("view.crosshair.enabled"),
        enabled);
    m_Context.PostDiagnostic(
        enabled ? QStringLiteral("Crosshair enabled.")
                : QStringLiteral("Crosshair disabled."));
}

void MainWindow::ShowOnlySelectedData()
{
    const QString selectedCatalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    auto selectedNode = m_Context.DataNodes()->FindNode(selectedCatalogEntryId);
    if (selectedNode.IsNull())
        return;

    const QStringList catalogEntryIds = m_Context.DataNodes()->CatalogEntryIds();
    for (const auto& catalogEntryId : catalogEntryIds)
    {
        auto node = m_Context.DataNodes()->FindNode(catalogEntryId);
        if (node.IsNotNull())
        {
            node->SetBoolProperty(
                "visible",
                catalogEntryId == selectedCatalogEntryId);
        }
    }

    RefreshDataManagerAfterVisibilityChange();
}

void MainWindow::SetAllDataVisibility(bool visible)
{
    const QStringList catalogEntryIds = m_Context.DataNodes()->CatalogEntryIds();
    for (const auto& catalogEntryId : catalogEntryIds)
    {
        auto node = m_Context.DataNodes()->FindNode(catalogEntryId);
        if (node.IsNotNull())
            node->SetBoolProperty("visible", visible);
    }

    RefreshDataManagerAfterVisibilityChange();
}

void MainWindow::RefreshDataManagerAfterVisibilityChange()
{
    UpdateDataManagerSelection();
    if (auto* renderingManager = mitk::RenderingManager::GetInstance())
        renderingManager->RequestUpdateAll();
}

void MainWindow::ReinitializeSelectedData()
{
    const QString catalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    auto node = m_Context.DataNodes()->FindNode(catalogEntryId);
    if (node.IsNull() || !node->GetData())
        return;

    if (auto* renderingManager = mitk::RenderingManager::GetInstance())
    {
        renderingManager->InitializeViews(
            node->GetData()->GetTimeGeometry());
    }
}

void MainWindow::GlobalReinitializeData()
{
    if (m_Context.DataStorage().IsNull())
        return;

    if (auto* renderingManager = mitk::RenderingManager::GetInstance())
        renderingManager->InitializeViewsByBoundingObjects(
            m_Context.DataStorage());
}

void MainWindow::SetSelectedDataRepresentation(int representation,
                                               bool materialWireframe,
                                               bool disableVolumeRendering)
{
    const QString catalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    auto node = m_Context.DataNodes()->FindNode(catalogEntryId);
    if (node.IsNull())
        return;

    if (disableVolumeRendering)
        node->SetBoolProperty("volumerendering", false);
    node->SetProperty("material.representation",
                      mitk::IntProperty::New(representation));
    node->SetBoolProperty("material.wireframe", materialWireframe);
    UpdateDataManagerSelection();
    if (auto* renderingManager = mitk::RenderingManager::GetInstance())
        renderingManager->RequestUpdateAll();
}

QWidget* MainWindow::CreateWorkflowPage(const QString& id,
                                        const QString& title)
{
    auto* page = new QFrame(this);
    page->setObjectName(QStringLiteral("xqWorkflowPage_%1").arg(id));
    page->setFrameShape(QFrame::NoFrame);

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* heading = new QLabel(title, page);
    QFont headingFont = heading->font();
    headingFont.setPointSize(14);
    headingFont.setBold(true);
    heading->setFont(headingFont);

    layout->addWidget(heading);
    if (id == QStringLiteral("project"))
    {
        auto* infoGroup =
            new QGroupBox(QStringLiteral("Project Information"), page);
        infoGroup->setObjectName(
            QStringLiteral("xqProjectInformationGroup"));
        auto* infoLayout = new QVBoxLayout(infoGroup);
        infoLayout->setContentsMargins(8, 8, 8, 8);
        infoLayout->setSpacing(6);

        m_ProjectNameLabel = new QLabel(infoGroup);
        m_ProjectNameLabel->setObjectName(QStringLiteral("xqProjectPageName"));
        QFont projectNameFont = m_ProjectNameLabel->font();
        projectNameFont.setBold(true);
        projectNameFont.setPointSize(projectNameFont.pointSize() + 2);
        m_ProjectNameLabel->setFont(projectNameFont);

        m_ProjectPathLabel = new QLabel(infoGroup);
        m_ProjectPathLabel->setObjectName(QStringLiteral("xqProjectPagePath"));
        m_ProjectPathLabel->setWordWrap(true);
        m_ProjectSchemaLabel = new QLabel(infoGroup);
        m_ProjectSchemaLabel->setObjectName(
            QStringLiteral("xqProjectPageSchema"));
        m_ProjectDataCountLabel = new QLabel(infoGroup);
        m_ProjectDataCountLabel->setObjectName(
            QStringLiteral("xqProjectPageDataCount"));
        m_ProjectOpenFolderButton =
            new QPushButton(QStringLiteral("Open Project Folder"),
                            infoGroup);
        m_ProjectOpenFolderButton->setObjectName(
            QStringLiteral("xqProjectOpenFolderButton"));
        m_ProjectOpenFolderButton->setEnabled(false);

        infoLayout->addWidget(m_ProjectNameLabel);
        infoLayout->addWidget(m_ProjectPathLabel);
        infoLayout->addWidget(m_ProjectSchemaLabel);
        infoLayout->addWidget(m_ProjectDataCountLabel);
        infoLayout->addWidget(m_ProjectOpenFolderButton);
        layout->addWidget(infoGroup);

        m_ProjectStructureTree = new QTreeWidget(page);
        m_ProjectStructureTree->setObjectName(
            QStringLiteral("xqProjectStructureTree"));
        m_ProjectStructureTree->setColumnCount(1);
        m_ProjectStructureTree->setHeaderLabel(
            QStringLiteral("Project Structure"));
        m_ProjectStructureTree->header()->setStretchLastSection(true);
        layout->addWidget(m_ProjectStructureTree, 1);

        auto* projectButtonLayout = new QHBoxLayout();
        auto* newProjectButton =
            new QPushButton(QStringLiteral("New Project"), page);
        newProjectButton->setObjectName(QStringLiteral("xqProjectNewButton"));
        auto* openProjectButton =
            new QPushButton(QStringLiteral("Open Project"), page);
        openProjectButton->setObjectName(QStringLiteral("xqProjectOpenButton"));
        m_ProjectRefreshButton =
            new QPushButton(QStringLiteral("Refresh"), page);
        m_ProjectRefreshButton->setObjectName(
            QStringLiteral("xqProjectRefreshButton"));
        m_ProjectRefreshButton->setEnabled(false);

        projectButtonLayout->addWidget(newProjectButton);
        projectButtonLayout->addWidget(openProjectButton);
        projectButtonLayout->addWidget(m_ProjectRefreshButton);
        layout->addLayout(projectButtonLayout);

        connect(newProjectButton,
                &QPushButton::clicked,
                this,
                [this]() {
                    CreateProjectFromProvider();
                });
        connect(openProjectButton,
                &QPushButton::clicked,
                this,
                [this]() {
                    OpenProjectFromProvider();
                });
    }
    else if (id == QStringLiteral("data"))
    {
        auto* selectedDataGroup =
            new QGroupBox(QStringLiteral("Selected Data"), page);
        selectedDataGroup->setObjectName(
            QStringLiteral("xqDataSelectedDataGroup"));
        auto* selectedDataLayout = new QVBoxLayout(selectedDataGroup);
        selectedDataLayout->setContentsMargins(8, 8, 8, 8);
        selectedDataLayout->setSpacing(6);

        m_DataSelectionLabel = new QLabel(selectedDataGroup);
        m_DataSelectionLabel->setObjectName(
            QStringLiteral("xqDataPageSelection"));
        m_DataDisplayNameLabel = new QLabel(selectedDataGroup);
        m_DataDisplayNameLabel->setObjectName(
            QStringLiteral("xqDataPageDisplayName"));
        m_DataWorkflowRoleLabel = new QLabel(selectedDataGroup);
        m_DataWorkflowRoleLabel->setObjectName(
            QStringLiteral("xqDataPageWorkflowRole"));
        selectedDataLayout->addWidget(m_DataSelectionLabel);
        selectedDataLayout->addWidget(m_DataDisplayNameLabel);
        selectedDataLayout->addWidget(m_DataWorkflowRoleLabel);
        layout->addWidget(selectedDataGroup);

        auto* provenanceGroup =
            new QGroupBox(QStringLiteral("Provenance"), page);
        provenanceGroup->setObjectName(QStringLiteral("xqDataProvenanceGroup"));
        auto* provenanceLayout = new QVBoxLayout(provenanceGroup);
        provenanceLayout->setContentsMargins(8, 8, 8, 8);
        provenanceLayout->setSpacing(6);
        m_DataCatalogIdLabel = new QLabel(provenanceGroup);
        m_DataCatalogIdLabel->setObjectName(
            QStringLiteral("xqDataPageCatalogId"));
        m_DataSourcePathLabel = new QLabel(provenanceGroup);
        m_DataSourcePathLabel->setObjectName(
            QStringLiteral("xqDataPageSourcePath"));
        m_DataSourcePathLabel->setWordWrap(true);
        provenanceLayout->addWidget(m_DataCatalogIdLabel);
        provenanceLayout->addWidget(m_DataSourcePathLabel);
        layout->addWidget(provenanceGroup);

        auto* actionsGroup =
            new QGroupBox(QStringLiteral("Data Actions"), page);
        actionsGroup->setObjectName(QStringLiteral("xqDataActionsGroup"));
        auto* actionsLayout = new QVBoxLayout(actionsGroup);
        actionsLayout->setContentsMargins(8, 8, 8, 8);
        actionsLayout->setSpacing(6);

        auto* importButton =
            new QPushButton(QStringLiteral("Open Data File..."), actionsGroup);
        importButton->setObjectName(QStringLiteral("xqDataPageImportButton"));
        m_DataPageRenameButton =
            new QPushButton(QStringLiteral("Rename..."), actionsGroup);
        m_DataPageRenameButton->setObjectName(
            QStringLiteral("xqDataPageRenameButton"));
        m_DataPageRemoveButton =
            new QPushButton(QStringLiteral("Remove"), actionsGroup);
        m_DataPageRemoveButton->setObjectName(
            QStringLiteral("xqDataPageRemoveButton"));
        m_DataPageShowOnlyButton =
            new QPushButton(QStringLiteral("Show Only Selected"),
                            actionsGroup);
        m_DataPageShowOnlyButton->setObjectName(
            QStringLiteral("xqDataPageShowOnlyButton"));
        m_DataPageReinitializeButton =
            new QPushButton(QStringLiteral("Reinitialize Node"),
                            actionsGroup);
        m_DataPageReinitializeButton->setObjectName(
            QStringLiteral("xqDataPageReinitializeButton"));

        actionsLayout->addWidget(importButton);
        actionsLayout->addWidget(m_DataPageRenameButton);
        actionsLayout->addWidget(m_DataPageRemoveButton);
        actionsLayout->addWidget(m_DataPageShowOnlyButton);
        actionsLayout->addWidget(m_DataPageReinitializeButton);
        layout->addWidget(actionsGroup);
        layout->addStretch(1);

        connect(importButton,
                &QPushButton::clicked,
                this,
                [this]() { ImportData(); });
        connect(m_DataPageRenameButton,
                &QPushButton::clicked,
                this,
                [this]() { RenameSelectedData(); });
        connect(m_DataPageRemoveButton,
                &QPushButton::clicked,
                this,
                [this]() { RemoveSelectedData(); });
        connect(m_DataPageShowOnlyButton,
                &QPushButton::clicked,
                this,
                [this]() { ShowOnlySelectedData(); });
        connect(m_DataPageReinitializeButton,
                &QPushButton::clicked,
                this,
                [this]() { ReinitializeSelectedData(); });
    }
    else
    {
        const auto acceptedRoles =
            xq::core::WorkflowContextService::AcceptedDataRolesForWorkflow(id);
        const auto operations =
            m_Context.WorkflowOperations()->OperationsForWorkflow(id);
        if (!acceptedRoles.isEmpty() || !operations.isEmpty())
        {
            if (!operations.isEmpty())
            {
                auto* operationSelector = new QComboBox(page);
                if (id == QStringLiteral("image-preprocessing"))
                {
                    operationSelector->setObjectName(QStringLiteral(
                        "xqImagePreprocessingOperationSelector"));
                }
                else
                {
                    operationSelector->setObjectName(
                        QStringLiteral("xqWorkflowOperationSelector_%1").arg(id));
                }
                for (const auto& operation : operations)
                {
                    operationSelector->addItem(operation.Title, operation.Id);
                }

                const int selectedIndex = operationSelector->findData(
                    m_Context.WorkflowOperations()->SelectedOperationId(id));
                if (selectedIndex >= 0)
                    operationSelector->setCurrentIndex(selectedIndex);

                connect(operationSelector,
                        &QComboBox::currentIndexChanged,
                        this,
                        [this, id, operationSelector](int index) {
                            if (index < 0)
                                return;

                            QString message;
                            if (!m_Context.WorkflowOperations()->SelectOperation(
                                    id,
                                    operationSelector->itemData(index).toString(),
                                    &message))
                            {
                                m_Context.PostDiagnostic(message);
                                UpdateWorkflowOperationControls();
                            }
                        });

                m_WorkflowOperationSelectors.insert(id, operationSelector);

                auto* parameterPanel = new QWidget(page);
                if (id == QStringLiteral("image-preprocessing"))
                {
                    parameterPanel->setObjectName(QStringLiteral(
                        "xqImagePreprocessingParameterPanel"));
                }
                else
                {
                    parameterPanel->setObjectName(
                        QStringLiteral("xqWorkflowParameterPanel_%1").arg(id));
                }
                auto* parameterLayout = new QFormLayout(parameterPanel);
                parameterLayout->setContentsMargins(0, 0, 0, 0);
                parameterLayout->setSpacing(8);
                m_WorkflowParameterPanels.insert(id, parameterPanel);

                if (id == QStringLiteral("image-preprocessing"))
                {
                    auto* intro = new QLabel(
                        QStringLiteral(
                            "Run image processing operations on loaded image nodes. "
                            "Outputs are added to DataStorage with source and parameter metadata."),
                        page);
                    intro->setWordWrap(true);
                    layout->addWidget(intro);

                    auto* contextLabel = new QLabel(page);
                    contextLabel->setObjectName(
                        QStringLiteral("xqImagePreprocessingContextLabel"));
                    contextLabel->setWordWrap(true);
                    contextLabel->setStyleSheet(QStringLiteral(
                        "QLabel { background: #F8FAFC; border: 1px solid #B7C9F7; "
                        "border-radius: 4px; padding: 6px; }"));
                    m_WorkflowContextStatusLabels.insert(id, contextLabel);
                    layout->addWidget(contextLabel);

                    auto* inputGroup =
                        new QGroupBox(QStringLiteral("Input"), page);
                    inputGroup->setObjectName(
                        QStringLiteral("xqImagePreprocessingInputGroup"));
                    auto* inputLayout = new QFormLayout(inputGroup);
                    auto* imageSelector = new QComboBox(inputGroup);
                    imageSelector->setObjectName(QStringLiteral(
                        "xqImagePreprocessingImageComboBox"));
                    auto* refreshButton =
                        new QPushButton(QStringLiteral("Refresh"), inputGroup);
                    refreshButton->setObjectName(QStringLiteral(
                        "xqImagePreprocessingRefreshButton"));
                    auto* inputRow = new QHBoxLayout();
                    inputRow->addWidget(imageSelector, 1);
                    inputRow->addWidget(refreshButton);
                    inputLayout->addRow(QStringLiteral("Image:"), inputRow);
                    layout->addWidget(inputGroup);

                    auto* operationGroup =
                        new QGroupBox(QStringLiteral("Operation"), page);
                    operationGroup->setObjectName(QStringLiteral(
                        "xqImagePreprocessingOperationGroup"));
                    auto* operationLayout = new QFormLayout(operationGroup);
                    operationSelector->setParent(operationGroup);
                    operationLayout->addRow(QStringLiteral("Tool:"),
                                            operationSelector);
                    layout->addWidget(operationGroup);

                    auto* thresholdGroup = new QGroupBox(
                        QStringLiteral("Threshold Parameters"), page);
                    thresholdGroup->setObjectName(QStringLiteral(
                        "xqImagePreprocessingThresholdGroup"));
                    auto* thresholdLayout = new QVBoxLayout(thresholdGroup);
                    thresholdLayout->setContentsMargins(8, 8, 8, 8);
                    thresholdLayout->addWidget(new QLabel(
                        QStringLiteral(
                            "Lower/upper range and output values are edited "
                            "below for threshold-based tools."),
                        thresholdGroup));
                    layout->addWidget(thresholdGroup);

                    auto* seedGroup =
                        new QGroupBox(QStringLiteral("Seed"), page);
                    seedGroup->setObjectName(
                        QStringLiteral("xqImagePreprocessingSeedGroup"));
                    auto* seedLayout = new QVBoxLayout(seedGroup);
                    seedLayout->setContentsMargins(8, 8, 8, 8);
                    seedLayout->addWidget(new QLabel(
                        QStringLiteral(
                            "Connected threshold seeds use x,y,z entries separated by semicolons."),
                        seedGroup));
                    layout->addWidget(seedGroup);

                    auto* cropGroup =
                        new QGroupBox(QStringLiteral("Crop Region"), page);
                    cropGroup->setObjectName(
                        QStringLiteral("xqImagePreprocessingCropGroup"));
                    auto* cropLayout = new QVBoxLayout(cropGroup);
                    cropLayout->setContentsMargins(8, 8, 8, 8);
                    cropLayout->addWidget(new QLabel(
                        QStringLiteral(
                            "Origin and size controls are available when Crop is selected."),
                        cropGroup));
                    layout->addWidget(cropGroup);

                    auto* resampleGroup =
                        new QGroupBox(QStringLiteral("Resample / Surface"),
                                      page);
                    resampleGroup->setObjectName(QStringLiteral(
                        "xqImagePreprocessingResampleGroup"));
                    auto* resampleLayout = new QVBoxLayout(resampleGroup);
                    resampleLayout->setContentsMargins(8, 8, 8, 8);
                    resampleLayout->addWidget(new QLabel(
                        QStringLiteral(
                            "Spacing controls are available for resampling tools."),
                        resampleGroup));
                    layout->addWidget(resampleGroup);

                    parameterPanel->setParent(page);
                    layout->addWidget(parameterPanel);

                    auto* diagnosticsText = new QTextEdit(page);
                    diagnosticsText->setObjectName(QStringLiteral(
                        "xqImagePreprocessingDiagnosticsText"));
                    diagnosticsText->setReadOnly(true);
                    diagnosticsText->setMinimumHeight(90);
                    layout->addWidget(diagnosticsText);
                    connect(&m_Context,
                            &xq::core::ApplicationContext::DiagnosticPosted,
                            diagnosticsText,
                            [diagnosticsText](const QString& message) {
                                diagnosticsText->append(message);
                            });
                }
                else if (id == QStringLiteral("flow-simulation"))
                {
                    operationSelector->setVisible(false);
                    layout->addWidget(operationSelector);

                    auto* jobLayout = new QHBoxLayout();
                    auto* jobTitleLabel =
                        new QLabel(QStringLiteral("Job:"), page);
                    jobTitleLabel->setObjectName(
                        QStringLiteral("xqFlowJobTitleLabel"));
                    auto* jobNameLabel =
                        new QLabel(QStringLiteral("(none)"), page);
                    jobNameLabel->setObjectName(
                        QStringLiteral("xqFlowJobNameLabel"));
                    auto* createJobButton =
                        new QPushButton(QStringLiteral("Create Job..."),
                                        page);
                    createJobButton->setObjectName(
                        QStringLiteral("xqFlowCreateJobButton"));
                    createJobButton->setEnabled(false);
                    jobLayout->addWidget(jobTitleLabel);
                    jobLayout->addWidget(jobNameLabel, 1);
                    jobLayout->addWidget(createJobButton);
                    layout->addLayout(jobLayout);

                    auto* meshLayout = new QHBoxLayout();
                    auto* meshLabel =
                        new QLabel(QStringLiteral("Mesh:"), page);
                    meshLabel->setObjectName(
                        QStringLiteral("xqFlowMeshLabel"));
                    auto* meshSelector = new QComboBox(page);
                    meshSelector->setObjectName(
                        QStringLiteral("xqFlowMeshSelector"));
                    meshLayout->addWidget(meshLabel);
                    meshLayout->addWidget(meshSelector, 1);
                    layout->addLayout(meshLayout);

                    auto* flowTabs = new QTabWidget(page);
                    flowTabs->setObjectName(QStringLiteral("xqFlowTabs"));

                    auto* basicTab = new QWidget(flowTabs);
                    auto* basicLayout = new QVBoxLayout(basicTab);
                    auto* timeGroup =
                        new QGroupBox(QStringLiteral("Time Stepping"),
                                      basicTab);
                    timeGroup->setObjectName(
                        QStringLiteral("xqFlowTimeSteppingGroup"));
                    auto* timeForm = new QFormLayout(timeGroup);
                    auto* startTimeSpin = new QDoubleSpinBox(timeGroup);
                    startTimeSpin->setObjectName(
                        QStringLiteral("xqFlowStartTimeSpinBox"));
                    startTimeSpin->setRange(0.0, 1.0e10);
                    startTimeSpin->setDecimals(6);
                    startTimeSpin->setSingleStep(0.001);
                    startTimeSpin->setValue(0.0);
                    auto* endTimeSpin = new QDoubleSpinBox(timeGroup);
                    endTimeSpin->setObjectName(
                        QStringLiteral("xqFlowEndTimeSpinBox"));
                    endTimeSpin->setRange(0.0, 1.0e10);
                    endTimeSpin->setDecimals(6);
                    endTimeSpin->setSingleStep(0.001);
                    endTimeSpin->setValue(1.0);
                    auto* timeStepSizeSpin =
                        new QDoubleSpinBox(timeGroup);
                    timeStepSizeSpin->setObjectName(
                        QStringLiteral("xqFlowTimeStepSizeSpinBox"));
                    timeStepSizeSpin->setRange(1.0e-12, 1.0e6);
                    timeStepSizeSpin->setDecimals(8);
                    timeStepSizeSpin->setSingleStep(0.0001);
                    timeStepSizeSpin->setValue(0.001);
                    auto* numTimeStepsSpin = new QSpinBox(timeGroup);
                    numTimeStepsSpin->setObjectName(
                        QStringLiteral("xqFlowNumTimeStepsSpinBox"));
                    numTimeStepsSpin->setRange(1, 1000000000);
                    numTimeStepsSpin->setValue(1000);
                    timeForm->addRow(QStringLiteral("Start Time:"),
                                     startTimeSpin);
                    timeForm->addRow(QStringLiteral("End Time:"),
                                     endTimeSpin);
                    timeForm->addRow(QStringLiteral("Time Step Size:"),
                                     timeStepSizeSpin);
                    timeForm->addRow(QStringLiteral("Num Time Steps:"),
                                     numTimeStepsSpin);
                    auto* saveJobButton =
                        new QPushButton(QStringLiteral("Save Job"),
                                        basicTab);
                    saveJobButton->setObjectName(
                        QStringLiteral("xqFlowSaveJobButton"));
                    saveJobButton->setEnabled(false);
                    basicLayout->addWidget(timeGroup);
                    basicLayout->addWidget(saveJobButton);
                    basicLayout->addStretch(1);
                    flowTabs->addTab(basicTab, QStringLiteral("Basic"));

                    auto* bcTab = new QWidget(flowTabs);
                    auto* bcLayout = new QVBoxLayout(bcTab);
                    auto* bcTable = new QTableWidget(bcTab);
                    bcTable->setObjectName(
                        QStringLiteral("xqFlowBCTable"));
                    bcTable->setMinimumHeight(200);
                    bcTable->setSelectionMode(
                        QAbstractItemView::SingleSelection);
                    bcTable->setSelectionBehavior(
                        QAbstractItemView::SelectRows);
                    bcTable->setColumnCount(3);
                    bcTable->setHorizontalHeaderLabels(
                        {QStringLiteral("Face"),
                         QStringLiteral("Type"),
                         QStringLiteral("Values")});
                    bcTable->horizontalHeader()->setStretchLastSection(true);
                    bcLayout->addWidget(bcTable);
                    auto* bcButtonLayout = new QHBoxLayout();
                    auto* addBcButton =
                        new QPushButton(QStringLiteral("Add BC"), bcTab);
                    addBcButton->setObjectName(
                        QStringLiteral("xqFlowAddBCButton"));
                    auto* removeBcButton =
                        new QPushButton(QStringLiteral("Remove BC"), bcTab);
                    removeBcButton->setObjectName(
                        QStringLiteral("xqFlowRemoveBCButton"));
                    removeBcButton->setEnabled(false);
                    bcButtonLayout->addWidget(addBcButton);
                    bcButtonLayout->addWidget(removeBcButton);
                    bcButtonLayout->addStretch(1);
                    bcLayout->addLayout(bcButtonLayout);
                    bcLayout->addStretch(1);
                    flowTabs->addTab(bcTab,
                                     QStringLiteral("Inlet/Outlet BCs"));

                    auto* wallTab = new QWidget(flowTabs);
                    auto* wallLayout = new QVBoxLayout(wallTab);
                    auto* wallTypeLayout = new QHBoxLayout();
                    auto* wallTypeLabel =
                        new QLabel(QStringLiteral("Wall Type:"), wallTab);
                    auto* wallTypeCombo = new QComboBox(wallTab);
                    wallTypeCombo->setObjectName(
                        QStringLiteral("xqFlowWallTypeCombo"));
                    wallTypeCombo->addItems(
                        {QStringLiteral("Rigid"),
                         QStringLiteral("Deformable")});
                    wallTypeLayout->addWidget(wallTypeLabel);
                    wallTypeLayout->addWidget(wallTypeCombo, 1);
                    wallLayout->addLayout(wallTypeLayout);
                    auto* deformableGroup =
                        new QGroupBox(QStringLiteral(
                                          "Deformable Wall Properties"),
                                      wallTab);
                    deformableGroup->setObjectName(
                        QStringLiteral("xqFlowDeformableWallGroup"));
                    deformableGroup->setEnabled(false);
                    auto* deformableForm =
                        new QFormLayout(deformableGroup);
                    auto* wallThicknessSpin =
                        new QDoubleSpinBox(deformableGroup);
                    wallThicknessSpin->setObjectName(
                        QStringLiteral("xqFlowWallThicknessSpinBox"));
                    wallThicknessSpin->setRange(0.0, 100.0);
                    wallThicknessSpin->setDecimals(4);
                    wallThicknessSpin->setSingleStep(0.01);
                    wallThicknessSpin->setValue(0.5);
                    auto* elasticModulusSpin =
                        new QDoubleSpinBox(deformableGroup);
                    elasticModulusSpin->setObjectName(
                        QStringLiteral("xqFlowElasticModulusSpinBox"));
                    elasticModulusSpin->setRange(0.0, 1.0e12);
                    elasticModulusSpin->setDecimals(2);
                    elasticModulusSpin->setSingleStep(1000.0);
                    elasticModulusSpin->setValue(4.0e6);
                    auto* poissonRatioSpin =
                        new QDoubleSpinBox(deformableGroup);
                    poissonRatioSpin->setObjectName(
                        QStringLiteral("xqFlowPoissonRatioSpinBox"));
                    poissonRatioSpin->setRange(0.0, 0.5);
                    poissonRatioSpin->setDecimals(4);
                    poissonRatioSpin->setSingleStep(0.01);
                    poissonRatioSpin->setValue(0.5);
                    deformableForm->addRow(QStringLiteral("Thickness:"),
                                           wallThicknessSpin);
                    deformableForm->addRow(QStringLiteral("Elastic Modulus:"),
                                           elasticModulusSpin);
                    deformableForm->addRow(QStringLiteral("Poisson Ratio:"),
                                           poissonRatioSpin);
                    auto* variableWallCheckBox =
                        new QCheckBox(QStringLiteral(
                                          "Variable Wall Properties"),
                                      wallTab);
                    variableWallCheckBox->setObjectName(
                        QStringLiteral("xqFlowVariableWallCheckBox"));
                    wallLayout->addWidget(deformableGroup);
                    wallLayout->addWidget(variableWallCheckBox);
                    wallLayout->addStretch(1);
                    connect(wallTypeCombo,
                            &QComboBox::currentIndexChanged,
                            deformableGroup,
                            [deformableGroup](int index) {
                                deformableGroup->setEnabled(index == 1);
                            });
                    flowTabs->addTab(wallTab,
                                     QStringLiteral("Wall Properties"));

                    auto* solverTab = new QWidget(flowTabs);
                    auto* solverLayout = new QVBoxLayout(solverTab);
                    auto* presetLayout = new QHBoxLayout();
                    auto* presetLabel =
                        new QLabel(QStringLiteral("Preset:"), solverTab);
                    auto* solverPresetCombo = new QComboBox(solverTab);
                    solverPresetCombo->setObjectName(
                        QStringLiteral("xqFlowSolverPresetCombo"));
                    solverPresetCombo->addItems(
                        {QStringLiteral("Custom"),
                         QStringLiteral("Steady Flow"),
                         QStringLiteral("Pulsatile Flow"),
                         QStringLiteral("Deformable Wall"),
                         QStringLiteral("High Accuracy")});
                    auto* applyPresetButton =
                        new QPushButton(QStringLiteral("Apply Preset"),
                                        solverTab);
                    applyPresetButton->setObjectName(
                        QStringLiteral("xqFlowApplyPresetButton"));
                    presetLayout->addWidget(presetLabel);
                    presetLayout->addWidget(solverPresetCombo, 1);
                    presetLayout->addWidget(applyPresetButton);
                    solverLayout->addLayout(presetLayout);

                    auto* solverSettingsGroup =
                        new QGroupBox(QStringLiteral("Solver Settings"),
                                      solverTab);
                    solverSettingsGroup->setObjectName(
                        QStringLiteral("xqFlowSolverSettingsGroup"));
                    auto* solverForm = new QFormLayout(solverSettingsGroup);
                    auto* residualTolSpin =
                        new QDoubleSpinBox(solverSettingsGroup);
                    residualTolSpin->setObjectName(
                        QStringLiteral("xqFlowResidualToleranceSpinBox"));
                    residualTolSpin->setRange(1.0e-15, 1.0);
                    residualTolSpin->setDecimals(10);
                    residualTolSpin->setSingleStep(0.0001);
                    residualTolSpin->setValue(0.001);
                    auto* stepConstructionCombo =
                        new QComboBox(solverSettingsGroup);
                    stepConstructionCombo->setObjectName(
                        QStringLiteral("xqFlowStepConstructionCombo"));
                    stepConstructionCombo->addItems(
                        {QStringLiteral("0 1 0 1"),
                         QStringLiteral("0 1 0 1 0 1"),
                         QStringLiteral("0 1 0 1 0 1 0 1")});
                    auto* pressureCouplingCombo =
                        new QComboBox(solverSettingsGroup);
                    pressureCouplingCombo->setObjectName(
                        QStringLiteral("xqFlowPressureCouplingCombo"));
                    pressureCouplingCombo->addItems(
                        {QStringLiteral("Implicit"),
                         QStringLiteral("Explicit")});
                    auto* maxIterationsSpin =
                        new QSpinBox(solverSettingsGroup);
                    maxIterationsSpin->setObjectName(
                        QStringLiteral("xqFlowMaxIterationsSpinBox"));
                    maxIterationsSpin->setRange(1, 100000);
                    maxIterationsSpin->setValue(10);
                    auto* stabilizationCheckBox =
                        new QCheckBox(solverSettingsGroup);
                    stabilizationCheckBox->setObjectName(
                        QStringLiteral("xqFlowStabilizationCheckBox"));
                    stabilizationCheckBox->setChecked(true);
                    solverForm->addRow(QStringLiteral("Residual Tolerance:"),
                                       residualTolSpin);
                    solverForm->addRow(QStringLiteral("Step Construction:"),
                                       stepConstructionCombo);
                    solverForm->addRow(QStringLiteral("Pressure Coupling:"),
                                       pressureCouplingCombo);
                    solverForm->addRow(QStringLiteral("Max Iterations:"),
                                       maxIterationsSpin);
                    solverForm->addRow(QStringLiteral("Stabilization:"),
                                       stabilizationCheckBox);
                    solverLayout->addWidget(solverSettingsGroup);
                    solverLayout->addWidget(parameterPanel);
                    solverLayout->addStretch(1);
                    flowTabs->addTab(solverTab,
                                     QStringLiteral("Solver Parameters"));

                    auto* runTab = new QWidget(flowTabs);
                    auto* runLayout = new QVBoxLayout(runTab);
                    auto* procLayout = new QHBoxLayout();
                    procLayout->addWidget(
                        new QLabel(QStringLiteral("Num Processors:"),
                                   runTab));
                    auto* numProcessorsSpin = new QSpinBox(runTab);
                    numProcessorsSpin->setObjectName(
                        QStringLiteral("xqFlowNumProcessorsSpinBox"));
                    numProcessorsSpin->setRange(1, 256);
                    numProcessorsSpin->setValue(1);
                    procLayout->addWidget(numProcessorsSpin, 1);
                    runLayout->addLayout(procLayout);

                    auto* runButtonLayout = new QHBoxLayout();
                    auto* configureJobButton =
                        new QPushButton(QStringLiteral("Configure CFD Job"),
                                        runTab);
                    configureJobButton->setObjectName(
                        QStringLiteral("xqFlowConfigureJobButton"));
                    configureJobButton->setCheckable(true);
                    configureJobButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("configure-cfd-job"));
                    auto* steadyFlowButton =
                        new QPushButton(QStringLiteral("Steady Flow Solve"),
                                        runTab);
                    steadyFlowButton->setObjectName(
                        QStringLiteral("xqFlowSteadyFlowButton"));
                    steadyFlowButton->setCheckable(true);
                    steadyFlowButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("run-steady-flow"));
                    auto* runSimulationButton =
                        new QPushButton(QStringLiteral("Run Simulation"),
                                        runTab);
                    runSimulationButton->setObjectName(
                        QStringLiteral("xqFlowRunSimulationButton"));
                    runSimulationButton->setEnabled(false);
                    auto* stopSimulationButton =
                        new QPushButton(QStringLiteral("Stop"), runTab);
                    stopSimulationButton->setObjectName(
                        QStringLiteral("xqFlowStopSimulationButton"));
                    stopSimulationButton->setEnabled(false);
                    runButtonLayout->addWidget(configureJobButton);
                    runButtonLayout->addWidget(steadyFlowButton);
                    runButtonLayout->addWidget(runSimulationButton);
                    runButtonLayout->addWidget(stopSimulationButton);
                    runLayout->addLayout(runButtonLayout);

                    auto* exportButtonLayout = new QHBoxLayout();
                    auto* exportOnlyButton =
                        new QPushButton(QStringLiteral("Export Only"),
                                        runTab);
                    exportOnlyButton->setObjectName(
                        QStringLiteral("xqFlowExportOnlyButton"));
                    exportOnlyButton->setEnabled(false);
                    auto* exportAndRunButton =
                        new QPushButton(QStringLiteral("Export and Run"),
                                        runTab);
                    exportAndRunButton->setObjectName(
                        QStringLiteral("xqFlowExportAndRunButton"));
                    exportAndRunButton->setEnabled(false);
                    auto* exportResultsButton =
                        new QPushButton(QStringLiteral("Export Results..."),
                                        runTab);
                    exportResultsButton->setObjectName(
                        QStringLiteral("xqFlowExportResultsButton"));
                    exportResultsButton->setEnabled(false);
                    exportButtonLayout->addWidget(exportOnlyButton);
                    exportButtonLayout->addWidget(exportAndRunButton);
                    exportButtonLayout->addWidget(exportResultsButton);
                    exportButtonLayout->addStretch(1);
                    runLayout->addLayout(exportButtonLayout);
                    auto* progressBar = new QProgressBar(runTab);
                    progressBar->setObjectName(
                        QStringLiteral("xqFlowProgressBar"));
                    progressBar->setValue(0);
                    auto* logText = new QTextEdit(runTab);
                    logText->setObjectName(
                        QStringLiteral("xqFlowLogTextEdit"));
                    logText->setReadOnly(true);
                    logText->setMinimumHeight(150);
                    runLayout->addWidget(progressBar);
                    runLayout->addWidget(logText);
                    flowTabs->addTab(runTab, QStringLiteral("Run"));

                    auto* resultsTab = new QWidget(flowTabs);
                    auto* resultsLayout = new QVBoxLayout(resultsTab);
                    auto* resultFieldLayout = new QHBoxLayout();
                    resultFieldLayout->addWidget(
                        new QLabel(QStringLiteral("Result Field:"),
                                   resultsTab));
                    auto* resultFieldCombo = new QComboBox(resultsTab);
                    resultFieldCombo->setObjectName(
                        QStringLiteral("xqFlowResultFieldCombo"));
                    resultFieldCombo->addItems(
                        {QStringLiteral("Pressure"),
                         QStringLiteral("Velocity Magnitude"),
                         QStringLiteral("Wall Shear Stress"),
                         QStringLiteral("Vorticity"),
                         QStringLiteral("Oscillatory Shear Index")});
                    resultFieldLayout->addWidget(resultFieldCombo, 1);
                    resultsLayout->addLayout(resultFieldLayout);
                    auto* scalarRangeGroup =
                        new QGroupBox(QStringLiteral("Scalar Range"),
                                      resultsTab);
                    scalarRangeGroup->setObjectName(
                        QStringLiteral("xqFlowScalarRangeGroup"));
                    auto* rangeForm = new QFormLayout(scalarRangeGroup);
                    auto* resultMinSpin =
                        new QDoubleSpinBox(scalarRangeGroup);
                    resultMinSpin->setObjectName(
                        QStringLiteral("xqFlowResultMinSpinBox"));
                    resultMinSpin->setRange(-1.0e12, 1.0e12);
                    resultMinSpin->setDecimals(2);
                    resultMinSpin->setValue(0.0);
                    auto* resultMaxSpin =
                        new QDoubleSpinBox(scalarRangeGroup);
                    resultMaxSpin->setObjectName(
                        QStringLiteral("xqFlowResultMaxSpinBox"));
                    resultMaxSpin->setRange(-1.0e12, 1.0e12);
                    resultMaxSpin->setDecimals(2);
                    resultMaxSpin->setValue(100.0);
                    auto* autoRangeCheckBox =
                        new QCheckBox(QStringLiteral("Auto Range"),
                                      scalarRangeGroup);
                    autoRangeCheckBox->setObjectName(
                        QStringLiteral("xqFlowAutoRangeCheckBox"));
                    autoRangeCheckBox->setChecked(true);
                    rangeForm->addRow(QStringLiteral("Range Min:"),
                                      resultMinSpin);
                    rangeForm->addRow(QStringLiteral("Range Max:"),
                                      resultMaxSpin);
                    rangeForm->addRow(autoRangeCheckBox);
                    resultsLayout->addWidget(scalarRangeGroup);
                    auto* colorMapLayout = new QHBoxLayout();
                    colorMapLayout->addWidget(
                        new QLabel(QStringLiteral("Color Map:"),
                                   resultsTab));
                    auto* colorMapCombo = new QComboBox(resultsTab);
                    colorMapCombo->setObjectName(
                        QStringLiteral("xqFlowColorMapCombo"));
                    colorMapCombo->addItems(
                        {QStringLiteral("Rainbow"),
                         QStringLiteral("Cool-Warm"),
                         QStringLiteral("Grayscale"),
                         QStringLiteral("Red-Blue"),
                         QStringLiteral("Viridis")});
                    colorMapLayout->addWidget(colorMapCombo, 1);
                    resultsLayout->addLayout(colorMapLayout);
                    auto* applyColorMapButton =
                        new QPushButton(QStringLiteral("Apply Color Map"),
                                        resultsTab);
                    applyColorMapButton->setObjectName(
                        QStringLiteral("xqFlowApplyColorMapButton"));
                    auto* showLegendButton =
                        new QPushButton(QStringLiteral("Show/Hide Legend"),
                                        resultsTab);
                    showLegendButton->setObjectName(
                        QStringLiteral("xqFlowShowLegendButton"));
                    showLegendButton->setCheckable(true);
                    auto* timeStepLayout = new QHBoxLayout();
                    timeStepLayout->addWidget(
                        new QLabel(QStringLiteral("Time Step:"),
                                   resultsTab));
                    auto* timeStepSpin = new QSpinBox(resultsTab);
                    timeStepSpin->setObjectName(
                        QStringLiteral("xqFlowTimeStepSpinBox"));
                    timeStepSpin->setRange(0, 10000);
                    timeStepSpin->setValue(0);
                    timeStepLayout->addWidget(timeStepSpin, 1);
                    auto* animateButton =
                        new QPushButton(QStringLiteral("Animate"),
                                        resultsTab);
                    animateButton->setObjectName(
                        QStringLiteral("xqFlowAnimateButton"));
                    animateButton->setCheckable(true);
                    auto* resultSummary = new QTextEdit(resultsTab);
                    resultSummary->setObjectName(
                        QStringLiteral("xqFlowResultSummaryTextEdit"));
                    resultSummary->setReadOnly(true);
                    resultSummary->setMaximumHeight(100);
                    auto* reviewResultsButton =
                        new QPushButton(QStringLiteral("Review Results"),
                                        resultsTab);
                    reviewResultsButton->setObjectName(
                        QStringLiteral("xqFlowReviewResultsButton"));
                    reviewResultsButton->setCheckable(true);
                    reviewResultsButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("review-flow-results"));
                    resultsLayout->addWidget(applyColorMapButton);
                    resultsLayout->addWidget(showLegendButton);
                    resultsLayout->addLayout(timeStepLayout);
                    resultsLayout->addWidget(animateButton);
                    resultsLayout->addWidget(reviewResultsButton);
                    resultsLayout->addWidget(resultSummary);
                    resultsLayout->addStretch(1);
                    flowTabs->addTab(resultsTab, QStringLiteral("Results"));

                    layout->addWidget(flowTabs);

                    auto* toolButtons = new QButtonGroup(page);
                    toolButtons->setExclusive(true);
                    toolButtons->addButton(configureJobButton);
                    toolButtons->addButton(steadyFlowButton);
                    toolButtons->addButton(reviewResultsButton);

                    auto connectFlowButton =
                        [this](QPushButton* button,
                               const QString& operationId) {
                            connect(button,
                                    &QPushButton::clicked,
                                    this,
                                    [this, operationId]() {
                                        QString message;
                                        if (!m_Context.WorkflowOperations()
                                                 ->SelectOperation(
                                                     QStringLiteral(
                                                         "flow-simulation"),
                                                     operationId,
                                                     &message))
                                        {
                                            m_Context.PostDiagnostic(message);
                                        }
                                        UpdateWorkflowOperationControls();
                                    });
                        };
                    connectFlowButton(configureJobButton,
                                      QStringLiteral("configure-cfd-job"));
                    connectFlowButton(steadyFlowButton,
                                      QStringLiteral("run-steady-flow"));
                    connectFlowButton(reviewResultsButton,
                                      QStringLiteral("review-flow-results"));
                }
                else if (id == QStringLiteral("python-api"))
                {
                    operationSelector->setVisible(false);
                    layout->addWidget(operationSelector);

                    auto* runtimeGroup =
                        new QGroupBox(QStringLiteral("Runtime Status"),
                                      page);
                    runtimeGroup->setObjectName(
                        QStringLiteral("xqPythonApiRuntimeGroup"));
                    auto* runtimeLayout = new QVBoxLayout(runtimeGroup);
                    runtimeLayout->setContentsMargins(8, 8, 8, 8);
                    runtimeLayout->setSpacing(6);
                    auto* runtimeStatus =
                        new QLabel(QStringLiteral(
                            "Python API unavailable in this build. The C++ inspection service remains available for tests and internal callers."),
                                   runtimeGroup);
                    runtimeStatus->setObjectName(QStringLiteral(
                        "xqPythonApiRuntimeStatusLabel"));
                    runtimeStatus->setWordWrap(true);
                    auto* consoleButton =
                        new QPushButton(QStringLiteral("Check Runtime"),
                                        runtimeGroup);
                    consoleButton->setObjectName(QStringLiteral(
                        "xqPythonApiConsoleButton"));
                    runtimeLayout->addWidget(runtimeStatus);
                    runtimeLayout->addWidget(consoleButton);
                    layout->addWidget(runtimeGroup);

                    auto* snippetGroup =
                        new QGroupBox(QStringLiteral("Snippet Catalog"),
                                      page);
                    snippetGroup->setObjectName(
                        QStringLiteral("xqPythonApiSnippetGroup"));
                    auto* snippetLayout = new QVBoxLayout(snippetGroup);
                    snippetLayout->setContentsMargins(8, 8, 8, 8);
                    snippetLayout->setSpacing(6);
                    auto* snippetSummary =
                        new QLabel(QStringLiteral(
                            "Export deterministic examples for xq.version(), xq.list_nodes(), xq.find_node(name), and related metadata inspection calls."),
                                   snippetGroup);
                    snippetSummary->setObjectName(QStringLiteral(
                        "xqPythonApiSnippetSummaryLabel"));
                    snippetSummary->setWordWrap(true);
                    auto* snippetButton =
                        new QPushButton(QStringLiteral("Export Snippets"),
                                        snippetGroup);
                    snippetButton->setObjectName(QStringLiteral(
                        "xqPythonApiSnippetButton"));
                    snippetLayout->addWidget(snippetSummary);
                    snippetLayout->addWidget(snippetButton);
                    layout->addWidget(snippetGroup);

                    auto* scriptGroup =
                        new QGroupBox(QStringLiteral("Project Script Runner"),
                                      page);
                    scriptGroup->setObjectName(
                        QStringLiteral("xqPythonApiScriptGroup"));
                    auto* scriptLayout = new QVBoxLayout(scriptGroup);
                    scriptLayout->setContentsMargins(8, 8, 8, 8);
                    scriptLayout->setSpacing(6);
                    auto* scriptNotice =
                        new QLabel(QStringLiteral(
                            "Python project script runtime is unavailable in Windows v1 until pybind11 and a matching Python 3.11 ABI are linked."),
                                   scriptGroup);
                    scriptNotice->setObjectName(QStringLiteral(
                        "xqPythonApiScriptUnavailableNotice"));
                    scriptNotice->setWordWrap(true);
                    auto* scriptButton =
                        new QPushButton(QStringLiteral("Run Project Script"),
                                        scriptGroup);
                    scriptButton->setObjectName(QStringLiteral(
                        "xqPythonApiScriptButton"));
                    scriptLayout->addWidget(scriptNotice);
                    scriptLayout->addWidget(scriptButton);
                    layout->addWidget(scriptGroup);

                    auto selectPythonOperation =
                        [this](const QString& operationId) {
                            QString message;
                            if (!m_Context.WorkflowOperations()->SelectOperation(
                                    QStringLiteral("python-api"),
                                    operationId,
                                    &message))
                            {
                                m_Context.PostDiagnostic(message);
                            }
                            UpdateWorkflowOperationControls();
                        };
                    connect(consoleButton,
                            &QPushButton::clicked,
                            this,
                            [this, selectPythonOperation]() {
                                selectPythonOperation(
                                    QStringLiteral("open-python-console"));
                                RunActiveWorkflowAction();
                            });
                    connect(snippetButton,
                            &QPushButton::clicked,
                            this,
                            [this, selectPythonOperation]() {
                                selectPythonOperation(
                                    QStringLiteral("export-api-snippet"));
                                RunActiveWorkflowAction();
                            });
                    connect(scriptButton,
                            &QPushButton::clicked,
                            this,
                            [this, selectPythonOperation]() {
                                selectPythonOperation(
                                    QStringLiteral("run-project-script"));
                                RunActiveWorkflowAction();
                            });

                    layout->addWidget(parameterPanel);
                }
                else if (id == QStringLiteral("rom-simulation"))
                {
                    operationSelector->setVisible(false);
                    layout->addWidget(operationSelector);

                    auto* statusLabel = new QLabel(
                        QStringLiteral(
                            "ROM Simulation\n\n"
                            "No ROM job selected.\n\n"
                            "Select or double-click an xq_MitkROMJob node to "
                            "bind it here. Opening this view does not export "
                            "files or run a solver."),
                        page);
                    statusLabel->setObjectName(
                        QStringLiteral("xqRomStatusLabel"));
                    statusLabel->setWordWrap(true);
                    layout->addWidget(statusLabel);

                    auto* stepsText = new QTextEdit(page);
                    stepsText->setObjectName(
                        QStringLiteral("xqRomWorkflowStepsText"));
                    stepsText->setReadOnly(true);
                    stepsText->setMaximumHeight(170);
                    stepsText->setPlainText(
                        QStringLiteral(
                            "Reduced-Order Model (ROM) simulation workflow.\n\n"
                            "Data model: xq_ROMSimulationJob (ready)\n"
                            "XML export: xq_ROMSimJobXmlWriter (ready)\n\n"
                            "GUI workflow steps (native scope):\n"
                            "  1. Select model + centerline\n"
                            "  2. Configure ROM mesh parameters\n"
                            "  3. Set boundary conditions\n"
                            "  4. Save/load ROM job metadata"));
                    layout->addWidget(stepsText);

                    auto* operationsGroup =
                        new QGroupBox(QStringLiteral("ROM Configuration"),
                                      page);
                    operationsGroup->setObjectName(
                        QStringLiteral("xqRomConfigurationGroup"));
                    auto* operationsLayout =
                        new QVBoxLayout(operationsGroup);
                    auto* operationButtonLayout = new QHBoxLayout();
                    auto* buildNetworkButton =
                        new QPushButton(QStringLiteral("Build 1D Network"),
                                        operationsGroup);
                    buildNetworkButton->setObjectName(
                        QStringLiteral("xqRomBuildNetworkButton"));
                    buildNetworkButton->setCheckable(true);
                    buildNetworkButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("build-1d-network"));
                    auto* calibrateBoundaryButton =
                        new QPushButton(QStringLiteral(
                                            "Calibrate Boundary Conditions"),
                                        operationsGroup);
                    calibrateBoundaryButton->setObjectName(QStringLiteral(
                        "xqRomCalibrateBoundaryButton"));
                    calibrateBoundaryButton->setCheckable(true);
                    calibrateBoundaryButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("calibrate-boundary-conditions"));
                    operationButtonLayout->addWidget(buildNetworkButton);
                    operationButtonLayout->addWidget(calibrateBoundaryButton);
                    operationsLayout->addLayout(operationButtonLayout);
                    operationsLayout->addWidget(parameterPanel);
                    layout->addWidget(operationsGroup);

                    auto* exportButton =
                        new QPushButton(QStringLiteral("Export ROM Metadata"),
                                        page);
                    exportButton->setObjectName(
                        QStringLiteral("xqRomExportMetadataButton"));
                    exportButton->setEnabled(false);
                    layout->addWidget(exportButton);

                    auto* solverNotice = new QLabel(
                        QStringLiteral(
                            "Runtime status: native ROM solver execution is "
                            "unavailable and disabled. Windows v1 configures "
                            "jobs and calibrates metadata only."),
                        page);
                    solverNotice->setObjectName(
                        QStringLiteral("xqRomSolverNoticeLabel"));
                    solverNotice->setWordWrap(true);
                    layout->addWidget(solverNotice);

                    auto* toolButtons = new QButtonGroup(page);
                    toolButtons->setExclusive(true);
                    toolButtons->addButton(buildNetworkButton);
                    toolButtons->addButton(calibrateBoundaryButton);

                    auto connectRomButton =
                        [this](QPushButton* button,
                               const QString& operationId) {
                            connect(button,
                                    &QPushButton::clicked,
                                    this,
                                    [this, operationId]() {
                                        QString message;
                                        if (!m_Context.WorkflowOperations()
                                                 ->SelectOperation(
                                                     QStringLiteral(
                                                         "rom-simulation"),
                                                     operationId,
                                                     &message))
                                        {
                                            m_Context.PostDiagnostic(message);
                                        }
                                        UpdateWorkflowOperationControls();
                                    });
                        };
                    connectRomButton(buildNetworkButton,
                                     QStringLiteral("build-1d-network"));
                    connectRomButton(
                        calibrateBoundaryButton,
                        QStringLiteral("calibrate-boundary-conditions"));
                }
                else if (id == QStringLiteral("multiphysics"))
                {
                    operationSelector->setVisible(false);
                    layout->addWidget(operationSelector);

                    auto* statusLabel = new QLabel(
                        QStringLiteral(
                            "Multi-Physics Simulation\n\n"
                            "No MultiPhysics job selected.\n\n"
                            "Select or double-click an "
                            "xq_MitkMultiPhysicsJob node to bind it here. "
                            "Opening this view does not export XML or run a "
                            "solver."),
                        page);
                    statusLabel->setObjectName(
                        QStringLiteral("xqMultiPhysicsStatusLabel"));
                    statusLabel->setWordWrap(true);
                    layout->addWidget(statusLabel);

                    auto* stepsText = new QTextEdit(page);
                    stepsText->setObjectName(QStringLiteral(
                        "xqMultiPhysicsWorkflowStepsText"));
                    stepsText->setReadOnly(true);
                    stepsText->setMaximumHeight(190);
                    stepsText->setPlainText(
                        QStringLiteral(
                            "Coupled multi-physics simulation workflow.\n\n"
                            "Data model: xq_MultiPhysicsJob (ready)\n"
                            "XML export: xq_MultiPhysicsXmlWriter (ready)\n\n"
                            "GUI workflow steps (native scope):\n"
                            "  1. Define computational domains\n"
                            "  2. Assign material properties per domain\n"
                            "  3. Configure equations and solver settings\n"
                            "  4. Set boundary conditions with parameters\n"
                            "  5. Save/load MultiPhysics XML metadata"));
                    layout->addWidget(stepsText);

                    auto* operationsGroup =
                        new QGroupBox(QStringLiteral(
                                          "MultiPhysics Configuration"),
                                      page);
                    operationsGroup->setObjectName(QStringLiteral(
                        "xqMultiPhysicsConfigurationGroup"));
                    auto* operationsLayout =
                        new QVBoxLayout(operationsGroup);
                    auto* operationButtonLayout = new QHBoxLayout();
                    auto* configureCouplingButton =
                        new QPushButton(QStringLiteral("Configure Coupling"),
                                        operationsGroup);
                    configureCouplingButton->setObjectName(QStringLiteral(
                        "xqMultiPhysicsConfigureCouplingButton"));
                    configureCouplingButton->setCheckable(true);
                    configureCouplingButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("configure-coupling"));
                    auto* reviewResultsButton =
                        new QPushButton(QStringLiteral(
                                            "Review Coupled Results"),
                                        operationsGroup);
                    reviewResultsButton->setObjectName(QStringLiteral(
                        "xqMultiPhysicsReviewResultsButton"));
                    reviewResultsButton->setCheckable(true);
                    reviewResultsButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("review-coupled-results"));
                    operationButtonLayout->addWidget(configureCouplingButton);
                    operationButtonLayout->addWidget(reviewResultsButton);
                    operationsLayout->addLayout(operationButtonLayout);
                    operationsLayout->addWidget(parameterPanel);
                    layout->addWidget(operationsGroup);

                    auto* exportButton =
                        new QPushButton(QStringLiteral(
                                            "Export MultiPhysics Metadata"),
                                        page);
                    exportButton->setObjectName(QStringLiteral(
                        "xqMultiPhysicsExportMetadataButton"));
                    exportButton->setEnabled(false);
                    layout->addWidget(exportButton);

                    auto* solverNotice = new QLabel(
                        QStringLiteral(
                            "Runtime status: native coupled solver execution "
                            "is unavailable and disabled. Windows v1 "
                            "configures coupling and reviews imported results "
                            "only."),
                        page);
                    solverNotice->setObjectName(QStringLiteral(
                        "xqMultiPhysicsSolverNoticeLabel"));
                    solverNotice->setWordWrap(true);
                    layout->addWidget(solverNotice);

                    auto* toolButtons = new QButtonGroup(page);
                    toolButtons->setExclusive(true);
                    toolButtons->addButton(configureCouplingButton);
                    toolButtons->addButton(reviewResultsButton);

                    auto connectMultiPhysicsButton =
                        [this](QPushButton* button,
                               const QString& operationId) {
                            connect(button,
                                    &QPushButton::clicked,
                                    this,
                                    [this, operationId]() {
                                        QString message;
                                        if (!m_Context.WorkflowOperations()
                                                 ->SelectOperation(
                                                     QStringLiteral(
                                                         "multiphysics"),
                                                     operationId,
                                                     &message))
                                        {
                                            m_Context.PostDiagnostic(message);
                                        }
                                        UpdateWorkflowOperationControls();
                                    });
                        };
                    connectMultiPhysicsButton(
                        configureCouplingButton,
                        QStringLiteral("configure-coupling"));
                    connectMultiPhysicsButton(
                        reviewResultsButton,
                        QStringLiteral("review-coupled-results"));
                }
                else if (id == QStringLiteral("modeling"))
                {
                    operationSelector->setVisible(false);
                    layout->addWidget(operationSelector);

                    auto* selectorLayout = new QHBoxLayout();
                    auto* modelLabel =
                        new QLabel(QStringLiteral("Model:"), page);
                    modelLabel->setObjectName(
                        QStringLiteral("xqModelingModelLabel"));
                    auto* modelSelector = new QComboBox(page);
                    modelSelector->setObjectName(
                        QStringLiteral("xqModelingModelSelector"));
                    selectorLayout->addWidget(modelLabel);
                    selectorLayout->addWidget(modelSelector, 1);
                    layout->addLayout(selectorLayout);

                    auto* facesTable = new QTableWidget(page);
                    facesTable->setObjectName(
                        QStringLiteral("xqModelingFacesTable"));
                    facesTable->setMinimumHeight(150);
                    facesTable->setSelectionMode(
                        QAbstractItemView::SingleSelection);
                    facesTable->setSelectionBehavior(
                        QAbstractItemView::SelectRows);
                    facesTable->setColumnCount(3);
                    facesTable->setHorizontalHeaderLabels(
                        {QStringLiteral("Face"),
                         QStringLiteral("Type"),
                         QStringLiteral("Visible")});
                    layout->addWidget(facesTable);

                    auto* operationTabs = new QTabWidget(page);
                    operationTabs->setObjectName(
                        QStringLiteral("xqModelingOperationTabs"));

                    auto* createTab = new QWidget(operationTabs);
                    auto* createLayout = new QVBoxLayout(createTab);
                    auto* loftButton =
                        new QPushButton(QStringLiteral("Create Model..."),
                                        createTab);
                    loftButton->setObjectName(
                        QStringLiteral("xqModelingLoftSurfaceButton"));
                    loftButton->setCheckable(true);
                    loftButton->setProperty("xqOperationId",
                                            QStringLiteral("loft-surface"));
                    auto* deleteModelButton =
                        new QPushButton(QStringLiteral("Delete Model"),
                                        createTab);
                    deleteModelButton->setObjectName(
                        QStringLiteral("xqModelingDeleteModelButton"));
                    deleteModelButton->setEnabled(false);
                    createLayout->addWidget(loftButton);
                    createLayout->addWidget(deleteModelButton);
                    createLayout->addStretch(1);
                    operationTabs->addTab(createTab, QStringLiteral("Create"));

                    auto* editTab = new QWidget(operationTabs);
                    auto* editLayout = new QVBoxLayout(editTab);
                    auto* faceOpsGroup =
                        new QGroupBox(QStringLiteral("Face Operations"),
                                      editTab);
                    faceOpsGroup->setObjectName(
                        QStringLiteral("xqModelingFaceOperationsGroup"));
                    auto* faceOpsLayout = new QVBoxLayout(faceOpsGroup);
                    auto* buildSolidButton =
                        new QPushButton(QStringLiteral("Build Solid Model"),
                                        faceOpsGroup);
                    buildSolidButton->setObjectName(QStringLiteral(
                        "xqModelingBuildSolidModelButton"));
                    buildSolidButton->setCheckable(true);
                    buildSolidButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("build-solid-model"));
                    auto* trimBranchesButton =
                        new QPushButton(QStringLiteral("Trim Branches"),
                                        faceOpsGroup);
                    trimBranchesButton->setObjectName(
                        QStringLiteral("xqModelingTrimBranchesButton"));
                    trimBranchesButton->setCheckable(true);
                    trimBranchesButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("trim-branches"));
                    faceOpsLayout->addWidget(buildSolidButton);
                    faceOpsLayout->addWidget(trimBranchesButton);
                    editLayout->addWidget(faceOpsGroup);
                    editLayout->addWidget(parameterPanel);
                    editLayout->addStretch(1);
                    operationTabs->addTab(editTab, QStringLiteral("Edit"));

                    auto* exportTab = new QWidget(operationTabs);
                    auto* exportLayout = new QVBoxLayout(exportTab);
                    auto* exportButton =
                        new QPushButton(QStringLiteral("Export Model (VTP/STL)..."),
                                        exportTab);
                    exportButton->setObjectName(
                        QStringLiteral("xqModelingExportModelButton"));
                    exportButton->setEnabled(false);
                    auto* statsButton =
                        new QPushButton(QStringLiteral("Model Statistics"),
                                        exportTab);
                    statsButton->setObjectName(
                        QStringLiteral("xqModelingModelStatsButton"));
                    statsButton->setEnabled(false);
                    exportLayout->addWidget(exportButton);
                    exportLayout->addWidget(statsButton);
                    exportLayout->addStretch(1);
                    operationTabs->addTab(exportTab, QStringLiteral("Export"));

                    layout->addWidget(operationTabs);

                    auto* toolButtons = new QButtonGroup(page);
                    toolButtons->setExclusive(true);
                    toolButtons->addButton(loftButton);
                    toolButtons->addButton(buildSolidButton);
                    toolButtons->addButton(trimBranchesButton);

                    auto connectModelingButton =
                        [this](QPushButton* button,
                               const QString& operationId) {
                            connect(button,
                                    &QPushButton::clicked,
                                    this,
                                    [this, operationId]() {
                                        QString message;
                                        if (!m_Context.WorkflowOperations()
                                                 ->SelectOperation(
                                                     QStringLiteral("modeling"),
                                                     operationId,
                                                     &message))
                                        {
                                            m_Context.PostDiagnostic(message);
                                        }
                                        UpdateWorkflowOperationControls();
                                    });
                        };
                    connectModelingButton(loftButton,
                                          QStringLiteral("loft-surface"));
                    connectModelingButton(buildSolidButton,
                                          QStringLiteral("build-solid-model"));
                    connectModelingButton(trimBranchesButton,
                                          QStringLiteral("trim-branches"));
                }
                else if (id == QStringLiteral("meshing"))
                {
                    operationSelector->setVisible(false);
                    layout->addWidget(operationSelector);

                    auto* selectorLayout = new QHBoxLayout();
                    auto* modelLabel =
                        new QLabel(QStringLiteral("Model:"), page);
                    modelLabel->setObjectName(
                        QStringLiteral("xqMeshingModelLabel"));
                    auto* modelSelector = new QComboBox(page);
                    modelSelector->setObjectName(
                        QStringLiteral("xqMeshingModelSelector"));
                    auto* newMeshButton =
                        new QPushButton(QStringLiteral("New Mesh..."),
                                        page);
                    newMeshButton->setObjectName(
                        QStringLiteral("xqMeshingNewMeshButton"));
                    newMeshButton->setEnabled(false);
                    selectorLayout->addWidget(modelLabel);
                    selectorLayout->addWidget(modelSelector, 1);
                    selectorLayout->addWidget(newMeshButton);
                    layout->addLayout(selectorLayout);

                    auto* meshingTabs = new QTabWidget(page);
                    meshingTabs->setObjectName(
                        QStringLiteral("xqMeshingTabs"));

                    auto* globalTab = new QWidget(meshingTabs);
                    auto* globalLayout = new QVBoxLayout(globalTab);
                    auto* globalGroup =
                        new QGroupBox(QStringLiteral("Global Mesh Parameters"),
                                      globalTab);
                    globalGroup->setObjectName(QStringLiteral(
                        "xqMeshingGlobalParamsGroup"));
                    auto* globalForm = new QFormLayout(globalGroup);
                    auto* meshTypeCombo = new QComboBox(globalGroup);
                    meshTypeCombo->setObjectName(
                        QStringLiteral("xqMeshingMeshTypeCombo"));
                    meshTypeCombo->addItem(QStringLiteral("TetGen"));
                    auto* globalEdgeSpin =
                        new QDoubleSpinBox(globalGroup);
                    globalEdgeSpin->setObjectName(QStringLiteral(
                        "xqMeshingGlobalEdgeSizeSpinBox"));
                    globalEdgeSpin->setRange(0.001, 1000.0);
                    globalEdgeSpin->setDecimals(3);
                    globalEdgeSpin->setSingleStep(0.1);
                    globalEdgeSpin->setValue(1.0);
                    globalEdgeSpin->setEnabled(false);
                    globalForm->addRow(QStringLiteral("Mesh Type:"),
                                       meshTypeCombo);
                    globalForm->addRow(QStringLiteral("Global Edge Size:"),
                                       globalEdgeSpin);
                    globalLayout->addWidget(globalGroup);

                    auto* globalOperationLayout = new QHBoxLayout();
                    auto* surfaceMeshButton =
                        new QPushButton(QStringLiteral("Generate Surface Mesh"),
                                        globalTab);
                    surfaceMeshButton->setObjectName(QStringLiteral(
                        "xqMeshingGenerateSurfaceMeshButton"));
                    surfaceMeshButton->setCheckable(true);
                    surfaceMeshButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("generate-surface-mesh"));
                    auto* volumeMeshButton =
                        new QPushButton(QStringLiteral("Generate Volume Mesh"),
                                        globalTab);
                    volumeMeshButton->setObjectName(QStringLiteral(
                        "xqMeshingGenerateVolumeMeshButton"));
                    volumeMeshButton->setCheckable(true);
                    volumeMeshButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("generate-volume-mesh"));
                    globalOperationLayout->addWidget(surfaceMeshButton);
                    globalOperationLayout->addWidget(volumeMeshButton);
                    globalLayout->addLayout(globalOperationLayout);
                    globalLayout->addWidget(parameterPanel);
                    globalLayout->addStretch(1);
                    meshingTabs->addTab(globalTab,
                                        QStringLiteral("Global Settings"));

                    auto* localTab = new QWidget(meshingTabs);
                    auto* localLayout = new QVBoxLayout(localTab);
                    auto* localSizeTable = new QTableWidget(localTab);
                    localSizeTable->setObjectName(
                        QStringLiteral("xqMeshingLocalSizeTable"));
                    localSizeTable->setMinimumHeight(150);
                    localSizeTable->setSelectionBehavior(
                        QAbstractItemView::SelectRows);
                    localSizeTable->setColumnCount(3);
                    localSizeTable->setHorizontalHeaderLabels(
                        {QStringLiteral("Face Name"),
                         QStringLiteral("Type"),
                         QStringLiteral("Edge Size")});
                    localSizeTable->horizontalHeader()->setStretchLastSection(
                        true);
                    localLayout->addWidget(localSizeTable);
                    auto* localButtonLayout = new QHBoxLayout();
                    auto* addLocalSizeButton =
                        new QPushButton(QStringLiteral("Add"), localTab);
                    addLocalSizeButton->setObjectName(
                        QStringLiteral("xqMeshingAddLocalSizeButton"));
                    addLocalSizeButton->setEnabled(false);
                    auto* removeLocalSizeButton =
                        new QPushButton(QStringLiteral("Remove"), localTab);
                    removeLocalSizeButton->setObjectName(
                        QStringLiteral("xqMeshingRemoveLocalSizeButton"));
                    removeLocalSizeButton->setEnabled(false);
                    localButtonLayout->addWidget(addLocalSizeButton);
                    localButtonLayout->addWidget(removeLocalSizeButton);
                    localButtonLayout->addStretch(1);
                    localLayout->addLayout(localButtonLayout);
                    meshingTabs->addTab(localTab,
                                        QStringLiteral("Local Size"));

                    auto* boundaryTab = new QWidget(meshingTabs);
                    auto* boundaryLayout = new QVBoxLayout(boundaryTab);
                    auto* boundaryLayerCheckBox =
                        new QCheckBox(QStringLiteral(
                                          "Enable Boundary Layer Mesh"),
                                      boundaryTab);
                    boundaryLayerCheckBox->setObjectName(QStringLiteral(
                        "xqMeshingBoundaryLayerCheckBox"));
                    auto* boundaryLayerGroup =
                        new QGroupBox(QStringLiteral(
                                          "Boundary Layer Parameters"),
                                      boundaryTab);
                    boundaryLayerGroup->setObjectName(QStringLiteral(
                        "xqMeshingBoundaryLayerParamsGroup"));
                    boundaryLayerGroup->setEnabled(false);
                    auto* boundaryForm = new QFormLayout(boundaryLayerGroup);
                    auto* layerCountSpin = new QSpinBox(boundaryLayerGroup);
                    layerCountSpin->setObjectName(
                        QStringLiteral("xqMeshingLayerCountSpinBox"));
                    layerCountSpin->setRange(1, 20);
                    layerCountSpin->setValue(4);
                    auto* firstHeightSpin =
                        new QDoubleSpinBox(boundaryLayerGroup);
                    firstHeightSpin->setObjectName(QStringLiteral(
                        "xqMeshingFirstLayerHeightSpinBox"));
                    firstHeightSpin->setRange(0.001, 10.0);
                    firstHeightSpin->setDecimals(4);
                    firstHeightSpin->setSingleStep(0.01);
                    firstHeightSpin->setValue(0.1);
                    auto* growthRateSpin =
                        new QDoubleSpinBox(boundaryLayerGroup);
                    growthRateSpin->setObjectName(
                        QStringLiteral("xqMeshingGrowthRateSpinBox"));
                    growthRateSpin->setRange(1.0, 5.0);
                    growthRateSpin->setDecimals(2);
                    growthRateSpin->setSingleStep(0.1);
                    growthRateSpin->setValue(1.2);
                    auto* inwardCheckBox =
                        new QCheckBox(QStringLiteral("Direction Inward"),
                                      boundaryLayerGroup);
                    inwardCheckBox->setObjectName(QStringLiteral(
                        "xqMeshingBoundaryLayerDirectionInwardCheckBox"));
                    inwardCheckBox->setChecked(true);
                    boundaryForm->addRow(QStringLiteral("Number of Layers:"),
                                         layerCountSpin);
                    boundaryForm->addRow(QStringLiteral("First Layer Height:"),
                                         firstHeightSpin);
                    boundaryForm->addRow(QStringLiteral("Growth Rate:"),
                                         growthRateSpin);
                    boundaryForm->addRow(inwardCheckBox);
                    auto* boundaryLayerButton =
                        new QPushButton(QStringLiteral("Boundary Layers"),
                                        boundaryTab);
                    boundaryLayerButton->setObjectName(QStringLiteral(
                        "xqMeshingBoundaryLayersButton"));
                    boundaryLayerButton->setCheckable(true);
                    boundaryLayerButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("boundary-layers"));
                    auto* previewButton =
                        new QPushButton(QStringLiteral(
                                            "Preview Boundary Layer"),
                                        boundaryTab);
                    previewButton->setObjectName(QStringLiteral(
                        "xqMeshingBoundaryLayerPreviewButton"));
                    previewButton->setEnabled(false);
                    auto* statusLabel = new QLabel(boundaryTab);
                    statusLabel->setObjectName(
                        QStringLiteral("xqMeshingBoundaryLayerStatusLabel"));
                    boundaryLayout->addWidget(boundaryLayerCheckBox);
                    boundaryLayout->addWidget(boundaryLayerGroup);
                    boundaryLayout->addWidget(boundaryLayerButton);
                    boundaryLayout->addWidget(previewButton);
                    boundaryLayout->addWidget(statusLabel);
                    boundaryLayout->addStretch(1);
                    connect(boundaryLayerCheckBox,
                            &QCheckBox::toggled,
                            boundaryLayerGroup,
                            &QWidget::setEnabled);
                    connect(boundaryLayerCheckBox,
                            &QCheckBox::toggled,
                            previewButton,
                            &QWidget::setEnabled);
                    meshingTabs->addTab(boundaryTab,
                                        QStringLiteral("Boundary Layer"));

                    auto* refinementTab = new QWidget(meshingTabs);
                    auto* refinementLayout =
                        new QVBoxLayout(refinementTab);
                    auto* refinementTable =
                        new QTableWidget(refinementTab);
                    refinementTable->setObjectName(QStringLiteral(
                        "xqMeshingRefinementRegionsTable"));
                    refinementTable->setMinimumHeight(150);
                    refinementTable->setSelectionBehavior(
                        QAbstractItemView::SelectRows);
                    refinementTable->setColumnCount(7);
                    refinementTable->setHorizontalHeaderLabels(
                        {QStringLiteral("Name"),
                         QStringLiteral("Type"),
                         QStringLiteral("Center X"),
                         QStringLiteral("Center Y"),
                         QStringLiteral("Center Z"),
                         QStringLiteral("Radius/Size"),
                         QStringLiteral("Target Size")});
                    refinementTable->horizontalHeader()->setStretchLastSection(
                        true);
                    refinementLayout->addWidget(refinementTable);
                    auto* refinementButtonLayout = new QHBoxLayout();
                    auto* addSphereButton =
                        new QPushButton(QStringLiteral("Add Sphere"),
                                        refinementTab);
                    addSphereButton->setObjectName(
                        QStringLiteral("xqMeshingAddSphereRegionButton"));
                    auto* addCylinderButton =
                        new QPushButton(QStringLiteral("Add Cylinder"),
                                        refinementTab);
                    addCylinderButton->setObjectName(QStringLiteral(
                        "xqMeshingAddCylinderRegionButton"));
                    auto* removeRegionButton =
                        new QPushButton(QStringLiteral("Remove Selected"),
                                        refinementTab);
                    removeRegionButton->setObjectName(
                        QStringLiteral("xqMeshingRemoveRegionButton"));
                    refinementButtonLayout->addWidget(addSphereButton);
                    refinementButtonLayout->addWidget(addCylinderButton);
                    refinementButtonLayout->addWidget(removeRegionButton);
                    refinementLayout->addLayout(refinementButtonLayout);
                    auto* visualizeRegionsButton =
                        new QPushButton(QStringLiteral("Visualize Regions"),
                                        refinementTab);
                    visualizeRegionsButton->setObjectName(QStringLiteral(
                        "xqMeshingVisualizeRegionsButton"));
                    auto* regionCountLabel =
                        new QLabel(QStringLiteral(
                                       "0 refinement regions defined"),
                                   refinementTab);
                    regionCountLabel->setObjectName(QStringLiteral(
                        "xqMeshingRegionCountLabel"));
                    refinementLayout->addWidget(visualizeRegionsButton);
                    refinementLayout->addWidget(regionCountLabel);
                    refinementLayout->addStretch(1);
                    meshingTabs->addTab(refinementTab,
                                        QStringLiteral(
                                            "Refinement Regions"));

                    auto* advancedTab = new QWidget(meshingTabs);
                    auto* advancedLayout = new QVBoxLayout(advancedTab);
                    auto* statisticsGroup =
                        new QGroupBox(QStringLiteral("Mesh Statistics"),
                                      advancedTab);
                    statisticsGroup->setObjectName(
                        QStringLiteral("xqMeshingStatisticsGroup"));
                    auto* statisticsLayout =
                        new QVBoxLayout(statisticsGroup);
                    auto* elementsLabel =
                        new QLabel(QStringLiteral("Elements: --"),
                                   statisticsGroup);
                    elementsLabel->setObjectName(
                        QStringLiteral("xqMeshingElementsLabel"));
                    auto* nodesLabel =
                        new QLabel(QStringLiteral("Nodes: --"),
                                   statisticsGroup);
                    nodesLabel->setObjectName(
                        QStringLiteral("xqMeshingNodesLabel"));
                    auto* qualityLabel =
                        new QLabel(QStringLiteral("Quality: --"),
                                   statisticsGroup);
                    qualityLabel->setObjectName(
                        QStringLiteral("xqMeshingQualityLabel"));
                    auto* qualityReportButton =
                        new QPushButton(QStringLiteral("Quality Report"),
                                        statisticsGroup);
                    qualityReportButton->setObjectName(QStringLiteral(
                        "xqMeshingQualityReportButton"));
                    qualityReportButton->setEnabled(false);
                    auto* exportMeshButton =
                        new QPushButton(QStringLiteral("Export Mesh"),
                                        statisticsGroup);
                    exportMeshButton->setObjectName(
                        QStringLiteral("xqMeshingExportMeshButton"));
                    exportMeshButton->setEnabled(false);
                    statisticsLayout->addWidget(elementsLabel);
                    statisticsLayout->addWidget(nodesLabel);
                    statisticsLayout->addWidget(qualityLabel);
                    statisticsLayout->addWidget(qualityReportButton);
                    statisticsLayout->addWidget(exportMeshButton);
                    advancedLayout->addWidget(statisticsGroup);
                    advancedLayout->addStretch(1);
                    meshingTabs->addTab(advancedTab,
                                        QStringLiteral("Advanced"));

                    layout->addWidget(meshingTabs);

                    auto* toolButtons = new QButtonGroup(page);
                    toolButtons->setExclusive(true);
                    toolButtons->addButton(surfaceMeshButton);
                    toolButtons->addButton(volumeMeshButton);
                    toolButtons->addButton(boundaryLayerButton);

                    auto connectMeshingButton =
                        [this](QPushButton* button,
                               const QString& operationId) {
                            connect(button,
                                    &QPushButton::clicked,
                                    this,
                                    [this, operationId]() {
                                        QString message;
                                        if (!m_Context.WorkflowOperations()
                                                 ->SelectOperation(
                                                     QStringLiteral("meshing"),
                                                     operationId,
                                                     &message))
                                        {
                                            m_Context.PostDiagnostic(message);
                                        }
                                        UpdateWorkflowOperationControls();
                                    });
                        };
                    connectMeshingButton(
                        surfaceMeshButton,
                        QStringLiteral("generate-surface-mesh"));
                    connectMeshingButton(
                        volumeMeshButton,
                        QStringLiteral("generate-volume-mesh"));
                    connectMeshingButton(boundaryLayerButton,
                                         QStringLiteral("boundary-layers"));
                }
                else if (id == QStringLiteral("path"))
                {
                    operationSelector->setVisible(false);
                    layout->addWidget(operationSelector);

                    auto* pathsGroup =
                        new QGroupBox(QStringLiteral("Paths"), page);
                    pathsGroup->setObjectName(
                        QStringLiteral("xqPathPlanningPathsGroup"));
                    auto* pathsLayout = new QVBoxLayout(pathsGroup);
                    pathsLayout->setContentsMargins(8, 8, 8, 8);
                    auto* pathTable = new QTableView(pathsGroup);
                    pathTable->setObjectName(
                        QStringLiteral("xqPathPlanningPathTableView"));
                    pathTable->setSelectionMode(
                        QAbstractItemView::SingleSelection);
                    pathTable->setSelectionBehavior(
                        QAbstractItemView::SelectRows);
                    pathsLayout->addWidget(pathTable);
                    auto* pathButtonLayout = new QHBoxLayout();
                    auto* addPathButton =
                        new QPushButton(QStringLiteral("Add Path"),
                                        pathsGroup);
                    addPathButton->setObjectName(
                        QStringLiteral("xqPathPlanningAddPathButton"));
                    addPathButton->setCheckable(true);
                    addPathButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("create-centerline"));
                    auto* deletePathButton =
                        new QPushButton(QStringLiteral("Delete Path"),
                                        pathsGroup);
                    deletePathButton->setObjectName(
                        QStringLiteral("xqPathPlanningDeletePathButton"));
                    deletePathButton->setEnabled(false);
                    auto* smartPointButton =
                        new QPushButton(QStringLiteral("Smart"), pathsGroup);
                    smartPointButton->setObjectName(
                        QStringLiteral("xqPathPlanningSmartPointButton"));
                    smartPointButton->setEnabled(false);
                    pathButtonLayout->addWidget(addPathButton);
                    pathButtonLayout->addWidget(deletePathButton);
                    pathButtonLayout->addWidget(smartPointButton);
                    pathsLayout->addLayout(pathButtonLayout);
                    layout->addWidget(pathsGroup);

                    auto* pointsGroup =
                        new QGroupBox(QStringLiteral("Control Points"), page);
                    pointsGroup->setObjectName(QStringLiteral(
                        "xqPathPlanningControlPointsGroup"));
                    auto* pointsLayout = new QVBoxLayout(pointsGroup);
                    pointsLayout->setContentsMargins(8, 8, 8, 8);
                    auto* pointTable = new QTableView(pointsGroup);
                    pointTable->setObjectName(
                        QStringLiteral("xqPathPlanningPointTableView"));
                    pointTable->setSelectionMode(
                        QAbstractItemView::SingleSelection);
                    pointTable->setSelectionBehavior(
                        QAbstractItemView::SelectRows);
                    pointsLayout->addWidget(pointTable);
                    auto* pointButtonLayout = new QHBoxLayout();
                    auto* addPointButton =
                        new QPushButton(QStringLiteral("Add Point"),
                                        pointsGroup);
                    addPointButton->setObjectName(
                        QStringLiteral("xqPathPlanningAddPointButton"));
                    addPointButton->setEnabled(false);
                    auto* editPointsButton =
                        new QPushButton(QStringLiteral("Edit Points"),
                                        pointsGroup);
                    editPointsButton->setObjectName(QStringLiteral(
                        "xqPathPlanningEditControlPointsButton"));
                    editPointsButton->setCheckable(true);
                    editPointsButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("edit-control-points"));
                    pointButtonLayout->addWidget(addPointButton);
                    pointButtonLayout->addWidget(editPointsButton);
                    pointsLayout->addLayout(pointButtonLayout);
                    layout->addWidget(pointsGroup);

                    auto* toolsGroup =
                        new QGroupBox(QStringLiteral("Path Tools"), page);
                    toolsGroup->setObjectName(
                        QStringLiteral("xqPathPlanningToolsGroup"));
                    auto* toolsLayout = new QVBoxLayout(toolsGroup);
                    toolsLayout->setContentsMargins(8, 8, 8, 8);
                    auto* toolButtonLayout = new QHBoxLayout();
                    auto* smoothPathButton =
                        new QPushButton(QStringLiteral("Smooth Path"),
                                        toolsGroup);
                    smoothPathButton->setObjectName(
                        QStringLiteral("xqPathPlanningSmoothPathButton"));
                    smoothPathButton->setCheckable(true);
                    smoothPathButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("smooth-path"));
                    toolButtonLayout->addWidget(smoothPathButton);
                    toolsLayout->addLayout(toolButtonLayout);
                    toolsLayout->addWidget(parameterPanel);
                    layout->addWidget(toolsGroup);

                    auto* toolButtons = new QButtonGroup(page);
                    toolButtons->setExclusive(true);
                    toolButtons->addButton(addPathButton);
                    toolButtons->addButton(editPointsButton);
                    toolButtons->addButton(smoothPathButton);

                    auto connectPathButton = [this](QPushButton* button,
                                                    const QString& operationId) {
                        connect(button,
                                &QPushButton::clicked,
                                this,
                                [this, operationId]() {
                                    QString message;
                                    if (!m_Context.WorkflowOperations()
                                             ->SelectOperation(
                                                 QStringLiteral("path"),
                                                 operationId,
                                                 &message))
                                    {
                                        m_Context.PostDiagnostic(message);
                                    }
                                    UpdateWorkflowOperationControls();
                                });
                    };
                    connectPathButton(addPathButton,
                                      QStringLiteral("create-centerline"));
                    connectPathButton(editPointsButton,
                                      QStringLiteral("edit-control-points"));
                    connectPathButton(smoothPathButton,
                                      QStringLiteral("smooth-path"));
                }
                else if (id == QStringLiteral("segmentation-2d"))
                {
                    auto* pathGroup =
                        new QGroupBox(QStringLiteral("Path Selection"), page);
                    pathGroup->setObjectName(QStringLiteral(
                        "xqSegmentation2DPathSelectionGroup"));
                    auto* pathLayout = new QVBoxLayout(pathGroup);
                    pathLayout->setContentsMargins(8, 8, 8, 8);
                    operationSelector->setVisible(false);
                    layout->addWidget(operationSelector);
                    auto* pathComboBox = new QComboBox(pathGroup);
                    pathComboBox->setObjectName(QStringLiteral(
                        "xqSegmentation2DPathComboBox"));
                    pathLayout->addWidget(pathComboBox);
                    layout->addWidget(pathGroup);

                    auto* contourGroups =
                        new QGroupBox(QStringLiteral("Contour Groups"), page);
                    contourGroups->setObjectName(QStringLiteral(
                        "xqSegmentation2DContourGroupsGroup"));
                    auto* contourGroupsLayout =
                        new QVBoxLayout(contourGroups);
                    contourGroupsLayout->setContentsMargins(8, 8, 8, 8);
                    auto* contourToolButtons =
                        new QHBoxLayout();
                    auto* createGroupButton =
                        new QPushButton(QStringLiteral("Create Group"),
                                        contourGroups);
                    createGroupButton->setObjectName(QStringLiteral(
                        "xqSegmentation2DCreateGroupButton"));
                    auto* contourGroupLoftButton =
                        new QPushButton(QStringLiteral("Loft"),
                                        contourGroups);
                    contourGroupLoftButton->setObjectName(QStringLiteral(
                        "xqSegmentation2DContourGroupLoftButton"));
                    contourGroupLoftButton->setEnabled(false);
                    contourToolButtons->addWidget(createGroupButton);
                    contourToolButtons->addWidget(contourGroupLoftButton);
                    contourGroupsLayout->addLayout(contourToolButtons);
                    layout->addWidget(contourGroups);

                    auto* contourTools =
                        new QGroupBox(QStringLiteral("Contour Tools"), page);
                    contourTools->setObjectName(QStringLiteral(
                        "xqSegmentation2DContourToolsGroup"));
                    auto* contourToolsLayout =
                        new QVBoxLayout(contourTools);
                    contourToolsLayout->setContentsMargins(8, 8, 8, 8);
                    auto* contourToolStack =
                        new QStackedWidget(contourTools);
                    contourToolStack->setObjectName(QStringLiteral(
                        "xqSegmentation2DContourToolStack"));
                    auto* contourButtonPage =
                        new QWidget(contourToolStack);
                    auto* contourButtonLayout =
                        new QHBoxLayout(contourButtonPage);
                    contourButtonLayout->setContentsMargins(0, 0, 0, 0);
                    auto* thresholdButton =
                        new QPushButton(QStringLiteral("Threshold"),
                                        contourButtonPage);
                    thresholdButton->setObjectName(QStringLiteral(
                        "xqSegmentation2DThresholdContourButton"));
                    thresholdButton->setCheckable(true);
                    thresholdButton->setProperty(
                        "xqOperationId",
                        QStringLiteral("threshold-contour"));
                    auto* manualButton =
                        new QPushButton(QStringLiteral("Manual"),
                                        contourButtonPage);
                    manualButton->setObjectName(QStringLiteral(
                        "xqSegmentation2DManualContourButton"));
                    manualButton->setCheckable(true);
                    manualButton->setProperty("xqOperationId",
                                              QStringLiteral("manual-contour"));
                    auto* loftButton =
                        new QPushButton(QStringLiteral("Loft Profiles"),
                                        contourButtonPage);
                    loftButton->setObjectName(QStringLiteral(
                        "xqSegmentation2DLoftProfilesButton"));
                    loftButton->setCheckable(true);
                    loftButton->setProperty("xqOperationId",
                                            QStringLiteral("loft-profiles"));
                    contourButtonLayout->addWidget(thresholdButton);
                    contourButtonLayout->addWidget(manualButton);
                    contourButtonLayout->addWidget(loftButton);
                    contourToolStack->addWidget(contourButtonPage);
                    contourToolsLayout->addWidget(contourToolStack);
                    contourToolsLayout->addWidget(parameterPanel);
                    layout->addWidget(contourTools);

                    auto* toolButtons = new QButtonGroup(contourTools);
                    toolButtons->setExclusive(true);
                    toolButtons->addButton(thresholdButton);
                    toolButtons->addButton(manualButton);
                    toolButtons->addButton(loftButton);

                    auto connectToolButton = [this](QPushButton* button,
                                                    const QString& operationId) {
                        connect(button,
                                &QPushButton::clicked,
                                this,
                                [this, operationId]() {
                                    QString message;
                                    if (!m_Context.WorkflowOperations()
                                             ->SelectOperation(
                                                 QStringLiteral(
                                                     "segmentation-2d"),
                                                 operationId,
                                                 &message))
                                    {
                                        m_Context.PostDiagnostic(message);
                                    }
                                    UpdateWorkflowOperationControls();
                                });
                    };
                    connectToolButton(thresholdButton,
                                      QStringLiteral("threshold-contour"));
                    connectToolButton(manualButton,
                                      QStringLiteral("manual-contour"));
                    connectToolButton(loftButton,
                                      QStringLiteral("loft-profiles"));
                }
                else if (id == QStringLiteral("segmentation-3d"))
                {
                    auto* referenceGroup =
                        new QGroupBox(QStringLiteral("Reference Image"), page);
                    referenceGroup->setObjectName(QStringLiteral(
                        "xqSegmentation3DReferenceImageGroup"));
                    auto* referenceLayout = new QHBoxLayout(referenceGroup);
                    referenceLayout->setContentsMargins(8, 8, 8, 8);
                    operationSelector->setVisible(false);
                    layout->addWidget(operationSelector);
                    auto* imageComboBox = new QComboBox(referenceGroup);
                    imageComboBox->setObjectName(QStringLiteral(
                        "xqSegmentation3DImageComboBox"));
                    referenceLayout->addWidget(imageComboBox, 1);
                    auto* refreshButton =
                        new QPushButton(QStringLiteral("Refresh"),
                                        referenceGroup);
                    refreshButton->setObjectName(QStringLiteral(
                        "xqSegmentation3DRefreshButton"));
                    referenceLayout->addWidget(refreshButton);
                    layout->addWidget(referenceGroup);

                    auto* toolsGroup =
                        new QGroupBox(QStringLiteral("Segmentation Tools"),
                                      page);
                    toolsGroup->setObjectName(QStringLiteral(
                        "xqSegmentation3DSegmentationToolsGroup"));
                    auto* toolsLayout = new QHBoxLayout(toolsGroup);
                    toolsLayout->setContentsMargins(8, 8, 8, 8);
                    auto* toolButtons = new QButtonGroup(toolsGroup);
                    toolButtons->setExclusive(true);

                    auto addToolButton = [this, toolsGroup, toolsLayout,
                                          toolButtons](const QString& text,
                                                       const QString& objectName,
                                                       const QString& operationId) {
                        auto* button = new QPushButton(text, toolsGroup);
                        button->setObjectName(objectName);
                        button->setCheckable(true);
                        button->setProperty("xqOperationId", operationId);
                        toolButtons->addButton(button);
                        toolsLayout->addWidget(button);
                        connect(button,
                                &QPushButton::clicked,
                                this,
                                [this, operationId]() {
                                    QString message;
                                    if (!m_Context.WorkflowOperations()
                                             ->SelectOperation(
                                                 QStringLiteral(
                                                     "segmentation-3d"),
                                                 operationId,
                                                 &message))
                                    {
                                        m_Context.PostDiagnostic(message);
                                    }
                                    UpdateWorkflowOperationControls();
                                });
                        return button;
                    };

                    addToolButton(QStringLiteral("Threshold"),
                                  QStringLiteral(
                                      "xqSegmentation3DThresholdButton"),
                                  QStringLiteral("threshold-region"));
                    addToolButton(QStringLiteral("Region Grow"),
                                  QStringLiteral(
                                      "xqSegmentation3DRegionGrowButton"),
                                  QStringLiteral("region-growing"));
                    addToolButton(QStringLiteral("Surface Preview"),
                                  QStringLiteral(
                                      "xqSegmentation3DSurfacePreviewButton"),
                                  QStringLiteral("surface-preview"));
                    layout->addWidget(toolsGroup);

                    auto* parametersGroup =
                        new QGroupBox(QStringLiteral("Tool Parameters"),
                                      page);
                    parametersGroup->setObjectName(QStringLiteral(
                        "xqSegmentation3DToolParametersGroup"));
                    auto* parametersLayout =
                        new QVBoxLayout(parametersGroup);
                    parametersLayout->setContentsMargins(8, 8, 8, 8);
                    auto* parameterStack =
                        new QStackedWidget(parametersGroup);
                    parameterStack->setObjectName(QStringLiteral(
                        "xqSegmentation3DToolParameterStack"));
                    auto* parameterStackPage =
                        new QWidget(parameterStack);
                    auto* parameterStackLayout =
                        new QVBoxLayout(parameterStackPage);
                    parameterStackLayout->setContentsMargins(0, 0, 0, 0);
                    parameterPanel->setParent(parameterStackPage);
                    parameterStackLayout->addWidget(parameterPanel);
                    parameterStack->addWidget(parameterStackPage);
                    parametersLayout->addWidget(parameterStack);
                    layout->addWidget(parametersGroup);
                }
                else
                {
                    layout->addWidget(operationSelector);
                    layout->addWidget(parameterPanel);
                }
            }

            if (!m_WorkflowContextStatusLabels.contains(id))
            {
                auto* statusLabel = new QLabel(page);
                statusLabel->setObjectName(
                    QStringLiteral("xqWorkflowContextStatus_%1").arg(id));
                statusLabel->setWordWrap(true);
                m_WorkflowContextStatusLabels.insert(id, statusLabel);
                layout->addWidget(statusLabel);
            }

            auto* actionButton = new QPushButton(QStringLiteral("Run"), page);
            actionButton->setObjectName(
                QStringLiteral("xqWorkflowPrimaryAction_%1").arg(id));
            actionButton->setEnabled(false);
            connect(actionButton,
                    &QPushButton::clicked,
                    this,
                    [this]() {
                        RunActiveWorkflowAction();
                    });
            m_WorkflowPrimaryActionButtons.insert(id, actionButton);
            layout->addWidget(actionButton);
            UpdateWorkflowOperationControls();
        }
    }
    layout->addStretch(1);

    return page;
}

void MainWindow::AddWorkflowPage(const QString& id, const QString& title)
{
    auto* item = new QListWidgetItem(title, m_Navigation);
    item->setData(Qt::UserRole, id);
    m_Pages->addWidget(CreateWorkflowPage(id, title));
}

} // namespace xq::presentation
