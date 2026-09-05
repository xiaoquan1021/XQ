#ifndef XQ_SERVICES_PATH_CENTERLINE_B_GRAPH_H
#define XQ_SERVICES_PATH_CENTERLINE_B_GRAPH_H

#include "core/GeometryTypes.h"
#include "core/path/ICenterlineSkeletonizer3D.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace xq {

struct CenterlineBGraphProfileV1 {
    static constexpr unsigned int ContractVersion = 1;

    unsigned int contractVersion = ContractVersion;
    double shortSpurLengthMm = 2.0;
};

enum class CenterlineBGraphStatus {
    Ok,
    InvalidProfile,
    InvalidSkeleton,
    EmptySkeleton,
    DisconnectedSkeleton,
    AmbiguousTopology,
    NonPositiveRadius,
    TooFewPathPoints
};

const char* centerlineBGraphStatusToken(CenterlineBGraphStatus status);

struct CenterlineBGraphNode {
    std::size_t voxelIndex = 0;
    std::array<int, 3> index{{0, 0, 0}};
    Point3 positionMm{0.0, 0.0, 0.0};
    double radiusMm = 0.0;
    std::vector<std::size_t> neighbors;
};

struct CenterlineBGraphSample {
    std::size_t voxelIndex = 0;
    Point3 positionMm{0.0, 0.0, 0.0};
    double radiusMm = 0.0;
    double arcLengthMm = 0.0;
};

struct CenterlineBGraphStats {
    std::size_t inputNodeCount = 0;
    std::size_t inputEndpointCount = 0;
    std::size_t inputJunctionCount = 0;
    std::size_t prunedNodeCount = 0;
    std::size_t outputNodeCount = 0;
    std::size_t outputEndpointCount = 0;
    std::size_t outputJunctionCount = 0;
    double mainPathLengthMm = 0.0;
};

struct CenterlineBGraphResult {
    CenterlineBGraphStatus status = CenterlineBGraphStatus::InvalidSkeleton;
    std::vector<CenterlineBGraphNode> graph;
    std::vector<CenterlineBGraphSample> mainPath;
    CenterlineBGraphStats stats;

    bool ok() const
    {
        return status == CenterlineBGraphStatus::Ok
            && graph.size() == stats.inputNodeCount
            && mainPath.size() >= 3;
    }
};

class CenterlineBGraph {
public:
    static CenterlineBGraphResult extractMainPath(
        const CenterlineSkeletonV1& skeleton,
        const CenterlineBGraphProfileV1& profile);
};

} // namespace xq

#endif // XQ_SERVICES_PATH_CENTERLINE_B_GRAPH_H
