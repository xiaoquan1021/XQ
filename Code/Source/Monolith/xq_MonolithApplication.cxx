#include "xq_MonolithApplication.h"

#include "Core/xq_DataImportCommand.h"
#include "Infrastructure/xq_MitkFileDataImportCommand.h"
#include "Infrastructure/xq_MitkRenderRefreshService.h"
#include "Infrastructure/xq_ImagePreprocessingWorkflowActionHandler.h"
#include "Infrastructure/xq_PathWorkflowActionHandler.h"
#include "Infrastructure/xq_SegmentationWorkflowActionHandler.h"
#include "Infrastructure/xq_ModelingWorkflowActionHandler.h"
#include "Infrastructure/xq_MeshingWorkflowActionHandler.h"
#include "Infrastructure/xq_FlowSimulationWorkflowActionHandler.h"
#include "Infrastructure/xq_RomSimulationWorkflowActionHandler.h"
#include "Infrastructure/xq_MultiPhysicsWorkflowActionHandler.h"
#include "Infrastructure/xq_PythonApiWorkflowActionHandler.h"
#include "Presentation/xq_MainWindow.h"
#include "Presentation/xq_QtFileImportPathProvider.h"
#include "Presentation/xq_QtProjectFilePathProvider.h"
#include "Presentation/xq_QtScreenshotFilePathProvider.h"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QPalette>
#include <QResource>
#include <QVBoxLayout>
#include <QWidget>

#include <QmitkRenderWindow.h>
#include <QmitkSliceNavigationWidget.h>
#include <QmitkStdMultiWidget.h>
#include <QmitkStepperAdapter.h>

#include <mitkRenderingManager.h>
#include <mitkTimeNavigationController.h>

static void InitXqApplicationResources()
{
    Q_INIT_RESOURCE(xqApplication);
}

namespace xq
{

namespace
{

QLabel* NavigatorLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("xqImageNavigator%1Label").arg(text));
    return label;
}

QmitkSliceNavigationWidget* NavigatorSlider(const QString& objectName,
                                            QWidget* parent)
{
    auto* slider = new QmitkSliceNavigationWidget(parent);
    slider->setObjectName(objectName);
    slider->ShowLabels(true);
    slider->ShowLabelUnit(true);
    return slider;
}

void AttachRenderWindowStepper(QWidget* owner,
                               QmitkSliceNavigationWidget* slider,
                               QmitkStdMultiWidget& multiWidget,
                               const QString& renderWindowName)
{
    auto* renderWindow = multiWidget.GetRenderWindow(renderWindowName);
    if (!renderWindow)
    {
        slider->setEnabled(false);
        return;
    }

    auto* adapter =
        new QmitkStepperAdapter(slider,
                                renderWindow->GetSliceNavigationController()
                                    ->GetStepper());
    adapter->setParent(owner);
}

} // namespace

ConfiguredMainWindow::~ConfiguredMainWindow() = default;

