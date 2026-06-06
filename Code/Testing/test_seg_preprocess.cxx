// Regression test for preprocess-architecture slice 1.
//
// Tests:
//   1. test_write_sparse_profiles  — WriteContourElements must serialize ALL profiles
//      even when path-position indices are non-dense (e.g. {0, 5, 10}).
//      FAILS before fix  (ordinal loop only finds profile at position 0).
//      PASSES after fix  (path-index loop finds all three).
//
//   2. test_version_validation — IsVersionSupported must reject unknown versions.
//      FAILS before fix  (stub always returns true).
//      PASSES after fix  (returns true only for kFormatVersion).
//
//   3. test_profile_group_api_consistency — GetProfileCount / CountProfiles must agree;
//      GetProfileAtPathPos / FetchProfile must agree; GetProfilePathIndices correct.
//      PASSES before and after fix (documents stable API contract).
//
// Compile-time check: the deprecated aliases CountProfiles() and FetchProfile() are
// still callable (backward compat), but callers should migrate to the canonical names.

// Suppress deprecation warnings in the test itself — we intentionally call the
// deprecated aliases to verify backward compat is preserved.
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include "xq_ProfileGroup.h"
#include "xq_PipelineDataUtils.h"
#include "xq_SpatialMath.h"
#include "xq_CircularProfile.h"
#include "xq_EllipticProfile.h"
#include "xq_LumenSegIO.h"
#include "xq_SegmentationPipeline.h"
#include "xq_SegmentationAlgorithm.h"
#include "xq_ContourGroup.h"
#include "xq_ContourGroupMigration.h"
#include "xq_SegmentationUtils.h"
#include "xq_ThresholdContour.h"
#include "xq_PolygonalProfile.h"
#include "xq_SplineProfile.h"
#include "xq_VesselCenterline.h"
#include "xq_CenterlineSegment.h"
#include "xq_PathPipeline.h"
#include "xq_ModelPipeline.h"
#include "xq_Model.h"
#include "xq_MeshPipeline.h"
#include "xq_Grid.h"
#include "xq_MitkGrid.h"
#include "xq_SimulationPrepPipeline.h"
#include "xq_MitkSolverJob.h"
#include "xq_LegacyImporter.h"

#include <mitkDataNode.h>
#include <mitkPointSet.h>
#include <mitkBaseRenderer.h>
#include <mitkCoreObjectFactory.h>
#include <mitkStandaloneDataStorage.h>

#include <tinyxml2.h>
#include <vtkAppendPolyData.h>
#include <vtkCellArray.h>
#include <vtkImageData.h>
#include <vtkMatrix4x4.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkSphereSource.h>
#include <vtkCellData.h>
#include <vtkIntArray.h>
#include <vtkMassProperties.h>
#include <vtkTriangleFilter.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
constexpr double kPi = 3.141592653589793238462643383279502884;
}

static xq_CircularProfile* MakeCircle()
{
    return new xq_CircularProfile();
}

static xq_ProfilePlacementFrame MakeAxialFrame(int pathPosIndex, double z)
{
    xq_ProfilePlacementFrame frame;
    frame.pathPosIndex = pathPosIndex;
    frame.position[0] = 0.0;
    frame.position[1] = 0.0;
    frame.position[2] = z;
    frame.tangent.Fill(0.0);
    frame.tangent[2] = 1.0;
    frame.rotation.Fill(0.0);
    frame.rotation[0] = 1.0;
    return frame;
}

static mitk::Image::Pointer MakeThresholdSphereImage()
{
    auto vtkTemplate = vtkSmartPointer<vtkImageData>::New();
    vtkTemplate->SetDimensions(25, 25, 25);
    vtkTemplate->SetSpacing(1.0, 1.0, 1.0);
    vtkTemplate->SetOrigin(-12.0, -12.0, -12.0);
    vtkTemplate->AllocateScalars(VTK_DOUBLE, 1);

    auto image = mitk::Image::New();
    image->Initialize(vtkTemplate);

    auto* vtkImage = image->GetVtkImageData();
    if (!vtkImage)
        return image;

    vtkImage->SetDimensions(25, 25, 25);
    vtkImage->SetSpacing(1.0, 1.0, 1.0);
    vtkImage->SetOrigin(-12.0, -12.0, -12.0);

    if (!vtkImage->GetPointData() || !vtkImage->GetPointData()->GetScalars())
        vtkImage->AllocateScalars(VTK_DOUBLE, 1);

    for (int z = 0; z < 25; ++z)
    {
        for (int y = 0; y < 25; ++y)
        {
            for (int x = 0; x < 25; ++x)
            {
                const double worldX = -12.0 + static_cast<double>(x);
                const double worldY = -12.0 + static_cast<double>(y);
                const double worldZ = -12.0 + static_cast<double>(z);
                const double radiusSquared =
                    worldX * worldX + worldY * worldY + worldZ * worldZ;
                vtkImage->SetScalarComponentFromDouble(
                    x, y, z, 0, radiusSquared <= 36.0 ? 100.0 : 0.0);
            }
        }
    }

    return image;
}

static mitk::Image::Pointer MakeBentPathImage()
{
    auto vtkTemplate = vtkSmartPointer<vtkImageData>::New();
    vtkTemplate->SetDimensions(25, 25, 1);
    vtkTemplate->SetSpacing(1.0, 1.0, 1.0);
    vtkTemplate->SetOrigin(0.0, 0.0, 0.0);
    vtkTemplate->AllocateScalars(VTK_DOUBLE, 1);

    auto image = mitk::Image::New();
    image->Initialize(vtkTemplate);

    auto* vtkImage = image->GetVtkImageData();
    if (!vtkImage)
        return image;

    vtkImage->SetDimensions(25, 25, 1);
    vtkImage->SetSpacing(1.0, 1.0, 1.0);
    vtkImage->SetOrigin(0.0, 0.0, 0.0);

    if (!vtkImage->GetPointData() || !vtkImage->GetPointData()->GetScalars())
        vtkImage->AllocateScalars(VTK_DOUBLE, 1);

    for (int y = 0; y < 25; ++y)
    {
        for (int x = 0; x < 25; ++x)
        {
            const bool onHorizontal = (y == 10 && x >= 2 && x <= 18);
            const bool onVertical = (x == 18 && y >= 10 && y <= 18);
            vtkImage->SetScalarComponentFromDouble(
                x, y, 0, 0, (onHorizontal || onVertical) ? 1000.0 : 1.0);
        }
    }

    return image;
}

static mitk::Image::Pointer MakeOrientedThresholdEllipsoidImage()
{
    auto vtkTemplate = vtkSmartPointer<vtkImageData>::New();
    vtkTemplate->SetDimensions(41, 41, 41);
    vtkTemplate->SetSpacing(1.0, 1.0, 1.0);
    vtkTemplate->SetOrigin(0.0, 0.0, 0.0);
    vtkTemplate->AllocateScalars(VTK_DOUBLE, 1);

    auto image = mitk::Image::New();
    image->Initialize(vtkTemplate);

    auto* vtkImage = image->GetVtkImageData();
    if (!vtkImage)
        return image;

    vtkImage->SetDimensions(41, 41, 41);
    vtkImage->SetSpacing(1.0, 1.0, 1.0);
    vtkImage->SetOrigin(0.0, 0.0, 0.0);

    if (!vtkImage->GetPointData() || !vtkImage->GetPointData()->GetScalars())
        vtkImage->AllocateScalars(VTK_DOUBLE, 1);

    for (int z = 0; z < 41; ++z)
    {
        for (int y = 0; y < 41; ++y)
        {
            for (int x = 0; x < 41; ++x)
            {
                const double dx = (static_cast<double>(x) - 20.0) / 9.0;
                const double dy = (static_cast<double>(y) - 20.0) / 5.0;
                const double dz = (static_cast<double>(z) - 20.0) / 7.0;
                const double inside = dx * dx + dy * dy + dz * dz;
                vtkImage->SetScalarComponentFromDouble(
                    x, y, z, 0, inside <= 1.0 ? 100.0 : 0.0);
            }
        }
    }

    auto orientedIndexToWorld = vtkSmartPointer<vtkMatrix4x4>::New();
    orientedIndexToWorld->Identity();
    orientedIndexToWorld->SetElement(0, 0, 0.0);
    orientedIndexToWorld->SetElement(1, 0, 0.0);
    orientedIndexToWorld->SetElement(2, 0, -1.0);
    orientedIndexToWorld->SetElement(0, 1, 0.0);
    orientedIndexToWorld->SetElement(1, 1, 1.0);
    orientedIndexToWorld->SetElement(2, 1, 0.0);
    orientedIndexToWorld->SetElement(0, 2, 1.0);
    orientedIndexToWorld->SetElement(1, 2, 0.0);
    orientedIndexToWorld->SetElement(2, 2, 0.0);
    orientedIndexToWorld->SetElement(0, 3, 40.0);
    orientedIndexToWorld->SetElement(1, 3, -3.0);
    orientedIndexToWorld->SetElement(2, 3, 5.0);
    image->GetGeometry()->SetIndexToWorldTransformByVtkMatrix(orientedIndexToWorld);

    return image;
}

static int CountXmlContours(tinyxml2::XMLElement* contoursElem)
{
    int n = 0;
    for (auto* e = contoursElem->FirstChildElement("contour");
         e;
         e = e->NextSiblingElement("contour"))
    {
        ++n;
    }
    return n;
}

static mitk::DataNode::Pointer MakePathNode(const std::string& name)
{
    auto centerline = xq_VesselCenterline::New();
    auto node = mitk::DataNode::New();
    node->SetData(centerline);
    node->SetName(name);
    node->SetBoolProperty("xq.pathplanning.path", true);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::Path);
    return node;
}

static std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream in(path);
    return std::string(std::istreambuf_iterator<char>(in),
                       std::istreambuf_iterator<char>());
}

