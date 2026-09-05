#ifndef XQ_DATANODEOPERATION_H
#define XQ_DATANODEOPERATION_H

#include <xqProjectManagementExports.h>

#include <mitkOperation.h>
#include <mitkDataNode.h>

enum xq_DataNodeOperationType
{
    OpADDDATANODE    = 400,
    OpREMOVEDATANODE = 401
};

class XQPROJECTMANAGEMENT_EXPORT xq_DataNodeOperation : public mitk::Operation
{
public:
    xq_DataNodeOperation(mitk::OperationType operationType,
                         mitk::DataNode::Pointer dataNode,
                         mitk::DataNode::Pointer parentNode);

    ~xq_DataNodeOperation() override;

    mitk::DataNode::Pointer GetDataNode() const;
    mitk::DataNode::Pointer GetParentNode() const;

private:
    mitk::DataNode::Pointer m_DataNode;
    mitk::DataNode::Pointer m_ParentNode;
};

#endif
