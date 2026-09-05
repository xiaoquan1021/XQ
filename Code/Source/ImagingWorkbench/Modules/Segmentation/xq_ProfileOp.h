// XQ ProfileOp: lightweight operation descriptor for undo/redo contour mutations
#pragma once

#include <xqModuleSegmentationExports.h>

#include <mitkOperation.h>
#include <mitkPoint.h>

class xq_LumenProfile;

enum xq_ProfileOpType
{
    OpINSERTPROFILE = 200,
    OpREMOVEPROFILE = 201,
    OpSETPROFILE    = 202
};

class XQMODULESEGMENTATION_EXPORT xq_ProfileOp : public mitk::Operation
{
public:
    xq_ProfileOp(mitk::OperationType operationType,
                 xq_LumenProfile* contour,
                 int pathPosIndex,
                 unsigned int timeStep = 0);

    xq_ProfileOp(mitk::OperationType operationType,
                 int pathPosIndex,
                 unsigned int timeStep = 0);

    ~xq_ProfileOp() override = default;

    xq_LumenProfile* GetProfile() const;
    int GetPathPosIndex() const;
    unsigned int GetTemporalIndex() const;

private:
    xq_LumenProfile* m_Profile = nullptr;
    int m_PathPosIndex = 0;
    unsigned int m_TemporalIndex = 0;
};
