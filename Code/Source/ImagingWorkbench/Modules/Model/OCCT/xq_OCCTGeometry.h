#ifndef XQ_OCCTGEOMETRY_H
#define XQ_OCCTGEOMETRY_H

#include <xqModelOCCTExports.h>
#include "xq_VascularGeometry.h"

#include <string>
#include <vtkSmartPointer.h>
#include <vtkPolyData.h>

#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp_Explorer.hxx>

class XQMODELOCCT_EXPORT xq_OCCTGeometry : public xq_VascularGeometry
{
public:
    xq_OCCTGeometry();
    ~xq_OCCTGeometry() override;

    std::unique_ptr<xq_VascularGeometry> Clone() const override;

    vtkSmartPointer<vtkPolyData> GetWholeVtkPolyData() override;
    vtkSmartPointer<vtkPolyData> GetFaceVtkPolyData(int faceId) override;

    void SetShape(const TopoDS_Shape& shape);
    const TopoDS_Shape& GetShape() const;

    void CreateFromBrep(const std::string& brepFile);

    static xq_OCCTGeometry* BooleanUnion(xq_OCCTGeometry* a, xq_OCCTGeometry* b);
    static xq_OCCTGeometry* BooleanSubtract(xq_OCCTGeometry* a, xq_OCCTGeometry* b);
    static xq_OCCTGeometry* BooleanIntersect(xq_OCCTGeometry* a, xq_OCCTGeometry* b);

    void ApplyFillet(int faceId, double radius);
    void ApplyChamfer(int faceId, double distance);

private:
    void UpdateFaceList();
    void TessellateShape();
    vtkSmartPointer<vtkPolyData> TessellateFace(const TopoDS_Face& face);

    TopoDS_Shape m_Shape;
    vtkSmartPointer<vtkPolyData> m_CachedPolyData;
    bool m_CacheDirty;
};

#endif // XQ_OCCTGEOMETRY_H
