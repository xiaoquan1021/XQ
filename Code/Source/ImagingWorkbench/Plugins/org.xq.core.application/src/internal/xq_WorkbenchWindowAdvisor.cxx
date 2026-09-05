#include "xq_WorkbenchWindowAdvisor.h"
#include "xq_ApplicationPluginActivator.h"
#include "xq_NewWorkspaceAction.h"
#include "xq_OpenWorkspaceAction.h"
#include "xq_SaveWorkspaceAction.h"
#include "xq_AboutDialog.h"

#include <QMenu>
#include <QMenuBar>
#include <QMainWindow>
#include <QStatusBar>
#include <QToolBar>
#include <QMessageBox>
#include <QLabel>
#include <QApplication>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QPixmap>
#include <QScreen>
#include <QInputDialog>
#include <QTabWidget>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QTimer>
#include <QRegularExpression>
#include <QToolButton>
#include <QTreeView>
#include <QActionGroup>
#include <QHash>

#include <mitkCoreServices.h>
#include <mitkIPreferencesService.h>
#include <mitkIPreferences.h>

#include <berryPlatform.h>
#include <berryPlatformUI.h>
#include <berryIWorkbenchWindow.h>
#include <berryIWorkbenchPage.h>
#include <berryIViewRegistry.h>
#include <berryIViewDescriptor.h>
#include <berryIPerspectiveRegistry.h>
#include <berryIPerspectiveDescriptor.h>

#include <mitkIDataStorageService.h>
#include <mitkIDataStorageReference.h>
#include <mitkDataStorageEditorInput.h>
#include <mitkWorkbenchUtil.h>
#include <mitkIOUtil.h>
#include <mitkSceneIO.h>
#include <mitkRenderingManager.h>
#include <mitkLogMacros.h>
#include <mitkNodePredicateNot.h>
#include <mitkNodePredicateProperty.h>
#include <mitkPointSet.h>
#include <mitkImage.h>
#include <mitkSurface.h>
#include <mitkTransferFunction.h>
#include <mitkTransferFunctionProperty.h>
#include <mitkUndoController.h>
#include <mitkNodePredicateDataType.h>
#include <mitkDataNodeSelection.h>

#include <xq_PipelineDataUtils.h>

#include <berryIIntroManager.h>

#include "xq_WorkspaceManager.h"
#include <QmitkPreferencesDialog.h>

#include <QmitkStatusBar.h>
#include <QmitkProgressBar.h>
#include <QmitkMemoryUsageIndicatorView.h>

#include <internal/berryQtShowViewAction.h>

#include <vtkMassProperties.h>
#include <vtkTriangleFilter.h>
#include <vtkLineSource.h>
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <QDoubleSpinBox>
#include <cmath>

constexpr double kPi = 3.141592653589793238462643383279502884;

// Title listener for tracking editor changes
class XQPartListenerForTitle : public berry::IPartListener
{
public:
    XQPartListenerForTitle(xq_WorkbenchWindowAdvisor* advisor)
        : m_Advisor(advisor) {}

    Events::Types GetPartEventTypes() const override
    {
        return Events::ACTIVATED | Events::CLOSED;
    }

    void PartActivated(const berry::IWorkbenchPartReference::Pointer& /*ref*/) override {}
    void PartClosed(const berry::IWorkbenchPartReference::Pointer& /*ref*/) override {}

private:
    xq_WorkbenchWindowAdvisor* m_Advisor;
};

// Perspective listener
class XQPerspectiveListener : public berry::IPerspectiveListener
{
public:
    XQPerspectiveListener(xq_WorkbenchWindowAdvisor* advisor)
        : m_Advisor(advisor) {}

    Events::Types GetPerspectiveEventTypes() const override
    {
        return Events::ACTIVATED | Events::DEACTIVATED;
    }

    void PerspectiveActivated(const berry::IWorkbenchPage::Pointer& /*page*/,
                              const berry::IPerspectiveDescriptor::Pointer& /*perspective*/) override
    {
        for (auto* action : m_Advisor->m_ViewActions)
        {
            action->setEnabled(true);
        }
    }

    void PerspectiveDeactivated(const berry::IWorkbenchPage::Pointer& /*page*/,
                                const berry::IPerspectiveDescriptor::Pointer& /*perspective*/) override
    {
    }

private:
    xq_WorkbenchWindowAdvisor* m_Advisor;
};

xq_WorkbenchWindowAdvisor::xq_WorkbenchWindowAdvisor(
    berry::WorkbenchAdvisor* wbAdvisor,
    berry::IWorkbenchWindowConfigurer::Pointer configurer)
    : berry::WorkbenchWindowAdvisor(configurer)
    , m_WorkbenchAdvisor(wbAdvisor)
    , m_SaveProjectAction(nullptr)
    , m_UndoAction(nullptr)
    , m_RedoAction(nullptr)
    , m_SaveAsAction(nullptr)
    , m_CloseProjectAction(nullptr)
    , m_OpenDataFileAction(nullptr)
    , m_SaveSceneAction(nullptr)
    , m_ResetPerspectiveAction(nullptr)
    , m_PreferencesAction(nullptr)
    , m_ToggleAxialAction(nullptr)
    , m_ToggleSagittalAction(nullptr)
    , m_ToggleCoronalAction(nullptr)
    , m_WelcomeAction(nullptr)
    , m_ImportDicomAction(nullptr)
    , m_RecentProjectsMenu(nullptr)
    , m_VolumeRenderingAction(nullptr)
    , m_CrosshairAction(nullptr)
    , m_MemoryLabel(nullptr)
    , m_CoordLabel(nullptr)
    , m_StatusTimer(nullptr)
    , m_LastNodeCount(-1)
{
}

xq_WorkbenchWindowAdvisor::~xq_WorkbenchWindowAdvisor()
{
}

void xq_WorkbenchWindowAdvisor::UpdateStatusInfo()
{
    // --- Memory usage from /proc/self/status ---
    int memMB = -1;
    QFile procStatus("/proc/self/status");
    if (procStatus.open(QIODevice::ReadOnly))
    {
        QString content = procStatus.readAll();
        procStatus.close();
        QRegularExpression rx("VmRSS:\\s+(\\d+)");
        auto match = rx.match(content);
        if (match.hasMatch())
            memMB = match.captured(1).toInt() / 1024;
    }

    // --- Node count from DataStorage ---
    int nodeCount = 0;
    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNotNull())
    {
        mitk::DataStorage::SetOfObjects::ConstPointer allNodes = ds->GetAll();
        if (allNodes.IsNotNull())
            nodeCount = static_cast<int>(allNodes->size());
    }

    // Update memory label
    if (m_MemoryLabel)
    {
        QString text;
        if (memMB >= 0)
            text = QString("Mem: %1 MB | Nodes: %2").arg(memMB).arg(nodeCount);
        else
            text = QString("Nodes: %1").arg(nodeCount);
        m_MemoryLabel->setText(text);
    }

    // Update window title when node count changes
    if (nodeCount != m_LastNodeCount)
    {
        m_LastNodeCount = nodeCount;
        berry::IWorkbenchWindow::Pointer wbWindow =
            berry::PlatformUI::GetWorkbench()->GetActiveWorkbenchWindow();
        if (wbWindow.IsNotNull())
        {
            berry::Shell::Pointer shell = wbWindow->GetShell();
            if (shell.IsNotNull())
                shell->SetText(QString("XQ \u2014 Cardiovascular Analysis Suite [%1 nodes]").arg(nodeCount));
        }
    }
}

void xq_WorkbenchWindowAdvisor::SetWindowIcon(const QString& iconPath)
{
    m_WindowIcon = iconPath;
}

void xq_WorkbenchWindowAdvisor::PreWindowOpen()
{
    berry::IWorkbenchWindowConfigurer::Pointer configurer = GetWindowConfigurer();

    configurer->SetTitle("XQ");
    configurer->AddEditorAreaTransfer(QStringList() << "text/uri-list");

    m_TitlePartListener.reset(new XQPartListenerForTitle(this));
    configurer->GetWindow()->GetPartService()->AddPartListener(m_TitlePartListener.data());

    m_PerspectiveListener.reset(new XQPerspectiveListener(this));
    configurer->GetWindow()->AddPerspectiveListener(m_PerspectiveListener.data());
}

