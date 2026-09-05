#include "services/path/CenterlineBGraph.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <initializer_list>

namespace {

int fail(const char* expression, int line)
{
    std::fprintf(stderr, "FAIL: %s (line %d)\n", expression, line);
    return 1;
}

#define CHECK(expression)                        \
    do {                                         \
        if (!(expression)) {                     \
            return fail(#expression, __LINE__);  \
        }                                        \
    } while (0)

using Index = std::array<int, 3>;

std::size_t flat(const Index& index, const xq::ImageGeometry& geometry)
{
    return static_cast<std::size_t>(index[0])
        + static_cast<std::size_t>(geometry.dimensions[0])
            * (static_cast<std::size_t>(index[1])
               + static_cast<std::size_t>(geometry.dimensions[1])
                   * static_cast<std::size_t>(index[2]));
}

xq::Point3 physical(const Index& index, const xq::ImageGeometry& geometry)
{
    const double scaled[3] = {
        geometry.spacing[0] * static_cast<double>(index[0]),
        geometry.spacing[1] * static_cast<double>(index[1]),
        geometry.spacing[2] * static_cast<double>(index[2])
    };
    xq::Point3 point{
        geometry.origin[0], geometry.origin[1], geometry.origin[2]};
    double* output[3] = {&point.x, &point.y, &point.z};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            *output[row] += geometry.direction[row][column] * scaled[column];
        }
    }
    return point;
}

xq::ImageGeometry geometry()
{
    xq::ImageGeometry value{};
    value.dimensions[0] = 11;
    value.dimensions[1] = 11;
    value.dimensions[2] = 11;
    value.spacing[0] = 0.8;
    value.spacing[1] = 1.2;
    value.spacing[2] = 1.5;
    value.origin[0] = 5.0;
    value.origin[1] = -4.0;
    value.origin[2] = 2.5;
    const double angle = 0.4;
    value.direction[0][0] = std::cos(angle);
    value.direction[0][1] = -std::sin(angle);
    value.direction[0][2] = 0.0;
    value.direction[1][0] = std::sin(angle);
    value.direction[1][1] = std::cos(angle);
    value.direction[1][2] = 0.0;
    value.direction[2][0] = 0.0;
    value.direction[2][1] = 0.0;
    value.direction[2][2] = 1.0;
    value.coordinateSystem = xq::ImageCoordinateSystem::LPS;
    return value;
}

xq::CenterlineSkeletonV1 skeleton(
    std::initializer_list<Index> active,
    double radiusMm = 2.0)
{
    xq::CenterlineSkeletonV1 value;
    value.geometry = geometry();
    const std::size_t voxelCount =
        static_cast<std::size_t>(value.geometry.dimensions[0])
        * static_cast<std::size_t>(value.geometry.dimensions[1])
        * static_cast<std::size_t>(value.geometry.dimensions[2]);
    value.skeleton.assign(voxelCount, 0);
    value.radiusMm.assign(voxelCount, 0.0f);
    for (const Index& index : active) {
        const std::size_t voxel = flat(index, value.geometry);
        value.skeleton[voxel] = 1;
        value.radiusMm[voxel] = static_cast<float>(radiusMm);
    }
    value.inputForegroundVoxelCount = active.size() + 10;
    value.skeletonVoxelCount = active.size();
    value.thinningBackendId = "test.thinning";
    value.thinningBackendVersion = "1";
    value.distanceBackendId = "test.distance";
    value.distanceBackendVersion = "1";
    return value;
}

bool near(double left, double right, double tolerance = 1e-10)
{
    return std::abs(left - right) <= tolerance;
}

bool samePoint(const xq::Point3& left, const xq::Point3& right)
{
    return near(left.x, right.x) && near(left.y, right.y)
        && near(left.z, right.z);
}

} // namespace

