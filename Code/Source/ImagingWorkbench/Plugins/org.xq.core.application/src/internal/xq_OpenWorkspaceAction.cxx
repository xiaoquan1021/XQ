#include "xq_OpenWorkspaceAction.h"
#include "xq_ApplicationPluginActivator.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QApplication>

#include <mitkIDataStorageService.h>
#include <mitkIDataStorageReference.h>
#include <mitkStatusBar.h>
#include <mitkRenderingManager.h>
#include <mitkLogMacros.h>

#include <xq_WorkspaceManager.h>
#include <xq_LegacyImporter.h>

xq_OpenWorkspaceAction::xq_OpenWorkspaceAction(
    berry::IWorkbenchWindow::Pointer window)
    : QAction(nullptr)
{
    Init(window);
}

xq_OpenWorkspaceAction::xq_OpenWorkspaceAction(
    const QIcon& icon, berry::IWorkbenchWindow::Pointer window)
    : QAction(nullptr)
{
    setIcon(icon);
    Init(window);
}

void xq_OpenWorkspaceAction::Init(berry::IWorkbenchWindow::Pointer window)
{
    m_Window = window.GetPointer();
    setText("&Open Workspace...");
    setToolTip("Open a project folder");
    connect(this, &QAction::triggered, this, &xq_OpenWorkspaceAction::Run);
}

void xq_OpenWorkspaceAction::Run()
{
    MITK_INFO << "[XQ-Open] File→Open Project triggered";

    QString projectDir = QFileDialog::getExistingDirectory(
        nullptr, "Open Project", QDir::homePath());

    if (projectDir.isEmpty())
    {
        MITK_INFO << "[XQ-Open] User cancelled directory selection";
        return;
    }

    MITK_INFO << "[XQ-Open] Selected directory: " << projectDir.toStdString();

    ctkPluginContext* context = xq_ApplicationPluginActivator::getContext();
    if (!context)
    {
        MITK_WARN << "[XQ-Open] Plugin context is null!";
        return;
    }

    ctkServiceReference dsServiceRef =
        context->getServiceReference<mitk::IDataStorageService>();
    if (!dsServiceRef)
    {
        MITK_WARN << "[XQ-Open] DataStorage service ref is null!";
        return;
    }

    mitk::IDataStorageService* dsService =
        context->getService<mitk::IDataStorageService>(dsServiceRef);
    if (!dsService)
    {
        MITK_WARN << "[XQ-Open] DataStorage service is null!";
        return;
    }

    mitk::IDataStorageReference::Pointer dsRef = dsService->GetDataStorage();
    context->ungetService(dsServiceRef);

    if (dsRef.IsNull())
    {
        MITK_WARN << "[XQ-Open] DataStorage reference is null!";
        return;
    }

    mitk::DataStorage::Pointer dataStorage = dsRef->GetDataStorage();

    QDir dir(projectDir);
    QStringList xqProjFiles = dir.entryList(QStringList("*.xqproj"), QDir::Files);
    // Use QFileInfo for hidden files — QDir::exists() may skip them
    bool hasSvproj = QFileInfo(projectDir + "/.svproj").exists();
    bool hasImageInfo = QFileInfo(projectDir + "/Images/image_information.xml").exists();
    bool hasImages = QFileInfo(projectDir + "/Images").isDir();
    bool hasModels = QFileInfo(projectDir + "/Models").isDir();
    bool hasMeshes = QFileInfo(projectDir + "/Meshes").isDir();

    MITK_INFO << "[XQ-Open] Detection: xqproj=" << !xqProjFiles.isEmpty()
              << " svproj=" << hasSvproj
              << " imageInfo=" << hasImageInfo
              << " images=" << hasImages
              << " models=" << hasModels
              << " meshes=" << hasMeshes;

    // Detect project type
    bool isXQProject = !xqProjFiles.isEmpty();
    bool isSVProject = hasSvproj || hasImageInfo;
    bool isProjectFolder = hasImages || hasModels || hasMeshes;

    if (!isXQProject && !isSVProject && !isProjectFolder)
    {
        MITK_WARN << "[XQ-Open] Directory not recognized as a project";
        QMessageBox::warning(nullptr, "XQ",
            "The selected directory does not appear to be a project folder.\n"
            "Expected: .xqproj file, .svproj file, or standard project subdirectories\n"
            "(Images, Paths, Models, Meshes, etc.).");
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    mitk::StatusBar::GetInstance()->DisplayText("Opening project...");

    bool success = false;
    QString projectName;

    if (isXQProject)
    {
        // Native .xqproj project
        std::string projFilePath = (projectDir + "/" + xqProjFiles.first()).toStdString();
        MITK_INFO << "[XQ-Open] Loading native project: " << projFilePath;
        xq_WorkspaceManager projectMgr;
        success = projectMgr.OpenProject(dataStorage, projFilePath);
        if (success)
            projectName = QString::fromStdString(projectMgr.GetProjectName());
    }
    else
    {
        MITK_INFO << "[XQ-Open] Loading compatible project: " << projectDir.toStdString();
        xq_LegacyImporter importer;
        success = importer.ImportProject(dataStorage, projectDir.toStdString());
        MITK_INFO << "[XQ-Open] Import result: " << (success ? "SUCCESS" : "FAILED");
        if (success)
        {
            projectName = QString::fromStdString(importer.GetProjectName());

            auto log = importer.GetImportLog();
            for (const auto& msg : log)
            {
                MITK_INFO << msg;
            }
        }
    }

    if (success)
    {
        MITK_INFO << "[XQ-Open] Initializing views...";
        mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(dataStorage);
    }

    QApplication::restoreOverrideCursor();

    if (!success)
    {
        mitk::StatusBar::GetInstance()->DisplayText("Failed to open project.");
        QMessageBox::critical(nullptr, "XQ",
                              "Failed to open project from:\n" + projectDir);
        return;
    }

    // Update window title
    if (m_Window && !projectName.isEmpty())
    {
        auto shell = m_Window->GetShell();
        if (shell.IsNotNull())
            shell->SetText(QString("XQ - %1").arg(projectName));
    }

    MITK_INFO << "[XQ-Open] Project opened: " << projectName.toStdString();
    mitk::StatusBar::GetInstance()->DisplayText(
        QString("Project \"%1\" opened successfully.").arg(projectName).toStdString().c_str());
}