void xq_WorkbenchWindowAdvisor::PostWindowCreate()
{
    berry::IWorkbenchWindow::Pointer window = GetWindowConfigurer()->GetWindow();
    QMainWindow* mainWindow =
        qobject_cast<QMainWindow*>(window->GetShell()->GetControl());

    if (!m_WindowIcon.isEmpty())
    {
        mainWindow->setWindowIcon(QIcon(m_WindowIcon));
    }
    mainWindow->setContextMenuPolicy(Qt::PreventContextMenu);

    // ========== Menu Bar ==========
    QMenuBar* menuBar = mainWindow->menuBar();
    menuBar->setContextMenuPolicy(Qt::PreventContextMenu);

#ifdef __APPLE__
    menuBar->setNativeMenuBar(true);
#else
    menuBar->setNativeMenuBar(false);
#endif

    // ----- File Menu -----
    QMenu* fileMenu = menuBar->addMenu("&File");
    fileMenu->setObjectName("FileMenu");

    QAction* newProjectAction =
        new xq_NewWorkspaceAction(QIcon::fromTheme("document-new",
            QIcon(":/xq/document-new.svg")), window);
    newProjectAction->setShortcut(QKeySequence::New);
    fileMenu->addAction(newProjectAction);

    QAction* openProjectAction =
        new xq_OpenWorkspaceAction(QIcon::fromTheme("document-open",
            QIcon(":/xq/document-open.svg")), window);
    openProjectAction->setShortcut(QKeySequence::Open);
    fileMenu->addAction(openProjectAction);

    m_SaveProjectAction =
        new xq_SaveWorkspaceAction(QIcon::fromTheme("document-save",
            QIcon(":/xq/document-save.svg")), window);
    m_SaveProjectAction->setShortcut(QKeySequence::Save);
    fileMenu->addAction(m_SaveProjectAction);

    m_SaveAsAction = new QAction("Save &As...", mainWindow);
    m_SaveAsAction->setShortcut(QKeySequence("Ctrl+Shift+S"));
    m_SaveAsAction->setToolTip("Save the current workspace to a different location");
    connect(m_SaveAsAction, &QAction::triggered, this, &xq_WorkbenchWindowAdvisor::OnSaveAs);
    fileMenu->addAction(m_SaveAsAction);

    m_CloseProjectAction = new QAction("&Close Workspace", mainWindow);
    m_CloseProjectAction->setShortcut(QKeySequence("Ctrl+W"));
    m_CloseProjectAction->setToolTip("Close the current workspace and clear all data");
    connect(m_CloseProjectAction, &QAction::triggered, this, &xq_WorkbenchWindowAdvisor::OnCloseProject);
    fileMenu->addAction(m_CloseProjectAction);

    fileMenu->addSeparator();

    m_OpenDataFileAction = new QAction("Open &Data File...", mainWindow);
    m_OpenDataFileAction->setToolTip("Load individual data files (images, surfaces, scenes)");
    connect(m_OpenDataFileAction, &QAction::triggered, this, &xq_WorkbenchWindowAdvisor::OnOpenDataFile);
    fileMenu->addAction(m_OpenDataFileAction);

    m_ImportDicomAction = new QAction("Import &DICOM...", mainWindow);
    m_ImportDicomAction->setToolTip("Import DICOM image series from a directory");
    connect(m_ImportDicomAction, &QAction::triggered, this, &xq_WorkbenchWindowAdvisor::OnImportDicom);
    fileMenu->addAction(m_ImportDicomAction);

    m_SaveSceneAction = new QAction("Save All as MITK &Scene...", mainWindow);
    m_SaveSceneAction->setToolTip("Export the entire data storage as an MITK scene file");
    connect(m_SaveSceneAction, &QAction::triggered, this, &xq_WorkbenchWindowAdvisor::OnSaveScene);
    fileMenu->addAction(m_SaveSceneAction);

    fileMenu->addSeparator();

    m_RecentProjectsMenu = fileMenu->addMenu("Recent &Projects");
    LoadRecentProjects();
    RebuildRecentProjectsMenu();

    fileMenu->addSeparator();

    QAction* exitAction = new QAction("&Exit", mainWindow);
    exitAction->setToolTip("Exit the application. Please save your data before exiting.");
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setIcon(QIcon::fromTheme("system-log-out",
        QIcon(":/org_mitk_icons/icons/tango/scalable/actions/system-log-out.svg")));
    connect(exitAction, &QAction::triggered, this, &xq_WorkbenchWindowAdvisor::OnExit);
    fileMenu->addAction(exitAction);

    // ----- Edit Menu -----
    QMenu* editMenu = menuBar->addMenu("&Edit");

    m_UndoAction = new QAction(
        QIcon::fromTheme("edit-undo",
            QIcon(":/xq/edit-undo.svg")),
        "&Undo", mainWindow);
    m_UndoAction->setShortcut(QKeySequence("Ctrl+Z"));
    m_UndoAction->setToolTip("Undo the last action (not supported by all modules)");
    connect(m_UndoAction, &QAction::triggered, this, [this]() {
      if (auto* model = mitk::UndoController::GetCurrentUndoModel())
      {
          model->Undo();
          mitk::RenderingManager::GetInstance()->RequestUpdateAll();
      }
    });
    editMenu->addAction(m_UndoAction);

    m_RedoAction = new QAction(
        QIcon::fromTheme("edit-redo",
            QIcon(":/xq/edit-redo.svg")),
        "&Redo", mainWindow);
    m_RedoAction->setShortcut(QKeySequence("Ctrl+Y"));
    m_RedoAction->setToolTip("Redo the last undone action (not supported by all modules)");
    connect(m_RedoAction, &QAction::triggered, this, [this]() {
      if (auto* model = mitk::UndoController::GetCurrentUndoModel())
      {
          model->Redo();
          mitk::RenderingManager::GetInstance()->RequestUpdateAll();
      }
    });
    editMenu->addAction(m_RedoAction);

    // ----- View Menu -----
    QMenu* viewMenu = menuBar->addMenu("&View");

    QAction* dataManagerAction = new QAction("Data Manager", mainWindow);
    connect(dataManagerAction, &QAction::triggered, this, [this]() {
        ShowView("org.xq.views.datamanager");
    });
    viewMenu->addAction(dataManagerAction);

    QAction* imageNavigatorAction = new QAction("Image Navigator", mainWindow);
    connect(imageNavigatorAction, &QAction::triggered, this, [this]() {
        ShowView("org.mitk.views.imagenavigator");
    });
    viewMenu->addAction(imageNavigatorAction);

    QAction* workspaceExplorerAction = new QAction("Workspace Explorer", mainWindow);
    connect(workspaceExplorerAction, &QAction::triggered, this, [this]() {
        ShowView("org.xq.views.projectmanager");
    });
    viewMenu->addAction(workspaceExplorerAction);

    viewMenu->addSeparator();

    m_ToggleLoggingAction = new QAction("Logging", mainWindow);
    m_ToggleLoggingAction->setCheckable(true);
    m_ToggleLoggingAction->setChecked(false);
    m_ToggleLoggingAction->setToolTip("Show/hide the log console");
    connect(m_ToggleLoggingAction, &QAction::triggered,
            this, &xq_WorkbenchWindowAdvisor::OnToggleLogging);
    viewMenu->addAction(m_ToggleLoggingAction);

    viewMenu->addSeparator();

    m_ToggleAxialAction = new QAction("Axial", mainWindow);
    m_ToggleAxialAction->setCheckable(true);
    m_ToggleAxialAction->setChecked(true);
    m_ToggleAxialAction->setToolTip("Show/hide axial slice plane in 3D view");
    connect(m_ToggleAxialAction, &QAction::triggered,
            this, &xq_WorkbenchWindowAdvisor::OnToggleAxial);
    viewMenu->addAction(m_ToggleAxialAction);

    m_ToggleSagittalAction = new QAction("Sagittal", mainWindow);
    m_ToggleSagittalAction->setCheckable(true);
    m_ToggleSagittalAction->setChecked(true);
    m_ToggleSagittalAction->setToolTip("Show/hide sagittal slice plane in 3D view");
    connect(m_ToggleSagittalAction, &QAction::triggered,
            this, &xq_WorkbenchWindowAdvisor::OnToggleSagittal);
    viewMenu->addAction(m_ToggleSagittalAction);

    m_ToggleCoronalAction = new QAction("Coronal", mainWindow);
    m_ToggleCoronalAction->setCheckable(true);
    m_ToggleCoronalAction->setChecked(true);
    m_ToggleCoronalAction->setToolTip("Show/hide coronal slice plane in 3D view");
    connect(m_ToggleCoronalAction, &QAction::triggered,
            this, &xq_WorkbenchWindowAdvisor::OnToggleCoronal);
    viewMenu->addAction(m_ToggleCoronalAction);

    viewMenu->addSeparator();

    auto* screenshotAction = new QAction("&Screenshot...", mainWindow);
    screenshotAction->setShortcut(QKeySequence("Ctrl+Shift+P"));
    screenshotAction->setToolTip("Capture a screenshot of the application window");
    connect(screenshotAction, &QAction::triggered, this, &xq_WorkbenchWindowAdvisor::OnScreenshot);
    viewMenu->addAction(screenshotAction);

    m_VolumeRenderingAction = new QAction("&Volume Rendering", mainWindow);
    m_VolumeRenderingAction->setCheckable(true);
    m_VolumeRenderingAction->setChecked(false);
    m_VolumeRenderingAction->setToolTip("Toggle 3D volume rendering for all image nodes");
    connect(m_VolumeRenderingAction, &QAction::triggered, this, &xq_WorkbenchWindowAdvisor::OnToggleVolumeRendering);
    viewMenu->addAction(m_VolumeRenderingAction);

    m_CrosshairAction = new QAction("&Crosshair", mainWindow);
    m_CrosshairAction->setCheckable(true);
    m_CrosshairAction->setChecked(true);
    m_CrosshairAction->setToolTip("Show/hide crosshair lines in slice views");
    connect(m_CrosshairAction, &QAction::triggered, this, &xq_WorkbenchWindowAdvisor::OnToggleCrosshair);
    viewMenu->addAction(m_CrosshairAction);

    viewMenu->addSeparator();

    QMenu* viewPresetMenu = viewMenu->addMenu("&View Presets");

    struct PerspEntry {
        const char* label;
        const char* perspId;
    };
    const PerspEntry perspectives[] = {
        {"Default",  "org.xq.application.defaultperspective"},
        {"Viewer",   "org.xq.application.viewerperspective"},
        {"Analysis", "org.xq.application.visualizationperspective"},
    };
    for (const auto& entry : perspectives)
    {
        QString perspId = entry.perspId;
        QAction* action = new QAction(entry.label, mainWindow);
        connect(action, &QAction::triggered, this, [this, perspId]() {
            berry::IWorkbenchWindow::Pointer wnd = GetWindowConfigurer()->GetWindow();
            if (wnd.IsNull())
                return;
            berry::IWorkbenchPage::Pointer pg = wnd->GetActivePage();
            if (pg.IsNull())
                return;
            berry::IPerspectiveDescriptor::Pointer persp =
                berry::PlatformUI::GetWorkbench()->GetPerspectiveRegistry()
                    ->FindPerspectiveWithId(perspId);
            if (persp.IsNotNull())
                pg->SetPerspective(persp);
        });
        viewPresetMenu->addAction(action);
    }

    viewPresetMenu->addSeparator();

    m_ResetPerspectiveAction = new QAction("Reset View Preset", mainWindow);
    m_ResetPerspectiveAction->setToolTip("Reset the current view preset to its default state");
    connect(m_ResetPerspectiveAction, &QAction::triggered,
            this, &xq_WorkbenchWindowAdvisor::OnResetPerspective);
    viewPresetMenu->addAction(m_ResetPerspectiveAction);

    // ----- Tools Menu -----
    QMenu* toolsMenu = menuBar->addMenu("&Tools");
    QActionGroup* toolActionGroup = new QActionGroup(mainWindow);
    toolActionGroup->setExclusive(true);

    QToolBar* mainToolBar = new QToolBar("Main Actions", mainWindow);
    mainToolBar->setObjectName("mainActionsToolBar");
    mainToolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    mainToolBar->setMovable(false);
    mainToolBar->setFloatable(false);
    mainToolBar->setAllowedAreas(Qt::TopToolBarArea);
    mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    mainToolBar->setIconSize(QSize(24, 24));
    mainWindow->addToolBar(Qt::TopToolBarArea, mainToolBar);

    mainToolBar->addAction(openProjectAction);
    mainToolBar->addAction(m_SaveProjectAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(m_UndoAction);
    mainToolBar->addAction(m_RedoAction);
    mainToolBar->addSeparator();

    auto addToolAction = [&](const QIcon& icon,
                             const QString& text,
                             const QString& toolbarText,
                             const QString& tooltip,
                             const QString& viewId,
                             const QString& resourcePath) {
        QAction* action = new QAction(icon, text, mainWindow);
        action->setIconText(toolbarText);
        action->setToolTip(tooltip);
        action->setStatusTip(tooltip);
        action->setCheckable(true);
        action->setActionGroup(toolActionGroup);
        connect(action, &QAction::triggered, this, [this, viewId]() {
            ShowView(viewId);
        });
        toolsMenu->addAction(action);
        m_ToolActionsByViewId.insert(viewId, action);

        auto* button = new QToolButton(mainToolBar);
        button->setDefaultAction(action);
        button->setText(toolbarText);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(tooltip);
        button->setIconSize(QSize(36, 36));
        button->setMinimumHeight(38);
        mainToolBar->addWidget(button);
        Q_UNUSED(resourcePath);
    };

    addToolAction(QIcon(":/xq/tool-process.svg"),
                  "Image Processing",
                  "Image",
                  "Image Processing",
                  "org.xq.views.imageprocessing",
                  ":/xq/tool-process.svg");
    addToolAction(QIcon(":/xq/tool-path.svg"),
                  "Path Planning",
                  "Path",
                  "Path Planning",
                  "org.xq.views.pathplanning",
                  ":/xq/tool-path.svg");
    addToolAction(QIcon(":/xq/tool-seg-2d.svg"),
                  "2D Segmentation",
                  "2D Seg",
                  "2D Segmentation",
                  "org.xq.views.segmentation",
                  ":/xq/tool-seg-2d.svg");
    addToolAction(QIcon(":/xq/tool-seg-3d.svg"),
                  "3D Segmentation",
                  "3D Seg",
                  "3D Segmentation",
                  "org.xq.views.mitksegmentation",
                  ":/xq/tool-seg-3d.svg");
    addToolAction(QIcon(":/xq/tool-model.svg"),
                  "Solid Modeling",
                  "Model",
                  "Solid Modeling",
                  "org.xq.views.modeling",
                  ":/xq/tool-model.svg");
    addToolAction(QIcon(":/xq/tool-mesh.svg"),
                  "Mesh Generation",
                  "Mesh",
                  "Mesh Generation",
                  "org.xq.views.meshing",
                  ":/xq/tool-mesh.svg");
    addToolAction(QIcon(":/xq/tool-flow.svg"),
                  "Flow Simulation",
                  "Simulation",
                  "Flow Simulation",
                  "org.xq.views.simulation",
                  ":/xq/tool-flow.svg");

    QMenu* windowMenu = menuBar->addMenu("&Window");

    m_PreferencesAction = new QAction("&Preferences...", mainWindow);
    m_PreferencesAction->setShortcut(QKeySequence("Ctrl+P"));
    m_PreferencesAction->setToolTip("Open the XQ preferences dialog");
    connect(m_PreferencesAction, &QAction::triggered,
            this, &xq_WorkbenchWindowAdvisor::OpenPreferencesDialog);
    windowMenu->addAction(m_PreferencesAction);

    // ----- Help Menu -----
    QMenu* helpMenu = menuBar->addMenu("&Help");

    m_WelcomeAction = new QAction("&Welcome", mainWindow);
    m_WelcomeAction->setToolTip("Show the welcome screen");
    connect(m_WelcomeAction, &QAction::triggered,
            this, &xq_WorkbenchWindowAdvisor::OnWelcome);
    helpMenu->addAction(m_WelcomeAction);

#ifndef __APPLE__
    helpMenu->addSeparator();
#endif

    QAction* aboutAction = new QAction("&About XQ", mainWindow);
    connect(aboutAction, &QAction::triggered,
            this, &xq_WorkbenchWindowAdvisor::OnAbout);
    helpMenu->addAction(aboutAction);

    // ========== Status Bar ==========
    auto* qStatusBar = new QStatusBar();
    auto* statusBar = new QmitkStatusBar(qStatusBar);
    statusBar->SetSizeGripEnabled(false);

    auto* progressBar = new QmitkProgressBar();
    qStatusBar->addPermanentWidget(progressBar, 0);
    progressBar->hide();

    mainWindow->setStatusBar(qStatusBar);

    auto* memoryIndicator = new QmitkMemoryUsageIndicatorView();
    qStatusBar->addPermanentWidget(memoryIndicator, 0);

    // Selection info label in status bar
    m_SelectionInfoLabel = new QLabel("Ready");
    m_SelectionInfoLabel->setMinimumWidth(200);
    qStatusBar->addWidget(m_SelectionInfoLabel, 1);

    // Coordinate display placeholder
    m_CoordLabel = new QLabel("Position: Ready");
    m_CoordLabel->setFixedWidth(250);
    qStatusBar->addPermanentWidget(m_CoordLabel, 0);

    // Memory / node-count display
    m_MemoryLabel = new QLabel("Mem: -- MB | Nodes: 0");
    m_MemoryLabel->setFixedWidth(200);
    m_MemoryLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_MemoryLabel->setStyleSheet("font-size: 11px;");
    qStatusBar->addPermanentWidget(m_MemoryLabel, 0);

    // Periodic status timer
    m_StatusTimer = new QTimer(this);
    m_StatusTimer->setInterval(10000);
    connect(m_StatusTimer, &QTimer::timeout, this, &xq_WorkbenchWindowAdvisor::UpdateStatusInfo);
    m_StatusTimer->start();

    // Set application palette for Arctic Light theme
    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor("#F8FAFC"));
    lightPalette.setColor(QPalette::WindowText, QColor("#1E293B"));
    lightPalette.setColor(QPalette::Base, QColor("#FFFFFF"));
    lightPalette.setColor(QPalette::AlternateBase, QColor("#F1F5F9"));
    lightPalette.setColor(QPalette::ToolTipBase, QColor("#FFFFFF"));
    lightPalette.setColor(QPalette::ToolTipText, QColor("#1E293B"));
    lightPalette.setColor(QPalette::Text, QColor("#1E293B"));
    lightPalette.setColor(QPalette::Button, QColor("#F1F5F9"));
    lightPalette.setColor(QPalette::ButtonText, QColor("#1E293B"));
    lightPalette.setColor(QPalette::BrightText, QColor("#FFFFFF"));
    lightPalette.setColor(QPalette::Link, QColor("#2563EB"));
    lightPalette.setColor(QPalette::Highlight, QColor("#2563EB"));
    lightPalette.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
    lightPalette.setColor(QPalette::Light, QColor("#FFFFFF"));
    lightPalette.setColor(QPalette::Midlight, QColor("#E2E8F0"));
    lightPalette.setColor(QPalette::Mid, QColor("#CBD5E1"));
    lightPalette.setColor(QPalette::Dark, QColor("#94A3B8"));
    lightPalette.setColor(QPalette::Shadow, QColor("#64748B"));
    qApp->setPalette(lightPalette);

    // Load QSS stylesheet
    QString stylesheetPath = ":/xq/xq.qss";
    QFile styleFile(stylesheetPath);
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QString styleSheet = styleFile.readAll();
        styleFile.close();
        qApp->setStyleSheet(styleSheet);
    }

}

