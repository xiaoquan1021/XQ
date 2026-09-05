#ifndef XQ_EXTRACTCENTERLINESACTION_H
#define XQ_EXTRACTCENTERLINESACTION_H

#include <QAction>
#include <QThread>

#include <mitkDataStorage.h>
#include <mitkDataNode.h>

class xq_ExtractCenterlinesAction : public QAction
{
  Q_OBJECT

public:
  explicit xq_ExtractCenterlinesAction(QObject* parent = nullptr);
  ~xq_ExtractCenterlinesAction() override;

  void SetDataStorage(mitk::DataStorage::Pointer dataStorage);
  void SetModelNode(mitk::DataNode::Pointer modelNode);

public slots:
  void Execute();

signals:
  void ExtractionFinished(bool success);

private:
  class WorkerThread : public QThread
  {
  public:
    WorkerThread(mitk::DataStorage::Pointer ds, mitk::DataNode::Pointer node);
    void run() override;

    mitk::DataNode::Pointer GetResultNode() const { return m_ResultNode; }
    bool WasSuccessful() const { return m_Success; }

  private:
    mitk::DataStorage::Pointer m_DataStorage;
    mitk::DataNode::Pointer m_ModelNode;
    mitk::DataNode::Pointer m_ResultNode;
    bool m_Success;
  };

  void OnExtractionComplete();

  mitk::DataStorage::Pointer m_DataStorage;
  mitk::DataNode::Pointer m_ModelNode;
  WorkerThread* m_Worker;
};

#endif // XQ_EXTRACTCENTERLINESACTION_H
