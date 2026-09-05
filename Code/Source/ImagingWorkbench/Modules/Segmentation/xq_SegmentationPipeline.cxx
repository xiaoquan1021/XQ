#include "xq_SegmentationPipeline.h"

#include "xq_ProfileGroup.h"
#include "xq_ContourGroup.h"
#include "xq_MLSegmentation.h"
#include "xq_SegmentationAlgorithm.h"
#include "xq_SegmentationUtils.h"

#include <xq_VesselCenterline.h>
#include <xq_CenterlineSegment.h>

#include <mitkImage.h>
#include <mitkImageCast.h>

#include <vtkImageData.h>
#include <vtkImageReslice.h>
#include <vtkMatrix4x4.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

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

mitk::DataNode::Pointer findNodeByNameAndDataType(
    mitk::DataStorage* ds, const std::string& name, const char* className)
{
    const auto all = ds->GetAll();
    for (auto it = all->Begin(); it != all->End(); ++it)
    {
        const auto& n = it->Value();
        if (n.IsNotNull() && n->GetName() == name && n->GetData() &&
            std::string_view(n->GetData()->GetNameOfClass()) == className)
            return n;
    }
    return nullptr;
}

// Build a 4x4 reslice axes matrix whose:
//   columns 0,1 = in-plane x, y axes
//   column  2   = slice normal (= path tangent)
//   column  3   = slice origin (centered on the trace vertex)
vtkSmartPointer<vtkMatrix4x4> BuildResliceAxes(
    const xq_CenterlineSegment::TraceVertex& v, double sizeMm)
{
    auto axes = vtkSmartPointer<vtkMatrix4x4>::New();
    axes->Identity();
    // In-plane x = rotation (persisted xhat from spline frame).
    axes->SetElement(0, 0, v.rotation[0]);
    axes->SetElement(1, 0, v.rotation[1]);
    axes->SetElement(2, 0, v.rotation[2]);
    // In-plane y = normal (= tangent x rotation, maintained by spline).
    axes->SetElement(0, 1, v.normal[0]);
    axes->SetElement(1, 1, v.normal[1]);
    axes->SetElement(2, 1, v.normal[2]);
    // Normal = tangent.
    axes->SetElement(0, 2, v.tangent[0]);
    axes->SetElement(1, 2, v.tangent[1]);
    axes->SetElement(2, 2, v.tangent[2]);
    // Origin: shift by half a slice in local -x/-y so the resliced image
    // is centered at the trace vertex.
    const double shift = -0.5 * sizeMm;
    const double ox = v.pos[0] + shift * v.rotation[0] + shift * v.normal[0];
    const double oy = v.pos[1] + shift * v.rotation[1] + shift * v.normal[1];
    const double oz = v.pos[2] + shift * v.rotation[2] + shift * v.normal[2];
    axes->SetElement(0, 3, ox);
    axes->SetElement(1, 3, oy);
    axes->SetElement(2, 3, oz);
    return axes;
}

// Lift slice-local polygon (x in column 0, y in column 1, z=0) to world
// coordinates using the same reslice-axes frame.
std::vector<mitk::Point3D> LiftSlicePolygonToWorld(
    vtkPolyData* polygon,
    const xq_CenterlineSegment::TraceVertex& v,
    double sizeMm)
{
    std::vector<mitk::Point3D> out;
    if (!polygon || !polygon->GetPoints())
        return out;

    const double shift = -0.5 * sizeMm;
    auto* pts = polygon->GetPoints();
    for (vtkIdType i = 0; i < pts->GetNumberOfPoints(); ++i)
    {
        double p[3];
        pts->GetPoint(i, p);
        const double lx = p[0] + shift;
        const double ly = p[1] + shift;
        mitk::Point3D w;
        w[0] = v.pos[0] + lx * v.rotation[0] + ly * v.normal[0];
        w[1] = v.pos[1] + lx * v.rotation[1] + ly * v.normal[1];
        w[2] = v.pos[2] + lx * v.rotation[2] + ly * v.normal[2];
        out.push_back(w);
    }
    return out;
}

} // namespace