void xq_WorkbenchWindowAdvisor::PostWindowOpen()
{
    berry::WorkbenchWindowAdvisor::PostWindowOpen();

    berry::IWorkbenchWindowConfigurer::Pointer configurer = GetWindowConfigurer();

    ctkPluginContext* context = xq_ApplicationPluginActivator::getContext();
    ctkServiceReference serviceRef =
        context->getServiceReference<mitk::IDataStorageService>();

    if (serviceRef)
    {
        mitk::IDataStorageService* dsService =
            context->getService<mitk::IDataStorageService>(serviceRef);
        if (dsService)
        {
            mitk::IDataStorageReference::Pointer dsRef = dsService->GetDataStorage();
            mitk::DataStorageEditorInput::Pointer dsInput(
                new mitk::DataStorageEditorInput(dsRef));
            mitk::WorkbenchUtil::OpenEditor(
                configurer->GetWindow()->GetActivePage(), dsInput);
            SetCrosshairGapZero();
            QTimer::singleShot(0, this, &xq_WorkbenchWindowAdvisor::SetCrosshairGapZero);
        }
    }

}

void xq_WorkbenchWindowAdvisor::SetCrosshairGapZero()
{
    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull())
        return;

    static const char* kPlaneNames[] = {
        "stdmulti.widget0.plane",
        "stdmulti.widget1.plane",
        "stdmulti.widget2.plane",
        "widget1Plane",
        "widget2Plane",
        "widget3Plane"
    };

    for (const char* planeName : kPlaneNames)
    {
        if (mitk::DataNode* node = ds->GetNamedNode(planeName))
            node->SetIntProperty("Crosshair.Gap Size", 0);
    }
    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_WorkbenchWindowAdvisor::OnDataManagerDoubleClick()
{
    berry::IWorkbenchWindow::Pointer window =
        berry::PlatformUI::GetWorkbench()->GetActiveWorkbenchWindow();
    if (window.IsNull())
        return;

    berry::IWorkbenchPage::Pointer page = window->GetActivePage();
    if (page.IsNull())
        return;

    berry::ISelectionService* selService = window->GetSelectionService();
    if (!selService)
        return;

    mitk::DataNodeSelection::ConstPointer nodeSelection =
        selService->GetSelection().Cast<const mitk::DataNodeSelection>();
    if (nodeSelection.IsNull())
        return;

    std::list<mitk::DataNode::Pointer> selectedNodes =
        nodeSelection->GetSelectedDataNodes();
    if (selectedNodes.empty())
        return;

    mitk::DataNode::Pointer selectedNode = selectedNodes.front();
    if (selectedNode.IsNull())
        return;

    const QString viewId = xq::pipeline::ResolveToolViewIdForNode(selectedNode);
    if (!viewId.isEmpty())
        page->ShowView(viewId);
}

