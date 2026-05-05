#include "xq_LegacyImporter.h"

#include "xq_DataFolder.h"
#include "xq_ImageFolder.h"
#include "xq_PathFolder.h"
#include "xq_SegmentationFolder.h"
#include "xq_ModelFolder.h"
#include "xq_MeshFolder.h"
#include "xq_SimulationFolder.h"
#include "xq_ROMSimulationFolder.h"
#include "xq_MultiPhysicsFolder.h"
#include "xq_RepositoryFolder.h"

#include <xq_PipelineDataUtils.h>
#include <xq_VesselCenterline.h>
#include <xq_CenterlineSegment.h>
#include <xq_ProfileGroup.h>
#include <xq_PolygonalProfile.h>
#include <xq_SegmentationUtils.h>
#include <xq_Math3.h>

#include <mitkIOUtil.h>
#include <mitkPointSet.h>
#include <mitkProperties.h>
#include <mitkSurface.h>

#include <tinyxml2.h>

#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <fstream>
#include <sstream>

// ---------------------------------------------------------------------------
// Helper: check if file/dir exists
// ---------------------------------------------------------------------------
static bool FileExists(const std::string& path)
{
    struct stat info;
    return (stat(path.c_str(), &info) == 0) && !(info.st_mode & S_IFDIR);
}

static bool DirExists(const std::string& path)
{
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
        return false;
    return (info.st_mode & S_IFDIR) != 0;
}

static std::string GetFileNameOnly(const std::string& path)
{
    auto pos = path.find_last_of("/\\");
    std::string name = (pos == std::string::npos) ? path : path.substr(pos + 1);
    auto dot = name.rfind('.');
    if (dot == std::string::npos)
        return name;
    return name.substr(0, dot);
}