xq_CreateContourGroupResult xq_SegmentationPipelineService::CreateContourGroup(
    mitk::DataStorage* dataStorage,
    const xq_CreateContourGroupRequest& request)
{
    xq_CreateContourGroupResult result;

    if (!dataStorage)
    {
        result.diagnostics.push_back(makeError("DataStorage is null."));
        return result;
    }

    if (request.groupName.empty())
    {
        result.diagnostics.push_back(makeError("Contour group name is empty."));
        return result;
    }

    auto profileGroup = xq_ProfileGroup::New();
    auto groupNode = mitk::DataNode::New();
    groupNode->SetData(profileGroup);
    groupNode->SetName(request.groupName);
    groupNode->SetColor(0.0f, 1.0f, 0.0f);
    groupNode->SetBoolProperty("xq.segmentation.profile_group", true);
    xq::pipeline::MarkNode(groupNode, xq::pipeline::Stage::ContourGroup);
    xq::pipeline::SetStringProperty(
        groupNode, xq::pipeline::kAlgorithmProperty, "manual");
    xq::pipeline::SetStringProperty(groupNode, "xq.segmentation.method", "manual");
    xq::pipeline::SetStringProperty(groupNode, "xq.params.segmentation2d.method", "manual");
    xq::pipeline::SetStringProperty(groupNode, "xq.units.length", "mm");
    groupNode->SetDoubleProperty("xq.segmentation.reslice_size", 12.0);
    groupNode->SetDoubleProperty("xq.params.segmentation2d.slice_spacing", 12.0);
    groupNode->SetDoubleProperty("xq.params.segmentation2d.point_size_2d", 1.0);
    groupNode->SetDoubleProperty("xq.params.segmentation2d.point_size_3d", 1.0);

    mitk::DataNode::Pointer parentNode = nullptr;
    if (!request.pathName.empty())
    {
        profileGroup->SetAttribute("path_name", request.pathName);
        xq::pipeline::SetStringProperty(
            groupNode, xq::pipeline::kSourcePathProperty, request.pathName);
        parentNode = xq::pipeline::FindNodeByNameAndStage(
            dataStorage, request.pathName, xq::pipeline::Stage::Path);
        if (parentNode.IsNotNull())
        {
            const auto sourceImage = xq::pipeline::GetStringProperty(
                parentNode.GetPointer(), xq::pipeline::kSourceImageProperty);
            if (!sourceImage.empty())
            {
                xq::pipeline::SetStringProperty(
                    groupNode, xq::pipeline::kSourceImageProperty, sourceImage);
            }
        }
        if (parentNode.IsNull())
        {
            result.diagnostics.push_back(makeWarning(
                "Selected path node was not found in DataStorage. The contour group was added at root level."));
        }
    }

    // Prefer the Segmentations category folder, which is what users see
    // in the Data Manager tree. Fall back to the upstream path node when
    // no project folder exists.
    auto segFolder = xq::pipeline::FindCategoryFolder(
        dataStorage, xq::pipeline::Stage::ContourGroup, parentNode.GetPointer());
    if (segFolder.IsNotNull())
        dataStorage->Add(groupNode, segFolder);
    else if (parentNode.IsNotNull())
        dataStorage->Add(groupNode, parentNode);
    else
        dataStorage->Add(groupNode);

    result.ok = true;
    result.node = groupNode;
    result.profileGroup = profileGroup;

    auto report = xq_SegmentationUtils::BuildReadinessReport(profileGroup.GetPointer());
    groupNode->SetBoolProperty("xq.contour.ready", report.loftReady && report.modelingReady);
    groupNode->SetIntProperty("xq.contour.profile_count", report.profileCount);
    groupNode->SetIntProperty("xq.contour.missing_count", report.missingCount);
    groupNode->SetIntProperty("xq.contour.warning_count", static_cast<int>(report.warnings.size()));
    groupNode->SetIntProperty("xq.contour.error_count", static_cast<int>(report.errors.size()));

    return result;
}

