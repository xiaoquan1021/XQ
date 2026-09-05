#include "services/path/CenterlineBGraph.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

namespace xq {
namespace {

constexpr std::size_t kNoNode = (std::numeric_limits<std::size_t>::max)();
constexpr double kDistanceTieTolerance = 1e-12;

CenterlineBGraphResult failure(CenterlineBGraphStatus status)
{
    CenterlineBGraphResult result;
    result.status = status;
    return result;
}

double directionDeterminant(const double direction[3][3])
{
    return direction[0][0]
            * (direction[1][1] * direction[2][2]
               - direction[1][2] * direction[2][1])
        - direction[0][1]
            * (direction[1][0] * direction[2][2]
               - direction[1][2] * direction[2][0])
        + direction[0][2]
            * (direction[1][0] * direction[2][1]
               - direction[1][1] * direction[2][0]);
}

bool validGeometry(const ImageGeometry& geometry)
{
    if (geometry.coordinateSystem != ImageCoordinateSystem::LPS) {
        return false;
    }
    for (int axis = 0; axis < 3; ++axis) {
        if (geometry.dimensions[axis] <= 0
            || !std::isfinite(geometry.spacing[axis])
            || !(geometry.spacing[axis] > 0.0)
            || !std::isfinite(geometry.origin[axis])) {
            return false;
        }
        for (int column = 0; column < 3; ++column) {
            if (!std::isfinite(geometry.direction[axis][column])) {
                return false;
            }
        }
    }
    const double determinant = directionDeterminant(geometry.direction);
    return std::isfinite(determinant) && std::abs(determinant) > 1e-12;
}

bool validSkeletonStructure(const CenterlineSkeletonV1& skeleton,
                            std::size_t* observedSkeletonCount)
{
    if (observedSkeletonCount == nullptr
        || skeleton.contractVersion != CenterlineSkeletonV1::ContractVersion
        || !validGeometry(skeleton.geometry)
        || skeleton.inputForegroundVoxelCount == 0
        || skeleton.skeletonVoxelCount > skeleton.inputForegroundVoxelCount
        || skeleton.thinningBackendId.empty()
        || skeleton.thinningBackendVersion.empty()
        || skeleton.distanceBackendId.empty()
        || skeleton.distanceBackendVersion.empty()) {
        return false;
    }

    std::size_t voxelCount = 1;
    for (int axis = 0; axis < 3; ++axis) {
        const std::size_t extent =
            static_cast<std::size_t>(skeleton.geometry.dimensions[axis]);
        if (voxelCount > (std::numeric_limits<std::size_t>::max)() / extent) {
            return false;
        }
        voxelCount *= extent;
    }
    if (skeleton.skeleton.size() != voxelCount
        || skeleton.radiusMm.size() != voxelCount) {
        return false;
    }

    std::size_t observed = 0;
    for (std::uint8_t value : skeleton.skeleton) {
        if (value > 1) {
            return false;
        }
        observed += value != 0 ? 1u : 0u;
    }
    if (observed != skeleton.skeletonVoxelCount) {
        return false;
    }
    *observedSkeletonCount = observed;
    return true;
}

Point3 indexToPhysical(const ImageGeometry& geometry,
                       const std::array<int, 3>& index)
{
    const double scaled[3] = {
        geometry.spacing[0] * static_cast<double>(index[0]),
        geometry.spacing[1] * static_cast<double>(index[1]),
        geometry.spacing[2] * static_cast<double>(index[2])
    };
    Point3 point{geometry.origin[0], geometry.origin[1], geometry.origin[2]};
    double* values[3] = {&point.x, &point.y, &point.z};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            *values[row] += geometry.direction[row][column] * scaled[column];
        }
    }
    return point;
}

std::array<int, 3> flatToIndex(std::size_t flat,
                               const ImageGeometry& geometry)
{
    const std::size_t xSize =
        static_cast<std::size_t>(geometry.dimensions[0]);
    const std::size_t ySize =
        static_cast<std::size_t>(geometry.dimensions[1]);
    std::array<int, 3> index{};
    index[0] = static_cast<int>(flat % xSize);
    flat /= xSize;
    index[1] = static_cast<int>(flat % ySize);
    index[2] = static_cast<int>(flat / ySize);
    return index;
}

std::size_t indexToFlat(int x, int y, int z, const ImageGeometry& geometry)
{
    return static_cast<std::size_t>(x)
        + static_cast<std::size_t>(geometry.dimensions[0])
            * (static_cast<std::size_t>(y)
               + static_cast<std::size_t>(geometry.dimensions[1])
                   * static_cast<std::size_t>(z));
}

std::size_t activeDegree(const std::vector<CenterlineBGraphNode>& nodes,
                         const std::vector<bool>& active,
                         std::size_t node)
{
    std::size_t degree = 0;
    for (std::size_t neighbor : nodes[node].neighbors) {
        degree += active[neighbor] ? 1u : 0u;
    }
    return degree;
}

