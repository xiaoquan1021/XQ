#ifndef XQ_MODELFACESELECTIONWIDGET_H
#define XQ_MODELFACESELECTIONWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QMenu>

#include <mitkDataNode.h>

class QPushButton;
class QVBoxLayout;

class xq_ModelFaceSelectionWidget : public QWidget
{
  Q_OBJECT

public:
  explicit xq_ModelFaceSelectionWidget(QWidget* parent = nullptr);
  ~xq_ModelFaceSelectionWidget() override;

  void SetModelNode(mitk::DataNode::Pointer node);
  void Clear();

  struct FaceEntry
  {
    int id;
    QString name;
    QString type;
    QColor color;
    bool visible;
  };

  QList<FaceEntry> GetSelectedFaces() const;
  QList<FaceEntry> GetAllFaces() const;

signals:
  void FaceSelectionChanged();
  void FaceRenamed(int faceId, const QString& newName);
  void FaceColorChanged(int faceId, const QColor& color);
  void FaceVisibilityToggled(int faceId, bool visible);

private slots:
  void OnCellDoubleClicked(int row, int column);
  void OnCellClicked(int row, int column);
  void ShowContextMenu(const QPoint& pos);
  void RenameSelectedFace();
  void SelectAllFaces();
  void DeselectAllFaces();

private:
  void PopulateTable(mitk::DataNode::Pointer node);

  enum Column
  {
    COL_VISIBLE = 0,
    COL_NAME    = 1,
    COL_TYPE    = 2,
    COL_COLOR   = 3,
    COL_COUNT   = 4
  };

  QTableWidget* m_Table;
  QMenu* m_ContextMenu;
  QAction* m_RenameAction;
  QAction* m_SelectAllAction;
  QAction* m_DeselectAllAction;
  mitk::DataNode::Pointer m_ModelNode;
};

#endif // XQ_MODELFACESELECTIONWIDGET_H
