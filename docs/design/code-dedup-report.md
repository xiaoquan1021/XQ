# XQ ↔ SimVascular Code Deduplication Report

**Date:** 2025-07-17  
**Scope:** `XQ/Code/Source/xq4gui/` vs `Simvascular/Code/Source/sv4gui/`  
**Method:** Normalized diff comparison (prefix-agnostic: `xq_` ↔ `sv4gui_` treated as identical)

---

## Executive Summary

The XQ codebase shares significant structural heritage with SimVascular (SV). After normalizing namespace/prefix differences (`xq_` ↔ `sv4gui_`), the **average structural similarity across 60 compared files is ~43%**. Key findings:

| Risk Level | Similarity | File Count | Description |
|------------|-----------|------------|-------------|
| 🔴 Critical | ≥55% | 6 files | Nearly identical structure, trivial rename |
| 🟠 High | 45–54% | 22 files | Same API shape, similar implementations |
| 🟡 Medium | 35–44% | 20 files | Recognizable ancestry, moderate divergence |
| 🟢 Low | <35% | 12 files | Substantially rewritten or new design |

**Highest similarity files** are concentrated in **ProjectManagement** (avg 52%) and **Mesh** (avg 43%). The lowest similarity is in **Simulation** (avg 39%) where XQ has diverged most in its solver-oriented design.

---

## Per-Module Breakdown

### 1. Common Module

| File Pair | XQ Lines | SV Lines | Similarity | Priority |
|-----------|---------|---------|-----------|----------|
| `xq_Spline.cxx` ↔ `sv4gui_Spline.cxx` | 39 | 186 | **53%** | 🔴 P1 |
| `xq_Spline.h` ↔ `sv4gui_Spline.h` | 31 | 81 | **51%** | 🔴 P1 |
| `xq_Math3.h` ↔ `sv4gui_Math3.h` | 50 | 64 | 41% | 🟡 P3 |
| `xq_StringUtils.h` ↔ `sv4gui_StringUtils.h` | 24 | 102 | **47%** | 🟠 P2 |
| `xq_XmlIOUtil.h` ↔ `sv4gui_XmlIOUtil.h` | 40 | 65 | 43% | 🟡 P3 |
| `xq_VtkUtils.h` ↔ `sv4gui_VtkUtils.h` | 41 | 58 | 43% | 🟡 P3 |
| `xq_VtkParametricSpline.h` ↔ `sv4gui_VtkParametricSpline.h` | 32 | 55 | 38% | 🟡 P3 |
| `xq_VtkUtils.cxx` ↔ `sv4gui_VtkUtils.cxx` | 124 | 124 | 19% | 🟢 OK |
| `xq_Math3.cxx` ↔ `sv4gui_Math3.cxx` | 202 | 354 | 39% | 🟡 P3 |
| `xq_XmlIOUtil.cxx` ↔ `sv4gui_XmlIOUtil.cxx` | 93 | 132 | 37% | 🟡 P3 |
| `xq_StringUtils.cxx` ↔ `sv4gui_StringUtils.cxx` | 90 | 74 | 27% | 🟢 OK |
| `xq_VtkParametricSpline.cxx` ↔ `sv4gui_VtkParametricSpline.cxx` | 163 | 84 | 39% | 🟡 P3 |

**Module average: 40%**

**Highest-similarity example — `xq_VtkUtils.h` vs `sv4gui_VtkUtils.h`:**

XQ retains 4 of 4 original SV methods with identical signatures and adds 3 new ones:

```cpp
// === XQ xq_VtkUtils.h ===
class XQMODULECOMMON_EXPORT xq_VtkUtils {
public:
    static vtkSmartPointer<vtkPolyData> MergePoints(vtkSmartPointer<vtkPolyData> inpd, double tol = 1e-6);
    static vtkImageData* MitkImage2VtkImage(mitk::Image* image);  // identical to SV
    static void ResetMitkImage(mitk::Image* image);               // identical to SV
    // NEW in XQ:
    static double CalculateSurfaceArea(vtkSmartPointer<vtkPolyData> polydata);
    static double CalculateVolume(vtkSmartPointer<vtkPolyData> polydata);
    static mitk::Point3D GetCenterOfMass(vtkSmartPointer<vtkPolyData> polydata);
    static vtkSmartPointer<vtkPolyData> CreateSphere(const mitk::Point3D& center, double radius, int resolution = 20);
};

// === SV sv4gui_VtkUtils.h ===
class SV4GUIMODULECOMMON_EXPORT sv4guiVtkUtils {
public:
    static vtkSmartPointer<vtkPolyData> MergePoints(vtkSmartPointer<vtkPolyData> inpd);
    static vtkSmartPointer<vtkPolyData> MergePoints(vtkSmartPointer<vtkPolyData> inpd, double tol);
    static vtkImageData* MitkImage2VtkImage(mitk::Image* image);
    static void ResetMitkImage(mitk::Image* image);
};
```