static std::string GetExtension(const std::string& path)
{
    auto dot = path.rfind('.');
    if (dot == std::string::npos)
        return "";
    std::string ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

static mitk::Point3D ParsePoint3D(tinyxml2::XMLElement* elem)
{
    mitk::Point3D point;
    point.Fill(0.0);
    if (!elem)
        return point;

    elem->QueryDoubleAttribute("x", &point[0]);
    elem->QueryDoubleAttribute("y", &point[1]);
    elem->QueryDoubleAttribute("z", &point[2]);
    return point;
}

static mitk::Vector3D ParseVector3D(tinyxml2::XMLElement* elem)
{
    mitk::Vector3D vec;
    vec.Fill(0.0);
    if (!elem)
        return vec;

    elem->QueryDoubleAttribute("x", &vec[0]);
    elem->QueryDoubleAttribute("y", &vec[1]);
    elem->QueryDoubleAttribute("z", &vec[2]);
    return vec;
}

static std::string NormalizeImportedPathName(const std::string& value)
{
    if (value.size() > 6 && value.rfind("_final") == value.size() - 6)
        return value.substr(0, value.size() - 6);
    return value;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

xq_LegacyImporter::xq_LegacyImporter()
    : m_ImagesFolderName("Images")
    , m_PathsFolderName("Paths")
    , m_SegmentationsFolderName("Segmentations")
    , m_ModelsFolderName("Models")
    , m_MeshesFolderName("Meshes")
    , m_SimulationsFolderName("Simulations")
    , m_FlowFolderName("flow-files")
{
}

xq_LegacyImporter::~xq_LegacyImporter()
{
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

bool xq_LegacyImporter::ImportProject(mitk::DataStorage::Pointer dataStorage,
                                          const std::string& svProjDir)
{
    if (dataStorage.IsNull())
    {
        Log("[Error] DataStorage is null.");
        return false;
    }

    if (!DirExists(svProjDir))
    {
        Log("[Error] Project directory does not exist: " + svProjDir);
        return false;
    }

    // Try to find and parse .svproj file
    std::string svprojPath = svProjDir + "/.svproj";
    if (!FileExists(svprojPath))
    {
        Log("[Warning] No .svproj file found. Attempting directory-based import.");
    }
    else
    {
        if (!ParseSVProjectFile(svprojPath))
        {
            Log("[Warning] Failed to parse .svproj, falling back to directory scan.");
        }
    }

    // Derive project name from directory
    m_ProjectName = GetFileNameOnly(svProjDir);
    if (m_ProjectName.empty())
        m_ProjectName = "ImportedLegacyProject";

    Log("[Info] Importing compatible project: " + m_ProjectName);
    Log("[Info] Source directory: " + svProjDir);

    // Create project root node
    mitk::DataNode::Pointer projectNode = mitk::DataNode::New();
    projectNode->SetName(m_ProjectName);
    projectNode->SetProperty("project.path", mitk::StringProperty::New(svProjDir));
    projectNode->SetProperty("project.version", mitk::StringProperty::New("1.0"));
    projectNode->SetProperty("project.source", mitk::StringProperty::New("LegacyVascularProject"));
    dataStorage->Add(projectNode);

    // Create folder nodes and import data
    auto addFolder = [&](auto folderData, const std::string& nodeName)
        -> mitk::DataNode::Pointer
    {
        mitk::DataNode::Pointer node = mitk::DataNode::New();
        node->SetName(nodeName);
        node->SetData(folderData);
        node->SetVisibility(false);
        dataStorage->Add(node, projectNode);
        return node;
    };

    auto imgFolderNode  = addFolder(xq_ImageFolder::New().GetPointer(),        "Images");
    auto pathFolderNode = addFolder(xq_PathFolder::New().GetPointer(),         "Paths");
    auto segFolderNode  = addFolder(xq_SegmentationFolder::New().GetPointer(), "Segmentations");
    auto mdlFolderNode  = addFolder(xq_ModelFolder::New().GetPointer(),        "Models");
    auto mshFolderNode  = addFolder(xq_GridFolder::New().GetPointer(),        "Meshes");
    auto simFolderNode  = addFolder(xq_SimulationFolder::New().GetPointer(),   "Simulations");
    addFolder(xq_ROMSimulationFolder::New().GetPointer(), "ROMSimulations");
    addFolder(xq_MultiPhysicsFolder::New().GetPointer(),  "MultiPhysics");
    addFolder(xq_RepositoryFolder::New().GetPointer(),    "Repository");

    // Import each category
    ImportImages(dataStorage, imgFolderNode, svProjDir);
    ImportPaths(dataStorage, pathFolderNode, svProjDir);
    ImportSegmentations(dataStorage, segFolderNode, svProjDir);
    ImportModels(dataStorage, mdlFolderNode, svProjDir);
    ImportMeshes(dataStorage, mshFolderNode, svProjDir);
    ImportSimulations(dataStorage, simFolderNode, svProjDir);

    Log("[Info] Import complete. " + std::to_string(m_ImportLog.size()) + " log entries.");
    return true;
}

std::vector<std::string> xq_LegacyImporter::GetImportLog() const
{
    return m_ImportLog;
}

std::string xq_LegacyImporter::GetProjectName() const
{
    return m_ProjectName;
}

// ---------------------------------------------------------------------------
// Parse .svproj XML
// ---------------------------------------------------------------------------

bool xq_LegacyImporter::ParseSVProjectFile(const std::string& svprojPath)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(svprojPath.c_str()) != tinyxml2::XML_SUCCESS)
    {
        Log("[Error] Failed to parse XML: " + svprojPath);
        return false;
    }

    tinyxml2::XMLElement* root = doc.FirstChildElement("projectDescription");
    if (!root)
    {
        Log("[Error] No <projectDescription> root element in .svproj");
        return false;
    }

    // Parse images section
    tinyxml2::XMLElement* imagesElem = root->FirstChildElement("images");
    if (imagesElem)
    {
        const char* folderName = imagesElem->Attribute("folder_name");
        if (folderName)
            m_ImagesFolderName = folderName;

        m_ImageEntries.clear();
        for (tinyxml2::XMLElement* imgElem = imagesElem->FirstChildElement("image");
             imgElem;
             imgElem = imgElem->NextSiblingElement("image"))
        {
            SVImageEntry entry;
            const char* name = imgElem->Attribute("name");
            const char* path = imgElem->Attribute("path");
            const char* inProj = imgElem->Attribute("in_project");

            entry.name = name ? name : "";
            entry.path = path ? path : "";
            entry.inProject = (inProj && std::string(inProj) == "yes");

            if (!entry.name.empty() || !entry.path.empty())
                m_ImageEntries.push_back(entry);
        }
    }

    // Parse other folder names
    auto readFolderName = [&](const char* tag, std::string& target)
    {
        tinyxml2::XMLElement* elem = root->FirstChildElement(tag);
        if (elem)
        {
            const char* fn = elem->Attribute("folder_name");
            if (fn)
                target = fn;
        }
    };

    readFolderName("paths",         m_PathsFolderName);
    readFolderName("segmentations", m_SegmentationsFolderName);
    readFolderName("models",        m_ModelsFolderName);
    readFolderName("meshes",        m_MeshesFolderName);
    readFolderName("simulations",   m_SimulationsFolderName);
    readFolderName("flow",          m_FlowFolderName);

    Log("[Info] Parsed .svproj: Images=" + m_ImagesFolderName +
        ", Paths=" + m_PathsFolderName +
        ", Models=" + m_ModelsFolderName);

    return true;
}

// ---------------------------------------------------------------------------
// Import: Images
// ---------------------------------------------------------------------------

void xq_LegacyImporter::ImportImages(mitk::DataStorage::Pointer ds,
                                         mitk::DataNode::Pointer parentNode,
                                         const std::string& projDir)
{
    // First try from parsed .svproj entries
    for (const auto& entry : m_ImageEntries)
    {
        std::string imgPath;
        if (entry.inProject)
        {
            if (!entry.path.empty())
                imgPath = projDir + "/" + m_ImagesFolderName + "/" + entry.path;
            else if (!entry.name.empty())
                imgPath = projDir + "/" + m_ImagesFolderName + "/" + entry.name + ".vti";
        }
        else
        {
            imgPath = entry.path;
        }

        if (!imgPath.empty() && FileExists(imgPath))
        {
            mitk::DataNode::Pointer node = LoadVTKFile(imgPath);
            if (node.IsNotNull())
            {
                if (!entry.name.empty())
                    node->SetName(entry.name);
                ds->Add(node, parentNode);
                Log("[OK] Loaded image: " + entry.name + " from " + imgPath);
            }
            else
            {
                Log("[Warning] Failed to load image: " + imgPath);
            }
        }
    }

    // Also scan directory for any VTK image files not listed in .svproj
    std::string imgDir = projDir + "/" + m_ImagesFolderName;
    if (DirExists(imgDir))
    {
        auto files = ScanDirectory(imgDir, {".vti", ".nii", ".nii.gz", ".nrrd", ".mhd"});
        for (const auto& f : files)
        {
            // Skip if already loaded via .svproj entries
            bool alreadyLoaded = false;
            for (const auto& entry : m_ImageEntries)
            {
                if (f.find(entry.path) != std::string::npos ||
                    f.find(entry.name) != std::string::npos)
                {
                    alreadyLoaded = true;
                    break;
                }
            }
            if (alreadyLoaded)
                continue;

            mitk::DataNode::Pointer node = LoadVTKFile(f);
            if (node.IsNotNull())
            {
                ds->Add(node, parentNode);
                Log("[OK] Loaded additional image: " + f);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Import: Paths (.pth XML → PointSet)
// ---------------------------------------------------------------------------

void xq_LegacyImporter::ImportPaths(mitk::DataStorage::Pointer ds,
                                        mitk::DataNode::Pointer parentNode,
                                        const std::string& projDir)
{
    std::string pathDir = projDir + "/" + m_PathsFolderName;
    if (!DirExists(pathDir))
    {
        Log("[Info] No Paths directory found.");
        return;
    }

    auto files = ScanDirectory(pathDir, {".pth"});
    for (const auto& f : files)
    {
        mitk::DataNode::Pointer node = ParsePathFile(f);
        if (node.IsNotNull())
        {
            ds->Add(node, parentNode);
            Log("[OK] Imported path: " + GetFileNameOnly(f));
        }
        else
        {
            Log("[Warning] Failed to parse path file: " + f);
        }
    }

    // Also load any VTK files in Paths directory
    auto vtkFiles = ScanDirectory(pathDir, {".vtp", ".vtk"});
    for (const auto& f : vtkFiles)
    {
        mitk::DataNode::Pointer node = LoadVTKFile(f);
        if (node.IsNotNull())
        {
            xq::pipeline::MarkNode(node, xq::pipeline::Stage::Path);
            // XQ UX: Paths are hidden by default; user toggles on when needed.
            node->SetVisibility(false);
            ds->Add(node, parentNode);
            Log("[OK] Loaded path geometry: " + GetFileNameOnly(f));
        }
    }
}

// ---------------------------------------------------------------------------
// Import: Segmentations (.ctgr XML → PointSet)
// ---------------------------------------------------------------------------

void xq_LegacyImporter::ImportSegmentations(mitk::DataStorage::Pointer ds,
                                                mitk::DataNode::Pointer parentNode,
                                                const std::string& projDir)
{
    std::string segDir = projDir + "/" + m_SegmentationsFolderName;
    if (!DirExists(segDir))
    {
        Log("[Info] No Segmentations directory found.");
        return;
    }

    auto ctgrFiles = ScanDirectory(segDir, {".ctgr"});
    for (const auto& f : ctgrFiles)
    {
        mitk::DataNode::Pointer node = ParseContourGroupFile(f);
        if (node.IsNotNull())
        {
            ds->Add(node, parentNode);
            Log("[OK] Imported contour group: " + GetFileNameOnly(f));
        }
        else
        {
            Log("[Warning] Failed to parse contour group: " + f);
        }
    }

    // Also load any VTK/surface files
    auto surfFiles = ScanDirectory(segDir, {".vtp", ".vtk", ".stl"});
    for (const auto& f : surfFiles)
    {
        mitk::DataNode::Pointer node = LoadVTKFile(f);
        if (node.IsNotNull())
        {
            xq::pipeline::MarkNode(node, xq::pipeline::Stage::ContourGroup);
            // XQ UX: Segmentations are hidden by default; user toggles on when needed.
            node->SetVisibility(false);
            ds->Add(node, parentNode);
            Log("[OK] Loaded segmentation surface: " + GetFileNameOnly(f));
        }
    }
}

// ---------------------------------------------------------------------------
// Import: Models (.mdl metadata + .vtp geometry)
// ---------------------------------------------------------------------------

void xq_LegacyImporter::ImportModels(mitk::DataStorage::Pointer ds,
                                         mitk::DataNode::Pointer parentNode,
                                         const std::string& projDir)
{
    std::string modelDir = projDir + "/" + m_ModelsFolderName;
    if (!DirExists(modelDir))
    {
        Log("[Info] No Models directory found.");
        return;
    }

    // Parse .mdl files for metadata, load companion .vtp for geometry
    auto mdlFiles = ScanDirectory(modelDir, {".mdl"});
    for (const auto& f : mdlFiles)
    {
        mitk::DataNode::Pointer node = ParseModelFile(f, projDir);
        if (node.IsNotNull())
        {
            xq::pipeline::MarkNode(node, xq::pipeline::Stage::Model);
            node->SetVisibility(true);
            ds->Add(node, parentNode);
            Log("[OK] Imported model: " + GetFileNameOnly(f));
        }
    }

    // Load any standalone VTK surface files
    auto vtpFiles = ScanDirectory(modelDir, {".vtp", ".vtk", ".stl"});
    for (const auto& f : vtpFiles)
    {
        // Skip .vtp files that were already loaded via .mdl companion
        bool alreadyLoaded = false;
        for (const auto& mdl : mdlFiles)
        {
            std::string baseName = GetFileNameOnly(mdl);
            if (GetFileNameOnly(f) == baseName)
            {
                alreadyLoaded = true;
                break;
            }
        }

        if (!alreadyLoaded)
        {
            mitk::DataNode::Pointer node = LoadVTKFile(f);
            if (node.IsNotNull())
            {
                xq::pipeline::MarkNode(node, xq::pipeline::Stage::Model);
                node->SetVisibility(true);
                ds->Add(node, parentNode);
                Log("[OK] Loaded model surface: " + GetFileNameOnly(f));
            }
        }
        // .vtp with same basename as .mdl already loaded via ParseModelFile
    }
}

// ---------------------------------------------------------------------------
// Import: Meshes (.msh + .vtp/.vtu)
// ---------------------------------------------------------------------------

void xq_LegacyImporter::ImportMeshes(mitk::DataStorage::Pointer ds,
                                         mitk::DataNode::Pointer parentNode,
                                         const std::string& projDir)
{
    std::string meshDir = projDir + "/" + m_MeshesFolderName;
    if (!DirExists(meshDir))
    {
        Log("[Info] No Meshes directory found.");
        return;
    }

    // Load VTK mesh files (.vtp = surface mesh)
    // NOTE: .vtu (UnstructuredGrid) files are excluded because MITK's
    // UnstructuredGridMapper2D crashes in GenerateDataForRenderer().
    auto files = ScanDirectory(meshDir, {".vtp", ".vtk", ".stl"});
    for (const auto& f : files)
    {
        mitk::DataNode::Pointer node = LoadVTKFile(f);
        if (node.IsNotNull())
        {
            xq::pipeline::MarkNode(node, xq::pipeline::Stage::VolumeMesh);
            node->SetVisibility(true);
            node->SetColor(0.0f, 0.8f, 0.2f);
            node->SetProperty("opacity", mitk::FloatProperty::New(0.6f));
            ds->Add(node, parentNode);
            Log("[OK] Loaded mesh: " + GetFileNameOnly(f));
        }
        else
        {
            Log("[Warning] Failed to load mesh: " + f);
        }
    }

    // Log skipped .vtu files
    auto vtuFiles = ScanDirectory(meshDir, {".vtu"});
    for (const auto& f : vtuFiles)
    {
        Log("[Info] Skipped .vtu volume mesh (UnstructuredGrid not supported): "
            + GetFileNameOnly(f));
    }

    // Note: .msh files are SV-specific binary format, log their presence
    auto mshFiles = ScanDirectory(meshDir, {".msh"});
    for (const auto& f : mshFiles)
    {
        Log("[Info] Found SV mesh config (.msh): " + GetFileNameOnly(f)
            + " - metadata only, geometry loaded from VTP/VTU.");
    }
}

// ---------------------------------------------------------------------------
// Import: Simulations (.sjb + solver input files)
// ---------------------------------------------------------------------------

void xq_LegacyImporter::ImportSimulations(mitk::DataStorage::Pointer ds,
                                              mitk::DataNode::Pointer parentNode,
                                              const std::string& projDir)
{
    std::string simDir = projDir + "/" + m_SimulationsFolderName;
    if (!DirExists(simDir))
    {
        Log("[Info] No Simulations directory found.");
        return;
    }

    // Load .sjb job files as reference nodes
    auto sjbFiles = ScanDirectory(simDir, {".sjb"});
    for (const auto& f : sjbFiles)
    {
        std::string jobName = GetFileNameOnly(f);
        mitk::DataNode::Pointer jobNode = mitk::DataNode::New();
        jobNode->SetName(jobName);
        jobNode->SetProperty("sv.simulation.job", mitk::StringProperty::New(f));
        jobNode->SetProperty("sv.import.type", mitk::StringProperty::New("SimulationJob"));
        xq::pipeline::MarkNode(jobNode, xq::pipeline::Stage::SimulationPrep);
        jobNode->SetVisibility(true);
        ds->Add(jobNode, parentNode);
        Log("[OK] Imported simulation job reference: " + jobName);

        // Note: mesh-complete subdirectories are NOT loaded by default
        // to avoid redundant data and improve performance.
        // Use File→Open Data to load specific simulation results on demand.
        std::string jobDir = simDir + "/" + jobName;
        if (DirExists(jobDir))
        {
            std::string meshCompleteDir = jobDir + "/mesh-complete";
            if (DirExists(meshCompleteDir))
            {
                Log("[Info] Simulation mesh-complete directory found: " + meshCompleteDir
                    + " — skipped for performance (load on demand).");
            }
        }
    }

    // Note: standalone VTP/VTU results in Simulations/ are skipped
    // to avoid loading redundant data. Use File→Open Data for specific results.
}

// ---------------------------------------------------------------------------
// SV-specific file parsers
// ---------------------------------------------------------------------------

mitk::DataNode::Pointer xq_LegacyImporter::ParsePathFile(const std::string& filePath)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filePath.c_str()) != tinyxml2::XML_SUCCESS)
        return nullptr;

    // Look for <path> element
    tinyxml2::XMLElement* pathElem = doc.FirstChildElement("path");
    if (!pathElem)
        return nullptr;

    tinyxml2::XMLElement* tsElem = pathElem->FirstChildElement("timestep");
    if (!tsElem)
        return nullptr;

    tinyxml2::XMLElement* pathElement = tsElem->FirstChildElement("path_element");
    if (!pathElement)
        return nullptr;

    tinyxml2::XMLElement* ctrlPtsElem = pathElement->FirstChildElement("control_points");
    if (!ctrlPtsElem)
        return nullptr;

    std::vector<mitk::Point3D> controlPoints;
    for (tinyxml2::XMLElement* pt = ctrlPtsElem->FirstChildElement("point");
         pt;
         pt = pt->NextSiblingElement("point"))
    {
        controlPoints.push_back(ParsePoint3D(pt));
    }

    if (controlPoints.empty())
        return nullptr;

    std::vector<xq_CenterlineSegment::TraceVertex> traceVertices;
    if (auto* pathPointsElem = pathElement->FirstChildElement("path_points"))
    {
        for (tinyxml2::XMLElement* ppElem = pathPointsElem->FirstChildElement("path_point");
             ppElem;
             ppElem = ppElem->NextSiblingElement("path_point"))
        {
            xq_CenterlineSegment::TraceVertex vertex;
            ppElem->QueryIntAttribute("id", &vertex.id);

            auto* posElem = ppElem->FirstChildElement("pos");
            auto* tangentElem = ppElem->FirstChildElement("tangent");
            auto* rotationElem = ppElem->FirstChildElement("rotation");

            vertex.pos = ParsePoint3D(posElem);
            vertex.tangent = xq_Math3::UnitVector(ParseVector3D(tangentElem));
            vertex.rotation = xq_Math3::UnitVector(ParseVector3D(rotationElem));
            vertex.normal = xq_Math3::UnitVector(
                xq_Math3::CrossProduct3D(vertex.tangent, vertex.rotation));
            traceVertices.push_back(vertex);
        }
    }

    auto pathData = xq_VesselCenterline::New();
    auto* segment = new xq_CenterlineSegment();
    segment->ReplaceAnchors(controlPoints, false);
    if (!traceVertices.empty())
        segment->SetTraceVertices(traceVertices);
    else
        segment->Interpolate();
    pathData->SetSegment(segment);

    int pathId = 0;
    pathElem->QueryIntAttribute("id", &pathId);
    if (pathId != 0)
    {
        pathData->SetAttribute("sv.path_id", std::to_string(pathId));
        m_PathIdToName[pathId] = GetFileNameOnly(filePath);
    }

    mitk::DataNode::Pointer node = mitk::DataNode::New();
    node->SetData(pathData);
    node->SetName(GetFileNameOnly(filePath));
    node->SetBoolProperty("xq.pathplanning.path", true);
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::Path);
    // XQ UX: Paths are hidden by default at import; user toggles on when needed.
    node->SetVisibility(false);
    node->SetProperty("sv.type", mitk::StringProperty::New("Path"));
    node->SetProperty("sv.path.file", mitk::StringProperty::New(filePath));
    node->SetProperty("sv.path.id", mitk::IntProperty::New(pathId));

    // Reduce control-point visual noise on imported paths; the actual path
    // curve should be the primary visual, not edit glyphs.
    node->SetColor(0.0f, 1.0f, 0.0f);
    node->SetProperty("point size", mitk::FloatProperty::New(0.35f));
    node->SetProperty("tube radius", mitk::FloatProperty::New(0.15f));
    node->SetProperty("line width", mitk::FloatProperty::New(2.0f));
    node->SetProperty("path.show.control.points", mitk::BoolProperty::New(true));

    return node;
}

mitk::DataNode::Pointer xq_LegacyImporter::ParseContourGroupFile(
    const std::string& filePath)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filePath.c_str()) != tinyxml2::XML_SUCCESS)
        return nullptr;

    tinyxml2::XMLElement* cgElem = doc.FirstChildElement("contourgroup");
    if (!cgElem)
        return nullptr;

    auto profileGroup = xq_ProfileGroup::New();
    profileGroup->SetAttribute("sv.contourgroup.file", filePath);

    int pathId = 0;
    cgElem->QueryIntAttribute("path_id", &pathId);
    if (pathId != 0)
        profileGroup->SetTrajectoryID(pathId);

    const char* pathNameAttr = cgElem->Attribute("path_name");
    std::string pathName = pathNameAttr ? pathNameAttr : "";
    pathName = NormalizeImportedPathName(pathName);
    if (pathName.empty() && pathId != 0)
    {
        const auto idIt = m_PathIdToName.find(pathId);
        if (idIt != m_PathIdToName.end())
            pathName = idIt->second;
    }
    if (pathName.empty())
        pathName = NormalizeImportedPathName(GetFileNameOnly(filePath));
    profileGroup->SetAttribute("path_name", pathName);

    if (const char* resliceSize = cgElem->Attribute("reslice_size"))
        profileGroup->SetAttribute("reslice_size", resliceSize);

    tinyxml2::XMLElement* tsElem = cgElem->FirstChildElement("timestep");
    if (!tsElem)
        return nullptr;

    for (tinyxml2::XMLElement* contourElem = tsElem->FirstChildElement("contour");
         contourElem;
         contourElem = contourElem->NextSiblingElement("contour"))
    {
        auto profile = std::make_unique<xq_PolygonalProfile>();
        profile->SetMethod("Legacy");

        int contourPathIndex = -1;
        bool hasPlacement = false;
        xq_ProfilePlacementFrame frame;

        if (auto* ppElem = contourElem->FirstChildElement("path_point"))
        {
            ppElem->QueryIntAttribute("id", &contourPathIndex);
            frame.pathPosIndex = contourPathIndex;
            frame.position = ParsePoint3D(ppElem->FirstChildElement("pos"));
            frame.tangent = xq_Math3::UnitVector(ParseVector3D(ppElem->FirstChildElement("tangent")));
            frame.rotation = xq_Math3::UnitVector(ParseVector3D(ppElem->FirstChildElement("rotation")));
            hasPlacement = contourPathIndex >= 0;
        }

        std::vector<mitk::Point3D> contourPoints;
        if (auto* contourPointsElem = contourElem->FirstChildElement("contour_points"))
        {
            for (auto* ptElem = contourPointsElem->FirstChildElement("point");
                 ptElem;
                 ptElem = ptElem->NextSiblingElement("point"))
            {
                contourPoints.push_back(ParsePoint3D(ptElem));
            }
        }

        if (contourPoints.size() < 3)
        {
            if (auto* controlPointsElem = contourElem->FirstChildElement("control_points"))
            {
                for (auto* ptElem = controlPointsElem->FirstChildElement("point");
                     ptElem;
                     ptElem = ptElem->NextSiblingElement("point"))
                {
                    contourPoints.push_back(ParsePoint3D(ptElem));
                }
            }
        }

        if (contourPoints.size() < 3)
            continue;

        profile->SetAnchorPoints(contourPoints);
        if (hasPlacement)
            xq_SegmentationUtils::ApplyPlacementFrame(profile.get(), frame);
        else
            profile->SetPathPosIndex(static_cast<int>(profileGroup->GetProfileCount()));

        const int resolvedPathIndex = profile->GetPathPosIndex() >= 0
            ? profile->GetPathPosIndex()
            : static_cast<int>(profileGroup->GetProfileCount());
        profileGroup->AppendProfile(profile.release(), resolvedPathIndex);
    }

    if (profileGroup->GetProfileCount() == 0)
        return nullptr;

    mitk::DataNode::Pointer node = mitk::DataNode::New();
    node->SetData(profileGroup);
    node->SetName(GetFileNameOnly(filePath));
    xq::pipeline::MarkNode(node, xq::pipeline::Stage::ContourGroup);
    // XQ UX: Segmentations are hidden by default at import; user toggles on when needed.
    node->SetVisibility(false);
    node->SetProperty("sv.type", mitk::StringProperty::New("ContourGroup"));
    node->SetProperty("sv.contourgroup.file", mitk::StringProperty::New(filePath));
    node->SetProperty("sv.path.id", mitk::IntProperty::New(pathId));
    node->SetColor(1.0f, 1.0f, 0.0f);
    node->SetProperty("contour.show.control.points", mitk::BoolProperty::New(false));
    node->SetProperty("contour.point size", mitk::FloatProperty::New(0.0f));
    node->SetProperty("contour.line width", mitk::FloatProperty::New(2.0f));

    return node;
}

