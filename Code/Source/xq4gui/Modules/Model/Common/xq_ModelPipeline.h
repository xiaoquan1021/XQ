#pragma once

#include <xqModelCommonExports.h>

#include <xq_PipelineDataUtils.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <vtkSmartPointer.h>

#include <string>

class vtkPolyData;

struct XQMODELCOMMON_EXPORT xq_CreateModelRequest
{
    std::string modelName;
    std::string modelType = "PolyData";
    int numSampling = 60;
    // Optional: restrict ContourGroup selection to groups whose
    // xq.source.path equals pathFilter. Empty = take every ContourGroup.
    std::string pathFilter;
    // Solid modeler engine ("vtk" | "occt"). Empty = auto (vtk default).
    std::string engine;
    double blendRadius = 0.0;
};

struct XQMODELCOMMON_EXPORT xq_CreateModelResult
    : public xq::pipeline::OperationStatus
{
    mitk::DataNode::Pointer node;
    vtkSmartPointer<vtkPolyData> surface;
};

class XQMODELCOMMON_EXPORT xq_ModelPipelineService
{
public:
    static xq_CreateModelResult CreateModel(
        mitk::DataStorage* dataStorage,
        const xq_CreateModelRequest& request);
};
