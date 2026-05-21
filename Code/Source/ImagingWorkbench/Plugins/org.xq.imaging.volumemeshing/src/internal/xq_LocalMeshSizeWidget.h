#ifndef XQ_LOCALMESHSIZEWIDGET_H
#define XQ_LOCALMESHSIZEWIDGET_H

#include <QWidget>
#include <QTableWidget>

#include <mitkDataNode.h>

class QPushButton;

class xq_LocalMeshSizeWidget : public QWidget
{
  Q_OBJECT

public:
  explicit xq_LocalMeshSizeWidget(QWidget* parent = nullptr);
  ~xq_LocalMeshSizeWidget() override;

  void SetModelNode(mitk::DataNode::Pointer node);
  void Clear();

  struct LocalSizeEntry
  {
    QString faceName;
    QString faceType;
    double edgeSize;
  };

  QList<LocalSizeEntry> GetAllEntries() const;

public slots:
  void AddEntry();
  void RemoveSelectedEntries();

signals:
  void EntriesChanged();

private:
  void PopulateFromModel(mitk::DataNode::Pointer node);

  enum Column
  {
    COL_FACE_NAME = 0,
    COL_TYPE      = 1,
    COL_EDGE_SIZE = 2,
    COL_COUNT     = 3
  };

  QTableWidget* m_Table;
  QPushButton* m_AddButton;
  QPushButton* m_RemoveButton;
  mitk::DataNode::Pointer m_ModelNode;
};

#endif // XQ_LOCALMESHSIZEWIDGET_H
