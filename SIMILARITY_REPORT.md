# XQ vs SimVascular Code Similarity Report

## Methodology

- Source files (.cxx, .h, .cpp) compared after normalization
- **Stripped**: comments, blank lines, includes, preprocessor guards
- **Naming normalized**: `xq_` → `sv4gui_`, `XQ` → `SV`, etc.
- **Two analyses performed**:
  1. **Raw similarity**: all code lines (includes boilerplate)
  2. **Meaningful similarity**: excludes C++/MITK/Qt boilerplate (braces, trivial statements, macros, lines < 10 chars)
- A line is 'similar' if it appears identically (post-normalization) in the SV codebase
- **Primary metric**: Meaningful similarity (excludes framework noise)

---
## Analysis 1: Raw Similarity (All Code Lines)

| Module | Raw Lines | Code Lines | Similar | Similarity % |
|--------|-----------|------------|---------|--------------|
| Common         |      1004 |        711 |     208 |        29.3% |
| Path           |      2136 |       1470 |     415 |        28.2% |
| Segmentation   |      5665 |       3893 |    1282 |        32.9% |
| Model          |      3070 |       2176 |     796 |        36.6% |
| Mesh           |      1614 |       1123 |     474 |        42.2% |
| Simulation     |      1404 |       1042 |     263 |        25.2% |
| ProjectMgmt    |      1975 |       1395 |     516 |        37.0% |
| Plugins        |     15275 |      11365 |    3308 |        29.1% |
|----------------|-----------|------------|---------|--------------|
| **TOTAL** | **32143** | **23175** | **7262** | **31.3%** |

> ⚠️ Raw similarity of 31.3% includes common C++/MITK/Qt patterns shared by any MITK-based project.

---
## Analysis 2: Meaningful Similarity (Excluding Boilerplate)

Boilerplate excluded: lone braces, trivial returns, macros (mitkClassMacro, itkNewMacro, etc.),
namespace/using statements, Qt/MITK registration macros, lines < 10 characters.

| Module | Raw Lines | Meaningful Lines | Similar | Similarity % |
|--------|-----------|------------------|---------|--------------|
| Common         |      1004 |              420 |       6 |         1.4% |
| Path           |      2136 |              826 |      16 |         1.9% |
| Segmentation   |      5665 |             2104 |     105 |         5.0% |
| Model          |      3070 |             1116 |      51 |         4.6% |
| Mesh           |      1614 |              542 |      44 |         8.1% |
| Simulation     |      1404 |              675 |      15 |         2.2% |
| ProjectMgmt    |      1975 |              673 |      22 |         3.3% |
| Plugins        |     15275 |             6930 |     322 |         4.6% |
|----------------|-----------|------------------|---------|--------------|
| **TOTAL** | **32143** | **13286** | **581** | **4.4%** |

### Plugin-Level Breakdown (Meaningful)

| XQ Plugin | vs SV Plugin(s) | Code Lines | Similar | Sim % |
|-----------|-----------------|------------|---------|-------|
| pipeline.vesselplanning | pathplanning |        596 |      16 |  2.7% |
| pipeline.lumencontouring | segmentation |        883 |      30 |  3.4% |
| pipeline.contouring3d | segmentation, mitksegmentation |        367 |      27 |  7.4% |
| pipeline.vascularmodeling | modeling |        816 |      37 |  4.5% |
| pipeline.gridgeneration | meshing |        821 |      18 |  2.2% |
| pipeline.hemodynamics | simulation |        720 |      21 |  2.9% |
| core.application | application |       1770 |     112 |  6.3% |
| core.datamanager | datamanager |        566 |      36 |  6.4% |
| core.workspace | projectmanager |        256 |      10 |  3.9% |
| data.projectnodes | projectdatanodes |        113 |       6 |  5.3% |
| data.pythonnodes | pythondatanodes |         22 |       9 | 40.9% |

## Files with >20% Meaningful Similarity (Flagged)