std::vector<std::size_t> activeNeighbors(
    const std::vector<CenterlineBGraphNode>& nodes,
    const std::vector<bool>& active,
    std::size_t node,
    std::size_t excluded)
{
    std::vector<std::size_t> neighbors;
    for (std::size_t neighbor : nodes[node].neighbors) {
        if (active[neighbor] && neighbor != excluded) {
            neighbors.push_back(neighbor);
        }
    }
    return neighbors;
}

void countTopology(const std::vector<CenterlineBGraphNode>& nodes,
                   const std::vector<bool>& active,
                   std::size_t* nodeCount,
                   std::vector<std::size_t>* endpoints,
                   std::size_t* junctionCount)
{
    *nodeCount = 0;
    endpoints->clear();
    *junctionCount = 0;
    for (std::size_t node = 0; node < nodes.size(); ++node) {
        if (!active[node]) {
            continue;
        }
        ++(*nodeCount);
        const std::size_t degree = activeDegree(nodes, active, node);
        if (degree == 1) {
            endpoints->push_back(node);
        } else if (degree >= 3) {
            ++(*junctionCount);
        }
    }
}

bool connected(const std::vector<CenterlineBGraphNode>& nodes,
               const std::vector<bool>& active,
               std::size_t expectedCount)
{
    if (expectedCount == 0) {
        return false;
    }
    std::size_t first = kNoNode;
    for (std::size_t node = 0; node < active.size(); ++node) {
        if (active[node]) {
            first = node;
            break;
        }
    }
    if (first == kNoNode) {
        return false;
    }

    std::vector<bool> visited(nodes.size(), false);
    std::vector<std::size_t> pending(1, first);
    visited[first] = true;
    std::size_t count = 0;
    while (!pending.empty()) {
        const std::size_t node = pending.back();
        pending.pop_back();
        ++count;
        for (std::size_t neighbor : nodes[node].neighbors) {
            if (active[neighbor] && !visited[neighbor]) {
                visited[neighbor] = true;
                pending.push_back(neighbor);
            }
        }
    }
    return count == expectedCount;
}

void pruneShortSpurs(const std::vector<CenterlineBGraphNode>& nodes,
                     double thresholdMm,
                     std::vector<bool>* active)
{
    if (!(thresholdMm > 0.0)) {
        return;
    }

    while (true) {
        std::size_t activeCount = 0;
        std::size_t junctionCount = 0;
        std::vector<std::size_t> endpoints;
        countTopology(nodes, *active, &activeCount, &endpoints, &junctionCount);
        (void)activeCount;
        (void)junctionCount;

        std::vector<bool> remove(nodes.size(), false);
        for (std::size_t endpoint : endpoints) {
            std::vector<std::size_t> chain(1, endpoint);
            std::size_t previous = kNoNode;
            std::size_t current = endpoint;
            double lengthMm = 0.0;

            while (true) {
                const std::vector<std::size_t> next =
                    activeNeighbors(nodes, *active, current, previous);
                if (next.size() != 1) {
                    break;
                }
                const std::size_t successor = next.front();
                lengthMm += distance(
                    nodes[current].positionMm, nodes[successor].positionMm);
                previous = current;
                current = successor;

                const std::size_t degree = activeDegree(nodes, *active, current);
                if (degree == 2) {
                    chain.push_back(current);
                    continue;
                }
                if (degree >= 3
                    && lengthMm + kDistanceTieTolerance < thresholdMm) {
                    for (std::size_t branchNode : chain) {
                        remove[branchNode] = true;
                    }
                }
                break;
            }
        }

        bool changed = false;
        for (std::size_t node = 0; node < remove.size(); ++node) {
            if (remove[node] && (*active)[node]) {
                (*active)[node] = false;
                changed = true;
            }
        }
        if (!changed) {
            return;
        }
    }
}

struct ShortestPaths {
    std::vector<double> distanceMm;
    std::vector<std::size_t> predecessor;
};

