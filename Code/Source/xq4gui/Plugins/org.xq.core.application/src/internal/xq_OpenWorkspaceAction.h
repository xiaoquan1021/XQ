#ifndef XQ_FILE_OPEN_PROJECT_ACTION_H
#define XQ_FILE_OPEN_PROJECT_ACTION_H

#include <QAction>
#include <berryIWorkbenchWindow.h>

class xq_OpenWorkspaceAction : public QAction
{
    Q_OBJECT

public:
    xq_OpenWorkspaceAction(berry::IWorkbenchWindow::Pointer window);
    xq_OpenWorkspaceAction(const QIcon& icon, berry::IWorkbenchWindow::Pointer window);

protected slots:
    void Run();

private:
    void Init(berry::IWorkbenchWindow::Pointer window);
    berry::IWorkbenchWindow* m_Window;
};

#endif // XQ_FILE_OPEN_PROJECT_ACTION_H
