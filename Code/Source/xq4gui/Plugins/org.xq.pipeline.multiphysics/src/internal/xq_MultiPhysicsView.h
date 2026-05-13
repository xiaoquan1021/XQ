#ifndef XQ_MULTIPHYSICSVIEW_H
#define XQ_MULTIPHYSICSVIEW_H

#include <QmitkAbstractView.h>
#include <XQ_QT_MULTIPHYSICSExports.h>

class QLabel;

class XQ_QT_MULTIPHYSICS_EXPORT xq_MultiPhysicsView : public QmitkAbstractView
{
  Q_OBJECT

public:
  static const QString VIEW_ID;

  xq_MultiPhysicsView();
  ~xq_MultiPhysicsView() override;

protected:
  void CreateQtPartControl(QWidget* parent) override;
  void SetFocus() override;

private:
  QLabel* m_StatusLabel;
};

#endif // XQ_MULTIPHYSICSVIEW_H
