#include "xq_WorkspaceExplorer.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QHeaderView>
#include <QFont>
#include <QMessageBox>
#include <QApplication>

#include <mitkIDataStorageService.h>
#include <mitkIDataStorageReference.h>
#include <mitkStatusBar.h>
#include <mitkProperties.h>

#include <xq_WorkspaceManager.h>
#include <xq_LegacyImporter.h>
#include <xq_DataFolder.h>

const QString xq_WorkspaceExplorer::VIEW_ID = "org.xq.views.projectmanager";

xq_WorkspaceExplorer::xq_WorkspaceExplorer()
  : m_ProjectTree(nullptr)
  , m_ProjectNameLabel(nullptr)
  , m_ProjectPathLabel(nullptr)
  , m_OpenFolderButton(nullptr)
  , m_NewProjectButton(nullptr)
  , m_OpenProjectButton(nullptr)
  , m_RefreshButton(nullptr)
{
}

xq_WorkspaceExplorer::~xq_WorkspaceExplorer()
{
}

void xq_WorkspaceExplorer::CreateQtPartControl(QWidget* parent)
{
  QVBoxLayout* layout = new QVBoxLayout(parent);

  // --- Project Info Group ---
  QGroupBox* infoGroup = new QGroupBox("Project Information", parent);
  QVBoxLayout* infoLayout = new QVBoxLayout(infoGroup);

  m_ProjectNameLabel = new QLabel("(No project loaded)", infoGroup);
  QFont boldFont = m_ProjectNameLabel->font();
  boldFont.setBold(true);
  boldFont.setPointSize(boldFont.pointSize() + 2);
  m_ProjectNameLabel->setFont(boldFont);

  m_ProjectPathLabel = new QLabel("", infoGroup);
  QFont smallFont = m_ProjectPathLabel->font();
  smallFont.setPointSize(smallFont.pointSize() - 1);
  m_ProjectPathLabel->setFont(smallFont);

  m_OpenFolderButton = new QPushButton("Open Project Folder", infoGroup);
  m_OpenFolderButton->setEnabled(false);

  infoLayout->addWidget(m_ProjectNameLabel);
  infoLayout->addWidget(m_ProjectPathLabel);
  infoLayout->addWidget(m_OpenFolderButton);

  layout->addWidget(infoGroup);

  // --- Project Tree ---
  m_ProjectTree = new QTreeWidget(parent);
  m_ProjectTree->setHeaderLabel("Project Structure");
  m_ProjectTree->header()->setStretchLastSection(true);
  m_ProjectTree->setColumnCount(1);

  QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_ProjectTree);
  rootItem->setText(0, "(No project loaded)");
  m_ProjectTree->addTopLevelItem(rootItem);

  layout->addWidget(m_ProjectTree, 1);

  // --- Action Buttons ---
  QHBoxLayout* buttonLayout = new QHBoxLayout();

  m_NewProjectButton = new QPushButton("New Project", parent);
  m_OpenProjectButton = new QPushButton("Open Project", parent);
  m_RefreshButton = new QPushButton("Refresh", parent);
  m_RefreshButton->setEnabled(false);

  buttonLayout->addWidget(m_NewProjectButton);
  buttonLayout->addWidget(m_OpenProjectButton);
  buttonLayout->addWidget(m_RefreshButton);

  layout->addLayout(buttonLayout);

  // --- Connections ---
  connect(m_NewProjectButton, &QPushButton::clicked, this, &xq_WorkspaceExplorer::OnNewProject);
  connect(m_OpenProjectButton, &QPushButton::clicked, this, &xq_WorkspaceExplorer::OnOpenProject);
  connect(m_RefreshButton, &QPushButton::clicked, this, &xq_WorkspaceExplorer::OnRefresh);
  connect(m_OpenFolderButton, &QPushButton::clicked, this, &xq_WorkspaceExplorer::OnOpenFolder);
}

void xq_WorkspaceExplorer::SetFocus()
{
  if (m_ProjectTree)
  {
    m_ProjectTree->setFocus();
  }
}

