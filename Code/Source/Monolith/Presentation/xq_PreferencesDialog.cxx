#include "xq_PreferencesDialog.h"

#include "Core/xq_PreferencesService.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace xq::presentation
{

namespace
{

QLineEdit* CreateColorEdit(QWidget* parent, const QString& objectName)
{
    auto* edit = new QLineEdit(parent);
    edit->setObjectName(objectName);
    edit->setMaxLength(7);
    edit->setPlaceholderText(QStringLiteral("#rrggbb"));
    return edit;
}

QGroupBox* CreateGroup(const QString& title, QWidget* parent)
{
    auto* group = new QGroupBox(title, parent);
    group->setObjectName(QStringLiteral("xqPreferences%1Group")
                             .arg(title.simplified().remove(QLatin1Char(' '))));
    return group;
}

} // namespace

PreferencesDialog::PreferencesDialog(xq::core::PreferencesService& preferences,
                                     QWidget* parent)
    : QDialog(parent)
    , m_Preferences(preferences)
{
    setObjectName(QStringLiteral("xqPreferencesDialog"));
    setWindowTitle(QStringLiteral("XQ Preferences"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(560, 440);
    BuildUi();
    LoadFromPreferences();
}

void PreferencesDialog::BuildUi()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    m_Tabs = new QTabWidget(this);
    m_Tabs->setObjectName(QStringLiteral("xqPreferencesTabs"));
    m_Tabs->addTab(CreatePathTab(), QStringLiteral("Path"));
    m_Tabs->addTab(CreateSegmentationTab(), QStringLiteral("Segmentation"));
    m_Tabs->addTab(CreateModelingTab(), QStringLiteral("Modeling"));
    m_Tabs->addTab(CreateMeshingTab(), QStringLiteral("Meshing"));
    m_Tabs->addTab(CreateSimulationTab(), QStringLiteral("Simulation"));
    layout->addWidget(m_Tabs);

    m_ButtonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
            QDialogButtonBox::Apply,
        this);
    m_ButtonBox->setObjectName(QStringLiteral("xqPreferencesButtonBox"));
    layout->addWidget(m_ButtonBox);

    connect(m_ButtonBox->button(QDialogButtonBox::Apply),
            &QPushButton::clicked,
            this,
            [this]() {
                StoreToPreferences();
            });
    connect(m_ButtonBox,
            &QDialogButtonBox::accepted,
            this,
            [this]() {
                StoreToPreferences();
                accept();
            });
    connect(m_ButtonBox,
            &QDialogButtonBox::rejected,
            this,
            &QDialog::reject);
}

QWidget* PreferencesDialog::CreatePathTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    auto* group = CreateGroup(QStringLiteral("Path Defaults"), tab);
    auto* form = new QFormLayout(group);

    m_PathPoint2DSize = new QSpinBox(group);
    m_PathPoint2DSize->setObjectName(
        QStringLiteral("xqPreferencesPathPoint2DSize"));
    m_PathPoint2DSize->setRange(1, 20);
    form->addRow(QStringLiteral("Default 2D Point Size:"),
                 m_PathPoint2DSize);

    m_PathPoint3DSize = new QSpinBox(group);
    m_PathPoint3DSize->setObjectName(
        QStringLiteral("xqPreferencesPathPoint3DSize"));
    m_PathPoint3DSize->setRange(1, 30);
    form->addRow(QStringLiteral("Default 3D Point Size:"),
                 m_PathPoint3DSize);

    m_PathColor = CreateColorEdit(group,
                                  QStringLiteral("xqPreferencesPathColor"));
    form->addRow(QStringLiteral("Default Path Color:"), m_PathColor);

    layout->addWidget(group);
    layout->addStretch();
    return tab;
}