static bool test_legacy_import_preserves_distinct_final_suffix()
{
    const auto tmpDir = std::filesystem::temp_directory_path() /
                        "xq_legacy_import_final_suffix_contract";
    std::filesystem::remove_all(tmpDir);
    std::filesystem::create_directories(tmpDir);

    const auto pathFile = tmpDir / "artery.path";
    {
        std::ofstream os(pathFile);
        os <<
            "<path id=\"17\">"
            "  <timestep>"
            "    <path_element>"
            "      <control_points>"
            "        <point x=\"0\" y=\"0\" z=\"0\"/>"
            "        <point x=\"0\" y=\"0\" z=\"1\"/>"
            "      </control_points>"
            "    </path_element>"
            "  </timestep>"
            "</path>";
    }

    const auto contourFile = tmpDir / "artery.ctgr";
    {
        std::ofstream os(contourFile);
        os <<
            "<contourgroup path_name=\"artery_final\">"
            "  <timestep>"
            "    <contour>"
            "      <contour_points>"
            "        <point x=\"0\" y=\"0\" z=\"0\"/>"
            "        <point x=\"1\" y=\"0\" z=\"0\"/>"
            "        <point x=\"0\" y=\"1\" z=\"0\"/>"
            "      </contour_points>"
            "    </contour>"
            "  </timestep>"
            "</contourgroup>";
    }

    xq_LegacyImporter importer;
    if (!importer.ParsePathFile(pathFile.string()))
    {
        std::cerr << "FAIL test_legacy_import_preserves_distinct_final_suffix: path parse failed\n";
        std::filesystem::remove_all(tmpDir);
        return false;
    }

    auto contourNode = importer.ParseContourGroupFile(contourFile.string());
    if (contourNode.IsNull())
    {
        std::cerr << "FAIL test_legacy_import_preserves_distinct_final_suffix: contour parse failed\n";
        std::filesystem::remove_all(tmpDir);
        return false;
    }

    auto* group = dynamic_cast<xq_ProfileGroup*>(contourNode->GetData());
    if (!group || group->GetAttribute("path_name") != "artery_final")
    {
        std::cerr << "FAIL test_legacy_import_preserves_distinct_final_suffix: path name was normalized unexpectedly\n";
        std::filesystem::remove_all(tmpDir);
        return false;
    }

    std::filesystem::remove_all(tmpDir);
    std::cout << "PASS test_legacy_import_preserves_distinct_final_suffix\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 1 — WriteContourElements with sparse path positions
// ---------------------------------------------------------------------------

static bool test_write_sparse_profiles()
{
    auto group = xq_ProfileGroup::New();
    group->AppendProfile(MakeCircle(), 0);
    group->AppendProfile(MakeCircle(), 5);
    group->AppendProfile(MakeCircle(), 10);

    tinyxml2::XMLDocument doc;
    auto* root = doc.NewElement("xq_contour_group");
    doc.InsertEndChild(root);
    auto* contoursElem = doc.NewElement("contours");
    root->InsertEndChild(contoursElem);

    const int written = xq_LumenSegIO::WriteContourElements(group.GetPointer(), contoursElem, doc);

    if (written != 3)
    {
        std::cerr << "FAIL test_write_sparse_profiles: "
                  << "expected 3 contours written, got " << written << "\n"
                  << "  (Bug: Write() used ordinal loop FetchProfile(i) for i in 0..N-1;\n"
                  << "   sparse positions {0,5,10} — only position 0 hit, 1 and 2 miss.)\n";
        return false;
    }

    // Verify the XML element count matches
    const int xmlCount = CountXmlContours(contoursElem);
    if (xmlCount != 3)
    {
        std::cerr << "FAIL test_write_sparse_profiles: "
                  << "expected 3 <contour> elements in XML, found " << xmlCount << "\n";
        return false;
    }

    // Verify path_pos_index attributes are correct: must contain 0, 5, 10
    std::vector<int> foundIndices;
    for (auto* e = contoursElem->FirstChildElement("contour");
         e;
         e = e->NextSiblingElement("contour"))
    {
        int idx = -1;
        e->QueryIntAttribute("path_pos_index", &idx);
        foundIndices.push_back(idx);
    }
    const std::vector<int> expected = {0, 5, 10};
    if (foundIndices != expected)
    {
        std::cerr << "FAIL test_write_sparse_profiles: path_pos_index mismatch\n";
        return false;
    }

    std::cout << "PASS test_write_sparse_profiles\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 2 — Version validation
// ---------------------------------------------------------------------------

static bool test_version_validation()
{
    // Supported version must be accepted
    if (!xq_LumenSegIO::IsVersionSupported("1.0"))
    {
        std::cerr << "FAIL test_version_validation: version '1.0' should be supported\n";
        return false;
    }

    // Unknown versions must be rejected
    const std::vector<std::string> unsupported = {"2.0", "0.9", "99.0", "1.1", ""};
    for (const auto& v : unsupported)
    {
        if (xq_LumenSegIO::IsVersionSupported(v))
        {
            std::cerr << "FAIL test_version_validation: version '" << v
                      << "' should NOT be supported but IsVersionSupported returned true\n";
            return false;
        }
    }

    std::cout << "PASS test_version_validation\n";
    return true;
}

static bool test_contour_group_pipeline_contract()
{
    auto ds = mitk::StandaloneDataStorage::New();
    auto pathNode = MakePathNode("aorta_path");
    pathNode->SetStringProperty(xq::pipeline::kSourceImageProperty, "aorta_image");
    ds->Add(pathNode);

    const auto result = xq_SegmentationPipelineService::CreateContourGroup(
        ds, {"aorta_profiles", "aorta_path"});
    if (!result.ok || result.node.IsNull())
    {
        std::cerr << "FAIL test_contour_group_pipeline_contract: service did not create node\n";
        return false;
    }

    if (!xq::pipeline::HasStage(result.node, xq::pipeline::Stage::ContourGroup))
    {
        std::cerr << "FAIL test_contour_group_pipeline_contract: stage metadata missing\n";
        return false;
    }
    if (xq::pipeline::GetStringProperty(result.node, xq::pipeline::kAlgorithmProperty) != "manual" ||
        xq::pipeline::GetStringProperty(result.node, "xq.segmentation.method") != "manual")
    {
        std::cerr << "FAIL test_contour_group_pipeline_contract: manual algorithm metadata missing\n";
        return false;
    }
    if (xq::pipeline::GetStringProperty(result.node, xq::pipeline::kSourceImageProperty) != "aorta_image")
    {
        std::cerr << "FAIL test_contour_group_pipeline_contract: source image not inherited from path\n";
        return false;
    }
    double resliceSize = 0.0;
    if (!result.node->GetDoubleProperty("xq.segmentation.reslice_size", resliceSize) ||
        std::abs(resliceSize - 12.0) > 1e-9)
    {
        std::cerr << "FAIL test_contour_group_pipeline_contract: default reslice metadata missing\n";
        return false;
    }

    auto* profileGroup = dynamic_cast<xq_ProfileGroup*>(result.node->GetData());
    if (!profileGroup || profileGroup->GetAttribute("path_name") != "aorta_path")
    {
        std::cerr << "FAIL test_contour_group_pipeline_contract: path binding not written back\n";
        return false;
    }

    auto sources = ds->GetSources(result.node);
    if (!sources || sources->size() != 1 || sources->GetElement(0) != pathNode)
    {
        std::cerr << "FAIL test_contour_group_pipeline_contract: contour group should derive from the selected path node\n";
        return false;
    }

    std::cout << "PASS test_contour_group_pipeline_contract\n";
    return true;
}

static bool test_path_pipeline_vmtk_disabled_diagnostic()
{
    auto ds = mitk::StandaloneDataStorage::New();

    auto imageNode = mitk::DataNode::New();
    imageNode->SetName("path_source_image");
    imageNode->SetData(MakeBentPathImage());
    ds->Add(imageNode);

    xq_PathPlanRequest request;
    request.pathName = "vmtk_requested_path";
    request.imageNodeName = "path_source_image";
    request.algorithm = "vmtk_fastmarching";
    request.sampleCount = 12;
    request.smoothCurve = false;

    mitk::Point3D start;
    start.Fill(0.0);
    start[0] = 2.0;
    start[1] = 10.0;
    mitk::Point3D end;
    end.Fill(0.0);
    end[0] = 18.0;
    end[1] = 18.0;
    request.seeds = {start, end};

    const auto result = xq_PathPipelineService::CreatePath(ds, request);
    if (result.ok || result.node.IsNotNull())
    {
        std::cerr << "FAIL test_path_pipeline_vmtk_disabled_diagnostic: "
                  << "VMTK request must not create a fallback path\n";
        return false;
    }

    bool sawVmtkDiagnostic = false;
    for (const auto& d : result.diagnostics)
    {
        if (d.message.find("VMTK Fast Marching is not available") != std::string::npos &&
            d.message.find("Dijkstra") != std::string::npos)
        {
            sawVmtkDiagnostic = true;
        }
    }
    if (!sawVmtkDiagnostic)
    {
        std::cerr << "FAIL test_path_pipeline_vmtk_disabled_diagnostic: missing diagnostic\n";
        for (const auto& d : result.diagnostics)
            std::cerr << "  diag: " << d.message << "\n";
        return false;
    }

    xq_PathPlanRequest dijkstraRequest = request;
    dijkstraRequest.pathName = "dijkstra_path";
    dijkstraRequest.algorithm = "dijkstra";
    const auto dijkstraResult = xq_PathPipelineService::CreatePath(ds, dijkstraRequest);
    if (!dijkstraResult.ok || dijkstraResult.node.IsNull())
    {
        std::cerr << "FAIL test_path_pipeline_vmtk_disabled_diagnostic: explicit Dijkstra failed\n";
        for (const auto& d : dijkstraResult.diagnostics)
            std::cerr << "  diag: " << d.message << "\n";
        return false;
    }

    const std::string pipelineAlgorithm =
        xq::pipeline::GetStringProperty(dijkstraResult.node, xq::pipeline::kAlgorithmProperty);
    if (pipelineAlgorithm != "dijkstra")
    {
        std::cerr << "FAIL test_path_pipeline_vmtk_disabled_diagnostic: "
                  << "explicit Dijkstra path should record 'dijkstra', got '"
                  << pipelineAlgorithm << "'\n";
        return false;
    }
    std::string pathMethod;
    std::string paramsPathMethod;
    std::string pathUnitsLength;
    std::string requestedAlgorithm;
    std::string actualAlgorithm;
    double pathSpacing = 0.0;
    double pathPointSize = 0.0;
    int pathCalculationNumber = 0;
    int pathPointCount = 0;
    bool pathEditable = false;
    if (!dijkstraResult.node->GetStringProperty("xq.path.method", pathMethod) ||
        pathMethod != "dijkstra" ||
        !dijkstraResult.node->GetStringProperty("xq.params.path.method", paramsPathMethod) ||
        paramsPathMethod != "dijkstra" ||
        !dijkstraResult.node->GetStringProperty("xq.units.length", pathUnitsLength) ||
        pathUnitsLength != "mm" ||
        !dijkstraResult.node->GetStringProperty("xq.pathplanning.algorithm.requested", requestedAlgorithm) ||
        requestedAlgorithm != "dijkstra" ||
        !dijkstraResult.node->GetStringProperty("xq.pathplanning.algorithm.actual", actualAlgorithm) ||
        actualAlgorithm != "dijkstra" ||
        !dijkstraResult.node->GetDoubleProperty("xq.path.spacing", pathSpacing) ||
        std::abs(pathSpacing - dijkstraRequest.stepSize) > 1e-9 ||
        !dijkstraResult.node->GetDoubleProperty("xq.path.point_size", pathPointSize) ||
        pathPointSize <= 0.0 ||
        !dijkstraResult.node->GetIntProperty("xq.path.calculation_number", pathCalculationNumber) ||
        pathCalculationNumber != dijkstraRequest.sampleCount ||
        !dijkstraResult.node->GetIntProperty("xq.path.point_count", pathPointCount) ||
        pathPointCount <= 0 ||
        !dijkstraResult.node->GetBoolProperty("xq.path.editable", pathEditable) ||
        !pathEditable)
    {
        std::cerr << "FAIL test_path_pipeline_vmtk_disabled_diagnostic: "
                  << "path restore metadata missing or incorrect\n";
        return false;
    }

    auto* segment = dijkstraResult.centerline ? dijkstraResult.centerline->GetSegment() : nullptr;
    const auto anchors = segment ? segment->GetAnchorPositions() : std::vector<mitk::Point3D>();
    bool sawBend = false;
    int offCorridorCount = 0;
    for (const auto& point : anchors)
    {
        const int x = static_cast<int>(std::round(point[0]));
        const int y = static_cast<int>(std::round(point[1]));
        const bool onHorizontal = (y == 10 && x >= 2 && x <= 18);
        const bool onVertical = (x == 18 && y >= 10 && y <= 18);
        if (x == 18 && y == 10)
            sawBend = true;
        if (!onHorizontal && !onVertical)
            ++offCorridorCount;
    }
    if (!sawBend || offCorridorCount != 0)
    {
        std::cerr << "FAIL test_path_pipeline_vmtk_disabled_diagnostic: "
                  << "Dijkstra path should follow the bent bright channel; sawBend="
                  << sawBend << " offCorridorCount=" << offCorridorCount << "\n";
        return false;
    }

    std::cout << "PASS test_path_pipeline_vmtk_disabled_diagnostic\n";
    return true;
}

static bool test_model_mesh_simprep_pipeline_contract()
{
    auto ds = mitk::StandaloneDataStorage::New();
    auto pathNode = MakePathNode("iliac_path");
    ds->Add(pathNode);

    const auto contourResult = xq_SegmentationPipelineService::CreateContourGroup(
        ds, {"iliac_profiles", "iliac_path"});
    auto* group = dynamic_cast<xq_ProfileGroup*>(contourResult.node->GetData());
    if (!group)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: contour group missing\n";
        return false;
    }

    // XQ fix: mitk::Point3D default constructor does not zero-initialize,
    // so m_ProfileCenter is garbage after MakeCircle().  Explicitly set it
    // to origin before any other operation that reads the centre.
    auto* lower = MakeCircle();
    auto* upper = MakeCircle();
    lower->SetMethod("manual");
    upper->SetMethod("manual");
    {
        mitk::Point3D origin; origin.Fill(0.0);
        lower->SetProfileCenter(origin);
        upper->SetProfileCenter(origin);
    }
    // Larger radius reduces the height:radius aspect ratio so that
    // vtkDelaunay3D can successfully tetrahedralize the lofted cylinder.
    lower->SetRadius(3.0);
    upper->SetRadius(3.0);
    group->AppendProfile(lower, 0);
    group->AppendProfile(upper, 1);
    xq_SegmentationUtils::ApplyPlacementFrame(group->GetProfileAtPathPos(0), MakeAxialFrame(0, -4.0));
    xq_SegmentationUtils::ApplyPlacementFrame(group->GetProfileAtPathPos(1), MakeAxialFrame(1, 4.0));
    group->SetLoftedMesh(xq_SegmentationUtils::LoftProfileGroup(group));

    const auto modelResult = xq_ModelPipelineService::CreateModel(
        ds, {"iliac_model", "PolyData", 48});
    if (!modelResult.ok || modelResult.node.IsNull())
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: model service failed\n";
        for (const auto& d : modelResult.diagnostics)
            std::cerr << "  diag: " << d.message << "\n";
        return false;
    }

    auto* model = dynamic_cast<xq_Model*>(modelResult.node->GetData());
    auto* modelElement = model ? model->GetModelElement(0) : nullptr;
    auto modelPoly = modelElement ? modelElement->GetWholeVtkPolyData() : nullptr;
    if (!modelElement || !modelPoly || modelPoly->GetNumberOfCells() == 0)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: model geometry missing\n";
        return false;
    }

    std::string sourceContourGroups;
    std::string sourcePath;
    std::string modelType;
    std::string loftParams;
    std::string capInfo;
    int modelSampling = 0;
    int modelFaceCount = 0;
    bool modelQaOk = false;
    if (!modelResult.node->GetStringProperty("xq.source.contour_groups", sourceContourGroups) ||
        sourceContourGroups != "iliac_profiles" ||
        !modelResult.node->GetStringProperty("xq.source.path", sourcePath) ||
        sourcePath != "iliac_path" ||
        !modelResult.node->GetStringProperty("xq.model.type", modelType) ||
        modelType != "PolyData" ||
        !modelResult.node->GetIntProperty("xq.model.sampling", modelSampling) ||
        modelSampling != 48 ||
        !modelResult.node->GetStringProperty("xq.model.loft.parameters", loftParams) ||
        loftParams.find("sampling=48") == std::string::npos ||
        !modelResult.node->GetStringProperty("xq.model.cap_info", capInfo) ||
        capInfo.empty() ||
        !modelResult.node->GetBoolProperty("xq.model.qa.ok", modelQaOk) ||
        !modelQaOk ||
        !modelResult.node->GetIntProperty("xq.model.face_count", modelFaceCount) ||
        modelFaceCount < 1)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: model restore metadata missing"
                  << " sourceContourGroups='" << sourceContourGroups << "'"
                  << " sourcePath='" << sourcePath << "'"
                  << " modelType='" << modelType << "'"
                  << " modelSampling=" << modelSampling
                  << " loftParams='" << loftParams << "'"
                  << " capInfo='" << capInfo << "'"
                  << " modelQaOk=" << modelQaOk
                  << " modelFaceCount=" << modelFaceCount << "\n";
        return false;
    }

    auto* faceIds = vtkIntArray::SafeDownCast(modelPoly->GetCellData()->GetArray("FaceIds"));
    if (!faceIds || modelElement->GetFaceNumber() < 1)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: model face-id contract missing\n";
        return false;
    }

    xq_CreateModelRequest occtRequest;
    occtRequest.modelName = "iliac_model_occt_request";
    occtRequest.modelType = "PolyData";
    occtRequest.numSampling = 48;
    occtRequest.engine = "occt";
    const auto occtResult = xq_ModelPipelineService::CreateModel(ds, occtRequest);
    if (!occtResult.ok || occtResult.node.IsNull())
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: OCCT fallback diagnostic path failed\n";
        return false;
    }
    std::string modelAlgorithm;
    std::string requestedEngine;
    std::string actualEngine;
    bool modelFallback = false;
    if (!occtResult.node->GetStringProperty("xq.pipeline.algorithm", modelAlgorithm) ||
        modelAlgorithm != "vtk_fallback" ||
        !occtResult.node->GetBoolProperty("xq.model.algorithm.fallback", modelFallback) ||
        !modelFallback ||
        !occtResult.node->GetStringProperty("xq.model.requested_engine", requestedEngine) ||
        requestedEngine != "occt" ||
        !occtResult.node->GetStringProperty("xq.model.actual_engine", actualEngine) ||
        actualEngine != "vtk_fallback")
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: OCCT fallback metadata dishonest\n";
        return false;
    }

    const auto secondContourResult = xq_SegmentationPipelineService::CreateContourGroup(
        ds, {"renal_profiles", "iliac_path"});
    auto* secondGroup = dynamic_cast<xq_ProfileGroup*>(secondContourResult.node->GetData());
    if (!secondGroup)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: second contour group missing\n";
        return false;
    }
    auto* secondLower = MakeCircle();
    auto* secondUpper = MakeCircle();
    secondLower->SetMethod("manual");
    secondUpper->SetMethod("manual");
    {
        mitk::Point3D origin; origin.Fill(0.0);
        secondLower->SetProfileCenter(origin);
        secondUpper->SetProfileCenter(origin);
    }
    secondLower->SetRadius(2.0);
    secondUpper->SetRadius(2.0);
    secondGroup->AppendProfile(secondLower, 0);
    secondGroup->AppendProfile(secondUpper, 1);
    xq_SegmentationUtils::ApplyPlacementFrame(
        secondGroup->GetProfileAtPathPos(0), MakeAxialFrame(0, -3.0));
    xq_SegmentationUtils::ApplyPlacementFrame(
        secondGroup->GetProfileAtPathPos(1), MakeAxialFrame(1, 3.0));
    secondGroup->SetLoftedMesh(xq_SegmentationUtils::LoftProfileGroup(secondGroup));

    xq_CreateModelRequest selectedOnlyRequest;
    selectedOnlyRequest.modelName = "selected_only_model";
    selectedOnlyRequest.modelType = "PolyData";
    selectedOnlyRequest.numSampling = 48;
    selectedOnlyRequest.sourceContourGroupNames = {"iliac_profiles"};
    const auto selectedOnlyResult =
        xq_ModelPipelineService::CreateModel(ds, selectedOnlyRequest);
    std::string selectedOnlySources;
    if (!selectedOnlyResult.ok || selectedOnlyResult.node.IsNull() ||
        !selectedOnlyResult.node->GetStringProperty(
            "xq.source.contour_groups", selectedOnlySources) ||
        selectedOnlySources != "iliac_profiles")
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: selected contour groups not honored"
                  << " sources='" << selectedOnlySources << "'\n";
        return false;
    }

    xq_MeshGenerationRequest meshRequest;
    meshRequest.meshName = "iliac_mesh";
    meshRequest.globalEdgeSize = 2.5;
    meshRequest.localFaceSizes[faceIds->GetValue(0)] = 1.25;
    xq_RefinementRegion refinementRegion;
    refinementRegion.type = xq_RefinementRegion::Type::Sphere;
    refinementRegion.center = {0.0, 0.0, 5.0};
    refinementRegion.radiusOrSize = {2.0, 2.0, 2.0};
    refinementRegion.edgeSize = 0.75;
    meshRequest.refinementRegions.push_back(refinementRegion);
    meshRequest.boundaryLayerLayers = 2;
    meshRequest.boundaryLayerGrowthRate = 1.3;

    const auto meshResult = xq_MeshPipelineService::CreateVolumeMesh(
        ds, modelResult.node, meshRequest);
    if (!meshResult.ok || meshResult.node.IsNull())
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: mesh service failed\n";
        return false;
    }
    bool sawMeshCapabilityWarning = false;
    for (const auto& d : meshResult.diagnostics)
    {
        if (d.message.find("recorded") != std::string::npos)
            sawMeshCapabilityWarning = true;
    }
    if (!sawMeshCapabilityWarning)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: mesh capability warning missing\n";
        return false;
    }
    int localSizeCount = 0;
    int refinementCount = 0;
    bool localSizesApplied = true;
    bool refinementApplied = true;
    std::string meshCapabilityDiagnostic;
    std::string requestedBackend;
    std::string actualBackend;
    std::string localFaceSizeValues;
    std::string refinementRegionValues;
    bool backendFallback = false;
    bool preserveSurface = false;
    double blFirstHeight = 0.0;
    double minDihedral = 0.0;
    double maxEdgeSize = -1.0;
    if (!meshResult.node->GetIntProperty("xq.mesh.local_face_sizes", localSizeCount) ||
        localSizeCount != 1 ||
        !meshResult.node->GetIntProperty("xq.mesh.refinement_regions", refinementCount) ||
        refinementCount != 1 ||
        !meshResult.node->GetBoolProperty("xq.mesh.local_face_sizes.applied", localSizesApplied) ||
        localSizesApplied ||
        !meshResult.node->GetBoolProperty("xq.mesh.refinement_regions.applied", refinementApplied) ||
        refinementApplied ||
        !meshResult.node->GetStringProperty("xq.mesh.requested_backend", requestedBackend) ||
        requestedBackend != "tetgen" ||
        !meshResult.node->GetStringProperty("xq.mesh.actual_backend", actualBackend) ||
        actualBackend != "vtk_delaunay3d_fallback" ||
        !meshResult.node->GetBoolProperty("xq.mesh.backend.fallback", backendFallback) ||
        !backendFallback ||
        !meshResult.node->GetStringProperty("xq.mesh.local_face_sizes.values", localFaceSizeValues) ||
        localFaceSizeValues.find(":1.25") == std::string::npos ||
        !meshResult.node->GetStringProperty("xq.mesh.refinement_regions.values", refinementRegionValues) ||
        refinementRegionValues.find("Sphere,0,0,5,2,2,2,0.75") == std::string::npos ||
        !meshResult.node->GetBoolProperty("xq.mesh.preserve_surface", preserveSurface) ||
        !preserveSurface ||
        !meshResult.node->GetDoubleProperty("xq.mesh.bl.firstHeight", blFirstHeight) ||
        std::abs(blFirstHeight - meshRequest.boundaryLayerFirstHeight) > 1e-9 ||
        !meshResult.node->GetDoubleProperty("xq.mesh.min_dihedral", minDihedral) ||
        std::abs(minDihedral - meshRequest.minDihedral) > 1e-9 ||
        !meshResult.node->GetDoubleProperty("xq.mesh.max_edge_size", maxEdgeSize) ||
        std::abs(maxEdgeSize - meshRequest.maxEdgeSize) > 1e-9 ||
        !meshResult.node->GetStringProperty("xq.mesh.capability.diagnostic", meshCapabilityDiagnostic) ||
        meshCapabilityDiagnostic.find("not fully applied") == std::string::npos)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: mesh parameter capability metadata missing\n";
        return false;
    }

    auto* mitkGrid = dynamic_cast<xq_MitkGrid*>(meshResult.node->GetData());
    auto* mesh = mitkGrid ? mitkGrid->GetMesh(0) : nullptr;
    if (!mesh || !mesh->GetVolumeMesh() || mesh->GetVolumeMesh()->GetNumberOfCells() == 0)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: volume mesh missing\n";
        return false;
    }

    auto meshSources = ds->GetSources(meshResult.node);
    if (!meshSources || meshSources->size() != 1 || meshSources->GetElement(0) != modelResult.node)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: mesh node should derive from the model node\n";
        return false;
    }

    modelResult.node->SetBoolProperty("xq.model.qa.ok", false);
    const auto blockedMeshResult = xq_MeshPipelineService::CreateVolumeMesh(
        ds, modelResult.node, meshRequest);
    if (blockedMeshResult.ok)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: bad model QA should block meshing\n";
        return false;
    }
    modelResult.node->SetBoolProperty("xq.model.qa.ok", true);

    xq_SimulationPrepRequest simRequest;
    simRequest.jobName = "iliac_job";
    simRequest.numTimesteps = 120;
    simRequest.timeStepSize = 0.002;
    simRequest.numCycles = 3;

    const auto simResult = xq_SimulationPrepPipelineService::CreateOrUpdateSimulationPrep(
        ds, modelResult.node, meshResult.node, simRequest);
    if (!simResult.ok || simResult.node.IsNull())
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: simulation prep service failed\n";
        return false;
    }

    auto* simJob = dynamic_cast<xq_MitkSolverJob*>(simResult.node->GetData());
    if (!simJob || simJob->GetMeshName() != "iliac_mesh" || simJob->GetModelName() != "iliac_model")
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: simulation prep bindings missing\n";
        return false;
    }

    if (!xq::pipeline::HasStage(simResult.node, xq::pipeline::Stage::SimulationPrep))
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: simulation prep stage metadata missing\n";
        return false;
    }

    std::string solverType;
    std::string simUnitsLength;
    std::string simUnitsTime;
    std::string simWallModel;
    bool deformableWall = true;
    double fluidDensity = 0.0;
    double fluidViscosity = 0.0;
    double initialPressure = -1.0;
    double initialVelocity = -1.0;
    double wallThickness = 0.0;
    double wallElasticModulus = 0.0;
    double wallPoissonRatio = 0.0;
    double wallDensity = 0.0;
    int numTimesteps = 0;
    int numCycles = 0;
    int numLinearIterations = 0;
    int numNonlinearIterations = 0;
    int bcCount = 0;
    int faceRoleCount = 0;
    int paramsNumSteps = 0;
    int paramsNumCycles = 0;
    if (!simResult.node->GetStringProperty("xq.sim.solver_type", solverType) ||
        solverType != "xq_export_only" ||
        !simResult.node->GetStringProperty("xq.units.length", simUnitsLength) ||
        simUnitsLength != "mm" ||
        !simResult.node->GetStringProperty("xq.units.time", simUnitsTime) ||
        simUnitsTime != "s" ||
        !simResult.node->GetStringProperty("xq.params.simulation.wall_model", simWallModel) ||
        simWallModel != "rigid" ||
        !simResult.node->GetBoolProperty("xq.sim.deformable_wall", deformableWall) ||
        deformableWall ||
        !simResult.node->GetDoubleProperty("xq.sim.fluid_density", fluidDensity) ||
        std::abs(fluidDensity - simRequest.fluidDensity) > 1e-9 ||
        !simResult.node->GetDoubleProperty("xq.sim.fluid_viscosity", fluidViscosity) ||
        std::abs(fluidViscosity - simRequest.fluidViscosity) > 1e-9 ||
        !simResult.node->GetDoubleProperty("xq.sim.initial_pressure", initialPressure) ||
        std::abs(initialPressure - simRequest.initialPressure) > 1e-9 ||
        !simResult.node->GetDoubleProperty("xq.sim.initial_velocity", initialVelocity) ||
        std::abs(initialVelocity - simRequest.initialVelocity) > 1e-9 ||
        !simResult.node->GetDoubleProperty("xq.sim.wall_thickness", wallThickness) ||
        std::abs(wallThickness - simRequest.wallThickness) > 1e-9 ||
        !simResult.node->GetDoubleProperty("xq.sim.wall_elastic_modulus", wallElasticModulus) ||
        std::abs(wallElasticModulus - simRequest.wallElasticModulus) > 1e-6 ||
        !simResult.node->GetDoubleProperty("xq.sim.wall_poisson_ratio", wallPoissonRatio) ||
        std::abs(wallPoissonRatio - simRequest.wallPoissonRatio) > 1e-9 ||
        !simResult.node->GetDoubleProperty("xq.sim.wall_density", wallDensity) ||
        std::abs(wallDensity - simRequest.wallDensity) > 1e-9 ||
        !simResult.node->GetIntProperty("xq.sim.num_timesteps", numTimesteps) ||
        numTimesteps != simRequest.numTimesteps ||
        !simResult.node->GetIntProperty("xq.sim.num_cycles", numCycles) ||
        numCycles != simRequest.numCycles ||
        !simResult.node->GetIntProperty("xq.sim.num_linear_iterations", numLinearIterations) ||
        numLinearIterations != simRequest.numLinearIterations ||
        !simResult.node->GetIntProperty("xq.sim.num_nonlinear_iterations", numNonlinearIterations) ||
        numNonlinearIterations != simRequest.numNonlinearIterations ||
        !simResult.node->GetIntProperty("xq.sim.bc_count", bcCount) ||
        bcCount == 0 ||
        !simResult.node->GetIntProperty("xq.simprep.face_role_count", faceRoleCount) ||
        faceRoleCount == 0 ||
        !simResult.node->GetIntProperty("xq.params.simulation.num_steps", paramsNumSteps) ||
        paramsNumSteps != simRequest.numTimesteps ||
        !simResult.node->GetIntProperty("xq.params.simulation.num_cycles", paramsNumCycles) ||
        paramsNumCycles != simRequest.numCycles)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: simulation prep restore metadata missing\n";
        return false;
    }

    meshResult.node->SetBoolProperty("xq.mesh.qa.ok", false);
    const auto blockedSimResult =
        xq_SimulationPrepPipelineService::CreateOrUpdateSimulationPrep(
            ds, modelResult.node, meshResult.node, simRequest);
    if (blockedSimResult.ok)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: bad mesh QA should block simulation prep\n";
        return false;
    }
    meshResult.node->SetBoolProperty("xq.mesh.qa.ok", true);

    xq_SimulationPrepRequest invalidBcRequest = simRequest;
    xq_BoundaryCondition invalidBc;
    invalidBc.faceName = "definitely_missing_face";
    invalidBc.faceRole = "outflow";
    invalidBc.bcType = "resistance";
    invalidBcRequest.boundaryConditions.push_back(invalidBc);
    const auto invalidBcResult =
        xq_SimulationPrepPipelineService::CreateOrUpdateSimulationPrep(
            ds, modelResult.node, meshResult.node, invalidBcRequest);
    if (invalidBcResult.ok)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: invalid BC should fail\n";
        return false;
    }
    bool sawBcDiagnostic = false;
    for (const auto& d : invalidBcResult.diagnostics)
    {
        if (d.message.find("unknown face") != std::string::npos ||
            d.message.find("Boundary condition") != std::string::npos)
            sawBcDiagnostic = true;
    }
    if (!sawBcDiagnostic)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: invalid BC diagnostic missing\n";
        return false;
    }

    const std::filesystem::path exportDir =
        std::filesystem::temp_directory_path() / "xq_simprep_export_contract";
    std::filesystem::remove_all(exportDir);
    xq_SimulationExportRequest exportRequest;
    exportRequest.outputDir = exportDir.string();
    exportRequest.inletWaveform = {{0.0, 1.0}, {0.5, 2.0}, {1.0, 1.0}};
    const auto exportResult = xq_SimulationPrepPipelineService::ExportForSolver(
        ds, simResult.node, exportRequest);
    if (!exportResult.ok || exportResult.filesWritten.empty())
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: solver export failed\n";
        std::filesystem::remove_all(exportDir);
        return false;
    }
    std::string simStatus;
    const bool exportHasWarnings = !exportResult.diagnostics.empty();
    const std::string expectedExportStatus =
        exportHasWarnings ? "exported_with_warnings" : "exported";
    if (!simResult.node->GetStringProperty("xq.sim.status", simStatus) ||
        simStatus != expectedExportStatus)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: export status not recorded\n";
        std::filesystem::remove_all(exportDir);
        return false;
    }
    if (simJob->GetStatus() != expectedExportStatus)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: mitk solver job status not exported\n";
        std::filesystem::remove_all(exportDir);
        return false;
    }
    if (exportHasWarnings)
    {
        std::string exportWarnings;
        if (!simResult.node->GetStringProperty("xq.sim.export_warnings", exportWarnings) ||
            exportWarnings.empty())
        {
            std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: export warning metadata missing\n";
            std::filesystem::remove_all(exportDir);
            return false;
        }
    }
    int fileCount = 0;
    if (!simResult.node->GetIntProperty("xq.sim.files_written_count", fileCount) ||
        fileCount != static_cast<int>(exportResult.filesWritten.size()))
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: export file count metadata mismatch\n";
        std::filesystem::remove_all(exportDir);
        return false;
    }
    std::string filesCsv;
    if (!simResult.node->GetStringProperty("xq.sim.files_written", filesCsv) ||
        filesCsv.find("solver.inp") == std::string::npos ||
        filesCsv.find("bct.dat") == std::string::npos)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: export file list metadata missing\n";
        std::filesystem::remove_all(exportDir);
        return false;
    }
    const auto bctText = ReadTextFile(exportDir / "bct.dat");
    if (bctText.find("3 10") == std::string::npos ||
        bctText.find("0.5 2") == std::string::npos)
    {
        std::cerr << "FAIL test_model_mesh_simprep_pipeline_contract: waveform bct.dat content missing\n";
        std::filesystem::remove_all(exportDir);
        return false;
    }
    std::filesystem::remove_all(exportDir);

    std::cout << "PASS test_model_mesh_simprep_pipeline_contract\n";
    return true;
}