| File | Raw Lines | Meaningful Lines | Similar | Similarity % |
|------|-----------|------------------|---------|--------------|
| Plugins/org.xq.pipeline.lumencontouring/xq_SegmentationPlugin.h | 17 | 3 | 3 | 100.0% |
| Plugins/org.xq.pipeline.vascularmodeling/xq_ModelingPlugin.h | 17 | 3 | 3 | 100.0% |
| Plugins/org.xq.pipeline.gridgeneration/xq_MeshingPlugin.h | 17 | 3 | 3 | 100.0% |
| Plugins/org.xq.pipeline.hemodynamics/xq_SimulationPlugin.h | 17 | 3 | 3 | 100.0% |
| Plugins/org.xq.core.application/xq_Application.h | 18 | 3 | 3 | 100.0% |
| Plugins/org.xq.core.application/xq_ViewerPerspective.h | 16 | 2 | 2 | 100.0% |
| Plugins/org.xq.core.application/xq_VisualizationPerspective.h | 16 | 2 | 2 | 100.0% |
| Plugins/org.xq.core.datamanager/xq_DataExplorerPluginActivator.h | 17 | 3 | 3 | 100.0% |
| Plugins/org.xq.data.projectnodes/xq_ProjectDataNodesPluginActivator.h | 28 | 8 | 5 | 62.5% |
| Plugins/org.xq.data.pythonnodes/xq_PythonDataNodesPluginActivator.h | 29 | 8 | 5 | 62.5% |
| Plugins/org.xq.pipeline.contouring3d/xq_MitkSegmentationPlugin.h | 19 | 5 | 3 | 60.0% |
| Plugins/org.xq.pipeline.vascularmodeling/xq_ModelPreferencePage.h | 40 | 14 | 8 | 57.1% |
| Plugins/org.xq.core.application/xq_ApplicationPluginActivator.h | 26 | 7 | 4 | 57.1% |
| Plugins/org.xq.core.datamanager/xq_mitkIContextMenuAction.h | 30 | 9 | 5 | 55.6% |
| Plugins/org.xq.pipeline.vesselplanning/xq_PathPlanningPlugin.h | 23 | 6 | 3 | 50.0% |
| Plugins/org.xq.pipeline.hemodynamics/xq_SimulationPreferencePage.h | 40 | 14 | 7 | 50.0% |
| Plugins/org.xq.core.workspace/xq_WorkspacePluginActivator.h | 24 | 6 | 3 | 50.0% |
| Modules/Mesh/xq_MitkGridOperation.cxx | 19 | 9 | 4 | 44.4% |
| Modules/ProjectMgmt/xq_DataNodeOperation.cxx | 25 | 12 | 5 | 41.7% |
| Modules/Model/xq_GeometryIO.h | 17 | 3 | 1 | 33.3% |
| Modules/Mesh/xq_MitkGridIO.h | 20 | 3 | 1 | 33.3% |
| Plugins/org.xq.core.application/xq_ImportLegacyAction.h | 23 | 6 | 2 | 33.3% |
| Plugins/org.xq.core.application/xq_NewWorkspaceAction.h | 23 | 6 | 2 | 33.3% |
| Plugins/org.xq.core.application/xq_OpenWorkspaceAction.h | 23 | 6 | 2 | 33.3% |
| Plugins/org.xq.core.application/xq_SaveWorkspaceAction.h | 23 | 6 | 2 | 33.3% |
| Plugins/org.xq.core.datamanager/xq_DataExplorerPluginActivator.cxx | 14 | 3 | 1 | 33.3% |
| Plugins/org.xq.data.pythonnodes/xq_PythonDataNodesPluginActivator.cxx | 35 | 14 | 4 | 28.6% |
| Modules/Segmentation/xq_ProfileRenderer2D.h | 61 | 22 | 6 | 27.3% |
| Modules/Segmentation/xq_ProfileRenderer3D.h | 61 | 22 | 6 | 27.3% |
| Modules/Segmentation/xq_SegmentationObjectFactory.h | 30 | 11 | 3 | 27.3% |
| Modules/Model/xq_GeomRenderer2D.h | 43 | 11 | 3 | 27.3% |
| Modules/Model/xq_GeomRenderer3D.h | 42 | 11 | 3 | 27.3% |
| Modules/Mesh/xq_MitkGridObjectFactory.h | 30 | 11 | 3 | 27.3% |
| Plugins/org.xq.pipeline.vascularmodeling/xq_ExtractCenterlinesAction.h | 51 | 19 | 5 | 26.3% |
| Modules/Segmentation/xq_ContourModelVtkMapper2D.h | 59 | 20 | 5 | 25.0% |
| Modules/Segmentation/xq_MitkSeg3DIO.h | 18 | 4 | 1 | 25.0% |
| Modules/Segmentation/xq_SurfaceRenderer3D.h | 57 | 20 | 5 | 25.0% |
| Plugins/org.xq.pipeline.contouring3d/xq_MitkSegmentationView.h | 60 | 25 | 6 | 24.0% |
| Modules/Path/xq_PathObjectFactory.h | 42 | 13 | 3 | 23.1% |
| Plugins/org.xq.core.application/xq_Application.cxx | 24 | 9 | 2 | 22.2% |
| Modules/Model/xq_GeometryOp.cxx | 27 | 14 | 3 | 21.4% |

## Detailed Per-File Breakdown (Meaningful Similarity)

### Common

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_VtkParametricSpline.h | 32 | 8 | 1 | 12.5% |
| xq_XmlIOUtil.h | 37 | 15 | 1 | 6.7% |
| xq_XmlIOUtil.cxx | 93 | 49 | 3 | 6.1% |
| xq_VtkParametricSpline.cxx | 163 | 73 | 1 | 1.4% |
| xq_Math3.cxx | 202 | 93 | 0 | 0.0% |
| xq_Math3.h | 40 | 15 | 0 | 0.0% |
| xq_Spline.cxx | 39 | 14 | 0 | 0.0% |
| xq_Spline.h | 31 | 10 | 0 | 0.0% |
| xq_StringUtils.cxx | 90 | 31 | 0 | 0.0% |
| xq_StringUtils.h | 24 | 9 | 0 | 0.0% |
| xq_UndoHelper.cxx | 43 | 23 | 0 | 0.0% |
| xq_UndoHelper.h | 34 | 14 | 0 | 0.0% |
| xq_VtkUtils.cxx | 133 | 55 | 0 | 0.0% |
| xq_VtkUtils.h | 43 | 11 | 0 | 0.0% |

### Path

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_PathObjectFactory.h | 42 | 13 | 3 | 23.1% ⚠️ |
| xq_CenterlineIO.h | 20 | 5 | 1 | 20.0% |
| xq_CenterlineLegacyIO.cxx | 82 | 25 | 2 | 8.0% |
| xq_VesselTracer2D.h | 45 | 13 | 1 | 7.7% |
| xq_VesselTracer3D.h | 50 | 15 | 1 | 6.7% |
| xq_CenterlineSegment.h | 87 | 42 | 2 | 4.8% |
| xq_PathObjectFactory.cxx | 160 | 66 | 2 | 3.0% |
| xq_CenterlineIO.cxx | 201 | 79 | 2 | 2.5% |
| xq_VesselCenterline.cxx | 283 | 106 | 2 | 1.9% |
| xq_CenterlineInteractor.cxx | 250 | 96 | 0 | 0.0% |
| xq_CenterlineInteractor.h | 43 | 17 | 0 | 0.0% |
| xq_CenterlineLegacyIO.h | 23 | 4 | 0 | 0.0% |
| xq_CenterlineOp.cxx | 30 | 14 | 0 | 0.0% |
| xq_CenterlineOp.h | 44 | 19 | 0 | 0.0% |
| xq_CenterlineSegment.cxx | 279 | 116 | 0 | 0.0% |
| xq_VesselCenterline.h | 70 | 28 | 0 | 0.0% |
| xq_VesselTracer2D.cxx | 221 | 85 | 0 | 0.0% |
| xq_VesselTracer3D.cxx | 206 | 83 | 0 | 0.0% |

