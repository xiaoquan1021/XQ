#ifndef XQ_PREFERENCESDIALOG_H
#define XQ_PREFERENCESDIALOG_H

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QDoubleSpinBox;
class QLineEdit;
class QSpinBox;
class QTabWidget;

namespace xq::core
{
class PreferencesService;
}

namespace xq::presentation
{

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(xq::core::PreferencesService& preferences,
                               QWidget* parent = nullptr);

private:
    void BuildUi();
    QWidget* CreatePathTab();
    QWidget* CreateSegmentationTab();
    QWidget* CreateModelingTab();
    QWidget* CreateMeshingTab();
    QWidget* CreateSimulationTab();
    void LoadFromPreferences();
    void StoreToPreferences();

    xq::core::PreferencesService& m_Preferences;
    QTabWidget* m_Tabs = nullptr;
    QSpinBox* m_PathPoint2DSize = nullptr;
    QSpinBox* m_PathPoint3DSize = nullptr;
    QLineEdit* m_PathColor = nullptr;
    QComboBox* m_SegmentationContourType = nullptr;
    QSpinBox* m_SegmentationLoftSections = nullptr;
    QSpinBox* m_SegmentationSplineDegree = nullptr;
    QSpinBox* m_SegmentationSamplesPerSection = nullptr;
    QCheckBox* m_SegmentationLinearSample = nullptr;
    QCheckBox* m_SegmentationUseFft = nullptr;
    QLineEdit* m_SegmentationContourColor = nullptr;
    QLineEdit* m_SegmentationSurfaceColor = nullptr;
    QComboBox* m_ModelingDefaultType = nullptr;
    QSpinBox* m_ModelingSamplingPoints = nullptr;
    QDoubleSpinBox* m_ModelingFilletRadius = nullptr;
    QCheckBox* m_ModelingAutoUpdate = nullptr;
    QComboBox* m_MeshingDefaultType = nullptr;
    QDoubleSpinBox* m_MeshingGlobalEdgeSize = nullptr;
    QSpinBox* m_MeshingBoundaryLayers = nullptr;
    QDoubleSpinBox* m_MeshingBoundaryLayerThickness = nullptr;
    QDoubleSpinBox* m_MeshingBoundaryLayerGrowth = nullptr;
    QCheckBox* m_MeshingAutoRun = nullptr;
    QLineEdit* m_SimulationSolverPath = nullptr;
    QLineEdit* m_SimulationMpiPath = nullptr;
    QSpinBox* m_SimulationNumProcessors = nullptr;
    QCheckBox* m_SimulationUseCustomMpi = nullptr;
    QDialogButtonBox* m_ButtonBox = nullptr;
};

} // namespace xq::presentation

#endif // XQ_PREFERENCESDIALOG_H
