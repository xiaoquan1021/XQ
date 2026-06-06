// Regression tests for Project save/load round-trip (Task B).
//
// Tests:
//   0. test_project_empty_roundtrip — basic CreateProject/OpenProject
//      round-trip without any data (smoke test).
//   1. test_project_save_load_sources — model node source/QA properties
//      survive SaveProject -> OpenProject round-trip.
//   2. test_project_roundtrip_custom_xq_types — full pipeline (Path,
//      ContourGroup, Model, VolumeMesh, SimulationPrep) with source
//      link properties survives round-trip with correct data types.

#include "xq_WorkspaceManager.h"
#include "xq_ProjectDataWriter.h"
#include "xq_ProjectDataReader.h"
#include "xq_DataFolder.h"
#include "xq_ImageFolder.h"
#include "xq_PathFolder.h"
#include "xq_SegmentationFolder.h"
#include "xq_ModelFolder.h"
#include "xq_MeshFolder.h"        // defines xq_GridFolder
#include "xq_SimulationFolder.h"
#include "xq_VesselCenterline.h"
#include "xq_CenterlineSegment.h"
#include "xq_ProfileGroup.h"
#include "xq_ContourGroup.h"
#include "xq_CircularProfile.h"
#include "xq_Model.h"
#include "xq_PolyGeometry.h"
#include "xq_MitkGrid.h"
#include "xq_TetGenGrid.h"
#include "xq_MitkSolverJob.h"
#include "xq_SolverJob.h"
#include "xq_ROMSimulationFolder.h"
#include "xq_MultiPhysicsFolder.h"
#include "xq_MitkROMJob.h"
#include "xq_ROMJob.h"
#include "xq_MitkMultiPhysicsJob.h"
#include "xq_MultiPhysicsJob.h"
#include "xq_MultiPhysicsDomain.h"
#include "xq_MultiPhysicsEquation.h"
#include "xq_PythonApiService.h"
#include "xq_PipelineDataUtils.h"
#include "xq_PipelineConsistency.h"
#include "xq_NodeMetadataIO.h"
#include "xq_PathPipeline.h"
#include "xq_MitkSeg3D.h"

#include <mitkStandaloneDataStorage.h>
#include <mitkDataNode.h>
#include <mitkProperties.h>
#include <mitkCoreObjectFactory.h>
#include <mitkImage.h>
#include <mitkSurface.h>

#include <vtkSphereSource.h>
#include <vtkPolyData.h>
#include <vtkUnstructuredGrid.h>
#include <vtkImageData.h>
#include <vtkTetra.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkSmartPointer.h>

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static const std::string kTestProjectPath =
    (std::filesystem::temp_directory_path() / "xq_test_project").string();
static const char* kTestProjectName = "RoundtripTest";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string JoinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

static bool FileExists(const std::string& path)
{
    return xq_WorkspaceManager::FileExists(path);
}

static bool DirExists(const std::string& path)
{
    return xq_WorkspaceManager::DirExists(path);
}

static bool EndsWithString(const std::string& value, const std::string& suffix)
{
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool RemoveDir(const std::string& path)
{
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    return !ec;
}

static vtkSmartPointer<vtkPolyData> MakeSpherePolyData()
{
    auto sphereSource = vtkSmartPointer<vtkSphereSource>::New();
    sphereSource->SetRadius(5.0);
    sphereSource->SetThetaResolution(16);
    sphereSource->SetPhiResolution(16);
    sphereSource->Update();
    return sphereSource->GetOutput();
}

static vtkSmartPointer<vtkUnstructuredGrid> MakeTetrahedralMesh()
{
    auto grid = vtkSmartPointer<vtkUnstructuredGrid>::New();
    auto pts = vtkSmartPointer<vtkPoints>::New();
    pts->InsertNextPoint(0.0, 0.0, 0.0);
    pts->InsertNextPoint(1.0, 0.0, 0.0);
    pts->InsertNextPoint(0.0, 1.0, 0.0);
    pts->InsertNextPoint(0.0, 0.0, 1.0);
    grid->SetPoints(pts);

    auto cell = vtkSmartPointer<vtkTetra>::New();
    cell->GetPointIds()->SetId(0, 0);
    cell->GetPointIds()->SetId(1, 1);
    cell->GetPointIds()->SetId(2, 2);
    cell->GetPointIds()->SetId(3, 3);

    vtkNew<vtkCellArray> cellArray;
    cellArray->InsertNextCell(cell);
    grid->SetCells(VTK_TETRA, cellArray);

    return grid;
}

static mitk::Image::Pointer MakeSmallMitkImage()
{
    auto vtkTemplate = vtkSmartPointer<vtkImageData>::New();
    vtkTemplate->SetDimensions(5, 5, 5);
    vtkTemplate->SetSpacing(1.0, 1.0, 1.0);
    vtkTemplate->SetOrigin(0.0, 0.0, 0.0);
    vtkTemplate->AllocateScalars(VTK_FLOAT, 1);

    for (int z = 0; z < 5; ++z)
    {
        for (int y = 0; y < 5; ++y)
        {
            for (int x = 0; x < 5; ++x)
            {
                vtkTemplate->SetScalarComponentFromDouble(
                    x, y, z, 0, static_cast<double>(x + y + z));
            }
        }
    }

    auto image = mitk::Image::New();
    image->Initialize(vtkTemplate);
    auto* outVtk = image->GetVtkImageData();
    if (outVtk)
        outVtk->DeepCopy(vtkTemplate);
    return image;
}

static std::vector<mitk::Point3D> MakeTestAnchors()
{
    std::vector<mitk::Point3D> anchors(3);
    anchors[0].Fill(0.0);
    anchors[0][2] = 0.0;
    anchors[1].Fill(0.0);
    anchors[1][2] = 5.0;
    anchors[2].Fill(0.0);
    anchors[2][2] = 10.0;
    return anchors;
}

static xq_ProfileGroup::Pointer MakeTestContourGroup()
{
    auto group = xq_ProfileGroup::New();

    auto* profile0 = new xq_CircularProfile();
    profile0->SetRadius(2.0);
    profile0->SetPathPosIndex(0);
    group->AppendProfile(profile0, 0, 0);

    auto* profile1 = new xq_CircularProfile();
    profile1->SetRadius(1.5);
    profile1->SetPathPosIndex(5);
    group->AppendProfile(profile1, 5, 0);

    return group;
}

static mitk::DataNode::Pointer CreateFolderNode(
    mitk::DataStorage::Pointer dataStorage,
    mitk::DataNode::Pointer parentNode,
    const std::string& folderType)
{
    xq_DataFolder::Pointer folderData;
    std::string nodeName;

    if (folderType == "PathFolder")
    {
        folderData = xq_PathFolder::New();
        nodeName = "Paths";
    }
    else if (folderType == "SegmentationFolder")
    {
        folderData = xq_SegmentationFolder::New();
        nodeName = "Segmentations";
    }
    else if (folderType == "ModelFolder")
    {
        folderData = xq_ModelFolder::New();
        nodeName = "Models";
    }
    else if (folderType == "MeshFolder")
    {
        folderData = xq_GridFolder::New();
        nodeName = "Meshes";
    }
    else if (folderType == "SimulationFolder")
    {
        folderData = xq_SimulationFolder::New();
        nodeName = "Simulations";
    }
    else if (folderType == "ROMSimulationFolder")
    {
        folderData = xq_ROMSimulationFolder::New();
        nodeName = "ROMSimulations";
    }
    else if (folderType == "MultiPhysicsFolder")
    {
        folderData = xq_MultiPhysicsFolder::New();
        nodeName = "MultiPhysics";
    }
    else if (folderType == "ImageFolder")
    {
        folderData = xq_ImageFolder::New();
        nodeName = "Images";
    }
    else
    {
        folderData = xq_DataFolder::New();
        folderData->SetFolderType(folderType);
        nodeName = folderType;
    }

    auto node = mitk::DataNode::New();
    node->SetName(nodeName);
    node->SetData(folderData);
    node->SetVisibility(false);
    dataStorage->Add(node, parentNode);
    return node;
}

// Set up a fresh workspace manager instance and create a project on disk.
// Returns the full project directory path.
static std::string SetupTestProject()
{
    // Ensure parent dir exists (CreateProject uses mkdir which won't
    // create intermediate directories).
    if (!xq_WorkspaceManager::DirExists(kTestProjectPath))
        xq_WorkspaceManager::CreateDir(kTestProjectPath);

    xq_WorkspaceManager wm;
    std::string projectDir = JoinPath(kTestProjectPath, kTestProjectName);

    if (!wm.CreateProject(kTestProjectPath, kTestProjectName))
    {
        std::cerr << "CreateProject failed\n";
        return "";
    }

    return projectDir;
}

// Create project root + category folder nodes in dataStorage, mirroring
// what OpenProject/CreateFolderNodes does, so SaveProject can collect
// children under them.
static mitk::DataNode::Pointer SetupProjectNodes(
    mitk::DataStorage::Pointer dataStorage,
    const std::string& projectDir)
{
    // Project root
    auto projNode = mitk::DataNode::New();
    projNode->SetName(kTestProjectName);
    projNode->SetProperty("project.path",
                          mitk::StringProperty::New(projectDir));
    projNode->SetProperty("project.version",
                          mitk::StringProperty::New("1.0"));
    dataStorage->Add(projNode);

    // Category folders
    CreateFolderNode(dataStorage, projNode, "ImageFolder");
    CreateFolderNode(dataStorage, projNode, "PathFolder");
    CreateFolderNode(dataStorage, projNode, "SegmentationFolder");
    CreateFolderNode(dataStorage, projNode, "ModelFolder");
    CreateFolderNode(dataStorage, projNode, "MeshFolder");
    CreateFolderNode(dataStorage, projNode, "SimulationFolder");
    CreateFolderNode(dataStorage, projNode, "ROMSimulationFolder");
    CreateFolderNode(dataStorage, projNode, "MultiPhysicsFolder");

    return projNode;
}

// Look up a category folder node by folder type string.
static mitk::DataNode::Pointer FindFolderNode(
    mitk::DataStorage::Pointer dataStorage,
    const std::string& folderType)
{
    auto allNodes = dataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        auto node = it->Value();
        auto* folder = dynamic_cast<xq_DataFolder*>(node->GetData());
        if (folder && folder->GetFolderType() == folderType)
            return node;
    }
    return nullptr;
}

