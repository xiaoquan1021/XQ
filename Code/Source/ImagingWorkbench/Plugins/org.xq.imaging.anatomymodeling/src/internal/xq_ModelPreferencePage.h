#ifndef XQ_MODELPREFERENCEPAGE_H
#define XQ_MODELPREFERENCEPAGE_H

#include <mitkIPreferences.h>
#include <berryIQtPreferencePage.h>

class QWidget;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;

class xq_ModelPreferencePage : public QObject, public berry::IQtPreferencePage
{
  Q_OBJECT
  Q_INTERFACES(berry::IPreferencePage)

public:
  xq_ModelPreferencePage();
  ~xq_ModelPreferencePage() override;

  void Init(berry::IWorkbench::Pointer workbench) override;
  void CreateQtControl(QWidget* parent) override;
  QWidget* GetQtControl() const override;

  bool PerformOk() override;
  void PerformCancel() override;
  void Update() override;

private:
  QWidget* m_Control;
  QComboBox* m_DefaultModelType;
  QSpinBox* m_DefaultSamplingPoints;
  QDoubleSpinBox* m_DefaultFilletRadius;
  QCheckBox* m_AutoUpdateModel;

  mitk::IPreferences* m_Preferences;
};

#endif // XQ_MODELPREFERENCEPAGE_H
