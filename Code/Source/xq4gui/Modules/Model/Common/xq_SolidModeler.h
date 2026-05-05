#pragma once

// XQ Pipeline Stage 3: solid modeling interface (Bridge pattern).
//
// The Model stage turns a set of ContourGroups into a capped 3D manifold
// surface. Two engines are supported:
//   - xq_VtkSolidModeler  : loft vtkPolyData + Boolean ops via VTK filters
//   - xq_OccSolidModeler  : promote to OpenCASCADE BRep and use BRepAlgoAPI
//
// Callers never link directly to VTK/OCC; they go through this interface.
// That is the "Bridge" — the abstraction (Model pipeline) is decoupled from
// the implementation (VTK vs OCC) and the two can evolve independently.

#include <xqModelCommonExports.h>

#include <vtkSmartPointer.h>
class vtkPolyData;

#include <memory>
#include <string>
#include <string_view>
#include <vector>

class XQMODELCOMMON_EXPORT xq_SolidModeler
{
public:
    struct BuildRequest
    {
        std::vector<vtkSmartPointer<vtkPolyData>> loftedWalls; // one per contour group
        bool     capEnds    = true;
        bool     booleanUnion = true;   // union all lofted walls
        double   blendRadius = 0.0;     // >0 enables blend/fillet at intersections
    };

    struct BuildResult
    {
        bool ok = false;
        std::string diagnostic;
        vtkSmartPointer<vtkPolyData> surface;
        // Cell-parallel vector of per-cap face IDs. 0 = wall, >=1 = cap index.
        // Ownership handed to caller via std::move.
        std::vector<int> faceIdsPerCell;
        int wallCellCount = 0;
        int capCount = 0;
    };

    virtual ~xq_SolidModeler() = default;
    [[nodiscard]] virtual std::string_view Name() const = 0;
    [[nodiscard]] virtual BuildResult Build(const BuildRequest& request) = 0;
};

// ---------------------------------------------------------------------------
// Concrete: VTK-based lofting and boolean. Always available.
// ---------------------------------------------------------------------------
class XQMODELCOMMON_EXPORT xq_VtkSolidModeler : public xq_SolidModeler
{
public:
    [[nodiscard]] std::string_view Name() const override { return "vtk"; }
    [[nodiscard]] BuildResult Build(const BuildRequest& request) override;
};

// ---------------------------------------------------------------------------
// Concrete: OpenCASCADE-based modeling — available when XQ_USE_OPENCASCADE
// is set. The implementation currently delegates to VTK and annotates the
// diagnostic; replacing that body with real BRepAlgoAPI calls is the next
// step and does not require API changes on the caller side.
// ---------------------------------------------------------------------------
class XQMODELCOMMON_EXPORT xq_OccSolidModeler : public xq_SolidModeler
{
public:
    [[nodiscard]] std::string_view Name() const override { return "occt"; }
    [[nodiscard]] BuildResult Build(const BuildRequest& request) override;
};

XQMODELCOMMON_EXPORT std::unique_ptr<xq_SolidModeler>
CreateSolidModeler(std::string_view name);