mitk::DataNode::Pointer xq_LegacyImporter::ParseModelFile(
    const std::string& filePath, const std::string& projDir)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(filePath.c_str()) != tinyxml2::XML_SUCCESS)
        return nullptr;

    tinyxml2::XMLElement* modelElem = doc.FirstChildElement("model");
    if (!modelElem)
        return nullptr;

    // Extract model type and face metadata
    const char* modelType = modelElem->Attribute("type");
    std::string type = modelType ? modelType : "PolyData";

    // Try to load companion .vtp file with same base name
    std::string baseName = GetFileNameOnly(filePath);
    std::string modelDir = projDir + "/" + m_ModelsFolderName;
    std::string vtpPath = modelDir + "/" + baseName + ".vtp";

    mitk::DataNode::Pointer node = nullptr;
    if (FileExists(vtpPath))
    {
        node = LoadVTKFile(vtpPath);
    }

    if (node.IsNull())
    {
        // Create metadata-only node
        node = mitk::DataNode::New();
        node->SetName(baseName);
    }

    node->SetProperty("sv.type", mitk::StringProperty::New("Model"));
    node->SetProperty("sv.model.type", mitk::StringProperty::New(type));
    node->SetProperty("sv.model.file", mitk::StringProperty::New(filePath));

    // Extract face info from .mdl
    tinyxml2::XMLElement* tsElem = modelElem->FirstChildElement("timestep");
    if (tsElem)
    {
        tinyxml2::XMLElement* meElem = tsElem->FirstChildElement("model_element");
        if (meElem)
        {
            tinyxml2::XMLElement* facesElem = meElem->FirstChildElement("faces");
            if (facesElem)
            {
                int faceCount = 0;
                std::string faceNames;
                for (tinyxml2::XMLElement* fe = facesElem->FirstChildElement("face");
                     fe;
                     fe = fe->NextSiblingElement("face"))
                {
                    const char* fname = fe->Attribute("name");
                    const char* ftype = fe->Attribute("type");
                    if (fname)
                    {
                        if (!faceNames.empty()) faceNames += ", ";
                        faceNames += fname;
                        if (ftype)
                            faceNames += std::string("(") + ftype + ")";
                    }
                    faceCount++;
                }
                node->SetProperty("sv.model.faces",
                    mitk::StringProperty::New(faceNames));
                node->SetProperty("sv.model.face_count",
                    mitk::IntProperty::New(faceCount));
            }
        }
    }

    return node;
}

