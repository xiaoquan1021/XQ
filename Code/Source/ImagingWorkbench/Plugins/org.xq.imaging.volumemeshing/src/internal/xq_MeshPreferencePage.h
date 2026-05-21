#ifndef XQ_GRIDPREFERENCEPAGE_H
#define XQ_GRIDPREFERENCEPAGE_H

#include <mitkIPreferences.h>
#include <berryIQtPreferencePage.h>

class QWidget;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;

class xq_GridPreferencePage : public QObject, public berry::IQtPreferencePage
{
  Q_OBJECT
  Q_INTERFACES(berry::IPreferencePage)

public:
  xq_GridPreferencePage();
  ~xq_GridPreferencePage() override;

  void Init(berry::IWorkbench::Pointer workbench) override;
  void CreateQtControl(QWidget* parent) override;
  QWidget* GetQtControl() const override;

  bool PerformOk() override;
  void PerformCancel() override;
  void Update() override;

private:
  QWidget* m_Control;
  QComboBox* m_DefaultMeshType;
  QDoubleSpinBox* m_DefaultGlobalEdgeSize;
  QSpinBox* m_DefaultBLLayers;
  QDoubleSpinBox* m_DefaultBLThickness;
  QDoubleSpinBox* m_DefaultBLGrowthRate;
  QCheckBox* m_AutoRunMeshing;

  mitk::IPreferences* m_Preferences;
};

#endif // XQ_GRIDPREFERENCEPAGE_H