// Find a child node by name under a given parent.
static mitk::DataNode::Pointer FindChildNode(
    mitk::DataStorage::Pointer dataStorage,
    mitk::DataNode::Pointer parentNode,
    const std::string& name)
{
    auto children = dataStorage->GetDerivations(parentNode);
    if (children.IsNull())
        return nullptr;

    for (auto it = children->Begin(); it != children->End(); ++it)
    {
        auto node = it->Value();
        if (node->GetName() == name)
            return node;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static int test_project_empty_roundtrip()
{
    std::cout << "--- test_project_empty_roundtrip ---\n";

    RemoveDir(kTestProjectPath);
    std::string projectDir = SetupTestProject();
    if (projectDir.empty())
    {
        std::cerr << "FAIL test_project_empty_roundtrip: SetupTestProject failed\n";
        return 1;
    }

    std::string projFilePath = JoinPath(projectDir,
        std::string(kTestProjectName) + ".xqproj");

    // Open the empty project, save, then reopen
    mitk::DataStorage::Pointer ds1 = mitk::StandaloneDataStorage::New();
    {
        xq_WorkspaceManager wm;
        if (!wm.OpenProject(ds1, projFilePath))
        {
            std::cerr << "FAIL: Initial OpenProject failed\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Initial OpenProject on empty project succeeded\n";

    {
        xq_WorkspaceManager wm;
        if (!wm.SaveProject(ds1, projectDir))
        {
            std::cerr << "FAIL: SaveProject failed\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] SaveProject on empty project succeeded\n";

    mitk::DataStorage::Pointer ds2 = mitk::StandaloneDataStorage::New();
    {
        xq_WorkspaceManager wm;
        if (!wm.OpenProject(ds2, projFilePath))
        {
            std::cerr << "FAIL: Second OpenProject failed\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Second OpenProject on saved project succeeded\n";

    RemoveDir(kTestProjectPath);
    std::cout << "PASS test_project_empty_roundtrip\n";
    return 0;
}

static int test_project_save_load_sources()
{
    std::cout << "--- test_project_save_load_sources ---\n";

    // Clean up any prior run
    RemoveDir(kTestProjectPath);

    // 1. Create StandaloneDataStorage (use base-class pointer so
    //    DataStorage::Add(DataNode*, DataNode*) convenience overload is visible)
    mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

    // 2. Create project on disk
    std::string projectDir = SetupTestProject();
    if (projectDir.empty())
    {
        std::cerr << "FAIL test_project_save_load_sources: "
                  << "SetupTestProject returned empty path\n";
        return 1;
    }

    // 3. Verify disk: .xqproj exists, subdirs exist
    std::string projFilePath = JoinPath(projectDir,
        std::string(kTestProjectName) + ".xqproj");
    if (!FileExists(projFilePath))
    {
        std::cerr << "FAIL test_project_save_load_sources: "
                  << ".xqproj file not found at " << projFilePath << "\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    const std::vector<std::string> expectedSubdirs = {
        "Images", "Paths", "Segmentations", "Models",
        "Meshes", "Simulations", "ROMSimulations",
        "MultiPhysics", "Repository"
    };
    for (const auto& subdir : expectedSubdirs)
    {
        std::string subdirPath = JoinPath(projectDir, subdir);
        if (!DirExists(subdirPath))
        {
            std::cerr << "FAIL test_project_save_load_sources: "
                      << "subdirectory missing: " << subdirPath << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }

    std::cout << "  [OK] Project directory structure verified on disk\n";

    // Open the empty project on ds to populate folder nodes the standard way
    {
        xq_WorkspaceManager openWM0;
        if (!openWM0.OpenProject(ds, projFilePath))
        {
            std::cerr << "FAIL test_project_save_load_sources: "
                      << "Initial OpenProject on empty project failed\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Initial OpenProject succeeded\n";

    // Find the Models folder
    auto modelsFolder = FindFolderNode(ds, "ModelFolder");
    if (modelsFolder.IsNull())
    {
        std::cerr << "FAIL test_project_save_load_sources: "
                  << "ModelFolder not found\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    // 4. Create a model node with xq_Model and sphere poly data
    auto spherePD = MakeSpherePolyData();
    auto* polyGeom = new xq_PolyGeometry();
    polyGeom->SetWholeVtkPolyData(spherePD);

    auto modelData = xq_Model::New();
    modelData->SetModelElement(polyGeom, 0);

    auto modelNode = mitk::DataNode::New();
    modelNode->SetName("test_model");
    modelNode->SetData(modelData);

    // 5. Set pipeline properties (use project-persistent keys from
    //    the NodeMetadataIO whitelist: xq.source.*)
    xq::pipeline::SetStringProperty(
        modelNode,
        xq::pipeline::kSourceContourGroupsProperty,
        "profiles");

    // 6. Set QA properties
    modelNode->SetBoolProperty("xq.model.qa.ok", true);
    modelNode->SetIntProperty("xq.model.qa.boundary_edges", 0);
    modelNode->SetIntProperty("xq.model.face_count", 5);

    // 7. Add node to DataStorage under Models folder
    ds->Add(modelNode, modelsFolder);
    std::cout << "  [OK] Model node added under Models folder\n";

    // 8. Save project and metadata sidecars
    {
        xq_WorkspaceManager saveWM;
        if (!saveWM.SaveProject(ds, projectDir))
        {
            std::cerr << "FAIL test_project_save_load_sources: "
                      << "SaveProject returned false\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] SaveProject succeeded\n";

    // 9. Read data back directly via ProjectDataReader (bypasses
    //    OpenProject's ReparentIntoCategoryFolders which has a known
    //    crash when stage-tagged nodes are present).
    std::string savedModelPath = JoinPath(projectDir,
        "Models/test_model.xqmodel.vtp");
    std::string metaPath = savedModelPath + ".xqmeta.xml";

    if (!FileExists(savedModelPath))
    {
        std::cerr << "FAIL test_project_save_load_sources: "
                  << "Saved model file not found: " << savedModelPath << "\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    auto readResult = xq_ProjectDataReader::LoadNodeData(savedModelPath);
    if (!readResult.ok || readResult.nodes.empty())
    {
        std::cerr << "FAIL test_project_save_load_sources: "
                  << "LoadNodeData failed\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    auto reopenedNode = readResult.nodes[0];

    // Restore metadata sidecar
    xq_NodeMetadataIO::ReadNodeMetadata(reopenedNode, metaPath);

    // Verify data type
    {
        auto* model = dynamic_cast<const xq_Model*>(reopenedNode->GetData());
        if (!model)
        {
            std::cerr << "FAIL test_project_save_load_sources: "
                      << "Loaded data is not xq_Model\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        auto* geom = model->GetModelElement(0);
        if (!geom || !geom->GetWholeVtkPolyData() ||
            geom->GetWholeVtkPolyData()->GetNumberOfPoints() == 0)
        {
            std::cerr << "FAIL test_project_save_load_sources: "
                      << "Model geometry missing after reload\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Model data reloaded correctly\n";

    // 10. Verify source properties exist (metadata sidecar)
    std::string contourGroupsProp;
    bool hasContourGroups = reopenedNode->GetStringProperty(
        xq::pipeline::kSourceContourGroupsProperty, contourGroupsProp);
    if (!hasContourGroups)
    {
        std::cerr << "FAIL test_project_save_load_sources: "
                  << "source contour_groups property not found on reopened node\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    if (contourGroupsProp != "profiles")
    {
        std::cerr << "FAIL test_project_save_load_sources: "
                  << "contour_groups value mismatch: expected 'profiles', got '"
                  << contourGroupsProp << "'\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    std::cout << "  [OK] Source property 'xq.source.contour_groups' = 'profiles' survived\n";

    // 11. Verify QA properties survived (from metadata sidecar)
    bool qaOk = false;
    if (!reopenedNode->GetBoolProperty("xq.model.qa.ok", qaOk) || !qaOk)
    {
        std::cerr << "FAIL test_project_save_load_sources: "
                  << "xq.model.qa.ok not preserved (value=" << qaOk << ")\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    int boundaryEdges = -1;
    if (!reopenedNode->GetIntProperty("xq.model.qa.boundary_edges", boundaryEdges) ||
        boundaryEdges != 0)
    {
        std::cerr << "FAIL test_project_save_load_sources: "
                  << "xq.model.qa.boundary_edges not preserved (value="
                  << boundaryEdges << ")\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    int faceCount = -1;
    if (!reopenedNode->GetIntProperty("xq.model.face_count", faceCount) ||
        faceCount != 5)
    {
        std::cerr << "FAIL test_project_save_load_sources: "
                  << "xq.model.face_count not preserved (value="
                  << faceCount << ")\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    std::cout << "  [OK] QA properties survived round-trip\n";

    // Cleanup
    RemoveDir(kTestProjectPath);

    std::cout << "PASS test_project_save_load_sources\n";
    return 0;
}

static int test_project_roundtrip_custom_xq_types()
{
    std::cout << "--- test_project_roundtrip_custom_xq_types ---\n" << std::flush;

    // Clean up any prior run
    RemoveDir(kTestProjectPath);

    // 1. Create project directories on disk
    std::string projectDir = SetupTestProject();
    if (projectDir.empty())
    {
        std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                  << "SetupTestProject failed\n";
        return 1;
    }

    // 2. Build each pipeline node and save directly via SaveNodeData.
    //    This bypasses DataStorage/OpenProject/SaveProject to avoid
    //    known crashes (ReparentIntoCategoryFolders, and a dynamic_cast
    //    issue in xq_MitkGridObjectFactory::SetDefaultProperties that was
    //    fixed by switching to a class-name check).

    auto saveNode = [&](mitk::DataNode::Pointer node,
                        const std::string& subdir) -> bool
    {
        std::string savePath = JoinPath(projectDir, subdir + "/" + node->GetName());
        auto writeResult = xq_ProjectDataWriter::SaveNodeData(node, savePath, subdir);
        if (!writeResult.ok)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "SaveNodeData failed for " << node->GetName()
                      << ": " << (writeResult.diagnostics.empty() ? "unknown" : writeResult.diagnostics[0])
                      << "\n";
            return false;
        }
        std::string metaPath = writeResult.savedPath + ".xqmeta.xml";
        xq_NodeMetadataIO::WriteNodeMetadata(node, metaPath);
        return true;
    };

    // --- Path node: xq_VesselCenterline with anchors ---
    auto* segment = new xq_CenterlineSegment();
    segment->ReplaceAnchors(MakeTestAnchors());

    auto cl = xq_VesselCenterline::New();
    cl->SetSegment(segment, 0);

    auto pathNode = mitk::DataNode::New();
    pathNode->SetName("test_path");
    pathNode->SetData(cl);
    xq::pipeline::MarkNode(pathNode, xq::pipeline::Stage::Path);
    if (!saveNode(pathNode, "Paths")) { RemoveDir(kTestProjectPath); return 1; }
    std::cout << "  [OK] Path node saved\n";

    // --- ContourGroup node: xq_ProfileGroup ---
    auto contourGroup = MakeTestContourGroup();

    auto contourNode = mitk::DataNode::New();
    contourNode->SetName("test_contours");
    contourNode->SetData(contourGroup);
    xq::pipeline::MarkNode(contourNode, xq::pipeline::Stage::ContourGroup);
    xq::pipeline::SetStringProperty(contourNode,
        xq::pipeline::kSourcePathProperty, "test_path");
    if (!saveNode(contourNode, "Segmentations")) { RemoveDir(kTestProjectPath); return 1; }
    std::cout << "  [OK] ContourGroup node saved\n";

    // --- Model node: xq_Model with sphere vtkPolyData ---
    auto spherePD = MakeSpherePolyData();
    auto* modelGeom = new xq_PolyGeometry();
    modelGeom->SetWholeVtkPolyData(spherePD);

    auto modelData = xq_Model::New();
    modelData->SetModelElement(modelGeom, 0);

    auto modelNode = mitk::DataNode::New();
    modelNode->SetName("test_model");
    modelNode->SetData(modelData);
    xq::pipeline::MarkNode(modelNode, xq::pipeline::Stage::Model);
    xq::pipeline::SetStringProperty(modelNode,
        xq::pipeline::kSourceContourGroupsProperty, "test_contours");
    if (!saveNode(modelNode, "Models")) { RemoveDir(kTestProjectPath); return 1; }
    std::cout << "  [OK] Model node saved\n";

    // --- Mesh node: xq_MitkGrid with tetrahedral vtkUnstructuredGrid ---
    auto tetMesh = MakeTetrahedralMesh();
    auto* tetGenGrid = new xq_TetGenGrid();
    tetGenGrid->SetVolumeMesh(tetMesh);

    auto mitkGrid = xq_MitkGrid::New();
    mitkGrid->SetMesh(tetGenGrid, 0);

    auto meshNode = mitk::DataNode::New();
    meshNode->SetName("test_mesh");
    meshNode->SetData(mitkGrid);
    xq::pipeline::MarkNode(meshNode, xq::pipeline::Stage::VolumeMesh);
    xq::pipeline::SetStringProperty(meshNode,
        xq::pipeline::kSourceModelProperty, "test_model");
    if (!saveNode(meshNode, "Meshes")) { RemoveDir(kTestProjectPath); return 1; }
    std::cout << "  [OK] Mesh node saved\n";

    // --- SimulationPrep node: xq_MitkSolverJob ---
    auto job = xq_SolverJob::Builder()
        .withName("test_sim")
        .withTimesteps(200, 0.001)
        .withCycles(2)
        .build();

    auto mitkJob = xq_MitkSolverJob::New();
    mitkJob->SetMeshName("test_mesh");
    mitkJob->SetModelName("test_model");
    mitkJob->SetSimJob(std::move(job), 0);

    auto simNode = mitk::DataNode::New();
    simNode->SetName("test_sim");
    simNode->SetData(mitkJob);
    xq::pipeline::MarkNode(simNode, xq::pipeline::Stage::SimulationPrep);
    xq::pipeline::SetStringProperty(simNode,
        xq::pipeline::kSourceMeshProperty, "test_mesh");
    xq::pipeline::SetStringProperty(simNode,
        xq::pipeline::kSourceModelProperty, "test_model");
    xq::pipeline::SetStringProperty(
        simNode, "xq.params.simulation.face_roles",
        "inlet=inflow\noutlet=outflow\nwall=wall");
    xq::pipeline::SetStringProperty(
        simNode, "xq.params.simulation.boundary_conditions",
        "inlet|inflow|prescribed_velocity|waveform:0,2;1,2\n"
        "outlet|outflow|resistance|params:resistance=1000\n"
        "wall|wall|no_slip");
    xq::pipeline::SetStringProperty(simNode, "xq.solver.backend_id", "xq_simple_flow");
    xq::pipeline::SetStringProperty(simNode, "xq.solver.backend_name", "XQ Simple Native Flow Solver");
    xq::pipeline::SetStringProperty(simNode, "xq.solver.backend_version", "1");
    if (!saveNode(simNode, "Simulations")) { RemoveDir(kTestProjectPath); return 1; }
    std::cout << "  [OK] SimulationPrep node saved\n";

    // 3. Load data back directly via ProjectDataReader + metadata sidecars
    auto loadAndVerify = [&](const std::string& relPath,
                             const std::string& label) -> mitk::DataNode::Pointer
    {
        std::string fullPath = JoinPath(projectDir, relPath);
        std::string metaPath = fullPath + ".xqmeta.xml";

        if (!FileExists(fullPath))
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << label << " file not found: " << fullPath << "\n";
            return nullptr;
        }

        auto readResult = xq_ProjectDataReader::LoadNodeData(fullPath);
        if (!readResult.ok || readResult.nodes.empty())
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "LoadNodeData failed for " << label << "\n";
            return nullptr;
        }

        auto node = readResult.nodes[0];

        // Restore metadata sidecar if present
        if (FileExists(metaPath))
            xq_NodeMetadataIO::ReadNodeMetadata(node, metaPath);

        return node;
    };

    // --- Path: test_path.xqpath.vtp ---
    auto reopenedPathNode = loadAndVerify("Paths/test_path.xqpath.vtp", "Path");
    if (reopenedPathNode.IsNull()) { RemoveDir(kTestProjectPath); return 1; }
    {
        auto* pathCL = dynamic_cast<const xq_VesselCenterline*>(
            reopenedPathNode->GetData());
        if (!pathCL)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Path node data is not xq_VesselCenterline\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        auto* seg = pathCL->GetSegment(0);
        if (!seg || seg->GetAnchorCount() != 3)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Path segment anchor count mismatch after reload\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Path reloaded correctly (xq_VesselCenterline, 3 anchors)\n";

    // --- ContourGroup: test_contours.xqprofiles.xml ---
    auto reopenedContourNode = loadAndVerify(
        "Segmentations/test_contours.xqprofiles.xml", "ContourGroup");
    if (reopenedContourNode.IsNull()) { RemoveDir(kTestProjectPath); return 1; }
    {
        auto* cg = dynamic_cast<const xq_ProfileGroup*>(
            reopenedContourNode->GetData());
        if (!cg)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Contour node data is not xq_ProfileGroup\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int profileCount = cg->GetProfileCount(0);
        if (profileCount < 1)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Contour group has no profiles after reload\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        // Verify source link: contour -> path
        std::string srcPath;
        if (!reopenedContourNode->GetStringProperty(
                xq::pipeline::kSourcePathProperty, srcPath) ||
            srcPath != "test_path")
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Contour xq.source.path expected 'test_path', got '"
                      << srcPath << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] ContourGroup reloaded correctly (xq_ProfileGroup, "
              << "source link intact)\n";

    // --- Model: test_model.xqmodel.vtp ---
    auto reopenedModelNode = loadAndVerify("Models/test_model.xqmodel.vtp", "Model");
    if (reopenedModelNode.IsNull()) { RemoveDir(kTestProjectPath); return 1; }
    {
        auto* m = dynamic_cast<const xq_Model*>(
            reopenedModelNode->GetData());
        if (!m)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Model node data is not xq_Model\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        auto* geom = m->GetModelElement(0);
        if (!geom)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Model geometry missing after reload\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        auto pd = geom->GetWholeVtkPolyData();
        if (!pd || pd->GetNumberOfPoints() == 0)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Model polydata empty after reload\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        // Verify source link: model -> contour_groups
        std::string srcCG;
        if (!reopenedModelNode->GetStringProperty(
                xq::pipeline::kSourceContourGroupsProperty, srcCG) ||
            srcCG != "test_contours")
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Model xq.source.contour_groups expected 'test_contours', got '"
                      << srcCG << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Model reloaded correctly (xq_Model, source link intact)\n";

    // --- Mesh: test_mesh.xqmesh.vtu ---
    auto reopenedMeshNode = loadAndVerify("Meshes/test_mesh.xqmesh.vtu", "Mesh");
    if (reopenedMeshNode.IsNull()) { RemoveDir(kTestProjectPath); return 1; }
    {
        auto* mg = dynamic_cast<const xq_MitkGrid*>(
            reopenedMeshNode->GetData());
        if (!mg)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Mesh node data is not xq_MitkGrid\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        auto* meshObj = mg->GetMesh(0);
        if (!meshObj)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Mesh object missing after reload\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        auto* ug = meshObj->GetVolumeMesh();
        if (!ug || ug->GetNumberOfPoints() == 0)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Mesh volume data empty after reload\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        // Verify source link: mesh -> model
        std::string srcModel;
        if (!reopenedMeshNode->GetStringProperty(
                xq::pipeline::kSourceModelProperty, srcModel) ||
            srcModel != "test_model")
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Mesh xq.source.model expected 'test_model', got '"
                      << srcModel << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Mesh reloaded correctly (xq_MitkGrid, source link intact)\n";

    // --- SimulationPrep: test_sim.xqsim.xml ---
    auto reopenedSimNode = loadAndVerify("Simulations/test_sim.xqsim.xml", "Simulation");
    if (reopenedSimNode.IsNull()) { RemoveDir(kTestProjectPath); return 1; }
    {
        auto* sj = dynamic_cast<const xq_MitkSolverJob*>(
            reopenedSimNode->GetData());
        if (!sj)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Sim node data is not xq_MitkSolverJob\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        // Verify source links: sim -> mesh and sim -> model
        std::string srcMesh;
        if (!reopenedSimNode->GetStringProperty(
                xq::pipeline::kSourceMeshProperty, srcMesh) ||
            srcMesh != "test_mesh")
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Sim xq.source.mesh expected 'test_mesh', got '"
                      << srcMesh << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string srcModel;
        if (!reopenedSimNode->GetStringProperty(
                xq::pipeline::kSourceModelProperty, srcModel) ||
            srcModel != "test_model")
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Sim xq.source.model expected 'test_model', got '"
                      << srcModel << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string faceRoles;
        if (!reopenedSimNode->GetStringProperty(
                "xq.params.simulation.face_roles", faceRoles) ||
            faceRoles.find("inlet=inflow") == std::string::npos ||
            faceRoles.find("outlet=outflow") == std::string::npos)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Simulation face roles did not roundtrip\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string bcSummary;
        if (!reopenedSimNode->GetStringProperty(
                "xq.params.simulation.boundary_conditions", bcSummary) ||
            bcSummary.find("prescribed_velocity") == std::string::npos ||
            bcSummary.find("resistance") == std::string::npos)
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Simulation BC summary did not roundtrip\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string backendId;
        std::string backendName;
        std::string backendVersion;
        if (!reopenedSimNode->GetStringProperty("xq.solver.backend_id", backendId) ||
            backendId != "xq_simple_flow" ||
            !reopenedSimNode->GetStringProperty("xq.solver.backend_name", backendName) ||
            backendName != "XQ Simple Native Flow Solver" ||
            !reopenedSimNode->GetStringProperty("xq.solver.backend_version", backendVersion) ||
            backendVersion != "1")
        {
            std::cerr << "FAIL test_project_roundtrip_custom_xq_types: "
                      << "Simulation backend metadata did not roundtrip\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] SimulationPrep reloaded correctly (xq_MitkSolverJob, "
              << "source links intact)\n";

    std::cout << "  [OK] All 5 data types verified via dynamic_cast\n";
    std::cout << "  [OK] All source links resolved correctly\n";

    // Cleanup
    RemoveDir(kTestProjectPath);

    std::cout << "PASS test_project_roundtrip_custom_xq_types\n";
    return 0;
}

static int test_project_full_openproject_roundtrip_custom_xq_types()
{
    std::cout << "--- test_project_full_openproject_roundtrip_custom_xq_types ---\n" << std::flush;

    // Clean up any prior run
    RemoveDir(kTestProjectPath);

    // 1. Create project directories on disk
    std::string projectDir = SetupTestProject();
    if (projectDir.empty())
    {
        std::cerr << "FAIL test_project_full_openproject_roundtrip_custom_xq_types: "
                  << "SetupTestProject failed\n";
        return 1;
    }
    std::string projFilePath = JoinPath(projectDir,
        std::string(kTestProjectName) + ".xqproj");

    // 2. Create StandaloneDataStorage and set up project nodes with folders
    mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();
    mitk::DataNode::Pointer projNode = SetupProjectNodes(ds, projectDir);
    if (projNode.IsNull())
    {
        std::cerr << "FAIL test_project_full_openproject_roundtrip_custom_xq_types: "
                  << "SetupProjectNodes returned null\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    std::cout << "  [OK] Project nodes set up with folder hierarchy\n";

    // 3. Create 5 data nodes and add to correct folders

    // --- Path node: xq_VesselCenterline with anchors ---
    auto* segment = new xq_CenterlineSegment();
    segment->ReplaceAnchors(MakeTestAnchors());

    auto cl = xq_VesselCenterline::New();
    cl->SetSegment(segment, 0);

    auto pathNode = mitk::DataNode::New();
    pathNode->SetName("test_fullpath");
    pathNode->SetData(cl);
    xq::pipeline::MarkNode(pathNode, xq::pipeline::Stage::Path);
    pathNode->SetBoolProperty("xq.contour.ready", false);

    auto pathsFolder = FindFolderNode(ds, "PathFolder");
    if (pathsFolder.IsNull())
    {
        std::cerr << "FAIL test_project_full_openproject_roundtrip_custom_xq_types: "
                  << "PathFolder not found\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    ds->Add(pathNode, pathsFolder);
    std::cout << "  [OK] Path node added under Paths folder\n";

    // --- ContourGroup node: xq_ProfileGroup ---
    auto contourGroup = MakeTestContourGroup();

    auto contourNode = mitk::DataNode::New();
    contourNode->SetName("test_fullcontours");
    contourNode->SetData(contourGroup);
    xq::pipeline::MarkNode(contourNode, xq::pipeline::Stage::ContourGroup);
    xq::pipeline::SetStringProperty(contourNode,
        xq::pipeline::kSourcePathProperty, "test_fullpath");
    contourNode->SetBoolProperty("xq.contour.ready", true);
    contourNode->SetIntProperty("xq.contour.profile_count", 2);
    contourNode->SetIntProperty("xq.contour.missing_count", 0);
    contourNode->SetIntProperty("xq.contour.warning_count", 0);
    contourNode->SetIntProperty("xq.contour.error_count", 0);

    auto segFolder = FindFolderNode(ds, "SegmentationFolder");
    if (segFolder.IsNull())
    {
        std::cerr << "FAIL test_project_full_openproject_roundtrip_custom_xq_types: "
                  << "SegmentationFolder not found\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    ds->Add(contourNode, segFolder);
    std::cout << "  [OK] ContourGroup node added under Segmentations folder\n";

    // --- Model node: xq_Model with sphere vtkPolyData ---
    auto spherePD = MakeSpherePolyData();
    auto* modelGeom = new xq_PolyGeometry();
    modelGeom->SetWholeVtkPolyData(spherePD);

    auto modelData = xq_Model::New();
    modelData->SetModelElement(modelGeom, 0);

    auto modelNode = mitk::DataNode::New();
    modelNode->SetName("test_fullmodel");
    modelNode->SetData(modelData);
    xq::pipeline::MarkNode(modelNode, xq::pipeline::Stage::Model);
    xq::pipeline::SetStringProperty(modelNode,
        xq::pipeline::kSourceContourGroupsProperty, "test_fullcontours");
    xq::pipeline::SetStringProperty(modelNode,
        xq::pipeline::kSourcePathProperty, "test_fullpath");
    modelNode->SetStringProperty("xq.model.type", "PolyData");
    modelNode->SetIntProperty("xq.model.sampling", 48);
    modelNode->SetStringProperty("xq.model.loft.parameters",
        "sampling=48;engine=occt;blendRadius=0.250000");
    modelNode->SetStringProperty("xq.model.cap_info", "0,wall,wall,1;1,cap_1,cap,1");
    modelNode->SetStringProperty("xq.model.requested_engine", "occt");
    modelNode->SetStringProperty("xq.model.actual_engine", "vtk_fallback");
    modelNode->SetBoolProperty("xq.model.algorithm.fallback", true);
    modelNode->SetBoolProperty("xq.model.qa.ok", true);
    modelNode->SetIntProperty("xq.model.qa.boundary_edges", 0);
    modelNode->SetIntProperty("xq.model.qa.non_manifold_edges", 0);
    modelNode->SetIntProperty("xq.model.qa.connected_components", 1);
    modelNode->SetBoolProperty("xq.model.qa.has_face_ids", true);
    modelNode->SetIntProperty("xq.model.face_count", 1);
    modelNode->SetIntProperty("selectedFaceId", 1);

    auto modelsFolder = FindFolderNode(ds, "ModelFolder");
    if (modelsFolder.IsNull())
    {
        std::cerr << "FAIL test_project_full_openproject_roundtrip_custom_xq_types: "
                  << "ModelFolder not found\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    ds->Add(modelNode, modelsFolder);
    std::cout << "  [OK] Model node added under Models folder\n";

    // --- Mesh node: xq_MitkGrid with tetrahedral vtkUnstructuredGrid ---
    auto tetMesh = MakeTetrahedralMesh();
    auto* tetGenGrid = new xq_TetGenGrid();
    tetGenGrid->SetVolumeMesh(tetMesh);

    auto mitkGrid = xq_MitkGrid::New();
    mitkGrid->SetMesh(tetGenGrid, 0);

    auto meshNode = mitk::DataNode::New();
    meshNode->SetName("test_fullmesh");
    meshNode->SetData(mitkGrid);
    xq::pipeline::MarkNode(meshNode, xq::pipeline::Stage::VolumeMesh);
    xq::pipeline::SetStringProperty(meshNode,
        xq::pipeline::kSourceModelProperty, "test_fullmodel");
    meshNode->SetBoolProperty("xq.mesh.qa.ok", true);
    meshNode->SetIntProperty("xq.mesh.cells", 1);
    meshNode->SetIntProperty("xq.mesh.points", 4);
    meshNode->SetIntProperty("xq.mesh.negative_volume_count", 0);
    meshNode->SetDoubleProperty("xq.mesh.min_volume", 0.1666667);
    meshNode->SetDoubleProperty("xq.mesh.max_volume", 0.1666667);
    meshNode->SetDoubleProperty("xq.mesh.globalEdgeSize", 2.5);
    meshNode->SetStringProperty("xq.mesh.local_face_sizes.values", "1:1.25");
    meshNode->SetStringProperty("xq.mesh.refinement_regions.values",
        "Sphere,0,0,5,2,2,2,0.75");
    meshNode->SetIntProperty("xq.mesh.bl.layers", 2);
    meshNode->SetDoubleProperty("xq.mesh.bl.firstHeight", 0.1);
    meshNode->SetDoubleProperty("xq.mesh.bl.growthRate", 1.3);
    meshNode->SetBoolProperty("xq.mesh.preserve_surface", true);
    meshNode->SetBoolProperty("xq.mesh.optimize", true);
    meshNode->SetDoubleProperty("xq.mesh.min_dihedral", 10.0);
    meshNode->SetDoubleProperty("xq.mesh.max_edge_size", 0.0);
    meshNode->SetBoolProperty("scalar visibility", true);

    auto meshesFolder = FindFolderNode(ds, "MeshFolder");
    if (meshesFolder.IsNull())
    {
        std::cerr << "FAIL test_project_full_openproject_roundtrip_custom_xq_types: "
                  << "MeshFolder not found\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    ds->Add(meshNode, meshesFolder);
    std::cout << "  [OK] Mesh node added under Meshes folder\n";

    // --- SimulationPrep node: xq_MitkSolverJob ---
    auto job = xq_SolverJob::Builder()
        .withName("test_fullsim")
        .withTimesteps(200, 0.001)
        .withCycles(2)
        .withSolver("svLS", 10, 2)
        .withWallProp("poisson_ratio", "0.45")
        .build();

    // Add boundary condition via direct setter
    job->SetSolverType("svLS");
    job->SetNumLinearIterations(10);
    job->SetNumNonlinearIterations(2);
    job->SetWallPoissonRatio(0.45);
    xq_BoundaryCondition bc;
    bc.faceName = "inlet";
    bc.faceRole = "inflow";
    bc.bcType = "prescribed_velocity";
    bc.parameters["flow_rate"] = "10.0";
    job->AddBoundaryCondition(bc);

    auto mitkJob = xq_MitkSolverJob::New();
    mitkJob->SetMeshName("test_fullmesh");
    mitkJob->SetModelName("test_fullmodel");
    mitkJob->SetSimJob(std::move(job), 0);

    auto simNode = mitk::DataNode::New();
    simNode->SetName("test_fullsim");
    simNode->SetData(mitkJob);
    xq::pipeline::MarkNode(simNode, xq::pipeline::Stage::SimulationPrep);
    simNode->SetIntProperty("xq.simprep.face_role_count", 1);
    xq::pipeline::SetStringProperty(simNode,
        xq::pipeline::kSourceMeshProperty, "test_fullmesh");
    xq::pipeline::SetStringProperty(simNode,
        xq::pipeline::kSourceModelProperty, "test_fullmodel");
    simNode->SetStringProperty("xq.sim.status", "configured");
    simNode->SetStringProperty("xq.sim.solver_type", "svLS");
    simNode->SetBoolProperty("xq.sim.deformable_wall", false);
    simNode->SetDoubleProperty("xq.sim.fluid_density", 1.06);
    simNode->SetDoubleProperty("xq.sim.fluid_viscosity", 0.04);
    simNode->SetDoubleProperty("xq.sim.initial_pressure", 80.0);
    simNode->SetDoubleProperty("xq.sim.initial_velocity", 0.0);
    simNode->SetDoubleProperty("xq.sim.wall_thickness", 0.5);
    simNode->SetDoubleProperty("xq.sim.wall_elastic_modulus", 4000000.0);
    simNode->SetDoubleProperty("xq.sim.wall_poisson_ratio", 0.45);
    simNode->SetDoubleProperty("xq.sim.wall_density", 1.0);
    simNode->SetIntProperty("xq.sim.num_timesteps", 200);
    simNode->SetIntProperty("xq.sim.num_cycles", 2);
    simNode->SetIntProperty("xq.sim.num_linear_iterations", 10);
    simNode->SetIntProperty("xq.sim.num_nonlinear_iterations", 2);
    simNode->SetDoubleProperty("xq.sim.time_step_size", 0.001);
    simNode->SetDoubleProperty("xq.sim.start_time", 0.25);
    simNode->SetDoubleProperty("xq.sim.end_time", 1.25);
    simNode->SetDoubleProperty("xq.sim.residual_tolerance", 0.0001);
    simNode->SetStringProperty("xq.sim.solver_preset", "Pulsatile Flow");
    simNode->SetStringProperty("xq.sim.step_construction", "0 1 0 1 0 1");
    simNode->SetStringProperty("xq.sim.pressure_coupling", "Implicit");
    simNode->SetBoolProperty("xq.sim.stabilization", true);
    simNode->SetIntProperty("xq.sim.num_processors", 4);
    simNode->SetIntProperty("xq.sim.bc_count", 1);
    simNode->SetIntProperty("xq.sim.current_bc_index", 0);
    simNode->SetStringProperty(
        "xq.sim.bc_table", "inlet%7Cmain|Prescribed%20Velocities|10.0");
    simNode->SetStringProperty("xq.sim.export_warnings",
        "Using placeholder resistance values; native solver run is disabled.");
    simNode->SetStringProperty(
        "xq.solver.command_line",
        "xq_simple_flow --case_dir /tmp/xq/test_fullsim --num_processors 4");
    simNode->SetBoolProperty("show contour", true);

    auto simsFolder = FindFolderNode(ds, "SimulationFolder");
    if (simsFolder.IsNull())
    {
        std::cerr << "FAIL test_project_full_openproject_roundtrip_custom_xq_types: "
                  << "SimulationFolder not found\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    ds->Add(simNode, simsFolder);
    std::cout << "  [OK] SimulationPrep node added under Simulations folder\n";

    auto resultTetMesh = MakeTetrahedralMesh();
    auto* resultGrid = new xq_TetGenGrid();
    resultGrid->SetVolumeMesh(resultTetMesh);
    auto resultMitkGrid = xq_MitkGrid::New();
    resultMitkGrid->SetMesh(resultGrid, 0);

    auto resultNode = mitk::DataNode::New();
    resultNode->SetName("test_fullsim_result_1");
    resultNode->SetData(resultMitkGrid);
    xq::pipeline::MarkGeneratedNode(
        resultNode,
        xq::pipeline::Stage::Result,
        "xq_simple_flow",
        "org.xq.imaging.flowanalysis",
        "1");
    resultNode->SetStringProperty("xq.type", "result");
    resultNode->SetStringProperty("xq.result.field_names",
                                  "point:pressure,point:velocity,cell:wall_shear");
    resultNode->SetStringProperty("xq.result.field_name", "point:pressure");
    resultNode->SetStringProperty("xq.result.field_type", "pressure");
    resultNode->SetStringProperty("xq.result.active_scalar", "point:pressure");
    resultNode->SetStringProperty("xq.result.dataset_type", "unstructured_grid");
    resultNode->SetStringProperty("xq.result.file_path",
                                  "/tmp/xq/test_fullsim/results/xq_simple_flow_result_0001.vtu");
    resultNode->SetStringProperty("xq.result.backend_id", "xq_simple_flow");
    resultNode->SetStringProperty("xq.result.backend_name", "XQ Simple Flow");
    resultNode->SetStringProperty("xq.result.backend_version", "1");
    resultNode->SetStringProperty("xq.result.importer_version", "1");
    resultNode->SetStringProperty("xq.result.units.pressure", "dyn/cm^2");
    resultNode->SetStringProperty("xq.result.units.velocity", "cm/s");
    resultNode->SetStringProperty("xq.result.units.wall_shear", "dyn/cm^2");
    xq::pipeline::SetStringProperty(
        resultNode, xq::pipeline::kSourceSimulationJobProperty, "test_fullsim");
    xq::pipeline::SetStringProperty(
        resultNode, xq::pipeline::kSourceSolverCaseProperty, "/tmp/xq/test_fullsim");
    resultNode->SetIntProperty("xq.result.time_step", 0);
    resultNode->SetIntProperty("xq.result.time_step_index", 0);
    resultNode->SetIntProperty("xq.result.time_step_count", 1);
    resultNode->SetDoubleProperty("xq.result.time_value", 0.25);
    resultNode->SetIntProperty("xq.result.field_count", 3);
    resultNode->SetIntProperty("xq.result.volume_points", 4);
    resultNode->SetIntProperty("xq.result.volume_cells", 1);
    resultNode->SetBoolProperty("xq.result.volume_grid_preserved", true);
    resultNode->SetBoolProperty("xq.result.visual_surface_derived", true);
    ds->Add(resultNode, simsFolder);
    std::cout << "  [OK] Result node added under Simulations folder\n";
    std::cout << "  [OK] All 6 pipeline nodes created with source links\n";

    // 4. Save project via WorkspaceManager
    {
        xq_WorkspaceManager saveWM;
        if (!saveWM.SaveProject(ds, projectDir))
        {
            std::cerr << "FAIL test_project_full_openproject_roundtrip_custom_xq_types: "
                      << "SaveProject returned false\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] SaveProject succeeded\n";

    // 5. Create new StandaloneDataStorage
    mitk::DataStorage::Pointer ds2 = mitk::StandaloneDataStorage::New();

    // 6. OpenProject on ds2
    {
        xq_WorkspaceManager openWM;
        if (!openWM.OpenProject(ds2, projFilePath))
        {
            std::cerr << "FAIL test_project_full_openproject_roundtrip_custom_xq_types: "
                      << "OpenProject returned false\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] OpenProject on ds2 succeeded\n";

    // 7. Find all 5 nodes by name in ds2 (under correct category folders)
    auto reopenedPathN = FindChildNode(ds2,
        FindFolderNode(ds2, "PathFolder"), "test_fullpath");
    auto reopenedContourN = FindChildNode(ds2,
        FindFolderNode(ds2, "SegmentationFolder"), "test_fullcontours");
    auto reopenedModelN = FindChildNode(ds2,
        FindFolderNode(ds2, "ModelFolder"), "test_fullmodel");
    auto reopenedMeshN = FindChildNode(ds2,
        FindFolderNode(ds2, "MeshFolder"), "test_fullmesh");
    auto reopenedSimN = FindChildNode(ds2,
        FindFolderNode(ds2, "SimulationFolder"), "test_fullsim");
    auto reopenedResultN = FindChildNode(ds2,
        FindFolderNode(ds2, "SimulationFolder"), "test_fullsim_result_1");

    if (reopenedPathN.IsNull() || reopenedContourN.IsNull() ||
        reopenedModelN.IsNull() || reopenedMeshN.IsNull() ||
        reopenedSimN.IsNull() || reopenedResultN.IsNull())
    {
        std::cerr << "FAIL test_project_full_openproject_roundtrip_custom_xq_types: "
                  << "One or more nodes not found after OpenProject\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    std::cout << "  [OK] All 6 nodes found by name after OpenProject\n";

    // 8. Verify dynamic_cast for all 5 types
    {
        auto* pathCL = dynamic_cast<const xq_VesselCenterline*>(
            reopenedPathN->GetData());
        if (!pathCL)
        {
            std::cerr << "FAIL: Path node data is not xq_VesselCenterline\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        auto* seg = pathCL->GetSegment(0);
        if (!seg || seg->GetAnchorCount() != 3)
        {
            std::cerr << "FAIL: Path anchor count mismatch (expected 3, got "
                      << (seg ? seg->GetAnchorCount() : 0) << ")\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Path verified: xq_VesselCenterline with 3 anchors\n";

    {
        auto* cg = dynamic_cast<const xq_ProfileGroup*>(
            reopenedContourN->GetData());
        if (!cg)
        {
            std::cerr << "FAIL: Contour node data is not xq_ProfileGroup\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        if (cg->GetProfileCount(0) < 1)
        {
            std::cerr << "FAIL: Contour group has no profiles\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] ContourGroup verified: xq_ProfileGroup with profiles\n";

    {
        auto* m = dynamic_cast<const xq_Model*>(reopenedModelN->GetData());
        if (!m)
        {
            std::cerr << "FAIL: Model node data is not xq_Model\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        auto* geom = m->GetModelElement(0);
        if (!geom || !geom->GetWholeVtkPolyData() ||
            geom->GetWholeVtkPolyData()->GetNumberOfPoints() == 0)
        {
            std::cerr << "FAIL: Model geometry missing or empty\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Model verified: xq_Model with geometry\n";

    {
        auto* mg = dynamic_cast<const xq_MitkGrid*>(reopenedMeshN->GetData());
        if (!mg)
        {
            std::cerr << "FAIL: Mesh node data is not xq_MitkGrid\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        auto* meshObj = mg->GetMesh(0);
        if (!meshObj || !meshObj->GetVolumeMesh() ||
            meshObj->GetVolumeMesh()->GetNumberOfPoints() == 0)
        {
            std::cerr << "FAIL: Mesh volume data missing or empty\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Mesh verified: xq_MitkGrid with volume mesh\n";

    {
        auto* sj = dynamic_cast<const xq_MitkSolverJob*>(reopenedSimN->GetData());
        if (!sj)
        {
            std::cerr << "FAIL: Sim node data is not xq_MitkSolverJob\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] SimulationPrep verified: xq_MitkSolverJob\n";
    std::cout << "  [OK] All 5 data types verified via dynamic_cast\n";

    // 9. Verify source properties survived
    {
        std::string srcPath;
        if (!reopenedContourN->GetStringProperty(
                xq::pipeline::kSourcePathProperty, srcPath) ||
            srcPath != "test_fullpath")
        {
            std::cerr << "FAIL: Contour source path expected 'test_fullpath', got '"
                      << srcPath << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    {
        std::string srcCG;
        if (!reopenedModelN->GetStringProperty(
                xq::pipeline::kSourceContourGroupsProperty, srcCG) ||
            srcCG != "test_fullcontours")
        {
            std::cerr << "FAIL: Model source contours expected 'test_fullcontours', got '"
                      << srcCG << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string srcPath;
        if (!reopenedModelN->GetStringProperty(
                xq::pipeline::kSourcePathProperty, srcPath) ||
            srcPath != "test_fullpath")
        {
            std::cerr << "FAIL: Model source path expected 'test_fullpath', got '"
                      << srcPath << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    {
        std::string srcModel;
        if (!reopenedMeshN->GetStringProperty(
                xq::pipeline::kSourceModelProperty, srcModel) ||
            srcModel != "test_fullmodel")
        {
            std::cerr << "FAIL: Mesh source model expected 'test_fullmodel', got '"
                      << srcModel << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    {
        std::string srcMesh;
        if (!reopenedSimN->GetStringProperty(
                xq::pipeline::kSourceMeshProperty, srcMesh) ||
            srcMesh != "test_fullmesh")
        {
            std::cerr << "FAIL: Sim source mesh expected 'test_fullmesh', got '"
                      << srcMesh << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string srcModel;
        if (!reopenedSimN->GetStringProperty(
                xq::pipeline::kSourceModelProperty, srcModel) ||
            srcModel != "test_fullmodel")
        {
            std::cerr << "FAIL: Sim source model expected 'test_fullmodel', got '"
                      << srcModel << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] All source properties survived roundtrip\n";

    // 10. Verify ResolveUpstreamNode works
    {
        // mesh -> model
        auto resolvedModel = xq::pipeline::ResolveUpstreamNode(
            ds2.GetPointer(), reopenedMeshN.GetPointer(),
            xq::pipeline::kSourceModelProperty, xq::pipeline::Stage::Model);
        if (resolvedModel.IsNull())
        {
            std::cerr << "FAIL: ResolveUpstreamNode mesh->model returned null\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        if (resolvedModel->GetName() != "test_fullmodel")
        {
            std::cerr << "FAIL: ResolveUpstreamNode mesh->model name mismatch (expected "
                      << "test_fullmodel, got " << resolvedModel->GetName() << ")\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::cout << "  [OK] ResolveUpstreamNode mesh->model resolved correctly\n";
    }
    {
        // sim -> mesh
        auto resolvedMesh = xq::pipeline::ResolveUpstreamNode(
            ds2.GetPointer(), reopenedSimN.GetPointer(),
            xq::pipeline::kSourceMeshProperty, xq::pipeline::Stage::VolumeMesh);
        if (resolvedMesh.IsNull())
        {
            std::cerr << "FAIL: ResolveUpstreamNode sim->mesh returned null\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        if (resolvedMesh->GetName() != "test_fullmesh")
        {
            std::cerr << "FAIL: ResolveUpstreamNode sim->mesh name mismatch (expected "
                      << "test_fullmesh, got " << resolvedMesh->GetName() << ")\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::cout << "  [OK] ResolveUpstreamNode sim->mesh resolved correctly\n";
    }
    {
        // sim -> model
        auto resolvedModel2 = xq::pipeline::ResolveUpstreamNode(
            ds2.GetPointer(), reopenedSimN.GetPointer(),
            xq::pipeline::kSourceModelProperty, xq::pipeline::Stage::Model);
        if (resolvedModel2.IsNull())
        {
            std::cerr << "FAIL: ResolveUpstreamNode sim->model returned null\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        if (resolvedModel2->GetName() != "test_fullmodel")
        {
            std::cerr << "FAIL: ResolveUpstreamNode sim->model name mismatch (expected "
                      << "test_fullmodel, got " << resolvedModel2->GetName() << ")\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::cout << "  [OK] ResolveUpstreamNode sim->model resolved correctly\n";
    }
    std::cout << "  [OK] All upstream node resolutions succeeded\n";

    // 11. Verify typed metadata properties survived round-trip

    // 11a. Contour typed properties
    {
        bool contourReady = false;
        if (!reopenedContourN->GetBoolProperty("xq.contour.ready", contourReady) || !contourReady)
        {
            std::cerr << "FAIL: xq.contour.ready not preserved (value="
                      << contourReady << ")\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int profileCount = 0;
        if (!reopenedContourN->GetIntProperty("xq.contour.profile_count", profileCount) ||
            profileCount != 2)
        {
            std::cerr << "FAIL: xq.contour.profile_count expected 2, got "
                      << profileCount << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Contour typed properties survived roundtrip\n";

    // 11b. Model QA typed properties
    {
        std::string modelType;
        if (!reopenedModelN->GetStringProperty("xq.model.type", modelType) ||
            modelType != "PolyData")
        {
            std::cerr << "FAIL: xq.model.type expected 'PolyData', got '"
                      << modelType << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int sampling = -1;
        if (!reopenedModelN->GetIntProperty("xq.model.sampling", sampling) ||
            sampling != 48)
        {
            std::cerr << "FAIL: xq.model.sampling expected 48, got "
                      << sampling << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string loftParams;
        if (!reopenedModelN->GetStringProperty("xq.model.loft.parameters", loftParams) ||
            loftParams.find("sampling=48") == std::string::npos ||
            loftParams.find("engine=occt") == std::string::npos)
        {
            std::cerr << "FAIL: xq.model.loft.parameters not preserved, got '"
                      << loftParams << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string capInfo;
        if (!reopenedModelN->GetStringProperty("xq.model.cap_info", capInfo) ||
            capInfo.find("cap_1") == std::string::npos)
        {
            std::cerr << "FAIL: xq.model.cap_info not preserved, got '"
                      << capInfo << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string requestedEngine;
        std::string actualEngine;
        bool fallback = false;
        if (!reopenedModelN->GetStringProperty("xq.model.requested_engine", requestedEngine) ||
            requestedEngine != "occt" ||
            !reopenedModelN->GetStringProperty("xq.model.actual_engine", actualEngine) ||
            actualEngine != "vtk_fallback" ||
            !reopenedModelN->GetBoolProperty("xq.model.algorithm.fallback", fallback) ||
            !fallback)
        {
            std::cerr << "FAIL: model engine fallback metadata did not roundtrip\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        bool qaOk = false;
        if (!reopenedModelN->GetBoolProperty("xq.model.qa.ok", qaOk) || !qaOk)
        {
            std::cerr << "FAIL: xq.model.qa.ok not preserved (value=" << qaOk << ")\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int boundaryEdges = -1;
        if (!reopenedModelN->GetIntProperty("xq.model.qa.boundary_edges", boundaryEdges) ||
            boundaryEdges != 0)
        {
            std::cerr << "FAIL: xq.model.qa.boundary_edges expected 0, got "
                      << boundaryEdges << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int nonManifoldEdges = -1;
        if (!reopenedModelN->GetIntProperty("xq.model.qa.non_manifold_edges", nonManifoldEdges) ||
            nonManifoldEdges != 0)
        {
            std::cerr << "FAIL: xq.model.qa.non_manifold_edges expected 0, got "
                      << nonManifoldEdges << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int connectedComponents = -1;
        if (!reopenedModelN->GetIntProperty("xq.model.qa.connected_components", connectedComponents) ||
            connectedComponents != 1)
        {
            std::cerr << "FAIL: xq.model.qa.connected_components expected 1, got "
                      << connectedComponents << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        bool hasFaceIds = false;
        if (!reopenedModelN->GetBoolProperty("xq.model.qa.has_face_ids", hasFaceIds) ||
            !hasFaceIds)
        {
            std::cerr << "FAIL: xq.model.qa.has_face_ids not preserved (value="
                      << hasFaceIds << ")\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int faceCount = -1;
        if (!reopenedModelN->GetIntProperty("xq.model.face_count", faceCount) ||
            faceCount != 1)
        {
            std::cerr << "FAIL: xq.model.face_count expected 1, got "
                      << faceCount << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int selectedFaceId = -1;
        if (!reopenedModelN->GetIntProperty("selectedFaceId", selectedFaceId) ||
            selectedFaceId != 1)
        {
            std::cerr << "FAIL: selectedFaceId expected 1, got "
                      << selectedFaceId << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Model QA typed properties survived roundtrip\n";

    // 11c. Mesh QA typed properties
    {
        bool qaOk = false;
        if (!reopenedMeshN->GetBoolProperty("xq.mesh.qa.ok", qaOk) || !qaOk)
        {
            std::cerr << "FAIL: xq.mesh.qa.ok not preserved (value=" << qaOk << ")\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int cells = -1;
        if (!reopenedMeshN->GetIntProperty("xq.mesh.cells", cells) ||
            cells != 1)
        {
            std::cerr << "FAIL: xq.mesh.cells expected 1, got " << cells << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int points = -1;
        if (!reopenedMeshN->GetIntProperty("xq.mesh.points", points) ||
            points != 4)
        {
            std::cerr << "FAIL: xq.mesh.points expected 4, got " << points << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int negativeVol = -1;
        if (!reopenedMeshN->GetIntProperty("xq.mesh.negative_volume_count", negativeVol) ||
            negativeVol != 0)
        {
            std::cerr << "FAIL: xq.mesh.negative_volume_count expected 0, got "
                      << negativeVol << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double minVol = 0.0;
        if (!reopenedMeshN->GetDoubleProperty("xq.mesh.min_volume", minVol) ||
            std::abs(minVol - 0.1666667) > 1e-6)
        {
            std::cerr << "FAIL: xq.mesh.min_volume expected 0.1666667, got "
                      << minVol << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double maxVol = 0.0;
        if (!reopenedMeshN->GetDoubleProperty("xq.mesh.max_volume", maxVol) ||
            std::abs(maxVol - 0.1666667) > 1e-6)
        {
            std::cerr << "FAIL: xq.mesh.max_volume expected 0.1666667, got "
                      << maxVol << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double globalEdgeSize = 0.0;
        if (!reopenedMeshN->GetDoubleProperty("xq.mesh.globalEdgeSize", globalEdgeSize) ||
            std::abs(globalEdgeSize - 2.5) > 1e-6)
        {
            std::cerr << "FAIL: xq.mesh.globalEdgeSize expected 2.5, got "
                      << globalEdgeSize << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string localFaceSizeValues;
        if (!reopenedMeshN->GetStringProperty(
                "xq.mesh.local_face_sizes.values", localFaceSizeValues) ||
            localFaceSizeValues != "1:1.25")
        {
            std::cerr << "FAIL: xq.mesh.local_face_sizes.values expected '1:1.25', got '"
                      << localFaceSizeValues << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string refinementRegionValues;
        if (!reopenedMeshN->GetStringProperty(
                "xq.mesh.refinement_regions.values", refinementRegionValues) ||
            refinementRegionValues != "Sphere,0,0,5,2,2,2,0.75")
        {
            std::cerr << "FAIL: xq.mesh.refinement_regions.values not preserved, got '"
                      << refinementRegionValues << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int blLayers = -1;
        if (!reopenedMeshN->GetIntProperty("xq.mesh.bl.layers", blLayers) ||
            blLayers != 2)
        {
            std::cerr << "FAIL: xq.mesh.bl.layers expected 2, got " << blLayers << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double blFirstHeight = 0.0;
        if (!reopenedMeshN->GetDoubleProperty("xq.mesh.bl.firstHeight", blFirstHeight) ||
            std::abs(blFirstHeight - 0.1) > 1e-6)
        {
            std::cerr << "FAIL: xq.mesh.bl.firstHeight expected 0.1, got "
                      << blFirstHeight << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double blGrowthRate = 0.0;
        if (!reopenedMeshN->GetDoubleProperty("xq.mesh.bl.growthRate", blGrowthRate) ||
            std::abs(blGrowthRate - 1.3) > 1e-6)
        {
            std::cerr << "FAIL: xq.mesh.bl.growthRate expected 1.3, got "
                      << blGrowthRate << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        bool preserveSurface = false;
        if (!reopenedMeshN->GetBoolProperty("xq.mesh.preserve_surface", preserveSurface) ||
            !preserveSurface)
        {
            std::cerr << "FAIL: xq.mesh.preserve_surface not preserved (value="
                      << preserveSurface << ")\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        bool optimize = false;
        if (!reopenedMeshN->GetBoolProperty("xq.mesh.optimize", optimize) ||
            !optimize)
        {
            std::cerr << "FAIL: xq.mesh.optimize not preserved (value="
                      << optimize << ")\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double minDihedral = 0.0;
        if (!reopenedMeshN->GetDoubleProperty("xq.mesh.min_dihedral", minDihedral) ||
            std::abs(minDihedral - 10.0) > 1e-6)
        {
            std::cerr << "FAIL: xq.mesh.min_dihedral expected 10, got "
                      << minDihedral << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double maxEdgeSize = -1.0;
        if (!reopenedMeshN->GetDoubleProperty("xq.mesh.max_edge_size", maxEdgeSize) ||
            std::abs(maxEdgeSize - 0.0) > 1e-6)
        {
            std::cerr << "FAIL: xq.mesh.max_edge_size expected 0, got "
                      << maxEdgeSize << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        bool scalarVisibility = false;
        if (!reopenedMeshN->GetBoolProperty("scalar visibility", scalarVisibility) ||
            !scalarVisibility)
        {
            std::cerr << "FAIL: scalar visibility not preserved (value="
                      << scalarVisibility << ")\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Mesh QA typed properties survived roundtrip\n";

    // 11d. Simulation typed properties
    {
        std::string simStatus;
        if (!reopenedSimN->GetStringProperty("xq.sim.status", simStatus) ||
            simStatus != "configured")
        {
            std::cerr << "FAIL: xq.sim.status expected 'configured', got '"
                      << simStatus << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        bool showContour = false;
        if (!reopenedSimN->GetBoolProperty("show contour", showContour) ||
            !showContour)
        {
            std::cerr << "FAIL: show contour not preserved (value="
                      << showContour << ")\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string solverType;
        if (!reopenedSimN->GetStringProperty("xq.sim.solver_type", solverType) ||
            solverType != "svLS")
        {
            std::cerr << "FAIL: xq.sim.solver_type expected 'svLS', got '"
                      << solverType << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        bool deformableWall = true;
        if (!reopenedSimN->GetBoolProperty("xq.sim.deformable_wall", deformableWall) ||
            deformableWall)
        {
            std::cerr << "FAIL: xq.sim.deformable_wall expected false, got "
                      << deformableWall << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double fluidDensity = 0.0;
        if (!reopenedSimN->GetDoubleProperty("xq.sim.fluid_density", fluidDensity) ||
            std::abs(fluidDensity - 1.06) > 1e-6)
        {
            std::cerr << "FAIL: xq.sim.fluid_density expected 1.06, got "
                      << fluidDensity << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double fluidViscosity = 0.0;
        if (!reopenedSimN->GetDoubleProperty("xq.sim.fluid_viscosity", fluidViscosity) ||
            std::abs(fluidViscosity - 0.04) > 1e-6)
        {
            std::cerr << "FAIL: xq.sim.fluid_viscosity expected 0.04, got "
                      << fluidViscosity << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double initialPressure = 0.0;
        if (!reopenedSimN->GetDoubleProperty("xq.sim.initial_pressure", initialPressure) ||
            std::abs(initialPressure - 80.0) > 1e-6)
        {
            std::cerr << "FAIL: xq.sim.initial_pressure expected 80, got "
                      << initialPressure << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double initialVelocity = -1.0;
        if (!reopenedSimN->GetDoubleProperty("xq.sim.initial_velocity", initialVelocity) ||
            std::abs(initialVelocity - 0.0) > 1e-6)
        {
            std::cerr << "FAIL: xq.sim.initial_velocity expected 0, got "
                      << initialVelocity << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double wallThickness = 0.0;
        if (!reopenedSimN->GetDoubleProperty("xq.sim.wall_thickness", wallThickness) ||
            std::abs(wallThickness - 0.5) > 1e-6)
        {
            std::cerr << "FAIL: xq.sim.wall_thickness expected 0.5, got "
                      << wallThickness << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double wallElasticModulus = 0.0;
        if (!reopenedSimN->GetDoubleProperty("xq.sim.wall_elastic_modulus", wallElasticModulus) ||
            std::abs(wallElasticModulus - 4000000.0) > 1e-3)
        {
            std::cerr << "FAIL: xq.sim.wall_elastic_modulus expected 4000000, got "
                      << wallElasticModulus << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double wallPoissonRatio = 0.0;
        if (!reopenedSimN->GetDoubleProperty("xq.sim.wall_poisson_ratio", wallPoissonRatio) ||
            std::abs(wallPoissonRatio - 0.45) > 1e-6)
        {
            std::cerr << "FAIL: xq.sim.wall_poisson_ratio expected 0.45, got "
                      << wallPoissonRatio << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double wallDensity = 0.0;
        if (!reopenedSimN->GetDoubleProperty("xq.sim.wall_density", wallDensity) ||
            std::abs(wallDensity - 1.0) > 1e-6)
        {
            std::cerr << "FAIL: xq.sim.wall_density expected 1, got "
                      << wallDensity << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int numTimesteps = -1;
        if (!reopenedSimN->GetIntProperty("xq.sim.num_timesteps", numTimesteps) ||
            numTimesteps != 200)
        {
            std::cerr << "FAIL: xq.sim.num_timesteps expected 200, got "
                      << numTimesteps << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int numCycles = -1;
        if (!reopenedSimN->GetIntProperty("xq.sim.num_cycles", numCycles) ||
            numCycles != 2)
        {
            std::cerr << "FAIL: xq.sim.num_cycles expected 2, got "
                      << numCycles << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int numLinearIterations = -1;
        if (!reopenedSimN->GetIntProperty("xq.sim.num_linear_iterations", numLinearIterations) ||
            numLinearIterations != 10)
        {
            std::cerr << "FAIL: xq.sim.num_linear_iterations expected 10, got "
                      << numLinearIterations << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int numNonlinearIterations = -1;
        if (!reopenedSimN->GetIntProperty("xq.sim.num_nonlinear_iterations", numNonlinearIterations) ||
            numNonlinearIterations != 2)
        {
            std::cerr << "FAIL: xq.sim.num_nonlinear_iterations expected 2, got "
                      << numNonlinearIterations << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double timeStepSize = 0.0;
        if (!reopenedSimN->GetDoubleProperty("xq.sim.time_step_size", timeStepSize) ||
            std::abs(timeStepSize - 0.001) > 1e-9)
        {
            std::cerr << "FAIL: xq.sim.time_step_size expected 0.001, got "
                      << timeStepSize << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double startTime = 0.0;
        if (!reopenedSimN->GetDoubleProperty("xq.sim.start_time", startTime) ||
            std::abs(startTime - 0.25) > 1e-9)
        {
            std::cerr << "FAIL: xq.sim.start_time expected 0.25, got "
                      << startTime << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double endTime = 0.0;
        if (!reopenedSimN->GetDoubleProperty("xq.sim.end_time", endTime) ||
            std::abs(endTime - 1.25) > 1e-9)
        {
            std::cerr << "FAIL: xq.sim.end_time expected 1.25, got "
                      << endTime << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double residualTolerance = 0.0;
        if (!reopenedSimN->GetDoubleProperty(
                "xq.sim.residual_tolerance", residualTolerance) ||
            std::abs(residualTolerance - 0.0001) > 1e-12)
        {
            std::cerr << "FAIL: xq.sim.residual_tolerance expected 0.0001, got "
                      << residualTolerance << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string solverPreset;
        if (!reopenedSimN->GetStringProperty("xq.sim.solver_preset", solverPreset) ||
            solverPreset != "Pulsatile Flow")
        {
            std::cerr << "FAIL: xq.sim.solver_preset expected 'Pulsatile Flow', got '"
                      << solverPreset << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string stepConstruction;
        if (!reopenedSimN->GetStringProperty(
                "xq.sim.step_construction", stepConstruction) ||
            stepConstruction != "0 1 0 1 0 1")
        {
            std::cerr << "FAIL: xq.sim.step_construction did not roundtrip, got '"
                      << stepConstruction << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string pressureCoupling;
        if (!reopenedSimN->GetStringProperty(
                "xq.sim.pressure_coupling", pressureCoupling) ||
            pressureCoupling != "Implicit")
        {
            std::cerr << "FAIL: xq.sim.pressure_coupling did not roundtrip, got '"
                      << pressureCoupling << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        bool stabilization = false;
        if (!reopenedSimN->GetBoolProperty("xq.sim.stabilization", stabilization) ||
            !stabilization)
        {
            std::cerr << "FAIL: xq.sim.stabilization expected true\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int numProcessors = -1;
        if (!reopenedSimN->GetIntProperty("xq.sim.num_processors", numProcessors) ||
            numProcessors != 4)
        {
            std::cerr << "FAIL: xq.sim.num_processors expected 4, got "
                      << numProcessors << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int faceRoleCount = -1;
        if (!reopenedSimN->GetIntProperty("xq.simprep.face_role_count", faceRoleCount) ||
            faceRoleCount != 1)
        {
            std::cerr << "FAIL: xq.simprep.face_role_count expected 1, got "
                      << faceRoleCount << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int bcCount = -1;
        if (!reopenedSimN->GetIntProperty("xq.sim.bc_count", bcCount) ||
            bcCount != 1)
        {
            std::cerr << "FAIL: xq.sim.bc_count expected 1, got "
                      << bcCount << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int currentBcIndex = -1;
        if (!reopenedSimN->GetIntProperty("xq.sim.current_bc_index", currentBcIndex) ||
            currentBcIndex != 0)
        {
            std::cerr << "FAIL: xq.sim.current_bc_index expected 0, got "
                      << currentBcIndex << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string bcTable;
        if (!reopenedSimN->GetStringProperty("xq.sim.bc_table", bcTable) ||
            bcTable.find("inlet%7Cmain") == std::string::npos)
        {
            std::cerr << "FAIL: xq.sim.bc_table did not roundtrip, got '"
                      << bcTable << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string exportWarnings;
        if (!reopenedSimN->GetStringProperty("xq.sim.export_warnings", exportWarnings) ||
            exportWarnings.find("placeholder") == std::string::npos)
        {
            std::cerr << "FAIL: xq.sim.export_warnings did not roundtrip, got '"
                      << exportWarnings << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        std::string solverCommandLine;
        if (!reopenedSimN->GetStringProperty(
                "xq.solver.command_line", solverCommandLine) ||
            solverCommandLine.find("xq_simple_flow") == std::string::npos ||
            solverCommandLine.find("--num_processors 4") == std::string::npos)
        {
            std::cerr << "FAIL: xq.solver.command_line did not roundtrip, got '"
                      << solverCommandLine << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Simulation typed properties survived roundtrip\n";

    // 11e. Result metadata
    {
        std::string resultBackend;
        if (!reopenedResultN->GetStringProperty("xq.result.backend_id", resultBackend) ||
            resultBackend != "xq_simple_flow")
        {
            std::cerr << "FAIL: xq.result.backend_id did not roundtrip, got '"
                      << resultBackend << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        int resultStep = -1;
        if (!reopenedResultN->GetIntProperty("xq.result.time_step", resultStep) ||
            resultStep != 0)
        {
            std::cerr << "FAIL: xq.result.time_step expected 0, got "
                      << resultStep << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        double resultTime = -1.0;
        if (!reopenedResultN->GetDoubleProperty("xq.result.time_value", resultTime) ||
            std::abs(resultTime - 0.25) > 1e-9)
        {
            std::cerr << "FAIL: xq.result.time_value expected 0.25, got "
                      << resultTime << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Result metadata survived roundtrip\n";

    // 11f. Solver job fields (solver/wall/BC XML round-trip)
    {
        auto* reopenedSimJob = dynamic_cast<const xq_MitkSolverJob*>(
            reopenedSimN->GetData());
        if (!reopenedSimJob)
        {
            std::cerr << "FAIL: reopenedSimN data is not xq_MitkSolverJob\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        auto* reopenedJob = reopenedSimJob->GetSimJob(0);
        if (!reopenedJob)
        {
            std::cerr << "FAIL: GetSimJob(0) returned null\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        // Verify solver type
        if (reopenedJob->GetSolverType() != "svLS")
        {
            std::cerr << "FAIL: solverType expected 'svLS', got '"
                      << reopenedJob->GetSolverType() << "'\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        // Verify linear iterations
        if (reopenedJob->GetNumLinearIterations() != 10)
        {
            std::cerr << "FAIL: numLinearIterations expected 10, got "
                      << reopenedJob->GetNumLinearIterations() << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        // Verify nonlinear iterations
        if (reopenedJob->GetNumNonlinearIterations() != 2)
        {
            std::cerr << "FAIL: numNonlinearIterations expected 2, got "
                      << reopenedJob->GetNumNonlinearIterations() << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        // Verify wall Poisson ratio
        if (std::abs(reopenedJob->GetWallPoissonRatio() - 0.45) > 1e-9)
        {
            std::cerr << "FAIL: wallPoissonRatio expected 0.45, got "
                      << reopenedJob->GetWallPoissonRatio() << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        // Verify boundary conditions
        const auto& bcs = reopenedJob->GetBoundaryConditions();
        if (bcs.size() < 1)
        {
            std::cerr << "FAIL: boundaryConditions.size() expected >= 1, got "
                      << bcs.size() << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
        // Verify BC parameter
        auto it = bcs[0].parameters.find("flow_rate");
        if (it == bcs[0].parameters.end() || it->second != "10.0")
        {
            std::cerr << "FAIL: boundaryConditions[0].parameters['flow_rate'] expected '10.0'"
                      << "\n";
            RemoveDir(kTestProjectPath);
            return 1;
        }
    }
    std::cout << "  [OK] Solver job fields (solver/wall/BC) survived roundtrip\n";

    std::cout << "  [OK] All typed metadata + xqsim XML fields survived roundtrip\n";

    // Cleanup
    RemoveDir(kTestProjectPath);

    std::cout << "PASS test_project_full_openproject_roundtrip_custom_xq_types\n";
    return 0;
}

// ---------------------------------------------------------------------------
// PipelineConsistency tests (Task 9)
// ---------------------------------------------------------------------------

static int test_pipeline_consistency_detects_missing_sources()
{
    std::cout << "--- test_pipeline_consistency_detects_missing_sources ---\n";

    mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

    // Create a Meshes folder (correct folder for VolumeMesh)
    auto meshFolder = mitk::DataNode::New();
    meshFolder->SetName("Meshes");
    meshFolder->SetData(xq_GridFolder::New());
    meshFolder->SetVisibility(false);
    ds->Add(meshFolder);

    // Create a VolumeMesh node referencing a nonexistent model
    auto tetMesh = MakeTetrahedralMesh();
    auto* tetGenGrid = new xq_TetGenGrid();
    tetGenGrid->SetVolumeMesh(tetMesh);

    auto mitkGrid = xq_MitkGrid::New();
    mitkGrid->SetMesh(tetGenGrid, 0);

    auto meshNode = mitk::DataNode::New();
    meshNode->SetName("test_mesh");
    meshNode->SetData(mitkGrid);
    xq::pipeline::MarkNode(meshNode, xq::pipeline::Stage::VolumeMesh);
    xq::pipeline::SetStringProperty(meshNode,
        xq::pipeline::kSourceModelProperty, "nonexistent_model");
    ds->Add(meshNode, meshFolder);

    auto issues = CheckDataStorage(ds.GetPointer());

    bool foundSourceNotFound = false;
    for (const auto& issue : issues)
    {
        if (issue.message.find("Source node not found") != std::string::npos)
        {
            foundSourceNotFound = true;
            break;
        }
    }

    if (!foundSourceNotFound)
    {
        std::cerr << "FAIL: Missing source not detected by CheckDataStorage\n";
        return 1;
    }

    std::cout << "PASS test_pipeline_consistency_detects_missing_sources\n";
    return 0;
}

static int test_pipeline_consistency_detects_wrong_folder()
{
    std::cout << "--- test_pipeline_consistency_detects_wrong_folder ---\n";

    mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

    // Create a Meshes folder (xq_GridFolder) — this is the WRONG folder for a Model
    auto meshFolder = mitk::DataNode::New();
    meshFolder->SetName("Meshes");
    meshFolder->SetData(xq_GridFolder::New());
    meshFolder->SetVisibility(false);
    ds->Add(meshFolder);

    // Create a Model node placed under the Meshes folder (wrong folder)
    auto spherePD = MakeSpherePolyData();
    auto* polyGeom = new xq_PolyGeometry();
    polyGeom->SetWholeVtkPolyData(spherePD);

    auto modelData = xq_Model::New();
    modelData->SetModelElement(polyGeom, 0);

    auto modelNode = mitk::DataNode::New();
    modelNode->SetName("test_model");
    modelNode->SetData(modelData);
    xq::pipeline::MarkNode(modelNode, xq::pipeline::Stage::Model);
    ds->Add(modelNode, meshFolder);  // placed under wrong folder intentionally

    auto issues = CheckDataStorage(ds.GetPointer());

    bool foundFolderMismatch = false;
    for (const auto& issue : issues)
    {
        if (issue.message.find("Pipeline node is not under the expected folder") != std::string::npos)
        {
            foundFolderMismatch = true;
            break;
        }
    }

    if (!foundFolderMismatch)
    {
        std::cerr << "FAIL: Wrong folder placement not detected by CheckDataStorage\n";
        return 1;
    }

    std::cout << "PASS test_pipeline_consistency_detects_wrong_folder\n";
    return 0;
}

static int test_pipeline_consistency_detects_qa_failure()
{
    std::cout << "--- test_pipeline_consistency_detects_qa_failure ---\n";

    mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

    // Create a Models folder (correct folder for a Model)
    auto modelFolder = mitk::DataNode::New();
    modelFolder->SetName("Models");
    modelFolder->SetData(xq_ModelFolder::New());
    modelFolder->SetVisibility(false);
    ds->Add(modelFolder);

    // Create a Model node with QA failure
    auto spherePD = MakeSpherePolyData();
    auto* polyGeom = new xq_PolyGeometry();
    polyGeom->SetWholeVtkPolyData(spherePD);

    auto modelData = xq_Model::New();
    modelData->SetModelElement(polyGeom, 0);

    auto modelNode = mitk::DataNode::New();
    modelNode->SetName("test_model");
    modelNode->SetData(modelData);
    xq::pipeline::MarkNode(modelNode, xq::pipeline::Stage::Model);
    modelNode->SetBoolProperty("xq.model.qa.ok", false);
    ds->Add(modelNode, modelFolder);

    auto issues = CheckDataStorage(ds.GetPointer());

    bool foundQaFailure = false;
    for (const auto& issue : issues)
    {
        if (issue.message.find("QA check failed") != std::string::npos)
        {
            foundQaFailure = true;
            break;
        }
    }

    if (!foundQaFailure)
    {
        std::cerr << "FAIL: QA failure not detected by CheckDataStorage\n";
        return 1;
    }

    std::cout << "PASS test_pipeline_consistency_detects_qa_failure\n";
    return 0;
}

static int test_pipeline_consistency_detects_type_mismatch()
{
    std::cout << "--- test_pipeline_consistency_detects_type_mismatch ---\n";

    mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

    // Create a Models folder (correct folder for a Model)
    auto modelFolder = mitk::DataNode::New();
    modelFolder->SetName("Models");
    modelFolder->SetData(xq_ModelFolder::New());
    modelFolder->SetVisibility(false);
    ds->Add(modelFolder);

    // Create a node marked Stage::Model but with mitk::Surface data
    auto surface = mitk::Surface::New();
    surface->SetVtkPolyData(MakeSpherePolyData());

    auto node = mitk::DataNode::New();
    node->SetName("test_surface_as_model");
    node->SetData(surface);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::Model);
    ds->Add(node, modelFolder);

    auto issues = CheckDataStorage(ds.GetPointer());

    bool foundTypeMismatch = false;
    for (const auto& issue : issues)
    {
        if (issue.message.find("legacy mitk::Surface") != std::string::npos)
        {
            foundTypeMismatch = true;
            break;
        }
    }

    if (!foundTypeMismatch)
    {
        std::cerr << "FAIL: Type mismatch not detected by CheckDataStorage\n";
        return 1;
    }

    std::cout << "PASS test_pipeline_consistency_detects_type_mismatch\n";
    return 0;
}

static int test_pipeline_consistency_refuses_ambiguous_missing_source()
{
    std::cout << "--- test_pipeline_consistency_refuses_ambiguous_missing_source ---\n";

    mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

    auto modelFolder = mitk::DataNode::New();
    modelFolder->SetName("Models");
    modelFolder->SetData(xq_ModelFolder::New());
    modelFolder->SetVisibility(false);
    ds->Add(modelFolder);

    for (int i = 0; i < 2; ++i)
    {
        auto spherePD = MakeSpherePolyData();
        auto* polyGeom = new xq_PolyGeometry();
        polyGeom->SetWholeVtkPolyData(spherePD);

        auto modelData = xq_Model::New();
        modelData->SetModelElement(polyGeom, 0);

        auto modelNode = mitk::DataNode::New();
        modelNode->SetName(i == 0 ? "candidate_model_a" : "candidate_model_b");
        modelNode->SetData(modelData);
        xq::pipeline::MarkNode(modelNode, xq::pipeline::Stage::Model);
        ds->Add(modelNode, modelFolder);
    }

    auto tetMesh = MakeTetrahedralMesh();
    auto* tetGenGrid = new xq_TetGenGrid();
    tetGenGrid->SetVolumeMesh(tetMesh);

    auto mitkGrid = xq_MitkGrid::New();
    mitkGrid->SetMesh(tetGenGrid, 0);

    auto meshNode = mitk::DataNode::New();
    meshNode->SetName("mesh_without_source_metadata");
    meshNode->SetData(mitkGrid);
    xq::pipeline::MarkNode(meshNode, xq::pipeline::Stage::VolumeMesh);
    ds->Add(meshNode);

    auto issues = CheckNode(ds.GetPointer(), meshNode.GetPointer());

    bool foundAmbiguousSource = false;
    for (const auto& issue : issues)
    {
        if (issue.message.find("Ambiguous source model source") != std::string::npos)
        {
            foundAmbiguousSource = true;
            break;
        }
    }

    if (!foundAmbiguousSource)
    {
        std::cerr << "FAIL: Ambiguous missing source was not refused\n";
        return 1;
    }

    std::cout << "PASS test_pipeline_consistency_refuses_ambiguous_missing_source\n";
    return 0;
}

static int test_pipeline_resolver_metadata_source_link_and_unique_fallback()
{
    std::cout << "--- test_pipeline_resolver_metadata_source_link_and_unique_fallback ---\n";

    {
        mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

        auto metadataModel = mitk::DataNode::New();
        metadataModel->SetName("metadata_model");
        xq::pipeline::MarkNode(metadataModel, xq::pipeline::Stage::Model);
        ds->Add(metadataModel);

        auto linkedModel = mitk::DataNode::New();
        linkedModel->SetName("linked_model");
        xq::pipeline::MarkNode(linkedModel, xq::pipeline::Stage::Model);
        ds->Add(linkedModel);

        auto meshNode = mitk::DataNode::New();
        meshNode->SetName("mesh_prefers_metadata");
        xq::pipeline::MarkNode(meshNode, xq::pipeline::Stage::VolumeMesh);
        xq::pipeline::SetStringProperty(
            meshNode, xq::pipeline::kSourceModelProperty, "metadata_model");
        ds->Add(meshNode, linkedModel);

        auto resolved = xq::pipeline::ResolveUpstreamNode(
            ds.GetPointer(),
            meshNode.GetPointer(),
            xq::pipeline::kSourceModelProperty,
            xq::pipeline::Stage::Model);
        if (resolved != metadataModel)
        {
            std::cerr << "FAIL: resolver did not prefer explicit metadata\n";
            return 1;
        }
    }

    {
        mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

        auto linkedModel = mitk::DataNode::New();
        linkedModel->SetName("linked_model");
        xq::pipeline::MarkNode(linkedModel, xq::pipeline::Stage::Model);
        ds->Add(linkedModel);

        auto meshNode = mitk::DataNode::New();
        meshNode->SetName("mesh_uses_source_link");
        xq::pipeline::MarkNode(meshNode, xq::pipeline::Stage::VolumeMesh);
        ds->Add(meshNode, linkedModel);

        auto resolved = xq::pipeline::ResolveUpstreamNode(
            ds.GetPointer(),
            meshNode.GetPointer(),
            xq::pipeline::kSourceModelProperty,
            xq::pipeline::Stage::Model);
        if (resolved != linkedModel)
        {
            std::cerr << "FAIL: resolver did not use direct DataStorage source link\n";
            return 1;
        }
    }

    {
        mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

        auto spherePD = MakeSpherePolyData();
        auto* polyGeom = new xq_PolyGeometry();
        polyGeom->SetWholeVtkPolyData(spherePD);
        auto modelData = xq_Model::New();
        modelData->SetModelElement(polyGeom, 0);

        auto legacyModel = mitk::DataNode::New();
        legacyModel->SetName("legacy_unmarked_model");
        legacyModel->SetData(modelData);
        ds->Add(legacyModel);

        auto meshNode = mitk::DataNode::New();
        meshNode->SetName("mesh_uses_legacy_typed_source_link");
        xq::pipeline::MarkNode(meshNode, xq::pipeline::Stage::VolumeMesh);
        ds->Add(meshNode, legacyModel);

        auto resolved = xq::pipeline::ResolveUpstreamNode(
            ds.GetPointer(),
            meshNode.GetPointer(),
            xq::pipeline::kSourceModelProperty,
            xq::pipeline::Stage::Model);
        if (resolved != legacyModel)
        {
            std::cerr << "FAIL: resolver did not accept typed legacy source link\n";
            return 1;
        }
    }

    {
        mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

        auto onlyModel = mitk::DataNode::New();
        onlyModel->SetName("only_model");
        xq::pipeline::MarkNode(onlyModel, xq::pipeline::Stage::Model);
        ds->Add(onlyModel);

        auto meshNode = mitk::DataNode::New();
        meshNode->SetName("mesh_uses_unique_fallback");
        xq::pipeline::MarkNode(meshNode, xq::pipeline::Stage::VolumeMesh);
        ds->Add(meshNode);

        auto resolved = xq::pipeline::ResolveUpstreamNode(
            ds.GetPointer(),
            meshNode.GetPointer(),
            xq::pipeline::kSourceModelProperty,
            xq::pipeline::Stage::Model);
        if (resolved != onlyModel)
        {
            std::cerr << "FAIL: resolver did not use unique upstream fallback\n";
            return 1;
        }
    }

    {
        mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

        for (int i = 0; i < 2; ++i)
        {
            auto modelNode = mitk::DataNode::New();
            modelNode->SetName(i == 0 ? "model_a" : "model_b");
            xq::pipeline::MarkNode(modelNode, xq::pipeline::Stage::Model);
            ds->Add(modelNode);
        }

        auto meshNode = mitk::DataNode::New();
        meshNode->SetName("mesh_refuses_ambiguous_fallback");
        xq::pipeline::MarkNode(meshNode, xq::pipeline::Stage::VolumeMesh);
        ds->Add(meshNode);

        auto resolved = xq::pipeline::ResolveUpstreamNode(
            ds.GetPointer(),
            meshNode.GetPointer(),
            xq::pipeline::kSourceModelProperty,
            xq::pipeline::Stage::Model);
        if (resolved.IsNotNull())
        {
            std::cerr << "FAIL: resolver guessed an ambiguous upstream model\n";
            return 1;
        }
    }

    std::cout << "PASS test_pipeline_resolver_metadata_source_link_and_unique_fallback\n";
    return 0;
}

static int test_pipeline_resolver_source_list()
{
    std::cout << "--- test_pipeline_resolver_source_list ---\n";

    mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

    auto firstGroup = mitk::DataNode::New();
    firstGroup->SetName("profiles_a");
    firstGroup->SetData(xq_ProfileGroup::New());
    xq::pipeline::MarkNode(firstGroup, xq::pipeline::Stage::ContourGroup);
    ds->Add(firstGroup);

    auto secondGroup = mitk::DataNode::New();
    secondGroup->SetName("profiles_b");
    secondGroup->SetData(xq_ProfileGroup::New());
    xq::pipeline::MarkNode(secondGroup, xq::pipeline::Stage::ContourGroup);
    ds->Add(secondGroup);

    auto modelNode = mitk::DataNode::New();
    modelNode->SetName("model_with_two_sources");
    xq::pipeline::MarkNode(modelNode, xq::pipeline::Stage::Model);
    xq::pipeline::SetStringProperty(
        modelNode,
        xq::pipeline::kSourceContourGroupsProperty,
        "profiles_a;profiles_b");
    ds->Add(modelNode);

    const auto resolved = xq::pipeline::ResolveUpstreamNodes(
        ds.GetPointer(),
        modelNode.GetPointer(),
        xq::pipeline::kSourceContourGroupsProperty,
        xq::pipeline::Stage::ContourGroup);
    if (resolved.size() != 2 ||
        resolved[0]->GetName() != "profiles_a" ||
        resolved[1]->GetName() != "profiles_b")
    {
        std::cerr << "FAIL: resolver did not preserve source list order\n";
        return 1;
    }

    auto partialModelNode = mitk::DataNode::New();
    partialModelNode->SetName("model_with_missing_source");
    xq::pipeline::MarkNode(partialModelNode, xq::pipeline::Stage::Model);
    xq::pipeline::SetStringProperty(
        partialModelNode,
        xq::pipeline::kSourceContourGroupsProperty,
        "profiles_a;missing_profiles");
    ds->Add(partialModelNode);

    const auto partialResolved = xq::pipeline::ResolveUpstreamNodes(
        ds.GetPointer(),
        partialModelNode.GetPointer(),
        xq::pipeline::kSourceContourGroupsProperty,
        xq::pipeline::Stage::ContourGroup);
    if (partialResolved.size() != 1 ||
        partialResolved[0]->GetName() != "profiles_a")
    {
        std::cerr << "FAIL: resolver should keep valid entries and skip missing source names\n";
        return 1;
    }

    std::cout << "PASS test_pipeline_resolver_source_list\n";
    return 0;
}

static int test_get_nodes_by_stage_accepts_legacy_data_classes()
{
    std::cout << "--- test_get_nodes_by_stage_accepts_legacy_data_classes ---\n";

    mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();

    auto pathNode = mitk::DataNode::New();
    pathNode->SetName("legacy_path");
    pathNode->SetData(xq_VesselCenterline::New());
    ds->Add(pathNode);

    auto contourNode = mitk::DataNode::New();
    contourNode->SetName("legacy_contours");
    contourNode->SetData(xq_ProfileGroup::New());
    ds->Add(contourNode);

    auto contourAltNode = mitk::DataNode::New();
    contourAltNode->SetName("legacy_contour_group");
    contourAltNode->SetData(xq_ContourGroup::New());
    ds->Add(contourAltNode);

    auto modelNode = mitk::DataNode::New();
    modelNode->SetName("legacy_model");
    modelNode->SetData(xq_Model::New());
    ds->Add(modelNode);

    auto meshNode = mitk::DataNode::New();
    meshNode->SetName("legacy_mesh");
    meshNode->SetData(xq_MitkGrid::New());
    ds->Add(meshNode);

    auto resultGridNode = mitk::DataNode::New();
    resultGridNode->SetName("legacy_result_grid");
    resultGridNode->SetData(xq_MitkGrid::New());
    resultGridNode->SetStringProperty("xq.type", "result");
    resultGridNode->SetStringProperty("xq.result.field_name", "point:pressure");
    ds->Add(resultGridNode);

    auto plainImageNode = mitk::DataNode::New();
    plainImageNode->SetName("plain_image_not_result");
    plainImageNode->SetData(MakeSmallMitkImage());
    ds->Add(plainImageNode);

    auto simNode = mitk::DataNode::New();
    simNode->SetName("legacy_sim");
    simNode->SetData(xq_MitkSolverJob::New());
    ds->Add(simNode);

    auto romNode = mitk::DataNode::New();
    romNode->SetName("legacy_rom");
    romNode->SetData(xq_MitkROMJob::New());
    ds->Add(romNode);

    auto mpNode = mitk::DataNode::New();
    mpNode->SetName("legacy_multiphysics");
    mpNode->SetData(xq_MitkMultiPhysicsJob::New());
    ds->Add(mpNode);

    const std::vector<std::pair<xq::pipeline::Stage, std::vector<mitk::DataNode::Pointer>>> expected = {
        {xq::pipeline::Stage::Path, {pathNode}},
        {xq::pipeline::Stage::ContourGroup, {contourNode, contourAltNode}},
        {xq::pipeline::Stage::Model, {modelNode}},
        {xq::pipeline::Stage::VolumeMesh, {meshNode}},
        {xq::pipeline::Stage::SimulationPrep, {simNode}},
        {xq::pipeline::Stage::ROMSimulation, {romNode}},
        {xq::pipeline::Stage::MultiPhysics, {mpNode}},
        {xq::pipeline::Stage::Result, {resultGridNode}}
    };

    for (const auto& [stage, expectedNodes] : expected)
    {
        const auto nodes = xq::pipeline::GetNodesByStage(ds, stage);
        if (nodes.size() != expectedNodes.size())
        {
            std::cerr << "FAIL: GetNodesByStage did not return the expected legacy node for stage "
                      << xq::pipeline::ToStageName(stage) << "\n";
            return 1;
        }
        for (const auto& expectedNode : expectedNodes)
        {
            if (std::find(nodes.begin(), nodes.end(), expectedNode) == nodes.end())
            {
                std::cerr << "FAIL: GetNodesByStage missed an expected legacy node for stage "
                          << xq::pipeline::ToStageName(stage) << "\n";
                return 1;
            }
        }
    }

    std::cout << "PASS test_get_nodes_by_stage_accepts_legacy_data_classes\n";
    return 0;
}

static int test_centerline_extract_path_contract()
{
    std::cout << "--- test_centerline_extract_path_contract ---\n";

    mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();
    auto projectNode = mitk::DataNode::New();
    projectNode->SetName("CenterlineProject");
    projectNode->SetStringProperty("project.name", "CenterlineProject");
    ds->Add(projectNode);
    auto pathFolder = CreateFolderNode(ds, projectNode, "PathFolder");

    auto centerline = xq_VesselCenterline::New();
    auto* segment = new xq_CenterlineSegment();
    segment->ReplaceAnchors(MakeTestAnchors());
    centerline->SetSegment(segment);

    auto centerlineNode = mitk::DataNode::New();
    centerlineNode->SetName("model_centerline");
    centerlineNode->SetData(centerline);
    xq::pipeline::MarkNode(centerlineNode, xq::pipeline::Stage::Centerline);
    xq::pipeline::SetStringProperty(
        centerlineNode, xq::pipeline::kSourceImageProperty, "image0");
    xq::pipeline::SetStringProperty(
        centerlineNode, xq::pipeline::kSourceModelProperty, "model0");
    ds->Add(centerlineNode, projectNode);

    xq_PathExtractRequest request;
    request.centerlineNodeName = "model_centerline";
    request.pathName = "model_centerline_path";
    auto result = xq_PathPipelineService::ExtractPathFromCenterline(ds, request);
    if (!result.ok || result.node.IsNull())
    {
        std::cerr << "FAIL test_centerline_extract_path_contract: extraction failed\n";
        return 1;
    }
    if (!xq::pipeline::HasStage(result.node, xq::pipeline::Stage::Path))
    {
        std::cerr << "FAIL test_centerline_extract_path_contract: output is not path stage\n";
        return 1;
    }
    if (!dynamic_cast<xq_VesselCenterline*>(result.node->GetData()))
    {
        std::cerr << "FAIL test_centerline_extract_path_contract: output is not xq_VesselCenterline\n";
        return 1;
    }
    const auto sourceCenterline = xq::pipeline::GetStringProperty(
        result.node.GetPointer(), xq::pipeline::kSourceCenterlineProperty);
    const auto sourceImage = xq::pipeline::GetStringProperty(
        result.node.GetPointer(), xq::pipeline::kSourceImageProperty);
    const auto sourceModel = xq::pipeline::GetStringProperty(
        result.node.GetPointer(), xq::pipeline::kSourceModelProperty);
    const auto algorithm = xq::pipeline::GetStringProperty(
        result.node.GetPointer(), xq::pipeline::kAlgorithmProperty);
    const auto requestedAlgorithm = xq::pipeline::GetStringProperty(
        result.node.GetPointer(), "xq.pathplanning.algorithm.requested");
    const auto actualAlgorithm = xq::pipeline::GetStringProperty(
        result.node.GetPointer(), "xq.pathplanning.algorithm.actual");
    if (sourceCenterline != "model_centerline" ||
        sourceImage != "image0" ||
        sourceModel != "model0" ||
        algorithm != "extract_paths" ||
        requestedAlgorithm != "extract_paths" ||
        actualAlgorithm != "extract_paths")
    {
        std::cerr << "FAIL test_centerline_extract_path_contract: source metadata missing\n";
        return 1;
    }

    auto derivations = ds->GetDerivations(pathFolder);
    if (!derivations || derivations->Size() == 0)
    {
        std::cerr << "FAIL test_centerline_extract_path_contract: output was not placed under Paths\n";
        return 1;
    }

    std::cout << "PASS test_centerline_extract_path_contract\n";
    return 0;
}

static int test_rom_multiphysics_project_contract()
{
    std::cout << "--- test_rom_multiphysics_project_contract ---\n";

    RemoveDir(kTestProjectPath);
    const std::string projectDir = SetupTestProject();
    if (projectDir.empty())
    {
        std::cerr << "FAIL test_rom_multiphysics_project_contract: setup failed\n";
        return 1;
    }

    auto saveNode = [&](mitk::DataNode::Pointer node,
                        const std::string& subdir) -> std::string
    {
        const std::string savePath = JoinPath(projectDir, subdir + "/" + node->GetName());
        auto writeResult = xq_ProjectDataWriter::SaveNodeData(node, savePath, subdir);
        if (!writeResult.ok)
        {
            std::cerr << "FAIL test_rom_multiphysics_project_contract: SaveNodeData failed for "
                      << node->GetName() << ": "
                      << (writeResult.diagnostics.empty() ? "unknown" : writeResult.diagnostics[0])
                      << "\n";
            return "";
        }
        xq_NodeMetadataIO::WriteNodeMetadata(node, writeResult.savedPath + ".xqmeta.xml");
        return writeResult.savedPath;
    };

    auto romJob = std::make_unique<xq_ROMJob>();
    romJob->SetJobName("test_rom");
    romJob->SetModelType("1D");
    romJob->SetRCR("outlet_1", 120.0, 0.002, 80.0);
    romJob->SetTimeStepSize(0.002);
    romJob->SetNumTimeSteps(400);
    romJob->SetSolverTolerance(1.0e-7);
    romJob->SetMaxSolverIterations(50);
    romJob->SetOutputFormat("csv");
    romJob->AddOutputField("pressure");
    romJob->AddOutputField("flow");
    romJob->SetProperty("centerline", "test_path");
    romJob->SetProperty("solver", "blocked_without_runtime");

    auto romData = xq_MitkROMJob::New();
    romData->SetStatus("created");
    romData->SetROMJob(std::move(romJob), 0);

    auto romNode = mitk::DataNode::New();
    romNode->SetName("test_rom");
    romNode->SetData(romData);
    xq::pipeline::MarkNode(romNode, xq::pipeline::Stage::ROMSimulation);
    xq::pipeline::SetStringProperty(romNode, xq::pipeline::kSourceModelProperty, "test_model");
    xq::pipeline::SetStringProperty(romNode, xq::pipeline::kSourceMeshProperty, "test_mesh");
    xq::pipeline::SetStringProperty(romNode, xq::pipeline::kSourcePathProperty, "test_path");
    xq::pipeline::SetStringProperty(romNode, xq::pipeline::kAlgorithmProperty, "rom_contract");

    const std::string romPath = saveNode(romNode, "ROMSimulations");
    if (romPath.empty()) { RemoveDir(kTestProjectPath); return 1; }

    xq_MultiPhysicsDomain domain;
    domain.name = "fluid_domain";
    domain.type = xq_MultiPhysicsDomainType::Fluid;
    domain.properties["mesh_name"] = "test_mesh";
    domain.material.density = 1.06;
    domain.material.viscosity = 0.04;

    xq_MultiPhysicsEquation equation;
    equation.name = "fluid_eq";
    equation.type = xq_MultiPhysicsEquationType::Fluid;
    equation.domainNames.push_back("fluid_domain");
    equation.solverSettings.linearSolver = "GMRES";
    equation.solverSettings.tolerance = 1.0e-5;

    xq_MultiPhysicsBoundaryCondition bc;
    bc.domainName = "fluid_domain";
    bc.faceName = "inlet";
    bc.bcType = xq_MultiPhysicsBCType::Dirichlet;
    bc.parameters["value"] = "1.0";
    bc.parameters["expression"] = "a=b=c";

    auto mpJob = std::make_unique<xq_MultiPhysicsJob>();
    mpJob->SetJobName("test_multiphysics");
    mpJob->SetTimeStepSize(0.001);
    mpJob->SetNumTimeSteps(100);
    mpJob->AddDomain(domain);
    mpJob->AddEquation(equation);
    mpJob->AddBoundaryCondition(bc);
    mpJob->SetProperty("solver_paths", "blocked_without_runtime");

    auto mpData = xq_MitkMultiPhysicsJob::New();
    mpData->SetStatus("created");
    mpData->SetJob(std::move(mpJob), 0);

    auto mpNode = mitk::DataNode::New();
    mpNode->SetName("test_multiphysics");
    mpNode->SetData(mpData);
    xq::pipeline::MarkNode(mpNode, xq::pipeline::Stage::MultiPhysics);
    xq::pipeline::SetStringProperty(mpNode, xq::pipeline::kSourceMeshProperty, "test_mesh");
    xq::pipeline::SetStringProperty(mpNode, xq::pipeline::kSourceModelProperty, "test_model");
    xq::pipeline::SetStringProperty(mpNode, xq::pipeline::kAlgorithmProperty, "multiphysics_contract");

    const std::string mpPath = saveNode(mpNode, "MultiPhysics");
    if (mpPath.empty()) { RemoveDir(kTestProjectPath); return 1; }

    auto loadNode = [&](const std::string& path) -> mitk::DataNode::Pointer
    {
        auto readResult = xq_ProjectDataReader::LoadNodeData(path);
        if (!readResult.ok || readResult.nodes.empty())
            return nullptr;
        auto node = readResult.nodes[0];
        xq_NodeMetadataIO::ReadNodeMetadata(node, path + ".xqmeta.xml");
        return node;
    };

    auto reopenedRom = loadNode(romPath);
    if (reopenedRom.IsNull())
    {
        std::cerr << "FAIL test_rom_multiphysics_project_contract: ROM reload failed\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    auto* reopenedRomData = dynamic_cast<xq_MitkROMJob*>(reopenedRom->GetData());
    auto* reopenedRomJob = reopenedRomData ? reopenedRomData->GetROMJob(0) : nullptr;
    if (!reopenedRomJob || reopenedRomJob->GetJobName() != "test_rom" ||
        reopenedRomJob->GetModelType() != "1D" ||
        reopenedRomJob->GetOutputFormat() != "csv" ||
        reopenedRomJob->GetOutputFields().size() != 2)
    {
        std::cerr << "FAIL test_rom_multiphysics_project_contract: ROM fields did not roundtrip\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    if (!xq::pipeline::HasStage(reopenedRom, xq::pipeline::Stage::ROMSimulation) ||
        xq::pipeline::GetStringProperty(reopenedRom.GetPointer(), xq::pipeline::kSourcePathProperty) != "test_path")
    {
        std::cerr << "FAIL test_rom_multiphysics_project_contract: ROM metadata missing\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    auto reopenedMp = loadNode(mpPath);
    if (reopenedMp.IsNull())
    {
        std::cerr << "FAIL test_rom_multiphysics_project_contract: MultiPhysics reload failed\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    auto* reopenedMpData = dynamic_cast<xq_MitkMultiPhysicsJob*>(reopenedMp->GetData());
    auto* reopenedMpJob = reopenedMpData ? reopenedMpData->GetJob(0) : nullptr;
    if (!reopenedMpJob || reopenedMpJob->GetJobName() != "test_multiphysics" ||
        reopenedMpJob->GetDomains().size() != 1 ||
        reopenedMpJob->GetEquations().size() != 1 ||
        reopenedMpJob->GetBoundaryConditions().size() != 1)
    {
        std::cerr << "FAIL test_rom_multiphysics_project_contract: MultiPhysics fields did not roundtrip\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    const auto& reopenedBc = reopenedMpJob->GetBoundaryConditions().front();
    auto exprIt = reopenedBc.parameters.find("expression");
    if (exprIt == reopenedBc.parameters.end() || exprIt->second != "a=b=c")
    {
        std::cerr << "FAIL test_rom_multiphysics_project_contract: MultiPhysics BC parameter did not roundtrip\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    if (!xq::pipeline::HasStage(reopenedMp, xq::pipeline::Stage::MultiPhysics) ||
        xq::pipeline::GetStringProperty(reopenedMp.GetPointer(), xq::pipeline::kSourceModelProperty) != "test_model" ||
        xq::pipeline::GetStringProperty(reopenedMp.GetPointer(), xq::pipeline::kSourceMeshProperty) != "test_mesh")
    {
        std::cerr << "FAIL test_rom_multiphysics_project_contract: MultiPhysics metadata missing\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    xq_MultiPhysicsJob mismatchedJob;
    mismatchedJob.SetJobName("mismatched");
    mismatchedJob.SetTimeStepSize(0.001);
    mismatchedJob.SetNumTimeSteps(10);
    xq_MultiPhysicsDomain solidDomain;
    solidDomain.name = "solid_domain";
    solidDomain.type = xq_MultiPhysicsDomainType::Solid;
    solidDomain.material.density = 1.0;
    solidDomain.material.elasticModulus = 2.0e6;
    solidDomain.material.poissonRatio = 0.45;
    xq_MultiPhysicsEquation fluidEquation;
    fluidEquation.name = "fluid_eq";
    fluidEquation.type = xq_MultiPhysicsEquationType::Fluid;
    fluidEquation.domainNames.push_back("solid_domain");
    fluidEquation.solverSettings.linearSolver = "GMRES";
    fluidEquation.solverSettings.tolerance = 1.0e-5;
    fluidEquation.solverSettings.maxIterations = 25;
    mismatchedJob.AddDomain(solidDomain);
    mismatchedJob.AddEquation(fluidEquation);
    const auto mismatchValidation = mismatchedJob.Validate();
    if (mismatchValidation.empty())
    {
        std::cerr << "FAIL test_rom_multiphysics_project_contract: mismatched MultiPhysics job was accepted\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    RemoveDir(kTestProjectPath);
    std::cout << "PASS test_rom_multiphysics_project_contract\n";
    return 0;
}

static int test_image_processing_project_contract()
{
    std::cout << "--- test_image_processing_project_contract ---\n";

    RemoveDir(kTestProjectPath);
    const std::string projectDir = SetupTestProject();
    if (projectDir.empty())
    {
        std::cerr << "FAIL test_image_processing_project_contract: setup failed\n";
        return 1;
    }

    auto node = mitk::DataNode::New();
    node->SetName("aorta_threshold");
    node->SetData(MakeSmallMitkImage());
    xq::pipeline::MarkGeneratedNode(
        node,
        xq::pipeline::Stage::ImageProcessing,
        "threshold",
        "org.xq.views.imageprocessing",
        "1");
    node->SetStringProperty("xq.type", "image_processing_result");
    node->SetStringProperty("xq.source.image", "aorta");
    node->SetStringProperty("xq.image.processing.operation", "threshold");
    node->SetStringProperty("xq.params.image_processing.operation", "threshold");
    node->SetStringProperty("xq.units.length", "mm");
    node->SetStringProperty("xq.image.processing.source_node", "aorta");
    node->SetStringProperty("xq.image.processing.parameters",
                            "lower=3;upper=5;inside=1;outside=0");
    node->SetStringProperty("xq.image.processing.seed_points", "0,0,0");
    node->SetBoolProperty("xq.image.processing.output_is_image", true);
    node->SetDoubleProperty("xq.image.processing.threshold.min", 3.0);
    node->SetDoubleProperty("xq.image.processing.threshold.max", 5.0);

    const std::string savePath = JoinPath(projectDir, "Images/aorta_threshold");
    auto writeResult = xq_ProjectDataWriter::SaveNodeData(node, savePath, "Images");
    if (!writeResult.ok)
    {
        std::cerr << "FAIL test_image_processing_project_contract: SaveNodeData failed: "
                  << (writeResult.diagnostics.empty() ? "unknown" : writeResult.diagnostics[0])
                  << "\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    if (!EndsWithString(writeResult.savedPath, ".nrrd") ||
        !FileExists(writeResult.savedPath))
    {
        std::cerr << "FAIL test_image_processing_project_contract: image result was not saved as .nrrd\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    xq_NodeMetadataIO::WriteNodeMetadata(node, writeResult.savedPath + ".xqmeta.xml");

    auto readResult = xq_ProjectDataReader::LoadNodeData(writeResult.savedPath);
    if (!readResult.ok || readResult.nodes.empty())
    {
        std::cerr << "FAIL test_image_processing_project_contract: LoadNodeData failed\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    auto reopenedNode = readResult.nodes.front();
    xq_NodeMetadataIO::ReadNodeMetadata(reopenedNode, writeResult.savedPath + ".xqmeta.xml");
    if (!dynamic_cast<mitk::Image*>(reopenedNode->GetData()))
    {
        std::cerr << "FAIL test_image_processing_project_contract: reloaded data is not mitk::Image\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    std::string stage;
    std::string sourceImage;
    std::string operation;
    std::string paramsOperation;
    std::string unitsLength;
    std::string algorithmVersion;
    std::string createdByTool;
    std::string createdAt;
    bool outputIsImage = false;
    bool valid = false;
    double lower = 0.0;
    double upper = 0.0;
    if (!reopenedNode->GetStringProperty("xq.pipeline.stage", stage) ||
        stage != "image_processing" ||
        !reopenedNode->GetStringProperty("xq.pipeline.algorithm_version", algorithmVersion) ||
        algorithmVersion != "1" ||
        !reopenedNode->GetStringProperty("xq.pipeline.created_by_tool", createdByTool) ||
        createdByTool != "org.xq.views.imageprocessing" ||
        !reopenedNode->GetStringProperty("xq.pipeline.created_at", createdAt) ||
        createdAt.empty() ||
        !reopenedNode->GetBoolProperty("xq.pipeline.valid", valid) ||
        !valid ||
        !reopenedNode->GetStringProperty("xq.source.image", sourceImage) ||
        sourceImage != "aorta" ||
        !reopenedNode->GetStringProperty("xq.image.processing.operation", operation) ||
        operation != "threshold" ||
        !reopenedNode->GetStringProperty("xq.params.image_processing.operation", paramsOperation) ||
        paramsOperation != "threshold" ||
        !reopenedNode->GetStringProperty("xq.units.length", unitsLength) ||
        unitsLength != "mm" ||
        !reopenedNode->GetBoolProperty("xq.image.processing.output_is_image", outputIsImage) ||
        !outputIsImage ||
        !reopenedNode->GetDoubleProperty("xq.image.processing.threshold.min", lower) ||
        std::abs(lower - 3.0) > 1e-9 ||
        !reopenedNode->GetDoubleProperty("xq.image.processing.threshold.max", upper) ||
        std::abs(upper - 5.0) > 1e-9)
    {
        std::cerr << "FAIL test_image_processing_project_contract: metadata did not roundtrip\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    RemoveDir(kTestProjectPath);
    std::cout << "PASS test_image_processing_project_contract\n";
    return 0;
}

static int test_python_api_cpp_contract()
{
    std::cout << "--- test_python_api_cpp_contract ---\n";

    mitk::DataStorage::Pointer ds = mitk::StandaloneDataStorage::New();
    auto projectNode = mitk::DataNode::New();
    projectNode->SetName("PythonApiProject");
    ds->Add(projectNode);
    CreateFolderNode(ds, projectNode, "PathFolder");
    CreateFolderNode(ds, projectNode, "SegmentationFolder");
    CreateFolderNode(ds, projectNode, "ModelFolder");
    CreateFolderNode(ds, projectNode, "MeshFolder");
    CreateFolderNode(ds, projectNode, "SimulationFolder");
    CreateFolderNode(ds, projectNode, "ROMSimulationFolder");
    CreateFolderNode(ds, projectNode, "MultiPhysicsFolder");

    xq_PythonApiService service(ds.GetPointer());
    if (service.IsAvailable())
    {
        std::cerr << "FAIL test_python_api_cpp_contract: Python runtime unexpectedly available\n";
        return 1;
    }

    auto addPath = service.AddPath("py_path", "image0");
    auto addSeg = service.AddSegmentation("py_seg", "py_path", "image0");
    auto addModel = service.AddModel("py_model", "py_seg", "py_path");
    auto addMesh = service.AddMesh("py_mesh", "py_model");
    auto addSim = service.AddSimulation("py_sim", "py_mesh", "py_model");
    auto addRom = service.AddROMSimulation("py_rom", "py_mesh", "py_model", "py_path");
    auto addMp = service.AddMultiPhysics("py_mp", "py_mesh", "py_model");
    if (!addPath.ok || !addSeg.ok || !addModel.ok || !addMesh.ok || !addSim.ok ||
        !addRom.ok || !addMp.ok)
    {
        std::cerr << "FAIL test_python_api_cpp_contract: add_* failed\n";
        return 1;
    }

    if (!service.ReadPath("py_path").ok ||
        !service.ReadSegmentation("py_seg").ok ||
        !service.ReadModel("py_model").ok ||
        !service.ReadMesh("py_mesh").ok ||
        !service.ReadSimulation("py_sim").ok ||
        !service.ReadROMSimulation("py_rom").ok ||
        !service.ReadMultiPhysics("py_mp").ok)
    {
        std::cerr << "FAIL test_python_api_cpp_contract: read_* failed\n";
        return 1;
    }

    auto upstream = service.ResolveUpstream("py_mesh", "model");
    if (!upstream.ok || upstream.value != "py_model")
    {
        std::cerr << "FAIL test_python_api_cpp_contract: upstream model resolution failed\n";
        return 1;
    }

    auto romPath = service.ResolveUpstream("py_rom", "path");
    auto mpModel = service.ResolveUpstream("py_mp", "model");
    if (!romPath.ok || romPath.value != "py_path" ||
        !mpModel.ok || mpModel.value != "py_model")
    {
        std::cerr << "FAIL test_python_api_cpp_contract: ROM/MultiPhysics upstream resolution failed\n";
        return 1;
    }

    auto metadataOnly = mitk::DataNode::New();
    metadataOnly->SetName("py_metadata_only");
    metadataOnly->SetBoolProperty("xq.python.metadata_only", true);
    const std::filesystem::path metaPath =
        std::filesystem::temp_directory_path() / "xq_python_metadata_only.xqmeta";
    if (!xq_NodeMetadataIO::WriteNodeMetadata(metadataOnly, metaPath.string()))
    {
        std::cerr << "FAIL test_python_api_cpp_contract: metadata-only write failed\n";
        return 1;
    }
    auto restoredMetadataOnly = mitk::DataNode::New();
    if (!xq_NodeMetadataIO::ReadNodeMetadata(restoredMetadataOnly, metaPath.string()))
    {
        std::cerr << "FAIL test_python_api_cpp_contract: metadata-only read failed\n";
        return 1;
    }
    bool metadataOnlyFlag = false;
    if (!restoredMetadataOnly->GetBoolProperty("xq.python.metadata_only", metadataOnlyFlag) ||
        !metadataOnlyFlag)
    {
        std::cerr << "FAIL test_python_api_cpp_contract: metadata-only flag did not roundtrip\n";
        return 1;
    }

    auto duplicate = service.AddPath("py_path", "image0");
    if (duplicate.ok)
    {
        std::cerr << "FAIL test_python_api_cpp_contract: duplicate add was accepted\n";
        return 1;
    }

    if (service.ProjectOpen("/tmp/nope").ok || service.ProjectSave("/tmp/nope").ok)
    {
        std::cerr << "FAIL test_python_api_cpp_contract: project skeleton reported success\n";
        return 1;
    }

    std::cout << "PASS test_python_api_cpp_contract\n";
    return 0;
}

static int test_seg3d_project_contract()
{
    std::cout << "--- test_seg3d_project_contract ---\n";

    RemoveDir(kTestProjectPath);
    const std::string projectDir = SetupTestProject();
    if (projectDir.empty())
    {
        std::cerr << "FAIL test_seg3d_project_contract: setup failed\n";
        return 1;
    }

    vtkNew<vtkSphereSource> sphere;
    sphere->SetRadius(2.0);
    sphere->SetThetaResolution(8);
    sphere->SetPhiResolution(8);
    sphere->Update();

    auto seg3d = xq_MitkSeg3D::New();
    vtkSmartPointer<vtkPolyData> surface = vtkSmartPointer<vtkPolyData>::New();
    surface->DeepCopy(sphere->GetOutput());
    seg3d->SetSurfaceMesh(surface);
    seg3d->SetMethod(xq_MitkSeg3D::Seg3DMethod::REGION_GROWING);
    seg3d->SetLowerThreshold(42.0);
    seg3d->SetUpperThreshold(420.0);
    mitk::Point3D seed;
    seed[0] = 1.0;
    seed[1] = 2.0;
    seed[2] = 3.0;
    seg3d->AddSeedPoint(seed);

    auto node = mitk::DataNode::New();
    node->SetName("test_seg3d");
    node->SetData(seg3d);
    node->SetStringProperty("xq.pipeline.stage", "segmentation_3d");
    node->SetStringProperty("xq.pipeline.algorithm", "region_growing");
    node->SetStringProperty("xq.source.image", "test_image");
    node->SetStringProperty("xq.source.segmentation", "test_segmentation");
    node->SetStringProperty("xq.segmentation.seed_points", "1,2,3");
    node->SetBoolProperty("xq.segmentation.3d", true);
    node->SetIntProperty("xq.segmentation.active_tool_id", 2);
    node->SetDoubleProperty("xq.segmentation.region_growing.tolerance", 7.5);
    node->SetDoubleProperty("xq.segmentation.threshold.min", 42.0);
    node->SetDoubleProperty("xq.segmentation.threshold.max", 420.0);

    const std::string savePath = JoinPath(projectDir, "Segmentations/test_seg3d");
    auto writeResult = xq_ProjectDataWriter::SaveNodeData(node, savePath, "Segmentations");
    if (!writeResult.ok)
    {
        std::cerr << "FAIL test_seg3d_project_contract: SaveNodeData failed: "
                  << (writeResult.diagnostics.empty() ? "unknown" : writeResult.diagnostics[0])
                  << "\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }
    xq_NodeMetadataIO::WriteNodeMetadata(node, writeResult.savedPath + ".xqmeta.xml");

    auto readResult = xq_ProjectDataReader::LoadNodeData(writeResult.savedPath);
    if (!readResult.ok || readResult.nodes.empty())
    {
        std::cerr << "FAIL test_seg3d_project_contract: LoadNodeData failed\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    auto reopenedNode = readResult.nodes.front();
    xq_NodeMetadataIO::ReadNodeMetadata(reopenedNode, writeResult.savedPath + ".xqmeta.xml");
    auto* reopenedSeg3d = dynamic_cast<xq_MitkSeg3D*>(reopenedNode->GetData());
    if (!reopenedSeg3d)
    {
        std::cerr << "FAIL test_seg3d_project_contract: reloaded data is not xq_MitkSeg3D\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    const auto reopenedSeeds = reopenedSeg3d->GetSeedPoints();
    if (reopenedSeg3d->GetMethod() != xq_MitkSeg3D::Seg3DMethod::REGION_GROWING ||
        reopenedSeg3d->GetLowerThreshold() != 42.0 ||
        reopenedSeg3d->GetUpperThreshold() != 420.0 ||
        reopenedSeeds.size() != 1 ||
        !reopenedSeg3d->GetSurfaceMesh() ||
        reopenedSeg3d->GetSurfaceMesh()->GetNumberOfPoints() == 0)
    {
        std::cerr << "FAIL test_seg3d_project_contract: segmentation state did not roundtrip\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    std::string stage;
    std::string sourceImage;
    std::string sourceSegmentation;
    std::string seedPoints;
    int activeToolId = -1;
    double tolerance = 0.0;
    double minThreshold = 0.0;
    double maxThreshold = 0.0;
    if (!reopenedNode->GetStringProperty("xq.pipeline.stage", stage) ||
        stage != "segmentation_3d" ||
        !reopenedNode->GetStringProperty("xq.source.image", sourceImage) ||
        sourceImage != "test_image" ||
        !reopenedNode->GetStringProperty("xq.source.segmentation", sourceSegmentation) ||
        sourceSegmentation != "test_segmentation" ||
        !reopenedNode->GetStringProperty("xq.segmentation.seed_points", seedPoints) ||
        seedPoints != "1,2,3" ||
        !reopenedNode->GetIntProperty("xq.segmentation.active_tool_id", activeToolId) ||
        activeToolId != 2 ||
        !reopenedNode->GetDoubleProperty("xq.segmentation.region_growing.tolerance", tolerance) ||
        tolerance != 7.5 ||
        !reopenedNode->GetDoubleProperty("xq.segmentation.threshold.min", minThreshold) ||
        minThreshold != 42.0 ||
        !reopenedNode->GetDoubleProperty("xq.segmentation.threshold.max", maxThreshold) ||
        maxThreshold != 420.0)
    {
        std::cerr << "FAIL test_seg3d_project_contract: metadata did not roundtrip\n";
        RemoveDir(kTestProjectPath);
        return 1;
    }

    RemoveDir(kTestProjectPath);
    std::cout << "PASS test_seg3d_project_contract\n";
    return 0;
}

static int test_tool_view_routing_contract()
{
    std::cout << "--- test_tool_view_routing_contract ---\n";

    auto expectView = [](const mitk::DataNode::Pointer& node,
                         const char* expectedView,
                         const char* label) -> int {
        const QString actual = xq::pipeline::ResolveToolViewIdForNode(node);
        if (actual != QString::fromLatin1(expectedView))
        {
            std::cerr << "FAIL test_tool_view_routing_contract: " << label
                      << " expected " << expectedView
                      << ", got " << actual.toStdString() << "\n";
            return 1;
        }
        return 0;
    };

    int failures = 0;

    auto imageNode = mitk::DataNode::New();
    imageNode->SetName("image0");
    imageNode->SetData(MakeSmallMitkImage());
    failures += expectView(
        imageNode, "org.xq.views.imageprocessing", "mitk::Image");

    auto imageProcessingNode = mitk::DataNode::New();
    imageProcessingNode->SetName("image_processing0");
    xq::pipeline::MarkGeneratedNode(
        imageProcessingNode,
        xq::pipeline::Stage::ImageProcessing,
        "connected_threshold",
        "org.xq.views.imageprocessing",
        "1");
    failures += expectView(
        imageProcessingNode, "org.xq.views.imageprocessing", "image_processing stage");

    std::string generatedAlgorithm;
    std::string generatedAlgorithmVersion;
    std::string generatedTool;
    std::string generatedCreatedAt;
    bool generatedValid = false;
    if (!xq::pipeline::HasStage(imageProcessingNode, xq::pipeline::Stage::ImageProcessing) ||
        !imageProcessingNode->GetStringProperty(
            xq::pipeline::kAlgorithmProperty, generatedAlgorithm) ||
        generatedAlgorithm != "connected_threshold" ||
        !imageProcessingNode->GetStringProperty(
            xq::pipeline::kAlgorithmVersionProperty, generatedAlgorithmVersion) ||
        generatedAlgorithmVersion != "1" ||
        !imageProcessingNode->GetStringProperty(
            xq::pipeline::kCreatedByToolProperty, generatedTool) ||
        generatedTool != "org.xq.views.imageprocessing" ||
        !imageProcessingNode->GetBoolProperty(
            xq::pipeline::kValidProperty, generatedValid) ||
        !generatedValid ||
        !imageProcessingNode->GetStringProperty(
            xq::pipeline::kCreatedAtProperty, generatedCreatedAt) || generatedCreatedAt.empty())
    {
        std::cerr << "FAIL test_tool_view_routing_contract: generated metadata contract missing\n";
        ++failures;
    }

    auto imageStageNode = mitk::DataNode::New();
    imageStageNode->SetName("image_stage0");
    xq::pipeline::MarkNode(imageStageNode, xq::pipeline::Stage::Image);
    failures += expectView(
        imageStageNode, "org.xq.views.imageprocessing", "image stage");

    auto pathNode = mitk::DataNode::New();
    pathNode->SetName("path0");
    xq::pipeline::MarkNode(pathNode, xq::pipeline::Stage::Path);
    failures += expectView(
        pathNode, "org.xq.views.pathplanning", "path stage");

    auto legacyPathNode = mitk::DataNode::New();
    legacyPathNode->SetName("legacy_path");
    legacyPathNode->SetBoolProperty("xq.pathplanning.path", true);
    failures += expectView(
        legacyPathNode, "org.xq.views.pathplanning", "legacy path property");

    auto contourNode = mitk::DataNode::New();
    contourNode->SetName("contour0");
    contourNode->SetData(MakeTestContourGroup());
    failures += expectView(
        contourNode, "org.xq.views.segmentation", "contour data");

    auto seg3dNode = mitk::DataNode::New();
    seg3dNode->SetName("seg3d0");
    xq::pipeline::MarkNode(seg3dNode, xq::pipeline::Stage::Segmentation3D);
    failures += expectView(
        seg3dNode, "org.xq.views.mitksegmentation", "segmentation_3d stage");

    auto modelNode = mitk::DataNode::New();
    modelNode->SetName("model0");
    xq::pipeline::MarkNode(modelNode, xq::pipeline::Stage::Model);
    failures += expectView(
        modelNode, "org.xq.views.modeling", "model stage");

    auto meshNode = mitk::DataNode::New();
    meshNode->SetName("mesh0");
    xq::pipeline::MarkNode(meshNode, xq::pipeline::Stage::VolumeMesh);
    failures += expectView(
        meshNode, "org.xq.views.meshing", "volume_mesh stage");

    auto simNode = mitk::DataNode::New();
    simNode->SetName("simulation0");
    xq::pipeline::MarkNode(simNode, xq::pipeline::Stage::SimulationPrep);
    failures += expectView(
        simNode, "org.xq.views.simulation", "simulation_prep stage");

    auto romNode = mitk::DataNode::New();
    romNode->SetName("rom0");
    xq::pipeline::MarkNode(romNode, xq::pipeline::Stage::ROMSimulation);
    failures += expectView(
        romNode, "org.xq.views.romsimulation", "rom_simulation stage");

    auto mpNode = mitk::DataNode::New();
    mpNode->SetName("multiphysics0");
    xq::pipeline::MarkNode(mpNode, xq::pipeline::Stage::MultiPhysics);
    failures += expectView(
        mpNode, "org.xq.views.multiphysics", "multiphysics stage");

    auto resultNode = mitk::DataNode::New();
    resultNode->SetName("result0");
    xq::pipeline::MarkNode(resultNode, xq::pipeline::Stage::Result);
    failures += expectView(
        resultNode, "org.xq.views.simulation", "result stage");

    auto unknownNode = mitk::DataNode::New();
    unknownNode->SetName("unknown0");
    const QString unknownView = xq::pipeline::ResolveToolViewIdForNode(unknownNode);
    if (!unknownView.isEmpty())
    {
        std::cerr << "FAIL test_tool_view_routing_contract: unknown node returned "
                  << unknownView.toStdString() << "\n";
        ++failures;
    }

    if (failures != 0)
        return 1;

    std::cout << "PASS test_tool_view_routing_contract\n";
    return 0;
}

static int test_required_parameter_metadata_roundtrip()
{
    std::cout << "--- test_required_parameter_metadata_roundtrip ---\n";

    const std::string metaPath = "/tmp/xq_required_parameter_metadata.xqmeta.xml";
    auto node = mitk::DataNode::New();
    node->SetName("metadata_contract_node");
    node->SetStringProperty("xq.params.modeling.model_type", "lofted_surface");
    node->SetStringProperty("xq.params.modeling.loft_method", "native_loft");
    node->SetBoolProperty("xq.params.modeling.qa.ok", true);
    node->SetIntProperty("xq.params.modeling.face_count", 5);
    node->SetIntProperty("xq.params.modeling.cap_count", 2);
    node->SetStringProperty("xq.params.meshing.backend", "native_tetgen");
    node->SetDoubleProperty("xq.params.meshing.global_size", 0.75);
    node->SetStringProperty("xq.params.meshing.command_history", "generate;quality");
    node->SetBoolProperty("xq.params.meshing.quality.ok", true);
    node->SetDoubleProperty("xq.params.meshing.quality.min_volume", 0.125);
    node->SetStringProperty("xq.params.solver.backend_id", "xq.simple_flow");
    node->SetStringProperty("xq.params.solver.command_line", "xq-simple-flow --case case0");
    node->SetIntProperty("xq.params.solver.exit_code", 0);
    node->SetBoolProperty("xq.params.solver.supports_result_import", true);
    node->SetStringProperty("xq.params.results.field_name", "pressure");
    node->SetStringProperty("xq.params.results.units", "Pa");
    node->SetIntProperty("xq.params.results.time_step", 4);
    node->SetDoubleProperty("xq.params.results.time_value", 0.04);
    node->SetStringProperty("xq.result.units", "Pa");

    if (!xq_NodeMetadataIO::WriteNodeMetadata(node, metaPath))
    {
        std::cerr << "FAIL test_required_parameter_metadata_roundtrip: write failed\n";
        return 1;
    }

    auto restored = mitk::DataNode::New();
    if (!xq_NodeMetadataIO::ReadNodeMetadata(restored, metaPath))
    {
        std::cerr << "FAIL test_required_parameter_metadata_roundtrip: read failed\n";
        return 1;
    }

    std::string modelingType;
    std::string loftMethod;
    std::string meshBackend;
    std::string meshHistory;
    std::string solverBackend;
    std::string solverCommand;
    std::string resultField;
    std::string resultUnits;
    std::string genericResultUnits;
    bool modelingQaOk = false;
    bool meshQaOk = false;
    bool supportsResultImport = false;
    int faceCount = -1;
    int capCount = -1;
    int solverExitCode = -1;
    int resultTimeStep = -1;
    double meshSize = 0.0;
    double minVolume = 0.0;
    double resultTimeValue = 0.0;

    if (!restored->GetStringProperty("xq.params.modeling.model_type", modelingType) ||
        modelingType != "lofted_surface" ||
        !restored->GetStringProperty("xq.params.modeling.loft_method", loftMethod) ||
        loftMethod != "native_loft" ||
        !restored->GetBoolProperty("xq.params.modeling.qa.ok", modelingQaOk) ||
        !modelingQaOk ||
        !restored->GetIntProperty("xq.params.modeling.face_count", faceCount) ||
        faceCount != 5 ||
        !restored->GetIntProperty("xq.params.modeling.cap_count", capCount) ||
        capCount != 2 ||
        !restored->GetStringProperty("xq.params.meshing.backend", meshBackend) ||
        meshBackend != "native_tetgen" ||
        !restored->GetDoubleProperty("xq.params.meshing.global_size", meshSize) ||
        meshSize != 0.75 ||
        !restored->GetStringProperty("xq.params.meshing.command_history", meshHistory) ||
        meshHistory != "generate;quality" ||
        !restored->GetBoolProperty("xq.params.meshing.quality.ok", meshQaOk) ||
        !meshQaOk ||
        !restored->GetDoubleProperty("xq.params.meshing.quality.min_volume", minVolume) ||
        minVolume != 0.125 ||
        !restored->GetStringProperty("xq.params.solver.backend_id", solverBackend) ||
        solverBackend != "xq.simple_flow" ||
        !restored->GetStringProperty("xq.params.solver.command_line", solverCommand) ||
        solverCommand != "xq-simple-flow --case case0" ||
        !restored->GetIntProperty("xq.params.solver.exit_code", solverExitCode) ||
        solverExitCode != 0 ||
        !restored->GetBoolProperty(
            "xq.params.solver.supports_result_import", supportsResultImport) ||
        !supportsResultImport ||
        !restored->GetStringProperty("xq.params.results.field_name", resultField) ||
        resultField != "pressure" ||
        !restored->GetStringProperty("xq.params.results.units", resultUnits) ||
        resultUnits != "Pa" ||
        !restored->GetIntProperty("xq.params.results.time_step", resultTimeStep) ||
        resultTimeStep != 4 ||
        !restored->GetDoubleProperty("xq.params.results.time_value", resultTimeValue) ||
        resultTimeValue != 0.04 ||
        !restored->GetStringProperty("xq.result.units", genericResultUnits) ||
        genericResultUnits != "Pa")
    {
        std::cerr << "FAIL test_required_parameter_metadata_roundtrip: required metadata lost\n";
        std::remove(metaPath.c_str());
        return 1;
    }

    std::remove(metaPath.c_str());
    std::cout << "PASS test_required_parameter_metadata_roundtrip\n";
    return 0;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);

    // Ensure MITK core is initialized (required for DataStorage operations)
    mitk::CoreObjectFactory::GetInstance();

    std::cout << "=== project_roundtrip regression tests ===\n";

    int failures = 0;
    if (test_project_empty_roundtrip() != 0)                              ++failures;
    if (test_project_save_load_sources() != 0)                            ++failures;
    if (test_project_roundtrip_custom_xq_types() != 0)                   ++failures;
    if (test_project_full_openproject_roundtrip_custom_xq_types() != 0)  ++failures;
    if (test_pipeline_consistency_detects_missing_sources() != 0)        ++failures;
    if (test_pipeline_consistency_detects_wrong_folder() != 0)           ++failures;
    if (test_pipeline_consistency_detects_qa_failure() != 0)             ++failures;
    if (test_pipeline_consistency_detects_type_mismatch() != 0)          ++failures;
    if (test_pipeline_consistency_refuses_ambiguous_missing_source() != 0) ++failures;
    if (test_pipeline_resolver_metadata_source_link_and_unique_fallback() != 0) ++failures;
    if (test_pipeline_resolver_source_list() != 0)                       ++failures;
    if (test_get_nodes_by_stage_accepts_legacy_data_classes() != 0)      ++failures;
    if (test_centerline_extract_path_contract() != 0)                    ++failures;
    if (test_rom_multiphysics_project_contract() != 0)                   ++failures;
    if (test_image_processing_project_contract() != 0)                   ++failures;
    if (test_python_api_cpp_contract() != 0)                              ++failures;
    if (test_seg3d_project_contract() != 0)                               ++failures;
    if (test_tool_view_routing_contract() != 0)                           ++failures;
    if (test_required_parameter_metadata_roundtrip() != 0)                 ++failures;

    if (failures == 0)
        std::cout << "All tests PASSED.\n";
    else
        std::cerr << failures << " test(s) FAILED.\n";

    return (failures == 0) ? 0 : 1;
}
