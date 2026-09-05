#ifndef XQ_SEGMENTATIONPREFERENCEPAGE_H
#define XQ_SEGMENTATIONPREFERENCEPAGE_H

#include <berryIQtPreferencePage.h>
#include <mitkIPreferences.h>

class QComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QWidget;

class xq_SegmentationPreferencePage : public QObject, public berry::IQtPreferencePage
{
  Q_OBJECT
  Q_INTERFACES(berry::IPreferencePage)

public:
  xq_SegmentationPreferencePage();
  ~xq_SegmentationPreferencePage() override;

  void Init(berry::IWorkbench::Pointer workbench) override;
  void CreateQtControl(QWidget* parent) override;
  QWidget* GetQtControl() const override;
  bool PerformOk() override;
  void PerformCancel() override;
  void Update() override;

private slots:
  void OnContourColorClicked();
  void OnSurfaceColorClicked();

private:
  QWidget* m_MainControl;
  QComboBox* m_DefaultContourType;
  QSpinBox* m_LoftSections;
  QSpinBox* m_LoftSplineDegree;
  QSpinBox* m_LoftSamplePerSection;
  QCheckBox* m_LoftLinearSample;
  QCheckBox* m_LoftUseFFT;
  QPushButton* m_ContourColorButton;
  QPushButton* m_SurfaceColorButton;
  QColor m_ContourColor;
  QColor m_SurfaceColor;
  mitk::IPreferences* m_Preferences;
};

#endif // XQ_SEGMENTATIONPREFERENCEPAGE_H
