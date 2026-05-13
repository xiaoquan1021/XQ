#include "xq_PathPipeline.h"

#include "xq_PathPlanner.h"
#include "xq_VesselCenterline.h"
#include "xq_CenterlineSegment.h"

#include <xq_VtkParametricSpline.h>

#include <mitkImage.h>
#include <mitkNodePredicateDataType.h>

#include <utility>

namespace
{

auto makeError(std::string message) -> xq::pipeline::Diagnostic
{
    return {xq::pipeline::Severity::Error, std::move(message)};
}

auto makeWarning(std::string message) -> xq::pipeline::Diagnostic
{
    return {xq::pipeline::Severity::Warning, std::move(message)};
}

std::unique_ptr<xq_PathPlanner> selectPlanner(const std::string& algoKey)
{
    if (algoKey == "dijkstra")
        return std::make_unique<xq_DijkstraPathPlanner>();
    if (algoKey == "vmtk_fastmarching")
        return std::make_unique<xq_VmtkFastMarchingPathPlanner>();
    return CreateDefaultPathPlanner();
}

// XQ Design: unify smoothing + Frenet frame computation in one place so all
// downstream stages (segmentation, modeling) see the same parametrization.
void PopulateSegmentFromPath(
    xq_CenterlineSegment* segment,
    const std::vector<mitk::Point3D>& rawPoints,
    int resolution,
    bool smoothCurve)
{
    if (!segment || rawPoints.size() < 2)
        return;

    segment->ReplaceAnchors(rawPoints, false);

    if (!smoothCurve)
    {
        segment->Interpolate();
        return;
    }

    xq_VtkParametricSpline spline;
    spline.SetControlVertices(rawPoints);
    spline.SetResolution(std::max(2, resolution));
    spline.SetPeriodicBoundary(false);

    const auto frames = spline.GetSplineFramePoints();
    if (frames.empty())
    {
        segment->Interpolate();
        return;
    }

    std::vector<xq_CenterlineSegment::TraceVertex> vertices;
    vertices.reserve(frames.size());
    int id = 0;
    for (const auto& f : frames)
    {
        xq_CenterlineSegment::TraceVertex v;
        v.id       = id++;
        v.pos      = f.position;
        v.tangent  = f.tangent;
        v.normal   = f.normal;
        v.rotation = f.rotation;
        vertices.push_back(v);
    }
    segment->SetTraceVertices(vertices);
}

} // namespace

xq_PathPlanResult xq_PathPipelineService::CreatePath(
    mitk::DataStorage* dataStorage,
    const xq_PathPlanRequest& request)
{
    xq_PathPlanResult result;

    if (!dataStorage)
    {
        result.diagnostics.push_back(makeError("DataStorage is null."));
        return result;
    }
    if (request.pathName.empty())
    {
        result.diagnostics.push_back(makeError("Path name is empty."));
        return result;
    }
    if (request.imageNodeName.empty())
    {
        result.diagnostics.push_back(makeError(
            "Path planning requires an upstream Image node name."));
        return result;
    }
    if (request.seeds.size() < 2)
    {
        result.diagnostics.push_back(makeError(
            "Path planning requires at least two seed points (start and end)."));
        return result;
    }

    // DataStorage read: locate the upstream Image node by name.
    mitk::DataNode::Pointer imageNode = nullptr;
    const auto allNodes = dataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        const auto& n = it->Value();
        if (n.IsNotNull() && n->GetName() == request.imageNodeName &&
            dynamic_cast<mitk::Image*>(n->GetData()))
        {
            imageNode = n;
            break;
        }
    }
    if (imageNode.IsNull())
    {
        result.diagnostics.push_back(makeError(
            "Upstream Image node '" + request.imageNodeName + "' was not found in DataStorage."));
        return result;
    }
    auto* image = dynamic_cast<mitk::Image*>(imageNode->GetData());

    // Run the planning algorithm.
    auto planner = selectPlanner(request.algorithm);

    xq_PathPlanner::Request pReq;
    pReq.image = image;
    pReq.seeds = request.seeds;
    pReq.resolution    = request.sampleCount;
    pReq.stepSize      = request.stepSize;
    pReq.smoothCurve   = request.smoothCurve;
    pReq.speedExponent = request.speedExponent;

    const auto planResult = planner->Plan(pReq);
    if (!planResult.ok)
    {
        result.diagnostics.push_back(makeError(
            planResult.diagnostic.empty()
                ? "Path planning algorithm failed."
                : planResult.diagnostic));
        return result;
    }
    if (!planResult.diagnostic.empty())
        result.diagnostics.push_back(makeWarning(planResult.diagnostic));

    // DataStorage write: create Path node and mark it.
    auto centerline = xq_VesselCenterline::New();
    auto* segment = new xq_CenterlineSegment();
    segment->SetSampleDensity(request.sampleCount);
    PopulateSegmentFromPath(segment, planResult.points, request.sampleCount, request.smoothCurve);
    centerline->SetSegment(segment);

    auto pathNode = mitk::DataNode::New();
    pathNode->SetData(centerline);
    pathNode->SetName(request.pathName);
    pathNode->SetBoolProperty("xq.pathplanning.path", true);
    xq::pipeline::MarkNode(pathNode, xq::pipeline::Stage::Path);
    xq::pipeline::SetStringProperty(
        pathNode, xq::pipeline::kSourceImageProperty, request.imageNodeName);
    xq::pipeline::SetStringProperty(
        pathNode, xq::pipeline::kAlgorithmProperty, std::string(planner->Name()));

    // Attach under the Paths category folder if the project has one;
    // otherwise fall back to hanging under the source Image node so the
    // pipeline still works outside a full project.
    auto pathFolder = xq::pipeline::FindCategoryFolder(
        dataStorage, xq::pipeline::Stage::Path, imageNode.GetPointer());
    if (pathFolder.IsNotNull())
        dataStorage->Add(pathNode, pathFolder);
    else
        dataStorage->Add(pathNode, imageNode);

    result.ok         = true;
    result.node       = pathNode;
    result.centerline = centerline;
    return result;
}