void xq_WorkspaceExplorer::OnNewProject()
{
  QString parentDir = QFileDialog::getExistingDirectory(
    nullptr, "Select Parent Directory for New Project");
  if (parentDir.isEmpty())
    return;

  bool ok = false;
  QString projectName = QInputDialog::getText(
    nullptr, "New Project", "Project name:", QLineEdit::Normal, "", &ok);
  if (!ok || projectName.isEmpty())
    return;

  QString projectPath = parentDir + "/" + projectName;
  if (QDir(projectPath).exists())
  {
    QMessageBox::warning(nullptr, "Error",
      "Directory already exists: " + projectPath);
    return;
  }

  QApplication::setOverrideCursor(Qt::WaitCursor);

  xq_WorkspaceManager projectMgr;
  if (!projectMgr.CreateProject(parentDir.toStdString(), projectName.toStdString()))
  {
    QApplication::restoreOverrideCursor();
    QMessageBox::critical(nullptr, "Error",
      "Failed to create project directory: " + projectPath);
    return;
  }

  // Populate DataStorage with folder nodes
  auto dataStorage = GetDataStorage();
  if (dataStorage.IsNotNull())
  {
    std::string projFilePath = projectPath.toStdString() + "/"
      + projectName.toStdString() + ".xqproj";
    projectMgr.OpenProject(dataStorage, projFilePath);
  }

  QApplication::restoreOverrideCursor();

  m_CurrentProjectPath = projectPath;
  m_ProjectNameLabel->setText(projectName);
  m_ProjectPathLabel->setText(projectPath);
  m_OpenFolderButton->setEnabled(true);
  m_RefreshButton->setEnabled(true);

  mitk::StatusBar::GetInstance()->DisplayText(
    ("Project created: " + projectName.toStdString()).c_str());
  UpdateProjectTree();
}

void xq_WorkspaceExplorer::OnOpenProject()
{
  QString projectPath = QFileDialog::getExistingDirectory(
    nullptr, "Open Project Directory");
  if (projectPath.isEmpty())
    return;

  // Look for .xqproj file first (native format)
  QDir dir(projectPath);
  QStringList projFiles = dir.entryList(QStringList("*.xqproj"), QDir::Files);

  if (!projFiles.isEmpty())
  {
    // Native XQ project - open directly
    QApplication::setOverrideCursor(Qt::WaitCursor);

    auto dataStorage = GetDataStorage();
    if (dataStorage.IsNotNull())
    {
      std::string projFilePath = (projectPath + "/" + projFiles.first()).toStdString();
      xq_WorkspaceManager projectMgr;
      projectMgr.OpenProject(dataStorage, projFilePath);
    }

    QApplication::restoreOverrideCursor();

    m_CurrentProjectPath = projectPath;
    m_ProjectNameLabel->setText(dir.dirName());
    m_ProjectPathLabel->setText(projectPath);
    m_OpenFolderButton->setEnabled(true);
    m_RefreshButton->setEnabled(true);

    mitk::StatusBar::GetInstance()->DisplayText("Project opened successfully.");
    UpdateProjectTree();
    return;
  }

  bool hasLegacyProjectFile = dir.exists(".svproj");
  bool hasNamedLegacyProjectFile = dir.exists("simvascular.proj");
  // Also check for image_information.xml in Images/ subfolder
  bool hasImageInfo = QFileInfo(projectPath + "/Images/image_information.xml").exists();

  if (hasLegacyProjectFile || hasNamedLegacyProjectFile || hasImageInfo)
  {
    QMessageBox::StandardButton reply = QMessageBox::question(nullptr,
      "Compatible Project Detected",
      "This directory appears to be a compatible legacy vascular project.\n\n"
      "XQ can import this project and convert compatible data "
      "(images, surfaces, meshes) into XQ format.\n\n"
      "Import this project?",
      QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes)
      return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    mitk::StatusBar::GetInstance()->DisplayText("Importing compatible project...");

    auto dataStorage = GetDataStorage();
    if (dataStorage.IsNotNull())
    {
      xq_LegacyImporter importer;
      bool success = importer.ImportProject(dataStorage, projectPath.toStdString());

      QApplication::restoreOverrideCursor();

      if (success)
      {
        m_CurrentProjectPath = projectPath;
        m_ProjectNameLabel->setText(
            QString::fromStdString(importer.GetProjectName()) + " [Imported]");
        m_ProjectPathLabel->setText(projectPath);
        m_OpenFolderButton->setEnabled(true);
        m_RefreshButton->setEnabled(true);

        // Show import summary
        QString logText;
        int okCount = 0, warnCount = 0;
        for (const auto& msg : importer.GetImportLog())
        {
          if (msg.find("[OK]") != std::string::npos) okCount++;
          if (msg.find("[Warning]") != std::string::npos) warnCount++;
          logText += QString::fromStdString(msg) + "\n";
        }

        mitk::StatusBar::GetInstance()->DisplayText(
            ("Imported: " + std::to_string(okCount) + " items, "
             + std::to_string(warnCount) + " warnings").c_str());

        QMessageBox::information(nullptr, "Import Complete",
            QString("Successfully imported compatible project.\n\n"
                    "Items loaded: %1\nWarnings: %2\n\n"
                    "Details:\n%3")
                .arg(okCount).arg(warnCount)
                .arg(logText.left(1000)));

        UpdateProjectTree();
      }
      else
      {
        QString logText;
        for (const auto& msg : importer.GetImportLog())
          logText += QString::fromStdString(msg) + "\n";

        QMessageBox::critical(nullptr, "Import Failed",
            "Failed to import compatible project.\n\nLog:\n" + logText);
        mitk::StatusBar::GetInstance()->DisplayText("Import failed.");
      }
    }
    else
    {
      QApplication::restoreOverrideCursor();
    }
    return;
  }

  QMessageBox::StandardButton reply = QMessageBox::question(nullptr,
    "Unknown Project Format",
    "This directory does not contain a recognized project file\n"
    "(.xqproj or .svproj).\n\n"
    "Open it as a generic data directory?",
    QMessageBox::Yes | QMessageBox::No);
  if (reply != QMessageBox::Yes)
    return;

  m_CurrentProjectPath = projectPath;
  m_ProjectNameLabel->setText(dir.dirName());
  m_ProjectPathLabel->setText(projectPath);
  m_OpenFolderButton->setEnabled(true);
  m_RefreshButton->setEnabled(true);
  UpdateProjectTree();
}

