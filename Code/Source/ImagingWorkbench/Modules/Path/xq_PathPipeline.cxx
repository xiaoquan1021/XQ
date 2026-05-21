#include "xq_PathPipeline.h"

#include "xq_PathPlanner.h"
#include "xq_VesselCenterline.h"
#include "xq_CenterlineSegment.h"

#include <xq_VtkParametricSpline.h>

#include <mitkImage.h>
#include <mitkNodePredicateDataType.h>
#include <mitkSurface.h>

#include <vtkPoints.h>
#include <vtkPolyData.h>

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

std::vector<mitk::Point3D> GetPointsFromCenterlineData(mitk::BaseData* data)
{
    std::vector<mitk::Point3D> points;
    if (!data)
        return points;

    if (auto* centerline = dynamic_cast<xq_VesselCenterline*>(data))
    {
        auto* segment = centerline->GetSegment();
        if (!segment)
            return points;

        points = segment->GetTracePositions();
        if (points.size() < 2)
            points = segment->GetAnchorPositions();
        return points;
    }

    auto* surface = dynamic_cast<mitk::Surface*>(data);
    vtkPolyData* polyData = surface ? surface->GetVtkPolyData() : nullptr;
    vtkPoints* vtkPts = polyData ? polyData->GetPoints() : nullptr;
    if (!vtkPts || vtkPts->GetNumberOfPoints() < 2)
        return points;

    points.reserve(static_cast<size_t>(vtkPts->GetNumberOfPoints()));
    for (vtkIdType i = 0; i < vtkPts->GetNumberOfPoints(); ++i)
    {
        double p[3] = {0.0, 0.0, 0.0};
        vtkPts->GetPoint(i, p);
        mitk::Point3D point;
        point[0] = p[0];
        point[1] = p[1];
        point[2] = p[2];
        points.push_back(point);
    }
    return points;
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
    const std::string requestedAlgorithm =
        request.algorithm.empty() ? std::string(planner->Name()) : request.algorithm;

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
    const std::string actualAlgorithm =
        planResult.actualAlgorithm.empty() ? std::string(planner->Name()) : planResult.actualAlgorithm;
    xq::pipeline::SetStringProperty(
        pathNode, xq::pipeline::kSourceImageProperty, request.imageNodeName);
    xq::pipeline::SetStringProperty(
        pathNode, xq::pipeline::kAlgorithmProperty, actualAlgorithm);
    xq::pipeline::SetStringProperty(
        pathNode, "xq.pathplanning.algorithm.requested", requestedAlgorithm);
    xq::pipeline::SetStringProperty(
        pathNode, "xq.pathplanning.algorithm.actual", actualAlgorithm);
    xq::pipeline::SetStringProperty(pathNode, "xq.path.method", actualAlgorithm);
    xq::pipeline::SetStringProperty(pathNode, "xq.params.path.method", actualAlgorithm);
    xq::pipeline::SetStringProperty(
        pathNode, "xq.params.path.smoothing", request.smoothCurve ? "spline" : "linear");
    xq::pipeline::SetStringProperty(pathNode, "xq.units.length", "mm");
    pathNode->SetIntProperty("xq.path.calculation_number", request.sampleCount);
    pathNode->SetDoubleProperty("xq.path.spacing", request.stepSize);
    pathNode->SetDoubleProperty("xq.path.point_size", 1.0);
    pathNode->SetDoubleProperty("xq.params.path.spacing", request.stepSize);
    pathNode->SetDoubleProperty("xq.params.path.point_size_2d", 1.0);
    pathNode->SetDoubleProperty("xq.params.path.point_size_3d", 1.0);
    pathNode->SetBoolProperty("xq.path.editable", true);
    pathNode->SetFloatProperty("point size", 1.0f);
    pathNode->SetIntProperty(
        "xq.path.point_count",
        segment ? std::max(segment->GetTraceVertexCount(), segment->GetAnchorCount()) : 0);
    pathNode->SetBoolProperty("xq.pathplanning.algorithm.fallback", planResult.usedFallback);
    if (!planResult.diagnostic.empty())
        xq::pipeline::SetStringProperty(
            pathNode, "xq.pathplanning.algorithm.diagnostic", planResult.diagnostic);

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

xq_PathExtractResult xq_PathPipelineService::ExtractPathFromCenterline(
    mitk::DataStorage* dataStorage,
    const xq_PathExtractRequest& request)
{
    xq_PathExtractResult result;

    if (!dataStorage)
    {
        result.diagnostics.push_back(makeError("DataStorage is null."));
        return result;
    }
    if (request.centerlineNodeName.empty())
    {
        result.diagnostics.push_back(makeError("Centerline node name is empty."));
        return result;
    }

    mitk::DataNode::Pointer centerlineNode = nullptr;
    const auto allNodes = dataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        const auto& node = it->Value();
        if (node.IsNotNull() && node->GetName() == request.centerlineNodeName)
        {
            centerlineNode = node;
            break;
        }
    }
    if (centerlineNode.IsNull())
    {
        result.diagnostics.push_back(makeError(
            "Centerline node '" + request.centerlineNodeName + "' was not found in DataStorage."));
        return result;
    }

    const auto points = GetPointsFromCenterlineData(centerlineNode->GetData());
    if (points.size() < 2)
    {
        result.diagnostics.push_back(makeError(
            "Centerline node '" + request.centerlineNodeName +
            "' does not contain at least two geometry points; no path was created."));
        return result;
    }

    const std::string imageName = request.imageNodeName.empty()
        ? xq::pipeline::GetStringProperty(
              centerlineNode.GetPointer(), xq::pipeline::kSourceImageProperty)
        : request.imageNodeName;
    const std::string modelName = xq::pipeline::GetStringProperty(
        centerlineNode.GetPointer(), xq::pipeline::kSourceModelProperty);
    if (imageName.empty())
    {
        result.diagnostics.push_back(makeWarning(
            "Centerline node has no xq.source.image metadata; extracted path will keep only xq.source.centerline."));
    }

    auto path = xq_VesselCenterline::New();
    auto* segment = new xq_CenterlineSegment();
    segment->SetSampleDensity(request.sampleCount);
    PopulateSegmentFromPath(segment, points, request.sampleCount, request.smoothCurve);
    path->SetSegment(segment);

    auto pathNode = mitk::DataNode::New();
    pathNode->SetData(path);
    pathNode->SetName(request.pathName.empty()
        ? request.centerlineNodeName + "_path"
        : request.pathName);
    pathNode->SetBoolProperty("xq.pathplanning.path", true);
    xq::pipeline::MarkNode(pathNode, xq::pipeline::Stage::Path);
    xq::pipeline::SetStringProperty(
        pathNode, xq::pipeline::kAlgorithmProperty, "extract_paths");
    xq::pipeline::SetStringProperty(
        pathNode, "xq.pathplanning.algorithm.requested", "extract_paths");
    xq::pipeline::SetStringProperty(
        pathNode, "xq.pathplanning.algorithm.actual", "extract_paths");
    xq::pipeline::SetStringProperty(pathNode, "xq.path.method", "extract_paths");
    xq::pipeline::SetStringProperty(pathNode, "xq.params.path.method", "extract_paths");
    xq::pipeline::SetStringProperty(
        pathNode, "xq.params.path.smoothing", request.smoothCurve ? "spline" : "linear");
    xq::pipeline::SetStringProperty(pathNode, "xq.units.length", "mm");
    xq::pipeline::SetStringProperty(
        pathNode, xq::pipeline::kSourceCenterlineProperty, request.centerlineNodeName);
    pathNode->SetIntProperty("xq.path.calculation_number", request.sampleCount);
    pathNode->SetDoubleProperty(
        "xq.path.spacing", segment ? segment->GetStepSize() : 0.5);
    pathNode->SetDoubleProperty("xq.path.point_size", 1.0);
    pathNode->SetDoubleProperty(
        "xq.params.path.spacing", segment ? segment->GetStepSize() : 0.5);
    pathNode->SetDoubleProperty("xq.params.path.point_size_2d", 1.0);
    pathNode->SetDoubleProperty("xq.params.path.point_size_3d", 1.0);
    pathNode->SetBoolProperty("xq.path.editable", true);
    pathNode->SetFloatProperty("point size", 1.0f);
    pathNode->SetIntProperty(
        "xq.path.point_count",
        segment ? std::max(segment->GetTraceVertexCount(), segment->GetAnchorCount()) : 0);
    if (!imageName.empty())
    {
        xq::pipeline::SetStringProperty(
            pathNode, xq::pipeline::kSourceImageProperty, imageName);
    }
    if (!modelName.empty())
    {
        xq::pipeline::SetStringProperty(
            pathNode, xq::pipeline::kSourceModelProperty, modelName);
    }

    auto pathFolder = xq::pipeline::FindCategoryFolder(
        dataStorage, xq::pipeline::Stage::Path, centerlineNode.GetPointer());
    if (pathFolder.IsNotNull())
        dataStorage->Add(pathNode, pathFolder);
    else
        dataStorage->Add(pathNode, centerlineNode);

    result.ok = true;
    result.node = pathNode;
    result.path = path;
    return result;
}