void xq_WorkbenchWindowAdvisor::PostWindowClose()
{
    qApp->removeEventFilter(this);
}

mitk::DataStorage::Pointer xq_WorkbenchWindowAdvisor::GetDataStorage()
{
    ctkPluginContext* context = xq_ApplicationPluginActivator::getContext();
    if (!context)
        return nullptr;

    ctkServiceReference dsServiceRef =
        context->getServiceReference<mitk::IDataStorageService>();
    if (!dsServiceRef)
        return nullptr;

    mitk::IDataStorageService* dsService =
        context->getService<mitk::IDataStorageService>(dsServiceRef);
    if (!dsService)
        return nullptr;

    mitk::IDataStorageReference::Pointer dsRef = dsService->GetDataStorage();
    context->ungetService(dsServiceRef);

    if (dsRef.IsNull())
        return nullptr;

    return dsRef->GetDataStorage();
}

void xq_WorkbenchWindowAdvisor::ShowView(const QString& viewId)
{
    berry::IWorkbenchWindow::Pointer window = GetWindowConfigurer()->GetWindow();
    if (window.IsNull())
        return;

    berry::IWorkbenchPage::Pointer page = window->GetActivePage();
    if (page.IsNull())
        return;

    try
    {
        page->ShowView(viewId);
        auto it = m_ToolActionsByViewId.find(viewId);
        if (it != m_ToolActionsByViewId.end() && it.value())
            it.value()->setChecked(true);
    }
    catch (const berry::PartInitException& e)
    {
        BERRY_ERROR << "Could not open view: " << viewId.toStdString()
                    << " - " << e.what();
    }
}

void xq_WorkbenchWindowAdvisor::OnShowPathPlanning()
{
    ShowView("org.xq.views.pathplanning");
}

void xq_WorkbenchWindowAdvisor::OnShowSegmentation()
{
    ShowView("org.xq.views.segmentation");
}

void xq_WorkbenchWindowAdvisor::OnShowMitkSegmentation()
{
    ShowView("org.xq.views.mitksegmentation");
}

void xq_WorkbenchWindowAdvisor::OnShowModeling()
{
    ShowView("org.xq.views.modeling");
}

void xq_WorkbenchWindowAdvisor::OnShowMeshing()
{
    ShowView("org.xq.views.meshing");
}

void xq_WorkbenchWindowAdvisor::OnShowSimulation()
{
    ShowView("org.xq.views.simulation");
}

void xq_WorkbenchWindowAdvisor::OnShowImageProcessing()
{
    ShowView("org.xq.views.imageprocessing");
}

void xq_WorkbenchWindowAdvisor::OnAbout()
{
    auto* aboutDialog = new xq_AboutDialog(
        qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()));
    aboutDialog->open();
}

void xq_WorkbenchWindowAdvisor::OnExit()
{
    QWidget* parent = qobject_cast<QWidget*>(
        GetWindowConfigurer()->GetWindow()->GetShell()->GetControl());

    if (QMessageBox::question(parent, "Exit XQ",
            "Are you sure you want to exit?\nPlease make sure your data is saved.",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    berry::PlatformUI::GetWorkbench()->Close();
}

void xq_WorkbenchWindowAdvisor::OnSaveAs()
{
    QString dir = QFileDialog::getExistingDirectory(
        qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
        "Save Workspace As...",
        QDir::homePath());

    if (dir.isEmpty())
        return;

    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull())
        return;

    mitk::SceneIO::Pointer sceneIO = mitk::SceneIO::New();
    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = ds->GetAll();
    std::string filename = dir.toStdString() + "/project.mitk";
    bool success = sceneIO->SaveScene(allNodes, ds, filename);

    if (!success)
    {
        QMessageBox::warning(
            qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
            "Save As",
            "Failed to save project to the selected location.");
    }
}

void xq_WorkbenchWindowAdvisor::OnCloseProject()
{
    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNotNull())
    {
        mitk::DataStorage::SetOfObjects::ConstPointer allNodes = ds->GetAll();
        for (auto it = allNodes->begin(); it != allNodes->end(); ++it)
        {
            ds->Remove(*it);
        }
    }

    GetWindowConfigurer()->SetTitle("XQ");
    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(
        GetWindowConfigurer()->GetWindow()->GetShell()->GetControl());
    if (mainWindow)
        mainWindow->setWindowTitle("XQ");

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_WorkbenchWindowAdvisor::OnOpenDataFile()
{
    QStringList fileNames = QFileDialog::getOpenFileNames(
        qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
        "Open Data File",
        QDir::homePath(),
        "All Supported Files (*.mitk *.nrrd *.nii *.nii.gz *.dcm *.stl *.vtp *.vtu *.ply *.obj);;MITK Scene (*.mitk);;Images (*.nrrd *.nii *.nii.gz *.dcm);;Surfaces (*.stl *.vtp *.vtu *.ply *.obj);;All Files (*)");

    if (fileNames.isEmpty())
        return;

    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull())
        return;

    for (const QString& fileName : fileNames)
    {
        try
        {
            auto loadedData = mitk::IOUtil::Load(fileName.toStdString());
            for (auto& baseData : loadedData)
            {
                mitk::DataNode::Pointer node = mitk::DataNode::New();
                node->SetData(baseData);
                QFileInfo fi(fileName);
                node->SetName(fi.baseName().toStdString());
                ds->Add(node);
            }
        }
        catch (const mitk::Exception& e)
        {
            QMessageBox::warning(
                qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
                "Open Data File",
                QString("Failed to load file:\n%1\n\n%2").arg(fileName, e.GetDescription()));
        }
    }

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_WorkbenchWindowAdvisor::OnImportDicom()
{
    QString dicomDir = QFileDialog::getExistingDirectory(
        qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
        "Select DICOM Directory",
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (dicomDir.isEmpty())
        return;

    // Collect all files in the directory (DICOM files often lack extensions)
    QDir dir(dicomDir);
    QStringList allFiles;
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : entries)
    {
        allFiles.append(fi.absoluteFilePath());
    }

    // Also check subdirectories one level deep
    QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& subdir : subdirs)
    {
        QDir sub(subdir.absoluteFilePath());
        QFileInfoList subEntries = sub.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fi : subEntries)
        {
            allFiles.append(fi.absoluteFilePath());
        }
    }

    if (allFiles.isEmpty())
    {
        QMessageBox::information(
            qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
            "Import DICOM",
            "No files found in the selected directory.");
        return;
    }

    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull())
        return;

    int loadedCount = 0;
    try
    {
        // Use IOUtil::Load to read DICOM — MITK auto-detects DICOM format
        std::vector<std::string> paths;
        for (const QString& f : allFiles)
            paths.push_back(f.toStdString());

        auto loadedData = mitk::IOUtil::Load(paths);
        for (auto& baseData : loadedData)
        {
            mitk::DataNode::Pointer node = mitk::DataNode::New();
            node->SetData(baseData);
            node->SetName(dir.dirName().toStdString());
            ds->Add(node);
            ++loadedCount;
        }
    }
    catch (const mitk::Exception& e)
    {
        QMessageBox::warning(
            qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
            "Import DICOM",
            QString("Error importing DICOM data:\n%1").arg(e.GetDescription()));
    }
    catch (const std::exception& e)
    {
        QMessageBox::warning(
            qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
            "Import DICOM",
            QString("Error importing DICOM data:\n%1").arg(e.what()));
    }

    if (loadedCount > 0)
    {
        mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(ds);
        mitk::RenderingManager::GetInstance()->RequestUpdateAll();

        if (m_SelectionInfoLabel)
            m_SelectionInfoLabel->setText(QString("Loaded %1 DICOM series from %2").arg(loadedCount).arg(dir.dirName()));
    }
    else
    {
        QMessageBox::information(
            qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
            "Import DICOM",
            "No valid DICOM data could be loaded from the selected directory.");
    }
}