ShortestPaths dijkstra(const std::vector<CenterlineBGraphNode>& nodes,
                       const std::vector<bool>& active,
                       std::size_t source)
{
    const double infinity = (std::numeric_limits<double>::infinity)();
    ShortestPaths result;
    result.distanceMm.assign(nodes.size(), infinity);
    result.predecessor.assign(nodes.size(), kNoNode);
    using QueueValue = std::pair<double, std::size_t>;
    std::priority_queue<QueueValue,
                        std::vector<QueueValue>,
                        std::greater<QueueValue>> pending;
    result.distanceMm[source] = 0.0;
    pending.push(std::make_pair(0.0, source));

    while (!pending.empty()) {
        const double queuedDistance = pending.top().first;
        const std::size_t node = pending.top().second;
        pending.pop();
        if (queuedDistance > result.distanceMm[node] + kDistanceTieTolerance) {
            continue;
        }
        for (std::size_t neighbor : nodes[node].neighbors) {
            if (!active[neighbor]) {
                continue;
            }
            const double candidate = result.distanceMm[node]
                + distance(nodes[node].positionMm, nodes[neighbor].positionMm);
            const bool shorter =
                candidate + kDistanceTieTolerance < result.distanceMm[neighbor];
            const bool tied =
                std::abs(candidate - result.distanceMm[neighbor])
                    <= kDistanceTieTolerance
                && (result.predecessor[neighbor] == kNoNode
                    || nodes[node].voxelIndex
                        < nodes[result.predecessor[neighbor]].voxelIndex);
            if (shorter || tied) {
                result.distanceMm[neighbor] = candidate;
                result.predecessor[neighbor] = node;
                pending.push(std::make_pair(candidate, neighbor));
            }
        }
    }
    return result;
}

bool pairBefore(const std::vector<CenterlineBGraphNode>& nodes,
                std::size_t leftStart,
                std::size_t leftEnd,
                std::size_t rightStart,
                std::size_t rightEnd)
{
    return std::make_pair(nodes[leftStart].voxelIndex,
                          nodes[leftEnd].voxelIndex)
        < std::make_pair(nodes[rightStart].voxelIndex,
                         nodes[rightEnd].voxelIndex);
}

} // namespace

const char* centerlineBGraphStatusToken(CenterlineBGraphStatus status)
{
    switch (status) {
    case CenterlineBGraphStatus::Ok: return "ok";
    case CenterlineBGraphStatus::InvalidProfile: return "invalid_profile";
    case CenterlineBGraphStatus::InvalidSkeleton: return "invalid_skeleton";
    case CenterlineBGraphStatus::EmptySkeleton: return "empty_skeleton";
    case CenterlineBGraphStatus::DisconnectedSkeleton:
        return "disconnected_skeleton";
    case CenterlineBGraphStatus::AmbiguousTopology:
        return "ambiguous_topology";
    case CenterlineBGraphStatus::NonPositiveRadius:
        return "non_positive_radius";
    case CenterlineBGraphStatus::TooFewPathPoints:
        return "too_few_path_points";
    }
    return "unknown";
}