### Segmentation

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_ProfileRenderer2D.h | 61 | 22 | 6 | 27.3% ⚠️ |
| xq_ProfileRenderer3D.h | 61 | 22 | 6 | 27.3% ⚠️ |
| xq_SegmentationObjectFactory.h | 30 | 11 | 3 | 27.3% ⚠️ |
| xq_ContourModelVtkMapper2D.h | 59 | 20 | 5 | 25.0% ⚠️ |
| xq_MitkSeg3DIO.h | 18 | 4 | 1 | 25.0% ⚠️ |
| xq_SurfaceRenderer3D.h | 57 | 20 | 5 | 25.0% ⚠️ |
| xq_ContourGroup.cxx | 70 | 33 | 6 | 18.2% |
| xq_MitkSeg3DVtkMapper3D.h | 60 | 22 | 4 | 18.2% |
| xq_ContourGroupIO.h | 26 | 7 | 1 | 14.3% |
| xq_LumenSegIO.h | 27 | 7 | 1 | 14.3% |
| xq_ProfileRenderer3D.cxx | 206 | 75 | 10 | 13.3% |
| xq_SurfaceRenderer3D.cxx | 157 | 55 | 7 | 12.7% |
| xq_ContourModelVtkMapper2D.cxx | 196 | 74 | 8 | 10.8% |
| xq_ProfileRenderer2D.cxx | 277 | 102 | 9 | 8.8% |
| xq_ThresholdContour.cxx | 85 | 37 | 3 | 8.1% |
| xq_ContourGroup.h | 56 | 25 | 2 | 8.0% |
| xq_MitkSeg3DVtkMapper3D.cxx | 181 | 70 | 5 | 7.1% |
| xq_ProfileOp.cxx | 35 | 18 | 1 | 5.6% |
| xq_LumenSegIO.cxx | 263 | 95 | 4 | 4.2% |
| xq_LumenLegacyIO.cxx | 143 | 52 | 2 | 3.8% |
| xq_SplineProfile.cxx | 72 | 34 | 1 | 2.9% |
| xq_SegmentationObjectFactory.cxx | 204 | 69 | 2 | 2.9% |
| xq_MitkSeg3DIO.cxx | 223 | 72 | 2 | 2.8% |
| xq_SegmentationUtils.cxx | 362 | 119 | 3 | 2.5% |
| xq_LumenProfile.h | 103 | 41 | 1 | 2.4% |
| xq_EllipticProfile.cxx | 85 | 44 | 1 | 2.3% |
| xq_TensionProfile.cxx | 98 | 48 | 1 | 2.1% |
| xq_MitkSeg3DDataInteractor.cxx | 160 | 49 | 1 | 2.0% |
| xq_CircularProfile.cxx | 119 | 51 | 1 | 2.0% |
| xq_ProfileGroup.cxx | 248 | 69 | 1 | 1.4% |
| xq_ContourGroupIO.cxx | 182 | 72 | 1 | 1.4% |
| xq_Seg3DUtils.cxx | 282 | 120 | 1 | 0.8% |
| xq_CircularProfile.h | 20 | 6 | 0 | 0.0% |
| xq_EllipticProfile.h | 17 | 4 | 0 | 0.0% |
| xq_LumenLegacyIO.h | 19 | 5 | 0 | 0.0% |
| xq_LumenProfile.cxx | 155 | 65 | 0 | 0.0% |
| xq_LumenSurface.cxx | 89 | 28 | 0 | 0.0% |
| xq_LumenSurface.h | 43 | 14 | 0 | 0.0% |
| xq_MitkSeg3D.cxx | 148 | 46 | 0 | 0.0% |
| xq_MitkSeg3D.h | 61 | 27 | 0 | 0.0% |
| xq_MitkSeg3DDataInteractor.h | 37 | 14 | 0 | 0.0% |
| xq_MitkSeg3DOperation.cxx | 49 | 24 | 0 | 0.0% |
| xq_MitkSeg3DOperation.h | 48 | 21 | 0 | 0.0% |
| xq_PolygonalProfile.cxx | 39 | 15 | 0 | 0.0% |
| xq_PolygonalProfile.h | 17 | 5 | 0 | 0.0% |
| xq_ProfileGroup.h | 80 | 32 | 0 | 0.0% |
| xq_ProfileGroupInteractor.cxx | 209 | 79 | 0 | 0.0% |
| xq_ProfileGroupInteractor.h | 41 | 17 | 0 | 0.0% |
| xq_ProfileOp.h | 40 | 17 | 0 | 0.0% |
| xq_Seg3DUtils.h | 46 | 24 | 0 | 0.0% |
| xq_SegUndoActor.cxx | 26 | 8 | 0 | 0.0% |
| xq_SegUndoActor.h | 32 | 10 | 0 | 0.0% |
| xq_SegmentationUtils.h | 39 | 14 | 0 | 0.0% |
| xq_SplineProfile.h | 17 | 5 | 0 | 0.0% |
| xq_TensionProfile.h | 23 | 8 | 0 | 0.0% |
| xq_ThresholdContour.h | 36 | 12 | 0 | 0.0% |
| xq_ThresholdInteractor.cxx | 97 | 34 | 0 | 0.0% |
| xq_ThresholdInteractor.h | 31 | 11 | 0 | 0.0% |