QWidget* PreferencesDialog::CreateSegmentationTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);

    auto* contourGroup = CreateGroup(QStringLiteral("Contour Defaults"), tab);
    auto* contourForm = new QFormLayout(contourGroup);
    m_SegmentationContourType = new QComboBox(contourGroup);
    m_SegmentationContourType->setObjectName(
        QStringLiteral("xqPreferencesSegmentationContourType"));
    m_SegmentationContourType->addItems(
        {QStringLiteral("Circle"),
         QStringLiteral("Ellipse"),
         QStringLiteral("SplinePolygon"),
         QStringLiteral("Manual")});
    contourForm->addRow(QStringLiteral("Default Contour Type:"),
                        m_SegmentationContourType);
    layout->addWidget(contourGroup);

    auto* loftGroup = CreateGroup(QStringLiteral("Lofting Defaults"), tab);
    auto* loftForm = new QFormLayout(loftGroup);
    m_SegmentationLoftSections = new QSpinBox(loftGroup);
    m_SegmentationLoftSections->setObjectName(
        QStringLiteral("xqPreferencesSegmentationLoftSections"));
    m_SegmentationLoftSections->setRange(2, 500);
    loftForm->addRow(QStringLiteral("Number of Sections:"),
                     m_SegmentationLoftSections);
    m_SegmentationSplineDegree = new QSpinBox(loftGroup);
    m_SegmentationSplineDegree->setObjectName(
        QStringLiteral("xqPreferencesSegmentationSplineDegree"));
    m_SegmentationSplineDegree->setRange(2, 5);
    loftForm->addRow(QStringLiteral("Spline Degree:"),
                     m_SegmentationSplineDegree);
    m_SegmentationSamplesPerSection = new QSpinBox(loftGroup);
    m_SegmentationSamplesPerSection->setObjectName(
        QStringLiteral("xqPreferencesSegmentationSamplesPerSection"));
    m_SegmentationSamplesPerSection->setRange(4, 500);
    loftForm->addRow(QStringLiteral("Samples Per Section:"),
                     m_SegmentationSamplesPerSection);
    m_SegmentationLinearSample = new QCheckBox(loftGroup);
    m_SegmentationLinearSample->setObjectName(
        QStringLiteral("xqPreferencesSegmentationLinearSample"));
    loftForm->addRow(QStringLiteral("Use Linear Sample:"),
                     m_SegmentationLinearSample);
    m_SegmentationUseFft = new QCheckBox(loftGroup);
    m_SegmentationUseFft->setObjectName(
        QStringLiteral("xqPreferencesSegmentationUseFft"));
    loftForm->addRow(QStringLiteral("Use FFT:"), m_SegmentationUseFft);
    layout->addWidget(loftGroup);

    auto* colorGroup = CreateGroup(QStringLiteral("Display Colors"), tab);
    auto* colorForm = new QFormLayout(colorGroup);
    m_SegmentationContourColor = CreateColorEdit(
        colorGroup,
        QStringLiteral("xqPreferencesSegmentationContourColor"));
    colorForm->addRow(QStringLiteral("Contour Color:"),
                      m_SegmentationContourColor);
    m_SegmentationSurfaceColor = CreateColorEdit(
        colorGroup,
        QStringLiteral("xqPreferencesSegmentationSurfaceColor"));
    colorForm->addRow(QStringLiteral("Surface Color:"),
                      m_SegmentationSurfaceColor);
    layout->addWidget(colorGroup);
    layout->addStretch();
    return tab;
}

QWidget* PreferencesDialog::CreateModelingTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    auto* group = CreateGroup(QStringLiteral("Modeling Defaults"), tab);
    auto* form = new QFormLayout(group);

    m_ModelingDefaultType = new QComboBox(group);
    m_ModelingDefaultType->setObjectName(
        QStringLiteral("xqPreferencesModelingDefaultType"));
    m_ModelingDefaultType->addItems(
        {QStringLiteral("PolyData"), QStringLiteral("OCCT")});
    form->addRow(QStringLiteral("Default Model Type:"), m_ModelingDefaultType);

    m_ModelingSamplingPoints = new QSpinBox(group);
    m_ModelingSamplingPoints->setObjectName(
        QStringLiteral("xqPreferencesModelingSamplingPoints"));
    m_ModelingSamplingPoints->setRange(10, 1000);
    form->addRow(QStringLiteral("Default Sampling Points:"),
                 m_ModelingSamplingPoints);

    m_ModelingFilletRadius = new QDoubleSpinBox(group);
    m_ModelingFilletRadius->setObjectName(
        QStringLiteral("xqPreferencesModelingFilletRadius"));
    m_ModelingFilletRadius->setRange(0.001, 100.0);
    m_ModelingFilletRadius->setDecimals(3);
    m_ModelingFilletRadius->setSingleStep(0.1);
    form->addRow(QStringLiteral("Default Fillet Radius:"),
                 m_ModelingFilletRadius);

    m_ModelingAutoUpdate = new QCheckBox(group);
    m_ModelingAutoUpdate->setObjectName(
        QStringLiteral("xqPreferencesModelingAutoUpdate"));
    form->addRow(QStringLiteral("Auto-Update Model:"), m_ModelingAutoUpdate);

    layout->addWidget(group);
    layout->addStretch();
    return tab;
}

