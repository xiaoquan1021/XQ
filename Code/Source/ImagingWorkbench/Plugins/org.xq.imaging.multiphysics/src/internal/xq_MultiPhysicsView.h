#ifndef XQ_MULTIPHYSICSVIEW_H
#define XQ_MULTIPHYSICSVIEW_H

#include <QmitkAbstractView.h>
#include <XQ_QT_MULTIPHYSICSExports.h>

#include <mitkDataNode.h>

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
  void OnSelectionChanged(berry::IWorkbenchPart::Pointer source,
                          const QList<mitk::DataNode::Pointer>& nodes) override;

private:
  void ClearMultiPhysicsState();
  void EnableMultiPhysicsControls(bool enabled);
  void BindCurrentDataManagerSelection();

  QLabel* m_StatusLabel;
  mitk::DataNode::Pointer m_CurrentMultiPhysicsJobNode;
};

#endif // XQ_MULTIPHYSICSVIEW_H
