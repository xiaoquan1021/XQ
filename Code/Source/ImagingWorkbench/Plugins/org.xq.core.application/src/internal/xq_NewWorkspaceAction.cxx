#include "xq_NewWorkspaceAction.h"
#include "xq_ApplicationPluginActivator.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>

#include <mitkIDataStorageService.h>
#include <mitkIDataStorageReference.h>

#include <xq_WorkspaceManager.h>

xq_NewWorkspaceAction::xq_NewWorkspaceAction(
    berry::IWorkbenchWindow::Pointer window)
    : QAction(nullptr)
{
    Init(window);
}

xq_NewWorkspaceAction::xq_NewWorkspaceAction(
    const QIcon& icon, berry::IWorkbenchWindow::Pointer window)
    : QAction(nullptr)
{
    setIcon(icon);
    Init(window);
}

void xq_NewWorkspaceAction::Init(berry::IWorkbenchWindow::Pointer window)
{
    m_Window = window.GetPointer();
    setText("&New Workspace...");
    setToolTip("Create a new project");
    connect(this, &QAction::triggered, this, &xq_NewWorkspaceAction::Run);
}

void xq_NewWorkspaceAction::Run()
{
    bool ok = false;
    QString projectName = QInputDialog::getText(
        nullptr, "Create Project", "Project name:",
        QLineEdit::Normal, "NewProject", &ok);

    if (!ok || projectName.isEmpty())
        return;

    QString parentDir = QFileDialog::getExistingDirectory(
        nullptr, "Choose project location", QDir::homePath());

    if (parentDir.isEmpty())
        return;

    QString projectDir = parentDir + "/" + projectName;
    if (QDir(projectDir).exists())
    {
        QMessageBox::warning(nullptr, "XQ",
                             "A directory with this name already exists.");
        return;
    }

    // Use ProjectManager to create the project (dirs + .xqproj file)
    xq_WorkspaceManager projectMgr;
    if (!projectMgr.CreateProject(parentDir.toStdString(), projectName.toStdString()))
    {
        QMessageBox::critical(nullptr, "XQ",
                              "Failed to create project directory.");
        return;
    }

    // Get DataStorage and populate it with folder nodes
    ctkPluginContext* context = xq_ApplicationPluginActivator::getContext();
    if (context)
    {
        ctkServiceReference dsServiceRef =
            context->getServiceReference<mitk::IDataStorageService>();
        if (dsServiceRef)
        {
            auto* dsService =
                context->getService<mitk::IDataStorageService>(dsServiceRef);
            if (dsService)
            {
                mitk::IDataStorageReference::Pointer dsRef = dsService->GetDataStorage();
                if (dsRef.IsNotNull())
                {
                    std::string projFilePath = projectDir.toStdString() + "/"
                        + projectName.toStdString() + ".xqproj";
                    projectMgr.OpenProject(dsRef->GetDataStorage(), projFilePath);
                }
            }
            context->ungetService(dsServiceRef);
        }
    }

    // Update window title
    if (m_Window)
    {
        auto shell = m_Window->GetShell();
        if (shell.IsNotNull())
            shell->SetText(QString("XQ - %1").arg(projectName));
    }

    QMessageBox::information(nullptr, "XQ",
                             "Project '" + projectName + "' created at:\n" + projectDir);
}
