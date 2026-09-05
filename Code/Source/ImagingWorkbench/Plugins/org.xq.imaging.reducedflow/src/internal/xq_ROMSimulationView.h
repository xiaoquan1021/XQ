#ifndef XQ_ROMSIMULATIONVIEW_H
#define XQ_ROMSIMULATIONVIEW_H

#include <QmitkAbstractView.h>
#include <XQ_QT_ROMSIMULATIONExports.h>

#include <mitkDataNode.h>

class QLabel;

class XQ_QT_ROMSIMULATION_EXPORT xq_ROMSimulationView : public QmitkAbstractView
{
  Q_OBJECT

public:
  static const QString VIEW_ID;

  xq_ROMSimulationView();
  ~xq_ROMSimulationView() override;

protected:
  void CreateQtPartControl(QWidget* parent) override;
  void SetFocus() override;
  void OnSelectionChanged(berry::IWorkbenchPart::Pointer source,
                          const QList<mitk::DataNode::Pointer>& nodes) override;

private:
  void ClearROMState();
  void EnableROMControls(bool enabled);
  void BindCurrentDataManagerSelection();

  QLabel* m_StatusLabel;
  mitk::DataNode::Pointer m_CurrentROMJobNode;
};

#endif // XQ_ROMSIMULATIONVIEW_H