void xq_WorkbenchWindowAdvisor::OnSaveScene()
{
    QString fileName = QFileDialog::getSaveFileName(
        qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
        "Save All as MITK Scene",
        QDir::homePath() + "/scene.mitk",
        "MITK Scene Files (*.mitk)");

    if (fileName.isEmpty())
        return;

    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull())
        return;

    mitk::SceneIO::Pointer sceneIO = mitk::SceneIO::New();
    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = ds->GetAll();
    bool success = sceneIO->SaveScene(allNodes, ds, fileName.toStdString());

    if (!success)
    {
        QMessageBox::warning(
            qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
            "Save Scene",
            "Failed to save the scene file.");
    }
}

void xq_WorkbenchWindowAdvisor::OnResetPerspective()
{
    berry::IWorkbenchWindow::Pointer window = GetWindowConfigurer()->GetWindow();
    if (window.IsNull())
        return;

    berry::IWorkbenchPage::Pointer page = window->GetActivePage();
    if (page.IsNull())
        return;

    page->ResetPerspective();
}

void xq_WorkbenchWindowAdvisor::OnPreferences()
{
    QWidget* parent = qobject_cast<QWidget*>(
        GetWindowConfigurer()->GetWindow()->GetShell()->GetControl());
    QmitkPreferencesDialog prefDialog(parent);
    prefDialog.exec();
}

static void ToggleSlicePlaneVisibility(mitk::DataStorage::Pointer ds,
                                       const std::string& planeName,
                                       bool visible)
{
    if (ds.IsNull())
        return;

    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = ds->GetAll();
    for (auto it = allNodes->begin(); it != allNodes->end(); ++it)
    {
        mitk::DataNode::Pointer node = *it;
        std::string name;
        if (node->GetName(name) && name == planeName)
        {
            node->SetVisibility(visible);
            const auto& rwMap =
                mitk::RenderingManager::GetInstance()->GetAllRegisteredRenderWindows();
            for (auto* rw : rwMap)
            {
                if (!rw)
                    continue;
                if (auto* renderer = mitk::BaseRenderer::GetInstance(rw))
                    node->SetVisibility(visible, renderer);
            }
            node->Modified();
        }
    }

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_WorkbenchWindowAdvisor::OnToggleAxial()
{
    bool visible = m_ToggleAxialAction->isChecked();
    ToggleSlicePlaneVisibility(GetDataStorage(), "stdmulti.widget0.plane", visible);
}

void xq_WorkbenchWindowAdvisor::OnToggleSagittal()
{
    bool visible = m_ToggleSagittalAction->isChecked();
    ToggleSlicePlaneVisibility(GetDataStorage(), "stdmulti.widget1.plane", visible);
}

void xq_WorkbenchWindowAdvisor::OnToggleCoronal()
{
    bool visible = m_ToggleCoronalAction->isChecked();
    ToggleSlicePlaneVisibility(GetDataStorage(), "stdmulti.widget2.plane", visible);
}

void xq_WorkbenchWindowAdvisor::OnToggleLogging()
{
    static constexpr const char* kLogView = "org.blueberry.views.logview";

    berry::IWorkbenchWindow::Pointer window = GetWindowConfigurer()->GetWindow();
    if (window.IsNull())
        return;

    berry::IWorkbenchPage::Pointer page = window->GetActivePage();
    if (page.IsNull())
        return;

    // Toggle semantics: if the view is already present, hide it; otherwise
    // open it (Berry materialises it in the kLogSidebarFolderId placeholder
    // declared by xq_DefaultPerspective, to the right of Standard Display).
    berry::IViewPart::Pointer existing = page->FindView(kLogView);
    if (existing.IsNotNull())
    {
        page->HideView(existing);
        if (m_ToggleLoggingAction)
            m_ToggleLoggingAction->setChecked(false);
        return;
    }

    try
    {
        page->ShowView(kLogView);
        if (m_ToggleLoggingAction)
            m_ToggleLoggingAction->setChecked(true);
    }
    catch (const berry::PartInitException& e)
    {
        BERRY_ERROR << "Could not open log view: " << e.what();
        if (m_ToggleLoggingAction)
            m_ToggleLoggingAction->setChecked(false);
    }
}

void xq_WorkbenchWindowAdvisor::OnWelcome()
{
    // Try to show the Intro view (welcome screen)
    berry::IWorkbenchWindow::Pointer window = GetWindowConfigurer()->GetWindow();
    if (window.IsNull()) return;

    try {
        auto* workbench = berry::PlatformUI::GetWorkbench();
        if (workbench)
            workbench->GetIntroManager()->ShowIntro(window, false);
    }
    catch (...) {
        QMessageBox::information(
            qobject_cast<QWidget*>(window->GetShell()->GetControl()),
            "Welcome to XQ",
            "Welcome to XQ — a cardiovascular analysis application.\n\n"
            "Use the File / View / Tools menus and the top tool buttons "
            "to navigate the workflow:\n"
            "Images → Paths → Segmentations → Modeling → Simulation.");
    }
}

void xq_WorkbenchWindowAdvisor::OnRecentProject(const QString& projectPath)
{
    QFileInfo fi(projectPath);
    if (!fi.exists()) {
        QMessageBox::warning(
            qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
            "Recent Project",
            QString("Project file not found:\n%1").arg(projectPath));
        RemoveRecentProject(projectPath);
        return;
    }

    // Use xq_WorkspaceManager to open the project
    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull()) return;

    try {
        xq_WorkspaceManager pm;
        pm.OpenProject(ds, projectPath.toStdString());

        QString projectName = fi.dir().dirName();
        QMainWindow* mainWindow = qobject_cast<QMainWindow*>(
            GetWindowConfigurer()->GetWindow()->GetShell()->GetControl());
        if (mainWindow)
            mainWindow->setWindowTitle(QString("XQ - %1").arg(projectName));

        AddRecentProject(projectPath);
    }
    catch (const std::exception& e) {
        QMessageBox::warning(
            qobject_cast<QWidget*>(GetWindowConfigurer()->GetWindow()->GetShell()->GetControl()),
            "Open Recent Project",
            QString("Failed to open project:\n%1").arg(e.what()));
    }
}

void xq_WorkbenchWindowAdvisor::AddRecentProject(const QString& projectPath)
{
    if (!m_RecentProjectsMenu) return;

    m_RecentProjects.removeAll(projectPath);
    m_RecentProjects.prepend(projectPath);
    while (m_RecentProjects.size() > 8)
        m_RecentProjects.removeLast();

    SaveRecentProjects();
    RebuildRecentProjectsMenu();
}

void xq_WorkbenchWindowAdvisor::RemoveRecentProject(const QString& projectPath)
{
    m_RecentProjects.removeAll(projectPath);
    SaveRecentProjects();
    RebuildRecentProjectsMenu();
}

void xq_WorkbenchWindowAdvisor::LoadRecentProjects()
{
    m_RecentProjects.clear();
    QString rcFile = QDir::homePath() + "/.xq_recent_projects";
    QFile f(rcFile);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty() && QFileInfo::exists(line))
                m_RecentProjects.append(line);
        }
        f.close();
    }
}

void xq_WorkbenchWindowAdvisor::SaveRecentProjects()
{
    QString rcFile = QDir::homePath() + "/.xq_recent_projects";
    QFile f(rcFile);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        for (const auto& p : m_RecentProjects)
            out << p << "\n";
        f.close();
    }
}