void xq_WorkspaceExplorer::OnRefresh()
{
  if (!m_CurrentProjectPath.isEmpty())
  {
    UpdateProjectTree();
  }
}

void xq_WorkspaceExplorer::OnOpenFolder()
{
  if (!m_CurrentProjectPath.isEmpty())
  {
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_CurrentProjectPath));
  }
}

void xq_WorkspaceExplorer::UpdateProjectTree()
{
  m_ProjectTree->clear();

  if (m_CurrentProjectPath.isEmpty())
  {
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_ProjectTree);
    rootItem->setText(0, "(No project loaded)");
    return;
  }

  QDir projectDir(m_CurrentProjectPath);
  QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_ProjectTree);
  rootItem->setText(0, projectDir.dirName());
  QFont rootFont = rootItem->font(0);
  rootFont.setBold(true);
  rootItem->setFont(0, rootFont);

  struct FolderDisplayInfo {
      const char* dirName;
      const char* displayName;
      const char* tooltip;
  };

  static const FolderDisplayInfo folderInfo[] = {
      {"Images",          "Medical Imaging Data",        "Volume image data (CT, MRI, etc.)"},
      {"Paths",           "Path Planning",               "Anatomical centerline paths for vessel path planning"},
      {"Segmentations",   "2D Segmentations",            "2D cross-sectional segmentation groups along paths"},
      {"Models",          "Solid Models",                "3D vascular solid models (NURBS/PolyData)"},
      {"Meshes",          "Mesh Generation",             "Finite element meshes for flow simulation"},
      {"Simulations",     "Flow Simulations",            "Flow simulation configurations and results"},
      {"ROMSimulations",  "Reduced-Order Simulations",   "0D/1D lumped-parameter network simulations"},
      {"MultiPhysics",    "Coupled Physics",             "Multi-physics (FSI, mass transport) analyses"},
      {"Repository",      "Data Repository",             "Shared data and external references"},
      {"flow-files",      "Flow Waveforms",              "Inflow/outflow boundary condition data"},
  };

  // Show all subdirectories with CRIMSON-style descriptive names
  for (const auto& info : folderInfo)
  {
    QDir subdirPath(m_CurrentProjectPath + "/" + QString(info.dirName));
    QTreeWidgetItem* childItem = new QTreeWidgetItem(rootItem);

    if (subdirPath.exists())
    {
      QStringList files = subdirPath.entryList(QDir::Files | QDir::NoDotAndDotDot);
      QString label = QString("%1 [%2]").arg(info.displayName).arg(files.count());
      childItem->setText(0, label);
      childItem->setToolTip(0, info.tooltip);

      // List files under each subdirectory
      for (const QString& fileName : files)
      {
        QTreeWidgetItem* fileItem = new QTreeWidgetItem(childItem);
        fileItem->setText(0, fileName);
      }
    }
    else
    {
      childItem->setText(0, QString("%1 (—)").arg(info.displayName));
      childItem->setToolTip(0, info.tooltip);
    }
  }

  // Also show non-standard files at root level (PDF, CSV, etc.)
  QStringList rootFiles = projectDir.entryList(QDir::Files | QDir::NoDotAndDotDot);
  if (!rootFiles.isEmpty())
  {
    QTreeWidgetItem* extraItem = new QTreeWidgetItem(rootItem);
    extraItem->setText(0, QString("Project Files [%1]").arg(rootFiles.count()));
    for (const QString& f : rootFiles)
    {
      QTreeWidgetItem* fileItem = new QTreeWidgetItem(extraItem);
      fileItem->setText(0, f);
    }
  }

  m_ProjectTree->expandAll();
}
