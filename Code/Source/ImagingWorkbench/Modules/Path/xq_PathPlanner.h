#pragma once

// XQ Pipeline Stage 1: Path planning interface.
//
// Design: strategy pattern — concrete planners encapsulate XQ-native Dijkstra
// on voxel graph or user-provided anchor interpolation.
// The planner reads a 3D scalar image and a list of seed points, and emits
// a smoothed 3D centerline with tangent/normal frames.
//
// Pipeline contract (DataStorage-level):
//   Input  : Image node          (MITK::Image)
//   Output : Path node           (xq_VesselCenterline), MarkNode(..., Stage::Path)
//   Props  : xq.source.image     = <image name>
//            xq.pipeline.algorithm = "dijkstra" | "manual"

#include <xqModulePathExports.h>

#include <mitkImage.h>
#include <mitkPoint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

class XQMODULEPATH_EXPORT xq_PathPlanner
{
public:
    struct Request
    {
        mitk::Image::Pointer image;
        std::vector<mitk::Point3D> seeds;     // >=2 points (start, end, optional waypoints)
        int    resolution    = 200;           // output spline sample count
        double stepSize      = 0.5;           // arc-length step in world units
        bool   smoothCurve   = true;          // apply vtkParametricSpline smoothing
        double speedExponent = 1.0;           // for speed image: 1/(|grad|+eps)^n
    };

    struct Result
    {
        bool ok = false;
        std::string diagnostic;
        std::vector<mitk::Point3D> points;    // raw extracted path
        std::string requestedAlgorithm;
        std::string actualAlgorithm;
        bool usedFallback = false;
    };

    virtual ~xq_PathPlanner() = default;
    [[nodiscard]] virtual std::string_view Name() const = 0;

    // Compute a path from image+seeds. MUST NOT touch DataStorage.
    [[nodiscard]] virtual Result Plan(const Request& request) = 0;
};

// ---------------------------------------------------------------------------
// Concrete: Dijkstra on voxel graph (XQ-native CPU planner).
// ---------------------------------------------------------------------------
class XQMODULEPATH_EXPORT xq_DijkstraPathPlanner : public xq_PathPlanner
{
public:
    [[nodiscard]] std::string_view Name() const override { return "dijkstra"; }
    [[nodiscard]] Result Plan(const Request& request) override;
};

// ---------------------------------------------------------------------------
// Concrete: VMTK Fast Marching request placeholder.
// VMTK is out of scope for the native-only plan, so this planner reports a
// diagnostic and does not create a path. Users must request "dijkstra" for
// the XQ-native planner.
// ---------------------------------------------------------------------------
class XQMODULEPATH_EXPORT xq_VmtkFastMarchingPathPlanner : public xq_PathPlanner
{
public:
    [[nodiscard]] std::string_view Name() const override { return "vmtk_fastmarching"; }
    [[nodiscard]] Result Plan(const Request& request) override;
};

// Factory: returns the default XQ-native planner. The caller owns the result.
XQMODULEPATH_EXPORT std::unique_ptr<xq_PathPlanner> CreateDefaultPathPlanner();
