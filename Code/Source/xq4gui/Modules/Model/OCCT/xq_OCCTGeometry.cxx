#include "xq_OCCTGeometry.h"

#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepAlgoAPI_Fuse.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepFilletAPI_MakeFillet.hxx>
#include <BRepFilletAPI_MakeChamfer.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Pnt.hxx>
#include <TopExp.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopTools_IndexedDataMapOfShapeListOfShape.hxx>
#include <TopoDS_Edge.hxx>

#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkTriangle.h>
#include <vtkAppendPolyData.h>

#include <BRep_Builder.hxx>

#include <sstream>

xq_OCCTGeometry::xq_OCCTGeometry()
    : m_CacheDirty(true)
{
    setTypeTag("OCCT");
    m_CachedPolyData = nullptr;
}

xq_OCCTGeometry::~xq_OCCTGeometry()
{
}

std::unique_ptr<xq_VascularGeometry> xq_OCCTGeometry::Clone() const
{
    auto clone = std::make_unique<xq_OCCTGeometry>();
    if (!m_Shape.IsNull())
        clone->SetShape(m_Shape);
    return clone;
}

void xq_OCCTGeometry::SetShape(const TopoDS_Shape& shape)
{
    m_Shape = shape;
    UpdateFaceList();
    m_CacheDirty = true;
}

const TopoDS_Shape& xq_OCCTGeometry::GetShape() const
{
    return m_Shape;
}

void xq_OCCTGeometry::UpdateFaceList()
{
    clearFaces();
    auto& faces = facesMut();
    int faceIndex = 0;

    for (TopExp_Explorer explorer(m_Shape, TopAbs_FACE); explorer.More(); explorer.Next())
    {
        std::ostringstream oss;
        oss << "Face_" << faceIndex;

        faces.push_back({faceIndex, oss.str(), "OCCT_Face", true, 1.0f, {1.0f, 1.0f, 1.0f}});
        faceIndex++;
    }
}

vtkSmartPointer<vtkPolyData> xq_OCCTGeometry::TessellateFace(const TopoDS_Face& face)
{
    vtkSmartPointer<vtkPolyData> polyData = vtkSmartPointer<vtkPolyData>::New();

    BRepMesh_IncrementalMesh mesh(face, 0.1);
    mesh.Perform();

    TopLoc_Location location;
    Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);

    if (triangulation.IsNull())
    {
        return polyData;
    }

    int nbNodes = triangulation->NbNodes();
    int nbTriangles = triangulation->NbTriangles();

    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->SetNumberOfPoints(nbNodes);

    for (int i = 1; i <= nbNodes; i++)
    {
        gp_Pnt pt = triangulation->Node(i);
        pt.Transform(location.Transformation());
        points->SetPoint(i - 1, pt.X(), pt.Y(), pt.Z());
    }

    vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();

    for (int i = 1; i <= nbTriangles; i++)
    {
        int n1, n2, n3;
        triangulation->Triangle(i).Get(n1, n2, n3);

        if (face.Orientation() == TopAbs_REVERSED)
        {
            std::swap(n1, n2);
        }

        vtkSmartPointer<vtkTriangle> triangle = vtkSmartPointer<vtkTriangle>::New();
        triangle->GetPointIds()->SetId(0, n1 - 1);
        triangle->GetPointIds()->SetId(1, n2 - 1);
        triangle->GetPointIds()->SetId(2, n3 - 1);
        cells->InsertNextCell(triangle);
    }

    polyData->SetPoints(points);
    polyData->SetPolys(cells);

    return polyData;
}

void xq_OCCTGeometry::TessellateShape()
{
    BRepMesh_IncrementalMesh mesh(m_Shape, 0.1);
    mesh.Perform();

    vtkSmartPointer<vtkAppendPolyData> appendFilter = vtkSmartPointer<vtkAppendPolyData>::New();

    for (TopExp_Explorer explorer(m_Shape, TopAbs_FACE); explorer.More(); explorer.Next())
    {
        const TopoDS_Face& face = TopoDS::Face(explorer.Current());

        TopLoc_Location location;
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, location);

        if (triangulation.IsNull())
        {
            continue;
        }

        int nbNodes = triangulation->NbNodes();
        int nbTriangles = triangulation->NbTriangles();

        vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
        points->SetNumberOfPoints(nbNodes);

        for (int i = 1; i <= nbNodes; i++)
        {
            gp_Pnt pt = triangulation->Node(i);
            pt.Transform(location.Transformation());
            points->SetPoint(i - 1, pt.X(), pt.Y(), pt.Z());
        }

        vtkSmartPointer<vtkCellArray> cells = vtkSmartPointer<vtkCellArray>::New();

        for (int i = 1; i <= nbTriangles; i++)
        {
            int n1, n2, n3;
            triangulation->Triangle(i).Get(n1, n2, n3);

            if (face.Orientation() == TopAbs_REVERSED)
            {
                std::swap(n1, n2);
            }

            vtkSmartPointer<vtkTriangle> triangle = vtkSmartPointer<vtkTriangle>::New();
            triangle->GetPointIds()->SetId(0, n1 - 1);
            triangle->GetPointIds()->SetId(1, n2 - 1);
            triangle->GetPointIds()->SetId(2, n3 - 1);
            cells->InsertNextCell(triangle);
        }

        vtkSmartPointer<vtkPolyData> facePoly = vtkSmartPointer<vtkPolyData>::New();
        facePoly->SetPoints(points);
        facePoly->SetPolys(cells);

        appendFilter->AddInputData(facePoly);
    }

    appendFilter->Update();

    m_CachedPolyData = vtkSmartPointer<vtkPolyData>::New();
    m_CachedPolyData->DeepCopy(appendFilter->GetOutput());
    m_CacheDirty = false;
}

