#pragma once

#include <xqModelCommonExports.h>

#include <vector>

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>

namespace xq::geometry
{

[[nodiscard]] XQMODELCOMMON_EXPORT
vtkSmartPointer<vtkPolyData> booleanUnion(vtkPolyData* a, vtkPolyData* b);

[[nodiscard]] XQMODELCOMMON_EXPORT
vtkSmartPointer<vtkPolyData> booleanIntersection(vtkPolyData* a, vtkPolyData* b);

[[nodiscard]] XQMODELCOMMON_EXPORT
vtkSmartPointer<vtkPolyData> booleanSubtract(vtkPolyData* a, vtkPolyData* b);

[[nodiscard]] XQMODELCOMMON_EXPORT
vtkSmartPointer<vtkPolyData> smoothSurface(vtkPolyData* polyData,
                                            int iterations,
                                            double relaxation);

[[nodiscard]] XQMODELCOMMON_EXPORT
vtkSmartPointer<vtkPolyData> decimateSurface(vtkPolyData* polyData,
                                              double targetReduction);

[[nodiscard]] XQMODELCOMMON_EXPORT
vtkSmartPointer<vtkPolyData> fillHoles(vtkPolyData* polyData,
                                        double maxHoleSize);

[[nodiscard]] XQMODELCOMMON_EXPORT
vtkSmartPointer<vtkPolyData> loftContours(const std::vector<vtkPolyData*>& contours,
                                           int numPoints,
                                           int splineType);

[[nodiscard]] XQMODELCOMMON_EXPORT
vtkSmartPointer<vtkPolyData> computeNormals(vtkPolyData* polyData);

} // namespace xq::geometry

// Backward-compatible alias
using xq_GeometryUtils = struct xq_GeometryUtils_Deprecated;
