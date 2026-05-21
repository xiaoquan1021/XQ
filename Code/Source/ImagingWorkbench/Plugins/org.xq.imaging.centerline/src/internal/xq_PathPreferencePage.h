#ifndef XQ_PATHPREFERENCEPAGE_H
#define XQ_PATHPREFERENCEPAGE_H

#include <berryIQtPreferencePage.h>
#include <mitkIPreferences.h>
#include <XQ_QT_PATHPLANNINGExports.h>

class QSpinBox;
class QPushButton;
class QWidget;
class QColor;

class XQ_QT_PATHPLANNING_EXPORT xq_PathPreferencePage
  : public QObject
  , public berry::IQtPreferencePage
{
  Q_OBJECT
  Q_INTERFACES(berry::IPreferencePage)

public:
  xq_PathPreferencePage();
  ~xq_PathPreferencePage() override;

  void Init(berry::IWorkbench::Pointer workbench) override;
  void CreateQtControl(QWidget* parent) override;
  QWidget* GetQtControl() const override;
  bool PerformOk() override;
  void PerformCancel() override;
  void Update() override;

protected slots:
  void OnColorButtonClicked();

private:
  QWidget* m_MainControl;
  QSpinBox* m_Point2DSizeSpinBox;
  QSpinBox* m_Point3DSizeSpinBox;
  QPushButton* m_PathColorButton;
  QColor m_PathColor;
  mitk::IPreferences* m_Preferences;
};

#endif // XQ_PATHPREFERENCEPAGE_H
