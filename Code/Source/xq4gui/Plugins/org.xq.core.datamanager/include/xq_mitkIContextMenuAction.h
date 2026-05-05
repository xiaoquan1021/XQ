#ifndef XQ_MITK_ICONTEXTMENUACTION_H
#define XQ_MITK_ICONTEXTMENUACTION_H

#include <berryMacros.h>
#include <berryQtViewPart.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <QList>

namespace xqmitk
{

struct IContextMenuAction
{
  virtual ~IContextMenuAction() = default;

  virtual void Run(const QList<mitk::DataNode::Pointer>& selectedNodes) = 0;
  virtual void SetDataStorage(mitk::DataStorage* dataStorage) = 0;
  virtual void SetSmoothed(bool smoothed) = 0;
  virtual void SetDecimated(bool decimated) = 0;
  virtual void SetFunctionality(berry::QtViewPart* functionality) = 0;
};

} // namespace xqmitk

Q_DECLARE_INTERFACE(xqmitk::IContextMenuAction, "org.xq.datamanager.IContextMenuAction")

#endif // XQ_MITK_ICONTEXTMENUACTION_H
