#ifndef XQ_HEMODYNAMICSVIEW_H
#define XQ_HEMODYNAMICSVIEW_H

#include <QmitkAbstractView.h>
#include <XQ_QT_SIMULATIONExports.h>

#include <mitkDataNode.h>

class xq_SolverProcessHandler;

namespace Ui {
class xq_HemodynamicsView;
}

class XQ_QT_SIMULATION_EXPORT xq_HemodynamicsView : public QmitkAbstractView
{
  Q_OBJECT

public:
  static const QString VIEW_ID;

  xq_HemodynamicsView();
  ~xq_HemodynamicsView() override;

protected:
  void CreateQtPartControl(QWidget* parent) override;
  void SetFocus() override;
  void OnSelectionChanged(berry::IWorkbenchPart::Pointer source,
                          const QList<mitk::DataNode::Pointer>& nodes) override;

private slots:
  void CreateSimJob();
  void SaveJob();
  void RunSimulation();
  void StopSimulation();
  void ExportResults();
  void ExportOnly();
  void ExportAndRun();
  void OnSolverOutput(const QString& text);
  void OnSolverFinished(int exitCode);
  void AddBoundaryCondition();
  void RemoveBoundaryCondition();
  void OnWallTypeChanged(int index);
  void EditBCValues(int row, int column);
  void ApplySolverPreset();
  void ApplyResultColorMap();
  void ToggleResultLegend();
  void OnAutoRangeToggled(bool checked);

private:
  Ui::xq_HemodynamicsView* m_Ui;
  mitk::DataNode::Pointer m_CurrentMeshNode;
  mitk::DataNode::Pointer m_SimPrepNode;
  xq_SolverProcessHandler* m_SolverHandler;
  bool m_LegendVisible;
};

#endif // XQ_HEMODYNAMICSVIEW_H
