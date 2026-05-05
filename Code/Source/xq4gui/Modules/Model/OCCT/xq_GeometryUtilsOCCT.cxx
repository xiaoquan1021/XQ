#include "xq_GeometryUtilsOCCT.h"

#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <BRep_Tool.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <Poly_Triangulation.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Pnt.hxx>
#include <gp_Circ.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Vec.hxx>

#include <BRepOffsetAPI_ThruSections.hxx>
#include <BRepOffsetAPI_MakePipe.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <Geom_BSplineCurve.hxx>
#include <GeomAPI_PointsToBSpline.hxx>
#include <TColgp_Array1OfPnt.hxx>

#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkTriangle.h>
#include <vtkAppendPolyData.h>

xq_GeometryUtilsOCCT::xq_GeometryUtilsOCCT()
{
}

xq_GeometryUtilsOCCT::~xq_GeometryUtilsOCCT()
{
}

TopoDS_Wire xq_GeometryUtilsOCCT::VtkPolyDataToWire(vtkPolyData* polydata)
{
    TopoDS_Wire wire;

    if (!polydata || polydata->GetNumberOfPoints() < 2)
    {
        return wire;
    }

    int nbPoints = static_cast<int>(polydata->GetNumberOfPoints());

    TColgp_Array1OfPnt points(1, nbPoints);
    for (int i = 0; i < nbPoints; i++)
    {
        double pt[3];
        polydata->GetPoint(i, pt);
        points.SetValue(i + 1, gp_Pnt(pt[0], pt[1], pt[2]));
    }

    GeomAPI_PointsToBSpline bsplineFitter(points);
    if (!bsplineFitter.IsDone())
    {
        return wire;
    }

    Handle(Geom_BSplineCurve) bspline = bsplineFitter.Curve();

    BRepBuilderAPI_MakeEdge edgeMaker(bspline);
    if (!edgeMaker.IsDone())
    {
        return wire;
    }

    BRepBuilderAPI_MakeWire wireMaker(edgeMaker.Edge());
    if (!wireMaker.IsDone())
    {
        return wire;
    }

    wire = wireMaker.Wire();
    return wire;
}

TopoDS_Shape xq_GeometryUtilsOCCT::LoftContours(const std::vector<vtkPolyData*>& contours)
{
    TopoDS_Shape result;

    if (contours.size() < 2)
    {
        return result;
    }

    BRepOffsetAPI_ThruSections loft(Standard_True);

    for (size_t i = 0; i < contours.size(); i++)
    {
        vtkPolyData* contour = contours[i];
        if (!contour || contour->GetNumberOfPoints() < 3)
        {
            continue;
        }

        int nbPoints = static_cast<int>(contour->GetNumberOfPoints());

        TColgp_Array1OfPnt points(1, nbPoints);
        for (int j = 0; j < nbPoints; j++)
        {
            double pt[3];
            contour->GetPoint(j, pt);
            points.SetValue(j + 1, gp_Pnt(pt[0], pt[1], pt[2]));
        }

        GeomAPI_PointsToBSpline bsplineFitter(points);
        if (!bsplineFitter.IsDone())
        {
            continue;
        }

        Handle(Geom_BSplineCurve) bspline = bsplineFitter.Curve();

        BRepBuilderAPI_MakeEdge edgeMaker(bspline);
        if (!edgeMaker.IsDone())
        {
            continue;
        }

        BRepBuilderAPI_MakeWire wireMaker(edgeMaker.Edge());
        if (!wireMaker.IsDone())
        {
            continue;
        }

        loft.AddWire(wireMaker.Wire());
    }

    loft.Build();

    if (loft.IsDone())
    {
        result = loft.Shape();
    }

    return result;
}

TopoDS_Shape xq_GeometryUtilsOCCT::CreatePipe(vtkPolyData* pathPolyData, double radius)
{
    TopoDS_Shape result;

    if (!pathPolyData || pathPolyData->GetNumberOfPoints() < 2 || radius <= 0.0)
    {
        return result;
    }

    TopoDS_Wire spine = VtkPolyDataToWire(pathPolyData);
    if (spine.IsNull())
    {
        return result;
    }

    // Get the start point and tangent direction of the spine
    double startPt[3];
    double nextPt[3];
    pathPolyData->GetPoint(0, startPt);
    pathPolyData->GetPoint(1, nextPt);

    gp_Pnt center(startPt[0], startPt[1], startPt[2]);
    gp_Dir direction(nextPt[0] - startPt[0],
                     nextPt[1] - startPt[1],
                     nextPt[2] - startPt[2]);

    gp_Ax2 axis(center, direction);
    gp_Circ circle(axis, radius);

    BRepBuilderAPI_MakeEdge circleEdge(circle);
    if (!circleEdge.IsDone())
    {
        return result;
    }

    BRepBuilderAPI_MakeWire profileWire(circleEdge.Edge());
    if (!profileWire.IsDone())
    {
        return result;
    }

    BRepBuilderAPI_MakeFace profileFace(profileWire.Wire());
    if (!profileFace.IsDone())
    {
        return result;
    }

    BRepOffsetAPI_MakePipe pipe(spine, profileFace.Face());
    pipe.Build();

    if (pipe.IsDone())
    {
        result = pipe.Shape();
    }

    return result;
}

vtkSmartPointer<vtkPolyData> xq_GeometryUtilsOCCT::TopoDS_ShapeToVtkPolyData(const TopoDS_Shape& shape)
{
    vtkSmartPointer<vtkPolyData> result = vtkSmartPointer<vtkPolyData>::New();

    if (shape.IsNull())
    {
        return result;
    }

    BRepMesh_IncrementalMesh mesh(shape, 0.1);
    mesh.Perform();

    vtkSmartPointer<vtkAppendPolyData> appendFilter = vtkSmartPointer<vtkAppendPolyData>::New();

    for (TopExp_Explorer explorer(shape, TopAbs_FACE); explorer.More(); explorer.Next())
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
    result->DeepCopy(appendFilter->GetOutput());

    return result;
}
