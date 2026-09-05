#include "xq_WelcomePart.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QFont>
#include <QFrame>
#include <QScrollArea>

#include <berryPlatformUI.h>
#include <berryIWorkbenchWindow.h>
#include <berryIWorkbenchPage.h>

xq_WelcomePart::xq_WelcomePart()
    : m_Control(nullptr)
{
}

xq_WelcomePart::~xq_WelcomePart()
{
}

static QPushButton* MakeToolCard(const QString& title, const QString& desc,
                                 QWidget* parent)
{
    auto* btn = new QPushButton(parent);
    btn->setText(QString("%1\n%2").arg(title, desc));
    btn->setStyleSheet(
        "QPushButton {"
        "  text-align: left;"
        "  padding: 12px 16px;"
        "  border: 1px solid #CBD5E1;"
        "  border-left: 3px solid #2563EB;"
        "  border-radius: 4px;"
        "  background: #FFFFFF;"
        "  font-size: 12px;"
        "  color: #1E293B;"
        "}"
        "QPushButton:hover {"
        "  background: #EFF6FF;"
        "  border-color: #2563EB;"
        "}"
        "QPushButton:pressed {"
        "  background: #2563EB; color: #FFFFFF;"
        "}");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setMinimumHeight(56);
    return btn;
}

void xq_WelcomePart::CreateQtPartControl(QWidget* parent)
{
    m_Control = new QWidget(parent);
    m_Control->setStyleSheet("background-color: #F8FAFC;");

    auto* scrollArea = new QScrollArea(parent);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(m_Control);

    auto* outerLayout = new QVBoxLayout(parent);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scrollArea);

    auto* mainLayout = new QVBoxLayout(m_Control);
    mainLayout->setContentsMargins(80, 48, 80, 48);
    mainLayout->setSpacing(0);

    // ========== Header Banner ==========
    auto* headerWidget = new QWidget(m_Control);
    headerWidget->setStyleSheet(
        "background: #FFFFFF;"
        "border-top: 4px solid #2563EB;"
        "border-radius: 4px;");
    headerWidget->setMinimumHeight(120);
    auto* headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(40, 28, 40, 28);
    headerLayout->setSpacing(6);

    auto* titleLabel = new QLabel("XQ", headerWidget);
    titleLabel->setStyleSheet(
        "color: #1E293B; font-size: 36px; font-weight: 700;"
        "letter-spacing: 2px; background: transparent;");
    titleLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(titleLabel);

    auto* subtitleLabel = new QLabel(
        "Cardiovascular Hemodynamics Workbench", headerWidget);
    subtitleLabel->setStyleSheet(
        "color: #64748B; font-size: 14px;"
        "font-weight: 400; background: transparent;");
    subtitleLabel->setAlignment(Qt::AlignCenter);
    headerLayout->addWidget(subtitleLabel);

    mainLayout->addWidget(headerWidget);
    mainLayout->addSpacing(32);

    // ========== Getting Started ==========
    auto* startTitle = new QLabel("Getting Started", m_Control);
    startTitle->setStyleSheet(
        "font-size: 15px; font-weight: 600; color: #1E293B;"
        "padding-bottom: 8px;");
    mainLayout->addWidget(startTitle);

    auto* startGrid = new QGridLayout();
    startGrid->setSpacing(10);

    struct StartEntry {
        const char* title;
        const char* desc;
        const char* viewId;
    };
    StartEntry startActions[] = {
        {"New Project",             "Start a new cardiovascular project",    nullptr},
        {"Open Project",            "Open an existing project folder",       nullptr},
        {"Import Legacy Project",    "Import from external format",           nullptr},
        {"Open Data File",          "Load images, surfaces, or scenes",      nullptr},
    };
    for (int i = 0; i < 4; i++) {
        auto* btn = MakeToolCard(startActions[i].title, startActions[i].desc, m_Control);
        startGrid->addWidget(btn, 0, i);
    }
    mainLayout->addLayout(startGrid);
    mainLayout->addSpacing(24);

    // ========== Workflow Pipeline ==========
    auto* pipelineTitle = new QLabel("Image-to-Simulation Pipeline", m_Control);
    pipelineTitle->setStyleSheet(
        "font-size: 15px; font-weight: 600; color: #1E293B;"
        "padding-bottom: 8px;");
    mainLayout->addWidget(pipelineTitle);

    auto* pipelineWidget = new QWidget(m_Control);
    pipelineWidget->setStyleSheet(
        "background: #FFFFFF; border: 1px solid #CBD5E1;"
        "border-radius: 4px;");
    auto* pipelineLayout = new QHBoxLayout(pipelineWidget);
    pipelineLayout->setContentsMargins(20, 16, 20, 16);
    pipelineLayout->setSpacing(0);

    struct PipelineStep {
        const char* label;
        const char* bgColor;
        const char* viewId;
    };
    PipelineStep steps[] = {
        {"Medical\nImaging",        "#F8FAFC", nullptr},
        {"Path\nPlanning",          "#F8FAFC", "org.xq.views.pathplanning"},
        {"2D\nSegmentation",        "#F8FAFC", "org.xq.views.segmentation"},
        {"Solid\nModeling",         "#F8FAFC", "org.xq.views.modeling"},
        {"Mesh\nGeneration",       "#F8FAFC", "org.xq.views.meshing"},
        {"Flow\nSimulation",        "#F8FAFC", "org.xq.views.simulation"},
    };
    for (int i = 0; i < 6; i++) {
        if (i > 0) {
            auto* arrow = new QLabel(QString::fromUtf8("\u25B8"), pipelineWidget);
            arrow->setStyleSheet(
                "font-size: 18px; color: #2563EB; background: transparent;"
                "padding: 0 4px;");
            arrow->setAlignment(Qt::AlignCenter);
            pipelineLayout->addWidget(arrow);
        }
        auto* stepBtn = new QPushButton(steps[i].label, pipelineWidget);
        stepBtn->setStyleSheet(
            QString("QPushButton {"
                    "  background: %1; border: 1px solid #CBD5E1;"
                    "  border-radius: 4px; padding: 10px 12px;"
                    "  font-size: 11px; font-weight: 600; color: #1E293B;"
                    "}"
                    "QPushButton:hover {"
                    "  border-color: #2563EB; background: #EFF6FF;"
                    "}").arg(steps[i].bgColor));
        stepBtn->setCursor(Qt::PointingHandCursor);
        stepBtn->setMinimumWidth(90);
        if (steps[i].viewId) {
            QString viewId = steps[i].viewId;
            connect(stepBtn, &QPushButton::clicked, this, [viewId]() {
                auto* workbench = berry::PlatformUI::GetWorkbench();
                if (!workbench) return;
                auto window = workbench->GetActiveWorkbenchWindow();
                if (window.IsNull()) return;
                auto page = window->GetActivePage();
                if (page.IsNull()) return;
                try { page->ShowView(viewId); }
                catch (...) {}
            });
        }
        pipelineLayout->addWidget(stepBtn, 1);
    }
    mainLayout->addWidget(pipelineWidget);
    mainLayout->addSpacing(24);

    // ========== Analysis Tools ==========
    auto* toolsTitle = new QLabel("Analysis Tools", m_Control);
    toolsTitle->setStyleSheet(
        "font-size: 15px; font-weight: 600; color: #1E293B;"
        "padding-bottom: 8px;");
    mainLayout->addWidget(toolsTitle);

    auto* toolsGrid = new QGridLayout();
    toolsGrid->setSpacing(10);

    struct ToolEntry {
        const char* title;
        const char* desc;
        const char* viewId;
    };
    ToolEntry tools[] = {
        {"3D Segmentation",         "Advanced volumetric segmentation",         "org.xq.views.mitksegmentation"},
    };

    for (int i = 0; i < 1; i++) {
        auto* btn = MakeToolCard(tools[i].title, tools[i].desc, m_Control);
        if (tools[i].viewId) {
            QString viewId = tools[i].viewId;
            connect(btn, &QPushButton::clicked, this, [viewId]() {
                auto* workbench = berry::PlatformUI::GetWorkbench();
                if (!workbench) return;
                auto window = workbench->GetActiveWorkbenchWindow();
                if (window.IsNull()) return;
                auto page = window->GetActivePage();
                if (page.IsNull()) return;
                try { page->ShowView(viewId); }
                catch (...) {}
            });
        }
        toolsGrid->addWidget(btn, 0, i);
    }
    mainLayout->addLayout(toolsGrid);

    mainLayout->addStretch();
    mainLayout->addSpacing(24);

    // ========== Footer ==========
    auto* footerLine = new QFrame(m_Control);
    footerLine->setFrameShape(QFrame::HLine);
    footerLine->setStyleSheet("color: #E2E8F0;");
    mainLayout->addWidget(footerLine);
    mainLayout->addSpacing(8);

    auto* versionLabel = new QLabel(
        "XQ v1.0  \u2022  Powered by MITK \u00B7 VTK \u00B7 ITK", m_Control);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet("color: #94a3b8; font-size: 11px;");
    mainLayout->addWidget(versionLabel);
}

void xq_WelcomePart::StandbyStateChanged(bool /*standby*/)
{
}

void xq_WelcomePart::SetFocus()
{
    if (m_Control)
        m_Control->setFocus();
}
