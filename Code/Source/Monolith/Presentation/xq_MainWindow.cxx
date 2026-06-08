#include "xq_MainWindow.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportCommand.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataManagementService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_ProjectSessionService.h"
#include "Core/xq_ProjectService.h"
#include "Core/xq_WorkflowActionService.h"
#include "Core/xq_WorkflowContextService.h"
#include "Core/xq_WorkflowOperationService.h"
#include "Core/xq_WorkflowRegistry.h"
#include "Core/xq_WorkflowSelectionService.h"
#include "Core/xq_TaskRunner.h"
#include "xq_DataHierarchyModel.h"

#include <QAction>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QVariantList>
#include <QWidget>

#include <utility>

namespace xq::presentation
{

namespace
{

QString RoleDisplayName(xq::core::DataWorkflowRole role)
{
    switch (role)
    {
    case xq::core::DataWorkflowRole::DICOMSeries:
        return QStringLiteral("DICOM Series");
    case xq::core::DataWorkflowRole::Image:
        return QStringLiteral("Image");
    case xq::core::DataWorkflowRole::Path:
        return QStringLiteral("Path");
    case xq::core::DataWorkflowRole::Segmentation:
        return QStringLiteral("Segmentation");
    case xq::core::DataWorkflowRole::Model:
        return QStringLiteral("Model");
    case xq::core::DataWorkflowRole::Mesh:
        return QStringLiteral("Mesh");
    case xq::core::DataWorkflowRole::SimulationPrep:
        return QStringLiteral("Simulation Prep");
    case xq::core::DataWorkflowRole::SimulationResult:
        return QStringLiteral("Simulation Result");
    case xq::core::DataWorkflowRole::Unknown:
        break;
    }

    return QStringLiteral("Unknown");
}

bool ParseIntegerPointList(const QString& text,
                           QVariantList& points,
                           QString* message)
{
    points.clear();
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
    {
        if (message)
            *message = QStringLiteral(
                "Point list must contain at least one x,y,z triplet.");
        return false;
    }

    const QStringList pointTexts =
        trimmed.split(QStringLiteral(";"), Qt::SkipEmptyParts);
    for (const auto& pointText : pointTexts)
    {
        const QStringList coordinates =
            pointText.trimmed().split(QStringLiteral(","));
        if (coordinates.size() != 3)
        {
            if (message)
                *message = QStringLiteral(
                    "Point list entries must use x,y,z format.");
            return false;
        }

        QVariantList point;
        for (const auto& coordinateText : coordinates)
        {
            bool ok = false;
            const int coordinate = coordinateText.trimmed().toInt(&ok);
            if (!ok)
            {
                if (message)
                    *message = QStringLiteral(
                        "Point list coordinates must be integers.");
                return false;
            }
            point.push_back(coordinate);
        }
        points.push_back(point);
    }

    if (points.isEmpty())
    {
        if (message)
            *message = QStringLiteral(
                "Point list must contain at least one x,y,z triplet.");
        return false;
    }

    if (message)
        message->clear();
    return true;
}

QString FormatIntegerPointList(const QVariant& value)
{
    QStringList pointTexts;
    const QVariantList points = value.toList();
    for (const auto& pointValue : points)
    {
        const QVariantList point = pointValue.toList();
        if (point.size() != 3)
            continue;

        pointTexts.push_back(QStringLiteral("%1,%2,%3")
                                 .arg(point.at(0).toInt())
                                 .arg(point.at(1).toInt())
                                 .arg(point.at(2).toInt()));
    }

    return pointTexts.join(QStringLiteral("; "));
}

} // namespace

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