void xq_WorkbenchWindowAdvisor::RebuildRecentProjectsMenu()
{
    if (!m_RecentProjectsMenu) return;
    m_RecentProjectsMenu->clear();

    if (m_RecentProjects.isEmpty()) {
        m_RecentProjectsMenu->addAction("(No recent projects)")->setEnabled(false);
        return;
    }

    for (const auto& path : m_RecentProjects) {
        QFileInfo fi(path);
        QString label = fi.dir().dirName() + " — " + fi.absoluteFilePath();
        QAction* action = m_RecentProjectsMenu->addAction(label);
        connect(action, &QAction::triggered, this, [this, path]() {
            OnRecentProject(path);
        });
    }

    m_RecentProjectsMenu->addSeparator();
    QAction* clearAction = m_RecentProjectsMenu->addAction("Clear Recent Projects");
    connect(clearAction, &QAction::triggered, this, [this]() {
        m_RecentProjects.clear();
        SaveRecentProjects();
        RebuildRecentProjectsMenu();
    });
}

void xq_WorkbenchWindowAdvisor::OnScreenshot()
{
    berry::IWorkbenchWindow::Pointer window =
        berry::PlatformUI::GetWorkbench()->GetActiveWorkbenchWindow();
    if (window.IsNull()) return;

    auto* mainWin = static_cast<QMainWindow*>(window->GetShell()->GetControl());
    if (!mainWin) return;

    QString filePath = QFileDialog::getSaveFileName(
        mainWin, "Save Screenshot", QDir::homePath() + "/screenshot.png",
        "PNG Image (*.png);;JPEG Image (*.jpg);;BMP Image (*.bmp)");

    if (filePath.isEmpty()) return;

    QScreen* screen = QApplication::primaryScreen();
    if (!screen) return;

    QPixmap pixmap = screen->grabWindow(mainWin->winId());
    if (pixmap.save(filePath))
    {
        if (m_SelectionInfoLabel)
            m_SelectionInfoLabel->setText("Screenshot saved: " + QFileInfo(filePath).fileName());
    }
    else
    {
        QMessageBox::warning(mainWin, "Screenshot Error",
            "Failed to save screenshot to:\n" + filePath);
    }
}

void xq_WorkbenchWindowAdvisor::OnMeasureDistance()
{
    MeasureDistance();
}

void xq_WorkbenchWindowAdvisor::OnMeasureAngle()
{
    MeasureAngle();
}

void xq_WorkbenchWindowAdvisor::MeasureDistance()
{
    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull()) return;

    QMainWindow* mainWin = qobject_cast<QMainWindow*>(
        berry::WorkbenchWindowAdvisor::GetWindowConfigurer()->GetWindow()->GetShell()->GetControl());

    QDialog dialog(mainWin);
    dialog.setWindowTitle("Distance Between Points");
    auto* layout = new QFormLayout(&dialog);

    double defaultAx = 0, defaultAy = 0, defaultAz = 0;

    // If a surface node is selected, use its bounding box center for Point A
    auto allNodes = ds->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        mitk::DataNode::Pointer n = it->Value();
        bool selected = false;
        n->GetBoolProperty("selected", selected);
        if (selected)
        {
            mitk::Surface* surf = dynamic_cast<mitk::Surface*>(n->GetData());
            if (surf && surf->GetVtkPolyData())
            {
                double bounds[6];
                surf->GetVtkPolyData()->GetBounds(bounds);
                defaultAx = (bounds[0] + bounds[1]) / 2.0;
                defaultAy = (bounds[2] + bounds[3]) / 2.0;
                defaultAz = (bounds[4] + bounds[5]) / 2.0;
                break;
            }
        }
    }

    auto makeSpin = [](double val) {
        auto* sb = new QDoubleSpinBox();
        sb->setRange(-1e6, 1e6);
        sb->setDecimals(4);
        sb->setValue(val);
        return sb;
    };

    auto* ax = makeSpin(defaultAx); auto* ay = makeSpin(defaultAy); auto* az = makeSpin(defaultAz);
    auto* bx = makeSpin(0); auto* by = makeSpin(0); auto* bz = makeSpin(0);

    layout->addRow("Point A - X:", ax);
    layout->addRow("Point A - Y:", ay);
    layout->addRow("Point A - Z:", az);
    layout->addRow("Point B - X:", bx);
    layout->addRow("Point B - Y:", by);
    layout->addRow("Point B - Z:", bz);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    double pA[3] = {ax->value(), ay->value(), az->value()};
    double pB[3] = {bx->value(), by->value(), bz->value()};

    double dx = pB[0] - pA[0];
    double dy = pB[1] - pA[1];
    double dz = pB[2] - pA[2];
    double dist = std::sqrt(dx*dx + dy*dy + dz*dz);

    QMessageBox::information(mainWin, "Distance Measurement",
        QString("Distance: %1 mm").arg(dist, 0, 'f', 4));

    // Create point nodes for visualization
    mitk::PointSet::Pointer pts = mitk::PointSet::New();
    mitk::Point3D ptA; ptA[0] = pA[0]; ptA[1] = pA[1]; ptA[2] = pA[2];
    mitk::Point3D ptB; ptB[0] = pB[0]; ptB[1] = pB[1]; ptB[2] = pB[2];
    pts->InsertPoint(0, ptA);
    pts->InsertPoint(1, ptB);

    mitk::DataNode::Pointer ptNode = mitk::DataNode::New();
    ptNode->SetData(pts);
    ptNode->SetName("Distance Measurement Points");
    ptNode->SetColor(1.0f, 1.0f, 0.0f);
    ptNode->SetFloatProperty("pointsize", 3.0f);
    ds->Add(ptNode);

    // Create line connecting the two points
    auto lineSource = vtkSmartPointer<vtkLineSource>::New();
    lineSource->SetPoint1(pA);
    lineSource->SetPoint2(pB);
    lineSource->Update();

    mitk::Surface::Pointer lineSurf = mitk::Surface::New();
    lineSurf->SetVtkPolyData(lineSource->GetOutput());

    mitk::DataNode::Pointer lineNode = mitk::DataNode::New();
    lineNode->SetData(lineSurf);
    lineNode->SetName("Distance Measurement Line");
    lineNode->SetColor(0.0f, 0.0f, 1.0f);
    lineNode->SetFloatProperty("line width", 2.0f);
    ds->Add(lineNode);

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_WorkbenchWindowAdvisor::MeasureAngle()
{
    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull()) return;

    QMainWindow* mainWin = qobject_cast<QMainWindow*>(
        berry::WorkbenchWindowAdvisor::GetWindowConfigurer()->GetWindow()->GetShell()->GetControl());

    QDialog dialog(mainWin);
    dialog.setWindowTitle("Angle Between Points");
    auto* layout = new QFormLayout(&dialog);

    auto makeSpin = [](double val) {
        auto* sb = new QDoubleSpinBox();
        sb->setRange(-1e6, 1e6);
        sb->setDecimals(4);
        sb->setValue(val);
        return sb;
    };

    auto* ax = makeSpin(0); auto* ay = makeSpin(0); auto* az = makeSpin(0);
    auto* bbx = makeSpin(0); auto* bby = makeSpin(0); auto* bbz = makeSpin(0);
    auto* cx = makeSpin(0); auto* cy = makeSpin(0); auto* cz = makeSpin(0);

    layout->addRow("Point A - X:", ax);
    layout->addRow("Point A - Y:", ay);
    layout->addRow("Point A - Z:", az);
    layout->addRow("Point B - X (vertex):", bbx);
    layout->addRow("Point B - Y (vertex):", bby);
    layout->addRow("Point B - Z (vertex):", bbz);
    layout->addRow("Point C - X:", cx);
    layout->addRow("Point C - Y:", cy);
    layout->addRow("Point C - Z:", cz);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    double pA[3] = {ax->value(), ay->value(), az->value()};
    double pB[3] = {bbx->value(), bby->value(), bbz->value()};
    double pC[3] = {cx->value(), cy->value(), cz->value()};

    // Vectors BA and BC
    double ba[3] = {pA[0] - pB[0], pA[1] - pB[1], pA[2] - pB[2]};
    double bc[3] = {pC[0] - pB[0], pC[1] - pB[1], pC[2] - pB[2]};

    double dot = ba[0]*bc[0] + ba[1]*bc[1] + ba[2]*bc[2];
    double magBA = std::sqrt(ba[0]*ba[0] + ba[1]*ba[1] + ba[2]*ba[2]);
    double magBC = std::sqrt(bc[0]*bc[0] + bc[1]*bc[1] + bc[2]*bc[2]);

    if (magBA < 1e-12 || magBC < 1e-12)
    {
        QMessageBox::warning(mainWin, "Angle Measurement", "Two or more points are coincident.");
        return;
    }

    double cosAngle = dot / (magBA * magBC);
    cosAngle = std::max(-1.0, std::min(1.0, cosAngle));
    double angleDeg = std::acos(cosAngle) * 180.0 / kPi;

    QMessageBox::information(mainWin, "Angle Measurement",
        QString("Angle at B: %1°").arg(angleDeg, 0, 'f', 2));

    // Create point nodes
    mitk::PointSet::Pointer pts = mitk::PointSet::New();
    mitk::Point3D ptA; ptA[0] = pA[0]; ptA[1] = pA[1]; ptA[2] = pA[2];
    mitk::Point3D ptB; ptB[0] = pB[0]; ptB[1] = pB[1]; ptB[2] = pB[2];
    mitk::Point3D ptC; ptC[0] = pC[0]; ptC[1] = pC[1]; ptC[2] = pC[2];
    pts->InsertPoint(0, ptA);
    pts->InsertPoint(1, ptB);
    pts->InsertPoint(2, ptC);

    mitk::DataNode::Pointer ptNode = mitk::DataNode::New();
    ptNode->SetData(pts);
    ptNode->SetName("Angle Measurement Points");
    ptNode->SetColor(0.0f, 1.0f, 1.0f);
    ptNode->SetFloatProperty("pointsize", 3.0f);
    ds->Add(ptNode);

    // Line A→B
    auto lineAB = vtkSmartPointer<vtkLineSource>::New();
    lineAB->SetPoint1(pA);
    lineAB->SetPoint2(pB);
    lineAB->Update();
    mitk::Surface::Pointer surfAB = mitk::Surface::New();
    surfAB->SetVtkPolyData(lineAB->GetOutput());
    mitk::DataNode::Pointer nodeAB = mitk::DataNode::New();
    nodeAB->SetData(surfAB);
    nodeAB->SetName("Angle Measurement Line A-B");
    nodeAB->SetColor(0.0f, 0.0f, 1.0f);
    nodeAB->SetFloatProperty("line width", 2.0f);
    ds->Add(nodeAB);

    // Line B→C
    auto lineBC = vtkSmartPointer<vtkLineSource>::New();
    lineBC->SetPoint1(pB);
    lineBC->SetPoint2(pC);
    lineBC->Update();
    mitk::Surface::Pointer surfBC = mitk::Surface::New();
    surfBC->SetVtkPolyData(lineBC->GetOutput());
    mitk::DataNode::Pointer nodeBC = mitk::DataNode::New();
    nodeBC->SetData(surfBC);
    nodeBC->SetName("Angle Measurement Line B-C");
    nodeBC->SetColor(0.0f, 0.0f, 1.0f);
    nodeBC->SetFloatProperty("line width", 2.0f);
    ds->Add(nodeBC);

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();
}

