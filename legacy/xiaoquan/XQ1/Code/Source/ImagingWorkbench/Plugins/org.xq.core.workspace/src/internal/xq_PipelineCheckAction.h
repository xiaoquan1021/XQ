#ifndef XQ_PIPELINECHECKACTION_H
#define XQ_PIPELINECHECKACTION_H

#include <XQ_QT_PROJECTMANAGERExports.h>
#include "xq_mitkIContextMenuAction.h"

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <QObject>
#include <QList>

#include <berryQtViewPart.h>

class XQ_QT_PROJECTMANAGER_EXPORT xq_PipelineCheckAction
  : public QObject
  , public xqmitk::IContextMenuAction
{
  Q_OBJECT
  Q_INTERFACES(xqmitk::IContextMenuAction)

public:
  xq_PipelineCheckAction();
  ~xq_PipelineCheckAction() override;

  void Run(const QList<mitk::DataNode::Pointer>& selectedNodes) override;
  void SetDataStorage(mitk::DataStorage* dataStorage) override;
  void SetSmoothed(bool) override {}
  void SetDecimated(bool) override {}
  void SetFunctionality(berry::QtViewPart*) override {}

private:
  mitk::DataStorage::Pointer m_DataStorage;
};

#endif // XQ_PIPELINECHECKACTION_H