### Model

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_GeometryIO.h | 17 | 3 | 1 | 33.3% ⚠️ |
| xq_GeomRenderer2D.h | 43 | 11 | 3 | 27.3% ⚠️ |
| xq_GeomRenderer3D.h | 42 | 11 | 3 | 27.3% ⚠️ |
| xq_GeometryOp.cxx | 27 | 14 | 3 | 21.4% ⚠️ |
| xq_GeometryFactory.cxx | 34 | 15 | 2 | 13.3% |
| xq_Model.cxx | 209 | 72 | 9 | 12.5% |
| xq_ModelObjectFactory.cxx | 122 | 46 | 5 | 10.9% |
| xq_VascularGeometry.h | 74 | 40 | 3 | 7.5% |
| xq_VascularGeometry.cxx | 211 | 83 | 6 | 7.2% |
| xq_GeomRenderer2D.cxx | 96 | 36 | 2 | 5.6% |
| xq_GeomRenderer3D.cxx | 191 | 74 | 4 | 5.4% |
| xq_GeometryLegacyIO.cxx | 67 | 20 | 1 | 5.0% |
| xq_ModelDataInteractor.cxx | 134 | 44 | 2 | 4.5% |
| xq_GeometryUtils.cxx | 268 | 105 | 3 | 2.9% |
| xq_GeometryUtilsOCCT.cxx | 271 | 84 | 2 | 2.4% |
| xq_GeometryIO.cxx | 188 | 73 | 1 | 1.4% |
| xq_AnalyticGeometry.cxx | 212 | 97 | 1 | 1.0% |
| xq_AnalyticGeometry.h | 41 | 12 | 0 | 0.0% |
| xq_GeometryFactory.h | 27 | 6 | 0 | 0.0% |
| xq_GeometryLegacyIO.h | 22 | 5 | 0 | 0.0% |
| xq_GeometryOp.h | 33 | 13 | 0 | 0.0% |
| xq_GeometryUtils.h | 46 | 23 | 0 | 0.0% |
| xq_Model.h | 57 | 24 | 0 | 0.0% |
| xq_ModelDataInteractor.h | 29 | 8 | 0 | 0.0% |
| xq_ModelObjectFactory.h | 37 | 12 | 0 | 0.0% |
| xq_PolyGeometry.cxx | 77 | 24 | 0 | 0.0% |
| xq_PolyGeometry.h | 28 | 8 | 0 | 0.0% |
| xq_RegisterPolyDataFunction.cxx | 17 | 5 | 0 | 0.0% |
| xq_RegisterPolyDataFunction.h | 5 | 1 | 0 | 0.0% |
| xq_GeometryUtilsOCCT.h | 29 | 4 | 0 | 0.0% |
| xq_OCCTGeometry.cxx | 350 | 121 | 0 | 0.0% |
| xq_OCCTGeometry.h | 48 | 18 | 0 | 0.0% |
| xq_RegisterOCCTFunction.cxx | 10 | 3 | 0 | 0.0% |
| xq_RegisterOCCTFunction.h | 8 | 1 | 0 | 0.0% |

### Mesh

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_MitkGridOperation.cxx | 19 | 9 | 4 | 44.4% ⚠️ |
| xq_MitkGridIO.h | 20 | 3 | 1 | 33.3% ⚠️ |
| xq_MitkGridObjectFactory.h | 30 | 11 | 3 | 27.3% ⚠️ |
| xq_MitkGridMapper3D.h | 39 | 10 | 2 | 20.0% |
| xq_MitkGridMapper2D.h | 44 | 12 | 2 | 16.7% |
| xq_MitkGrid.cxx | 202 | 49 | 7 | 14.3% |
| xq_GridFactory.cxx | 39 | 17 | 2 | 11.8% |
| xq_GridLegacyIO.cxx | 78 | 19 | 2 | 10.5% |
| xq_MitkGridMapper3D.cxx | 104 | 38 | 4 | 10.5% |
| xq_Grid.cxx | 92 | 39 | 4 | 10.3% |
| xq_GridAdaptor.h | 30 | 10 | 1 | 10.0% |
| xq_Grid.h | 75 | 36 | 3 | 8.3% |
| xq_MitkGridObjectFactory.cxx | 117 | 36 | 3 | 8.3% |
| xq_MitkGridIO.cxx | 177 | 71 | 4 | 5.6% |
| xq_MitkGridMapper2D.cxx | 104 | 42 | 2 | 4.8% |
| xq_GridAdaptor.cxx | 29 | 12 | 0 | 0.0% |
| xq_GridFactory.h | 28 | 6 | 0 | 0.0% |
| xq_GridLegacyIO.h | 16 | 3 | 0 | 0.0% |
| xq_MitkGrid.h | 45 | 16 | 0 | 0.0% |
| xq_MitkGridOperation.h | 27 | 9 | 0 | 0.0% |
| xq_TetGenAdaptor.cxx | 74 | 10 | 0 | 0.0% |
| xq_TetGenAdaptor.h | 24 | 9 | 0 | 0.0% |
| xq_TetGenGrid.cxx | 139 | 44 | 0 | 0.0% |
| xq_TetGenGrid.h | 62 | 31 | 0 | 0.0% |

### Simulation

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_MitkSolverObjectFactory.h | 43 | 15 | 3 | 20.0% |
| xq_MitkSolverJobIO.h | 22 | 6 | 1 | 16.7% |
| xq_MitkSolverJob.h | 62 | 29 | 3 | 10.3% |
| xq_MitkSolverJob.cxx | 110 | 44 | 4 | 9.1% |
| xq_MitkSolverObjectFactory.cxx | 82 | 32 | 2 | 6.2% |
| xq_MitkSolverJobIO.cxx | 282 | 128 | 2 | 1.6% |
| xq_SolverConfigWriter.cxx | 201 | 109 | 0 | 0.0% |
| xq_SolverConfigWriter.h | 56 | 25 | 0 | 0.0% |
| xq_SolverJob.cxx | 217 | 120 | 0 | 0.0% |
| xq_SolverJob.h | 145 | 88 | 0 | 0.0% |
| xq_SolverUtils.cxx | 152 | 66 | 0 | 0.0% |
| xq_SolverUtils.h | 32 | 13 | 0 | 0.0% |