QWidget* PreferencesDialog::CreateMeshingTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    auto* group = CreateGroup(QStringLiteral("Meshing Defaults"), tab);
    auto* form = new QFormLayout(group);

    m_MeshingDefaultType = new QComboBox(group);
    m_MeshingDefaultType->setObjectName(
        QStringLiteral("xqPreferencesMeshingDefaultType"));
    m_MeshingDefaultType->addItem(QStringLiteral("TetGen"));
    form->addRow(QStringLiteral("Default Mesh Type:"), m_MeshingDefaultType);

    m_MeshingGlobalEdgeSize = new QDoubleSpinBox(group);
    m_MeshingGlobalEdgeSize->setObjectName(
        QStringLiteral("xqPreferencesMeshingGlobalEdgeSize"));
    m_MeshingGlobalEdgeSize->setRange(0.001, 1000.0);
    m_MeshingGlobalEdgeSize->setDecimals(3);
    m_MeshingGlobalEdgeSize->setSingleStep(0.1);
    form->addRow(QStringLiteral("Default Global Edge Size:"),
                 m_MeshingGlobalEdgeSize);

    m_MeshingBoundaryLayers = new QSpinBox(group);
    m_MeshingBoundaryLayers->setObjectName(
        QStringLiteral("xqPreferencesMeshingBoundaryLayers"));
    m_MeshingBoundaryLayers->setRange(0, 20);
    form->addRow(QStringLiteral("Default BL Layers:"),
                 m_MeshingBoundaryLayers);

    m_MeshingBoundaryLayerThickness = new QDoubleSpinBox(group);
    m_MeshingBoundaryLayerThickness->setObjectName(
        QStringLiteral("xqPreferencesMeshingBoundaryLayerThickness"));
    m_MeshingBoundaryLayerThickness->setRange(0.001, 100.0);
    m_MeshingBoundaryLayerThickness->setDecimals(3);
    m_MeshingBoundaryLayerThickness->setSingleStep(0.05);
    form->addRow(QStringLiteral("Default BL Thickness:"),
                 m_MeshingBoundaryLayerThickness);

    m_MeshingBoundaryLayerGrowth = new QDoubleSpinBox(group);
    m_MeshingBoundaryLayerGrowth->setObjectName(
        QStringLiteral("xqPreferencesMeshingBoundaryLayerGrowth"));
    m_MeshingBoundaryLayerGrowth->setRange(1.0, 5.0);
    m_MeshingBoundaryLayerGrowth->setDecimals(2);
    m_MeshingBoundaryLayerGrowth->setSingleStep(0.1);
    form->addRow(QStringLiteral("Default BL Growth Rate:"),
                 m_MeshingBoundaryLayerGrowth);

    m_MeshingAutoRun = new QCheckBox(group);
    m_MeshingAutoRun->setObjectName(
        QStringLiteral("xqPreferencesMeshingAutoRun"));
    form->addRow(QStringLiteral("Auto-Run Meshing:"), m_MeshingAutoRun);

    layout->addWidget(group);
    layout->addStretch();
    return tab;
}

QWidget* PreferencesDialog::CreateSimulationTab()
{
    auto* tab = new QWidget(this);
    auto* layout = new QVBoxLayout(tab);
    auto* group = CreateGroup(QStringLiteral("Simulation Runtime"), tab);
    auto* form = new QFormLayout(group);

    m_SimulationSolverPath = new QLineEdit(group);
    m_SimulationSolverPath->setObjectName(
        QStringLiteral("xqPreferencesSimulationSolverPath"));
    m_SimulationSolverPath->setPlaceholderText(
        QStringLiteral("Path to solver executable"));
    form->addRow(QStringLiteral("Solver Path:"), m_SimulationSolverPath);

    m_SimulationNumProcessors = new QSpinBox(group);
    m_SimulationNumProcessors->setObjectName(
        QStringLiteral("xqPreferencesSimulationNumProcessors"));
    m_SimulationNumProcessors->setRange(1, 256);
    form->addRow(QStringLiteral("Num Processors:"),
                 m_SimulationNumProcessors);

    m_SimulationMpiPath = new QLineEdit(group);
    m_SimulationMpiPath->setObjectName(
        QStringLiteral("xqPreferencesSimulationMpiPath"));
    m_SimulationMpiPath->setPlaceholderText(
        QStringLiteral("Path to MPI executable"));
    form->addRow(QStringLiteral("MPI Path:"), m_SimulationMpiPath);

    m_SimulationUseCustomMpi = new QCheckBox(group);
    m_SimulationUseCustomMpi->setObjectName(
        QStringLiteral("xqPreferencesSimulationUseCustomMpi"));
    form->addRow(QStringLiteral("Use Custom MPI:"),
                 m_SimulationUseCustomMpi);

    auto* note = new QLabel(
        QStringLiteral("Solver execution remains disabled in Windows v1."),
        group);
    note->setObjectName(QStringLiteral("xqPreferencesSimulationScopeNote"));
    note->setWordWrap(true);
    form->addRow(QString(), note);

    layout->addWidget(group);
    layout->addStretch();
    return tab;
}

