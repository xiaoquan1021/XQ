#include "xq_PathPlanner.h"

#include <vtkImageData.h>
#include <vtkMath.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>

namespace
{

struct VoxelIdx
{
    int x = 0;
    int y = 0;
    int z = 0;
    [[nodiscard]] bool InBounds(const int* dims) const
    {
        return x >= 0 && y >= 0 && z >= 0 &&
               x < dims[0] && y < dims[1] && z < dims[2];
    }
    [[nodiscard]] size_t Linear(const int* dims) const
    {
        return static_cast<size_t>(x)
             + static_cast<size_t>(y) * dims[0]
             + static_cast<size_t>(z) * static_cast<size_t>(dims[0]) * dims[1];
    }
};

VoxelIdx WorldToVoxel(const mitk::Point3D& world, vtkImageData* vtkImg)
{
    double origin[3], spacing[3];
    vtkImg->GetOrigin(origin);
    vtkImg->GetSpacing(spacing);
    VoxelIdx v;
    v.x = static_cast<int>(std::round((world[0] - origin[0]) / spacing[0]));
    v.y = static_cast<int>(std::round((world[1] - origin[1]) / spacing[1]));
    v.z = static_cast<int>(std::round((world[2] - origin[2]) / spacing[2]));
    return v;
}

mitk::Point3D VoxelToWorld(const VoxelIdx& v, vtkImageData* vtkImg)
{
    double origin[3], spacing[3];
    vtkImg->GetOrigin(origin);
    vtkImg->GetSpacing(spacing);
    mitk::Point3D p;
    p[0] = origin[0] + v.x * spacing[0];
    p[1] = origin[1] + v.y * spacing[1];
    p[2] = origin[2] + v.z * spacing[2];
    return p;
}

// Cost: inverse speed. Brighter intensity (vessel lumen on CT/MR
// angiography) => lower cost. caller-scaled with speedExponent.
double CostAt(vtkImageData* vtkImg, const VoxelIdx& v, double exponent)
{
    const double intensity = vtkImg->GetScalarComponentAsDouble(v.x, v.y, v.z, 0);
    // Map to speed in (0,1]; avoid div-by-zero, bias so even dark voxels
    // are traversable rather than impassable.
    const double speed = std::pow(std::max(intensity, 1.0), exponent);
    return 1.0 / (speed + 1e-6);
}

} // namespace

