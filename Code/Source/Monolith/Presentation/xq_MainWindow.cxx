#include "xq_MainWindow.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_WorkflowRegistry.h"

#include <QDockWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <QmitkStdMultiWidget.h>

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
    auto* workflowLayout = new QHBoxLayout(workflowPanel);
    workflowLayout->setContentsMargins(0, 0, 0, 0);

    auto* workflowSplitter = new QSplitter(Qt::Horizontal, workflowPanel);
    workflowLayout->addWidget(workflowSplitter);

    m_Navigation = new QListWidget(workflowSplitter);
    m_Navigation->setMinimumWidth(220);
    m_Navigation->setMaximumWidth(320);

    m_Pages = new QStackedWidget(workflowSplitter);
    workflowSplitter->addWidget(m_Navigation);
    workflowSplitter->addWidget(m_Pages);
    workflowSplitter->setStretchFactor(1, 1);

    m_RenderHost = new QmitkStdMultiWidget(splitter);
    m_RenderHost->setObjectName(QStringLiteral("xqMitkRenderHost"));
    m_RenderHost->SetDataStorage(m_Context.DataStorage().GetPointer());
    m_RenderHost->InitializeMultiWidget();
    m_RenderHost->AddPlanesToDataStorage();

    splitter->addWidget(workflowPanel);
    splitter->addWidget(m_RenderHost);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({420, 1020});
    setCentralWidget(splitter);

    for (const auto& workflow : xq::core::DefaultWorkflowRegistry())
        AddWorkflowPage(workflow.Title);

    connect(m_Navigation, &QListWidget::currentRowChanged,
            m_Pages, &QStackedWidget::setCurrentIndex);
    m_Navigation->setCurrentRow(0);

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
