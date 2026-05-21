#ifndef XQ_HEMODYNAMICSVIEW_H
#define XQ_HEMODYNAMICSVIEW_H

#include <QmitkAbstractView.h>
#include <XQ_QT_SIMULATIONExports.h>

#include <mitkDataNode.h>

class QComboBox;
class QLabel;

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
  void SetSimulationStatus(const char* status);
  void ClearSimulationState();
  void EnableSimulationControls(bool hasMesh, bool hasJob, bool hasResult);
  bool EnsureSolverHandler();
  bool StartConfiguredSolver(const QString& workDir);
  mitk::DataNode::Pointer ResolveMeshForSimulationJob(mitk::DataNode* jobNode) const;
  void RestoreSimulationParametersFromMetadata();
  void PersistSimulationParametersToMetadata();
  void BindCurrentDataManagerSelection();
  QComboBox* CreateBoundaryConditionTypeCombo(const QString& currentText = QString());
  QString SerializeBoundaryConditionRows() const;
  void RestoreBoundaryConditionRows(const QString& serializedRows);
  bool IsSimulationExportReady(QString* reason = nullptr);
  bool IsSimulationRunReady(QString* reason = nullptr);
  QString BuildSimulationReadinessStatus(bool requireRunnable);
  void UpdateContextStatus(const QString& status = QString());

  Ui::xq_HemodynamicsView* m_Ui;
  QLabel* m_ContextLabel;
  mitk::DataNode::Pointer m_CurrentMeshNode;
  mitk::DataNode::Pointer m_CurrentResultNode;
  mitk::DataNode::Pointer m_SimPrepNode;
  xq_SolverProcessHandler* m_SolverHandler;
  bool m_StopRequested;
  bool m_LegendVisible;
  bool m_RestoringSimulationMetadata;
};

#endif // XQ_HEMODYNAMICSVIEW_H