int main()
{
    xq::CenterlineBGraphProfileV1 noPrune;
    noPrune.shortSpurLengthMm = 0.0;

    const xq::CenterlineSkeletonV1 straight = skeleton({
        {1, 2, 3}, {2, 2, 3}, {3, 2, 3}, {4, 2, 3}, {5, 2, 3}});
    const xq::CenterlineBGraphResult straightResult =
        xq::CenterlineBGraph::extractMainPath(straight, noPrune);
    CHECK(straightResult.ok());
    CHECK(straightResult.mainPath.size() == 5);
    CHECK(straightResult.stats.inputEndpointCount == 2);
    CHECK(straightResult.stats.inputJunctionCount == 0);
    CHECK(near(straightResult.stats.mainPathLengthMm, 4.0 * 0.8));
    CHECK(samePoint(straightResult.mainPath.front().positionMm,
                    physical({1, 2, 3}, straight.geometry)));
    CHECK(samePoint(straightResult.mainPath.back().positionMm,
                    physical({5, 2, 3}, straight.geometry)));
    for (std::size_t index = 1; index < straightResult.mainPath.size(); ++index) {
        CHECK(straightResult.mainPath[index].arcLengthMm
              > straightResult.mainPath[index - 1].arcLengthMm);
    }

    const xq::CenterlineSkeletonV1 bend = skeleton({
        {1, 1, 4}, {2, 2, 4}, {3, 3, 4}, {4, 4, 4}});
    const xq::CenterlineBGraphResult bendResult =
        xq::CenterlineBGraph::extractMainPath(bend, noPrune);
    CHECK(bendResult.ok());
    CHECK(bendResult.mainPath.size() == 4);
    CHECK(near(bendResult.stats.mainPathLengthMm,
               3.0 * std::sqrt(0.8 * 0.8 + 1.2 * 1.2)));

    const Index junction{5, 5, 5};
    const Index shortSpur{4, 6, 4};
    const xq::CenterlineSkeletonV1 branched = skeleton({
        junction,
        {6, 6, 6}, {7, 7, 7}, {8, 8, 8}, {9, 9, 9},
        {4, 4, 6}, {3, 3, 7}, {2, 2, 8}, {1, 1, 9},
        {6, 4, 4}, {7, 3, 3}, {8, 2, 2},
        shortSpur});
    xq::CenterlineBGraphProfileV1 prune;
    prune.shortSpurLengthMm = 2.2;
    const xq::CenterlineBGraphResult pruned =
        xq::CenterlineBGraph::extractMainPath(branched, prune);
    CHECK(pruned.ok());
    CHECK(pruned.stats.inputEndpointCount == 4);
    CHECK(pruned.stats.inputJunctionCount == 1);
    CHECK(pruned.stats.prunedNodeCount == 1);
    CHECK(pruned.stats.outputEndpointCount == 3);
    for (const xq::CenterlineBGraphSample& sample : pruned.mainPath) {
        CHECK(sample.voxelIndex != flat(shortSpur, branched.geometry));
    }

    xq::CenterlineBGraphProfileV1 keepSpur;
    keepSpur.shortSpurLengthMm = 1.0;
    const xq::CenterlineBGraphResult unpruned =
        xq::CenterlineBGraph::extractMainPath(branched, keepSpur);
    CHECK(unpruned.ok());
    CHECK(unpruned.stats.prunedNodeCount == 0);
    CHECK(unpruned.stats.outputEndpointCount == 4);

    const xq::CenterlineBGraphResult repeated =
        xq::CenterlineBGraph::extractMainPath(branched, prune);
    CHECK(repeated.ok());
    CHECK(repeated.mainPath.size() == pruned.mainPath.size());
    for (std::size_t index = 0; index < pruned.mainPath.size(); ++index) {
        CHECK(repeated.mainPath[index].voxelIndex
              == pruned.mainPath[index].voxelIndex);
        CHECK(repeated.mainPath[index].arcLengthMm
              == pruned.mainPath[index].arcLengthMm);
    }

    const xq::CenterlineSkeletonV1 disconnected = skeleton({
        {1, 1, 1}, {2, 1, 1}, {3, 1, 1},
        {7, 7, 7}, {8, 7, 7}, {9, 7, 7}});
    CHECK(xq::CenterlineBGraph::extractMainPath(disconnected, noPrune).status
          == xq::CenterlineBGraphStatus::DisconnectedSkeleton);

    const xq::CenterlineSkeletonV1 loop = skeleton({
        {2, 2, 2}, {3, 2, 2}, {3, 3, 2}, {2, 3, 2}});
    CHECK(xq::CenterlineBGraph::extractMainPath(loop, noPrune).status
          == xq::CenterlineBGraphStatus::AmbiguousTopology);

    xq::CenterlineSkeletonV1 badRadius = straight;
    badRadius.radiusMm[flat({3, 2, 3}, badRadius.geometry)] = 0.0f;
    CHECK(xq::CenterlineBGraph::extractMainPath(badRadius, noPrune).status
          == xq::CenterlineBGraphStatus::NonPositiveRadius);

    xq::CenterlineSkeletonV1 empty = skeleton({});
    empty.inputForegroundVoxelCount = 1;
    CHECK(xq::CenterlineBGraph::extractMainPath(empty, noPrune).status
          == xq::CenterlineBGraphStatus::EmptySkeleton);

    const xq::CenterlineSkeletonV1 tooShort = skeleton({
        {1, 1, 1}, {2, 1, 1}});
    CHECK(xq::CenterlineBGraph::extractMainPath(tooShort, noPrune).status
          == xq::CenterlineBGraphStatus::TooFewPathPoints);

    xq::CenterlineBGraphProfileV1 invalidProfile;
    invalidProfile.shortSpurLengthMm = -1.0;
    CHECK(xq::CenterlineBGraph::extractMainPath(straight, invalidProfile).status
          == xq::CenterlineBGraphStatus::InvalidProfile);

    std::printf("Centerline B physical graph checks passed\n");
    return 0;
}