### ProjectMgmt

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_DataNodeOperation.cxx | 25 | 12 | 5 | 41.7% ⚠️ |
| xq_DataNodeOperation.h | 32 | 10 | 2 | 20.0% |
| xq_DataNodeOperationInterface.h | 24 | 5 | 1 | 20.0% |
| xq_DataNodeOperationInterface.cxx | 69 | 14 | 1 | 7.1% |
| xq_LegacyImporter.cxx | 842 | 285 | 10 | 3.5% |
| xq_WorkspaceManager.cxx | 573 | 219 | 3 | 1.4% |
| xq_DataFolder.cxx | 60 | 21 | 0 | 0.0% |
| xq_DataFolder.h | 37 | 13 | 0 | 0.0% |
| xq_ImageFolder.h | 17 | 2 | 0 | 0.0% |
| xq_LegacyImporter.h | 77 | 37 | 0 | 0.0% |
| xq_MeshFolder.h | 17 | 2 | 0 | 0.0% |
| xq_ModelFolder.h | 17 | 2 | 0 | 0.0% |
| xq_MultiPhysicsFolder.h | 17 | 2 | 0 | 0.0% |
| xq_PathFolder.h | 17 | 2 | 0 | 0.0% |
| xq_ROMSimulationFolder.h | 17 | 2 | 0 | 0.0% |
| xq_RepositoryFolder.h | 17 | 2 | 0 | 0.0% |
| xq_SegmentationFolder.h | 17 | 2 | 0 | 0.0% |
| xq_SimulationFolder.h | 17 | 2 | 0 | 0.0% |
| xq_WorkspaceManager.h | 74 | 39 | 0 | 0.0% |

### Plugins (per-file)

#### pipeline.vesselplanning (2.7% overall)

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_PathPlanningPlugin.h | 23 | 6 | 3 | 50.0% ⚠️ |
| xq_PathCreate.cxx | 33 | 14 | 1 | 7.1% |
| xq_VesselPlanningView.h | 91 | 47 | 2 | 4.3% |
| xq_VesselPlanningView.cxx | 1033 | 430 | 10 | 2.3% |
| xq_CenterlineSmoother.cxx | 36 | 16 | 0 | 0.0% |
| xq_CenterlineSmoother.h | 26 | 6 | 0 | 0.0% |
| xq_PathCreate.h | 26 | 6 | 0 | 0.0% |
| xq_PathPlanningPlugin.cxx | 24 | 5 | 0 | 0.0% |
| xq_PathPreferencePage.cxx | 123 | 48 | 0 | 0.0% |
| xq_PathPreferencePage.h | 43 | 18 | 0 | 0.0% |

#### pipeline.lumencontouring (3.4% overall)

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_SegmentationPlugin.h | 17 | 3 | 3 | 100.0% ⚠️ |
| xq_Seg3DCreateAction.h | 35 | 11 | 2 | 18.2% |
| xq_ProfileGroupCreate.h | 28 | 7 | 1 | 14.3% |
| xq_LumenContouringView.h | 67 | 35 | 4 | 11.4% |
| xq_ProfileGroupCreate.cxx | 49 | 19 | 1 | 5.3% |
| xq_Seg3DCreateAction.cxx | 170 | 76 | 4 | 5.3% |
| xq_LumenContouringView.cxx | 1200 | 556 | 15 | 2.7% |
| xq_LoftParamWidget.cxx | 54 | 24 | 0 | 0.0% |
| xq_LoftParamWidget.h | 31 | 10 | 0 | 0.0% |
| xq_SegmentationPlugin.cxx | 16 | 3 | 0 | 0.0% |
| xq_SegmentationPreferencePage.cxx | 210 | 116 | 0 | 0.0% |
| xq_SegmentationPreferencePage.h | 48 | 23 | 0 | 0.0% |

#### pipeline.contouring3d (7.4% overall)

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_MitkSegmentationPlugin.h | 19 | 5 | 3 | 60.0% ⚠️ |
| xq_MitkSegmentationView.h | 60 | 25 | 6 | 24.0% ⚠️ |
| xq_MitkSegmentationView.cxx | 845 | 334 | 18 | 5.4% |
| xq_MitkSegmentationPlugin.cxx | 12 | 3 | 0 | 0.0% |

#### pipeline.vascularmodeling (4.5% overall)

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_ModelingPlugin.h | 17 | 3 | 3 | 100.0% ⚠️ |
| xq_ModelPreferencePage.h | 40 | 14 | 8 | 57.1% ⚠️ |
| xq_ExtractCenterlinesAction.h | 51 | 19 | 5 | 26.3% ⚠️ |
| xq_ModelCreate.h | 32 | 10 | 1 | 10.0% |
| xq_VascularModelingView.h | 57 | 24 | 2 | 8.3% |
| xq_ModelPreferencePage.cxx | 112 | 52 | 2 | 3.8% |
| xq_ModelCreate.cxx | 65 | 27 | 1 | 3.7% |
| xq_ExtractCenterlinesAction.cxx | 152 | 59 | 2 | 3.4% |
| xq_ModelFaceSelectionWidget.h | 70 | 34 | 1 | 2.9% |
| xq_VascularModelingView.cxx | 967 | 460 | 11 | 2.4% |
| xq_ModelFaceSelectionWidget.cxx | 242 | 111 | 1 | 0.9% |
| xq_ModelingPlugin.cxx | 16 | 3 | 0 | 0.0% |

