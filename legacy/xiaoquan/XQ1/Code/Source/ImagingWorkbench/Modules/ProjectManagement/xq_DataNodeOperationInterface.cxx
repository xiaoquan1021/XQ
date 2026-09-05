#include "xq_DataNodeOperationInterface.h"
#include "xq_DataNodeOperation.h"

#include <mitkDataNode.h>
#include <mitkUndoController.h>

xq_DataNodeOperationInterface::xq_DataNodeOperationInterface()
    : m_DataStorage(nullptr)
{
}

xq_DataNodeOperationInterface::~xq_DataNodeOperationInterface()
{
}

void xq_DataNodeOperationInterface::SetDataStorage(mitk::DataStorage::Pointer dataStorage)
{
    m_DataStorage = dataStorage;
}

mitk::DataStorage::Pointer xq_DataNodeOperationInterface::GetDataStorage() const
{
    return m_DataStorage;
}

void xq_DataNodeOperationInterface::ExecuteOperation(mitk::Operation* operation)
{
    if (!operation || !m_DataStorage)
    {
        return;
    }

    auto* dataNodeOp = dynamic_cast<xq_DataNodeOperation*>(operation);
    if (!dataNodeOp)
    {
        return;
    }

    mitk::DataNode::Pointer dataNode = dataNodeOp->GetDataNode();
    mitk::DataNode::Pointer parentNode = dataNodeOp->GetParentNode();

    if (!dataNode)
    {
        return;
    }

    switch (operation->GetOperationType())
    {
    case OpADDDATANODE:
    {
        if (parentNode.IsNotNull())
        {
            m_DataStorage->Add(dataNode, parentNode);
        }
        else
        {
            m_DataStorage->Add(dataNode);
        }
        break;
    }
    case OpREMOVEDATANODE:
    {
        m_DataStorage->Remove(dataNode);
        break;
    }
    default:
        break;
    }
}