std::unique_ptr<ConfiguredMainWindow> CreateConfiguredMainWindow(
    xq::core::ApplicationContext& context,
    xq::core::FileImportPathProvider* pathProvider)
{
    auto configured = std::make_unique<ConfiguredMainWindow>();
    if (!pathProvider)
    {
        configured->OwnedPathProvider =
            std::make_unique<xq::presentation::QtFileImportPathProvider>();
        pathProvider = configured->OwnedPathProvider.get();
    }

    configured->RenderRefresh =
        std::make_unique<xq::infrastructure::MitkRenderRefreshService>();
    configured->ImportCommand =
        std::make_unique<xq::infrastructure::MitkFileDataImportCommand>(
            pathProvider,
            nullptr,
            configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicImagePreprocessingWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicPathWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicSegmentationWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicModelingWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicMeshingWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicFlowSimulationWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicRomSimulationWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicMultiPhysicsWorkflowActionHandler(
        context,
        configured->RenderRefresh.get());
    xq::infrastructure::RegisterDynamicPythonApiWorkflowActionHandler(context);
    configured->Window =
        std::make_unique<xq::presentation::MainWindow>(context);
    configured->Window->SetDataImportCommand(
        configured->ImportCommand.get());
    configured->OwnedProjectPathProvider =
        std::make_unique<xq::presentation::QtProjectFilePathProvider>(
            configured->Window.get());
    configured->Window->SetProjectFilePathProvider(
        configured->OwnedProjectPathProvider.get());
    configured->OwnedScreenshotPathProvider =
        std::make_unique<xq::presentation::QtScreenshotFilePathProvider>(
            configured->Window.get());
    configured->Window->SetScreenshotFilePathProvider(
        configured->OwnedScreenshotPathProvider.get());
    return configured;
}

QWidget* CreateMitkImageNavigator(QmitkStdMultiWidget& multiWidget,
                                  QWidget* parent)
{
    auto* navigator = new QWidget(parent);
    navigator->setObjectName(QStringLiteral("xqMitkImageNavigator"));

    auto* layout = new QGridLayout(navigator);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setHorizontalSpacing(8);
    layout->setVerticalSpacing(6);

    auto* axial = NavigatorSlider(
        QStringLiteral("xqImageNavigatorAxial"), navigator);
    auto* sagittal = NavigatorSlider(
        QStringLiteral("xqImageNavigatorSagittal"), navigator);
    auto* coronal = NavigatorSlider(
        QStringLiteral("xqImageNavigatorCoronal"), navigator);
    auto* time = NavigatorSlider(
        QStringLiteral("xqImageNavigatorTime"), navigator);

    layout->addWidget(NavigatorLabel(QStringLiteral("Axial"), navigator),
                      0,
                      0);
    layout->addWidget(axial, 0, 1);
    layout->addWidget(NavigatorLabel(QStringLiteral("Sagittal"), navigator),
                      1,
                      0);
    layout->addWidget(sagittal, 1, 1);
    layout->addWidget(NavigatorLabel(QStringLiteral("Coronal"), navigator),
                      2,
                      0);
    layout->addWidget(coronal, 2, 1);
    layout->addWidget(NavigatorLabel(QStringLiteral("Time"), navigator),
                      3,
                      0);
    layout->addWidget(time, 3, 1);
    layout->setColumnStretch(1, 1);
    layout->setRowStretch(4, 1);

    AttachRenderWindowStepper(navigator,
                              axial,
                              multiWidget,
                              QStringLiteral("axial"));
    AttachRenderWindowStepper(navigator,
                              sagittal,
                              multiWidget,
                              QStringLiteral("sagittal"));
    AttachRenderWindowStepper(navigator,
                              coronal,
                              multiWidget,
                              QStringLiteral("coronal"));

    auto* timeController =
        mitk::RenderingManager::GetInstance()->GetTimeNavigationController();
    auto* timeAdapter =
        new QmitkStepperAdapter(time, timeController->GetStepper());
    timeAdapter->setParent(navigator);

    return navigator;
}

bool ApplyXqWorkbenchTheme(QApplication& application, QString* errorMessage)
{
    InitXqApplicationResources();

    QFile styleFile(QStringLiteral(":/xq/xq.qss"));
    if (!styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("Unable to load XQ Workbench stylesheet: %1")
                    .arg(styleFile.errorString());
        }
        return false;
    }

    QPalette lightPalette;
    lightPalette.setColor(QPalette::Window, QColor(QStringLiteral("#F8FAFC")));
    lightPalette.setColor(QPalette::WindowText,
                          QColor(QStringLiteral("#1E293B")));
    lightPalette.setColor(QPalette::Base, QColor(QStringLiteral("#FFFFFF")));
    lightPalette.setColor(QPalette::AlternateBase,
                          QColor(QStringLiteral("#F1F5F9")));
    lightPalette.setColor(QPalette::ToolTipBase,
                          QColor(QStringLiteral("#FFFFFF")));
    lightPalette.setColor(QPalette::ToolTipText,
                          QColor(QStringLiteral("#1E293B")));
    lightPalette.setColor(QPalette::Text, QColor(QStringLiteral("#1E293B")));
    lightPalette.setColor(QPalette::Button, QColor(QStringLiteral("#F1F5F9")));
    lightPalette.setColor(QPalette::ButtonText,
                          QColor(QStringLiteral("#1E293B")));
    lightPalette.setColor(QPalette::BrightText,
                          QColor(QStringLiteral("#FFFFFF")));
    lightPalette.setColor(QPalette::Link, QColor(QStringLiteral("#2563EB")));
    lightPalette.setColor(QPalette::Highlight,
                          QColor(QStringLiteral("#2563EB")));
    lightPalette.setColor(QPalette::HighlightedText,
                          QColor(QStringLiteral("#FFFFFF")));
    lightPalette.setColor(QPalette::Light, QColor(QStringLiteral("#FFFFFF")));
    lightPalette.setColor(QPalette::Midlight,
                          QColor(QStringLiteral("#E2E8F0")));
    lightPalette.setColor(QPalette::Mid, QColor(QStringLiteral("#CBD5E1")));
    lightPalette.setColor(QPalette::Dark, QColor(QStringLiteral("#94A3B8")));
    lightPalette.setColor(QPalette::Shadow, QColor(QStringLiteral("#64748B")));
    application.setPalette(lightPalette);

    application.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    application.setWindowIcon(QIcon(QStringLiteral(":/xq/icon.png")));

    if (errorMessage)
        errorMessage->clear();
    return true;
}

} // namespace xq
