#ifndef XQ_LOFTPARAMWIDGET_H
#define XQ_LOFTPARAMWIDGET_H

#include <QWidget>

namespace Ui {
class xq_LoftParamWidget;
}

class xq_LoftParamWidget : public QWidget
{
  Q_OBJECT

public:
  explicit xq_LoftParamWidget(QWidget* parent = nullptr);
  ~xq_LoftParamWidget() override;

  int GetNumSections() const;
  int GetSplineDegree() const;
  int GetSamplePerSection() const;
  bool GetUseLinearSample() const;
  bool GetUseFFT() const;
  int GetNumOutputPoints() const;

  void SetDefaults();

private:
  Ui::xq_LoftParamWidget* m_Ui;
};

#endif // XQ_LOFTPARAMWIDGET_H