void PreferencesDialog::LoadFromPreferences()
{
    m_PathPoint2DSize->setValue(
        m_Preferences.IntValue(QStringLiteral("path.point.2d.size"), 6));
    m_PathPoint3DSize->setValue(
        m_Preferences.IntValue(QStringLiteral("path.point.3d.size"), 10));
    m_PathColor->setText(
        m_Preferences.StringValue(QStringLiteral("path.color"),
                                  QStringLiteral("#ffff00")));

    m_SegmentationContourType->setCurrentIndex(
        m_Preferences.IntValue(
            QStringLiteral("segmentation.default.contour.type"),
            0));
    m_SegmentationLoftSections->setValue(
        m_Preferences.IntValue(
            QStringLiteral("segmentation.loft.sections"),
            12));
    m_SegmentationSplineDegree->setValue(
        m_Preferences.IntValue(
            QStringLiteral("segmentation.loft.spline.degree"),
            3));
    m_SegmentationSamplesPerSection->setValue(
        m_Preferences.IntValue(
            QStringLiteral("segmentation.loft.samples.per.section"),
            60));
    m_SegmentationLinearSample->setChecked(
        m_Preferences.BoolValue(
            QStringLiteral("segmentation.loft.linear.sample"),
            false));
    m_SegmentationUseFft->setChecked(
        m_Preferences.BoolValue(
            QStringLiteral("segmentation.loft.use.fft"),
            false));
    m_SegmentationContourColor->setText(
        m_Preferences.StringValue(
            QStringLiteral("segmentation.contour.color"),
            QStringLiteral("#ffff00")));
    m_SegmentationSurfaceColor->setText(
        m_Preferences.StringValue(
            QStringLiteral("segmentation.surface.color"),
            QStringLiteral("#ff0000")));

    const QString modelType = m_Preferences.StringValue(
        QStringLiteral("modeling.default.type"),
        QStringLiteral("PolyData"));
    const int modelIndex = m_ModelingDefaultType->findText(modelType);
    m_ModelingDefaultType->setCurrentIndex(modelIndex >= 0 ? modelIndex : 0);
    m_ModelingSamplingPoints->setValue(
        m_Preferences.IntValue(
            QStringLiteral("modeling.default.sampling.points"),
            60));
    m_ModelingFilletRadius->setValue(
        m_Preferences.StringValue(
            QStringLiteral("modeling.default.fillet.radius"),
            QStringLiteral("0.5")).toDouble());
    m_ModelingAutoUpdate->setChecked(
        m_Preferences.BoolValue(
            QStringLiteral("modeling.auto.update"),
            false));

    const QString meshType = m_Preferences.StringValue(
        QStringLiteral("meshing.default.type"),
        QStringLiteral("TetGen"));
    const int meshIndex = m_MeshingDefaultType->findText(meshType);
    m_MeshingDefaultType->setCurrentIndex(meshIndex >= 0 ? meshIndex : 0);
    m_MeshingGlobalEdgeSize->setValue(
        m_Preferences.StringValue(
            QStringLiteral("meshing.default.global.edge.size"),
            QStringLiteral("1.0")).toDouble());
    m_MeshingBoundaryLayers->setValue(
        m_Preferences.IntValue(
            QStringLiteral("meshing.default.boundary.layers"),
            2));
    m_MeshingBoundaryLayerThickness->setValue(
        m_Preferences.StringValue(
            QStringLiteral("meshing.default.boundary.layer.thickness"),
            QStringLiteral("0.5")).toDouble());
    m_MeshingBoundaryLayerGrowth->setValue(
        m_Preferences.StringValue(
            QStringLiteral("meshing.default.boundary.layer.growth"),
            QStringLiteral("1.2")).toDouble());
    m_MeshingAutoRun->setChecked(
        m_Preferences.BoolValue(QStringLiteral("meshing.auto.run"), false));

    m_SimulationSolverPath->setText(
        m_Preferences.StringValue(
            QStringLiteral("simulation.solver.path")));
    m_SimulationMpiPath->setText(
        m_Preferences.StringValue(
            QStringLiteral("simulation.mpi.path"),
            QStringLiteral("mpiexec")));
    m_SimulationNumProcessors->setValue(
        m_Preferences.IntValue(
            QStringLiteral("simulation.num.processors"),
            1));
    m_SimulationUseCustomMpi->setChecked(
        m_Preferences.BoolValue(
            QStringLiteral("simulation.use.custom.mpi"),
            false));
}