#### pipeline.gridgeneration (2.2% overall)

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_MeshingPlugin.h | 17 | 3 | 3 | 100.0% ⚠️ |
| xq_MeshCreate.h | 29 | 8 | 1 | 12.5% |
| xq_LocalMeshSizeWidget.h | 55 | 22 | 2 | 9.1% |
| xq_GridGenerationView.h | 62 | 27 | 2 | 7.4% |
| xq_MeshCreate.cxx | 55 | 22 | 1 | 4.5% |
| xq_LocalMeshSizeWidget.cxx | 167 | 76 | 2 | 2.6% |
| xq_GridGenerationView.cxx | 1157 | 571 | 7 | 1.2% |
| xq_MeshPreferencePage.cxx | 139 | 73 | 0 | 0.0% |
| xq_MeshPreferencePage.h | 42 | 16 | 0 | 0.0% |
| xq_MeshingPlugin.cxx | 16 | 3 | 0 | 0.0% |

#### pipeline.hemodynamics (2.9% overall)

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_SimulationPlugin.h | 17 | 3 | 3 | 100.0% ⚠️ |
| xq_SimulationPreferencePage.h | 40 | 14 | 7 | 50.0% ⚠️ |
| xq_SimJobCreate.h | 28 | 7 | 1 | 14.3% |
| xq_SolverProcessHandler.h | 44 | 23 | 2 | 8.7% |
| xq_SimJobCreate.cxx | 50 | 20 | 1 | 5.0% |
| xq_HemodynamicsView.h | 55 | 26 | 1 | 3.8% |
| xq_CapBCWidget.h | 62 | 27 | 1 | 3.7% |
| xq_SolverProcessHandler.cxx | 147 | 55 | 2 | 3.6% |
| xq_CapBCWidget.cxx | 237 | 146 | 1 | 0.7% |
| xq_HemodynamicsView.cxx | 679 | 347 | 2 | 0.6% |
| xq_SimulationPlugin.cxx | 16 | 3 | 0 | 0.0% |
| xq_SimulationPreferencePage.cxx | 101 | 49 | 0 | 0.0% |

#### core.application (6.3% overall)

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_Application.h | 18 | 3 | 3 | 100.0% ⚠️ |
| xq_ViewerPerspective.h | 16 | 2 | 2 | 100.0% ⚠️ |
| xq_VisualizationPerspective.h | 16 | 2 | 2 | 100.0% ⚠️ |
| xq_ApplicationPluginActivator.h | 26 | 7 | 4 | 57.1% ⚠️ |
| xq_ImportLegacyAction.h | 23 | 6 | 2 | 33.3% ⚠️ |
| xq_NewWorkspaceAction.h | 23 | 6 | 2 | 33.3% ⚠️ |
| xq_OpenWorkspaceAction.h | 23 | 6 | 2 | 33.3% ⚠️ |
| xq_SaveWorkspaceAction.h | 23 | 6 | 2 | 33.3% ⚠️ |
| xq_Application.cxx | 24 | 9 | 2 | 22.2% ⚠️ |
| xq_WelcomePart.h | 22 | 5 | 1 | 20.0% |
| xq_Main.cxx | 27 | 11 | 2 | 18.2% |
| xq_AppWorkbenchAdvisor.h | 36 | 12 | 2 | 16.7% |
| xq_ViewerPerspective.cxx | 25 | 12 | 2 | 16.7% |
| xq_DefaultPerspective.h | 39 | 14 | 2 | 14.3% |
| xq_VisualizationPerspective.cxx | 30 | 14 | 2 | 14.3% |
| xq_AppWorkbenchAdvisor.cxx | 81 | 28 | 3 | 10.7% |
| xq_SaveWorkspaceAction.cxx | 105 | 43 | 4 | 9.3% |
| xq_WorkbenchWindowAdvisor.h | 132 | 85 | 7 | 8.2% |
| xq_ApplicationPluginActivator.cxx | 48 | 14 | 1 | 7.1% |
| xq_ImportLegacyAction.cxx | 101 | 43 | 3 | 7.0% |
| xq_WorkbenchWindowAdvisor.cxx | 2003 | 1043 | 56 | 5.4% |
| xq_OpenWorkspaceAction.cxx | 183 | 78 | 4 | 5.1% |
| xq_NewWorkspaceAction.cxx | 104 | 40 | 2 | 5.0% |
| xq_AboutDialog.cxx | 95 | 52 | 0 | 0.0% |
| xq_AboutDialog.h | 38 | 10 | 0 | 0.0% |
| xq_DefaultPerspective.cxx | 103 | 39 | 0 | 0.0% |
| xq_Main.h | 16 | 3 | 0 | 0.0% |
| xq_MitkApp.cxx | 52 | 13 | 0 | 0.0% |
| xq_MitkApp.h | 19 | 2 | 0 | 0.0% |
| xq_WelcomePart.cxx | 258 | 162 | 0 | 0.0% |

#### core.datamanager (6.4% overall)

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_DataExplorerPluginActivator.h | 17 | 3 | 3 | 100.0% ⚠️ |
| xq_mitkIContextMenuAction.h | 30 | 9 | 5 | 55.6% ⚠️ |
| xq_DataExplorerPluginActivator.cxx | 14 | 3 | 1 | 33.3% ⚠️ |
| xq_DataExplorerView.h | 98 | 43 | 8 | 18.6% |
| xq_DataExplorerView.cxx | 1177 | 508 | 19 | 3.7% |

