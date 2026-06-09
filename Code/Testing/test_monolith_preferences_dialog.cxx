#include "Core/xq_ApplicationContext.h"
#include "Core/xq_PreferencesService.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>

#include <iostream>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

QDialog* FindPreferencesDialog()
{
    for (auto* widget : QApplication::topLevelWidgets())
    {
        if (auto* dialog = qobject_cast<QDialog*>(widget))
        {
            if (dialog->objectName() == QStringLiteral("xqPreferencesDialog"))
                return dialog;
        }
    }

    return nullptr;
}

template <typename T>
T* FindRequired(QWidget* parent, const QString& objectName)
{
    return parent ? parent->findChild<T*>(objectName) : nullptr;
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    context->Preferences()->SetIntValue(
        QStringLiteral("path.point.2d.size"),
        8);
    context->Preferences()->SetStringValue(
        QStringLiteral("simulation.mpi.path"),
        QStringLiteral("mpiexec"));

    xq::presentation::MainWindow window(*context);
    window.show();
    app.processEvents();

    auto* preferencesAction =
        window.findChild<QAction*>(QStringLiteral("xqOpenPreferencesAction"));
    if (Expect(preferencesAction != nullptr,
               "Preferences action should exist"))
    {
        delete context;
        return 1;
    }

    preferencesAction->trigger();
    app.processEvents();

    auto* dialog = FindPreferencesDialog();
    if (Expect(dialog != nullptr && dialog->isVisible(),
               "Preferences action should open a visible monolith dialog"))
    {
        delete context;
        return 1;
    }
    if (Expect(dialog->windowTitle() == QStringLiteral("XQ Preferences"),
               "Preferences dialog should have the XQ title"))
    {
        delete context;
        return 1;
    }

    auto* tabs =
        FindRequired<QTabWidget>(dialog, QStringLiteral("xqPreferencesTabs"));
    if (Expect(tabs != nullptr && tabs->count() >= 5,
               "Preferences dialog should expose Workbench preference pages"))
    {
        delete context;
        return 1;
    }
    if (Expect(tabs->tabText(0) == QStringLiteral("Path") &&
                   tabs->tabText(1) == QStringLiteral("Segmentation") &&
                   tabs->tabText(2) == QStringLiteral("Modeling") &&
                   tabs->tabText(3) == QStringLiteral("Meshing") &&
                   tabs->tabText(4) == QStringLiteral("Simulation"),
               "Preferences tabs should restore the original XQ categories"))
    {
        delete context;
        return 1;
    }

    auto* pathPoint2D =
        FindRequired<QSpinBox>(dialog,
                               QStringLiteral("xqPreferencesPathPoint2DSize"));
    auto* pathPoint3D =
        FindRequired<QSpinBox>(dialog,
                               QStringLiteral("xqPreferencesPathPoint3DSize"));
    auto* pathColor =
        FindRequired<QLineEdit>(dialog,
                                QStringLiteral("xqPreferencesPathColor"));
    auto* simulationSolver =
        FindRequired<QLineEdit>(
            dialog,
            QStringLiteral("xqPreferencesSimulationSolverPath"));
    auto* simulationMpi =
        FindRequired<QLineEdit>(
            dialog,
            QStringLiteral("xqPreferencesSimulationMpiPath"));
    auto* simulationProcs =
        FindRequired<QSpinBox>(
            dialog,
            QStringLiteral("xqPreferencesSimulationNumProcessors"));
    auto* simulationUseCustomMpi =
        FindRequired<QCheckBox>(
            dialog,
            QStringLiteral("xqPreferencesSimulationUseCustomMpi"));
    auto* buttonBox =
        FindRequired<QDialogButtonBox>(
            dialog,
            QStringLiteral("xqPreferencesButtonBox"));

    if (Expect(pathPoint2D != nullptr && pathPoint3D != nullptr &&
                   pathColor != nullptr && simulationSolver != nullptr &&
                   simulationMpi != nullptr && simulationProcs != nullptr &&
                   simulationUseCustomMpi != nullptr && buttonBox != nullptr,
               "Preferences dialog should expose editable controls"))
    {
        delete context;
        return 1;
    }
    if (Expect(pathPoint2D->value() == 8 &&
                   simulationMpi->text() == QStringLiteral("mpiexec"),
               "Preferences dialog should load existing preference values"))
    {
        delete context;
        return 1;
    }

    pathPoint2D->setValue(12);
    pathPoint3D->setValue(18);
    pathColor->setText(QStringLiteral("#00ff88"));
    simulationSolver->setText(QStringLiteral("C:/xq/bin/solver.exe"));
    simulationMpi->setText(QStringLiteral("C:/mpi/mpiexec.exe"));
    simulationProcs->setValue(16);
    simulationUseCustomMpi->setChecked(true);

    auto* applyButton = buttonBox->button(QDialogButtonBox::Apply);
    if (Expect(applyButton != nullptr,
               "Preferences dialog should expose an Apply button"))
    {
        delete context;
        return 1;
    }
    applyButton->click();
    app.processEvents();

    if (Expect(context->Preferences()->IntValue(
                   QStringLiteral("path.point.2d.size")) == 12 &&
                   context->Preferences()->IntValue(
                       QStringLiteral("path.point.3d.size")) == 18 &&
                   context->Preferences()->StringValue(
                       QStringLiteral("path.color")) ==
                       QStringLiteral("#00ff88") &&
                   context->Preferences()->StringValue(
                       QStringLiteral("simulation.solver.path")) ==
                       QStringLiteral("C:/xq/bin/solver.exe") &&
                   context->Preferences()->StringValue(
                       QStringLiteral("simulation.mpi.path")) ==
                       QStringLiteral("C:/mpi/mpiexec.exe") &&
                   context->Preferences()->IntValue(
                       QStringLiteral("simulation.num.processors")) == 16 &&
                   context->Preferences()->BoolValue(
                       QStringLiteral("simulation.use.custom.mpi")),
               "Apply should store edited preferences in PreferencesService"))
    {
        delete context;
        return 1;
    }
    if (Expect(dialog->isVisible(),
               "Apply should keep Preferences dialog open"))
    {
        delete context;
        return 1;
    }

    pathPoint2D->setValue(6);
    auto* okButton = buttonBox->button(QDialogButtonBox::Ok);
    if (Expect(okButton != nullptr,
               "Preferences dialog should expose an OK button"))
    {
        delete context;
        return 1;
    }
    okButton->click();
    app.processEvents();

    if (Expect(context->Preferences()->IntValue(
                   QStringLiteral("path.point.2d.size")) == 6,
               "OK should store preferences before closing"))
    {
        delete context;
        return 1;
    }
    if (Expect(FindPreferencesDialog() == nullptr,
               "OK should close the Preferences dialog"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
