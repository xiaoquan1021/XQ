#ifndef XQ_WORKSPACEEXPLORER_H
#define XQ_WORKSPACEEXPLORER_H

#include <QmitkAbstractView.h>
#include <XQ_QT_PROJECTMANAGERExports.h>

#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>

class XQ_QT_PROJECTMANAGER_EXPORT xq_WorkspaceExplorer : public QmitkAbstractView
{
  Q_OBJECT

public:
  static const QString VIEW_ID;

  xq_WorkspaceExplorer();
  ~xq_WorkspaceExplorer() override;

protected:
  void CreateQtPartControl(QWidget* parent) override;
  void SetFocus() override;

private slots:
  void OnNewProject();
  void OnOpenProject();
  void OnRefresh();
  void OnOpenFolder();

private:
  void UpdateProjectTree();

  QTreeWidget* m_ProjectTree;
  QLabel* m_ProjectNameLabel;
  QLabel* m_ProjectPathLabel;
  QPushButton* m_OpenFolderButton;
  QPushButton* m_NewProjectButton;
  QPushButton* m_OpenProjectButton;
  QPushButton* m_RefreshButton;
  QString m_CurrentProjectPath;
};

#endif // XQ_WORKSPACEEXPLORER_H
