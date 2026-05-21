#include "xq_SaveWorkspaceAction.h"
#include "xq_ApplicationPluginActivator.h"

#include <QMessageBox>
#include <QApplication>

#include <mitkIDataStorageService.h>
#include <mitkIDataStorageReference.h>
#include <mitkProgressBar.h>
#include <mitkStatusBar.h>
#include <mitkProperties.h>

#include <xq_WorkspaceManager.h>

xq_SaveWorkspaceAction::xq_SaveWorkspaceAction(
    berry::IWorkbenchWindow::Pointer window)
    : QAction(nullptr)
{
    Init(window);
}

xq_SaveWorkspaceAction::xq_SaveWorkspaceAction(
    const QIcon& icon, berry::IWorkbenchWindow::Pointer window)
    : QAction(nullptr)
{
    setIcon(icon);
    Init(window);
}

void xq_SaveWorkspaceAction::Init(berry::IWorkbenchWindow::Pointer window)
{
    m_Window = window.GetPointer();
    setText("&Save Workspace");
    setToolTip("Save the current project");
    connect(this, &QAction::triggered, this, &xq_SaveWorkspaceAction::Run);
}

void xq_SaveWorkspaceAction::Run()
{
    ctkPluginContext* context = xq_ApplicationPluginActivator::getContext();
    if (!context)
        return;

    ctkServiceReference dsServiceRef =
        context->getServiceReference<mitk::IDataStorageService>();
    if (!dsServiceRef)
        return;

    mitk::IDataStorageService* dsService =
        context->getService<mitk::IDataStorageService>(dsServiceRef);
    if (!dsService)
        return;

    mitk::IDataStorageReference::Pointer dsRef = dsService->GetDataStorage();
    context->ungetService(dsServiceRef);

    if (dsRef.IsNull())
        return;

    mitk::DataStorage::Pointer dataStorage = dsRef->GetDataStorage();

    // Find the project root node (has "project.path" property)
    std::string projDir;
    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = dataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        auto* pathProp = dynamic_cast<mitk::StringProperty*>(
            it->Value()->GetProperty("project.path"));
        if (pathProp)
        {
            projDir = pathProp->GetValue();
            break;
        }
    }

    if (projDir.empty())
    {
        QMessageBox::warning(nullptr, "XQ",
                             "No open project to save.\n"
                             "Please create or open a project first.");
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    mitk::StatusBar::GetInstance()->DisplayText("Saving project...");
    mitk::ProgressBar::GetInstance()->AddStepsToDo(1);

    xq_WorkspaceManager projectMgr;
    bool success = projectMgr.SaveProject(dataStorage, projDir);

    mitk::ProgressBar::GetInstance()->Progress();
    QApplication::restoreOverrideCursor();

    if (success)
    {
        mitk::StatusBar::GetInstance()->DisplayText("Project saved successfully.");
    }
    else
    {
        mitk::StatusBar::GetInstance()->DisplayText("Failed to save project.");
        QMessageBox::critical(nullptr, "XQ",
                              "Failed to save project to:\n"
                              + QString::fromStdString(projDir));
    }
}