    m_ImportDataAction =
        new QAction(QStringLiteral("Import"), dataToolbar);
    m_ImportDataAction->setObjectName(QStringLiteral("xqImportDataAction"));
    dataToolbar->addAction(m_ImportDataAction);

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
    connect(m_Context.WorkflowOperations(),
            &xq::core::WorkflowOperationService::SelectedOperationChanged,
            this,
            [this](const QString&, const QString&) {
                UpdateWorkflowOperationControls();
            });
    connect(m_Context.WorkflowOperations(),
            &xq::core::WorkflowOperationService::ParameterValueChanged,
            this,
            [this](const QString& workflowId,
                   const QString&,
                   const QString& parameterId,
                   const QVariant& value) {
                UpdateWorkflowParameterEditorValue(workflowId,
                                                   parameterId,
                                                   value);
            });
    SyncWorkflowNavigationFromCore(
        m_Context.WorkflowSelection()->SelectedWorkflowId());
    UpdateWorkflowContextStatusPage();
    UpdateWorkflowOperationControls();

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
    connect(m_ImportDataAction,
            &QAction::triggered,
            this,
            [this]() {
                ImportData();
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
                UpdateDataActions();
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

    m_TaskHistoryTable = new QTableWidget(this);
    m_TaskHistoryTable->setObjectName(QStringLiteral("xqTaskHistoryTable"));
    m_TaskHistoryTable->setColumnCount(3);
    m_TaskHistoryTable->setHorizontalHeaderLabels(
        {QStringLiteral("Task"),
         QStringLiteral("Status"),
         QStringLiteral("Message")});
    m_TaskHistoryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_TaskHistoryTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_TaskHistoryTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_TaskHistoryTable->horizontalHeader()->setStretchLastSection(true);

    auto* taskHistoryDock =
        new QDockWidget(QStringLiteral("Task History"), this);
    taskHistoryDock->setObjectName(QStringLiteral("xqTaskHistoryDock"));
    taskHistoryDock->setWidget(m_TaskHistoryTable);
    addDockWidget(Qt::BottomDockWidgetArea, taskHistoryDock);

    for (const auto& task : m_Context.Tasks()->History())
        AppendTaskHistoryRow(task);
    connect(m_Context.Tasks(),
            &xq::core::TaskRunner::TaskFinished,
            this,
            [this](const QString& taskName,
                   bool succeeded,
                   const QString& message) {
                xq::core::TaskRecord task;
                task.Name = taskName;
                task.Succeeded = succeeded;
                task.Message = message;
                AppendTaskHistoryRow(task);
            });
}

void MainWindow::SetDataImportCommand(xq::core::DataImportCommand* command)
{
    m_DataImportCommand = command;
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

void MainWindow::AppendTaskHistoryRow(const xq::core::TaskRecord& task)
{
    if (!m_TaskHistoryTable)
        return;

    const int row = m_TaskHistoryTable->rowCount();
    m_TaskHistoryTable->insertRow(row);
    m_TaskHistoryTable->setItem(row, 0, new QTableWidgetItem(task.Name));
    m_TaskHistoryTable->setItem(
        row,
        1,
        new QTableWidgetItem(task.Succeeded ? QStringLiteral("Succeeded")
                                            : QStringLiteral("Failed")));
    m_TaskHistoryTable->setItem(row, 2, new QTableWidgetItem(task.Message));
    m_TaskHistoryTable->scrollToBottom();
}

void MainWindow::UpdateDataWorkflowPage()
{
    if (!m_DataSelectionLabel || !m_DataCatalogIdLabel ||
        !m_DataDisplayNameLabel || !m_DataSourcePathLabel ||
        !m_DataWorkflowRoleLabel)
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
        m_DataWorkflowRoleLabel->clear();
        return;
    }

    m_DataSelectionLabel->setText(
        QStringLiteral("Selected data: %1").arg(entry->DisplayName));
    m_DataCatalogIdLabel->setText(QStringLiteral("ID: %1").arg(entry->Id));
    m_DataDisplayNameLabel->setText(
        QStringLiteral("Name: %1").arg(entry->DisplayName));
    m_DataSourcePathLabel->setText(
        QStringLiteral("Source: %1").arg(entry->SourcePath));
    m_DataWorkflowRoleLabel->setText(
        QStringLiteral("Role: %1").arg(RoleDisplayName(entry->WorkflowRole)));
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

    if (!snapshot.RequiresSelectedData)
    {
        label->setText(QStringLiteral("Ready."));
        return;
    }

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

void MainWindow::UpdateWorkflowOperationControls()
{
    for (auto it = m_WorkflowPrimaryActionButtons.begin();
         it != m_WorkflowPrimaryActionButtons.end();
         ++it)
    {
        const QString workflowId = it.key();
        auto* button = it.value();
        if (!button)
            continue;

        const auto operations =
            m_Context.WorkflowOperations()->OperationsForWorkflow(workflowId);
        if (operations.isEmpty())
        {
            button->setText(QStringLiteral("Run"));
            continue;
        }

        QString selectedOperationId =
            m_Context.WorkflowOperations()->SelectedOperationId(workflowId);
        QString operationTitle;
        for (const auto& operation : operations)
        {
            if (operation.Id == selectedOperationId)
            {
                operationTitle = operation.Title;
                break;
            }
        }
        if (operationTitle.trimmed().isEmpty())
            operationTitle = operations.front().Title;

        button->setText(QStringLiteral("Run %1").arg(operationTitle));
        RebuildWorkflowParameterPanel(workflowId);
    }

    for (auto it = m_WorkflowOperationSelectors.begin();
         it != m_WorkflowOperationSelectors.end();
         ++it)
    {
        auto* selector = it.value();
        if (!selector)
            continue;

        const QString selectedOperationId =
            m_Context.WorkflowOperations()->SelectedOperationId(it.key());
        const int index = selector->findData(selectedOperationId);
        if (index >= 0 && selector->currentIndex() != index)
        {
            QSignalBlocker blocker(selector);
            selector->setCurrentIndex(index);
        }
    }
}

void MainWindow::RebuildWorkflowParameterPanel(const QString& workflowId)
{
    auto* panel = m_WorkflowParameterPanels.value(workflowId, nullptr);
    if (!panel)
        return;

    auto* formLayout = qobject_cast<QFormLayout*>(panel->layout());
    if (!formLayout)
        return;

    while (formLayout->rowCount() > 0)
        formLayout->removeRow(0);

    const auto operations =
        m_Context.WorkflowOperations()->OperationsForWorkflow(workflowId);
    const QString selectedOperationId =
        m_Context.WorkflowOperations()->SelectedOperationId(workflowId);
    const xq::core::WorkflowOperationDescriptor* selectedOperation = nullptr;
    for (const auto& operation : operations)
    {
        if (operation.Id == selectedOperationId)
        {
            selectedOperation = &operation;
            break;
        }
    }
    if (!selectedOperation && !operations.isEmpty())
        selectedOperation = &operations.front();
    if (!selectedOperation)
        return;

    for (const auto& parameter : selectedOperation->Parameters)
    {
        QWidget* editor = nullptr;
        const QVariantMap values =
            m_Context.WorkflowOperations()->ParameterValues(
                workflowId,
                selectedOperation->Id);
        const QVariant value = values.value(parameter.Id);
        switch (parameter.Type)
        {
        case xq::core::WorkflowOperationParameterValueType::NumericScalar:
        {
            auto* spinBox = new QDoubleSpinBox(panel);
            if (workflowId == QStringLiteral("image-preprocessing"))
            {
                spinBox->setObjectName(
                    QStringLiteral("xqImagePreprocessingParameter_%1")
                        .arg(parameter.Id));
            }
            else
            {
                spinBox->setObjectName(
                    QStringLiteral("xqWorkflowParameter_%1")
                        .arg(parameter.Id));
            }
            spinBox->setDecimals(3);
            spinBox->setRange(-1000000.0, 1000000.0);
            spinBox->setValue(value.toDouble());
            connect(spinBox,
                    qOverload<double>(&QDoubleSpinBox::valueChanged),
                    this,
                    [this, workflowId, operationId = selectedOperation->Id,
                     parameterId = parameter.Id](double newValue) {
                        QString message;
                        if (!m_Context.WorkflowOperations()->SetParameterValue(
                                workflowId,
                                operationId,
                                parameterId,
                                newValue,
                                &message))
                        {
                            m_Context.PostDiagnostic(message);
                        }
                    });
            editor = spinBox;
            break;
        }
        case xq::core::WorkflowOperationParameterValueType::IntegerScalar:
        {
            auto* spinBox = new QSpinBox(panel);
            if (workflowId == QStringLiteral("image-preprocessing"))
            {
                spinBox->setObjectName(
                    QStringLiteral("xqImagePreprocessingParameter_%1")
                        .arg(parameter.Id));
            }
            else
            {
                spinBox->setObjectName(
                    QStringLiteral("xqWorkflowParameter_%1")
                        .arg(parameter.Id));
            }
            spinBox->setRange(-1000000, 1000000);
            spinBox->setValue(value.toInt());
            connect(spinBox,
                    qOverload<int>(&QSpinBox::valueChanged),
                    this,
                    [this, workflowId, operationId = selectedOperation->Id,
                     parameterId = parameter.Id](int newValue) {
                        QString message;
                        if (!m_Context.WorkflowOperations()->SetParameterValue(
                                workflowId,
                                operationId,
                                parameterId,
                                newValue,
                                &message))
                        {
                            m_Context.PostDiagnostic(message);
                        }
                    });
            editor = spinBox;
            break;
        }
        case xq::core::WorkflowOperationParameterValueType::IntegerPointList:
        {
            auto* lineEdit = new QLineEdit(panel);
            if (workflowId == QStringLiteral("image-preprocessing"))
            {
                lineEdit->setObjectName(
                    QStringLiteral("xqImagePreprocessingParameter_%1")
                        .arg(parameter.Id));
            }
            else
            {
                lineEdit->setObjectName(
                    QStringLiteral("xqWorkflowParameter_%1")
                        .arg(parameter.Id));
            }
            lineEdit->setText(FormatIntegerPointList(value));
            connect(lineEdit,
                    &QLineEdit::editingFinished,
                    this,
                    [this, lineEdit, workflowId,
                     operationId = selectedOperation->Id,
                     parameterId = parameter.Id]() {
                        StoreWorkflowPointListParameter(workflowId,
                                                        operationId,
                                                        parameterId,
                                                        lineEdit->text());
                    });
            editor = lineEdit;
            break;
        }
        case xq::core::WorkflowOperationParameterValueType::Option:
        {
            auto* comboBox = new QComboBox(panel);
            if (workflowId == QStringLiteral("image-preprocessing"))
            {
                comboBox->setObjectName(
                    QStringLiteral("xqImagePreprocessingParameter_%1")
                        .arg(parameter.Id));
            }
            else
            {
                comboBox->setObjectName(
                    QStringLiteral("xqWorkflowParameter_%1")
                        .arg(parameter.Id));
            }
            for (const auto& option : parameter.Options)
                comboBox->addItem(option.Title, option.Id);
            const int index = comboBox->findData(value.toString());
            if (index >= 0)
                comboBox->setCurrentIndex(index);
            connect(comboBox,
                    &QComboBox::currentIndexChanged,
                    this,
                    [this, comboBox, workflowId,
                     operationId = selectedOperation->Id,
                     parameterId = parameter.Id](int) {
                        QString message;
                        if (!m_Context.WorkflowOperations()->SetParameterValue(
                                workflowId,
                                operationId,
                                parameterId,
                                comboBox->currentData().toString(),
                                &message))
                        {
                            m_Context.PostDiagnostic(message);
                        }
                    });
            editor = comboBox;
            break;
        }
        }

        formLayout->addRow(parameter.Title, editor);
    }
}

void MainWindow::UpdateWorkflowParameterEditorValue(const QString& workflowId,
                                                    const QString& parameterId,
                                                    const QVariant& value)
{
    auto* panel = m_WorkflowParameterPanels.value(workflowId, nullptr);
    if (!panel)
        return;

    QString objectName =
        QStringLiteral("xqWorkflowParameter_%1").arg(parameterId);
    if (workflowId == QStringLiteral("image-preprocessing"))
    {
        objectName = QStringLiteral("xqImagePreprocessingParameter_%1")
                         .arg(parameterId);
    }

    if (auto* doubleEditor = panel->findChild<QDoubleSpinBox*>(objectName))
    {
        QSignalBlocker blocker(doubleEditor);
        doubleEditor->setValue(value.toDouble());
        return;
    }

    if (auto* integerEditor = panel->findChild<QSpinBox*>(objectName))
    {
        QSignalBlocker blocker(integerEditor);
        integerEditor->setValue(value.toInt());
        return;
    }

    if (auto* optionEditor = panel->findChild<QComboBox*>(objectName))
    {
        const int index = optionEditor->findData(value.toString());
        if (index >= 0)
        {
            QSignalBlocker blocker(optionEditor);
            optionEditor->setCurrentIndex(index);
        }
        return;
    }

    if (auto* pointListEditor = panel->findChild<QLineEdit*>(objectName))
    {
        QSignalBlocker blocker(pointListEditor);
        pointListEditor->setText(FormatIntegerPointList(value));
    }
}

void MainWindow::StoreWorkflowPointListParameter(const QString& workflowId,
                                                 const QString& operationId,
                                                 const QString& parameterId,
                                                 const QString& text)
{
    QVariantList points;
    QString parseMessage;
    if (!ParseIntegerPointList(text, points, &parseMessage))
    {
        m_Context.PostDiagnostic(parseMessage);
        return;
    }

    QString message;
    if (!m_Context.WorkflowOperations()->SetParameterValue(workflowId,
                                                           operationId,
                                                           parameterId,
                                                           points,
                                                           &message))
    {
        m_Context.PostDiagnostic(message);
    }
}

void MainWindow::RunActiveWorkflowAction()
{
    QString message;
    if (!m_Context.WorkflowActions()->RunActiveWorkflowAction(&message))
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

void MainWindow::ImportData()
{
    if (!m_DataImportCommand)
    {
        m_Context.PostDiagnostic(
            QStringLiteral("No data import command is configured."));
        return;
    }

    const auto result = m_DataImportCommand->RunImport(m_Context);
    if (!result.Message.trimmed().isEmpty())
        m_Context.PostDiagnostic(result.Message);

    UpdateDataWorkflowPage();
    UpdateProjectPageDataCount();
    UpdateDataActions();
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
        m_DataWorkflowRoleLabel = new QLabel(page);
        m_DataWorkflowRoleLabel->setObjectName(
            QStringLiteral("xqDataPageWorkflowRole"));

        layout->addWidget(m_DataSelectionLabel);
        layout->addWidget(m_DataCatalogIdLabel);
        layout->addWidget(m_DataDisplayNameLabel);
        layout->addWidget(m_DataSourcePathLabel);
        layout->addWidget(m_DataWorkflowRoleLabel);
    }
    else
    {
        const auto acceptedRoles =
            xq::core::WorkflowContextService::AcceptedDataRolesForWorkflow(id);
        const auto operations =
            m_Context.WorkflowOperations()->OperationsForWorkflow(id);
        if (!acceptedRoles.isEmpty() || !operations.isEmpty())
        {
            if (!operations.isEmpty())
            {
                auto* operationSelector = new QComboBox(page);
                if (id == QStringLiteral("image-preprocessing"))
                {
                    operationSelector->setObjectName(QStringLiteral(
                        "xqImagePreprocessingOperationSelector"));
                }
                else
                {
                    operationSelector->setObjectName(
                        QStringLiteral("xqWorkflowOperationSelector_%1").arg(id));
                }
                for (const auto& operation : operations)
                {
                    operationSelector->addItem(operation.Title, operation.Id);
                }

                const int selectedIndex = operationSelector->findData(
                    m_Context.WorkflowOperations()->SelectedOperationId(id));
                if (selectedIndex >= 0)
                    operationSelector->setCurrentIndex(selectedIndex);

                connect(operationSelector,
                        &QComboBox::currentIndexChanged,
                        this,
                        [this, id, operationSelector](int index) {
                            if (index < 0)
                                return;

                            QString message;
                            if (!m_Context.WorkflowOperations()->SelectOperation(
                                    id,
                                    operationSelector->itemData(index).toString(),
                                    &message))
                            {
                                m_Context.PostDiagnostic(message);
                                UpdateWorkflowOperationControls();
                            }
                        });

                m_WorkflowOperationSelectors.insert(id, operationSelector);
                layout->addWidget(operationSelector);

                auto* parameterPanel = new QWidget(page);
                if (id == QStringLiteral("image-preprocessing"))
                {
                    parameterPanel->setObjectName(QStringLiteral(
                        "xqImagePreprocessingParameterPanel"));
                }
                else
                {
                    parameterPanel->setObjectName(
                        QStringLiteral("xqWorkflowParameterPanel_%1").arg(id));
                }
                auto* parameterLayout = new QFormLayout(parameterPanel);
                parameterLayout->setContentsMargins(0, 0, 0, 0);
                parameterLayout->setSpacing(8);
                m_WorkflowParameterPanels.insert(id, parameterPanel);
                layout->addWidget(parameterPanel);
            }

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
            UpdateWorkflowOperationControls();
        }
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
