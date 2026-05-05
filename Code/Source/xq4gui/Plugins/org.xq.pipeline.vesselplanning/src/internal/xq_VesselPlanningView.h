#ifndef XQ_VESSELPLANNINGVIEW_H
#define XQ_VESSELPLANNINGVIEW_H

#include <QmitkAbstractView.h>
#include <QWidgetAction>
#include <mitkDataNode.h>
#include <mitkDataInteractor.h>
#include <mitkImage.h>
#include <mitkProportionalTimeGeometry.h>
#include <XQ_QT_PATHPLANNINGExports.h>

class QTableView;
class QMenu;
class QAction;
class QSlider;
class QLabel;
class QDoubleSpinBox;

namespace Ui {
class xq_VesselPlanningView;
}

class XQ_QT_PATHPLANNING_EXPORT xq_VesselPlanningView : public QmitkAbstractView
{
  Q_OBJECT

public:
  static const QString VIEW_ID;

  xq_VesselPlanningView();
  ~xq_VesselPlanningView() override;

protected:
  void CreateQtPartControl(QWidget* parent) override;
  void SetFocus() override;
  void OnSelectionChanged(berry::IWorkbenchPart::Pointer source,
                          const QList<mitk::DataNode::Pointer>& nodes) override;

protected slots:
  void AddPath();
  void DeletePath();
  void AddPoint();
  void DeletePoint();
  void AddSmartPoint();
  void SelectPoint();
  void InterpolatePath();
  void ShowPathStatistics();
  void ExportPath();
  void OnReslicePositionChanged(int index);
  void OnPathTableSelectionChanged(const QItemSelection& selected,
                                   const QItemSelection& deselected);
  void OnPointTableSelectionChanged(const QItemSelection& selected,
                                    const QItemSelection& deselected);

private slots:
  void ResamplePath();
  void ReversePath();
  void MergePaths();
  void ShowCrossSectionInfo();
  void SmoothPath();

private:
  void UpdatePathTable();
  void UpdatePointTable();
  void UpdatePathReslice();
  mitk::Image::Pointer FindSelectedImage() const;
  mitk::DataNode::Pointer FindSelectedImageNode() const;
  void EnsureSourceImageProperty(const mitk::DataNode::Pointer& pathNode,
                                  const mitk::DataNode* sourcePathNode = nullptr);
  void AddPathNodeToFolder(const mitk::DataNode::Pointer& pathNode);
  void SetupConnections();

  Ui::xq_VesselPlanningView* m_Ui;
  QTableView* m_PathTableView;
  QTableView* m_PointTableView;
  QSlider* m_ResliceSlider;
  QLabel* m_ResliceLabel;
  QDoubleSpinBox* m_ResliceSizeSpin;
  mitk::DataNode::Pointer m_CurrentPathNode;
  mitk::DataInteractor::Pointer m_PathInteractor;
  mitk::ProportionalTimeGeometry::Pointer m_CurrentSlicedGeometry;
};

// Point size QWidgetAction helper (extracted to top level for MOC support)
class XQ_QT_PATHPLANNING_EXPORT PathPointSizeAction : public QWidgetAction
{
  Q_OBJECT
public:
  PathPointSizeAction(const QString& label, const QString& prefKey,
                      int minVal, int maxVal, int defaultVal, int tickInterval,
                      QWidget* parent = nullptr);
  ~PathPointSizeAction() override;
protected slots:
  void OnSizeChanged(int value);
protected:
  QWidget* createWidget(QWidget* parent) override;
private:
  QString m_Label;
  QString m_PrefKey;
  int m_MinVal;
  int m_MaxVal;
  int m_DefaultVal;
  int m_TickInterval;
  QSlider* m_Slider;
};

#endif // XQ_VESSELPLANNINGVIEW_H
