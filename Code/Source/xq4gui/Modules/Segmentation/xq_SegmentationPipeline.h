#pragma once

#include <xqModuleSegmentationExports.h>

#include <xq_PipelineDataUtils.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <string>

class xq_ProfileGroup;

struct XQMODULESEGMENTATION_EXPORT xq_CreateContourGroupRequest
{
    std::string groupName;
    std::string pathName;
};

struct XQMODULESEGMENTATION_EXPORT xq_CreateContourGroupResult
    : public xq::pipeline::OperationStatus
{
    mitk::DataNode::Pointer node;
    xq_ProfileGroup* profileGroup = nullptr;
};

// XQ Pipeline Stage 2 automatic extraction: reads Image + Path nodes
// from DataStorage, reslices along each path frame normal plane using
// vtkImageReslice, runs the selected XQSegmentationAlgorithm on each
// slice, and writes the resulting ContourGroup node with canonical
// xq.source.image / xq.source.path backlinks.
struct XQMODULESEGMENTATION_EXPORT xq_ExtractContoursRequest
{
    std::string groupName;
    std::string pathName;                  // REQUIRED upstream Path node name
    std::string imageNodeName;             // REQUIRED upstream Image node name
    std::string algorithm   = "threshold"; // "threshold" | "levelset"
    double      threshold   = 0.0;
    int         strideAlongPath = 10;      // sample every Nth trace vertex
    double      sliceSizeMm = 20.0;        // physical extent per resliced slice
    double      pixelSpacing = 0.2;        // pixel size of the resliced image
    int         outputPoints = 64;         // points per output contour
};

struct XQMODULESEGMENTATION_EXPORT xq_ExtractContoursResult
    : public xq::pipeline::OperationStatus
{
    mitk::DataNode::Pointer node;
    int contoursExtracted = 0;
};

class XQMODULESEGMENTATION_EXPORT xq_SegmentationPipelineService
{
public:
    static xq_CreateContourGroupResult CreateContourGroup(
        mitk::DataStorage* dataStorage,
        const xq_CreateContourGroupRequest& request);

    // Automatic segmentation along an upstream path.
    static xq_ExtractContoursResult ExtractContours(
        mitk::DataStorage* dataStorage,
        const xq_ExtractContoursRequest& request);
};
