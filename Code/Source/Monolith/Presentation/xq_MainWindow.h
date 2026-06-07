#ifndef XQ_MAINWINDOW_H
#define XQ_MAINWINDOW_H

#include <QMainWindow>

class QAction;
class QListWidget;
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
    QWidget* CreateWorkflowPage(const QString& title);
    void AddWorkflowPage(const QString& title);
    void RemoveSelectedData();
    void UpdateProjectWindowState(const xq::core::ProjectMetadata& project);
    void SyncTreeSelectionFromCore(const QString& hierarchyNodeId);
    void UpdateDataActions();

    xq::core::ApplicationContext& m_Context;
    DataHierarchyModel* m_DataHierarchyModel = nullptr;
    QTreeView* m_DataHierarchyView = nullptr;
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