#### core.workspace (3.9% overall)

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_WorkspacePluginActivator.h | 24 | 6 | 3 | 50.0% ⚠️ |
| xq_AddImageAction.h | 51 | 19 | 1 | 5.3% |
| xq_AddImageAction.cxx | 71 | 21 | 1 | 4.8% |
| xq_WorkspaceExplorer.cxx | 398 | 187 | 5 | 2.7% |
| xq_WorkspaceExplorer.h | 44 | 18 | 0 | 0.0% |
| xq_WorkspacePluginActivator.cxx | 22 | 5 | 0 | 0.0% |

#### data.projectnodes (5.3% overall)

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_ProjectDataNodesPluginActivator.h | 28 | 8 | 5 | 62.5% ⚠️ |
| xq_ProjectDataNodesPluginActivator.cxx | 157 | 103 | 1 | 1.0% |
| xq_DataNodeInit.cxx | 6 | 1 | 0 | 0.0% |
| xq_DataNodeInit.h | 10 | 1 | 0 | 0.0% |

#### data.pythonnodes (40.9% overall)

| File | Raw Lines | Code Lines | Similar | Sim % |
|------|-----------|------------|---------|-------|
| xq_PythonDataNodesPluginActivator.h | 29 | 8 | 5 | 62.5% ⚠️ |
| xq_PythonDataNodesPluginActivator.cxx | 35 | 14 | 4 | 28.6% ⚠️ |

## Sample Matched Lines (for verification)

These are examples of lines counted as 'similar' (post-normalization).

### Common

**xq_VtkParametricSpline.cxx**:
```cpp
mitk::Vector3D tangent;
```
**xq_VtkParametricSpline.h**:
```cpp
mitk::Vector3D tangent;
```
**xq_XmlIOUtil.cxx**:
```cpp
mitk::Point3D point;
return point;
return point;
```

### Path

**xq_CenterlineIO.cxx**:
```cpp
std::vector<mitk::Point3D> controlPoints;
return Unsupported;
```
**xq_CenterlineIO.h**:
```cpp
void Write() override;
```
**xq_CenterlineLegacyIO.cxx**:
```cpp
mitk::Point3D pt;
std::vector<mitk::Point3D> controlPoints;
```

### Segmentation

**xq_CircularProfile.cxx**:
```cpp
mitk::Point3D pt;
```
**xq_ContourGroup.cxx**:
```cpp
Superclass::InitializeTimeGeometry(1);
, m_PathName(other.m_PathName)
this->Modified();
```
**xq_ContourGroup.h**:
```cpp
std::vector<mitk::Point3D> points;
std::string m_PathName;
```

### Model

**xq_AnalyticGeometry.cxx**:
```cpp
return ids;
```
**xq_GeomRenderer2D.cxx**:
```cpp
bool visible = true;
float opacity = 1.0f;
```
**xq_GeomRenderer2D.h**:
```cpp
vtkSmartPointer<vtkActor> m_Actor;
itk::TimeStamp m_LastUpdateTime;
mitk::LocalStorageHandler<LocalStorage> m_LSH;
```

### Mesh

**xq_Grid.cxx**:
```cpp
return m_ModelElement;
return m_VolumeMesh;
return m_SurfaceMesh;
```
**xq_Grid.h**:
```cpp
vtkSmartPointer<vtkUnstructuredGrid> m_VolumeMesh;
vtkSmartPointer<vtkPolyData> m_SurfaceMesh;
std::string m_Type;
```
**xq_GridAdaptor.h**:
```cpp
virtual bool Adapt() = 0;
```

### Simulation

**xq_MitkSolverJob.cxx**:
```cpp
: mitk::BaseData(other)
, m_MeshName(other.m_MeshName)
, m_ModelName(other.m_ModelName)
```
**xq_MitkSolverJob.h**:
```cpp
std::string m_MeshName;
std::string m_ModelName;
std::string m_Status;
```
**xq_MitkSolverJobIO.cxx**:
```cpp
return mitk::IFileIO::Unsupported;
return mitk::IFileIO::Unsupported;
```

### ProjectMgmt

**xq_DataNodeOperation.cxx**:
```cpp
: mitk::Operation(operationType)
, m_DataNode(dataNode)
, m_ParentNode(parentNode)
```
**xq_DataNodeOperation.h**:
```cpp
mitk::DataNode::Pointer m_DataNode;
mitk::DataNode::Pointer m_ParentNode;
```
**xq_DataNodeOperationInterface.cxx**:
```cpp
return m_DataStorage;
```

---
## Summary

| Metric | Value |
|--------|-------|
| Total XQ raw source lines | 32143 |
| Total XQ meaningful code lines | 13286 |
| Lines similar to SV (meaningful) | 581 |
| **Overall meaningful similarity** | **4.4%** |
| Raw similarity (incl. boilerplate) | 31.3% |
| Files flagged (>20% meaningful sim) | 41 |
| **Target (≤10% meaningful)** | **✅ PASS** |

---

## Updated Analysis (Post Mesh Dedup Changes)

Re-analysis using the same methodology: strip comments/blanks/includes/guards, normalize naming
(xq_→sv4gui, XQ→SV, Grid→Mesh, etc.), remove boilerplate (lines <10 chars, lone braces, trivial
returns, macros, namespace/using, access specifiers, default constructors/destructors, forward
declarations, pure virtuals, simple setters/updates, class declarations with EXPORT). A line is
'similar' if it appears identically (post-normalization) in the SV codebase.

> **Note**: XQ lines are normalized to SV naming before comparison; SV lines are kept as-is.
> Boilerplate filtering uses original (pre-normalization) line lengths to avoid normalization
> inflating short identifiers past the 10-char threshold.

### Meaningful Similarity (Excluding Boilerplate)

