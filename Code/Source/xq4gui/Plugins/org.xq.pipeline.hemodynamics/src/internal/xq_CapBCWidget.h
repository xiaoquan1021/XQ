#ifndef XQ_CAPBCWIDGET_H
#define XQ_CAPBCWIDGET_H

#include <QWidget>
#include <QMap>

class QComboBox;
class QStackedWidget;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;

class xq_CapBCWidget : public QWidget
{
  Q_OBJECT

public:
  explicit xq_CapBCWidget(QWidget* parent = nullptr);
  ~xq_CapBCWidget() override;

  void SetCapName(const QString& name);
  QString GetBCType() const;
  QMap<QString, double> GetBCValues() const;

private slots:
  void OnBCTypeChanged(int index);
  void BrowseWaveformFile();
  void BrowseImpedanceFile();

private:
  void BuildLayout();

  QComboBox* m_ComboType;
  QStackedWidget* m_Stack;
  QString m_CapName;

  // Page 0: Prescribed Velocities
  QDoubleSpinBox* m_SpinFlowRate;
  QLineEdit* m_EditWaveformFile;
  QPushButton* m_BtnBrowseWaveform;

  // Page 1: Resistance
  QDoubleSpinBox* m_SpinResistance;

  // Page 2: RCR
  QDoubleSpinBox* m_SpinRp;
  QDoubleSpinBox* m_SpinC;
  QDoubleSpinBox* m_SpinRd;

  // Page 3: Coronary
  QDoubleSpinBox* m_SpinRa;
  QDoubleSpinBox* m_SpinCa;
  QDoubleSpinBox* m_SpinRaMicro;
  QDoubleSpinBox* m_SpinCim;
  QDoubleSpinBox* m_SpinRv;

  // Page 4: Impedance
  QLineEdit* m_EditImpedanceFile;
  QPushButton* m_BtnBrowseImpedance;
};

#endif // XQ_CAPBCWIDGET_H
