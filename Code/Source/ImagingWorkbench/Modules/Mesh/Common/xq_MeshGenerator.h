#pragma once

// XQ Pipeline Stage 4: volumetric mesh generator interface.
//
// Wraps the concrete generator so the pipeline can switch engines without
// touching the Model stage. The current native implementation uses VTK
// Delaunay3D and records that honestly in pipeline metadata.

#include <xqMeshCommonExports.h>

#include "xq_Grid.h"
#include "xq_MeshPipeline.h"

class xq_VascularGeometry;

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class XQMESHCOMMON_EXPORT xq_MeshGenerator
{
public:
    struct Params
    {
        double globalEdgeSize         = 1.0;
        double qualityRatio           = 1.4;
        double volumeConstraint       = 0.0;
        bool   boundaryLayer          = false;
        int    boundaryLayerCount     = 0;
        double boundaryLayerFirstHeight = 0.1;   // XQ fix: was dropped before
        double boundaryLayerGrowthRate = 1.2;
        std::map<int, double> localFaceSizes;
        std::vector<xq_RefinementRegion> refinementRegions;
        bool   preserveSurface        = true;
        bool   optimize               = true;
        double minDihedral            = 10.0;
        double maxEdgeSize            = 0.0;
    };

    struct Result
    {
        bool ok = false;
        std::string diagnostic;
        std::unique_ptr<xq_Grid> grid;     // concrete type depends on engine
    };

    virtual ~xq_MeshGenerator() = default;
    [[nodiscard]] virtual std::string_view Name() const = 0;
    [[nodiscard]] virtual Result Generate(
        xq_VascularGeometry* modelElement, const Params& params) = 0;
};

// XQ-native VTK Delaunay3D generator. The class name remains for legacy mesh
// storage compatibility; Name() reports the actual backend used.
class XQMESHCOMMON_EXPORT xq_TetGenMeshGenerator : public xq_MeshGenerator
{
public:
    [[nodiscard]] std::string_view Name() const override { return "vtk_delaunay3d_fallback"; }
    [[nodiscard]] Result Generate(
        xq_VascularGeometry* modelElement, const Params& params) override;
};

// Factory.
XQMESHCOMMON_EXPORT std::unique_ptr<xq_MeshGenerator>
CreateMeshGenerator(std::string_view name);
