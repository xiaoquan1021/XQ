#ifndef XQ_GRIDGENERATIONVIEW_H
#define XQ_GRIDGENERATIONVIEW_H

#include <QmitkAbstractView.h>
#include <XQ_QT_MESHINGExports.h>

class QComboBox;
class QTabWidget;
class QTableWidget;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QGroupBox;

namespace Ui {
class xq_GridGenerationView;
}

class XQ_QT_MESHING_EXPORT xq_GridGenerationView : public QmitkAbstractView
{
  Q_OBJECT

public:
  static const QString VIEW_ID;

  xq_GridGenerationView();
  ~xq_GridGenerationView() override;

protected:
  void CreateQtPartControl(QWidget* parent) override;
  void SetFocus() override;
  void OnSelectionChanged(berry::IWorkbenchPart::Pointer source,
                          const QList<mitk::DataNode::Pointer>& nodes) override;

private slots:
  void CreateMesh();
  void RunMeshing();
  void AdaptMesh();
  void SetGlobalSize(double size);
  void AddLocalSize();
  void ShowMeshQualityReport();
  void ExportMesh();
  void PreviewBoundaryLayer();
  void OnBoundaryLayerToggled(bool enabled);
  void AddSphereRefinementRegion();
  void AddCylinderRefinementRegion();
  void RemoveRefinementRegion();
  void VisualizeRefinementRegions();

private:
  void UpdateModelSelector();
  void PopulateLocalSizeTable();
  void UpdateMeshStatistics();
  void ClearMeshStatistics();

  Ui::xq_GridGenerationView* m_Ui;
  mitk::DataNode::Pointer m_CurrentModelNode;
  mitk::DataNode::Pointer m_CurrentMeshNode;
};

#endif // XQ_GRIDGENERATIONVIEW_H
