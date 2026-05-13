#ifndef XQ_DATAEXPLORERVIEW_H
#define XQ_DATAEXPLORERVIEW_H

#include <QmitkAbstractView.h>
#include <QmitkNodeDescriptorManager.h>
#include <QItemSelection>

#include <XQ_QT_DATAMANAGERExports.h>

// Forward declarations
class QMenu;
class QAction;
class QSlider;
class QModelIndex;
class QTreeView;
class QPushButton;
class QLabel;
class QLineEdit;
class QTableWidget;
class QTimer;
class QmitkDataStorageTreeModel;

namespace Ui {
class xq_DataExplorerView;
}

class XQ_QT_DATAMANAGER_EXPORT xq_DataExplorerView : public QmitkAbstractView
{
  Q_OBJECT

public:
  static const QString VIEW_ID;

  xq_DataExplorerView();
  ~xq_DataExplorerView() override;

  QTreeView* GetTreeView();

public slots:
  void OpacityChanged(int value);
  void ColorChanged();
  void RemoveSelectedNodes();
  void ToggleVisibility();
  void GlobalReinit();

  // Phase 9 enhancements
  void RenameSelectedNode();
  void ShowOnlySelected();
  void MakeAllVisible();
  void MakeAllInvisible();
  void ReinitializeSelectedNode();
  void ShowNodeInfo();
  void OnSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
  void OnSearchTextChanged(const QString& text);

  // Phase 10 enhancements
  void SetRepresentationSurface();
  void SetRepresentationWireframe();
  void SetRepresentationPoints();
  void ExportSelectedNode();
  void AddImageToSelectedFolder();

  // Properties panel
  void TogglePropertiesPanel();

  // DICOM information
  void ShowDicomInfo();

  void CopySelectedNode();
  void PasteDataNode();

protected:
  void CreateQtPartControl(QWidget* parent) override;
  void SetFocus() override;
  void NodeChanged(const mitk::DataNode* node) override;

private:
  void UpdateOpacitySliderForNode(mitk::DataNode* node);
  void SetAllNodesVisibility(bool visible);
  mitk::DataNode* GetSelectedNode();
  void UpdatePropertiesTable(mitk::DataNode* node);
  void DeferredRenderUpdate();

  QTreeView* m_NodeTreeView;
  QmitkDataStorageTreeModel* m_NodeTreeModel;
  QSlider* m_OpacitySlider;
  QPushButton* m_ColorButton;
  QLabel* m_OpacityValueLabel;
  QLineEdit* m_SearchBox;
  QMenu* m_ContextMenu;
  Ui::xq_DataExplorerView* m_Ui;
  bool m_InternalSliderUpdate;

  // Properties panel
  QTableWidget* m_PropertiesTable;
  QPushButton* m_PropertiesToggle;

  // Render debounce timer
  QTimer* m_RenderDebounceTimer;

  mitk::DataNode::Pointer m_CopiedNode;
};

#endif // XQ_DATAEXPLORERVIEW_H
