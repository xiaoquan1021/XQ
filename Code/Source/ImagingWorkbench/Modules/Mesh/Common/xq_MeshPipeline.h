#pragma once

#include <xqMeshCommonExports.h>

#include <xq_PipelineDataUtils.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <array>
#include <map>
#include <string>
#include <vector>

class xq_MitkGrid;

struct XQMESHCOMMON_EXPORT xq_RefinementRegion
{
    enum class Type { Sphere, Box, Cylinder };
    Type type = Type::Sphere;
    std::array<double, 3> center = {0, 0, 0};
    std::array<double, 3> radiusOrSize = {1, 1, 1};
    double edgeSize = 0.5;
};

struct XQMESHCOMMON_EXPORT xq_MeshGenerationRequest
{
    std::string meshName;
    double globalEdgeSize = 1.0;
    double boundaryLayerFirstHeight = 0.1;
    int boundaryLayerLayers = 0;
    double boundaryLayerGrowthRate = 1.2;
    std::map<int, double> localFaceSizes;
    std::vector<xq_RefinementRegion> refinementRegions;
    bool preserveSurface = true;
    bool optimize = true;
    double minDihedral = 10.0;
    double maxEdgeSize = 0.0;
};

struct XQMESHCOMMON_EXPORT xq_MeshGenerationResult
    : public xq::pipeline::OperationStatus
{
    mitk::DataNode::Pointer node;
    xq_MitkGrid* gridData = nullptr;
};

class XQMESHCOMMON_EXPORT xq_MeshPipelineService
{
public:
    static xq_MeshGenerationResult CreateVolumeMesh(
        mitk::DataStorage* dataStorage,
        const mitk::DataNode::Pointer& modelNode,
        const xq_MeshGenerationRequest& request);
};