| Module | Raw Lines | Meaningful Lines | Similar | Similarity % |
|--------|-----------|------------------|---------|--------------|
| Common         |      1004 |              422 |      12 |         2.8% |
| Path           |      2136 |              869 |      49 |         5.6% |
| Segmentation   |      5665 |             2180 |     143 |         6.6% |
| Model          |      3070 |             1149 |      84 |         7.3% |
| Mesh           |      1661 |              576 |      68 |        11.8% |
| Simulation     |      1404 |              697 |      36 |         5.2% |
| ProjectMgmt    |      1975 |              753 |      47 |         6.2% |
| Plugins        |     15275 |             7444 |     442 |         5.9% |
|----------------|-----------|------------------|---------|--------------|
| **TOTAL** | **32190** | **14090** | **881** | **6.3%** |

### Plugin-Level Breakdown (Meaningful)

| XQ Plugin | vs SV Plugin(s) | Code Lines | Similar | Sim % |
|-----------|-----------------|------------|---------|-------|
| pipeline.vesselplanning | pathplanning |        662 |      34 |  5.1% |
| pipeline.lumencontouring | segmentation |        958 |      49 |  5.1% |
| pipeline.contouring3d | segmentation, mitksegmentation |        437 |      34 |  7.8% |
| pipeline.vascularmodeling | modeling |        853 |      50 |  5.9% |
| pipeline.gridgeneration | meshing |        875 |      31 |  3.5% |
| pipeline.hemodynamics | simulation |        740 |      30 |  4.1% |
| core.application | application |       1897 |     137 |  7.2% |
| core.datamanager | datamanager |        632 |      40 |  6.3% |
| core.workspace | projectmanager |        264 |      13 |  4.9% |
| data.projectnodes | projectdatanodes |        112 |      11 |  9.8% |
| data.pythonnodes | pythondatanodes |         14 |      13 | 92.9% |

### Comparison with Previous Results

| Module | Previous % | Current % | Change |
|--------|-----------|-----------|--------|
| Common         |      1.4% |      2.8% | +1.4% |
| Path           |      1.9% |      5.6% | +3.7% |
| Segmentation   |      5.0% |      6.6% | +1.6% |
| Model          |      4.6% |      7.3% | +2.7% |
| Mesh           |      8.1% |     11.8% | +3.7% |
| Simulation     |      2.2% |      5.2% | +3.0% |
| ProjectMgmt    |      3.3% |      6.2% | +2.9% |
| Plugins        |      4.6% |      5.9% | +1.3% |
| **TOTAL**      |  **4.4%** |  **6.3%** | **+1.9%** |

> **Interpretation**: All modules show a modest increase in similarity (~1-4pp), attributable to
> refined boilerplate filtering (boilerplate check now uses pre-normalization line lengths, and
> additional patterns: access specifiers, default ctors/dtors, pure virtuals, forward declarations).
> The Mesh module increase (+3.7pp) is consistent with the average cross-module increase (+2.5pp),
> indicating the mesh dedup changes did **not** introduce significant new code similarity with SV.
> Mesh raw lines changed from 1614→1661 (+47 lines), with the module restructured into `Common/`
> subdirectory.

### Mesh Module Detail (Post-Dedup)

| File | Raw Lines | Meaningful Lines | Similar | Sim % |
|------|-----------|------------------|---------|-------|
| xq_MitkGridObjectFactory.h | 30 | 2 | 1 | 50.0% ⚠️ |
| xq_MitkGridOperation.cxx | 19 | 7 | 3 | 42.9% ⚠️ |
| xq_MitkGridMapper3D.cxx | 104 | 40 | 12 | 30.0% ⚠️ |
| xq_MitkGridMapper2D.cxx | 104 | 45 | 11 | 24.4% ⚠️ |
| xq_Grid.cxx | 92 | 31 | 7 | 22.6% ⚠️ |
| xq_MitkGrid.cxx | 175 | 79 | 15 | 19.0% |
| xq_MitkGridObjectFactory.cxx | 146 | 52 | 8 | 15.4% |
| xq_GridFactory.cxx | 39 | 16 | 2 | 12.5% |
| xq_GridLegacyIO.cxx | 78 | 20 | 2 | 10.0% |
| xq_MitkGridIO.cxx | 212 | 109 | 6 | 5.5% |
| xq_Grid.h | 75 | 29 | 1 | 3.4% |
| xq_GridAdaptor.cxx | 29 | 6 | 0 | 0.0% |
| xq_GridAdaptor.h | 30 | 10 | 0 | 0.0% |
| xq_GridFactory.h | 28 | 5 | 0 | 0.0% |
| xq_GridLegacyIO.h | 16 | 2 | 0 | 0.0% |
| xq_MitkGrid.h | 49 | 8 | 0 | 0.0% |
| xq_MitkGridIO.h | 20 | 0 | 0 | 0.0% |
| xq_MitkGridMapper2D.h | 44 | 0 | 0 | 0.0% |
| xq_MitkGridMapper3D.h | 39 | 0 | 0 | 0.0% |
| xq_MitkGridOperation.h | 27 | 7 | 0 | 0.0% |
| xq_TetGenAdaptor.cxx | 80 | 27 | 0 | 0.0% |
| xq_TetGenAdaptor.h | 24 | 6 | 0 | 0.0% |
| xq_TetGenGrid.cxx | 139 | 50 | 0 | 0.0% |
| xq_TetGenGrid.h | 62 | 25 | 0 | 0.0% |

### Updated Summary

| Metric | Previous | Current |
|--------|----------|---------|
| Total XQ raw source lines | 32143 | 32190 |
| Total XQ meaningful code lines | 13286 | 14090 |
| Lines similar to SV (meaningful) | 581 | 881 |
| **Overall meaningful similarity** | **4.4%** | **6.3%** |
| Mesh module similarity | 8.1% | 11.8% |
| Files flagged (>20% meaningful sim) | 41 | 47 |
| **Target (≤10% meaningful)** | **✅ PASS** | **✅ PASS** |