void xq_WorkbenchWindowAdvisor::MeasureArea()
{
    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull()) return;

    QMainWindow* mainWin = qobject_cast<QMainWindow*>(
        berry::WorkbenchWindowAdvisor::GetWindowConfigurer()->GetWindow()->GetShell()->GetControl());

    // Find selected surface node
    mitk::DataNode::Pointer surfNode;
    auto allNodes = ds->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        mitk::DataNode::Pointer n = it->Value();
        bool selected = false;
        n->GetBoolProperty("selected", selected);
        if (selected && dynamic_cast<mitk::Surface*>(n->GetData()))
        {
            surfNode = n;
            break;
        }
    }

    if (surfNode.IsNull())
    {
        QMessageBox::warning(mainWin, "Surface Area",
            "Please select a surface node in the Data Manager first.");
        return;
    }

    mitk::Surface* surf = dynamic_cast<mitk::Surface*>(surfNode->GetData());
    vtkPolyData* polyData = surf ? surf->GetVtkPolyData() : nullptr;
    if (!polyData)
    {
        QMessageBox::warning(mainWin, "Surface Area",
            "Selected node does not contain valid surface data.");
        return;
    }

    auto tri = vtkSmartPointer<vtkTriangleFilter>::New();
    tri->SetInputData(polyData);
    tri->Update();

    auto mass = vtkSmartPointer<vtkMassProperties>::New();
    mass->SetInputConnection(tri->GetOutputPort());
    mass->Update();

    double area = mass->GetSurfaceArea();

    QMessageBox::information(mainWin, "Surface Area",
        QString("Surface Area: %1 mm\u00B2").arg(area, 0, 'f', 2));
}

void xq_WorkbenchWindowAdvisor::MeasureVolume()
{
    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull()) return;

    QMainWindow* mainWin = qobject_cast<QMainWindow*>(
        berry::WorkbenchWindowAdvisor::GetWindowConfigurer()->GetWindow()->GetShell()->GetControl());

    // Find selected surface node
    mitk::DataNode::Pointer surfNode;
    auto allNodes = ds->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        mitk::DataNode::Pointer n = it->Value();
        bool selected = false;
        n->GetBoolProperty("selected", selected);
        if (selected && dynamic_cast<mitk::Surface*>(n->GetData()))
        {
            surfNode = n;
            break;
        }
    }

    if (surfNode.IsNull())
    {
        QMessageBox::warning(mainWin, "Volume",
            "Please select a surface node in the Data Manager first.");
        return;
    }

    mitk::Surface* surf = dynamic_cast<mitk::Surface*>(surfNode->GetData());
    vtkPolyData* polyData = surf ? surf->GetVtkPolyData() : nullptr;
    if (!polyData)
    {
        QMessageBox::warning(mainWin, "Volume",
            "Selected node does not contain valid surface data.");
        return;
    }

    auto tri = vtkSmartPointer<vtkTriangleFilter>::New();
    tri->SetInputData(polyData);
    tri->Update();

    auto mass = vtkSmartPointer<vtkMassProperties>::New();
    mass->SetInputConnection(tri->GetOutputPort());
    mass->Update();

    double area = mass->GetSurfaceArea();
    double volume = mass->GetVolume();
    double ratio = (area > 1e-12) ? (volume / area) : 0.0;

    QMessageBox::information(mainWin, "Volume",
        QString("Surface Area: %1 mm\u00B2\nVolume: %2 mm\u00B3\nVolume-to-Surface Ratio: %3 mm")
            .arg(area, 0, 'f', 2)
            .arg(volume, 0, 'f', 2)
            .arg(ratio, 0, 'f', 2));
}

void xq_WorkbenchWindowAdvisor::OnToggleVolumeRendering()
{
    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull()) return;

    bool enabled = m_VolumeRenderingAction ? m_VolumeRenderingAction->isChecked() : false;

    // Apply volume rendering toggle to all image nodes
    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = ds->GetAll();
    int count = 0;
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        mitk::DataNode::Pointer node = it->Value();
        if (!node) continue;

        mitk::BaseData* data = node->GetData();
        if (!data) continue;

        if (dynamic_cast<mitk::Image*>(data))
        {
            node->SetBoolProperty("volumerendering", enabled);
            count++;
        }
    }

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();

    if (m_SelectionInfoLabel)
    {
        if (count > 0)
            m_SelectionInfoLabel->setText(QString("Volume rendering %1 for %2 image(s)")
                .arg(enabled ? "enabled" : "disabled").arg(count));
        else
            m_SelectionInfoLabel->setText("No image nodes found for volume rendering");
    }
}

void xq_WorkbenchWindowAdvisor::OnToggleCrosshair()
{
    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull()) return;

    bool show = m_CrosshairAction ? m_CrosshairAction->isChecked() : true;

    // Toggle crosshair visibility on PlaneGeometryData nodes
    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = ds->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        mitk::DataNode::Pointer node = it->Value();
        if (!node) continue;

        std::string className;
        if (node->GetData())
            className = node->GetData()->GetNameOfClass();

        // MITK creates PlaneGeometryData nodes for crosshairs
        if (className == "PlaneGeometryData" || className == "Geometry2DData")
        {
            node->SetVisibility(show);
        }
    }

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();

    if (m_SelectionInfoLabel)
        m_SelectionInfoLabel->setText(QString("Crosshair %1")
            .arg(show ? "shown" : "hidden"));
}

void xq_WorkbenchWindowAdvisor::ApplyVolumeRenderingPreset()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action) return;

    QString presetName = action->data().toString();

    mitk::DataStorage::Pointer ds = GetDataStorage();
    if (ds.IsNull()) return;

    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = ds->GetAll();
    int count = 0;
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        mitk::DataNode::Pointer node = it->Value();
        if (!node || !node->GetData()) continue;
        if (!dynamic_cast<mitk::Image*>(node->GetData())) continue;

        node->SetBoolProperty("volumerendering", true);

        auto tf = mitk::TransferFunction::New();
        auto* opacityTF = tf->GetScalarOpacityFunction();
        opacityTF->RemoveAllPoints();
        auto* colorTF = tf->GetColorTransferFunction();
        colorTF->RemoveAllPoints();

        if (presetName == "CT Bone")
        {
            opacityTF->AddPoint(200.0, 0.0);
            opacityTF->AddPoint(1500.0, 1.0);
            colorTF->AddRGBPoint(200.0, 0.4, 0.1, 0.1);
            colorTF->AddRGBPoint(1500.0, 1.0, 1.0, 1.0);
        }
        else if (presetName == "CT Soft Tissue")
        {
            opacityTF->AddPoint(-100.0, 0.0);
            opacityTF->AddPoint(400.0, 0.8);
            colorTF->AddRGBPoint(-100.0, 0.6, 0.4, 0.3);
            colorTF->AddRGBPoint(400.0, 1.0, 0.8, 0.7);
        }
        else if (presetName == "CT Lung")
        {
            opacityTF->AddPoint(-900.0, 0.0);
            opacityTF->AddPoint(-500.0, 0.6);
            opacityTF->AddPoint(0.0, 0.0);
            colorTF->AddRGBPoint(-900.0, 0.2, 0.3, 0.8);
            colorTF->AddRGBPoint(-500.0, 0.3, 0.9, 0.9);
        }
        else if (presetName == "CT Vessel (Angiography)")
        {
            opacityTF->AddPoint(100.0, 0.0);
            opacityTF->AddPoint(500.0, 1.0);
            colorTF->AddRGBPoint(100.0, 0.8, 0.0, 0.0);
            colorTF->AddRGBPoint(500.0, 1.0, 0.2, 0.2);
        }
        else if (presetName == "MR Default")
        {
            opacityTF->AddPoint(0.0, 0.0);
            opacityTF->AddPoint(1000.0, 0.8);
            colorTF->AddRGBPoint(0.0, 0.0, 0.0, 0.0);
            colorTF->AddRGBPoint(1000.0, 1.0, 1.0, 1.0);
        }

        node->SetProperty("TransferFunction", mitk::TransferFunctionProperty::New(tf));
        count++;
    }

    mitk::RenderingManager::GetInstance()->RequestUpdateAll();

    if (m_SelectionInfoLabel)
    {
        if (count > 0)
            m_SelectionInfoLabel->setText(
                QString("Applied \"%1\" preset to %2 image(s)").arg(presetName).arg(count));
        else
            m_SelectionInfoLabel->setText("No image nodes found for volume rendering preset");
    }
}

