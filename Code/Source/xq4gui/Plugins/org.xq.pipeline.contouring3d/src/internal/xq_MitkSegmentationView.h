#ifndef XQ_MITKSEGMENTATIONVIEW_H
#define XQ_MITKSEGMENTATIONVIEW_H

#include <QmitkAbstractView.h>

#include <XQ_QT_MITKSEGMENTATIONExports.h>

#include <mitkDataNode.h>
#include <xq_SegmentationUtils.h>

class QComboBox;
class QToolBar;
class QStackedWidget;
class xq_ProfileGroup;

namespace Ui {
class xq_MitkSegmentationView;
}

class XQ_QT_MITKSEGMENTATION_EXPORT xq_MitkSegmentationView : public QmitkAbstractView
{
  Q_OBJECT

public:
  static const QString VIEW_ID;

  xq_MitkSegmentationView();
  ~xq_MitkSegmentationView() override;

protected:
  void CreateQtPartControl(QWidget* parent) override;
  void SetFocus() override;
  void OnSelectionChanged(berry::IWorkbenchPart::Pointer source,
                          const QList<mitk::DataNode::Pointer>& nodes) override;

private slots:
  void OnImageSelectionChanged(int index);
  void OnToolSelected(int toolId);
  void OnThresholdApply();
  void OnRegionGrowApply();
  void OnLevelSetApply();
  void CreateNewSegmentation();
  void UpdateImageList();

  // Phase 16a: Morphological operations
  void OnErodeApply();
  void OnDilateApply();
  void OnMaskImageApply();
  void OnInvertSegmentation();

private:
  Ui::xq_MitkSegmentationView* m_Ui;
  QComboBox* m_ImageSelector;
  QToolBar* m_ToolBar;
  QStackedWidget* m_ToolParameterStack;

  mitk::DataNode::Pointer m_ReferenceNode;
  mitk::DataNode::Pointer m_WorkingNode;
  // Tracks the currently selected preprocessing target (ProfileGroup node).
  mitk::DataNode::Pointer m_ActiveProfileGroupNode;
  int m_ActiveToolId;

  // Resolves the active preprocessing target state via xq_SegmentationUtils.
  // Returns the resolution struct; callers should check state before proceeding.
  xq_PreprocTargetResolution ResolveTargetContext() const;

  // Returns the correct parent node for new segmentation result nodes.
  // When a TargetReady ProfileGroup is active, result nodes are parented under it;
  // otherwise falls back to the reference image node.
  mitk::DataNode::Pointer GetSegmentationParentNode() const;

  // Checks whether segmentation-producing actions can proceed.
  // Returns false and fills outReason when the target is ineligible.
  // Returns true when the target is ready (canonical) or absent (legacy path).
  bool CheckPreprocessingTarget(QString& outReason) const;
};

#endif // XQ_MITKSEGMENTATIONVIEW_H
