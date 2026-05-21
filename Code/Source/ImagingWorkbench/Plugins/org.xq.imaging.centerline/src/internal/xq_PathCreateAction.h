#ifndef XQ_PATHCREATEACTION_H
#define XQ_PATHCREATEACTION_H

#include <mitkDataNode.h>
#include <mitkDataStorage.h>
#include <berryQtViewPart.h>
#include <QObject>
#include <QList>

#include "xq_mitkIContextMenuAction.h"

class xq_PathCreateAction : public QObject, public xqmitk::IContextMenuAction
{
  Q_OBJECT
  Q_INTERFACES(xqmitk::IContextMenuAction)
public:
  xq_PathCreateAction();
  ~xq_PathCreateAction() override;
  void Run(const QList<mitk::DataNode::Pointer>& selectedNodes) override;
  void SetDataStorage(mitk::DataStorage* dataStorage) override;
  void SetSmoothed(bool) override {}
  void SetDecimated(bool) override {}
  void SetFunctionality(berry::QtViewPart*) override {}
private:
  mitk::DataStorage::Pointer m_DataStorage;
};

#endif
