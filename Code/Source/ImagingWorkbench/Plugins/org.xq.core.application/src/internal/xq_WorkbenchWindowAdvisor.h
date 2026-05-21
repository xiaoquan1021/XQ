#ifndef XQ_WORKBENCH_WINDOW_ADVISOR_H
#define XQ_WORKBENCH_WINDOW_ADVISOR_H

#include <berryWorkbenchWindowAdvisor.h>
#include <berryIPartListener.h>
#include <berryIPerspectiveListener.h>
#include <berryIWorkbenchPage.h>
#include <berryWorkbenchAdvisor.h>

#include <mitkDataStorage.h>

#include <QAction>
#include <QList>
#include <QHash>
#include <QPointer>
#include <QStringList>

class QMenu;
class QEvent;
class QFrame;
class QHBoxLayout;
class QLabel;
class QTimer;
class QToolBar;
class QActionGroup;
class QButtonGroup;
class QToolButton;
class XQPerspectiveListener;

class xq_WorkbenchWindowAdvisor : public QObject, public berry::WorkbenchWindowAdvisor
{
    Q_OBJECT

    friend class XQPerspectiveListener;

public:
    xq_WorkbenchWindowAdvisor(berry::WorkbenchAdvisor* wbAdvisor,
                              berry::IWorkbenchWindowConfigurer::Pointer configurer);
    ~xq_WorkbenchWindowAdvisor();

    void PreWindowOpen() override;
    void PostWindowCreate() override;
    void PostWindowOpen() override;
    void PostWindowClose() override;

    void SetWindowIcon(const QString& iconPath);

    mitk::DataStorage::Pointer GetDataStorage();

    bool eventFilter(QObject* watched, QEvent* event) override;

protected slots:
    void ShowView(const QString& viewId);
    void OnShowPathPlanning();
    void OnShowSegmentation();
    void OnShowMitkSegmentation();
    void OnShowModeling();
    void OnShowMeshing();
    void OnShowSimulation();
    void OnShowImageProcessing();
    void OnAbout();
    void OnExit();

    void OnSaveAs();
    void OnCloseProject();
    void OnOpenDataFile();
    void OnSaveScene();
    void OnResetPerspective();
    void OnPreferences();
    void OnToggleAxial();
    void OnToggleSagittal();
    void OnToggleCoronal();
    void OnToggleLogging();
    void OnWelcome();
    void OnRecentProject(const QString& projectPath);
    void OnImportDicom();
    void OnScreenshot();
    void OnMeasureDistance();
    void OnMeasureAngle();
    void OnToggleVolumeRendering();
    void OnToggleCrosshair();

private slots:
    void OpenPreferencesDialog();
    void ApplyVolumeRenderingPreset();
    void UpdateStatusInfo();
    void MeasureDistance();
    void MeasureAngle();
    void MeasureArea();
    void MeasureVolume();
    void OnDataManagerDoubleClick();

private:
    void AddRecentProject(const QString& projectPath);
    void RemoveRecentProject(const QString& projectPath);
    void LoadRecentProjects();
    void SaveRecentProjects();
    void RebuildRecentProjectsMenu();
    void ShowImportTools();
    void ShowTraceTools();
    void ShowContourTools();
    void ShowBuildTools();
    void ShowSolveTools();
    void ClearStageToolsBar();
    QToolButton* AddStageDrawerButton(
        const QIcon& icon,
        const QString& label,
        const QString& activeStage,
        const QString& stageKey);
    void SetActiveStageButton(QToolButton* button);
    void EnsureStageDrawerPanel();
    void ActivateStageDrawer(QToolButton* button, const QString& stageKey);
    void DeactivateStageDrawer();
    void RebuildStageDrawer(const QString& stageKey);
    void PositionStageDrawer(QToolButton* button);
    void AddDisplayControlsToRibbon();
    void SetCrosshairGapZero();

    berry::WorkbenchAdvisor* m_WorkbenchAdvisor;
    QString m_WindowIcon;

    QAction* m_SaveProjectAction;
    QAction* m_UndoAction;
    QAction* m_RedoAction;
    QList<QAction*> m_ViewActions;

    QAction* m_SaveAsAction;
    QAction* m_CloseProjectAction;
    QAction* m_OpenDataFileAction;
    QAction* m_SaveSceneAction;
    QAction* m_ResetPerspectiveAction;
    QAction* m_PreferencesAction;
    QAction* m_ToggleAxialAction;
    QAction* m_ToggleSagittalAction;
    QAction* m_ToggleCoronalAction;
    QAction* m_ToggleLoggingAction = nullptr;
    QAction* m_WelcomeAction;
    QAction* m_ImportDicomAction;
    QAction* m_VolumeRenderingAction;
    QAction* m_CrosshairAction;

    QMenu* m_RecentProjectsMenu;
    QStringList m_RecentProjects;
    QLabel* m_SelectionInfoLabel;
    QLabel* m_MemoryLabel;
    QLabel* m_CoordLabel;
    QTimer* m_StatusTimer;
    int m_LastNodeCount;

    QToolBar* m_StageToolsBar;
    QPointer<QToolButton> m_ActiveStageButton;
    QFrame* m_StageDrawerPanel;
    QHBoxLayout* m_StageDrawerLayout;
    QString m_ActiveStageKey;

    QScopedPointer<berry::IPartListener> m_TitlePartListener;
    QScopedPointer<berry::IPerspectiveListener> m_PerspectiveListener;
};

#endif // XQ_WORKBENCH_WINDOW_ADVISOR_H
