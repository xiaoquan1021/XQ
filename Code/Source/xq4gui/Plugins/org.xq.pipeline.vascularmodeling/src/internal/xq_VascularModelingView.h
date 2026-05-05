#ifndef XQ_VASCULARMODELINGVIEW_H
#define XQ_VASCULARMODELINGVIEW_H

#include <QmitkAbstractView.h>
#include <XQ_QT_MODELINGExports.h>

class QComboBox;
class QTableWidget;
class QTabWidget;
class QLineEdit;
class QPushButton;
class QAction;

namespace Ui {
class xq_VascularModelingView;
}

class XQ_QT_MODELING_EXPORT xq_VascularModelingView : public QmitkAbstractView
{
  Q_OBJECT

public:
  static const QString VIEW_ID;

  xq_VascularModelingView();
  ~xq_VascularModelingView() override;

protected:
  void CreateQtPartControl(QWidget* parent) override;
  void SetFocus() override;
  void OnSelectionChanged(berry::IWorkbenchPart::Pointer source,
                          const QList<mitk::DataNode::Pointer>& nodes) override;

private slots:
  void CreateModel();
  void DeleteModel();
  void ChangeFaceColor();
  void ToggleFaceVisibility();
  void ExportModel();
  void ExtractCenterlines();
  void BooleanUnion();
  void BooleanSubtract();
  void ApplyFillet();
  void ShowModelStatistics();
  void DecimateSurface();
  void ComputeNormals();
  void CleanSurface();
  void FillHoles();

private:
  void PopulateFaceTable(mitk::DataNode::Pointer modelNode);
  void ClearFaceTable();

  Ui::xq_VascularModelingView* m_Ui;
  mitk::DataNode::Pointer m_CurrentModelNode;
};

#endif // XQ_VASCULARMODELINGVIEW_H
