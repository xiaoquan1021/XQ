#ifndef XQ_GEOMETRYUTILSOCCT_H
#define XQ_GEOMETRYUTILSOCCT_H

#include <xqModelOCCTExports.h>

#include <vector>
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>

#include <TopoDS_Shape.hxx>
#include <TopoDS_Wire.hxx>

class XQMODELOCCT_EXPORT xq_GeometryUtilsOCCT
{
public:
    static TopoDS_Shape LoftContours(const std::vector<vtkPolyData*>& contours);

    static TopoDS_Shape CreatePipe(vtkPolyData* pathPolyData, double radius);

    static vtkSmartPointer<vtkPolyData> TopoDS_ShapeToVtkPolyData(const TopoDS_Shape& shape);

    static TopoDS_Wire VtkPolyDataToWire(vtkPolyData* polydata);

private:
    xq_GeometryUtilsOCCT();
    ~xq_GeometryUtilsOCCT();
};

#endif // XQ_GEOMETRYUTILSOCCT_H