xq_ExtractContoursResult xq_SegmentationPipelineService::ExtractContours(
    mitk::DataStorage* dataStorage,
    const xq_ExtractContoursRequest& request)
{
    xq_ExtractContoursResult result;

    if (!dataStorage)
    {
        result.diagnostics.push_back(makeError("DataStorage is null."));
        return result;
    }
    if (request.groupName.empty() || request.pathName.empty() ||
        request.imageNodeName.empty())
    {
        result.diagnostics.push_back(makeError(
            "ExtractContours requires groupName, pathName and imageNodeName."));
        return result;
    }

    // DataStorage reads.
    auto imageNode = findNodeByNameAndDataType(
        dataStorage, request.imageNodeName, "Image");
    if (imageNode.IsNull())
    {
        result.diagnostics.push_back(makeError(
            "Image node '" + request.imageNodeName + "' not found."));
        return result;
    }
    auto* image = dynamic_cast<mitk::Image*>(imageNode->GetData());
    vtkImageData* vtkImg = image ? image->GetVtkImageData() : nullptr;
    if (!vtkImg)
    {
        result.diagnostics.push_back(makeError("Image node has no vtkImageData."));
        return result;
    }

    auto pathNode = xq::pipeline::FindNodeByNameAndStage(
        dataStorage, request.pathName, xq::pipeline::Stage::Path);
    if (pathNode.IsNull())
    {
        result.diagnostics.push_back(makeError(
            "Path node '" + request.pathName + "' not found (need Stage::Path)."));
        return result;
    }
    auto* centerline = dynamic_cast<xq_VesselCenterline*>(pathNode->GetData());
    auto* segment = centerline ? centerline->GetSegment() : nullptr;
    if (!segment)
    {
        result.diagnostics.push_back(makeError("Path node has no centerline segment."));
        return result;
    }
    if (segment->GetTraceVertexCount() == 0 && segment->GetAnchorCount() >= 2)
        segment->Interpolate();
    const auto traceVertices = segment->GetTraceVertices();
    if (traceVertices.empty())
    {
        result.diagnostics.push_back(makeError(
            "Path has no trace vertices; add anchor points first."));
        return result;
    }

    auto algorithm = CreateSegmentationAlgorithm(request.algorithm);
    if (std::string(algorithm->Name()) == "ml" && !xq_MLSegmentation::IsBackendAvailable())
    {
        result.diagnostics.push_back(makeError(xq_MLSegmentation::GetBackendDiagnostic()));
        return result;
    }

    // Output ContourGroup.
    auto contourGroup = xq_ContourGroup::New();
    contourGroup->SetPathName(request.pathName);

    const int stride = std::max(1, request.strideAlongPath);
    const int nPixels = std::max(16,
        static_cast<int>(std::round(request.sliceSizeMm / std::max(1e-3, request.pixelSpacing))));

    int extracted = 0;
    for (size_t i = 0; i < traceVertices.size(); i += static_cast<size_t>(stride))
    {
        const auto& v = traceVertices[i];

        // vtkImageReslice to sample the normal plane.
        auto reslice = vtkSmartPointer<vtkImageReslice>::New();
        reslice->SetInputData(vtkImg);
        reslice->SetResliceAxes(BuildResliceAxes(v, request.sliceSizeMm));
        reslice->SetOutputDimensionality(2);
        reslice->SetOutputSpacing(request.pixelSpacing, request.pixelSpacing, 1.0);
        reslice->SetOutputOrigin(0.0, 0.0, 0.0);
        reslice->SetOutputExtent(0, nPixels - 1, 0, nPixels - 1, 0, 0);
        reslice->SetInterpolationModeToLinear();
        reslice->Update();

        xq_SegmentationAlgorithm::SliceInput sliceIn;
        sliceIn.slice = reslice->GetOutput();
        sliceIn.seed[0] = 0.5 * nPixels * request.pixelSpacing;
        sliceIn.seed[1] = 0.5 * nPixels * request.pixelSpacing;
        sliceIn.pixelSpacing[0] = request.pixelSpacing;
        sliceIn.pixelSpacing[1] = request.pixelSpacing;

        xq_SegmentationAlgorithm::Params params;
        params.threshold = request.threshold;
        params.outputPointCount = request.outputPoints;

        const auto contour = algorithm->Extract(sliceIn, params);
        if (!contour.ok)
        {
            result.diagnostics.push_back(makeWarning(
                "Slice " + std::to_string(i) + ": " + contour.diagnostic));
            continue;
        }

        ContourSlice slice;
        slice.slicePosition = static_cast<double>(v.id);
        slice.isClosed = true;
        slice.method = std::string(algorithm->Name());
        slice.points = LiftSlicePolygonToWorld(
            contour.polygon, v, request.sliceSizeMm);
        if (slice.points.size() < 3)
            continue;
        contourGroup->AddContour(slice);
        ++extracted;
    }

    if (extracted == 0)
    {
        result.diagnostics.push_back(makeError(
            "No valid contours were extracted along the path."));
        return result;
    }

    auto node = mitk::DataNode::New();
    node->SetData(contourGroup);
    node->SetName(request.groupName);
    node->SetColor(0.0f, 0.9f, 0.2f);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::ContourGroup);
    xq::pipeline::SetStringProperty(
        node, xq::pipeline::kSourcePathProperty, request.pathName);
    xq::pipeline::SetStringProperty(
        node, xq::pipeline::kSourceImageProperty, request.imageNodeName);
    xq::pipeline::SetStringProperty(
        node, xq::pipeline::kAlgorithmProperty, std::string(algorithm->Name()));
    xq::pipeline::SetStringProperty(
        node, "xq.segmentation.method", std::string(algorithm->Name()));
    xq::pipeline::SetStringProperty(
        node, "xq.params.segmentation2d.method", std::string(algorithm->Name()));
    xq::pipeline::SetStringProperty(node, "xq.units.length", "mm");
    node->SetDoubleProperty("xq.segmentation.reslice_size", request.sliceSizeMm);
    node->SetDoubleProperty("xq.segmentation.threshold.min", request.threshold);
    node->SetDoubleProperty("xq.segmentation.threshold.max", request.threshold);
    node->SetIntProperty("xq.segmentation.interval", stride);
    node->SetDoubleProperty("xq.params.segmentation2d.slice_spacing", request.sliceSizeMm);
    node->SetDoubleProperty("xq.params.segmentation2d.pixel_spacing", request.pixelSpacing);
    node->SetDoubleProperty("xq.params.segmentation2d.threshold", request.threshold);
    node->SetDoubleProperty("xq.params.segmentation2d.point_size_2d", 1.0);
    node->SetDoubleProperty("xq.params.segmentation2d.point_size_3d", 1.0);

    auto segFolder2 = xq::pipeline::FindCategoryFolder(
        dataStorage, xq::pipeline::Stage::ContourGroup, pathNode.GetPointer());
    if (segFolder2.IsNotNull())
        dataStorage->Add(node, segFolder2);
    else
        dataStorage->Add(node, pathNode);

    result.ok = true;
    result.node = node;
    result.contoursExtracted = extracted;
    return result;
}

