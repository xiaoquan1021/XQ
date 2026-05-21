#include "xq_DataNodeOperation.h"

xq_DataNodeOperation::xq_DataNodeOperation(
    mitk::OperationType operationType,
    mitk::DataNode::Pointer dataNode,
    mitk::DataNode::Pointer parentNode)
    : mitk::Operation(operationType)
    , m_DataNode(dataNode)
    , m_ParentNode(parentNode)
{
}

xq_DataNodeOperation::~xq_DataNodeOperation()
{
}

mitk::DataNode::Pointer xq_DataNodeOperation::GetDataNode() const
{
    return m_DataNode;
}

mitk::DataNode::Pointer xq_DataNodeOperation::GetParentNode() const
{
    return m_ParentNode;
}
