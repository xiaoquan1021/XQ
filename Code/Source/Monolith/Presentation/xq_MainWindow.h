#ifndef XQ_MAINWINDOW_H
#define XQ_MAINWINDOW_H

#include <QHash>
#include <QMainWindow>

class QAction;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTextEdit;
class QTreeView;
class QWidget;

namespace xq::core
{
class ApplicationContext;
struct ProjectMetadata;
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
    void SetRenderHost(QWidget* renderHost);

private:
    QWidget* CreateWorkflowPage(const QString& id, const QString& title);
    void AddWorkflowPage(const QString& id, const QString& title);
    void RemoveSelectedData();
    void RunActiveWorkflowAction();
    void SaveProject();
    void SyncWorkflowNavigationFromCore(const QString& workflowId);
    void UpdateWorkflowContextStatusPage();
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
    QHash<QString, QLabel*> m_WorkflowContextStatusLabels;
    QHash<QString, QPushButton*> m_WorkflowPrimaryActionButtons;
    QAction* m_SaveProjectAction = nullptr;
    QAction* m_RemoveDataAction = nullptr;
    QListWidget* m_Navigation = nullptr;
    QStackedWidget* m_Pages = nullptr;
    QWidget* m_RenderHostContainer = nullptr;
    QWidget* m_RenderHost = nullptr;
    QTextEdit* m_Diagnostics = nullptr;
    bool m_SyncingSelectionFromCore = false;
};

} // namespace xq::presentation

#endif // XQ_MAINWINDOW_H
