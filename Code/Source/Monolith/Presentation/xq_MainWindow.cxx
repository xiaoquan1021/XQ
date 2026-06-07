#include "xq_MainWindow.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_WorkflowRegistry.h"
#include "xq_DataHierarchyModel.h"

#include <QDockWidget>
#include <QFrame>
#include <QItemSelectionModel>
#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

namespace xq::presentation
{

MainWindow::MainWindow(xq::core::ApplicationContext& context, QWidget* parent)
    : QMainWindow(parent)
    , m_Context(context)
{
    setWindowTitle(QStringLiteral("XQ"));
    resize(1440, 960);

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
        AddWorkflowPage(workflow.Title);

    connect(m_Navigation, &QListWidget::currentRowChanged,
            m_Pages, &QStackedWidget::setCurrentIndex);
    m_Navigation->setCurrentRow(0);

    connect(m_DataHierarchyView->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this,
            [this](const QModelIndex& current, const QModelIndex&) {
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

    m_Diagnostics = new QTextEdit(this);
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

QWidget* MainWindow::CreateWorkflowPage(const QString& title)
{
    auto* page = new QFrame(this);
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
    layout->addStretch(1);

    return page;
}

void MainWindow::AddWorkflowPage(const QString& title)
{
    m_Navigation->addItem(title);
    m_Pages->addWidget(CreateWorkflowPage(title));
}

} // namespace xq::presentation
