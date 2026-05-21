#ifndef XQ_IMAGEPROCESSINGVIEW_H
#define XQ_IMAGEPROCESSINGVIEW_H

#include <QmitkAbstractView.h>
#include <XQ_QT_IMAGEPROCESSINGExports.h>

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QTextEdit;
class QGroupBox;

class XQ_QT_IMAGEPROCESSING_EXPORT xq_ImageProcessingView : public QmitkAbstractView
{
  Q_OBJECT

public:
  static const QString VIEW_ID;

  xq_ImageProcessingView();
  ~xq_ImageProcessingView() override;

protected:
  void CreateQtPartControl(QWidget* parent) override;
  void SetFocus() override;
  void OnSelectionChanged(berry::IWorkbenchPart::Pointer source,
                          const QList<mitk::DataNode::Pointer>& nodes) override;

private slots:
  void RefreshImageList();
  void UpdateParameterVisibility();
  void ApplyOperation();

private:
  mitk::DataNode::Pointer CurrentImageNode() const;
  void ClearImageProcessingState();
  void SetDiagnostic(const QString& message, bool isError = false);
  void BindCurrentDataManagerSelection();
  bool RestoreProcessingMetadataForNode(const mitk::DataNode::Pointer& node);
  void PersistProcessingMetadata();
  bool SelectImageNodeByName(const std::string& name);
  void SetOperationByName(const std::string& operation);
  bool IsApplyReady(QString* reason = nullptr) const;
  void UpdateApplyButtonState();
  void UpdateContextStatus(const QString& status = QString(), bool isError = false);

  QWidget* m_Control;
  QComboBox* m_ImageComboBox;
  QComboBox* m_OperationComboBox;
  QGroupBox* m_ThresholdGroup;
  QGroupBox* m_SeedGroup;
  QGroupBox* m_CropGroup;
  QGroupBox* m_ResampleGroup;
  QDoubleSpinBox* m_LowerSpinBox;
  QDoubleSpinBox* m_UpperSpinBox;
  QDoubleSpinBox* m_InsideSpinBox;
  QDoubleSpinBox* m_OutsideSpinBox;
  QSpinBox* m_SeedXSpinBox;
  QSpinBox* m_SeedYSpinBox;
  QSpinBox* m_SeedZSpinBox;
  QSpinBox* m_CropXSpinBox;
  QSpinBox* m_CropYSpinBox;
  QSpinBox* m_CropZSpinBox;
  QSpinBox* m_CropSizeXSpinBox;
  QSpinBox* m_CropSizeYSpinBox;
  QSpinBox* m_CropSizeZSpinBox;
  QDoubleSpinBox* m_SpacingXSpinBox;
  QDoubleSpinBox* m_SpacingYSpinBox;
  QDoubleSpinBox* m_SpacingZSpinBox;
  QDoubleSpinBox* m_IsoValueSpinBox;
  QPushButton* m_ApplyButton;
  QLabel* m_ContextLabel;
  QTextEdit* m_DiagnosticText;
  QList<mitk::DataNode::Pointer> m_ImageNodes;
  mitk::DataNode::Pointer m_CurrentProcessingNode;
};

#endif