xq_PathPlanner::Result xq_DijkstraPathPlanner::Plan(const Request& request)
{
    Result res;
    if (!request.image)
    {
        res.diagnostic = "Dijkstra planner: image is null.";
        return res;
    }
    if (request.seeds.size() < 2)
    {
        res.diagnostic = "Dijkstra planner: need at least 2 seeds (start and end).";
        return res;
    }

    vtkImageData* vtk = request.image->GetVtkImageData();
    if (!vtk)
    {
        res.diagnostic = "Dijkstra planner: image has no vtkImageData.";
        return res;
    }

    int dims[3];
    vtk->GetDimensions(dims);
    if (dims[0] <= 0 || dims[1] <= 0 || dims[2] <= 0)
    {
        res.diagnostic = "Dijkstra planner: image dimensions are empty.";
        return res;
    }

    // Multi-segment Dijkstra: chain seeds[i] -> seeds[i+1].
    std::vector<mitk::Point3D> fullPath;
    for (size_t seg = 0; seg + 1 < request.seeds.size(); ++seg)
    {
        const VoxelIdx start = WorldToVoxel(request.seeds[seg], vtk);
        const VoxelIdx end   = WorldToVoxel(request.seeds[seg + 1], vtk);
        if (!start.InBounds(dims) || !end.InBounds(dims))
        {
            res.diagnostic = "Dijkstra planner: seed " +
                std::to_string(seg) + " is outside image bounds.";
            return res;
        }

        const size_t voxelCount = static_cast<size_t>(dims[0]) * dims[1] * dims[2];
        std::vector<double> dist(voxelCount, std::numeric_limits<double>::infinity());
        std::unordered_map<size_t, size_t> predecessor;
        using Node = std::pair<double, size_t>; // (cost, linear index)
        std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;

        const size_t startLin = start.Linear(dims);
        const size_t endLin   = end.Linear(dims);
        dist[startLin] = 0.0;
        pq.emplace(0.0, startLin);

        // 6-neighborhood offsets (axis-aligned; sufficient for voxel graph).
        static constexpr int kNb[6][3] = {
            { 1, 0, 0}, {-1, 0, 0},
            { 0, 1, 0}, { 0,-1, 0},
            { 0, 0, 1}, { 0, 0,-1}};

        while (!pq.empty())
        {
            auto [cost, lin] = pq.top();
            pq.pop();
            if (cost > dist[lin])
                continue;
            if (lin == endLin)
                break;

            const int z = static_cast<int>(lin / (static_cast<size_t>(dims[0]) * dims[1]));
            const int rem = static_cast<int>(lin % (static_cast<size_t>(dims[0]) * dims[1]));
            const int y = rem / dims[0];
            const int x = rem % dims[0];

            for (const auto& off : kNb)
            {
                VoxelIdx nb{x + off[0], y + off[1], z + off[2]};
                if (!nb.InBounds(dims))
                    continue;
                const size_t nbLin = nb.Linear(dims);
                const double step = CostAt(vtk, nb, request.speedExponent);
                const double nd = cost + step;
                if (nd < dist[nbLin])
                {
                    dist[nbLin] = nd;
                    predecessor[nbLin] = lin;
                    pq.emplace(nd, nbLin);
                }
            }
        }

        if (!std::isfinite(dist[endLin]))
        {
            res.diagnostic = "Dijkstra planner: no path between seed " +
                std::to_string(seg) + " and seed " + std::to_string(seg + 1) + ".";
            return res;
        }

        // Backtrack from endLin.
        std::vector<mitk::Point3D> segPath;
        for (size_t cur = endLin; ; )
        {
            const int z = static_cast<int>(cur / (static_cast<size_t>(dims[0]) * dims[1]));
            const int rem = static_cast<int>(cur % (static_cast<size_t>(dims[0]) * dims[1]));
            const int y = rem / dims[0];
            const int x = rem % dims[0];
            segPath.push_back(VoxelToWorld({x, y, z}, vtk));
            auto it = predecessor.find(cur);
            if (it == predecessor.end())
                break;
            cur = it->second;
        }
        std::reverse(segPath.begin(), segPath.end());

        // Avoid duplicating the seam point when chaining segments.
        if (seg > 0 && !segPath.empty() && !fullPath.empty())
            segPath.erase(segPath.begin());
        fullPath.insert(fullPath.end(), segPath.begin(), segPath.end());
    }

    res.ok = true;
    res.points = std::move(fullPath);
    return res;
}

xq_PathPlanner::Result xq_VmtkFastMarchingPathPlanner::Plan(const Request& request)
{
    // TODO(xq-vmtk): integrate vtkvmtkFastMarchingUpwindGradientImageFilter
    // from the external VMTK package once it is exposed through XQ's CMake.
    // Until then, gracefully fall back to Dijkstra so the pipeline is
    // functional end-to-end without silently returning empty paths.
    xq_DijkstraPathPlanner fallback;
    auto r = fallback.Plan(request);
    if (r.ok)
        r.diagnostic = "VMTK Fast Marching is not yet linked in this build; "
                       "falling back to Dijkstra.";
    return r;
}

std::unique_ptr<xq_PathPlanner> CreateDefaultPathPlanner()
{
#if defined(XQ_HAS_VMTK)
    return std::make_unique<xq_VmtkFastMarchingPathPlanner>();
#else
    return std::make_unique<xq_DijkstraPathPlanner>();
#endif
}
