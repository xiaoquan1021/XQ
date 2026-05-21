#ifndef XQ_SOLVEREXPORTACTION_H
#define XQ_SOLVEREXPORTACTION_H

#include "xq_HemoContextMenuActionBase.h"

class xq_SolverExportAction : public QObject, public xqmitk::IContextMenuAction
{
  Q_OBJECT
  Q_INTERFACES(xqmitk::IContextMenuAction)
public:
  xq_SolverExportAction();
  ~xq_SolverExportAction() override;
  void Run(const QList<mitk::DataNode::Pointer>& selectedNodes) override;
  void SetDataStorage(mitk::DataStorage* dataStorage) override;
  void SetSmoothed(bool) override {}
  void SetDecimated(bool) override {}
  void SetFunctionality(berry::QtViewPart*) override {}
private:
  mitk::DataStorage::Pointer m_DataStorage;
};

#endif