vtkSmartPointer<vtkPolyData> xq_OCCTGeometry::GetWholeVtkPolyData()
{
    if (m_CacheDirty || m_CachedPolyData == nullptr)
    {
        TessellateShape();
    }
    return m_CachedPolyData;
}

vtkSmartPointer<vtkPolyData> xq_OCCTGeometry::GetFaceVtkPolyData(int faceId)
{
    int currentIndex = 0;
    for (TopExp_Explorer explorer(m_Shape, TopAbs_FACE); explorer.More(); explorer.Next())
    {
        if (currentIndex == faceId)
        {
            const TopoDS_Face& face = TopoDS::Face(explorer.Current());
            return TessellateFace(face);
        }
        currentIndex++;
    }
    return vtkSmartPointer<vtkPolyData>::New();
}

void xq_OCCTGeometry::CreateFromBrep(const std::string& brepFile)
{
    TopoDS_Shape shape;
    BRep_Builder builder;

    if (BRepTools::Read(shape, brepFile.c_str(), builder))
    {
        SetShape(shape);
    }
}

xq_OCCTGeometry* xq_OCCTGeometry::BooleanUnion(xq_OCCTGeometry* a, xq_OCCTGeometry* b)
{
    if (!a || !b)
    {
        return nullptr;
    }

    BRepAlgoAPI_Fuse fuse(a->GetShape(), b->GetShape());
    fuse.Build();

    if (!fuse.IsDone())
    {
        return nullptr;
    }

    xq_OCCTGeometry* result = new xq_OCCTGeometry();
    result->SetShape(fuse.Shape());
    return result;
}

xq_OCCTGeometry* xq_OCCTGeometry::BooleanSubtract(xq_OCCTGeometry* a, xq_OCCTGeometry* b)
{
    if (!a || !b)
    {
        return nullptr;
    }

    BRepAlgoAPI_Cut cut(a->GetShape(), b->GetShape());
    cut.Build();

    if (!cut.IsDone())
    {
        return nullptr;
    }

    xq_OCCTGeometry* result = new xq_OCCTGeometry();
    result->SetShape(cut.Shape());
    return result;
}

xq_OCCTGeometry* xq_OCCTGeometry::BooleanIntersect(xq_OCCTGeometry* a, xq_OCCTGeometry* b)
{
    if (!a || !b)
    {
        return nullptr;
    }

    BRepAlgoAPI_Common common(a->GetShape(), b->GetShape());
    common.Build();

    if (!common.IsDone())
    {
        return nullptr;
    }

    xq_OCCTGeometry* result = new xq_OCCTGeometry();
    result->SetShape(common.Shape());
    return result;
}

void xq_OCCTGeometry::ApplyFillet(int faceId, double radius)
{
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(m_Shape, TopAbs_FACE, faceMap);

    if (faceId < 0 || faceId >= faceMap.Extent())
    {
        return;
    }

    const TopoDS_Face& targetFace = TopoDS::Face(faceMap(faceId + 1));

    TopTools_IndexedDataMapOfShapeListOfShape edgeFaceMap;
    TopExp::MapShapesAndAncestors(m_Shape, TopAbs_EDGE, TopAbs_FACE, edgeFaceMap);

    BRepFilletAPI_MakeFillet fillet(m_Shape);

    for (TopExp_Explorer edgeExp(targetFace, TopAbs_EDGE); edgeExp.More(); edgeExp.Next())
    {
        const TopoDS_Edge& edge = TopoDS::Edge(edgeExp.Current());
        fillet.Add(radius, edge);
    }

    fillet.Build();

    if (fillet.IsDone())
    {
        SetShape(fillet.Shape());
    }
}

void xq_OCCTGeometry::ApplyChamfer(int faceId, double distance)
{
    TopTools_IndexedMapOfShape faceMap;
    TopExp::MapShapes(m_Shape, TopAbs_FACE, faceMap);

    if (faceId < 0 || faceId >= faceMap.Extent())
    {
        return;
    }

    const TopoDS_Face& targetFace = TopoDS::Face(faceMap(faceId + 1));

    BRepFilletAPI_MakeChamfer chamfer(m_Shape);

    for (TopExp_Explorer edgeExp(targetFace, TopAbs_EDGE); edgeExp.More(); edgeExp.Next())
    {
        const TopoDS_Edge& edge = TopoDS::Edge(edgeExp.Current());
        chamfer.Add(distance, distance, edge, targetFace);
    }

    chamfer.Build();

    if (chamfer.IsDone())
    {
        SetShape(chamfer.Shape());
    }
}