static bool test_sv_project_import_creates_vascular_nodes()
{
    const auto projectRoot = std::filesystem::temp_directory_path() /
                             "xq_legacy_sv_project_import_contract";
    std::filesystem::remove_all(projectRoot);
    std::filesystem::create_directories(projectRoot / "Paths");
    std::filesystem::create_directories(projectRoot / "Segmentations");

    {
        std::ofstream os(projectRoot / "Paths" / "aorta.pth");
        os <<
            "<path id=\"17\">"
            "  <timestep>"
            "    <path_element>"
            "      <control_points>"
            "        <point x=\"0\" y=\"0\" z=\"0\"/>"
            "        <point x=\"0\" y=\"0\" z=\"5\"/>"
            "        <point x=\"0\" y=\"0\" z=\"10\"/>"
            "      </control_points>"
            "    </path_element>"
            "  </timestep>"
            "</path>";
    }

    {
        std::ofstream os(projectRoot / "Segmentations" / "aorta.ctgr");
        os <<
            "<contourgroup path_id=\"17\" path_name=\"aorta\">"
            "  <timestep>"
            "    <contour>"
            "      <path_point id=\"0\">"
            "        <pos x=\"0\" y=\"0\" z=\"0\"/>"
            "        <tangent x=\"0\" y=\"0\" z=\"1\"/>"
            "        <rotation x=\"1\" y=\"0\" z=\"0\"/>"
            "      </path_point>"
            "      <contour_points>"
            "        <point x=\"1\" y=\"0\" z=\"0\"/>"
            "        <point x=\"0\" y=\"1\" z=\"0\"/>"
            "        <point x=\"-1\" y=\"0\" z=\"0\"/>"
            "        <point x=\"0\" y=\"-1\" z=\"0\"/>"
            "      </contour_points>"
            "    </contour>"
            "  </timestep>"
            "</contourgroup>";
    }

    const std::string projectDir = projectRoot.string();
    auto ds = mitk::StandaloneDataStorage::New();

    xq_LegacyImporter importer;
    if (!importer.ImportProject(ds, projectDir))
    {
        std::cerr << "FAIL test_sv_project_import_creates_vascular_nodes: import failed\n";
        std::filesystem::remove_all(projectRoot);
        return false;
    }

    int pathNodeCount = 0;
    int profileGroupCount = 0;
    int pathPointSetCount = 0;
    int segPointSetCount = 0;

    const auto allNodes = ds->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        const auto& node = it->Value();
        const std::string name = node->GetName();

        if (name == "Paths")
            continue;
        if (name == "Segmentations")
            continue;

        if (dynamic_cast<xq_VesselCenterline*>(node->GetData()) != nullptr)
            ++pathNodeCount;

        if (dynamic_cast<xq_ProfileGroup*>(node->GetData()) != nullptr)
            ++profileGroupCount;

        if (dynamic_cast<mitk::PointSet*>(node->GetData()) != nullptr)
        {
            auto sources = ds->GetSources(node);
            if (sources && !sources->empty())
            {
                const auto parentName = sources->GetElement(0)->GetName();
                if (parentName == "Paths")
                    ++pathPointSetCount;
                if (parentName == "Segmentations")
                    ++segPointSetCount;
            }
        }
    }

    if (pathNodeCount == 0)
    {
        std::cerr << "FAIL test_sv_project_import_creates_vascular_nodes: no imported path nodes were converted to xq_VesselCenterline\n";
        std::filesystem::remove_all(projectRoot);
        return false;
    }

    if (profileGroupCount == 0)
    {
        std::cerr << "FAIL test_sv_project_import_creates_vascular_nodes: no imported contour groups were converted to xq_ProfileGroup\n";
        std::filesystem::remove_all(projectRoot);
        return false;
    }

    if (pathPointSetCount > 0 || segPointSetCount > 0)
    {
        std::cerr << "FAIL test_sv_project_import_creates_vascular_nodes: legacy importer still created PointSet nodes under Paths/Segmentations\n";
        std::filesystem::remove_all(projectRoot);
        return false;
    }

    std::filesystem::remove_all(projectRoot);
    std::cout << "PASS test_sv_project_import_creates_vascular_nodes\n";
    return true;
}

