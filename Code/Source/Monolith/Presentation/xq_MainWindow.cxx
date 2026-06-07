#include "xq_MainWindow.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataManagementService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_ProjectSessionService.h"
#include "Core/xq_ProjectService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowContextService.h"
#include "Core/xq_WorkflowRegistry.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "xq_DataHierarchyModel.h"

#include <QAction>
#include <QDockWidget>
#include <QFrame>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

#include <utility>

namespace xq::presentation
{

MainWindow::MainWindow(xq::core::ApplicationContext& context, QWidget* parent)
    : QMainWindow(parent)
    , m_Context(context)
{
    setWindowTitle(QStringLiteral("XQ"));
    resize(1440, 960);
    statusBar()->setObjectName(QStringLiteral("xqProjectStatusBar"));
    statusBar()->showMessage(QStringLiteral("No project"));

    auto* projectToolbar = new QToolBar(this);
    projectToolbar->setObjectName(QStringLiteral("xqProjectToolbar"));
    projectToolbar->setMovable(false);
    projectToolbar->setFloatable(false);

    m_SaveProjectAction = new QAction(QStringLiteral("Save"), projectToolbar);
    m_SaveProjectAction->setObjectName(
        QStringLiteral("xqSaveProjectAction"));
    m_SaveProjectAction->setEnabled(false);
    projectToolbar->addAction(m_SaveProjectAction);
    addToolBar(Qt::TopToolBarArea, projectToolbar);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    auto* workflowPanel = new QWidget(splitter);
    auto* workflowLayout = new QVBoxLayout(workflowPanel);
    workflowLayout->setContentsMargins(0, 0, 0, 0);
    workflowLayout->setSpacing(0);

    m_DataHierarchyModel =
        new DataHierarchyModel(*m_Context.DataHierarchy(), workflowPanel);

    m_DataHierarchyView = new QTreeView(workflowPanel);
    m_DataHierarchyView->setObjectName(
        QStringLiteral("xqDataHierarchyView"));
    m_DataHierarchyView->setHeaderHidden(true);
    m_DataHierarchyView->setMinimumHeight(160);
    m_DataHierarchyView->setModel(m_DataHierarchyModel);

    auto* dataToolbar = new QToolBar(workflowPanel);
    dataToolbar->setObjectName(QStringLiteral("xqDataPanelToolbar"));
    dataToolbar->setMovable(false);
    dataToolbar->setFloatable(false);

    m_RemoveDataAction =
        new QAction(QStringLiteral("Remove"), dataToolbar);
    m_RemoveDataAction->setObjectName(QStringLiteral("xqRemoveDataAction"));
    m_RemoveDataAction->setEnabled(false);
    dataToolbar->addAction(m_RemoveDataAction);

    workflowLayout->addWidget(dataToolbar, 0);
    workflowLayout->addWidget(m_DataHierarchyView, 0);

    auto* workflowSplitter = new QSplitter(Qt::Horizontal, workflowPanel);
    workflowLayout->addWidget(workflowSplitter);

    m_Navigation = new QListWidget(workflowSplitter);
    m_Navigation->setObjectName(QStringLiteral("xqWorkflowNavigation"));
    m_Navigation->setMinimumWidth(220);
    m_Navigation->setMaximumWidth(320);

    m_Pages = new QStackedWidget(workflowSplitter);
    m_Pages->setObjectName(QStringLiteral("xqWorkflowPages"));
    workflowSplitter->addWidget(m_Navigation);
    workflowSplitter->addWidget(m_Pages);
    workflowSplitter->setStretchFactor(1, 1);

    m_RenderHostContainer = new QWidget(splitter);
    m_RenderHostContainer->setObjectName(
        QStringLiteral("xqRenderHostContainer"));
    auto* renderHostLayout = new QVBoxLayout(m_RenderHostContainer);
    renderHostLayout->setContentsMargins(0, 0, 0, 0);

    m_RenderHost = new QFrame(m_RenderHostContainer);
    m_RenderHost->setObjectName(QStringLiteral("xqRenderPlaceholder"));
    renderHostLayout->addWidget(m_RenderHost);

    splitter->addWidget(workflowPanel);
    splitter->addWidget(m_RenderHostContainer);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({420, 1020});
    setCentralWidget(splitter);

    for (const auto& workflow : xq::core::DefaultWorkflowRegistry())
        AddWorkflowPage(workflow.Id, workflow.Title);

    connect(m_Navigation, &QListWidget::currentRowChanged,
            this,
            [this](int row) {
                if (row < 0)
                    return;

                m_Pages->setCurrentIndex(row);
                auto* item = m_Navigation->item(row);
                if (!item)
                    return;

                m_Context.WorkflowSelection()->SelectWorkflow(
                    item->data(Qt::UserRole).toString());
            });
    connect(m_Context.WorkflowSelection(),
            &xq::core::WorkflowSelectionService::WorkflowChanged,
            this,
            [this](const QString& workflowId) {
                SyncWorkflowNavigationFromCore(workflowId);
            });
    connect(m_Context.WorkflowContext(),
            &xq::core::WorkflowContextService::ContextChanged,
            this,
            [this]() {
                UpdateWorkflowContextStatusPage();
            });
    SyncWorkflowNavigationFromCore(
        m_Context.WorkflowSelection()->SelectedWorkflowId());
    UpdateWorkflowContextStatusPage();

    connect(m_DataHierarchyView->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            [this](const QModelIndex& current, const QModelIndex&) {
                if (m_SyncingSelectionFromCore)
                    return;

                const QString nodeId =
                    current.data(DataHierarchyModel::NodeIdRole).toString();
                const auto* node = m_Context.DataHierarchy()->FindNode(nodeId);
                if (!node ||
                    node->Kind != xq::core::DataHierarchyNodeKind::DataEntry)
                {
                    return;
                }

                QString errorMessage;
                if (!m_Context.DataSelection()->SelectHierarchyNode(
                        nodeId,
                        &errorMessage))
                {
                    m_Context.PostDiagnostic(errorMessage);
                }
            });
    connect(m_RemoveDataAction,
            &QAction::triggered,
            this,
            [this]() {
                RemoveSelectedData();
            });
    connect(m_SaveProjectAction,
            &QAction::triggered,
            this,
            [this]() {
                SaveProject();
            });

    connect(m_Context.DataSelection(),
            &xq::core::DataSelectionService::SelectionChanged,
            this,
            [this](const QString& hierarchyNodeId, const QString&) {
                SyncTreeSelectionFromCore(hierarchyNodeId);
                UpdateDataWorkflowPage();
                UpdateDataActions();
            });

    UpdateDataActions();

    connect(m_Context.DataCatalog(),
            &xq::core::DataCatalogService::EntriesChanged,
            this,
            [this]() {
                UpdateDataWorkflowPage();
                UpdateProjectPageDataCount();
            });

    connect(m_Context.Projects(),
            &xq::core::ProjectService::ProjectChanged,
            this,
            [this](const xq::core::ProjectMetadata& project) {
                UpdateProjectPage(&project);
                UpdateProjectWindowState(project);
                UpdateProjectActions();
            });
    if (const auto* project = m_Context.Projects()->CurrentProject())
    {
        UpdateDataWorkflowPage();
        UpdateProjectPage(project);
        UpdateProjectWindowState(*project);
    }
    else
    {
        UpdateDataWorkflowPage();
        UpdateProjectPage(nullptr);
    }
    UpdateProjectActions();

    m_Diagnostics = new QTextEdit(this);
    m_Diagnostics->setObjectName(QStringLiteral("xqDiagnosticsLog"));
    m_Diagnostics->setReadOnly(true);
    auto* diagnosticsDock = new QDockWidget(QStringLiteral("Diagnostics"), this);
    diagnosticsDock->setObjectName(QStringLiteral("xqDiagnosticsDock"));
    diagnosticsDock->setWidget(m_Diagnostics);
    addDockWidget(Qt::BottomDockWidgetArea, diagnosticsDock);

    connect(&m_Context, &xq::core::ApplicationContext::DiagnosticPosted,
            this, [this](const QString& message) {
                m_Diagnostics->append(message);
            });
}

void MainWindow::SetRenderHost(QWidget* renderHost)
{
    if (!renderHost || !m_RenderHostContainer)
        return;

    auto* layout = qobject_cast<QVBoxLayout*>(m_RenderHostContainer->layout());
    if (!layout)
        return;

    if (m_RenderHost)
    {
        layout->removeWidget(m_RenderHost);
        m_RenderHost->deleteLater();
    }

    m_RenderHost = renderHost;
    m_RenderHost->setParent(m_RenderHostContainer);
    layout->addWidget(m_RenderHost);
}

void MainWindow::UpdateDataWorkflowPage()
{
    if (!m_DataSelectionLabel || !m_DataCatalogIdLabel ||
        !m_DataDisplayNameLabel || !m_DataSourcePathLabel)
    {
        return;
    }

    const QString catalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    const auto* entry = m_Context.DataCatalog()->FindById(catalogEntryId);
    if (!entry)
    {
        m_DataSelectionLabel->setText(QStringLiteral("No data selected"));
        m_DataCatalogIdLabel->clear();
        m_DataDisplayNameLabel->clear();
        m_DataSourcePathLabel->clear();
        return;
    }

    m_DataSelectionLabel->setText(
        QStringLiteral("Selected data: %1").arg(entry->DisplayName));
    m_DataCatalogIdLabel->setText(QStringLiteral("ID: %1").arg(entry->Id));
    m_DataDisplayNameLabel->setText(
        QStringLiteral("Name: %1").arg(entry->DisplayName));
    m_DataSourcePathLabel->setText(
        QStringLiteral("Source: %1").arg(entry->SourcePath));
}

void MainWindow::SaveProject()
{
    if (!m_Context.Projects()->HasActiveProject())
    {
        m_Context.PostDiagnostic(QStringLiteral("No active project to save."));
        UpdateProjectActions();
        return;
    }

    QString errorMessage;
    if (!m_Context.ProjectSession()->Save(&errorMessage))
    {
        m_Context.PostDiagnostic(
            errorMessage.trimmed().isEmpty()
                ? QStringLiteral("Unable to save project.")
                : errorMessage);
        UpdateProjectActions();
        return;
    }

    m_Context.PostDiagnostic(QStringLiteral("Project saved."));
    UpdateProjectActions();
}

void MainWindow::SyncWorkflowNavigationFromCore(const QString& workflowId)
{
    if (!m_Navigation || !m_Pages)
        return;

    for (int row = 0; row < m_Navigation->count(); ++row)
    {
        auto* item = m_Navigation->item(row);
        if (!item)
            continue;

        if (item->data(Qt::UserRole).toString() != workflowId)
            continue;

        m_Navigation->setCurrentRow(row);
        m_Pages->setCurrentIndex(row);
        return;
    }
}

void MainWindow::UpdateWorkflowContextStatusPage()
{
    const xq::core::WorkflowContextSnapshot snapshot =
        m_Context.WorkflowContext()->Snapshot();
    auto* label = m_WorkflowContextStatusLabels.value(snapshot.WorkflowId,
                                                      nullptr);
    for (auto* actionButton : std::as_const(m_WorkflowPrimaryActionButtons))
    {
        if (actionButton)
            actionButton->setEnabled(false);
    }

    auto* button = m_WorkflowPrimaryActionButtons.value(snapshot.WorkflowId,
                                                       nullptr);
    if (button)
        button->setEnabled(snapshot.HasCompatibleSelection);

    if (!label)
        return;

    if (!snapshot.HasSelectedData)
    {
        label->setText(QStringLiteral("Select compatible data to continue."));
        return;
    }

    if (!snapshot.HasCompatibleSelection)
    {
        label->setText(
            QStringLiteral("Selected data is not compatible with %1.")
                .arg(snapshot.WorkflowTitle));
        return;
    }

    const QString displayName =
        snapshot.SelectedDataDisplayName.trimmed().isEmpty()
            ? snapshot.SelectedCatalogEntryId
            : snapshot.SelectedDataDisplayName;
    label->setText(QStringLiteral("Using %1.").arg(displayName));
}

void MainWindow::RunActiveWorkflowAction()
{
    QString message;
    m_Context.WorkflowActions()->RequestActiveWorkflowAction(&message);
    m_Context.PostDiagnostic(message);
}

void MainWindow::UpdateProjectPage(const xq::core::ProjectMetadata* project)
{
    if (!m_ProjectNameLabel || !m_ProjectPathLabel || !m_ProjectSchemaLabel)
        return;

    if (!project)
    {
        m_ProjectNameLabel->setText(QStringLiteral("No project"));
        m_ProjectPathLabel->clear();
        m_ProjectSchemaLabel->setText(QStringLiteral("Schema: -"));
        UpdateProjectPageDataCount();
        return;
    }

    m_ProjectNameLabel->setText(project->Name);
    m_ProjectPathLabel->setText(project->ProjectFilePath);
    m_ProjectSchemaLabel->setText(
        QStringLiteral("Schema: %1").arg(project->SchemaVersion));
    UpdateProjectPageDataCount();
}

void MainWindow::UpdateProjectPageDataCount()
{
    if (!m_ProjectDataCountLabel)
        return;

    m_ProjectDataCountLabel->setText(
        QStringLiteral("Data items: %1")
            .arg(m_Context.DataCatalog()->Entries().size()));
}

void MainWindow::UpdateProjectWindowState(
    const xq::core::ProjectMetadata& project)
{
    setWindowTitle(QStringLiteral("XQ - %1").arg(project.Name));
    statusBar()->showMessage(
        QStringLiteral("%1 | %2").arg(project.Name, project.ProjectFilePath));
}

void MainWindow::UpdateProjectActions()
{
    if (!m_SaveProjectAction)
        return;

    m_SaveProjectAction->setEnabled(m_Context.Projects()->HasActiveProject());
}

void MainWindow::RemoveSelectedData()
{
    const QString selectedCatalogEntryId =
        m_Context.DataSelection()->SelectedCatalogEntryId();
    if (selectedCatalogEntryId.trimmed().isEmpty())
    {
        m_Context.PostDiagnostic(QStringLiteral("No data selected."));
        UpdateDataActions();
        return;
    }

    QString errorMessage;
    if (!m_Context.DataManagement()->RemoveEntry(selectedCatalogEntryId,
                                                 &errorMessage))
    {
        m_Context.PostDiagnostic(errorMessage);
        UpdateDataActions();
        return;
    }

    if (!errorMessage.trimmed().isEmpty())
        m_Context.PostDiagnostic(errorMessage);

    UpdateDataActions();
}

void MainWindow::SyncTreeSelectionFromCore(const QString& hierarchyNodeId)
{
    if (!m_DataHierarchyView || !m_DataHierarchyModel ||
        !m_DataHierarchyView->selectionModel())
    {
        return;
    }

    m_SyncingSelectionFromCore = true;
    const auto resetSyncing = qScopeGuard([this]() {
        m_SyncingSelectionFromCore = false;
    });

    QSignalBlocker selectionBlocker(m_DataHierarchyView->selectionModel());
    if (hierarchyNodeId.trimmed().isEmpty())
    {
        m_DataHierarchyView->selectionModel()->clear();
        m_DataHierarchyView->selectionModel()->clearCurrentIndex();
        return;
    }

    const QModelIndex index =
        m_DataHierarchyModel->IndexForNodeId(hierarchyNodeId);
    if (!index.isValid())
        return;

    m_DataHierarchyView->expand(index.parent());
    m_DataHierarchyView->setCurrentIndex(index);
    m_DataHierarchyView->scrollTo(index);
}

void MainWindow::UpdateDataActions()
{
    if (!m_RemoveDataAction)
        return;

    m_RemoveDataAction->setEnabled(
        !m_Context.DataSelection()->SelectedCatalogEntryId().isEmpty());
}

QWidget* MainWindow::CreateWorkflowPage(const QString& id,
                                        const QString& title)
{
    auto* page = new QFrame(this);
    page->setObjectName(QStringLiteral("xqWorkflowPage_%1").arg(id));
    page->setFrameShape(QFrame::NoFrame);

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* heading = new QLabel(title, page);
    QFont headingFont = heading->font();
    headingFont.setPointSize(14);
    headingFont.setBold(true);
    heading->setFont(headingFont);

    layout->addWidget(heading);
    if (id == QStringLiteral("project"))
    {
        m_ProjectNameLabel = new QLabel(page);
        m_ProjectNameLabel->setObjectName(QStringLiteral("xqProjectPageName"));
        m_ProjectPathLabel = new QLabel(page);
        m_ProjectPathLabel->setObjectName(QStringLiteral("xqProjectPagePath"));
        m_ProjectSchemaLabel = new QLabel(page);
        m_ProjectSchemaLabel->setObjectName(
            QStringLiteral("xqProjectPageSchema"));
        m_ProjectDataCountLabel = new QLabel(page);
        m_ProjectDataCountLabel->setObjectName(
            QStringLiteral("xqProjectPageDataCount"));

        layout->addWidget(m_ProjectNameLabel);
        layout->addWidget(m_ProjectPathLabel);
        layout->addWidget(m_ProjectSchemaLabel);
        layout->addWidget(m_ProjectDataCountLabel);
    }
    else if (id == QStringLiteral("data"))
    {
        m_DataSelectionLabel = new QLabel(page);
        m_DataSelectionLabel->setObjectName(
            QStringLiteral("xqDataPageSelection"));
        m_DataCatalogIdLabel = new QLabel(page);
        m_DataCatalogIdLabel->setObjectName(
            QStringLiteral("xqDataPageCatalogId"));
        m_DataDisplayNameLabel = new QLabel(page);
        m_DataDisplayNameLabel->setObjectName(
            QStringLiteral("xqDataPageDisplayName"));
        m_DataSourcePathLabel = new QLabel(page);
        m_DataSourcePathLabel->setObjectName(
            QStringLiteral("xqDataPageSourcePath"));

        layout->addWidget(m_DataSelectionLabel);
        layout->addWidget(m_DataCatalogIdLabel);
        layout->addWidget(m_DataDisplayNameLabel);
        layout->addWidget(m_DataSourcePathLabel);
    }
    else if (!xq::core::WorkflowContextService::AcceptedDataRolesForWorkflow(
                  id).isEmpty())
    {
        auto* statusLabel = new QLabel(page);
        statusLabel->setObjectName(
            QStringLiteral("xqWorkflowContextStatus_%1").arg(id));
        statusLabel->setWordWrap(true);
        m_WorkflowContextStatusLabels.insert(id, statusLabel);
        layout->addWidget(statusLabel);

        auto* actionButton = new QPushButton(QStringLiteral("Run"), page);
        actionButton->setObjectName(
            QStringLiteral("xqWorkflowPrimaryAction_%1").arg(id));
        actionButton->setEnabled(false);
        connect(actionButton,
                &QPushButton::clicked,
                this,
                [this]() {
                    RunActiveWorkflowAction();
                });
        m_WorkflowPrimaryActionButtons.insert(id, actionButton);
        layout->addWidget(actionButton);
    }
    layout->addStretch(1);

    return page;
}

void MainWindow::AddWorkflowPage(const QString& id, const QString& title)
{
    auto* item = new QListWidgetItem(title, m_Navigation);
    item->setData(Qt::UserRole, id);
    m_Pages->addWidget(CreateWorkflowPage(id, title));
}

} // namespace xq::presentation