CenterlineBGraphResult CenterlineBGraph::extractMainPath(
    const CenterlineSkeletonV1& skeleton,
    const CenterlineBGraphProfileV1& profile)
{
    if (profile.contractVersion != CenterlineBGraphProfileV1::ContractVersion
        || !std::isfinite(profile.shortSpurLengthMm)
        || profile.shortSpurLengthMm < 0.0) {
        return failure(CenterlineBGraphStatus::InvalidProfile);
    }
    std::size_t observedSkeletonCount = 0;
    if (!validSkeletonStructure(skeleton, &observedSkeletonCount)) {
        return failure(CenterlineBGraphStatus::InvalidSkeleton);
    }
    if (observedSkeletonCount == 0) {
        return failure(CenterlineBGraphStatus::EmptySkeleton);
    }

    const std::size_t voxelCount = skeleton.skeleton.size();
    std::vector<std::size_t> nodeForVoxel(voxelCount, kNoNode);
    std::vector<CenterlineBGraphNode> nodes;
    nodes.reserve(skeleton.skeletonVoxelCount);
    for (std::size_t flat = 0; flat < voxelCount; ++flat) {
        if (skeleton.skeleton[flat] == 0) {
            continue;
        }
        const double radius = static_cast<double>(skeleton.radiusMm[flat]);
        if (!std::isfinite(radius) || !(radius > 0.0)) {
            return failure(CenterlineBGraphStatus::NonPositiveRadius);
        }
        CenterlineBGraphNode node;
        node.voxelIndex = flat;
        node.index = flatToIndex(flat, skeleton.geometry);
        node.positionMm = indexToPhysical(skeleton.geometry, node.index);
        node.radiusMm = radius;
        nodeForVoxel[flat] = nodes.size();
        nodes.push_back(std::move(node));
    }
    if (nodes.empty()) {
        return failure(CenterlineBGraphStatus::EmptySkeleton);
    }

    for (std::size_t node = 0; node < nodes.size(); ++node) {
        const std::array<int, 3>& index = nodes[node].index;
        for (int dz = -1; dz <= 1; ++dz) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0 && dz == 0) {
                        continue;
                    }
                    const int x = index[0] + dx;
                    const int y = index[1] + dy;
                    const int z = index[2] + dz;
                    if (x < 0 || y < 0 || z < 0
                        || x >= skeleton.geometry.dimensions[0]
                        || y >= skeleton.geometry.dimensions[1]
                        || z >= skeleton.geometry.dimensions[2]) {
                        continue;
                    }
                    const std::size_t neighborFlat =
                        indexToFlat(x, y, z, skeleton.geometry);
                    const std::size_t neighbor = nodeForVoxel[neighborFlat];
                    if (neighbor != kNoNode && neighbor != node) {
                        nodes[node].neighbors.push_back(neighbor);
                    }
                }
            }
        }
        std::sort(nodes[node].neighbors.begin(), nodes[node].neighbors.end(),
                  [&nodes](std::size_t left, std::size_t right) {
                      return nodes[left].voxelIndex < nodes[right].voxelIndex;
                  });
        nodes[node].neighbors.erase(
            std::unique(nodes[node].neighbors.begin(), nodes[node].neighbors.end()),
            nodes[node].neighbors.end());
    }

    CenterlineBGraphResult result;
    result.graph = nodes;
    result.stats.inputNodeCount = nodes.size();
    std::vector<bool> active(nodes.size(), true);
    std::vector<std::size_t> endpoints;
    countTopology(nodes, active, &result.stats.outputNodeCount, &endpoints,
                  &result.stats.inputJunctionCount);
    result.stats.inputEndpointCount = endpoints.size();
    if (!connected(nodes, active, nodes.size())) {
        return failure(CenterlineBGraphStatus::DisconnectedSkeleton);
    }

    pruneShortSpurs(nodes, profile.shortSpurLengthMm, &active);
    countTopology(nodes, active, &result.stats.outputNodeCount, &endpoints,
                  &result.stats.outputJunctionCount);
    result.stats.outputEndpointCount = endpoints.size();
    result.stats.prunedNodeCount =
        result.stats.inputNodeCount - result.stats.outputNodeCount;
    if (!connected(nodes, active, result.stats.outputNodeCount)) {
        return failure(CenterlineBGraphStatus::DisconnectedSkeleton);
    }
    if (endpoints.size() < 2) {
        return failure(CenterlineBGraphStatus::AmbiguousTopology);
    }

    double bestDistance = -1.0;
    std::size_t bestStart = kNoNode;
    std::size_t bestEnd = kNoNode;
    std::vector<std::size_t> bestPredecessor;
    for (std::size_t startIndex = 0; startIndex < endpoints.size(); ++startIndex) {
        const std::size_t start = endpoints[startIndex];
        const ShortestPaths paths = dijkstra(nodes, active, start);
        for (std::size_t endIndex = startIndex + 1;
             endIndex < endpoints.size(); ++endIndex) {
            const std::size_t end = endpoints[endIndex];
            const double candidate = paths.distanceMm[end];
            if (!std::isfinite(candidate)) {
                return failure(CenterlineBGraphStatus::DisconnectedSkeleton);
            }
            const bool longer = candidate > bestDistance + kDistanceTieTolerance;
            const bool tied = std::abs(candidate - bestDistance)
                    <= kDistanceTieTolerance
                && (bestStart == kNoNode
                    || pairBefore(nodes, start, end, bestStart, bestEnd));
            if (longer || tied) {
                bestDistance = candidate;
                bestStart = start;
                bestEnd = end;
                bestPredecessor = paths.predecessor;
            }
        }
    }
    if (bestStart == kNoNode || bestEnd == kNoNode) {
        return failure(CenterlineBGraphStatus::AmbiguousTopology);
    }

    std::vector<std::size_t> pathNodes;
    for (std::size_t node = bestEnd;; node = bestPredecessor[node]) {
        pathNodes.push_back(node);
        if (node == bestStart) {
            break;
        }
        if (bestPredecessor[node] == kNoNode
            || pathNodes.size() > result.stats.outputNodeCount) {
            return failure(CenterlineBGraphStatus::DisconnectedSkeleton);
        }
    }
    std::reverse(pathNodes.begin(), pathNodes.end());
    if (pathNodes.size() < 3) {
        return failure(CenterlineBGraphStatus::TooFewPathPoints);
    }

    result.mainPath.reserve(pathNodes.size());
    double arcLength = 0.0;
    for (std::size_t ordinal = 0; ordinal < pathNodes.size(); ++ordinal) {
        const CenterlineBGraphNode& node = nodes[pathNodes[ordinal]];
        if (ordinal > 0) {
            arcLength += distance(
                nodes[pathNodes[ordinal - 1]].positionMm, node.positionMm);
        }
        CenterlineBGraphSample sample;
        sample.voxelIndex = node.voxelIndex;
        sample.positionMm = node.positionMm;
        sample.radiusMm = node.radiusMm;
        sample.arcLengthMm = arcLength;
        result.mainPath.push_back(sample);
    }
    result.stats.mainPathLengthMm = arcLength;
    result.status = CenterlineBGraphStatus::Ok;
    return result;
}

} // namespace xq
