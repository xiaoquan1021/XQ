#pragma once

// XQ Pipeline Stage 1 service: bridges xq_PathPlanner algorithms to the
// MITK DataStorage contract. Reads an Image node + seeds, produces a Path
// node (xq_VesselCenterline) with spline-smoothed trace vertices, and
// tags it with xq.source.image / xq.pipeline.algorithm.

#include <xqModulePathExports.h>

#include <xq_PipelineDataUtils.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>
#include <mitkPoint.h>

#include <string>
#include <vector>

class xq_VesselCenterline;

struct XQMODULEPATH_EXPORT xq_PathPlanRequest
{
    std::string pathName;
    std::string imageNodeName;             // REQUIRED: upstream Image node by name
    std::vector<mitk::Point3D> seeds;      // >=2
    std::string algorithm = "";            // "", "dijkstra", or "vmtk_fastmarching"
    int    sampleCount   = 200;
    double stepSize      = 0.5;
    bool   smoothCurve   = true;
    double speedExponent = 1.0;
};

struct XQMODULEPATH_EXPORT xq_PathPlanResult
    : public xq::pipeline::OperationStatus
{
    mitk::DataNode::Pointer node;
    xq_VesselCenterline* centerline = nullptr;
};

class XQMODULEPATH_EXPORT xq_PathPipelineService
{
public:
    // Read Image upstream, run planner, write Path downstream.
    static xq_PathPlanResult CreatePath(
        mitk::DataStorage* dataStorage,
        const xq_PathPlanRequest& request);
};
