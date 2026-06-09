#ifndef XQ_MAINWINDOW_H
#define XQ_MAINWINDOW_H

#include <QHash>
#include <QMainWindow>

class QAction;
class QActionGroup;
class QComboBox;
class QDockWidget;
class QFormLayout;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTextEdit;
class QTreeView;
class QWidget;

namespace xq::core
{
class ApplicationContext;
class DataImportCommand;
struct ProjectMetadata;
struct TaskRecord;
}

namespace xq::presentation
{

class DataHierarchyModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(xq::core::ApplicationContext& context,
                        QWidget* parent = nullptr);
    void SetDataImportCommand(xq::core::DataImportCommand* command);
    void SetRenderHost(QWidget* renderHost);
    void SetImageNavigatorWidget(QWidget* imageNavigator);

private:
    QWidget* CreateWorkflowPage(const QString& id, const QString& title);
    void AddWorkflowPage(const QString& id, const QString& title);
    void AppendTaskHistoryRow(const xq::core::TaskRecord& task);
    void ImportData();
    void RemoveSelectedData();
    void RunActiveWorkflowAction();
    void SaveProject();
    void SyncWorkflowNavigationFromCore(const QString& workflowId);
    void UpdateWorkflowToolbarSelection(const QString& workflowId);
    void UpdateWorkflowContextStatusPage();
    void StoreWorkflowPointListParameter(const QString& workflowId,
                                         const QString& operationId,
                                         const QString& parameterId,
                                         const QString& text);
    void UpdateWorkflowOperationControls();
    void UpdateWorkflowParameterEditorValue(const QString& workflowId,
                                            const QString& parameterId,
                                            const QVariant& value);
    void RebuildWorkflowParameterPanel(const QString& workflowId);
    void UpdateDataWorkflowPage();
    void UpdateProjectPage(const xq::core::ProjectMetadata* project);
    void UpdateProjectPageDataCount();
    void UpdateProjectWindowState(const xq::core::ProjectMetadata& project);
    void UpdateProjectActions();
    void SyncTreeSelectionFromCore(const QString& hierarchyNodeId);
    void UpdateDataActions();

    xq::core::ApplicationContext& m_Context;
    DataHierarchyModel* m_DataHierarchyModel = nullptr;
    QTreeView* m_DataHierarchyView = nullptr;
    QLabel* m_ProjectNameLabel = nullptr;
    QLabel* m_ProjectPathLabel = nullptr;
    QLabel* m_ProjectSchemaLabel = nullptr;
    QLabel* m_ProjectDataCountLabel = nullptr;
    QLabel* m_DataSelectionLabel = nullptr;
    QLabel* m_DataCatalogIdLabel = nullptr;
    QLabel* m_DataDisplayNameLabel = nullptr;
    QLabel* m_DataSourcePathLabel = nullptr;
    QLabel* m_DataWorkflowRoleLabel = nullptr;
    QHash<QString, QWidget*> m_WorkflowParameterPanels;
    QHash<QString, QComboBox*> m_WorkflowOperationSelectors;
    QHash<QString, QLabel*> m_WorkflowContextStatusLabels;
    QHash<QString, QPushButton*> m_WorkflowPrimaryActionButtons;
    QHash<QString, QAction*> m_WorkflowToolbarActions;
    QActionGroup* m_WorkflowToolbarActionGroup = nullptr;
    QDockWidget* m_DataManagerDock = nullptr;
    QAction* m_SaveProjectAction = nullptr;
    QAction* m_ImportDataAction = nullptr;
    QAction* m_RemoveDataAction = nullptr;
    xq::core::DataImportCommand* m_DataImportCommand = nullptr;
    QListWidget* m_Navigation = nullptr;
    QStackedWidget* m_Pages = nullptr;
    QDockWidget* m_ImageNavigatorDock = nullptr;
    QDockWidget* m_WorkflowToolsDock = nullptr;
    QDockWidget* m_DiagnosticsDock = nullptr;
    QDockWidget* m_TaskHistoryDock = nullptr;
    QWidget* m_RenderHostContainer = nullptr;
    QWidget* m_RenderHost = nullptr;
    QTextEdit* m_Diagnostics = nullptr;
    QTableWidget* m_TaskHistoryTable = nullptr;
    bool m_SyncingSelectionFromCore = false;
};

} // namespace xq::presentation

#endif // XQ_MAINWINDOW_H