// ---------------------------------------------------------------------------
// Utility: load via MITK IOUtil
// ---------------------------------------------------------------------------

mitk::DataNode::Pointer xq_LegacyImporter::LoadVTKFile(const std::string& filePath)
{
    if (!FileExists(filePath))
        return nullptr;

    try
    {
        std::vector<mitk::BaseData::Pointer> loaded = mitk::IOUtil::Load(filePath);
        if (!loaded.empty() && loaded[0].IsNotNull())
        {
            mitk::DataNode::Pointer node = mitk::DataNode::New();
            node->SetData(loaded[0]);
            node->SetName(GetFileNameOnly(filePath));
            return node;
        }
    }
    catch (const mitk::Exception&)
    {
        // Fall through
    }
    catch (const std::exception&)
    {
        // Fall through
    }

    return nullptr;
}

// ---------------------------------------------------------------------------
// Utility: scan directory for files with given extensions
// ---------------------------------------------------------------------------

std::vector<std::string> xq_LegacyImporter::ScanDirectory(
    const std::string& dirPath,
    const std::vector<std::string>& extensions)
{
    std::vector<std::string> result;

    DIR* dir = opendir(dirPath.c_str());
    if (!dir)
        return result;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string name(entry->d_name);
        if (name == "." || name == "..")
            continue;

        std::string fullPath = dirPath + "/" + name;
        struct stat info;
        if (stat(fullPath.c_str(), &info) != 0)
            continue;

        if (info.st_mode & S_IFDIR)
            continue; // skip subdirectories

        std::string ext = GetExtension(name);
        for (const auto& target : extensions)
        {
            if (ext == target)
            {
                result.push_back(fullPath);
                break;
            }
        }
    }

    closedir(dir);
    std::sort(result.begin(), result.end());
    return result;
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

void xq_LegacyImporter::Log(const std::string& message)
{
    m_ImportLog.push_back(message);
}