**Recommendations for Common:**
- `Spline`: Refactor to use a different spline parameterization (e.g., Catmull-Rom or custom arc-length) instead of mirroring SV's vtkParametricSpline wrapper. Change class interface to builder pattern.
- `VtkUtils`: The 3 new methods already differentiate XQ. Remove or rename `MergePoints` / `MitkImage2VtkImage` — wrap in XQ-specific namespace or combine into a broader geometry-ops utility.
- `Math3`: Rewrite using Eigen or glm vector ops instead of raw array math matching SV's style.

---

### 2. Path Module

| File Pair | XQ Lines | SV Lines | Similarity | Priority |
|-----------|---------|---------|-----------|----------|
| `xq_CenterlineIO.h` ↔ `sv4gui_PathIO.h` | 20 | 67 | **52%** | 🔴 P1 |
| `xq_CenterlineInteractor.h` ↔ `sv4gui_PathDataInteractor.h` | 43 | 109 | **46%** | 🟠 P2 |
| `xq_PathObjectFactory.h` ↔ `sv4gui_PathObjectFactory.h` | 42 | 78 | **45%** | 🟠 P2 |
| `xq_CenterlineOp.h` ↔ `sv4gui_PathOperation.h` | 44 | 90 | 36% | 🟡 P3 |
| `xq_VesselCenterline.cxx` ↔ `sv4gui_Path.cxx` | 283 | 416 | 35% | 🟡 P3 |
| `xq_VesselCenterline.h` ↔ `sv4gui_Path.h` | 70 | 148 | 34% | 🟡 P3 |
| `xq_CenterlineSegment.cxx` ↔ `sv4gui_PathElement.cxx` | 279 | 330 | 32% | 🟢 OK |
| `xq_PathObjectFactory.cxx` ↔ `sv4gui_PathObjectFactory.cxx` | 160 | 147 | 31% | 🟢 OK |
| `xq_CenterlineIO.cxx` ↔ `sv4gui_PathIO.cxx` | 201 | 192 | 25% | 🟢 OK |
| `xq_CenterlineSegment.h` ↔ `sv4gui_PathElement.h` | 87 | 99 | 21% | 🟢 OK |

**Module average: 36%**

**Note:** XQ's Path module has been meaningfully renamed (Centerline/Vessel terminology vs SV's Path terminology). The `.cxx` implementations show ≤35% similarity, indicating substantial algorithmic divergence. The header files for IO and factory patterns remain structurally similar due to MITK framework conventions.

**Recommendations for Path:**
- `CenterlineIO.h`: Already small (20 lines). Add XQ-specific metadata fields (vessel type, branch order) to the IO interface.
- `PathObjectFactory`: This is boilerplate dictated by MITK. Consider using a template-based registration macro to replace hand-written factory code.
- `CenterlineInteractor.h`: Add vessel-specific interaction modes (bifurcation snapping, stenosis marking) to differentiate from SV's generic path interactor.

---

### 3. Segmentation Module

