#ifndef XQ_IMPORTLEGACYACTION_H
#define XQ_IMPORTLEGACYACTION_H

#include <berryIWorkbenchWindow.h>
#include <QAction>

class xq_ImportLegacyAction : public QAction
{
    Q_OBJECT

public:
    xq_ImportLegacyAction(berry::IWorkbenchWindow::Pointer window);
    xq_ImportLegacyAction(const QIcon& icon, berry::IWorkbenchWindow::Pointer window);

private slots:
    void Run();

private:
    void Init(berry::IWorkbenchWindow::Pointer window);
    berry::IWorkbenchWindow* m_Window;
};

#endif