static bool test_segmentation_object_factory_creates_mappers()
{
    auto group = xq_ProfileGroup::New();
    auto* c0 = MakeCircle();
    auto* c1 = MakeCircle();
    c0->SetMethod("manual");
    c1->SetMethod("manual");
    group->AppendProfile(c0, 0);
    group->AppendProfile(c1, 1);
    xq_SegmentationUtils::ApplyPlacementFrame(group->GetProfileAtPathPos(0), MakeAxialFrame(0, 0.0));
    xq_SegmentationUtils::ApplyPlacementFrame(group->GetProfileAtPathPos(1), MakeAxialFrame(1, 2.0));

    auto node = mitk::DataNode::New();
    node->SetData(group);

    auto mapper2D = mitk::CoreObjectFactory::GetInstance()->CreateMapper(
        node, mitk::BaseRenderer::Standard2D);
    auto mapper3D = mitk::CoreObjectFactory::GetInstance()->CreateMapper(
        node, mitk::BaseRenderer::Standard3D);

    if (mapper2D.IsNull() || mapper3D.IsNull())
    {
        std::cerr << "FAIL test_segmentation_object_factory_creates_mappers: ProfileGroup mappers were not registered\n";
        return false;
    }

    std::cout << "PASS test_segmentation_object_factory_creates_mappers\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 3 — ProfileGroup API consistency (stable contract; passes before + after)
// ---------------------------------------------------------------------------

static bool test_profile_group_api_consistency()
{
    auto group = xq_ProfileGroup::New();
    group->AppendProfile(MakeCircle(), 0);
    group->AppendProfile(MakeCircle(), 5);
    group->AppendProfile(MakeCircle(), 10);

    // CountProfiles and GetProfileCount must agree
    if (group->CountProfiles() != group->GetProfileCount())
    {
        std::cerr << "FAIL test_profile_group_api_consistency: "
                  << "CountProfiles() != GetProfileCount()\n";
        return false;
    }
    if (group->GetProfileCount() != 3)
    {
        std::cerr << "FAIL test_profile_group_api_consistency: expected count 3\n";
        return false;
    }

    // FetchProfile and GetProfileAtPathPos must agree
    for (int pos : {0, 5, 10})
    {
        if (group->FetchProfile(pos) != group->GetProfileAtPathPos(pos))
        {
            std::cerr << "FAIL test_profile_group_api_consistency: "
                      << "FetchProfile(" << pos << ") != GetProfileAtPathPos(" << pos << ")\n";
            return false;
        }
        if (group->FetchProfile(pos) == nullptr)
        {
            std::cerr << "FAIL test_profile_group_api_consistency: "
                      << "expected non-null at pos " << pos << "\n";
            return false;
        }
    }

    // Ordinal i=1 must return nullptr (pos 1 not present in sparse group)
    if (group->FetchProfile(1) != nullptr)
    {
        std::cerr << "FAIL test_profile_group_api_consistency: "
                  << "FetchProfile(1) should be nullptr for sparse group\n";
        return false;
    }

    // GetProfilePathIndices must be sorted and exactly {0, 5, 10}
    const auto indices = group->GetProfilePathIndices();
    if (indices != std::vector<int>{0, 5, 10})
    {
        std::cerr << "FAIL test_profile_group_api_consistency: "
                  << "GetProfilePathIndices() returned unexpected values\n";
        return false;
    }

    // HasProfile must be consistent
    if (!group->HasProfile(0) || !group->HasProfile(5) || !group->HasProfile(10))
    {
        std::cerr << "FAIL test_profile_group_api_consistency: HasProfile wrong for known pos\n";
        return false;
    }
    if (group->HasProfile(1) || group->HasProfile(3))
    {
        std::cerr << "FAIL test_profile_group_api_consistency: HasProfile wrong for absent pos\n";
        return false;
    }

    std::cout << "PASS test_profile_group_api_consistency\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 4 — ReplaceProfile null-pointer safety
//
// Before fix: ReplaceProfile(nullptr, …) dereferences contour immediately →
//             segfault.
// After fix:  null is treated as a no-op; the pre-existing profile is intact.
// ---------------------------------------------------------------------------

static bool test_replace_profile_null_safety()
{
    auto group = xq_ProfileGroup::New();
    group->AppendProfile(MakeCircle(), 3);

    // This CRASHES before the null-guard fix is applied.
    group->ReplaceProfile(nullptr, 3);

    // After the fix the existing profile at pos 3 must be untouched.
    if (!group->HasProfile(3))
    {
        std::cerr << "FAIL test_replace_profile_null_safety: "
                  << "profile at pos 3 should still exist after ReplaceProfile(null)\n";
        return false;
    }
    if (group->GetProfileCount() != 1)
    {
        std::cerr << "FAIL test_replace_profile_null_safety: "
                  << "count should still be 1, got " << group->GetProfileCount() << "\n";
        return false;
    }

    std::cout << "PASS test_replace_profile_null_safety\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 4b — AppendProfile convenience overload null-pointer safety
//
// Before fix: AppendProfile(nullptr) dereferences contour->GetPathPosIndex() and
//             segfaults.
// After fix:  null is treated as a no-op and the group remains unchanged.
// ---------------------------------------------------------------------------

static bool test_append_profile_null_safety()
{
    auto group = xq_ProfileGroup::New();
    group->AppendProfile(MakeCircle(), 3);

    group->AppendProfile(nullptr);

    if (!group->HasProfile(3))
    {
        std::cerr << "FAIL test_append_profile_null_safety: "
                  << "profile at pos 3 should still exist after AppendProfile(null)\n";
        return false;
    }
    if (group->GetProfileCount() != 1)
    {
        std::cerr << "FAIL test_append_profile_null_safety: "
                  << "count should still be 1, got " << group->GetProfileCount() << "\n";
        return false;
    }

    std::cout << "PASS test_append_profile_null_safety\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 5 — ParseIntSafe handles malformed path_id without throwing
//
// Before fix: DoRead() calls std::stoi() directly; a non-numeric path_id
//             attribute throws std::invalid_argument / std::out_of_range.
// After fix:  xq_LumenSegIO::ParseIntSafe() returns a default value instead.
// ---------------------------------------------------------------------------

static bool test_parse_int_safe()
{
    // Valid integers must parse correctly.
    if (xq_LumenSegIO::ParseIntSafe("0") != 0)
    {
        std::cerr << "FAIL test_parse_int_safe: \"0\" should parse as 0\n";
        return false;
    }
    if (xq_LumenSegIO::ParseIntSafe("42") != 42)
    {
        std::cerr << "FAIL test_parse_int_safe: \"42\" should parse as 42\n";
        return false;
    }
    if (xq_LumenSegIO::ParseIntSafe("-7") != -7)
    {
        std::cerr << "FAIL test_parse_int_safe: \"-7\" should parse as -7\n";
        return false;
    }

    // Alphabetic strings would throw std::invalid_argument from std::stoi.
    if (xq_LumenSegIO::ParseIntSafe("abc") != 0)
    {
        std::cerr << "FAIL test_parse_int_safe: \"abc\" should return default 0\n";
        return false;
    }
    if (xq_LumenSegIO::ParseIntSafe("") != 0)
    {
        std::cerr << "FAIL test_parse_int_safe: empty string should return default 0\n";
        return false;
    }

    // Out-of-range value would throw std::out_of_range from std::stoi.
    if (xq_LumenSegIO::ParseIntSafe("99999999999999999999") != 0)
    {
        std::cerr << "FAIL test_parse_int_safe: "
                  << "out-of-range string should return default 0\n";
        return false;
    }

    // Custom default value is forwarded when parsing fails.
    if (xq_LumenSegIO::ParseIntSafe("bad", -1) != -1)
    {
        std::cerr << "FAIL test_parse_int_safe: custom default -1 should be returned\n";
        return false;
    }

    std::cout << "PASS test_parse_int_safe\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 6 — Loft cache dirty-state rules
//
// Before fix: AppendProfile/RemoveProfile/ReplaceProfile do not touch any
//             dirty flag; IsLoftCacheDirty / MarkLoftCacheDirty /
//             ClearLoftCacheDirty do not exist.
// After fix:  Mutations mark the cache dirty; SetLoftedMesh clears it;
//             ClearLoftCacheDirty resets it explicitly.
// ---------------------------------------------------------------------------

static bool test_loft_cache_dirty_state()
{
    auto group = xq_ProfileGroup::New();

    // Fresh empty group: not dirty — no profiles, no stale mesh.
    if (group->IsLoftCacheDirty())
    {
        std::cerr << "FAIL test_loft_cache_dirty_state: "
                  << "fresh group should NOT be dirty\n";
        return false;
    }

    // Appending a profile makes the cache stale.
    group->AppendProfile(MakeCircle(), 0);
    if (!group->IsLoftCacheDirty())
    {
        std::cerr << "FAIL test_loft_cache_dirty_state: "
                  << "should be dirty after AppendProfile\n";
        return false;
    }

    // Providing a new loft mesh clears the dirty flag.
    group->SetLoftedMesh(vtkSmartPointer<vtkPolyData>::New());
    if (group->IsLoftCacheDirty())
    {
        std::cerr << "FAIL test_loft_cache_dirty_state: "
                  << "should NOT be dirty after SetLoftedMesh\n";
        return false;
    }

    // Replacing a profile makes the existing mesh stale again.
    group->ReplaceProfile(MakeCircle(), 0);
    if (!group->IsLoftCacheDirty())
    {
        std::cerr << "FAIL test_loft_cache_dirty_state: "
                  << "should be dirty after ReplaceProfile\n";
        return false;
    }

    // Explicitly clearing the dirty flag (e.g., after external re-loft).
    group->ClearLoftCacheDirty();
    if (group->IsLoftCacheDirty())
    {
        std::cerr << "FAIL test_loft_cache_dirty_state: "
                  << "should NOT be dirty after ClearLoftCacheDirty\n";
        return false;
    }

    // Removing a profile also makes the mesh stale.
    group->RemoveProfile(0);
    if (!group->IsLoftCacheDirty())
    {
        std::cerr << "FAIL test_loft_cache_dirty_state: "
                  << "should be dirty after RemoveProfile\n";
        return false;
    }

    // MarkLoftCacheDirty is callable explicitly (e.g., by external mutators).
    group->ClearLoftCacheDirty();
    group->MarkLoftCacheDirty();
    if (!group->IsLoftCacheDirty())
    {
        std::cerr << "FAIL test_loft_cache_dirty_state: "
                  << "should be dirty after explicit MarkLoftCacheDirty\n";
        return false;
    }

    // Timestep isolation: mutation on timestep 1 must not dirty timestep 0.
    auto group2 = xq_ProfileGroup::New();
    group2->AppendProfile(MakeCircle(), 0, /*timeStep=*/0);
    group2->SetLoftedMesh(vtkSmartPointer<vtkPolyData>::New(), /*timeStep=*/0);
    group2->AppendProfile(MakeCircle(), 0, /*timeStep=*/1);  // mutates ts=1

    if (group2->IsLoftCacheDirty(/*timeStep=*/0))
    {
        std::cerr << "FAIL test_loft_cache_dirty_state: "
                  << "timestep 0 should NOT be dirty; only ts 1 was mutated after mesh set\n";
        return false;
    }
    if (!group2->IsLoftCacheDirty(/*timeStep=*/1))
    {
        std::cerr << "FAIL test_loft_cache_dirty_state: "
                  << "timestep 1 should be dirty\n";
        return false;
    }

    std::cout << "PASS test_loft_cache_dirty_state\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 7 — GetLoftedMesh must not return stale data when cache is dirty
//
// Before fix: GetLoftedMesh() returns the previously cached mesh even after
//             a profile mutation marks the cache dirty — stale data survives silently.
// After fix:  GetLoftedMesh() returns nullptr when IsLoftCacheDirty() is true.
// ---------------------------------------------------------------------------

static bool test_stale_loft_mesh_not_returned_when_dirty()
{
    auto group = xq_ProfileGroup::New();
    group->AppendProfile(MakeCircle(), 0);

    // Provide a lofted mesh — cache is now clean.
    auto mesh = vtkSmartPointer<vtkPolyData>::New();
    group->SetLoftedMesh(mesh);

    if (group->IsLoftCacheDirty())
    {
        std::cerr << "FAIL test_stale_loft_mesh_not_returned_when_dirty: "
                  << "should not be dirty right after SetLoftedMesh\n";
        return false;
    }

    // Clean cache: GetLoftedMesh must return the stored mesh (not null).
    if (group->GetLoftedMesh() == nullptr)
    {
        std::cerr << "FAIL test_stale_loft_mesh_not_returned_when_dirty: "
                  << "GetLoftedMesh() should return mesh when cache is clean\n";
        return false;
    }

    // Mutate a profile — cache is now dirty.
    group->ReplaceProfile(MakeCircle(), 0);

    if (!group->IsLoftCacheDirty())
    {
        std::cerr << "FAIL test_stale_loft_mesh_not_returned_when_dirty: "
                  << "should be dirty after ReplaceProfile\n";
        return false;
    }

    // Stale-data check: GetLoftedMesh must NOT return the old mesh while dirty.
    if (group->GetLoftedMesh() != nullptr)
    {
        std::cerr << "FAIL test_stale_loft_mesh_not_returned_when_dirty: "
                  << "GetLoftedMesh() returned stale mesh while cache is dirty\n";
        return false;
    }

    // Cloning a dirty group must not "wash clean" the stale-cache state.
    auto cloneObj = group->Clone();
    auto* clone = dynamic_cast<xq_ProfileGroup*>(cloneObj.GetPointer());
    if (!clone)
    {
        std::cerr << "FAIL test_stale_loft_mesh_not_returned_when_dirty: "
                  << "Clone() did not produce an xq_ProfileGroup\n";
        return false;
    }
    if (!clone->IsLoftCacheDirty())
    {
        std::cerr << "FAIL test_stale_loft_mesh_not_returned_when_dirty: "
                  << "cloned dirty group should remain dirty\n";
        return false;
    }
    if (clone->GetLoftedMesh() != nullptr)
    {
        std::cerr << "FAIL test_stale_loft_mesh_not_returned_when_dirty: "
                  << "cloned dirty group should not expose a stale mesh\n";
        return false;
    }

    // After providing a fresh mesh, GetLoftedMesh must return it again.
    auto newMesh = vtkSmartPointer<vtkPolyData>::New();
    group->SetLoftedMesh(newMesh);
    if (group->GetLoftedMesh() != newMesh)
    {
        std::cerr << "FAIL test_stale_loft_mesh_not_returned_when_dirty: "
                  << "GetLoftedMesh() should return new mesh after SetLoftedMesh clears dirty\n";
        return false;
    }

    // Timestep isolation: dirty on ts=1 must not block ts=0.
    auto group2 = xq_ProfileGroup::New();
    group2->AppendProfile(MakeCircle(), 0, /*timeStep=*/0);
    auto meshTs0 = vtkSmartPointer<vtkPolyData>::New();
    group2->SetLoftedMesh(meshTs0, /*timeStep=*/0);
    group2->AppendProfile(MakeCircle(), 0, /*timeStep=*/1);  // dirties ts=1 only

    if (group2->GetLoftedMesh(/*timeStep=*/0) == nullptr)
    {
        std::cerr << "FAIL test_stale_loft_mesh_not_returned_when_dirty: "
                  << "ts=0 should still return mesh; only ts=1 is dirty\n";
        return false;
    }
    if (group2->GetLoftedMesh(/*timeStep=*/1) != nullptr)
    {
        std::cerr << "FAIL test_stale_loft_mesh_not_returned_when_dirty: "
                  << "ts=1 GetLoftedMesh() should return nullptr while dirty\n";
        return false;
    }

    // Clearing the group must release any previously cached mesh instead of
    // letting it reappear if a caller clears the dirty flag afterward.
    group2->ClearData();
    group2->ClearLoftCacheDirty(/*timeStep=*/0);
    if (group2->GetLoftedMesh(/*timeStep=*/0) != nullptr)
    {
        std::cerr << "FAIL test_stale_loft_mesh_not_returned_when_dirty: "
                  << "ClearData() should drop cached loft mesh for ts=0\n";
        return false;
    }

    std::cout << "PASS test_stale_loft_mesh_not_returned_when_dirty\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 8 — Lumen UI preset helpers create canonical profile objects
// ---------------------------------------------------------------------------

static bool test_create_preset_profiles()
{
    mitk::Point3D center;
    center[0] = 4.0;
    center[1] = -2.0;
    center[2] = 7.5;

    auto circle = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", center, /*primarySize=*/6.0, /*secondarySize=*/0.0, /*subdivisionCount=*/24);
    if (!circle)
    {
        std::cerr << "FAIL test_create_preset_profiles: circle preset returned null\n";
        return false;
    }
    if (circle->GetProfileKind() != "Circle" || circle->GetMethod() != "circle")
    {
        std::cerr << "FAIL test_create_preset_profiles: circle preset metadata mismatch\n";
        return false;
    }
    if (circle->GetProfilePoints().size() != 24)
    {
        std::cerr << "FAIL test_create_preset_profiles: circle preset point count mismatch\n";
        return false;
    }
    auto* circular = dynamic_cast<xq_CircularProfile*>(circle.get());
    if (!circular || std::abs(circular->GetRadius() - 6.0) > 1e-9)
    {
        std::cerr << "FAIL test_create_preset_profiles: circle preset radius mismatch\n";
        return false;
    }

    auto ellipse = xq_SegmentationUtils::CreatePresetProfile(
        "Ellipse", center, /*primarySize=*/8.0, /*secondarySize=*/3.0, /*subdivisionCount=*/18);
    if (!ellipse)
    {
        std::cerr << "FAIL test_create_preset_profiles: ellipse preset returned null\n";
        return false;
    }
    if (ellipse->GetProfileKind() != "Ellipse" || ellipse->GetMethod() != "ellipse")
    {
        std::cerr << "FAIL test_create_preset_profiles: ellipse preset metadata mismatch\n";
        return false;
    }
    if (ellipse->GetProfilePoints().size() != 18)
    {
        std::cerr << "FAIL test_create_preset_profiles: ellipse preset point count mismatch\n";
        return false;
    }
    auto* elliptic = dynamic_cast<xq_EllipticProfile*>(ellipse.get());
    if (!elliptic || ellipse->GetAnchorPointCount() != 3)
    {
        std::cerr << "FAIL test_create_preset_profiles: ellipse preset anchor layout mismatch\n";
        return false;
    }
    if (std::abs(ellipse->GetAnchorPoint(1)[0] - (center[0] + 8.0)) > 1e-9 ||
        std::abs(ellipse->GetAnchorPoint(2)[1] - (center[1] + 3.0)) > 1e-9)
    {
        std::cerr << "FAIL test_create_preset_profiles: ellipse preset axis lengths mismatch\n";
        return false;
    }

    std::cout << "PASS test_create_preset_profiles\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 9 — Lumen UI loft helper consumes canonical ProfileGroup contours
// ---------------------------------------------------------------------------

static bool test_loft_profile_group_from_canonical_profiles()
{
    auto group = xq_ProfileGroup::New();

    mitk::Point3D center0;
    center0[0] = 0.0;
    center0[1] = 0.0;
    center0[2] = 0.0;
    auto lower = xq_SegmentationUtils::CreatePresetProfile("Circle", center0, 4.0, 0.0, 24);

    mitk::Point3D center1;
    center1[0] = 0.0;
    center1[1] = 0.0;
    center1[2] = 5.0;
    auto upper = xq_SegmentationUtils::CreatePresetProfile("Circle", center1, 3.0, 0.0, 24);

    if (!lower || !upper)
    {
        std::cerr << "FAIL test_loft_profile_group_from_canonical_profiles: preset creation failed\n";
        return false;
    }

    group->AppendProfile(lower.release(), 2);
    group->AppendProfile(upper.release(), 9);

    auto loftMesh = xq_SegmentationUtils::LoftProfileGroup(group.GetPointer());
    if (!loftMesh)
    {
        std::cerr << "FAIL test_loft_profile_group_from_canonical_profiles: loft helper returned null\n";
        return false;
    }
    if (loftMesh->GetNumberOfPoints() == 0 || loftMesh->GetNumberOfCells() == 0)
    {
        std::cerr << "FAIL test_loft_profile_group_from_canonical_profiles: loft mesh is empty\n";
        return false;
    }

    vtkIdType stripPointCount = 0;
    const vtkIdType* stripPointIds = nullptr;
    auto* strips = loftMesh->GetStrips();
    if (!strips)
    {
        std::cerr << "FAIL test_loft_profile_group_from_canonical_profiles: loft mesh has no strips\n";
        return false;
    }
    strips->InitTraversal();
    if (!strips->GetNextCell(stripPointCount, stripPointIds))
    {
        std::cerr << "FAIL test_loft_profile_group_from_canonical_profiles: no strip cells found\n";
        return false;
    }
    if (stripPointCount != 50)
    {
        std::cerr << "FAIL test_loft_profile_group_from_canonical_profiles: "
                  << "expected closed strip with 50 ids, got " << stripPointCount << "\n";
        return false;
    }
    if (stripPointIds[stripPointCount - 2] != stripPointIds[0] ||
        stripPointIds[stripPointCount - 1] != stripPointIds[1])
    {
        std::cerr << "FAIL test_loft_profile_group_from_canonical_profiles: "
                  << "strip does not wrap back to the first contour pair\n";
        return false;
    }

    std::cout << "PASS test_loft_profile_group_from_canonical_profiles\n";
    return true;
}

static bool test_loft_contours_aligns_cyclic_point_order()
{
    constexpr int kSamples = 24;
    constexpr double kRadius = 4.0;
    constexpr double kLength = 10.0;

    std::vector<mitk::Point3D> lower;
    std::vector<mitk::Point3D> upper;
    lower.reserve(kSamples);
    upper.reserve(kSamples);

    for (int i = 0; i < kSamples; ++i)
    {
        const double angle = 2.0 * kPi * static_cast<double>(i) / static_cast<double>(kSamples);
        mitk::Point3D point;
        point[0] = kRadius * std::cos(angle);
        point[1] = kRadius * std::sin(angle);
        point[2] = -kLength * 0.5;
        lower.push_back(point);
    }

    // Same circle, but deliberately phase-shift the contour indexing by 180 deg.
    for (int i = 0; i < kSamples; ++i)
    {
        const int shifted = (i + kSamples / 2) % kSamples;
        const double angle = 2.0 * kPi * static_cast<double>(shifted) / static_cast<double>(kSamples);
        mitk::Point3D point;
        point[0] = kRadius * std::cos(angle);
        point[1] = kRadius * std::sin(angle);
        point[2] = kLength * 0.5;
        upper.push_back(point);
    }

    auto loft = xq_SegmentationUtils::LoftContours({lower, upper});
    if (!loft || loft->GetNumberOfCells() == 0)
    {
        std::cerr << "FAIL test_loft_contours_aligns_cyclic_point_order: loft surface missing\n";
        return false;
    }

    auto triangles = vtkSmartPointer<vtkTriangleFilter>::New();
    triangles->SetInputData(loft);
    triangles->Update();

    auto mass = vtkSmartPointer<vtkMassProperties>::New();
    mass->SetInputData(triangles->GetOutput());
    mass->Update();

    const double measuredArea = mass->GetSurfaceArea();
    const double expectedArea = 2.0 * kPi * kRadius * kLength + 2.0 * kPi * kRadius * kRadius;
    const double relativeError = std::abs(measuredArea - expectedArea) / expectedArea;

    if (relativeError > 0.35)
    {
        std::cerr << "FAIL test_loft_contours_aligns_cyclic_point_order: "
                  << "surface area deviates too much from cylindrical expectation. "
                  << "Measured=" << measuredArea << " expected=" << expectedArea << "\n";
        return false;
    }

    std::cout << "PASS test_loft_contours_aligns_cyclic_point_order\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 10 — ProfileGroup edit helpers clone and scale canonical profiles
// ---------------------------------------------------------------------------

static bool test_profile_edit_helpers_for_canonical_profiles()
{
    mitk::Point3D center;
    center[0] = 1.0;
    center[1] = 2.0;
    center[2] = 3.0;

    auto original = xq_SegmentationUtils::CreatePresetProfile("Circle", center, 2.5, 0.0, 24);
    if (!original)
    {
        std::cerr << "FAIL test_profile_edit_helpers_for_canonical_profiles: original preset null\n";
        return false;
    }

    auto shifted = xq_SegmentationUtils::CloneProfileWithOffset(original.get(), /*zOffset=*/4.0);
    if (!shifted)
    {
        std::cerr << "FAIL test_profile_edit_helpers_for_canonical_profiles: shifted clone null\n";
        return false;
    }
    if (shifted->GetProfileKind() != "Circle" || shifted->GetMethod() != "circle")
    {
        std::cerr << "FAIL test_profile_edit_helpers_for_canonical_profiles: shifted clone metadata mismatch\n";
        return false;
    }
    if (std::abs(shifted->GetProfileCenter()[2] - 7.0) > 1e-9)
    {
        std::cerr << "FAIL test_profile_edit_helpers_for_canonical_profiles: shifted clone center mismatch\n";
        return false;
    }
    if (std::abs(shifted->GetAnchorPoint(0)[2] - 7.0) > 1e-9 ||
        std::abs(shifted->GetAnchorPoint(1)[2] - 7.0) > 1e-9)
    {
        std::cerr << "FAIL test_profile_edit_helpers_for_canonical_profiles: shifted clone anchor mismatch\n";
        return false;
    }

    auto scaled = xq_SegmentationUtils::ScaleProfile(original.get(), /*factor=*/1.6);
    if (!scaled)
    {
        std::cerr << "FAIL test_profile_edit_helpers_for_canonical_profiles: scaled clone null\n";
        return false;
    }
    auto* scaledCircle = dynamic_cast<xq_CircularProfile*>(scaled.get());
    if (!scaledCircle || std::abs(scaledCircle->GetRadius() - 4.0) > 1e-9)
    {
        std::cerr << "FAIL test_profile_edit_helpers_for_canonical_profiles: scaled radius mismatch\n";
        return false;
    }
    if (std::abs(scaled->GetProfileCenter()[0] - center[0]) > 1e-9 ||
        std::abs(scaled->GetProfileCenter()[1] - center[1]) > 1e-9 ||
        std::abs(scaled->GetProfileCenter()[2] - center[2]) > 1e-9)
    {
        std::cerr << "FAIL test_profile_edit_helpers_for_canonical_profiles: scaling moved center\n";
        return false;
    }

    std::cout << "PASS test_profile_edit_helpers_for_canonical_profiles\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 11 — Manual/Spline profile skeletons are created on the canonical path
// ---------------------------------------------------------------------------

static bool test_create_editable_profile_skeletons()
{
    mitk::Point3D center;
    center[0] = -1.0;
    center[1] = 4.0;
    center[2] = 2.5;

    auto manual = xq_SegmentationUtils::CreateEditableProfile("Manual", center);
    if (!manual)
    {
        std::cerr << "FAIL test_create_editable_profile_skeletons: manual skeleton null\n";
        return false;
    }
    if (manual->GetProfileKind() != "Polygon" || manual->GetMethod() != "manual")
    {
        std::cerr << "FAIL test_create_editable_profile_skeletons: manual skeleton metadata mismatch\n";
        return false;
    }
    if (manual->GetAnchorPointCount() != 0)
    {
        std::cerr << "FAIL test_create_editable_profile_skeletons: manual skeleton should start empty\n";
        return false;
    }
    if (std::abs(manual->GetProfileCenter()[2] - center[2]) > 1e-9)
    {
        std::cerr << "FAIL test_create_editable_profile_skeletons: manual skeleton center mismatch\n";
        return false;
    }

    auto spline = xq_SegmentationUtils::CreateEditableProfile("SplinePolygon", center);
    if (!spline)
    {
        std::cerr << "FAIL test_create_editable_profile_skeletons: spline skeleton null\n";
        return false;
    }
    if (spline->GetProfileKind() != "SplinePolygon" || spline->GetMethod() != "splinepolygon")
    {
        std::cerr << "FAIL test_create_editable_profile_skeletons: spline skeleton metadata mismatch\n";
        return false;
    }
    if (dynamic_cast<xq_SplineProfile*>(spline.get()) == nullptr)
    {
        std::cerr << "FAIL test_create_editable_profile_skeletons: spline skeleton type mismatch\n";
        return false;
    }
    if (spline->GetAnchorPointCount() != 0)
    {
        std::cerr << "FAIL test_create_editable_profile_skeletons: spline skeleton should start empty\n";
        return false;
    }

    std::cout << "PASS test_create_editable_profile_skeletons\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 12 — Canonical profile statistics helper
// ---------------------------------------------------------------------------

static bool test_profile_statistics_helper()
{
    mitk::Point3D center;
    center[0] = 0.0;
    center[1] = 0.0;
    center[2] = 0.0;

    auto circle = xq_SegmentationUtils::CreatePresetProfile("Circle", center, 5.0, 0.0, 40);
    if (!circle)
    {
        std::cerr << "FAIL test_profile_statistics_helper: circle preset null\n";
        return false;
    }

    const auto stats = xq_SegmentationUtils::ComputeProfileStatistics(circle.get());
    if (stats.pointCount != 40)
    {
        std::cerr << "FAIL test_profile_statistics_helper: expected 40 points, got "
                  << stats.pointCount << "\n";
        return false;
    }
    if (std::abs(stats.perimeter - (2.0 * kPi * 5.0)) > 1.0)
    {
        std::cerr << "FAIL test_profile_statistics_helper: perimeter mismatch\n";
        return false;
    }
    if (std::abs(stats.area - (kPi * 25.0)) > 2.0)
    {
        std::cerr << "FAIL test_profile_statistics_helper: area mismatch\n";
        return false;
    }
    if (std::abs(stats.boundingBox[0] + 5.0) > 0.5 ||
        std::abs(stats.boundingBox[1] - 5.0) > 0.5)
    {
        std::cerr << "FAIL test_profile_statistics_helper: bounding box mismatch\n";
        return false;
    }

    std::cout << "PASS test_profile_statistics_helper\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 13 — Canonical profile smooth/resample helpers
// ---------------------------------------------------------------------------

static bool test_profile_smooth_and_resample_helpers()
{
    mitk::Point3D center;
    center[0] = 0.0;
    center[1] = 0.0;
    center[2] = 1.0;

    auto manual = xq_SegmentationUtils::CreateEditableProfile("Manual", center);
    if (!manual)
    {
        std::cerr << "FAIL test_profile_smooth_and_resample_helpers: manual profile null\n";
        return false;
    }

    std::vector<mitk::Point3D> square(4);
    square[0][0] = -2.0; square[0][1] = -2.0; square[0][2] = 1.0;
    square[1][0] =  2.0; square[1][1] = -2.0; square[1][2] = 1.0;
    square[2][0] =  2.0; square[2][1] =  2.0; square[2][2] = 1.0;
    square[3][0] = -2.0; square[3][1] =  2.0; square[3][2] = 1.0;
    manual->SetAnchorPoints(square);
    manual->SetMethod("manual");

    auto smoothed = xq_SegmentationUtils::SmoothProfile(manual.get(), 3, 0.25);
    if (!smoothed)
    {
        std::cerr << "FAIL test_profile_smooth_and_resample_helpers: smooth helper returned null\n";
        return false;
    }
    if (smoothed->GetProfileKind() != "Polygon" || smoothed->GetMethod() != "manual")
    {
        std::cerr << "FAIL test_profile_smooth_and_resample_helpers: smooth helper metadata mismatch\n";
        return false;
    }
    if (smoothed->GetAnchorPointCount() != 4)
    {
        std::cerr << "FAIL test_profile_smooth_and_resample_helpers: smooth helper anchor count mismatch\n";
        return false;
    }
    if (std::abs(smoothed->GetAnchorPoint(0)[0] - manual->GetAnchorPoint(0)[0]) < 1e-6)
    {
        std::cerr << "FAIL test_profile_smooth_and_resample_helpers: smoothing did not move anchor points\n";
        return false;
    }

    auto resampledManual = xq_SegmentationUtils::ResampleProfile(manual.get(), 12);
    if (!resampledManual)
    {
        std::cerr << "FAIL test_profile_smooth_and_resample_helpers: manual resample returned null\n";
        return false;
    }
    if (resampledManual->GetProfileKind() != "Polygon" || resampledManual->GetAnchorPointCount() != 12)
    {
        std::cerr << "FAIL test_profile_smooth_and_resample_helpers: manual resample shape mismatch\n";
        return false;
    }

    auto circle = xq_SegmentationUtils::CreatePresetProfile("Circle", center, 3.0, 0.0, 24);
    auto resampledCircle = xq_SegmentationUtils::ResampleProfile(circle.get(), 18);
    if (!resampledCircle)
    {
        std::cerr << "FAIL test_profile_smooth_and_resample_helpers: circle resample returned null\n";
        return false;
    }
    if (resampledCircle->GetProfileKind() != "Circle" || resampledCircle->GetProfilePoints().size() != 18)
    {
        std::cerr << "FAIL test_profile_smooth_and_resample_helpers: circle resample did not preserve primitive form\n";
        return false;
    }

    std::cout << "PASS test_profile_smooth_and_resample_helpers\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 14 — Threshold/raw contour points can be converted to canonical profiles
// ---------------------------------------------------------------------------

static bool test_create_profile_from_contour_points()
{
    std::vector<mitk::Point3D> contourPoints(4);
    contourPoints[0][0] = -1.0; contourPoints[0][1] = -1.0; contourPoints[0][2] = 2.0;
    contourPoints[1][0] =  1.0; contourPoints[1][1] = -1.0; contourPoints[1][2] = 2.0;
    contourPoints[2][0] =  1.0; contourPoints[2][1] =  1.0; contourPoints[2][2] = 2.0;
    contourPoints[3][0] = -1.0; contourPoints[3][1] =  1.0; contourPoints[3][2] = 2.0;

    auto thresholdProfile = xq_SegmentationUtils::CreateProfileFromContourPoints(
        contourPoints, "threshold", "Polygon");
    if (!thresholdProfile)
    {
        std::cerr << "FAIL test_create_profile_from_contour_points: helper returned null\n";
        return false;
    }
    if (thresholdProfile->GetProfileKind() != "Polygon" || thresholdProfile->GetMethod() != "threshold")
    {
        std::cerr << "FAIL test_create_profile_from_contour_points: metadata mismatch\n";
        return false;
    }
    if (thresholdProfile->GetAnchorPointCount() != 4)
    {
        std::cerr << "FAIL test_create_profile_from_contour_points: anchor count mismatch\n";
        return false;
    }
    if (std::abs(thresholdProfile->GetProfileCenter()[2] - 2.0) > 1e-9)
    {
        std::cerr << "FAIL test_create_profile_from_contour_points: center mismatch\n";
        return false;
    }

    std::cout << "PASS test_create_profile_from_contour_points\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 15 — Path placement helpers bind profiles to canonical path semantics
// ---------------------------------------------------------------------------

static bool test_apply_profile_placement_frame()
{
    xq_ProfilePlacementFrame frame0;
    frame0.pathPosIndex = 2;
    frame0.position[0] = 0.0;
    frame0.position[1] = 0.0;
    frame0.position[2] = 0.0;
    frame0.tangent.Fill(0.0);
    frame0.tangent[2] = 1.0;

    xq_ProfilePlacementFrame frame1;
    frame1.pathPosIndex = 7;
    frame1.position[0] = 10.0;
    frame1.position[1] = 0.0;
    frame1.position[2] = 0.0;
    frame1.tangent.Fill(0.0);
    frame1.tangent[0] = 1.0;

    xq_ProfilePlacementFrame resolved;
    const std::vector<xq_ProfilePlacementFrame> frames = {frame0, frame1};

    mitk::Point3D probe;
    probe[0] = 9.1;
    probe[1] = 0.2;
    probe[2] = 0.0;
    if (!xq_SegmentationUtils::FindNearestPlacementFrame(frames, probe, resolved))
    {
        std::cerr << "FAIL test_apply_profile_placement_frame: helper returned false\n";
        return false;
    }
    if (resolved.pathPosIndex != 7)
    {
        std::cerr << "FAIL test_apply_profile_placement_frame: nearest frame mismatch\n";
        return false;
    }

    auto profile = xq_SegmentationUtils::CreateEditableProfile("Manual", probe);
    if (!profile)
    {
        std::cerr << "FAIL test_apply_profile_placement_frame: profile helper returned null\n";
        return false;
    }

    xq_SegmentationUtils::ApplyPlacementFrame(profile.get(), resolved);

    if (profile->GetPathPosIndex() != 7)
    {
        std::cerr << "FAIL test_apply_profile_placement_frame: pathPosIndex not applied\n";
        return false;
    }

    const auto center = profile->GetProfileCenter();
    if (std::abs(center[0] - frame1.position[0]) > 1e-9 ||
        std::abs(center[1] - frame1.position[1]) > 1e-9 ||
        std::abs(center[2] - frame1.position[2]) > 1e-9)
    {
        std::cerr << "FAIL test_apply_profile_placement_frame: center mismatch\n";
        return false;
    }

    auto plane = profile->GetSlicePlane();
    if (plane.IsNull())
    {
        std::cerr << "FAIL test_apply_profile_placement_frame: slice plane missing\n";
        return false;
    }

    const auto origin = plane->GetOrigin();
    if (std::abs(origin[0] - frame1.position[0]) > 1e-9 ||
        std::abs(origin[1] - frame1.position[1]) > 1e-9 ||
        std::abs(origin[2] - frame1.position[2]) > 1e-9)
    {
        std::cerr << "FAIL test_apply_profile_placement_frame: plane origin mismatch\n";
        return false;
    }

    auto normal = plane->GetNormal();
    normal.Normalize();
    if (std::abs(normal[0] - frame1.tangent[0]) > 1e-9 ||
        std::abs(normal[1] - frame1.tangent[1]) > 1e-9 ||
        std::abs(normal[2] - frame1.tangent[2]) > 1e-9)
    {
        std::cerr << "FAIL test_apply_profile_placement_frame: plane normal mismatch\n";
        return false;
    }

    std::cout << "PASS test_apply_profile_placement_frame\n";
    return true;
}

static bool test_centerline_rotation_matches_inplane_xhat_convention()
{
    xq_CenterlineSegment segment;

    mitk::Point3D p0;
    p0[0] = 0.0; p0[1] = 0.0; p0[2] = 0.0;
    mitk::Point3D p1;
    p1[0] = 0.0; p1[1] = 0.0; p1[2] = 10.0;

    segment.ReplaceAnchors({p0, p1}, true);
    const auto traceVertices = segment.GetTraceVertices();
    if (traceVertices.empty())
    {
        std::cerr << "FAIL test_centerline_rotation_matches_inplane_xhat_convention: no trace vertices generated\n";
        return false;
    }

    const auto& first = traceVertices.front();
    const auto expectedXhat = xq_SpatialMath::ComputeOrthogonalVector(first.tangent);
    const auto dot = xq_SpatialMath::DotProduct3D(first.rotation, expectedXhat);

    if (std::abs(dot - 1.0) > 1e-6)
    {
        std::cerr << "FAIL test_centerline_rotation_matches_inplane_xhat_convention: "
                  << "rotation should store in-plane xhat, not binormal\n";
        return false;
    }

    std::cout << "PASS test_centerline_rotation_matches_inplane_xhat_convention\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 16 — Profile edit helpers preserve placement metadata
// ---------------------------------------------------------------------------

static bool test_profile_edit_helpers_preserve_placement_metadata()
{
    auto profile = xq_SegmentationUtils::CreateEditableProfile("Manual", mitk::Point3D());
    if (!profile)
    {
        std::cerr << "FAIL test_profile_edit_helpers_preserve_placement_metadata: profile helper returned null\n";
        return false;
    }

    std::vector<mitk::Point3D> anchors(4);
    anchors[0][0] = -1.0; anchors[0][1] = -1.0; anchors[0][2] = 2.0;
    anchors[1][0] =  1.0; anchors[1][1] = -1.0; anchors[1][2] = 2.0;
    anchors[2][0] =  1.0; anchors[2][1] =  1.0; anchors[2][2] = 2.0;
    anchors[3][0] = -1.0; anchors[3][1] =  1.0; anchors[3][2] = 2.0;
    profile->SetAnchorPoints(anchors);

    xq_ProfilePlacementFrame frame;
    frame.pathPosIndex = 9;
    frame.position[0] = 0.0;
    frame.position[1] = 0.0;
    frame.position[2] = 2.0;
    frame.tangent.Fill(0.0);
    frame.tangent[2] = 1.0;
    xq_SegmentationUtils::ApplyPlacementFrame(profile.get(), frame);

    auto pasted = xq_SegmentationUtils::CloneProfileWithOffset(profile.get(), 3.0);
    if (!pasted)
    {
        std::cerr << "FAIL test_profile_edit_helpers_preserve_placement_metadata: clone helper returned null\n";
        return false;
    }
    if (pasted->GetPathPosIndex() != 9)
    {
        std::cerr << "FAIL test_profile_edit_helpers_preserve_placement_metadata: pathPosIndex lost on clone\n";
        return false;
    }
    auto pastedPlane = pasted->GetSlicePlane();
    if (pastedPlane.IsNull() || std::abs(pastedPlane->GetOrigin()[2] - 5.0) > 1e-9)
    {
        std::cerr << "FAIL test_profile_edit_helpers_preserve_placement_metadata: pasted plane origin not offset\n";
        return false;
    }

    auto scaled = xq_SegmentationUtils::ScaleProfile(profile.get(), 1.5);
    auto smoothed = xq_SegmentationUtils::SmoothProfile(profile.get(), 2, 0.3);
    auto resampled = xq_SegmentationUtils::ResampleProfile(profile.get(), 12);
    if (!scaled || !smoothed || !resampled)
    {
        std::cerr << "FAIL test_profile_edit_helpers_preserve_placement_metadata: edit helper returned null\n";
        return false;
    }

    const auto checkPreservedPlacement = [](const xq_LumenProfile* edited) -> bool {
        auto plane = edited->GetSlicePlane();
        return edited->GetPathPosIndex() == 9 &&
               plane.IsNotNull() &&
               std::abs(plane->GetOrigin()[2] - 2.0) < 1e-9;
    };

    if (!checkPreservedPlacement(scaled.get()) ||
        !checkPreservedPlacement(smoothed.get()) ||
        !checkPreservedPlacement(resampled.get()))
    {
        std::cerr << "FAIL test_profile_edit_helpers_preserve_placement_metadata: placement metadata not preserved\n";
        return false;
    }

    std::cout << "PASS test_profile_edit_helpers_preserve_placement_metadata\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 17 — Preset profiles can align to placement planes
// ---------------------------------------------------------------------------

static bool test_orient_preset_profile_to_placement()
{
    mitk::Point3D center;
    center.Fill(0.0);
    auto ellipse = xq_SegmentationUtils::CreatePresetProfile("Ellipse", center, 4.0, 2.0, 24);
    if (!ellipse)
    {
        std::cerr << "FAIL test_orient_preset_profile_to_placement: preset helper returned null\n";
        return false;
    }

    xq_ProfilePlacementFrame frame;
    frame.pathPosIndex = 3;
    frame.position[0] = 1.0;
    frame.position[1] = 2.0;
    frame.position[2] = 3.0;
    frame.tangent.Fill(0.0);
    frame.tangent[0] = 1.0;
    frame.rotation.Fill(0.0);
    frame.rotation[2] = 1.0;

    xq_SegmentationUtils::ApplyPlacementFrame(ellipse.get(), frame);
    xq_SegmentationUtils::OrientPresetProfileToPlacement(ellipse.get(), 4.0, 2.0);

    const auto major = ellipse->GetAnchorPoint(1);
    const auto minor = ellipse->GetAnchorPoint(2);
    mitk::Vector3D majorVec;
    mitk::Vector3D minorVec;
    for (int d = 0; d < 3; ++d)
    {
        majorVec[d] = major[d] - frame.position[d];
        minorVec[d] = minor[d] - frame.position[d];
    }

    const double majorLen = std::sqrt(majorVec[0] * majorVec[0] +
                                      majorVec[1] * majorVec[1] +
                                      majorVec[2] * majorVec[2]);
    const double minorLen = std::sqrt(minorVec[0] * minorVec[0] +
                                      minorVec[1] * minorVec[1] +
                                      minorVec[2] * minorVec[2]);
    const double majorDotNormal = majorVec[0] * frame.tangent[0] +
                                  majorVec[1] * frame.tangent[1] +
                                  majorVec[2] * frame.tangent[2];
    const double minorDotNormal = minorVec[0] * frame.tangent[0] +
                                  minorVec[1] * frame.tangent[1] +
                                  minorVec[2] * frame.tangent[2];

    if (std::abs(majorLen - 4.0) > 1e-9 ||
        std::abs(minorLen - 2.0) > 1e-9 ||
        std::abs(majorDotNormal) > 1e-9 ||
        std::abs(minorDotNormal) > 1e-9)
    {
        std::cerr << "FAIL test_orient_preset_profile_to_placement: anchor alignment mismatch\n";
        return false;
    }

    std::cout << "PASS test_orient_preset_profile_to_placement\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 18 — Surface intersection can be converted into ordered contour points
// ---------------------------------------------------------------------------

static bool test_extract_surface_contour_on_plane()
{
    auto sphere = vtkSmartPointer<vtkSphereSource>::New();
    sphere->SetCenter(0.0, 0.0, 0.0);
    sphere->SetRadius(5.0);
    sphere->SetThetaResolution(48);
    sphere->SetPhiResolution(48);
    sphere->Update();

    xq_ProfilePlacementFrame frame;
    frame.pathPosIndex = 4;
    frame.position.Fill(0.0);
    frame.tangent.Fill(0.0);
    frame.tangent[2] = 1.0;
    frame.rotation.Fill(0.0);
    frame.rotation[0] = 1.0;

    auto plane = xq_SegmentationUtils::CreatePlacementPlane(frame);
    auto contourPoints = xq_SegmentationUtils::ExtractSurfaceContourOnPlane(
        sphere->GetOutput(), plane);
    if (contourPoints.size() < 12)
    {
        std::cerr << "FAIL test_extract_surface_contour_on_plane: insufficient contour points\n";
        return false;
    }

    auto centroid = xq_SegmentationUtils::ComputeContourCentroid(contourPoints);
    if (std::abs(centroid[0]) > 0.2 ||
        std::abs(centroid[1]) > 0.2 ||
        std::abs(centroid[2]) > 0.2)
    {
        std::cerr << "FAIL test_extract_surface_contour_on_plane: centroid mismatch\n";
        return false;
    }

    const double area = xq_SegmentationUtils::CalculateContourArea(contourPoints);
    if (area < 70.0)
    {
        std::cerr << "FAIL test_extract_surface_contour_on_plane: area too small\n";
        return false;
    }

    std::cout << "PASS test_extract_surface_contour_on_plane\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 19 — Surface extraction rejects disconnected multi-loop intersections
// ---------------------------------------------------------------------------

static bool test_extract_surface_contour_on_plane_rejects_disconnected_loops()
{
    auto sphereA = vtkSmartPointer<vtkSphereSource>::New();
    sphereA->SetCenter(-8.0, 0.0, 0.0);
    sphereA->SetRadius(3.0);
    sphereA->SetThetaResolution(48);
    sphereA->SetPhiResolution(48);
    sphereA->Update();

    auto sphereB = vtkSmartPointer<vtkSphereSource>::New();
    sphereB->SetCenter(8.0, 0.0, 0.0);
    sphereB->SetRadius(3.0);
    sphereB->SetThetaResolution(48);
    sphereB->SetPhiResolution(48);
    sphereB->Update();

    auto append = vtkSmartPointer<vtkAppendPolyData>::New();
    append->AddInputData(sphereA->GetOutput());
    append->AddInputData(sphereB->GetOutput());
    append->Update();

    auto plane = xq_SegmentationUtils::CreatePlacementPlane(MakeAxialFrame(0, 0.0));
    auto contourPoints = xq_SegmentationUtils::ExtractSurfaceContourOnPlane(
        append->GetOutput(), plane);
    if (!contourPoints.empty())
    {
        std::cerr << "FAIL test_extract_surface_contour_on_plane_rejects_disconnected_loops: disconnected intersections must be rejected\n";
        return false;
    }

    std::cout << "PASS test_extract_surface_contour_on_plane_rejects_disconnected_loops\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 20 — Threshold contour generation must stay on the active slice plane
// ---------------------------------------------------------------------------

static bool test_threshold_contour_respects_slice_plane()
{
    auto image = MakeThresholdSphereImage();
    auto plane = xq_SegmentationUtils::CreatePlacementPlane(MakeAxialFrame(3, 0.0));

    auto thresholdContour = xq_ThresholdContour::New();
    thresholdContour->SetImageData(image);
    thresholdContour->SetPlaneGeometry(plane);
    thresholdContour->SetThresholdValue(50.0);
    thresholdContour->GenerateContourPoints();

    const auto& contourPoints = thresholdContour->GetContourPoints();
    if (contourPoints.size() < 12)
    {
        std::cerr << "FAIL test_threshold_contour_respects_slice_plane: insufficient contour points\n";
        return false;
    }

    for (const auto& point : contourPoints)
    {
        if (std::abs(point[2]) > 1e-3)
        {
            std::cerr << "FAIL test_threshold_contour_respects_slice_plane: contour point drifted off plane\n";
            return false;
        }
    }

    const double area = xq_SegmentationUtils::CalculateContourArea(contourPoints);
    if (area < 70.0)
    {
        std::cerr << "FAIL test_threshold_contour_respects_slice_plane: planar contour area too small\n";
        return false;
    }

    std::cout << "PASS test_threshold_contour_respects_slice_plane\n";
    return true;
}

static bool test_threshold_contour_respects_oriented_image_geometry()
{
    auto image = MakeOrientedThresholdEllipsoidImage();

    mitk::Point3D centerIndex;
    centerIndex[0] = 20.0;
    centerIndex[1] = 20.0;
    centerIndex[2] = 20.0;

    mitk::Point3D centerWorld;
    image->GetGeometry()->IndexToWorld(centerIndex, centerWorld);

    xq_ProfilePlacementFrame frame;
    frame.pathPosIndex = 20;
    frame.position = centerWorld;
    frame.tangent.Fill(0.0);
    frame.tangent[0] = 1.0;
    frame.rotation.Fill(0.0);
    frame.rotation[2] = -1.0;

    auto plane = xq_SegmentationUtils::CreatePlacementPlane(frame);

    auto thresholdContour = xq_ThresholdContour::New();
    thresholdContour->SetImageData(image);
    thresholdContour->SetPlaneGeometry(plane);
    thresholdContour->SetThresholdValue(50.0);
    thresholdContour->GenerateContourPoints();

    const auto& contourPoints = thresholdContour->GetContourPoints();
    if (contourPoints.size() < 12)
    {
        std::cerr << "FAIL test_threshold_contour_respects_oriented_image_geometry: insufficient contour points\n";
        return false;
    }

    for (const auto& point : contourPoints)
    {
        mitk::Point3D contourIndex;
        image->GetGeometry()->WorldToIndex(point, contourIndex);
        if (std::abs(contourIndex[2] - centerIndex[2]) > 0.75)
        {
            std::cerr << "FAIL test_threshold_contour_respects_oriented_image_geometry: contour left the expected image slice in index space\n";
            return false;
        }
    }

    auto centroid = xq_SegmentationUtils::ComputeContourCentroid(contourPoints);
    mitk::Point3D centroidIndex;
    image->GetGeometry()->WorldToIndex(centroid, centroidIndex);
    if (std::abs(centroidIndex[0] - centerIndex[0]) > 0.75 ||
        std::abs(centroidIndex[1] - centerIndex[1]) > 0.75 ||
        std::abs(centroidIndex[2] - centerIndex[2]) > 0.75)
    {
        std::cerr << "FAIL test_threshold_contour_respects_oriented_image_geometry: centroid does not map back to the oriented image center slice\n";
        return false;
    }

    std::cout << "PASS test_threshold_contour_respects_oriented_image_geometry\n";
    return true;
}

static bool test_create_legacy_threshold_contour_node()
{
    std::vector<mitk::Point3D> contourPoints(4);
    contourPoints[0][0] = 1.0; contourPoints[0][1] = 0.0; contourPoints[0][2] = 2.0;
    contourPoints[1][0] = 0.0; contourPoints[1][1] = 1.0; contourPoints[1][2] = 2.0;
    contourPoints[2][0] = -1.0; contourPoints[2][1] = 0.0; contourPoints[2][2] = 2.0;
    contourPoints[3][0] = 0.0; contourPoints[3][1] = -1.0; contourPoints[3][2] = 2.0;

    auto contourNode = xq_SegmentationUtils::CreateLegacyThresholdContourNode(
        contourPoints, "LegacyGroup", 4);
    if (contourNode.IsNull())
    {
        std::cerr << "FAIL test_create_legacy_threshold_contour_node: helper returned null node\n";
        return false;
    }

    auto* pointSet = dynamic_cast<mitk::PointSet*>(contourNode->GetData());
    if (!pointSet)
    {
        std::cerr << "FAIL test_create_legacy_threshold_contour_node: node data is not a PointSet, so legacy lofting cannot consume it\n";
        return false;
    }

    if (pointSet->GetSize() != static_cast<unsigned int>(contourPoints.size()))
    {
        std::cerr << "FAIL test_create_legacy_threshold_contour_node: PointSet size mismatch\n";
        return false;
    }

    for (size_t i = 0; i < contourPoints.size(); ++i)
    {
        auto it = pointSet->Begin();
        std::advance(it, static_cast<long>(i));
        const auto pt = it->Value();
        if (std::abs(pt[0] - contourPoints[i][0]) > 1e-9 ||
            std::abs(pt[1] - contourPoints[i][1]) > 1e-9 ||
            std::abs(pt[2] - contourPoints[i][2]) > 1e-9)
        {
            std::cerr << "FAIL test_create_legacy_threshold_contour_node: PointSet coordinate mismatch\n";
            return false;
        }
    }

    bool isContour = false;
    contourNode->GetBoolProperty("xq.segmentation.contour", isContour);
    if (!isContour)
    {
        std::cerr << "FAIL test_create_legacy_threshold_contour_node: legacy contour flag missing\n";
        return false;
    }

    int contourIndex = -1;
    contourNode->GetIntProperty("xq.segmentation.contourindex", contourIndex);
    if (contourIndex != 4)
    {
        std::cerr << "FAIL test_create_legacy_threshold_contour_node: contour index property mismatch\n";
        return false;
    }

    std::string contourType;
    contourNode->GetStringProperty("xq.segmentation.contourtype", contourType);
    if (contourType != "Threshold")
    {
        std::cerr << "FAIL test_create_legacy_threshold_contour_node: contour type property mismatch\n";
        return false;
    }

    if (contourNode->GetName() != "LegacyGroup_threshold_4")
    {
        std::cerr << "FAIL test_create_legacy_threshold_contour_node: node name mismatch\n";
        return false;
    }

    std::cout << "PASS test_create_legacy_threshold_contour_node\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 20 — ContourGroup -> ProfileGroup migration boundary
// 
// Before fix: xq_ContourGroupMigration does not exist; no systematic
//             conversion boundary.
// After fix:  ToProfileGroup() converts slices to canonical profiles,
//             preserves anchor points and method strings, and keeps ordinal
//             fallback only for truly unbound legacy contour groups.
// ---------------------------------------------------------------------------

static bool test_contour_group_migration()
{
    auto src = xq_ContourGroup::New();

    // Slice 0: two anchor points, method "manual"
    ContourSlice s0;
    s0.slicePosition = 0.0;
    s0.method = "manual";
    mitk::Point3D p0; p0[0] = 1.0; p0[1] = 0.0; p0[2] = 0.0;
    mitk::Point3D p1; p1[0] = 0.0; p1[1] = 1.0; p1[2] = 0.0;
    s0.points = {p0, p1};
    src->AddContour(s0);

    // Slice 1: one anchor point, method "threshold"
    ContourSlice s1;
    s1.slicePosition = 2.5;
    s1.method = "threshold";
    mitk::Point3D p2; p2[0] = 2.0; p2[1] = 0.5; p2[2] = 1.0;
    s1.points = {p2};
    src->AddContour(s1);

    auto dst = xq_ContourGroupMigration::ToProfileGroup(src.GetPointer());
    if (!dst)
    {
        std::cerr << "FAIL test_contour_group_migration: ToProfileGroup returned null\n";
        return false;
    }

    // Profile count must match contour count.
    if (dst->GetProfileCount() != 2)
    {
        std::cerr << "FAIL test_contour_group_migration: expected 2 profiles, got "
                  << dst->GetProfileCount() << "\n";
        return false;
    }

    // Truly unbound groups must remain unbound after migration.
    if (!dst->GetAttribute("path_name").empty())
    {
        std::cerr << "FAIL test_contour_group_migration: unexpected path_name attribute '"
                  << dst->GetAttribute("path_name") << "'\n";
        return false;
    }

    // Ordinal path position rule: slice 0 -> pathPosIndex 0, slice 1 -> pathPosIndex 1.
    auto* prof0 = dst->GetProfileAtPathPos(0);
    auto* prof1 = dst->GetProfileAtPathPos(1);
    if (!prof0 || !prof1)
    {
        std::cerr << "FAIL test_contour_group_migration: "
                  << "expected profiles at ordinal positions 0 and 1\n";
        return false;
    }

    // Method strings must be preserved verbatim.
    if (prof0->GetMethod() != "manual")
    {
        std::cerr << "FAIL test_contour_group_migration: "
                  << "method mismatch for profile 0, got '" << prof0->GetMethod() << "'\n";
        return false;
    }
    if (prof1->GetMethod() != "threshold")
    {
        std::cerr << "FAIL test_contour_group_migration: "
                  << "method mismatch for profile 1, got '" << prof1->GetMethod() << "'\n";
        return false;
    }

    // Anchor point counts must match the original contour point counts.
    if (prof0->GetAnchorPointCount() != 2)
    {
        std::cerr << "FAIL test_contour_group_migration: "
                  << "expected 2 anchor points for profile 0, got "
                  << prof0->GetAnchorPointCount() << "\n";
        return false;
    }
    if (prof1->GetAnchorPointCount() != 1)
    {
        std::cerr << "FAIL test_contour_group_migration: "
                  << "expected 1 anchor point for profile 1, got "
                  << prof1->GetAnchorPointCount() << "\n";
        return false;
    }

    // Anchor point values must be preserved (check first point of slice 0).
    auto anchorPt = prof0->GetAnchorPoint(0);
    if (std::abs(anchorPt[0] - 1.0) > 1e-9 || std::abs(anchorPt[1]) > 1e-9)
    {
        std::cerr << "FAIL test_contour_group_migration: "
                  << "anchor point[0] of profile 0 incorrect\n";
        return false;
    }

    // After migration the loft cache must be dirty — profiles exist but no mesh.
    if (!dst->IsLoftCacheDirty())
    {
        std::cerr << "FAIL test_contour_group_migration: "
                  << "migrated group should have dirty loft cache (no mesh yet)\n";
        return false;
    }

    // Empty source: must produce an empty (not null) ProfileGroup.
    auto srcEmpty = xq_ContourGroup::New();
    auto dstEmpty = xq_ContourGroupMigration::ToProfileGroup(srcEmpty.GetPointer());
    if (!dstEmpty || dstEmpty->GetProfileCount() != 0)
    {
        std::cerr << "FAIL test_contour_group_migration: empty source must yield empty group\n";
        return false;
    }

    std::cout << "PASS test_contour_group_migration\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 20 — Path-bound legacy migration rejects missing placement frames
// ---------------------------------------------------------------------------

static bool test_contour_group_migration_requires_frames_for_path_bound_groups()
{
    auto src = xq_ContourGroup::New();
    src->SetPathName("test_vessel_path");

    ContourSlice s0;
    s0.slicePosition = 0.0;
    s0.method = "manual";
    mitk::Point3D p0; p0[0] = 1.0; p0[1] = 0.0; p0[2] = 0.0;
    mitk::Point3D p1; p1[0] = 0.0; p1[1] = 1.0; p1[2] = 0.0;
    s0.points = {p0, p1};
    src->AddContour(s0);

    ContourSlice s1;
    s1.slicePosition = 2.5;
    s1.method = "threshold";
    mitk::Point3D p2; p2[0] = 2.0; p2[1] = 0.5; p2[2] = 1.0;
    s1.points = {p2};
    src->AddContour(s1);

    auto dst = xq_ContourGroupMigration::ToProfileGroup(src.GetPointer());
    if (!dst)
    {
        std::cerr << "FAIL test_contour_group_migration_requires_frames_for_path_bound_groups: ToProfileGroup returned null\n";
        return false;
    }

    if (dst->GetAttribute("path_name") != "test_vessel_path")
    {
        std::cerr << "FAIL test_contour_group_migration_requires_frames_for_path_bound_groups: path_name attribute mismatch, got '"
                  << dst->GetAttribute("path_name") << "'\n";
        return false;
    }

    if (dst->GetProfileCount() != 0)
    {
        std::cerr << "FAIL test_contour_group_migration_requires_frames_for_path_bound_groups: path-bound migration without frames must not invent ordinal fallback, got "
                  << dst->GetProfileCount() << " profile(s)\n";
        return false;
    }

    if (dst->HasProfile(0) || dst->HasProfile(1))
    {
        std::cerr << "FAIL test_contour_group_migration_requires_frames_for_path_bound_groups: migration must not populate ordinal path slots for unresolved path-bound contours\n";
        return false;
    }

    if (dst->GetAttribute("migration_unresolved_count") != "2")
    {
        std::cerr << "FAIL test_contour_group_migration_requires_frames_for_path_bound_groups: expected two unresolved contours, got '"
                  << dst->GetAttribute("migration_unresolved_count") << "'\n";
        return false;
    }

    std::cout << "PASS test_contour_group_migration_requires_frames_for_path_bound_groups\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 21 — ContourGroup migration can bind real placement frames up front
// ---------------------------------------------------------------------------

static bool test_contour_group_migration_binds_placement_frames()
{
    auto src = xq_ContourGroup::New();
    src->SetPathName("framed_path");

    ContourSlice s0;
    mitk::Point3D a0; a0[0] = -1.0; a0[1] = 0.0; a0[2] = 10.0;
    mitk::Point3D a1; a1[0] =  1.0; a1[1] = 0.0; a1[2] = 10.0;
    s0.points = {a0, a1};
    src->AddContour(s0);

    ContourSlice s1;
    mitk::Point3D b0; b0[0] = -1.0; b0[1] = 0.0; b0[2] = 20.0;
    mitk::Point3D b1; b1[0] =  1.0; b1[1] = 0.0; b1[2] = 20.0;
    s1.points = {b0, b1};
    src->AddContour(s1);

    std::vector<xq_ProfilePlacementFrame> frames(2);
    frames[0].pathPosIndex = 7;
    frames[0].position[0] = 0.0;
    frames[0].position[1] = 0.0;
    frames[0].position[2] = 10.0;
    frames[0].tangent.Fill(0.0);
    frames[0].tangent[2] = 1.0;
    frames[0].rotation.Fill(0.0);
    frames[0].rotation[0] = 1.0;

    frames[1].pathPosIndex = 42;
    frames[1].position[0] = 0.0;
    frames[1].position[1] = 0.0;
    frames[1].position[2] = 20.0;
    frames[1].tangent = frames[0].tangent;
    frames[1].rotation = frames[0].rotation;

    auto dst = xq_ContourGroupMigration::ToProfileGroup(src.GetPointer(), frames);
    if (!dst)
    {
        std::cerr << "FAIL test_contour_group_migration_binds_placement_frames: ToProfileGroup returned null\n";
        return false;
    }

    if (dst->GetProfileCount() != 2)
    {
        std::cerr << "FAIL test_contour_group_migration_binds_placement_frames: expected 2 profiles, got "
                  << dst->GetProfileCount() << "\n";
        return false;
    }

    if (dst->HasProfile(0) || dst->HasProfile(1))
    {
        std::cerr << "FAIL test_contour_group_migration_binds_placement_frames: migration should not keep ordinal fallback when placement frames are available\n";
        return false;
    }

    auto* profA = dst->GetProfileAtPathPos(7);
    auto* profB = dst->GetProfileAtPathPos(42);
    if (!profA || !profB)
    {
        std::cerr << "FAIL test_contour_group_migration_binds_placement_frames: expected profiles at bound path positions 7 and 42\n";
        return false;
    }

    if (profA->GetSlicePlane().IsNull() || profB->GetSlicePlane().IsNull())
    {
        std::cerr << "FAIL test_contour_group_migration_binds_placement_frames: placement-bound profiles should carry slice planes\n";
        return false;
    }

    const auto centerA = profA->GetProfileCenter();
    const auto centerB = profB->GetProfileCenter();
    if (std::abs(centerA[2] - 10.0) > 1e-9 || std::abs(centerB[2] - 20.0) > 1e-9)
    {
        std::cerr << "FAIL test_contour_group_migration_binds_placement_frames: profile centers should remain aligned with matched placement frames\n";
        return false;
    }

    std::cout << "PASS test_contour_group_migration_binds_placement_frames\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 21 — ApplyPlacementFrame relocates existing geometry into target plane
// ---------------------------------------------------------------------------

static bool test_apply_placement_frame_relocates_existing_geometry()
{
    mitk::Point3D center;
    center[0] = 0.0;
    center[1] = 0.0;
    center[2] = 2.0;

    auto profile = xq_SegmentationUtils::CreateEditableProfile("Manual", center);
    if (!profile)
    {
        std::cerr << "FAIL test_apply_placement_frame_relocates_existing_geometry: profile helper returned null\n";
        return false;
    }

    std::vector<mitk::Point3D> anchors(4);
    anchors[0][0] = -1.0; anchors[0][1] = -1.0; anchors[0][2] = 2.0;
    anchors[1][0] =  1.0; anchors[1][1] = -1.0; anchors[1][2] = 2.0;
    anchors[2][0] =  1.0; anchors[2][1] =  1.0; anchors[2][2] = 2.0;
    anchors[3][0] = -1.0; anchors[3][1] =  1.0; anchors[3][2] = 2.0;
    profile->SetAnchorPoints(anchors);

    xq_ProfilePlacementFrame sourceFrame;
    sourceFrame.pathPosIndex = 2;
    sourceFrame.position = center;
    sourceFrame.tangent.Fill(0.0);
    sourceFrame.tangent[2] = 1.0;
    sourceFrame.rotation.Fill(0.0);
    sourceFrame.rotation[0] = 1.0;
    xq_SegmentationUtils::ApplyPlacementFrame(profile.get(), sourceFrame);

    xq_ProfilePlacementFrame targetFrame;
    targetFrame.pathPosIndex = 8;
    targetFrame.position[0] = 5.0;
    targetFrame.position[1] = 6.0;
    targetFrame.position[2] = 7.0;
    targetFrame.tangent.Fill(0.0);
    targetFrame.tangent[0] = 1.0;
    targetFrame.rotation.Fill(0.0);
    targetFrame.rotation[2] = 1.0;

    xq_SegmentationUtils::ApplyPlacementFrame(profile.get(), targetFrame);

    if (profile->GetPathPosIndex() != 8)
    {
        std::cerr << "FAIL test_apply_placement_frame_relocates_existing_geometry: pathPosIndex mismatch\n";
        return false;
    }

    const auto relocatedCenter = profile->GetProfileCenter();
    if (std::abs(relocatedCenter[0] - 5.0) > 1e-9 ||
        std::abs(relocatedCenter[1] - 6.0) > 1e-9 ||
        std::abs(relocatedCenter[2] - 7.0) > 1e-9)
    {
        std::cerr << "FAIL test_apply_placement_frame_relocates_existing_geometry: center mismatch\n";
        return false;
    }

    for (int i = 0; i < profile->GetAnchorPointCount(); ++i)
    {
        const auto anchor = profile->GetAnchorPoint(i);
        if (std::abs(anchor[0] - 5.0) > 1e-9)
        {
            std::cerr << "FAIL test_apply_placement_frame_relocates_existing_geometry: anchor not moved into target plane\n";
            return false;
        }
    }

    const auto contourPoints = profile->GetProfilePoints();
    const auto contourCenter = xq_SegmentationUtils::ComputeContourCentroid(contourPoints);
    if (std::abs(contourCenter[0] - 5.0) > 1e-9 ||
        std::abs(contourCenter[1] - 6.0) > 1e-9 ||
        std::abs(contourCenter[2] - 7.0) > 1e-9)
    {
        std::cerr << "FAIL test_apply_placement_frame_relocates_existing_geometry: contour centroid mismatch\n";
        return false;
    }

    std::cout << "PASS test_apply_placement_frame_relocates_existing_geometry\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 22 — Rebinding migrated groups preserves unique frame assignments
// ---------------------------------------------------------------------------

static bool test_rebind_profile_group_to_placement_frames_preserves_profiles()
{
    auto group = xq_ProfileGroup::New();

    auto profileA = xq_SegmentationUtils::CreateEditableProfile("Manual", mitk::Point3D());
    auto profileB = xq_SegmentationUtils::CreateEditableProfile("Manual", mitk::Point3D());
    if (!profileA || !profileB)
    {
        std::cerr << "FAIL test_rebind_profile_group_to_placement_frames_preserves_profiles: profile helper returned null\n";
        return false;
    }

    std::vector<mitk::Point3D> anchorsA(4);
    anchorsA[0][0] = -0.5; anchorsA[0][1] = -0.5; anchorsA[0][2] = 0.05;
    anchorsA[1][0] =  0.5; anchorsA[1][1] = -0.5; anchorsA[1][2] = 0.05;
    anchorsA[2][0] =  0.5; anchorsA[2][1] =  0.5; anchorsA[2][2] = 0.05;
    anchorsA[3][0] = -0.5; anchorsA[3][1] =  0.5; anchorsA[3][2] = 0.05;
    profileA->SetAnchorPoints(anchorsA);
    profileA->SetPathPosIndex(0);

    std::vector<mitk::Point3D> anchorsB(4);
    anchorsB[0][0] = -0.5; anchorsB[0][1] = -0.5; anchorsB[0][2] = 0.20;
    anchorsB[1][0] =  0.5; anchorsB[1][1] = -0.5; anchorsB[1][2] = 0.20;
    anchorsB[2][0] =  0.5; anchorsB[2][1] =  0.5; anchorsB[2][2] = 0.20;
    anchorsB[3][0] = -0.5; anchorsB[3][1] =  0.5; anchorsB[3][2] = 0.20;
    profileB->SetAnchorPoints(anchorsB);
    profileB->SetPathPosIndex(1);

    group->AppendProfile(profileA.release(), 0);
    group->AppendProfile(profileB.release(), 1);

    std::vector<xq_ProfilePlacementFrame> frames(3);
    frames[0].pathPosIndex = 10;
    frames[0].position.Fill(0.0);
    frames[0].tangent.Fill(0.0);
    frames[0].tangent[2] = 1.0;
    frames[0].rotation.Fill(0.0);
    frames[0].rotation[0] = 1.0;

    frames[1].pathPosIndex = 11;
    frames[1].position[0] = 0.0;
    frames[1].position[1] = 0.0;
    frames[1].position[2] = 1.0;
    frames[1].tangent = frames[0].tangent;
    frames[1].rotation = frames[0].rotation;

    frames[2].pathPosIndex = 12;
    frames[2].position[0] = 0.0;
    frames[2].position[1] = 0.0;
    frames[2].position[2] = 2.0;
    frames[2].tangent = frames[0].tangent;
    frames[2].rotation = frames[0].rotation;

    if (!xq_SegmentationUtils::RebindProfileGroupToPlacementFrames(group, frames))
    {
        std::cerr << "FAIL test_rebind_profile_group_to_placement_frames_preserves_profiles: rebind helper returned false\n";
        return false;
    }

    if (group->GetProfileCount() != 2)
    {
        std::cerr << "FAIL test_rebind_profile_group_to_placement_frames_preserves_profiles: expected 2 profiles after rebind, got "
                  << group->GetProfileCount() << "\n";
        return false;
    }

    const auto indices = group->GetProfilePathIndices();
    if (indices.size() != 2 || indices[0] != 10 || indices[1] != 11)
    {
        std::cerr << "FAIL test_rebind_profile_group_to_placement_frames_preserves_profiles: expected distinct rebound indices 10 and 11\n";
        return false;
    }

    std::cout << "PASS test_rebind_profile_group_to_placement_frames_preserves_profiles\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 23 — Rebinding must reject unmatched profiles instead of inventing indices
// ---------------------------------------------------------------------------

static bool test_rebind_profile_group_to_placement_frames_rejects_unmatched_profiles()
{
    auto group = xq_ProfileGroup::New();
    group->SetAttribute("path_name", "centerline_rebind");

    for (int i = 0; i < 3; ++i)
    {
        auto profile = xq_SegmentationUtils::CreateEditableProfile("Manual", mitk::Point3D());
        if (!profile)
        {
            std::cerr << "FAIL test_rebind_profile_group_to_placement_frames_rejects_unmatched_profiles: profile helper returned null\n";
            return false;
        }

        std::vector<mitk::Point3D> anchors(4);
        anchors[0][0] = -0.5; anchors[0][1] = -0.5; anchors[0][2] = static_cast<double>(i) + 0.05;
        anchors[1][0] =  0.5; anchors[1][1] = -0.5; anchors[1][2] = static_cast<double>(i) + 0.05;
        anchors[2][0] =  0.5; anchors[2][1] =  0.5; anchors[2][2] = static_cast<double>(i) + 0.05;
        anchors[3][0] = -0.5; anchors[3][1] =  0.5; anchors[3][2] = static_cast<double>(i) + 0.05;
        profile->SetAnchorPoints(anchors);
        profile->SetPathPosIndex(i);
        group->AppendProfile(profile.release(), i);
    }

    std::vector<xq_ProfilePlacementFrame> frames(2);
    frames[0].pathPosIndex = 10;
    frames[0].position.Fill(0.0);
    frames[0].tangent.Fill(0.0);
    frames[0].tangent[2] = 1.0;
    frames[0].rotation.Fill(0.0);
    frames[0].rotation[0] = 1.0;

    frames[1].pathPosIndex = 11;
    frames[1].position[2] = 1.0;
    frames[1].tangent = frames[0].tangent;
    frames[1].rotation = frames[0].rotation;

    if (xq_SegmentationUtils::RebindProfileGroupToPlacementFrames(group, frames))
    {
        std::cerr << "FAIL test_rebind_profile_group_to_placement_frames_rejects_unmatched_profiles: helper should reject unmatched path-bound profiles\n";
        return false;
    }

    if (group->GetProfileCount() != 3)
    {
        std::cerr << "FAIL test_rebind_profile_group_to_placement_frames_rejects_unmatched_profiles: failed rebind must leave original profiles intact\n";
        return false;
    }

    const auto indices = group->GetProfilePathIndices();
    if (indices != std::vector<int>{0, 1, 2})
    {
        std::cerr << "FAIL test_rebind_profile_group_to_placement_frames_rejects_unmatched_profiles: failed rebind changed original path indices\n";
        return false;
    }

    std::cout << "PASS test_rebind_profile_group_to_placement_frames_rejects_unmatched_profiles\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 24 — Path-bound canonical groups must not use synthetic fallback placement
// ---------------------------------------------------------------------------

static bool test_requires_path_placement_for_canonical_groups()
{
    auto profileGroup = xq_ProfileGroup::New();
    if (xq_SegmentationUtils::RequiresPathPlacement(profileGroup))
    {
        std::cerr << "FAIL test_requires_path_placement_for_canonical_groups: empty path_name should not require path placement\n";
        return false;
    }

    profileGroup->SetAttribute("path_name", "centerline_a");
    if (!xq_SegmentationUtils::RequiresPathPlacement(profileGroup))
    {
        std::cerr << "FAIL test_requires_path_placement_for_canonical_groups: path-bound group should require path placement\n";
        return false;
    }

    std::cout << "PASS test_requires_path_placement_for_canonical_groups\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 25 — Canonical path index resolution only falls back for unbound groups
// ---------------------------------------------------------------------------

static bool test_resolve_canonical_path_pos_index()
{
    auto unboundGroup = xq_ProfileGroup::New();
    auto existingA = xq_SegmentationUtils::CreateEditableProfile("Manual", mitk::Point3D());
    auto existingB = xq_SegmentationUtils::CreateEditableProfile("Manual", mitk::Point3D());
    if (!existingA || !existingB)
    {
        std::cerr << "FAIL test_resolve_canonical_path_pos_index: profile helper returned null\n";
        return false;
    }
    unboundGroup->AppendProfile(existingA.release(), 0);
    unboundGroup->AppendProfile(existingB.release(), 4);

    int resolvedIndex = -1;
    if (!xq_SegmentationUtils::ResolveCanonicalPathPosIndex(unboundGroup, nullptr, resolvedIndex))
    {
        std::cerr << "FAIL test_resolve_canonical_path_pos_index: unbound group should allow synthetic fallback\n";
        return false;
    }
    if (resolvedIndex != 5)
    {
        std::cerr << "FAIL test_resolve_canonical_path_pos_index: expected fallback index 5, got "
                  << resolvedIndex << "\n";
        return false;
    }

    xq_ProfilePlacementFrame frame;
    frame.pathPosIndex = 12;
    if (!xq_SegmentationUtils::ResolveCanonicalPathPosIndex(unboundGroup, &frame, resolvedIndex) ||
        resolvedIndex != 12)
    {
        std::cerr << "FAIL test_resolve_canonical_path_pos_index: explicit placement index should win\n";
        return false;
    }

    auto boundGroup = xq_ProfileGroup::New();
    boundGroup->SetAttribute("path_name", "centerline_b");
    if (xq_SegmentationUtils::ResolveCanonicalPathPosIndex(boundGroup, nullptr, resolvedIndex))
    {
        std::cerr << "FAIL test_resolve_canonical_path_pos_index: path-bound group should reject synthetic fallback\n";
        return false;
    }

    std::cout << "PASS test_resolve_canonical_path_pos_index\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 26 — Canonical writeback warnings encode placement-failure policy
// ---------------------------------------------------------------------------

static bool test_canonical_writeback_warning_policy()
{
    auto boundGroup = xq_ProfileGroup::New();
    boundGroup->SetAttribute("path_name", "centerline_c");

    if (xq_SegmentationUtils::GetCanonicalWritebackWarningReason(
            boundGroup, false, false) !=
        xq_CanonicalWritebackWarningReason::SlicePlacementUnavailable)
    {
        std::cerr << "FAIL test_canonical_writeback_warning_policy: missing slice plane should report unavailable placement\n";
        return false;
    }

    if (xq_SegmentationUtils::GetCanonicalWritebackWarningReason(
            boundGroup, true, false) !=
        xq_CanonicalWritebackWarningReason::PlacementUnresolved)
    {
        std::cerr << "FAIL test_canonical_writeback_warning_policy: unresolved frame should report placement resolution failure\n";
        return false;
    }

    if (xq_SegmentationUtils::GetCanonicalWritebackWarningReason(
            boundGroup, true, true) !=
        xq_CanonicalWritebackWarningReason::None)
    {
        std::cerr << "FAIL test_canonical_writeback_warning_policy: valid placement should not emit a warning\n";
        return false;
    }

    auto unboundGroup = xq_ProfileGroup::New();
    if (xq_SegmentationUtils::GetCanonicalWritebackWarningReason(
            unboundGroup, false, false) !=
        xq_CanonicalWritebackWarningReason::None)
    {
        std::cerr << "FAIL test_canonical_writeback_warning_policy: unbound group should not require placement warnings\n";
        return false;
    }

    if (xq_SegmentationUtils::GetCanonicalWritebackWarningMessage(
            xq_CanonicalWritebackWarningContext::AutoSegmentation,
            xq_CanonicalWritebackWarningReason::SlicePlacementUnavailable) !=
        "Cannot write the 3D segmentation result back into the canonical profile group because the current centerline slice placement is unavailable. The surface preview will still be created.")
    {
        std::cerr << "FAIL test_canonical_writeback_warning_policy: auto-seg unavailable-placement warning text mismatch\n";
        return false;
    }

    if (xq_SegmentationUtils::GetCanonicalWritebackWarningMessage(
            xq_CanonicalWritebackWarningContext::AutoSegmentation,
            xq_CanonicalWritebackWarningReason::PlacementUnresolved) !=
        "Cannot write the 3D segmentation result back into the canonical profile group because the current centerline placement could not be resolved. The surface preview will still be created.")
    {
        std::cerr << "FAIL test_canonical_writeback_warning_policy: auto-seg unresolved-placement warning text mismatch\n";
        return false;
    }

    if (xq_SegmentationUtils::GetCanonicalWritebackWarningMessage(
            xq_CanonicalWritebackWarningContext::ThresholdContour,
            xq_CanonicalWritebackWarningReason::PlacementUnresolved) !=
        "Cannot convert the threshold contour into a canonical profile because the current centerline placement could not be resolved.")
    {
        std::cerr << "FAIL test_canonical_writeback_warning_policy: threshold unresolved-placement warning text mismatch\n";
        return false;
    }

    std::cout << "PASS test_canonical_writeback_warning_policy\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 26 — View writeback decisions preserve threshold/auto-seg flow control
// ---------------------------------------------------------------------------

static bool test_canonical_writeback_view_decisions()
{
    auto boundGroup = xq_ProfileGroup::New();
    boundGroup->SetAttribute("path_name", "centerline_view");

    const auto thresholdDecision =
        xq_SegmentationUtils::GetThresholdContourWritebackDecision(boundGroup, true, false);
    if (!thresholdDecision.ShouldWarn() ||
        thresholdDecision.warningReason != xq_CanonicalWritebackWarningReason::PlacementUnresolved ||
        thresholdDecision.ShouldAttemptCanonicalWriteback() ||
        !thresholdDecision.ShouldReturnEarly() ||
        thresholdDecision.ShouldCreateSurfacePreview())
    {
        std::cerr << "FAIL test_canonical_writeback_view_decisions: threshold unresolved placement should warn and return early\n";
        return false;
    }

    const auto autoSegDecision =
        xq_SegmentationUtils::GetAutoSegmentationWritebackDecision(boundGroup, false, false);
    if (!autoSegDecision.ShouldWarn() ||
        autoSegDecision.warningReason != xq_CanonicalWritebackWarningReason::SlicePlacementUnavailable ||
        autoSegDecision.ShouldAttemptCanonicalWriteback() ||
        autoSegDecision.ShouldReturnEarly() ||
        !autoSegDecision.ShouldCreateSurfacePreview())
    {
        std::cerr << "FAIL test_canonical_writeback_view_decisions: auto-seg missing slice placement should warn but keep preview creation alive\n";
        return false;
    }

    const auto thresholdBoundDecision =
        xq_SegmentationUtils::GetThresholdContourWritebackDecision(boundGroup, true, true);
    if (thresholdBoundDecision.ShouldWarn() ||
        !thresholdBoundDecision.ShouldAttemptCanonicalWriteback() ||
        thresholdBoundDecision.ShouldReturnEarly() ||
        !thresholdBoundDecision.ShouldCreateSurfacePreview())
    {
        std::cerr << "FAIL test_canonical_writeback_view_decisions: resolved threshold writeback should proceed normally\n";
        return false;
    }

    auto unboundGroup = xq_ProfileGroup::New();
    const auto unboundAutoSegDecision =
        xq_SegmentationUtils::GetAutoSegmentationWritebackDecision(unboundGroup, false, false);
    if (unboundAutoSegDecision.ShouldWarn() ||
        !unboundAutoSegDecision.ShouldAttemptCanonicalWriteback() ||
        unboundAutoSegDecision.ShouldReturnEarly() ||
        !unboundAutoSegDecision.ShouldCreateSurfacePreview())
    {
        std::cerr << "FAIL test_canonical_writeback_view_decisions: unbound groups should not block canonical writeback flow\n";
        return false;
    }

    std::cout << "PASS test_canonical_writeback_view_decisions\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 28 — Auto-seg writeback must resolve placement before deciding
// ---------------------------------------------------------------------------

static bool test_auto_segmentation_writeback_resolves_placement_before_deciding()
{
    auto boundGroup = xq_ProfileGroup::New();
    boundGroup->SetAttribute("path_name", "centerline_view_resolved");

    const auto frame0 = MakeAxialFrame(2, 0.0);
    const auto frame1 = MakeAxialFrame(11, 10.0);
    const std::vector<xq_ProfilePlacementFrame> frames = {frame0, frame1};
    auto plane = xq_SegmentationUtils::CreatePlacementPlane(frame1);

    xq_ProfilePlacementFrame resolvedFrame;
    const auto decision =
        xq_SegmentationUtils::ResolveAutoSegmentationWritebackDecisionForPlacementFrames(
            boundGroup, plane, frames, &resolvedFrame);

    if (resolvedFrame.pathPosIndex != 11)
    {
        std::cerr << "FAIL test_auto_segmentation_writeback_resolves_placement_before_deciding: "
                  << "expected helper to resolve placement frame 11, got "
                  << resolvedFrame.pathPosIndex << "\n";
        return false;
    }

    if (decision.ShouldWarn() ||
        !decision.ShouldAttemptCanonicalWriteback() ||
        decision.ShouldReturnEarly() ||
        !decision.ShouldCreateSurfacePreview())
    {
        std::cerr << "FAIL test_auto_segmentation_writeback_resolves_placement_before_deciding: "
                  << "resolved frame should allow canonical writeback\n";
        return false;
    }

    std::cout << "PASS test_auto_segmentation_writeback_resolves_placement_before_deciding\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 29 — ProfileGroup readiness tracks unresolved placement metadata
// ---------------------------------------------------------------------------

static bool test_profile_group_loft_readiness_api()
{
    auto boundGroup = xq_ProfileGroup::New();
    boundGroup->SetAttribute("path_name", "centerline_ready_api");
    mitk::Point3D origin;
    origin[0] = 0.0;
    origin[1] = 0.0;
    origin[2] = 0.0;

    auto unresolvedProfile = xq_SegmentationUtils::CreateEditableProfile(
        "Manual", origin);
    if (!unresolvedProfile)
    {
        std::cerr << "FAIL test_profile_group_loft_readiness_api: unresolved profile helper returned null\n";
        return false;
    }
    unresolvedProfile->SetPathPosIndex(7);
    boundGroup->AppendProfile(unresolvedProfile.release(), 7);

    const auto unresolvedIndices = boundGroup->GetUnresolvedProfilePathIndices();
    if (unresolvedIndices.size() != 1 || unresolvedIndices.front() != 7)
    {
        std::cerr << "FAIL test_profile_group_loft_readiness_api: expected unresolved index {7}\n";
        return false;
    }

    if (boundGroup->IsReadyForLoft())
    {
        std::cerr << "FAIL test_profile_group_loft_readiness_api: path-bound group without slice plane must not be ready for loft\n";
        return false;
    }

    auto readyGroup = xq_ProfileGroup::New();
    readyGroup->SetAttribute("path_name", "centerline_ready_api_ok");

    const auto frame0 = MakeAxialFrame(3, 0.0);
    const auto frame1 = MakeAxialFrame(4, 5.0);

    auto profile0 = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", frame0.position, 2.0, 0.0, 24);
    auto profile1 = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", frame1.position, 2.0, 0.0, 24);
    if (!profile0 || !profile1)
    {
        std::cerr << "FAIL test_profile_group_loft_readiness_api: preset profile helper returned null\n";
        return false;
    }

    xq_SegmentationUtils::ApplyPlacementFrame(profile0.get(), frame0);
    xq_SegmentationUtils::ApplyPlacementFrame(profile1.get(), frame1);
    readyGroup->AppendProfile(profile0.release(), 3);
    readyGroup->AppendProfile(profile1.release(), 4);

    if (!readyGroup->GetUnresolvedProfilePathIndices().empty())
    {
        std::cerr << "FAIL test_profile_group_loft_readiness_api: fully placed profiles should not remain unresolved\n";
        return false;
    }

    if (!readyGroup->IsReadyForLoft())
    {
        std::cerr << "FAIL test_profile_group_loft_readiness_api: fully placed profile group should be ready for loft\n";
        return false;
    }

    auto gappedGroup = xq_ProfileGroup::New();
    gappedGroup->SetAttribute("path_name", "centerline_ready_api_gap");
    auto gapProfile0 = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", frame0.position, 2.0, 0.0, 24);
    auto gapProfile2 = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", MakeAxialFrame(2, 10.0).position, 2.0, 0.0, 24);
    if (!gapProfile0 || !gapProfile2)
    {
        std::cerr << "FAIL test_profile_group_loft_readiness_api: gapped profile helper returned null\n";
        return false;
    }

    xq_SegmentationUtils::ApplyPlacementFrame(gapProfile0.get(), frame0);
    xq_SegmentationUtils::ApplyPlacementFrame(gapProfile2.get(), MakeAxialFrame(2, 10.0));
    gappedGroup->AppendProfile(gapProfile0.release(), 0);
    gappedGroup->AppendProfile(gapProfile2.release(), 2);

    if (gappedGroup->IsReadyForLoft())
    {
        std::cerr << "FAIL test_profile_group_loft_readiness_api: path-bound group with missing intermediate path slot must not be ready for loft\n";
        return false;
    }

    std::cout << "PASS test_profile_group_loft_readiness_api\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 30 — Loft readiness helper reports blocking reason clearly
// ---------------------------------------------------------------------------

static bool test_loft_readiness_blocking_reason()
{
    auto sparseGroup = xq_ProfileGroup::New();
    mitk::Point3D sparseCenter;
    sparseCenter[0] = 0.0;
    sparseCenter[1] = 0.0;
    sparseCenter[2] = 0.0;
    auto singleProfile = xq_SegmentationUtils::CreateEditableProfile(
        "Manual", sparseCenter);
    if (!singleProfile)
    {
        std::cerr << "FAIL test_loft_readiness_blocking_reason: single profile helper returned null\n";
        return false;
    }
    singleProfile->SetPathPosIndex(0);
    sparseGroup->AppendProfile(singleProfile.release(), 0);

    const auto sparseReason =
        xq_SegmentationUtils::GetLoftReadinessBlockingReason(sparseGroup);
    if (sparseReason.find("At least two canonical profiles") == std::string::npos)
    {
        std::cerr << "FAIL test_loft_readiness_blocking_reason: sparse-group reason mismatch\n";
        return false;
    }

    auto unresolvedGroup = xq_ProfileGroup::New();
    unresolvedGroup->SetAttribute("path_name", "centerline_reason");
    mitk::Point3D unresolvedCenter;
    unresolvedCenter[0] = 1.0;
    unresolvedCenter[1] = 0.0;
    unresolvedCenter[2] = 0.0;
    mitk::Point3D resolvedCenter;
    resolvedCenter[0] = 0.0;
    resolvedCenter[1] = 0.0;
    resolvedCenter[2] = 5.0;
    auto unresolvedProfile = xq_SegmentationUtils::CreateEditableProfile(
        "Manual", unresolvedCenter);
    auto resolvedProfile = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", resolvedCenter, 1.5, 0.0, 24);
    if (!unresolvedProfile || !resolvedProfile)
    {
        std::cerr << "FAIL test_loft_readiness_blocking_reason: profile helper returned null\n";
        return false;
    }
    unresolvedProfile->SetPathPosIndex(7);
    unresolvedGroup->AppendProfile(unresolvedProfile.release(), 7);
    const auto resolvedFrame = MakeAxialFrame(8, 5.0);
    xq_SegmentationUtils::ApplyPlacementFrame(resolvedProfile.get(), resolvedFrame);
    unresolvedGroup->AppendProfile(resolvedProfile.release(), 8);

    const auto unresolvedReason =
        xq_SegmentationUtils::GetLoftReadinessBlockingReason(unresolvedGroup);
    if (unresolvedReason.find("7") == std::string::npos ||
        unresolvedReason.find("placement") == std::string::npos)
    {
        std::cerr << "FAIL test_loft_readiness_blocking_reason: unresolved-placement reason mismatch\n";
        return false;
    }

    auto gappedGroup = xq_ProfileGroup::New();
    gappedGroup->SetAttribute("path_name", "centerline_reason_gap");
    auto gapFrame0 = MakeAxialFrame(0, 0.0);
    auto gapFrame2 = MakeAxialFrame(2, 10.0);
    auto gapProfile0 = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", gapFrame0.position, 1.0, 0.0, 24);
    auto gapProfile2 = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", gapFrame2.position, 1.0, 0.0, 24);
    if (!gapProfile0 || !gapProfile2)
    {
        std::cerr << "FAIL test_loft_readiness_blocking_reason: gap profile helper returned null\n";
        return false;
    }
    xq_SegmentationUtils::ApplyPlacementFrame(gapProfile0.get(), gapFrame0);
    xq_SegmentationUtils::ApplyPlacementFrame(gapProfile2.get(), gapFrame2);
    gappedGroup->AppendProfile(gapProfile0.release(), 0);
    gappedGroup->AppendProfile(gapProfile2.release(), 2);

    const auto gapReason =
        xq_SegmentationUtils::GetLoftReadinessBlockingReason(gappedGroup);
    if (gapReason.find("gap") == std::string::npos ||
        gapReason.find("1") == std::string::npos)
    {
        std::cerr << "FAIL test_loft_readiness_blocking_reason: gap reason mismatch\n";
        return false;
    }

    auto readyGroup = xq_ProfileGroup::New();
    readyGroup->SetAttribute("path_name", "centerline_reason_ok");
    mitk::Point3D readyCenter0;
    readyCenter0[0] = 0.0;
    readyCenter0[1] = 0.0;
    readyCenter0[2] = 0.0;
    mitk::Point3D readyCenter1;
    readyCenter1[0] = 0.0;
    readyCenter1[1] = 0.0;
    readyCenter1[2] = 5.0;
    auto readyProfile0 = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", readyCenter0, 1.0, 0.0, 24);
    auto readyProfile1 = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", readyCenter1, 1.0, 0.0, 24);
    if (!readyProfile0 || !readyProfile1)
    {
        std::cerr << "FAIL test_loft_readiness_blocking_reason: ready profile helper returned null\n";
        return false;
    }
    xq_SegmentationUtils::ApplyPlacementFrame(readyProfile0.get(), MakeAxialFrame(0, 0.0));
    xq_SegmentationUtils::ApplyPlacementFrame(readyProfile1.get(), MakeAxialFrame(1, 5.0));
    readyGroup->AppendProfile(readyProfile0.release(), 0);
    readyGroup->AppendProfile(readyProfile1.release(), 1);

    if (!xq_SegmentationUtils::GetLoftReadinessBlockingReason(readyGroup).empty())
    {
        std::cerr << "FAIL test_loft_readiness_blocking_reason: ready group should not report a blocking reason\n";
        return false;
    }

    std::cout << "PASS test_loft_readiness_blocking_reason\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 31 — Extraction failure surfaces an explicit canonical writeback warning
// instead of silently skipping when a canonical target exists
// ---------------------------------------------------------------------------

static bool test_extraction_failed_writeback_decision()
{
    // GetExtractionFailedWritebackDecision must always return WarnAndReturn with
    // ExtractionYieldedNoValidContour so the view can surface explicit feedback.
    const auto decision = xq_SegmentationUtils::GetExtractionFailedWritebackDecision();

    if (!decision.ShouldWarn() ||
        decision.warningReason != xq_CanonicalWritebackWarningReason::ExtractionYieldedNoValidContour)
    {
        std::cerr << "FAIL test_extraction_failed_writeback_decision: "
                     "decision must carry ExtractionYieldedNoValidContour warning reason\n";
        return false;
    }

    if (!decision.ShouldReturnEarly() || decision.ShouldAttemptCanonicalWriteback())
    {
        std::cerr << "FAIL test_extraction_failed_writeback_decision: "
                     "flow must be WarnAndReturn (no canonical writeback attempt)\n";
        return false;
    }

    // Both UI contexts must produce a non-empty, human-readable message.
    const auto threshMsg = xq_SegmentationUtils::GetCanonicalWritebackWarningMessage(
        xq_CanonicalWritebackWarningContext::ThresholdContour,
        xq_CanonicalWritebackWarningReason::ExtractionYieldedNoValidContour);
    const auto autoMsg = xq_SegmentationUtils::GetCanonicalWritebackWarningMessage(
        xq_CanonicalWritebackWarningContext::AutoSegmentation,
        xq_CanonicalWritebackWarningReason::ExtractionYieldedNoValidContour);

    if (threshMsg.empty() || autoMsg.empty())
    {
        std::cerr << "FAIL test_extraction_failed_writeback_decision: "
                     "warning message must not be empty for either context\n";
        return false;
    }

    std::cout << "PASS test_extraction_failed_writeback_decision\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 27 — Mixed placement-frame migration reports collision fallback
// ---------------------------------------------------------------------------

static bool test_contour_group_migration_rejects_synthetic_unresolved_fallback()
{
    auto src = xq_ContourGroup::New();
    src->SetPathName("mixed_frame_path");

    ContourSlice s0;
    mitk::Point3D a0; a0[0] = -1.0; a0[1] = 0.0; a0[2] = 10.0;
    mitk::Point3D a1; a1[0] =  1.0; a1[1] = 0.0; a1[2] = 10.0;
    s0.points = {a0, a1};
    src->AddContour(s0);

    ContourSlice s1;
    mitk::Point3D b0; b0[0] = -1.0; b0[1] = 0.0; b0[2] = 20.0;
    mitk::Point3D b1; b1[0] =  1.0; b1[1] = 0.0; b1[2] = 20.0;
    s1.points = {b0, b1};
    src->AddContour(s1);

    ContourSlice s2;
    mitk::Point3D c0; c0[0] = -1.0; c0[1] = 0.0; c0[2] = 30.0;
    mitk::Point3D c1; c1[0] =  1.0; c1[1] = 0.0; c1[2] = 30.0;
    s2.points = {c0, c1};
    src->AddContour(s2);

    std::vector<xq_ProfilePlacementFrame> frames(2);
    frames[0].pathPosIndex = 0;
    frames[0].position[0] = 0.0;
    frames[0].position[1] = 0.0;
    frames[0].position[2] = 10.0;
    frames[0].tangent.Fill(0.0);
    frames[0].tangent[2] = 1.0;
    frames[0].rotation.Fill(0.0);
    frames[0].rotation[0] = 1.0;

    frames[1].pathPosIndex = 2;
    frames[1].position[0] = 0.0;
    frames[1].position[1] = 0.0;
    frames[1].position[2] = 20.0;
    frames[1].tangent = frames[0].tangent;
    frames[1].rotation = frames[0].rotation;

    auto dst = xq_ContourGroupMigration::ToProfileGroup(src.GetPointer(), frames);
    if (!dst)
    {
        std::cerr << "FAIL test_contour_group_migration_rejects_synthetic_unresolved_fallback: ToProfileGroup returned null\n";
        return false;
    }

    if (dst->GetProfileCount() != 2)
    {
        std::cerr << "FAIL test_contour_group_migration_rejects_synthetic_unresolved_fallback: expected only 2 uniquely bound profiles, got "
                  << dst->GetProfileCount() << "\n";
        return false;
    }

    if (!dst->HasProfile(0) || !dst->HasProfile(2))
    {
        std::cerr << "FAIL test_contour_group_migration_rejects_synthetic_unresolved_fallback: expected indices {0,2}\n";
        return false;
    }

    if (dst->HasProfile(1) || dst->HasProfile(3))
    {
        std::cerr << "FAIL test_contour_group_migration_rejects_synthetic_unresolved_fallback: migration must not invent synthetic fallback slots\n";
        return false;
    }

    const auto* boundA = dst->GetProfileAtPathPos(0);
    const auto* boundB = dst->GetProfileAtPathPos(2);
    if (!boundA || !boundB)
    {
        std::cerr << "FAIL test_contour_group_migration_rejects_synthetic_unresolved_fallback: missing migrated profiles at expected indices\n";
        return false;
    }

    if (boundA->GetSlicePlane().IsNull() || boundB->GetSlicePlane().IsNull())
    {
        std::cerr << "FAIL test_contour_group_migration_rejects_synthetic_unresolved_fallback: frame-bound profiles should carry slice planes\n";
        return false;
    }

    if (dst->GetAttribute("migration_unresolved_count") != "1")
    {
        std::cerr << "FAIL test_contour_group_migration_rejects_synthetic_unresolved_fallback: expected one unresolved contour, got '"
                  << dst->GetAttribute("migration_unresolved_count") << "'\n";
        return false;
    }

    if (!dst->GetAttribute("migration_slot_collision_count").empty())
    {
        std::cerr << "FAIL test_contour_group_migration_rejects_synthetic_unresolved_fallback: unexpected collision count '"
                  << dst->GetAttribute("migration_slot_collision_count") << "'\n";
        return false;
    }

    std::cout << "PASS test_contour_group_migration_rejects_synthetic_unresolved_fallback\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 32 — Segmentation-to-modeling phase boundary: dirty profile groups must
// block modeling until the loft has been explicitly regenerated.
//
// Contract:
//   GetModelingPhaseBoundaryBlockingReason returns non-empty when:
//     (a) the group is dirty (profiles modified since last loft), or
//     (b) the group has no stored loft mesh at all.
//   It returns empty when the group carries a clean, up-to-date stored loft.
//   After a profile edit dirtying the group, the reason is non-empty again.
// ---------------------------------------------------------------------------

static bool test_phase_boundary_modeling_blocked_when_dirty()
{
    // Case (a): freshly created group with profiles appended — dirty, no stored loft.
    auto group = xq_ProfileGroup::New();
    group->SetAttribute("path_name", "centerline_phase_boundary");
    mitk::Point3D c0; c0[0] = 0.0; c0[1] = 0.0; c0[2] = 0.0;
    mitk::Point3D c1; c1[0] = 0.0; c1[1] = 0.0; c1[2] = 5.0;
    auto p0 = xq_SegmentationUtils::CreatePresetProfile("Circle", c0, 1.0, 0.0, 24);
    auto p1 = xq_SegmentationUtils::CreatePresetProfile("Circle", c1, 1.0, 0.0, 24);
    if (!p0 || !p1)
    {
        std::cerr << "FAIL test_phase_boundary_modeling_blocked_when_dirty: profile helper returned null\n";
        return false;
    }
    xq_SegmentationUtils::ApplyPlacementFrame(p0.get(), MakeAxialFrame(0, 0.0));
    xq_SegmentationUtils::ApplyPlacementFrame(p1.get(), MakeAxialFrame(1, 5.0));
    group->AppendProfile(p0.release(), 0);
    group->AppendProfile(p1.release(), 1);

    // Group is dirty after AppendProfile; no stored loft → must be blocked.
    if (!group->IsLoftCacheDirty(0))
    {
        std::cerr << "FAIL test_phase_boundary_modeling_blocked_when_dirty: "
                     "group should be dirty after AppendProfile\n";
        return false;
    }
    const auto dirtyReason = xq_SegmentationUtils::GetModelingPhaseBoundaryBlockingReason(group);
    if (dirtyReason.empty())
    {
        std::cerr << "FAIL test_phase_boundary_modeling_blocked_when_dirty: "
                     "dirty group must produce a non-empty blocking reason\n";
        return false;
    }

    // Case (b): simulate loft completion → SetLoftedMesh clears dirty flag.
    auto fakeMesh = vtkSmartPointer<vtkPolyData>::New();
    auto pts = vtkSmartPointer<vtkPoints>::New();
    pts->InsertNextPoint(0, 0, 0);
    fakeMesh->SetPoints(pts);
    group->SetLoftedMesh(fakeMesh, 0);

    if (group->IsLoftCacheDirty(0))
    {
        std::cerr << "FAIL test_phase_boundary_modeling_blocked_when_dirty: "
                     "group should not be dirty after SetLoftedMesh\n";
        return false;
    }
    const auto cleanReason = xq_SegmentationUtils::GetModelingPhaseBoundaryBlockingReason(group);
    if (!cleanReason.empty())
    {
        std::cerr << "FAIL test_phase_boundary_modeling_blocked_when_dirty: "
                     "clean group with stored loft must not be blocked, got: '"
                  << cleanReason << "'\n";
        return false;
    }

    // Case (c): profile edit after loft → dirty again → must be blocked.
    mitk::Point3D c2; c2[0] = 0.0; c2[1] = 0.0; c2[2] = 10.0;
    auto p2 = xq_SegmentationUtils::CreatePresetProfile("Circle", c2, 1.0, 0.0, 24);
    if (!p2)
    {
        std::cerr << "FAIL test_phase_boundary_modeling_blocked_when_dirty: profile helper returned null for p2\n";
        return false;
    }
    xq_SegmentationUtils::ApplyPlacementFrame(p2.get(), MakeAxialFrame(2, 10.0));
    group->AppendProfile(p2.release(), 2);

    if (!group->IsLoftCacheDirty(0))
    {
        std::cerr << "FAIL test_phase_boundary_modeling_blocked_when_dirty: "
                     "group should be dirty after editing profiles post-loft\n";
        return false;
    }
    const auto reDirtyReason = xq_SegmentationUtils::GetModelingPhaseBoundaryBlockingReason(group);
    if (reDirtyReason.empty())
    {
        std::cerr << "FAIL test_phase_boundary_modeling_blocked_when_dirty: "
                     "re-dirtied group must produce a non-empty blocking reason\n";
        return false;
    }

    std::cout << "PASS test_phase_boundary_modeling_blocked_when_dirty\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 33 — ResolvePreprocessingTarget: no group selected
//
// When no ProfileGroup has been selected (null pointer), the resolution must
// report NoTargetAvailable, not block detached preview (it is the fallback),
// and supply a human-readable blocking reason.
// ---------------------------------------------------------------------------

static bool test_preprocess_target_no_group_selected()
{
    const auto result = xq_SegmentationUtils::ResolvePreprocessingTarget(nullptr);

    if (result.state != xq_PreprocTargetState::NoTargetAvailable)
    {
        std::cerr << "FAIL test_preprocess_target_no_group_selected: "
                     "expected NoTargetAvailable for null group, got state "
                  << static_cast<int>(result.state) << "\n";
        return false;
    }

    if (result.IsResolved())
    {
        std::cerr << "FAIL test_preprocess_target_no_group_selected: "
                     "IsResolved() must be false for null group\n";
        return false;
    }

    // Null group → no canonical target → detached preview is the only option,
    // so ShouldBlockDetachedPreview must be false.
    if (result.ShouldBlockDetachedPreview())
    {
        std::cerr << "FAIL test_preprocess_target_no_group_selected: "
                     "ShouldBlockDetachedPreview() must be false when no target exists\n";
        return false;
    }

    if (result.blockingReason.empty())
    {
        std::cerr << "FAIL test_preprocess_target_no_group_selected: "
                     "blockingReason must be non-empty so the UI can explain the gap\n";
        return false;
    }

    std::cout << "PASS test_preprocess_target_no_group_selected\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 34 — ResolvePreprocessingTarget: target exists but is ineligible
//
// A path-bound ProfileGroup whose loft readiness is blocked (unresolved
// placement on at least one profile) cannot participate in canonical
// preprocessing and must be reported as TargetIneligible.
// ---------------------------------------------------------------------------

static bool test_preprocess_target_ineligible_path_bound_group()
{
    auto group = xq_ProfileGroup::New();
    group->SetAttribute("path_name", "centerline_ineligible_target");

    // One profile without a slice plane → unresolved placement for a path-
    // bound group → GetLoftReadinessBlockingReason returns non-empty.
    auto profile = xq_SegmentationUtils::CreateEditableProfile("Manual", mitk::Point3D());
    if (!profile)
    {
        std::cerr << "FAIL test_preprocess_target_ineligible_path_bound_group: "
                     "profile helper returned null\n";
        return false;
    }
    profile->SetPathPosIndex(3);
    group->AppendProfile(profile.release(), 3);

    const auto result = xq_SegmentationUtils::ResolvePreprocessingTarget(group.GetPointer());

    if (result.state != xq_PreprocTargetState::TargetIneligible)
    {
        std::cerr << "FAIL test_preprocess_target_ineligible_path_bound_group: "
                     "expected TargetIneligible for path-bound group with unresolved profiles, "
                     "got state " << static_cast<int>(result.state) << "\n";
        return false;
    }

    if (result.IsResolved())
    {
        std::cerr << "FAIL test_preprocess_target_ineligible_path_bound_group: "
                     "IsResolved() must be false for ineligible target\n";
        return false;
    }

    if (result.ShouldBlockDetachedPreview())
    {
        std::cerr << "FAIL test_preprocess_target_ineligible_path_bound_group: "
                     "ShouldBlockDetachedPreview() must be false when target is ineligible "
                     "(detached is the only fallback)\n";
        return false;
    }

    if (result.blockingReason.empty())
    {
        std::cerr << "FAIL test_preprocess_target_ineligible_path_bound_group: "
                     "blockingReason must be non-empty for ineligible target\n";
        return false;
    }

    std::cout << "PASS test_preprocess_target_ineligible_path_bound_group\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 35 — ResolvePreprocessingTarget: valid target available
//
// Two sub-cases:
//   (a) Unbound group: always a valid target regardless of profile count.
//   (b) Path-bound group that is loft-ready (two profiles with placement).
// Both must report TargetReady, IsResolved=true, ShouldBlockDetachedPreview=true.
// ---------------------------------------------------------------------------

static bool test_preprocess_target_valid_target()
{
    // --- Case (a): unbound group is always a valid target ---
    auto unboundGroup = xq_ProfileGroup::New();
    // even an empty unbound group is a valid target (no path constraint)
    const auto unboundEmpty =
        xq_SegmentationUtils::ResolvePreprocessingTarget(unboundGroup.GetPointer());
    if (unboundEmpty.state != xq_PreprocTargetState::TargetReady)
    {
        std::cerr << "FAIL test_preprocess_target_valid_target: "
                     "empty unbound group should be TargetReady\n";
        return false;
    }
    if (!unboundEmpty.IsResolved())
    {
        std::cerr << "FAIL test_preprocess_target_valid_target: "
                     "unbound empty group IsResolved() must be true\n";
        return false;
    }
    if (!unboundEmpty.ShouldBlockDetachedPreview())
    {
        std::cerr << "FAIL test_preprocess_target_valid_target: "
                     "unbound group ShouldBlockDetachedPreview() must be true\n";
        return false;
    }
    if (!unboundEmpty.blockingReason.empty())
    {
        std::cerr << "FAIL test_preprocess_target_valid_target: "
                     "unbound ready target must have empty blockingReason\n";
        return false;
    }

    // --- Case (b): path-bound group that is loft-ready ---
    auto boundGroup = xq_ProfileGroup::New();
    boundGroup->SetAttribute("path_name", "centerline_valid_target");

    const auto frame0 = MakeAxialFrame(0, 0.0);
    const auto frame1 = MakeAxialFrame(1, 5.0);

    auto p0 = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", frame0.position, 1.5, 0.0, 24);
    auto p1 = xq_SegmentationUtils::CreatePresetProfile(
        "Circle", frame1.position, 1.5, 0.0, 24);
    if (!p0 || !p1)
    {
        std::cerr << "FAIL test_preprocess_target_valid_target: "
                     "preset profile helper returned null\n";
        return false;
    }
    xq_SegmentationUtils::ApplyPlacementFrame(p0.get(), frame0);
    xq_SegmentationUtils::ApplyPlacementFrame(p1.get(), frame1);
    boundGroup->AppendProfile(p0.release(), 0);
    boundGroup->AppendProfile(p1.release(), 1);

    const auto boundResult =
        xq_SegmentationUtils::ResolvePreprocessingTarget(boundGroup.GetPointer());

    if (boundResult.state != xq_PreprocTargetState::TargetReady)
    {
        std::cerr << "FAIL test_preprocess_target_valid_target: "
                     "loft-ready path-bound group should be TargetReady, "
                     "blocking reason: '" << boundResult.blockingReason << "'\n";
        return false;
    }
    if (!boundResult.IsResolved())
    {
        std::cerr << "FAIL test_preprocess_target_valid_target: "
                     "loft-ready bound group IsResolved() must be true\n";
        return false;
    }
    if (!boundResult.ShouldBlockDetachedPreview())
    {
        std::cerr << "FAIL test_preprocess_target_valid_target: "
                     "loft-ready bound group ShouldBlockDetachedPreview() must be true\n";
        return false;
    }
    if (!boundResult.blockingReason.empty())
    {
        std::cerr << "FAIL test_preprocess_target_valid_target: "
                     "ready target must have empty blockingReason\n";
        return false;
    }

    std::cout << "PASS test_preprocess_target_valid_target\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 36 — ResolvePreprocessingTarget: ineligible due to unresolved placement
//
// A path-bound ProfileGroup with at least 2 profiles but no slice planes
// applied triggers the unresolved-placement branch (not the <2 profiles
// branch) in GetLoftReadinessBlockingReason.  The resolution must report
// TargetIneligible with a non-empty blocking reason.
// ---------------------------------------------------------------------------

static bool test_preprocess_target_ineligible_unresolved_placement()
{
    auto group = xq_ProfileGroup::New();
    group->SetAttribute("path_name", "centerline_unresolved_placement");

    // Two profiles with valid path-position indices but no slice planes
    // → GetUnresolvedProfilePathIndices returns both indices
    // → GetLoftReadinessBlockingReason returns the "missing placement" message
    auto p0 = xq_SegmentationUtils::CreateEditableProfile("Manual", mitk::Point3D());
    auto p1 = xq_SegmentationUtils::CreateEditableProfile("Manual", mitk::Point3D());
    if (!p0 || !p1)
    {
        std::cerr << "FAIL test_preprocess_target_ineligible_unresolved_placement: "
                     "profile helper returned null\n";
        return false;
    }
    p0->SetPathPosIndex(2);
    p1->SetPathPosIndex(5);
    group->AppendProfile(p0.release(), 2);
    group->AppendProfile(p1.release(), 5);

    const auto result =
        xq_SegmentationUtils::ResolvePreprocessingTarget(group.GetPointer());

    if (result.state != xq_PreprocTargetState::TargetIneligible)
    {
        std::cerr << "FAIL test_preprocess_target_ineligible_unresolved_placement: "
                     "expected TargetIneligible for path-bound group with 2 unplaced "
                     "profiles, got state " << static_cast<int>(result.state) << "\n";
        return false;
    }

    if (result.IsResolved())
    {
        std::cerr << "FAIL test_preprocess_target_ineligible_unresolved_placement: "
                     "IsResolved() must be false for ineligible target\n";
        return false;
    }

    if (result.ShouldBlockDetachedPreview())
    {
        std::cerr << "FAIL test_preprocess_target_ineligible_unresolved_placement: "
                     "ShouldBlockDetachedPreview() must be false when target is ineligible\n";
        return false;
    }

    if (result.blockingReason.empty())
    {
        std::cerr << "FAIL test_preprocess_target_ineligible_unresolved_placement: "
                     "blockingReason must be non-empty for ineligible target\n";
        return false;
    }

    // Verify the message specifically mentions placement (not the <2 profiles message)
    if (result.blockingReason.find("placement") == std::string::npos)
    {
        std::cerr << "FAIL test_preprocess_target_ineligible_unresolved_placement: "
                     "blockingReason must reference placement, got: '"
                  << result.blockingReason << "'\n";
        return false;
    }

    std::cout << "PASS test_preprocess_target_ineligible_unresolved_placement\n";
    return true;
}

// ---------------------------------------------------------------------------
// Test 37 — View CheckPreprocessingTarget decision logic
//
// Simulates the decision rules that xq_MitkSegmentationView::CheckPreprocessingTarget
// applies to a xq_PreprocTargetResolution without requiring any Qt types:
//
//   TargetIneligible  → should block (return false), reason non-empty.
//   NoTargetAvailable → should NOT block (return true), legacy image path.
//   TargetReady       → should NOT block (return true), canonical path.
//
// These assertions match the logic written in CheckPreprocessingTarget in the view.
// ---------------------------------------------------------------------------

// Mirrors the view's blocking rule without depending on Qt.
static bool SimulateCheckPreprocessingTarget(
    const xq_PreprocTargetResolution& resolution,
    std::string& outReason)
{
    if (resolution.state == xq_PreprocTargetState::TargetIneligible)
    {
        outReason = "Preprocessing target is not eligible: " + resolution.blockingReason;
        return false;
    }
    return true;
}

static bool test_view_check_preprocessing_target_logic()
{
    // --- Case 1: no group → NoTargetAvailable → must NOT block ---
    {
        const auto res = xq_SegmentationUtils::ResolvePreprocessingTarget(nullptr);
        std::string reason;
        if (!SimulateCheckPreprocessingTarget(res, reason))
        {
            std::cerr << "FAIL test_view_check_preprocessing_target_logic: "
                         "NoTargetAvailable must NOT block, but got reason: '"
                      << reason << "'\n";
            return false;
        }
    }

    // --- Case 2: ineligible path-bound group → TargetIneligible → must block ---
    {
        auto group = xq_ProfileGroup::New();
        group->SetAttribute("path_name", "check_target_ineligible");
        auto p = xq_SegmentationUtils::CreateEditableProfile("Manual", mitk::Point3D());
        if (!p)
        {
            std::cerr << "FAIL test_view_check_preprocessing_target_logic: "
                         "profile helper returned null\n";
            return false;
        }
        p->SetPathPosIndex(0);
        group->AppendProfile(p.release(), 0);

        const auto res = xq_SegmentationUtils::ResolvePreprocessingTarget(group.GetPointer());
        std::string reason;
        if (SimulateCheckPreprocessingTarget(res, reason))
        {
            std::cerr << "FAIL test_view_check_preprocessing_target_logic: "
                         "TargetIneligible must block, but check returned true\n";
            return false;
        }
        if (reason.empty())
        {
            std::cerr << "FAIL test_view_check_preprocessing_target_logic: "
                         "blocking reason must be non-empty for TargetIneligible\n";
            return false;
        }
    }

    // --- Case 3: valid unbound group → TargetReady → must NOT block ---
    {
        auto group = xq_ProfileGroup::New();
        const auto res = xq_SegmentationUtils::ResolvePreprocessingTarget(group.GetPointer());
        std::string reason;
        if (!SimulateCheckPreprocessingTarget(res, reason))
        {
            std::cerr << "FAIL test_view_check_preprocessing_target_logic: "
                         "TargetReady must NOT block, got reason: '"
                      << reason << "'\n";
            return false;
        }
    }

    std::cout << "PASS test_view_check_preprocessing_target_logic\n";
    return true;
}

static bool test_legacy_levelset_does_not_return_threshold_fallback()
{
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(16, 16, 1);
    image->AllocateScalars(VTK_DOUBLE, 1);
    for (int y = 0; y < 16; ++y)
    {
        for (int x = 0; x < 16; ++x)
            image->SetScalarComponentFromDouble(x, y, 0, 0, 100.0);
    }

    xq_SegmentationAlgorithm::SliceInput slice;
    slice.slice = image;
    slice.seed[0] = 8.0;
    slice.seed[1] = 8.0;

    xq_SegmentationAlgorithm::Params params;
    params.threshold = 50.0;

    xq_LevelSetSegmentation legacyLevelSet;
    const auto result = legacyLevelSet.Extract(slice, params);
    if (result.ok || result.polygon)
    {
        std::cerr << "FAIL test_legacy_levelset_does_not_return_threshold_fallback: "
                     "legacy LevelSet must not return a threshold fallback contour\n";
        return false;
    }
    if (result.diagnostic.find("disabled") == std::string::npos)
    {
        std::cerr << "FAIL test_legacy_levelset_does_not_return_threshold_fallback: "
                     "missing disabled diagnostic\n";
        return false;
    }

    std::cout << "PASS test_legacy_levelset_does_not_return_threshold_fallback\n";
    return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

#if defined(__GNUC__)
#pragma GCC diagnostic pop  // restore -Wdeprecated-declarations
#endif

int main()
{
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    std::cout << "=== seg_preprocess regression tests ===\n";

    int failures = 0;
    if (!test_profile_group_api_consistency()) ++failures;
    if (!test_write_sparse_profiles())         ++failures;
    if (!test_version_validation())            ++failures;
    if (!test_contour_group_pipeline_contract()) ++failures;
    if (!test_path_pipeline_vmtk_disabled_diagnostic()) ++failures;
    if (!test_legacy_import_preserves_distinct_final_suffix()) ++failures;
    if (!test_model_mesh_simprep_pipeline_contract()) ++failures;
    if (!test_sv_project_import_creates_vascular_nodes()) ++failures;
    if (!test_segmentation_object_factory_creates_mappers()) ++failures;
    if (!test_replace_profile_null_safety())   ++failures;
    if (!test_append_profile_null_safety())    ++failures;
    if (!test_parse_int_safe())                ++failures;
    if (!test_loft_cache_dirty_state())        ++failures;
    if (!test_stale_loft_mesh_not_returned_when_dirty()) ++failures;
    if (!test_create_preset_profiles())        ++failures;
    if (!test_loft_profile_group_from_canonical_profiles()) ++failures;
    if (!test_loft_contours_aligns_cyclic_point_order()) ++failures;
    if (!test_profile_edit_helpers_for_canonical_profiles()) ++failures;
    if (!test_create_editable_profile_skeletons()) ++failures;
    if (!test_profile_statistics_helper()) ++failures;
    if (!test_profile_smooth_and_resample_helpers()) ++failures;
    if (!test_create_profile_from_contour_points()) ++failures;
    if (!test_apply_profile_placement_frame()) ++failures;
    if (!test_centerline_rotation_matches_inplane_xhat_convention()) ++failures;
    if (!test_profile_edit_helpers_preserve_placement_metadata()) ++failures;
    if (!test_orient_preset_profile_to_placement()) ++failures;
    if (!test_extract_surface_contour_on_plane()) ++failures;
    if (!test_extract_surface_contour_on_plane_rejects_disconnected_loops()) ++failures;
    if (!test_threshold_contour_respects_slice_plane()) ++failures;
    if (!test_threshold_contour_respects_oriented_image_geometry()) ++failures;
    if (!test_create_legacy_threshold_contour_node()) ++failures;
    if (!test_contour_group_migration())       ++failures;
    if (!test_contour_group_migration_requires_frames_for_path_bound_groups()) ++failures;
    if (!test_contour_group_migration_binds_placement_frames()) ++failures;
    if (!test_apply_placement_frame_relocates_existing_geometry()) ++failures;
    if (!test_rebind_profile_group_to_placement_frames_preserves_profiles()) ++failures;
    if (!test_rebind_profile_group_to_placement_frames_rejects_unmatched_profiles()) ++failures;
    if (!test_requires_path_placement_for_canonical_groups()) ++failures;
    if (!test_resolve_canonical_path_pos_index()) ++failures;
    if (!test_canonical_writeback_warning_policy()) ++failures;
    if (!test_canonical_writeback_view_decisions()) ++failures;
    if (!test_auto_segmentation_writeback_resolves_placement_before_deciding()) ++failures;
    if (!test_profile_group_loft_readiness_api()) ++failures;
    if (!test_loft_readiness_blocking_reason()) ++failures;
    if (!test_contour_group_migration_rejects_synthetic_unresolved_fallback()) ++failures;
    if (!test_extraction_failed_writeback_decision()) ++failures;
    if (!test_phase_boundary_modeling_blocked_when_dirty()) ++failures;
    if (!test_preprocess_target_no_group_selected())         ++failures;
    if (!test_preprocess_target_ineligible_path_bound_group()) ++failures;
    if (!test_preprocess_target_ineligible_unresolved_placement()) ++failures;
    if (!test_preprocess_target_valid_target())               ++failures;
    if (!test_view_check_preprocessing_target_logic())        ++failures;
    if (!test_legacy_levelset_does_not_return_threshold_fallback()) ++failures;

    if (failures == 0)
        std::cout << "All tests PASSED.\n";
    else
        std::cerr << failures << " test(s) FAILED.\n";

    return (failures == 0) ? 0 : 1;
}