| File Pair | XQ Lines | SV Lines | Similarity | Priority |
|-----------|---------|---------|-----------|----------|
| `xq_SegmentationUtils.cxx` ↔ `sv4gui_SegmentationUtils.cxx` | 363 | 1045 | 42% | 🟡 P3 |
| `xq_ProfileGroup.cxx` ↔ `sv4gui_ContourGroup.cxx` | 248 | 735 | **49%** | 🟠 P2 |
| `xq_PolygonalProfile.cxx` ↔ `sv4gui_ContourPolygon.cxx` | 39 | 230 | **49%** | 🟠 P2 |
| `xq_LumenProfile.cxx` ↔ `sv4gui_Contour.cxx` | 155 | 418 | **48%** | 🟠 P2 |
| `xq_SegmentationUtils.h` ↔ `sv4gui_SegmentationUtils.h` | 39 | 148 | **47%** | 🟠 P2 |
| `xq_EllipticProfile.h` ↔ `sv4gui_ContourEllipse.h` | 17 | 77 | **46%** | 🟠 P2 |
| `xq_EllipticProfile.cxx` ↔ `sv4gui_ContourEllipse.cxx` | 85 | 327 | **46%** | 🟠 P2 |
| `xq_PolygonalProfile.h` ↔ `sv4gui_ContourPolygon.h` | 17 | 73 | **46%** | 🟠 P2 |
| `xq_SegmentationObjectFactory.h` ↔ `sv4gui_SegmentationObjectFactory.h` | 30 | 79 | **45%** | 🟠 P2 |
| `xq_CircularProfile.h` ↔ `sv4gui_ContourCircle.h` | 20 | 73 | 44% | 🟡 P3 |
| `xq_ProfileGroup.h` ↔ `sv4gui_ContourGroup.h` | 80 | 314 | 43% | 🟡 P3 |
| `xq_CircularProfile.cxx` ↔ `sv4gui_ContourCircle.cxx` | 119 | 205 | 35% | 🟡 P3 |
| `xq_LumenProfile.h` ↔ `sv4gui_Contour.h` | 103 | 168 | 23% | 🟢 OK |
| `xq_MitkSeg3D.h` ↔ `sv4gui_MitkSeg3D.h` | 61 | 76 | 22% | 🟢 OK |

**Module average: 42%**

**Highest-similarity example — `xq_PolygonalProfile.cxx` vs `sv4gui_ContourPolygon.cxx`:**

XQ uses modern C++ (std::accumulate with lambdas, unique_ptr), but the core algorithm (centroid from control points) is the same:

```cpp
// === XQ xq_PolygonalProfile.cxx ===
void xq_PolygonalProfile::CreateContourPoints() {
    m_ContourPoints = m_ControlPoints;
    if (!m_ControlPoints.empty()) {
        // Compute centroid using std::accumulate with lambda
        auto sum = std::accumulate(m_ControlPoints.cbegin(), m_ControlPoints.cend(), zero,
            [](mitk::Point3D acc, const mitk::Point3D& p) { ... });
        m_CenterPoint[0] = sum[0] / n;  // same algorithm as SV
    }
}
std::unique_ptr<xq_LumenProfile> xq_PolygonalProfile::Clone() const {
    auto clone = std::make_unique<xq_PolygonalProfile>();  // unique_ptr (differs from SV)
    copyBaseState(*clone);
    return clone;
}

// === SV sv4gui_ContourPolygon.cxx (via sv3::ContourPolygon) ===
// Uses raw new/clone pattern, same centroid computation
```

**Recommendations for Segmentation:**
- `ProfileGroup` / `LumenProfile`: Introduce a profile-storage abstraction (e.g., indexed by vessel branch ID) that SV doesn't have. This would naturally differentiate the group management code.
- `SegmentationUtils`: XQ is already 1/3 the size of SV — the shared methods (lofting, contour-to-polydata) should be reimplemented using a different surface reconstruction approach (e.g., CGAL-based or marching cubes variant).
- Circular/Elliptic/Polygonal profiles: These are mathematically determined — differentiation should come from the class hierarchy design (XQ already uses unique_ptr instead of raw clone). Continue moving toward value semantics.

---

### 4. Model Module

| File Pair | XQ Lines | SV Lines | Similarity | Priority |
|-----------|---------|---------|-----------|----------|
| `xq_ModelDataInteractor.h` ↔ `sv4gui_ModelDataInteractor.h` | 29 | 95 | **51%** | 🔴 P1 |
| `xq_VascularGeometry.cxx` ↔ `sv4gui_ModelElement.cxx` | 67 | 650 | **51%** | 🔴 P1 |
| `xq_GeometryIO.h` ↔ `sv4gui_ModelIO.h` | 17 | 62 | **51%** | 🔴 P1 |
| `xq_PolyGeometry.cxx` ↔ `sv4gui_ModelElementPolyData.cxx` | 64 | 924 | **50%** | 🟠 P2 |
| `xq_GeometryUtils.cxx` ↔ `sv4gui_ModelUtils.cxx` | 268 | 1596 | **48%** | 🟠 P2 |
| `xq_PolyGeometry.h` ↔ `sv4gui_ModelElementPolyData.h` | 26 | 144 | **47%** | 🟠 P2 |
| `xq_VascularGeometry.h` ↔ `sv4gui_ModelElement.h` | 48 | 353 | **47%** | 🟠 P2 |
| `xq_Model.cxx` ↔ `sv4gui_Model.cxx` | 198 | 298 | **45%** | 🟠 P2 |
| `xq_GeometryUtils.h` ↔ `sv4gui_ModelUtils.h` | 46 | 127 | 43% | 🟡 P3 |
| `xq_Model.h` ↔ `sv4gui_Model.h` | 55 | 104 | 34% | 🟢 OK |

