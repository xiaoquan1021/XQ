#include "xq_ImportLegacyAction.h"
#include "xq_ApplicationPluginActivator.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QApplication>

#include <mitkIDataStorageService.h>
#include <mitkIDataStorageReference.h>
#include <mitkStatusBar.h>

#include <xq_LegacyImporter.h>

xq_ImportLegacyAction::xq_ImportLegacyAction(
    berry::IWorkbenchWindow::Pointer window)
    : QAction(nullptr)
{
    Init(window);
}

xq_ImportLegacyAction::xq_ImportLegacyAction(
    const QIcon& icon, berry::IWorkbenchWindow::Pointer window)
    : QAction(nullptr)
{
    setIcon(icon);
    Init(window);
}

void xq_ImportLegacyAction::Init(berry::IWorkbenchWindow::Pointer window)
{
    m_Window = window.GetPointer();
    setText("Import &Legacy Project...");
    setToolTip("Import a compatible legacy vascular project folder (.svproj)");
    connect(this, &QAction::triggered, this, &xq_ImportLegacyAction::Run);
}

void xq_ImportLegacyAction::Run()
{
    QString projectDir = QFileDialog::getExistingDirectory(
        nullptr, "Select Legacy Vascular Project Folder", QDir::homePath());

    if (projectDir.isEmpty())
        return;

    ctkPluginContext* context = xq_ApplicationPluginActivator::getContext();
    if (!context) return;

    ctkServiceReference dsServiceRef =
        context->getServiceReference<mitk::IDataStorageService>();
    if (!dsServiceRef) return;

    mitk::IDataStorageService* dsService =
        context->getService<mitk::IDataStorageService>(dsServiceRef);
    if (!dsService) return;

    mitk::IDataStorageReference::Pointer dsRef = dsService->GetDataStorage();
    context->ungetService(dsServiceRef);

    if (dsRef.IsNull()) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    mitk::StatusBar::GetInstance()->DisplayText("Importing compatible project...");

    xq_LegacyImporter importer;
    bool success = importer.ImportProject(dsRef->GetDataStorage(),
                                           projectDir.toStdString());

    QApplication::restoreOverrideCursor();

    if (!success)
    {
        mitk::StatusBar::GetInstance()->DisplayText("Failed to import compatible project.");

        QString logText;
        for (const auto& msg : importer.GetImportLog())
            logText += QString::fromStdString(msg) + "\n";

        QMessageBox::critical(nullptr, "Import Failed",
            "Failed to import compatible project from:\n" + projectDir +
            "\n\nLog:\n" + logText);
        return;
    }

    QString summary = QString("Successfully imported compatible project: %1\n\nImport log:\n")
                          .arg(QString::fromStdString(importer.GetProjectName()));
    for (const auto& msg : importer.GetImportLog())
        summary += QString::fromStdString(msg) + "\n";

    QMessageBox::information(nullptr, "Import Complete", summary);

    if (m_Window)
    {
        auto shell = m_Window->GetShell();
        if (shell.IsNotNull())
            shell->SetText(QString("XQ - %1 [Imported]").arg(
                QString::fromStdString(importer.GetProjectName())));
    }

    mitk::StatusBar::GetInstance()->DisplayText("Compatible project imported successfully.");
}
