#ifndef XQ_LUMENCONTOURINGVIEW_H
#define XQ_LUMENCONTOURINGVIEW_H

#include <QmitkAbstractView.h>
#include <itkSmartPointer.h>
#include <mitkDataNode.h>
#include <mitkImage.h>
#include <mitkProportionalTimeGeometry.h>
#include <XQ_QT_SEGMENTATIONExports.h>

class QComboBox;
class QListWidget;
class QPushButton;
class QSlider;
class QLabel;
class QDoubleSpinBox;
class xq_ProfileGroupInteractor;
struct xq_ProfilePlacementFrame;

namespace Ui {
class xq_LumenContouringView;
}

class XQ_QT_SEGMENTATION_EXPORT xq_LumenContouringView : public QmitkAbstractView
{
  Q_OBJECT

public:
  static const QString VIEW_ID;

  xq_LumenContouringView();
  ~xq_LumenContouringView() override;

protected:
  void CreateQtPartControl(QWidget* parent) override;
  void SetFocus() override;
  void OnSelectionChanged(berry::IWorkbenchPart::Pointer source,
                          const QList<mitk::DataNode::Pointer>& nodes) override;

private slots:
  void CreateContourGroup();
  void DeleteContourGroup();
  void AddContour();
  void DeleteContour();
  void LoftContourGroup();
  void OnContourTypeChanged(int index);
  void OnPathSelectionChanged(int index);
  void UpdateContourList();
  void SetCircleContour();
  void SetEllipseContour();
  void SetSplinePolygonContour();
  void SetManualContour();
  void CopyContour();
  void PasteContour();
  void ScaleContour();
  void OnReslicePositionChanged(int index);
  void SmoothContour();
  void ResampleContour();
  void ShowContourStatistics();
  void PerformAutoSegmentation();
  void OnApplyThresholdContour();

private:
  void EnsureProfileInteractor(int pathPosIndex);
  void DetachProfileInteractor();
  void PopulatePathComboBox();
  void UpdatePathReslice();
  mitk::Image::Pointer FindSelectedImage();

  Ui::xq_LumenContouringView* m_Ui;
  QComboBox* m_PathComboBox;
  QListWidget* m_ContourGroupListWidget;
  QListWidget* m_ContourListWidget;
  QSlider* m_ResliceSlider;
  QLabel* m_ResliceLabel;
  QDoubleSpinBox* m_ResliceSizeSpin;
  mitk::DataNode::Pointer m_CurrentContourGroupNode;
  mitk::DataNode::Pointer m_CopiedContourNode;
  itk::SmartPointer<xq_ProfileGroupInteractor> m_ProfileInteractor;
  mitk::ProportionalTimeGeometry::Pointer m_CurrentSlicedGeometry;
  int m_CurrentContourIndex;
};

#endif // XQ_LUMENCONTOURINGVIEW_H