**Module average: 47%**

**Highest-similarity example — `xq_GeometryIO.h` vs `sv4gui_ModelIO.h`:**

```cpp
// === XQ ===                                    // === SV ===
#pragma once                                     // #ifndef SV4GUI_MODELIO_H
#include <xqModelCommonExports.h>                // #include "sv4guiModuleModelExports.h"
#include <mitkAbstractFileIO.h>                  // #include <mitkAbstractFileIO.h>
                                                 //
class XQMODELCOMMON_EXPORT xq_GeometryIO         // class SV4GUIMODULEMODEL_EXPORT sv4guiModelIO
    : public mitk::AbstractFileIO               //     : public mitk::AbstractFileIO
{                                                // {
public:                                          // public:
    xq_GeometryIO();                             //     sv4guiModelIO();
    std::vector<...> DoRead() override;          //     std::vector<...> Read() override;
    void Write() override;                       //     void Write() override;
protected:                                       // protected:
    xq_GeometryIO* IOClone() const override;     //     sv4guiModelIO* IOClone() const override;
};                                               // };
```

The structure is essentially identical — only class names and export macros differ.

**Recommendations for Model:**
- `GeometryIO` / `ModelDataInteractor`: These are MITK framework patterns with minimal logic. Add XQ-specific capabilities (multi-format export, undo history tracking) or consolidate into a single GeometryPersistence class.
- `VascularGeometry` / `PolyGeometry`: XQ files are 10× smaller — they delegate to SV-compatible interfaces. Introduce XQ-native geometry representation (e.g., half-edge mesh or CGAL surface mesh) to break the dependency.
- `GeometryUtils`: At 268 lines (vs SV's 1596), XQ has already trimmed significantly. Ensure remaining methods use distinct algorithms.

---

### 5. Mesh Module

| File Pair | XQ Lines | SV Lines | Similarity | Priority |
|-----------|---------|---------|-----------|----------|
| `xq_MitkGridIO.h` ↔ `sv4gui_MitkMeshIO.h` | 20 | 73 | **53%** | 🔴 P1 |
| `xq_Grid.cxx` ↔ `sv4gui_Mesh.cxx` | 92 | 297 | **52%** | 🔴 P1 |
| `xq_GridFactory.cxx` ↔ `sv4gui_MeshFactory.cxx` | 39 | 135 | **48%** | 🟠 P2 |
| `xq_TetGenGrid.cxx` ↔ `sv4gui_MeshTetGen.cxx` | 139 | 550 | **47%** | 🟠 P2 |
| `xq_GridFactory.h` ↔ `sv4gui_MeshFactory.h` | 28 | 78 | 43% | 🟡 P3 |
| `xq_MitkGrid.h` ↔ `sv4gui_MitkMesh.h` | 45 | 109 | 38% | 🟡 P3 |
| `xq_Grid.h` ↔ `sv4gui_Mesh.h` | 75 | 133 | 36% | 🟡 P3 |
| `xq_TetGenGrid.h` ↔ `sv4gui_MeshTetGen.h` | 62 | 76 | 23% | 🟢 OK |

**Module average: 43%**

**Highest-similarity example — `xq_MitkGridIO.h` vs `sv4gui_MitkMeshIO.h`:**

```cpp
// === XQ (20 lines) ===
#pragma once
#include <xqMeshCommonExports.h>
#include <mitkAbstractFileIO.h>

class XQMESHCOMMON_EXPORT xq_MitkGridIO : public mitk::AbstractFileIO {
public:
    xq_MitkGridIO();
    using mitk::AbstractFileIO::Read;
protected:
    std::vector<mitk::BaseData::Pointer> DoRead() override;
    void Write() override;
private:
    [[nodiscard]] xq_MitkGridIO* IOClone() const override;
};

// === SV (73 lines, same structure after removing license header) ===
// Identical pattern: inherit AbstractFileIO, override DoRead/Write/IOClone
```

**Recommendations for Mesh:**
- `MitkGridIO`: Framework boilerplate — differentiate by supporting additional mesh formats (CGNS, OpenFOAM) or adding validation/metadata in the IO layer.
- `Grid`/`GridFactory`: Move toward a strategy pattern for mesh generation backends instead of mirroring SV's factory hierarchy.
- `TetGenGrid`: XQ at 139 lines vs SV's 550 shows significant trimming, but the interface pattern is the same. Consider wrapping TetGen behind an abstract mesher interface.

---

### 6. Simulation Module

| File Pair | XQ Lines | SV Lines | Similarity | Priority |
|-----------|---------|---------|-----------|----------|
| `xq_SolverUtils.cxx` ↔ `sv4gui_SimulationUtils.cxx` | 152 | 1161 | **49%** | 🟠 P2 |
| `xq_MitkSolverJobIO.h` ↔ `sv4gui_MitkSimJobIO.h` | 22 | 71 | **49%** | 🟠 P2 |
| `xq_MitkSolverJob.h` ↔ `sv4gui_MitkSimJob.h` | 62 | 108 | 43% | 🟡 P3 |
| `xq_SolverUtils.h` ↔ `sv4gui_SimulationUtils.h` | 32 | 76 | 43% | 🟡 P3 |
| `xq_SolverConfigWriter.h` ↔ `sv4gui_SimXmlWriter.h` | 56 | 160 | 42% | 🟡 P3 |
| `xq_SolverJob.cxx` ↔ `sv4gui_SimJob.cxx` | 217 | 155 | 35% | 🟢 OK |
| `xq_SolverJob.h` ↔ `sv4gui_SimJob.h` | 143 | 117 | 15% | 🟢 OK |

**Module average: 39%**

`SolverJob.h` at 15% is the most differentiated file in the entire comparison — XQ's solver job model has been substantially redesigned with a different parameter structure.

**Recommendations for Simulation:**
- `SolverUtils`: Already 87% smaller than SV. Ensure the remaining shared utility functions use different helper patterns.
- `MitkSolverJobIO.h`: Framework boilerplate — same approach as Mesh IO.
- Continue the strong divergence path already taken with `SolverJob`.

---

### 7. ProjectManagement Module

| File Pair | XQ Lines | SV Lines | Similarity | Priority |
|-----------|---------|---------|-----------|----------|
| `xq_DataNodeOperation.cxx` ↔ `sv4gui_DataNodeOperation.cxx` | 25 | 60 | **60%** | 🔴 P1 |
| `xq_DataFolder.cxx` ↔ `sv4gui_DataFolder.cxx` | 60 | 91 | **56%** | 🔴 P1 |
| `xq_ImageFolder.h` ↔ `sv4gui_ImageFolder.h` | 17 | 62 | **56%** | 🔴 P1 |
| `xq_PathFolder.h` ↔ `sv4gui_PathFolder.h` | 17 | 62 | **56%** | 🔴 P1 |
| `xq_DataNodeOperation.h` ↔ `sv4gui_DataNodeOperation.h` | 32 | 68 | **53%** | 🔴 P1 |
| `xq_DataFolder.h` ↔ `sv4gui_DataFolder.h` | 37 | 89 | **47%** | 🟠 P2 |
| `xq_WorkspaceManager.cxx` ↔ `sv4gui_ProjectManager.cxx` | 554 | 2268 | **45%** | 🟠 P2 |
| `xq_WorkspaceManager.h` ↔ `sv4gui_ProjectManager.h` | 74 | 276 | 39% | 🟡 P3 |

**Module average: 52% — highest of all modules**

**Highest-similarity example — `xq_ImageFolder.h` vs `sv4gui_ImageFolder.h`:**

```cpp
// === XQ (17 lines) ===
class XQPROJECTMANAGEMENT_EXPORT xq_ImageFolder : public xq_DataFolder {
public:
    mitkClassMacro(xq_ImageFolder, xq_DataFolder)
    itkFactorylessNewMacro(Self)
protected:
    xq_ImageFolder() { SetFolderName("Images"); SetFolderType("ImageFolder"); }
    ~xq_ImageFolder() override = default;
};

// === SV (62 lines, same pattern after license removal) ===
class SV4GUIMODULEPROJECTMANAGEMENT_EXPORT sv4guiImageFolder : public sv4guiDataFolder {
public:
    mitkClassMacro(sv4guiImageFolder, sv4guiDataFolder);
    itkFactorylessNewMacro(Self)
    itkCloneMacro(Self)
protected:
    mitkCloneMacro(Self);
    sv4guiImageFolder();
    sv4guiImageFolder(const sv4guiImageFolder &other);
    virtual ~sv4guiImageFolder();
};
```

These are functionally identical — XQ just uses modern C++ defaults.

**Recommendations for ProjectManagement:**
- `DataFolder` / `ImageFolder` / `PathFolder`: These are pure boilerplate. Replace with a **template-based folder system** (`xq_TypedFolder<ImageTag>`) that generates folder classes from tag types. This would eliminate per-folder files entirely.
- `DataNodeOperation`: Add XQ-specific operation types (workspace versioning, undo grouping) or merge into a command-pattern framework.
- `WorkspaceManager`: Already renamed and 75% smaller than SV's ProjectManager. Continue adding XQ-specific workspace features (multi-project, cloud sync).

---

## Priority Ranking: Files to Modify First

Files ranked by similarity (highest first). Target: reduce all to <10%.

| Rank | File (XQ) | Module | Similarity | Action |
|------|-----------|--------|-----------|--------|
| 1 | `xq_DataNodeOperation.cxx` | PM | **60%** | Add versioning metadata, command grouping |
| 2 | `xq_DataFolder.cxx` | PM | **56%** | Template-based folder system |
| 3 | `xq_ImageFolder.h` | PM | **56%** | Merge into template folder |
| 4 | `xq_PathFolder.h` | PM | **56%** | Merge into template folder |
| 5 | `xq_DataNodeOperation.h` | PM | **53%** | Add XQ-specific op types |
| 6 | `xq_MitkGridIO.h` | Mesh | **53%** | Add multi-format support |
| 7 | `xq_Spline.cxx` | Common | **53%** | New spline algorithm |
| 8 | `xq_CenterlineIO.h` | Path | **52%** | Add vessel metadata IO |
| 9 | `xq_Grid.cxx` | Mesh | **52%** | Strategy-pattern mesher |
| 10 | `xq_Spline.h` | Common | **51%** | New interface design |
| 11 | `xq_ModelDataInteractor.h` | Model | **51%** | Add vessel-specific interactions |
| 12 | `xq_VascularGeometry.cxx` | Model | **51%** | Native geometry representation |
| 13 | `xq_GeometryIO.h` | Model | **51%** | Multi-format persistence |
| 14 | `xq_PolyGeometry.cxx` | Model | **50%** | CGAL-based geometry |
| 15 | `xq_SolverUtils.cxx` | Sim | **49%** | Different helper patterns |
| 16 | `xq_MitkSolverJobIO.h` | Sim | **49%** | Solver-specific IO metadata |
| 17 | `xq_ProfileGroup.cxx` | Seg | **49%** | Branch-indexed storage |
| 18 | `xq_PolygonalProfile.cxx` | Seg | **49%** | Already using unique_ptr |
| 19 | `xq_LumenProfile.cxx` | Seg | **48%** | Different profile math |
| 20 | `xq_GridFactory.cxx` | Mesh | **48%** | Strategy pattern |

---

## Strategy Summary

### Systematic approaches to reduce similarity below 10%:

1. **Template-based MITK boilerplate** (affects ~15 files): Replace hand-written folder types, IO classes, and object factories with template/macro-generated code. This eliminates the entire category of "identical because MITK requires it" files.

2. **Algorithm replacement** (affects ~10 files): Where XQ uses the same mathematical approach (spline interpolation, contour centroid, mesh generation), switch to alternative algorithms or libraries (Eigen, CGAL, custom implementations).

3. **Interface redesign** (affects ~8 files): Add XQ-specific concepts (vessel branch indexing, workspace versioning, multi-solver support) that fundamentally change the API surface.

4. **Modern C++ idioms** (partially done, affects ~5 files): XQ already uses `unique_ptr`, `override`, `= default`, `[[nodiscard]]`. Continue replacing SV patterns (raw pointers, virtual destructors, copy constructors) with move semantics and value types.

5. **Framework abstraction** (affects ~7 files): Where both XQ and SV implement the same MITK interfaces (AbstractFileIO, DataInteractor, BaseData), add an intermediate XQ abstraction layer that provides XQ-specific base functionality.

---

*Generated by automated diff analysis. Similarity percentages are computed after normalizing `xq_`↔`sv4gui_` prefixes. Actual semantic similarity may differ from structural similarity.*