xq_ExtractMultiPathContoursResult xq_SegmentationPipelineService::ExtractContoursForPaths(
    mitk::DataStorage* dataStorage,
    const xq_ExtractMultiPathContoursRequest& request)
{
    xq_ExtractMultiPathContoursResult result;

    if (!dataStorage)
    {
        result.diagnostics.push_back(makeError("DataStorage is null."));
        return result;
    }
    if (request.pathNames.empty() || request.imageNodeName.empty())
    {
        result.diagnostics.push_back(makeError(
            "ExtractContoursForPaths requires at least one path and an image node name."));
        return result;
    }
    if (request.algorithm == "ml" && !xq_MLSegmentation::IsBackendAvailable())
    {
        result.diagnostics.push_back(makeError(xq_MLSegmentation::GetBackendDiagnostic()));
        return result;
    }

    const std::string allPaths = xq::pipeline::JoinSourceList(request.pathNames);
    const std::string prefix =
        request.groupNamePrefix.empty() ? std::string("multipath_contours") : request.groupNamePrefix;

    int pathIndex = 0;
    for (const auto& pathName : request.pathNames)
    {
        if (pathName.empty())
        {
            result.diagnostics.push_back(makeWarning("Skipped an empty path name."));
            continue;
        }

        xq_ExtractContoursRequest single;
        single.groupName = prefix + "_" + std::to_string(pathIndex);
        single.pathName = pathName;
        single.imageNodeName = request.imageNodeName;
        single.algorithm = request.algorithm;
        single.threshold = request.threshold;
        single.strideAlongPath = request.strideAlongPath;
        single.sliceSizeMm = request.sliceSizeMm;
        single.pixelSpacing = request.pixelSpacing;
        single.outputPoints = request.outputPoints;

        auto singleResult = ExtractContours(dataStorage, single);
        for (const auto& diagnostic : singleResult.diagnostics)
            result.diagnostics.push_back(diagnostic);

        if (singleResult.ok && singleResult.node.IsNotNull())
        {
            xq::pipeline::SetStringProperty(
                singleResult.node, "xq.source.paths", allPaths);
            xq::pipeline::SetStringProperty(
                singleResult.node, "xq.segmentation.method", "multipath");
            xq::pipeline::SetStringProperty(
                singleResult.node, "xq.params.segmentation2d.method", "multipath");
            xq::pipeline::SetStringProperty(
                singleResult.node, xq::pipeline::kAlgorithmProperty,
                "multipath_" + request.algorithm);
            singleResult.node->SetIntProperty(
                "xq.segmentation.interval", std::max(1, request.strideAlongPath));
            result.nodes.push_back(singleResult.node);
            result.contoursExtracted += singleResult.contoursExtracted;
        }
        ++pathIndex;
    }

    if (result.nodes.empty())
    {
        result.diagnostics.push_back(makeError(
            "No contour groups were extracted for the requested paths."));
        return result;
    }

    result.ok = true;
    return result;
}