void xq_WorkbenchWindowAdvisor::OpenPreferencesDialog()
{
    QWidget* parent = qobject_cast<QWidget*>(
        GetWindowConfigurer()->GetWindow()->GetShell()->GetControl());

    auto* rootPrefs = mitk::CoreServices::GetPreferencesService()->GetSystemPreferences();
    auto* prefs = rootPrefs->Node("/org.xq.preferences");
    auto* simPrefs = rootPrefs->Node("/org.xq.views.simulation");

    QDialog dialog(parent);
    dialog.setWindowTitle("XQ Preferences");
    dialog.setMinimumSize(500, 400);

    auto* mainLayout = new QVBoxLayout(&dialog);
    auto* tabWidget = new QTabWidget(&dialog);

    // --- General tab ---
    auto* generalTab = new QWidget;
    auto* generalLayout = new QFormLayout(generalTab);

    auto* restoreWorkspace = new QCheckBox("Restore last workspace on startup");
    restoreWorkspace->setChecked(prefs->GetBool("general.restoreWorkspace", false));
    generalLayout->addRow(restoreWorkspace);

    auto* showWelcome = new QCheckBox("Show Welcome screen on startup");
    showWelcome->setChecked(prefs->GetBool("general.showWelcome", true));
    generalLayout->addRow(showWelcome);

    auto* autoSave = new QSpinBox;
    autoSave->setRange(0, 60);
    autoSave->setValue(prefs->GetInt("general.autoSaveInterval", 5));
    generalLayout->addRow("Auto-save interval (minutes):", autoSave);

    auto* themeLabel = new QLabel("Flat Minimalist (Light)");
    themeLabel->setStyleSheet("color: #64748B;");
    generalLayout->addRow("Theme:", themeLabel);

    tabWidget->addTab(generalTab, "General");

    // --- Simulation tab ---
    auto* simTab = new QWidget;
    auto* simLayout = new QFormLayout(simTab);

    auto* solverPath = new QLineEdit(QString::fromStdString(
        simPrefs->Get("solverPath", prefs->Get("sim.solverPath", ""))));
    auto* solverBrowse = new QPushButton("Browse...");
    auto* solverRow = new QHBoxLayout;
    solverRow->addWidget(solverPath);
    solverRow->addWidget(solverBrowse);
    simLayout->addRow("Solver executable path:", solverRow);
    QObject::connect(solverBrowse, &QPushButton::clicked, &dialog, [&]() {
        auto path = QFileDialog::getOpenFileName(&dialog, "Select Solver Executable");
        if (!path.isEmpty()) solverPath->setText(path);
    });

    auto* mpiPath = new QLineEdit(QString::fromStdString(
        simPrefs->Get("mpiPath", prefs->Get("sim.mpiPath", "mpiexec"))));
    auto* mpiBrowse = new QPushButton("Browse...");
    auto* mpiRow = new QHBoxLayout;
    mpiRow->addWidget(mpiPath);
    mpiRow->addWidget(mpiBrowse);
    simLayout->addRow("MPI executable path:", mpiRow);
    QObject::connect(mpiBrowse, &QPushButton::clicked, &dialog, [&]() {
        auto path = QFileDialog::getOpenFileName(&dialog, "Select MPI Executable");
        if (!path.isEmpty()) mpiPath->setText(path);
    });

    auto* numProcs = new QSpinBox;
    numProcs->setRange(1, 128);
    numProcs->setValue(simPrefs->GetInt("numProcessors", prefs->GetInt("sim.numProcessors", 4)));
    simLayout->addRow("Default number of processors:", numProcs);

    tabWidget->addTab(simTab, "Simulation");

    // --- Visualization tab ---
    auto* vizTab = new QWidget;
    auto* vizLayout = new QFormLayout(vizTab);

    auto* antialiasing = new QCheckBox("Enable antialiasing");
    antialiasing->setChecked(prefs->GetBool("viz.antialiasing", true));
    vizLayout->addRow(antialiasing);

    auto* orientWidget = new QCheckBox("Show orientation widget");
    orientWidget->setChecked(prefs->GetBool("viz.orientationWidget", true));
    vizLayout->addRow(orientWidget);

    auto* bgColor = new QComboBox;
    bgColor->addItems({"Black", "White", "Gradient Blue", "Gradient Gray"});
    auto savedBg = QString::fromStdString(prefs->Get("viz.backgroundColor", "Black"));
    int bgIdx = bgColor->findText(savedBg);
    if (bgIdx >= 0) bgColor->setCurrentIndex(bgIdx);
    vizLayout->addRow("Background color:", bgColor);

    auto* lwRange = new QSpinBox;
    lwRange->setRange(0, 10000);
    lwRange->setValue(prefs->GetInt("viz.levelWindowRange", 1000));
    vizLayout->addRow("Default level window range:", lwRange);

    tabWidget->addTab(vizTab, "Visualization");

    // --- Paths tab ---
    auto* pathsTab = new QWidget;
    auto* pathsLayout = new QFormLayout(pathsTab);

    auto* projectDir = new QLineEdit(QString::fromStdString(prefs->Get("paths.projectDir", "")));
    auto* projectBrowse = new QPushButton("Browse...");
    auto* projectRow = new QHBoxLayout;
    projectRow->addWidget(projectDir);
    projectRow->addWidget(projectBrowse);
    pathsLayout->addRow("Default project directory:", projectRow);
    QObject::connect(projectBrowse, &QPushButton::clicked, &dialog, [&]() {
        auto dir = QFileDialog::getExistingDirectory(&dialog, "Select Default Project Directory");
        if (!dir.isEmpty()) projectDir->setText(dir);
    });

    auto* exportDir = new QLineEdit(QString::fromStdString(prefs->Get("paths.exportDir", "")));
    auto* exportBrowse = new QPushButton("Browse...");
    auto* exportRow = new QHBoxLayout;
    exportRow->addWidget(exportDir);
    exportRow->addWidget(exportBrowse);
    pathsLayout->addRow("Default export directory:", exportRow);
    QObject::connect(exportBrowse, &QPushButton::clicked, &dialog, [&]() {
        auto dir = QFileDialog::getExistingDirectory(&dialog, "Select Default Export Directory");
        if (!dir.isEmpty()) exportDir->setText(dir);
    });

    tabWidget->addTab(pathsTab, "Paths");

    mainLayout->addWidget(tabWidget);

    auto* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    mainLayout->addWidget(buttonBox);

    auto savePrefs = [&]() {
        prefs->PutBool("general.restoreWorkspace", restoreWorkspace->isChecked());
        prefs->PutBool("general.showWelcome", showWelcome->isChecked());
        prefs->PutInt("general.autoSaveInterval", autoSave->value());
        // Theme is now fixed to "Flat Minimalist (Light)"

        prefs->Put("sim.solverPath", solverPath->text().toStdString());
        prefs->Put("sim.mpiPath", mpiPath->text().toStdString());
        prefs->PutInt("sim.numProcessors", numProcs->value());
        simPrefs->Put("solverPath", solverPath->text().toStdString());
        simPrefs->Put("mpiPath", mpiPath->text().toStdString());
        simPrefs->PutInt("numProcessors", numProcs->value());

        prefs->PutBool("viz.antialiasing", antialiasing->isChecked());
        prefs->PutBool("viz.orientationWidget", orientWidget->isChecked());
        prefs->Put("viz.backgroundColor", bgColor->currentText().toStdString());
        prefs->PutInt("viz.levelWindowRange", lwRange->value());

        prefs->Put("paths.projectDir", projectDir->text().toStdString());
        prefs->Put("paths.exportDir", exportDir->text().toStdString());

        prefs->Flush();
    };

    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [&]() {
        savePrefs();
        dialog.accept();
    });
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog, [&]() {
        savePrefs();
    });

    dialog.exec();
}
