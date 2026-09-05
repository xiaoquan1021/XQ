#ifndef XQ_MULTIPHYSICSJOBCREATEACTION_H
#define XQ_MULTIPHYSICSJOBCREATEACTION_H

#include "xq_mitkIContextMenuAction.h"

#include <berryQtViewPart.h>
#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <QObject>
#include <QList>

class xq_MultiPhysicsJobCreateAction : public QObject, public xqmitk::IContextMenuAction
{
  Q_OBJECT
  Q_INTERFACES(xqmitk::IContextMenuAction)

public:
  xq_MultiPhysicsJobCreateAction();
  ~xq_MultiPhysicsJobCreateAction() override;

  void Run(const QList<mitk::DataNode::Pointer>& selectedNodes) override;
  void SetDataStorage(mitk::DataStorage* dataStorage) override;
  void SetSmoothed(bool) override {}
  void SetDecimated(bool) override {}
  void SetFunctionality(berry::QtViewPart*) override {}

private:
  mitk::DataStorage::Pointer m_DataStorage;
};

#endif