void PreferencesDialog::StoreToPreferences()
{
    m_Preferences.SetIntValue(QStringLiteral("path.point.2d.size"),
                              m_PathPoint2DSize->value());
    m_Preferences.SetIntValue(QStringLiteral("path.point.3d.size"),
                              m_PathPoint3DSize->value());
    m_Preferences.SetStringValue(QStringLiteral("path.color"),
                                 m_PathColor->text().trimmed());

    m_Preferences.SetIntValue(
        QStringLiteral("segmentation.default.contour.type"),
        m_SegmentationContourType->currentIndex());
    m_Preferences.SetIntValue(QStringLiteral("segmentation.loft.sections"),
                              m_SegmentationLoftSections->value());
    m_Preferences.SetIntValue(
        QStringLiteral("segmentation.loft.spline.degree"),
        m_SegmentationSplineDegree->value());
    m_Preferences.SetIntValue(
        QStringLiteral("segmentation.loft.samples.per.section"),
        m_SegmentationSamplesPerSection->value());
    m_Preferences.SetBoolValue(
        QStringLiteral("segmentation.loft.linear.sample"),
        m_SegmentationLinearSample->isChecked());
    m_Preferences.SetBoolValue(QStringLiteral("segmentation.loft.use.fft"),
                               m_SegmentationUseFft->isChecked());
    m_Preferences.SetStringValue(
        QStringLiteral("segmentation.contour.color"),
        m_SegmentationContourColor->text().trimmed());
    m_Preferences.SetStringValue(
        QStringLiteral("segmentation.surface.color"),
        m_SegmentationSurfaceColor->text().trimmed());

    m_Preferences.SetStringValue(QStringLiteral("modeling.default.type"),
                                 m_ModelingDefaultType->currentText());
    m_Preferences.SetIntValue(
        QStringLiteral("modeling.default.sampling.points"),
        m_ModelingSamplingPoints->value());
    m_Preferences.SetStringValue(
        QStringLiteral("modeling.default.fillet.radius"),
        QString::number(m_ModelingFilletRadius->value(), 'f', 3));
    m_Preferences.SetBoolValue(QStringLiteral("modeling.auto.update"),
                               m_ModelingAutoUpdate->isChecked());

    m_Preferences.SetStringValue(QStringLiteral("meshing.default.type"),
                                 m_MeshingDefaultType->currentText());
    m_Preferences.SetStringValue(
        QStringLiteral("meshing.default.global.edge.size"),
        QString::number(m_MeshingGlobalEdgeSize->value(), 'f', 3));
    m_Preferences.SetIntValue(
        QStringLiteral("meshing.default.boundary.layers"),
        m_MeshingBoundaryLayers->value());
    m_Preferences.SetStringValue(
        QStringLiteral("meshing.default.boundary.layer.thickness"),
        QString::number(m_MeshingBoundaryLayerThickness->value(), 'f', 3));
    m_Preferences.SetStringValue(
        QStringLiteral("meshing.default.boundary.layer.growth"),
        QString::number(m_MeshingBoundaryLayerGrowth->value(), 'f', 2));
    m_Preferences.SetBoolValue(QStringLiteral("meshing.auto.run"),
                               m_MeshingAutoRun->isChecked());

    m_Preferences.SetStringValue(QStringLiteral("simulation.solver.path"),
                                 m_SimulationSolverPath->text().trimmed());
    m_Preferences.SetStringValue(QStringLiteral("simulation.mpi.path"),
                                 m_SimulationMpiPath->text().trimmed());
    m_Preferences.SetIntValue(QStringLiteral("simulation.num.processors"),
                              m_SimulationNumProcessors->value());
    m_Preferences.SetBoolValue(QStringLiteral("simulation.use.custom.mpi"),
                               m_SimulationUseCustomMpi->isChecked());
}

} // namespace xq::presentation
