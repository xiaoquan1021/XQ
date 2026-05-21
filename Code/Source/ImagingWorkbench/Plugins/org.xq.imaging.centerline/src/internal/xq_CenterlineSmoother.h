#ifndef XQ_PATHSMOOTH_H
#define XQ_PATHSMOOTH_H

#include <QDialog>

namespace Ui {
class xq_CenterlineSmoother;
}

class xq_CenterlineSmoother : public QDialog
{
  Q_OBJECT

public:
  explicit xq_CenterlineSmoother(QWidget* parent = nullptr);
  ~xq_CenterlineSmoother() override;

  int GetSamplingNumber() const;
  QString GetSmoothingMethod() const;
  int GetOutputNumber() const;

private:
  Ui::xq_CenterlineSmoother* m_Ui;
};

#endif // XQ_PATHSMOOTH_H
