#include "services/path/PathService.h"

#include "core/XQDataNode.h"
#include "core/XQDomainType.h"
#include "core/XQPathPayload.h"
#include "core/XQScene.h"
#include "core/command/XQSceneCommands.h"

#include <memory>
#include <vector>

namespace xq {
namespace {

PathService::Result failure(PathService::Status status)
{
    PathService::Result result;
    result.status = status;
    result.command = nullptr;
    return result;
}

PathService::Result success(std::unique_ptr<XQCommand> command)
{
    PathService::Result result;
    result.status = PathService::Status::Ok;
    result.command = std::move(command);
    return result;
}

// Reads the node's XQPath payload. Returns nullptr if the node is not a Path
// node carrying an XQPathPayload.
const XQPathPayload* path_payload_of(const XQDataNode& node)
{
    if (node.domainType() != XQDomainType::Path) {
        return nullptr;
    }
    return dynamic_cast<const XQPathPayload*>(node.payload().get());
}

// Resamples the path and maps the XQPath status onto a PathService status.
PathService::Status resample_into(XQPath* path, double spacing)
{
    const XQPath::ResampleStatus status = path->resample(spacing);
    switch (status) {
    case XQPath::ResampleStatus::Ok:
        return PathService::Status::Ok;
    case XQPath::ResampleStatus::NotEnoughPoints:
        return PathService::Status::NotEnoughControlPoints;
    case XQPath::ResampleStatus::InvalidSpacing:
        return PathService::Status::InvalidSpacing;
    }
    return PathService::Status::ResampleFailed;
}

// Builds the ReplacePayloadCommand for an edited control-point set. Validates
// spacing and resamples; preserves the original path id and source image node.
PathService::Result replace_with_control_points(XQScene* scene,
                                                const XQDataNode& node,
                                                const XQPath& oldPath,
                                                const std::vector<PathControlPoint>& controlPoints,
                                                double spacing)
{
    if (controlPoints.size() < 2) {
        return failure(PathService::Status::NotEnoughControlPoints);
    }
    if (spacing <= 0.0) {
        return failure(PathService::Status::InvalidSpacing);
    }

    XQPath edited = oldPath; // deep copy: preserves id, interpolation, source image node
    edited.setControlPoints(controlPoints);

    const PathService::Status resample_status = resample_into(&edited, spacing);
    if (resample_status != PathService::Status::Ok) {
        return failure(resample_status);
    }

    std::shared_ptr<XQPayload> payload = std::make_shared<XQPathPayload>(edited);
    std::unique_ptr<XQCommand> command(
        new ReplacePayloadCommand(scene, node.id(), XQDomainType::Path, payload, "Edit path"));
    return success(std::move(command));
}

} // namespace

PathService::Result PathService::createPathCommand(XQScene* scene,
                                                   const NodeId& newPathId,
                                                   const std::string& name,
                                                   const NodeId& sourceImageNodeId,
                                                   const std::vector<PathControlPoint>& controlPoints,
                                                   double spacing)
{
    if (scene == nullptr) {
        return failure(Status::NullScene);
    }
    if (controlPoints.size() < 2) {
        return failure(Status::NotEnoughControlPoints);
    }
    if (spacing <= 0.0) {
        return failure(Status::InvalidSpacing);
    }

    XQPath path;
    path.setId(newPathId);
    path.setSourceImageNode(sourceImageNodeId);
    path.setControlPoints(controlPoints);

    const Status resample_status = resample_into(&path, spacing);
    if (resample_status != Status::Ok) {
        return failure(resample_status);
    }

    std::shared_ptr<XQPayload> payload = std::make_shared<XQPathPayload>(path);
    const XQDataNode node(newPathId, XQDomainType::Path, name, payload);
    std::unique_ptr<XQCommand> command(
        new AddNodeWithSourceRelationCommand(scene, node, sourceImageNodeId, "Add path"));
    return success(std::move(command));
}

PathService::Result PathService::moveControlPointCommand(XQScene* scene,
                                                         const XQDataNode& node,
                                                         std::size_t index,
                                                         const Point3& newPosition,
                                                         double spacing)
{
    if (scene == nullptr) {
        return failure(Status::NullScene);
    }
    const XQPathPayload* payload = path_payload_of(node);
    if (payload == nullptr) {
        return failure(Status::NotAPathNode);
    }

    std::vector<PathControlPoint> points = payload->path().controlPoints();
    if (index >= points.size()) {
        return failure(Status::IndexOutOfRange);
    }
    points[index].position = newPosition;

    return replace_with_control_points(scene, node, payload->path(), points, spacing);
}

PathService::Result PathService::insertControlPointCommand(XQScene* scene,
                                                           const XQDataNode& node,
                                                           std::size_t index,
                                                           const Point3& point,
                                                           double spacing)
{
    if (scene == nullptr) {
        return failure(Status::NullScene);
    }
    const XQPathPayload* payload = path_payload_of(node);
    if (payload == nullptr) {
        return failure(Status::NotAPathNode);
    }

    std::vector<PathControlPoint> points = payload->path().controlPoints();
    if (index > points.size()) { // insert index may equal the point count (append)
        return failure(Status::IndexOutOfRange);
    }
    PathControlPoint inserted;
    inserted.position = point;
    points.insert(points.begin() + static_cast<std::ptrdiff_t>(index), inserted);

    return replace_with_control_points(scene, node, payload->path(), points, spacing);
}

PathService::Result PathService::deleteControlPointCommand(XQScene* scene,
                                                           const XQDataNode& node,
                                                           std::size_t index,
                                                           double spacing)
{
    if (scene == nullptr) {
        return failure(Status::NullScene);
    }
    const XQPathPayload* payload = path_payload_of(node);
    if (payload == nullptr) {
        return failure(Status::NotAPathNode);
    }

    std::vector<PathControlPoint> points = payload->path().controlPoints();
    if (index >= points.size()) {
        return failure(Status::IndexOutOfRange);
    }
    if (points.size() <= 2) { // deleting would leave fewer than 2 control points
        return failure(Status::NotEnoughControlPoints);
    }
    points.erase(points.begin() + static_cast<std::ptrdiff_t>(index));

    return replace_with_control_points(scene, node, payload->path(), points, spacing);
}

PathService::Result PathService::resamplePathCommand(XQScene* scene,
                                                     const XQDataNode& node,
                                                     double spacing)
{
    if (scene == nullptr) {
        return failure(Status::NullScene);
    }
    const XQPathPayload* payload = path_payload_of(node);
    if (payload == nullptr) {
        return failure(Status::NotAPathNode);
    }
    if (spacing <= 0.0) {
        return failure(Status::InvalidSpacing);
    }

    XQPath edited = payload->path(); // deep copy: preserves id + source image node
    const Status resample_status = resample_into(&edited, spacing);
    if (resample_status != Status::Ok) {
        return failure(resample_status);
    }

    std::shared_ptr<XQPayload> new_payload = std::make_shared<XQPathPayload>(edited);
    std::unique_ptr<XQCommand> command(
        new ReplacePayloadCommand(scene, node.id(), XQDomainType::Path, new_payload, "Resample path"));
    return success(std::move(command));
}

} // namespace xq
