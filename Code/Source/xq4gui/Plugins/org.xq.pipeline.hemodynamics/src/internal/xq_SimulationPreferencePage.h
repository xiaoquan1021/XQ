#ifndef XQ_SIMULATIONPREFERENCEPAGE_H
#define XQ_SIMULATIONPREFERENCEPAGE_H

#include <mitkIPreferences.h>
#include <berryIQtPreferencePage.h>

class QWidget;

namespace Ui {
class xq_SimulationPreferencePage;
}

class xq_SimulationPreferencePage : public QObject, public berry::IQtPreferencePage
{
  Q_OBJECT
  Q_INTERFACES(berry::IPreferencePage)

public:
  xq_SimulationPreferencePage();
  ~xq_SimulationPreferencePage() override;

  void Init(berry::IWorkbench::Pointer workbench) override;
  void CreateQtControl(QWidget* parent) override;
  QWidget* GetQtControl() const override;

  bool PerformOk() override;
  void PerformCancel() override;
  void Update() override;

private slots:
  void BrowseSolverPath();
  void BrowseMpiPath();

private:
  Ui::xq_SimulationPreferencePage* m_Ui;
  QWidget* m_MainControl;
  mitk::IPreferences* m_Prefs;
};

#endif // XQ_SIMULATIONPREFERENCEPAGE_H
