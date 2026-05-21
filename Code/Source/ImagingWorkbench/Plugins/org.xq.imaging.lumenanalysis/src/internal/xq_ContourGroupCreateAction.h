#ifndef XQ_CONTOURGROUPCREATEACTION_H
#define XQ_CONTOURGROUPCREATEACTION_H

#include <mitkDataNode.h>
#include <mitkDataStorage.h>
#include <berryQtViewPart.h>
#include <QObject>
#include <QList>

#include "xq_mitkIContextMenuAction.h"

class xq_ContourGroupCreateAction : public QObject, public xqmitk::IContextMenuAction
{
  Q_OBJECT
  Q_INTERFACES(xqmitk::IContextMenuAction)
public:
  xq_ContourGroupCreateAction();
  ~xq_ContourGroupCreateAction() override;
  void Run(const QList<mitk::DataNode::Pointer>& selectedNodes) override;
  void SetDataStorage(mitk::DataStorage* dataStorage) override;
  void SetSmoothed(bool) override {}
  void SetDecimated(bool) override {}
  void SetFunctionality(berry::QtViewPart*) override {}
private:
  mitk::DataStorage::Pointer m_DataStorage;
};

#endif
