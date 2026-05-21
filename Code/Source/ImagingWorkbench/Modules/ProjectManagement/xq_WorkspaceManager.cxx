#include "xq_WorkspaceManager.h"

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
#include "xq_LegacyNodeMigration.h"
#include "xq_NodeMetadataIO.h"
#include "xq_ProjectDataWriter.h"
#include "xq_ProjectDataReader.h"

#include <xq_PipelineDataUtils.h>

#include <mitkIOUtil.h>
#include <mitkNodePredicateDataType.h>
#include <mitkProperties.h>
#include <mitkRenderingManager.h>

#include <tinyxml2.h>

#include <algorithm>
#include <cstdio>
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>

static const char* PROJECT_FILE_EXTENSION = ".xqproj";
static const char* PROJECT_VERSION = "1.0";

static const std::vector<std::string> SUBDIRECTORY_NAMES = {
    "Images",
    "Paths",
    "Segmentations",
    "Models",
    "Meshes",
    "Simulations",
    "ROMSimulations",
    "MultiPhysics",
    "Repository"
};

static const std::map<std::string, std::string> FOLDER_TYPE_TO_SUBDIR = {
    {"ImageFolder",          "Images"},
    {"PathFolder",           "Paths"},
    {"SegmentationFolder",   "Segmentations"},
    {"ModelFolder",          "Models"},
    {"MeshFolder",           "Meshes"},
    {"SimulationFolder",     "Simulations"},
    {"ROMSimulationFolder",  "ROMSimulations"},
    {"MultiPhysicsFolder",   "MultiPhysics"},
    {"RepositoryFolder",     "Repository"}
};

// Normalize folder type strings to canonical subdirectory names.
// Handles both raw GetFolderType() returns ("ImageFolder", "PathFolder", ...)
// and already-normalized subdirectory names ("Images", "Paths", ...)
// so that legacy .xqproj files continue to work.
static std::string NormalizeFolderType(const std::string& folderType)
{
    // First, try the direct map
    auto it = FOLDER_TYPE_TO_SUBDIR.find(folderType);
    if (it != FOLDER_TYPE_TO_SUBDIR.end())
        return it->second;

    // If the value IS already a subdirectory name, return it unchanged
    for (const auto& pair : FOLDER_TYPE_TO_SUBDIR)
    {
        if (pair.second == folderType)
            return folderType;
    }

    return folderType;
}

static int FolderLoadOrder(const std::string& folderType)
{
    const std::string normalizedType = NormalizeFolderType(folderType);
    for (size_t i = 0; i < SUBDIRECTORY_NAMES.size(); ++i)
    {
        if (SUBDIRECTORY_NAMES[i] == normalizedType)
            return static_cast<int>(i);
    }
    return static_cast<int>(SUBDIRECTORY_NAMES.size());
}

xq_WorkspaceManager::xq_WorkspaceManager()
    : m_ProjectPath("")
    , m_ProjectName("")
    , m_ProjectOpen(false)
{
}

xq_WorkspaceManager::~xq_WorkspaceManager()
{
}

// ---------------------------------------------------------------------------
// Static utility methods
// ---------------------------------------------------------------------------

bool xq_WorkspaceManager::DirExists(const std::string& path)
{
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
        return false;
    return (info.st_mode & S_IFDIR) != 0;
}

bool xq_WorkspaceManager::FileExists(const std::string& path)
{
    struct stat info;
    return (stat(path.c_str(), &info) == 0) && !(info.st_mode & S_IFDIR);
}

bool xq_WorkspaceManager::CreateDir(const std::string& path)
{
    if (DirExists(path))
        return true;
#ifdef _WIN32
    return _mkdir(path.c_str()) == 0;
#else
    return mkdir(path.c_str(), 0755) == 0;
#endif
}

bool xq_WorkspaceManager::RemoveFile(const std::string& path)
{
    return std::remove(path.c_str()) == 0;
}

bool xq_WorkspaceManager::CopyFile(const std::string& src, const std::string& dst)
{
    std::ifstream srcFile(src, std::ios::binary);
    if (!srcFile.is_open())
        return false;

    std::ofstream dstFile(dst, std::ios::binary);
    if (!dstFile.is_open())
        return false;

    dstFile << srcFile.rdbuf();
    return dstFile.good();
}

std::string xq_WorkspaceManager::GetFileExtension(const std::string& filename)
{
    auto pos = filename.rfind('.');
    if (pos == std::string::npos)
        return "";
    return filename.substr(pos);
}

std::string xq_WorkspaceManager::GetFileName(const std::string& path)
{
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos)
        return path;
    return path.substr(pos + 1);
}

std::string xq_WorkspaceManager::GetFileNameWithoutExtension(const std::string& path)
{
    std::string name = GetFileName(path);
    auto pos = name.rfind('.');
    if (pos == std::string::npos)
        return name;
    return name.substr(0, pos);
}

std::vector<std::string> xq_WorkspaceManager::GetFilesInDirectory(const std::string& dirPath)
{
    std::vector<std::string> files;
    DIR* dir = opendir(dirPath.c_str());
    if (!dir)
        return files;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        std::string name(entry->d_name);
        if (name == "." || name == "..")
            continue;

        std::string fullPath = dirPath + "/" + name;
        struct stat info;
        if (stat(fullPath.c_str(), &info) == 0 && !(info.st_mode & S_IFDIR))
        {
            files.push_back(name);
        }
    }
    closedir(dir);

    std::sort(files.begin(), files.end());
    return files;
}

const std::vector<std::string>& xq_WorkspaceManager::GetSubdirectoryNames()
{
    return SUBDIRECTORY_NAMES;
}

std::string xq_WorkspaceManager::GetSubdirForFolderType(const std::string& folderType)
{
    auto it = FOLDER_TYPE_TO_SUBDIR.find(folderType);
    if (it != FOLDER_TYPE_TO_SUBDIR.end())
        return it->second;
    return "";
}

// ---------------------------------------------------------------------------
// Project lifecycle
// ---------------------------------------------------------------------------

bool xq_WorkspaceManager::CreateProject(const std::string& path, const std::string& name)
{
    std::string projectDir = path + "/" + name;

    if (!CreateDir(projectDir))
        return false;

    for (const auto& subdir : SUBDIRECTORY_NAMES)
    {
        std::string subdirPath = projectDir + "/" + subdir;
        if (!CreateDir(subdirPath))
            return false;
    }

    std::string projFilePath = projectDir + "/" + name + PROJECT_FILE_EXTENSION;
    if (!WriteProjectFile(projFilePath, name, PROJECT_VERSION))
        return false;

    m_ProjectPath = projectDir;
    m_ProjectName = name;
    m_ProjectOpen = true;

    return true;
}

bool xq_WorkspaceManager::OpenProject(mitk::DataStorage::Pointer dataStorage,
                                    const std::string& projFilePath)
{
    if (dataStorage.IsNull())
        return false;

    if (!FileExists(projFilePath))
        return false;

    std::string projectName;
    std::string version;
    std::vector<std::pair<std::string, std::string>> dataEntries;

    if (!ReadProjectFile(projFilePath, projectName, version, dataEntries))
        return false;

    // Derive project directory from the .xqproj file path
    std::string projDir;
    auto lastSlash = projFilePath.find_last_of("/\\");
    if (lastSlash != std::string::npos)
        projDir = projFilePath.substr(0, lastSlash);
    else
        projDir = ".";

    m_ProjectPath = projDir;
    m_ProjectName = projectName;
    m_ProjectOpen = true;

    // Create a project root node
    mitk::DataNode::Pointer projectNode = mitk::DataNode::New();
    projectNode->SetName(projectName);
    projectNode->SetProperty("project.name", mitk::StringProperty::New(projectName));
    projectNode->SetProperty("project.path", mitk::StringProperty::New(projDir));
    projectNode->SetProperty("project.version", mitk::StringProperty::New(version));
    dataStorage->Add(projectNode);

    // Create folder nodes under the project node
    CreateFolderNodes(dataStorage, projectNode);

    std::stable_sort(dataEntries.begin(), dataEntries.end(),
        [](const auto& lhs, const auto& rhs)
        {
            return FolderLoadOrder(lhs.first) < FolderLoadOrder(rhs.first);
        });

    // Load data entries listed in the project file
    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = dataStorage->GetAll();
    for (const auto& entry : dataEntries)
    {
        const std::string& folderType = entry.first;
        const std::string& relPath = entry.second;

        std::string fullPath = projDir + "/" + relPath;
        if (!FileExists(fullPath))
            continue;

        // Normalize the folder type from the project file to match the
        // canonical subdirectory names (handles both legacy raw types
        // like "ImageFolder" and new normalized types like "Images").
        std::string normalizedType = NormalizeFolderType(folderType);

        // Find the folder node that matches this folder type
        mitk::DataNode::Pointer parentFolderNode = nullptr;
        for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
        {
            mitk::DataNode::Pointer node = it->Value();
            if (auto* folder = dynamic_cast<xq_DataFolder*>(node->GetData()))
            {
                std::string folderNorm = NormalizeFolderType(folder->GetFolderType());
                if (folderNorm == normalizedType)
                {
                    parentFolderNode = node;
                    break;
                }
            }
        }

        // Load data using XQ custom reader (handles XQ types first, falls back to MITK IOUtil)
        const auto readResult = xq_ProjectDataReader::LoadNodeData(fullPath);
        for (auto dataNode : readResult.nodes)
        {
            // Restore pipeline metadata from sidecar file FIRST — this
            // gives the correct node name and properties.  Only then
            // fall back to the filename-derived name if the metadata
            // sidecar is missing or did not contain a name element.
            std::string metaPath = fullPath + ".xqmeta.xml";
            xq_NodeMetadataIO::ReadNodeMetadata(dataNode, metaPath);

            if (dataNode->GetName().empty())
                dataNode->SetName(GetFileNameWithoutExtension(relPath));

            // Initialize display properties based on data category
            // AND stamp the pipeline stage so downstream services and
            // the folder-based migration can resolve this node.
            if (normalizedType == "Images") {
                dataNode->SetVisibility(true);
                dataNode->SetOpacity(1.0);
            } else if (normalizedType == "Paths") {
                dataNode->SetVisibility(false);
                dataNode->SetFloatProperty("point size", 1.0f);
                if (!xq::pipeline::HasStage(dataNode, xq::pipeline::Stage::Path))
                    xq::pipeline::MarkNode(dataNode, xq::pipeline::Stage::Path);
            } else if (normalizedType == "Segmentations") {
                dataNode->SetVisibility(false);
                if (!xq::pipeline::HasStage(dataNode, xq::pipeline::Stage::ContourGroup))
                    xq::pipeline::MarkNode(dataNode, xq::pipeline::Stage::ContourGroup);
            } else if (normalizedType == "Models") {
                dataNode->SetVisibility(true);
                if (!xq::pipeline::HasStage(dataNode, xq::pipeline::Stage::Model))
                    xq::pipeline::MarkNode(dataNode, xq::pipeline::Stage::Model);
            } else if (normalizedType == "Meshes") {
                dataNode->SetVisibility(true);
                dataNode->SetColor(0.0f, 0.8f, 0.2f);
                dataNode->SetProperty("opacity", mitk::FloatProperty::New(0.6f));
                if (!xq::pipeline::HasStage(dataNode, xq::pipeline::Stage::VolumeMesh))
                    xq::pipeline::MarkNode(dataNode, xq::pipeline::Stage::VolumeMesh);
            } else if (normalizedType == "Simulations") {
                dataNode->SetVisibility(true);
                if (!xq::pipeline::HasStage(dataNode, xq::pipeline::Stage::SimulationPrep))
                    xq::pipeline::MarkNode(dataNode, xq::pipeline::Stage::SimulationPrep);
            } else {
                dataNode->SetVisibility(false);
            }

            if (parentFolderNode.IsNotNull())
                dataStorage->Add(dataNode, parentFolderNode);
            else
                dataStorage->Add(dataNode, projectNode);
        }

        // Log any reader diagnostics
        for (const auto& diag : readResult.diagnostics)
        {
            MITK_WARN << "OpenProject: " << diag;
        }
    }

    // Re-parent any pre-existing stage-tagged nodes into their category
    // folders so the Data Manager tree reflects current conventions.
    xq_LegacyNodeMigration::ReparentIntoCategoryFolders(dataStorage);

    // Auto-center all views around loaded data
    mitk::RenderingManager::GetInstance()->InitializeViewsByBoundingObjects(dataStorage);

    return true;
}

bool xq_WorkspaceManager::SaveProject(mitk::DataStorage::Pointer dataStorage,
                                    const std::string& projDir)
{
    if (dataStorage.IsNull())
        return false;

    if (!DirExists(projDir))
        return false;

    std::string projectName = GetFileName(projDir);
    std::string projFilePath = projDir + "/" + projectName + PROJECT_FILE_EXTENSION;

    // Collect data entries: traverse folder nodes and their children
    std::vector<std::pair<std::string, std::string>> dataEntries;

    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = dataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        mitk::DataNode::Pointer node = it->Value();
        auto* folder = dynamic_cast<xq_DataFolder*>(node->GetData());
        if (!folder)
            continue;

        std::string folderType = folder->GetFolderType();
        std::string subdir = GetSubdirForFolderType(folderType);
        if (subdir.empty())
            continue;

        std::string subdirPath = projDir + "/" + subdir;
        if (!DirExists(subdirPath))
            CreateDir(subdirPath);

        // Get children of this folder node
        mitk::DataStorage::SetOfObjects::ConstPointer children =
            dataStorage->GetDerivations(node);

        if (children.IsNull())
            continue;

        for (auto childIt = children->Begin(); childIt != children->End(); ++childIt)
        {
            mitk::DataNode::Pointer childNode = childIt->Value();
            if (childNode.IsNull() || childNode->GetData() == nullptr)
                continue;

            // Skip other folder nodes
            if (dynamic_cast<xq_DataFolder*>(childNode->GetData()))
                continue;

            std::string nodeName = childNode->GetName();
            if (nodeName.empty())
                nodeName = "Unnamed";

            // Save data using explicit writer (handles custom XQ types)
            std::string savePath = subdirPath + "/" + nodeName;
            xq_ProjectDataWriteResult writeResult =
                xq_ProjectDataWriter::SaveNodeData(childNode, savePath, subdir);

            if (writeResult.ok)
            {
                if (!writeResult.relPath.empty())
                {
                    std::string normalizedType = NormalizeFolderType(folderType);
                    dataEntries.push_back(std::make_pair(normalizedType, writeResult.relPath));
                }

                // Write pipeline metadata sidecar alongside the data file
                std::string metaPath = (writeResult.savedPath.empty() ? savePath : writeResult.savedPath)
                                       + ".xqmeta.xml";
                xq_NodeMetadataIO::WriteNodeMetadata(childNode, metaPath);
            }
            else
            {
                // Log diagnostics — don't silently skip nodes
                for (const auto& diag : writeResult.diagnostics)
                {
                    MITK_WARN << "SaveProject: failed to save node '"
                              << nodeName << "': " << diag;
                }
            }
        }
    }

    return WriteProjectFileWithData(projFilePath, projectName, PROJECT_VERSION, dataEntries);
}

void xq_WorkspaceManager::SaveAllProjects(mitk::DataStorage::Pointer dataStorage)
{
    if (dataStorage.IsNull())
        return;

    // Find all project root nodes (nodes with project.path property)
    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = dataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
        mitk::DataNode::Pointer node = it->Value();
        mitk::StringProperty* pathProp =
            dynamic_cast<mitk::StringProperty*>(node->GetProperty("project.path"));
        if (pathProp)
        {
            std::string projDir = pathProp->GetValue();
            SaveProject(dataStorage, projDir);
        }
    }
}

// ---------------------------------------------------------------------------
// Data node management
// ---------------------------------------------------------------------------

void xq_WorkspaceManager::AddDataNode(mitk::DataStorage::Pointer dataStorage,
                                    mitk::DataNode::Pointer node,
                                    mitk::DataNode::Pointer parentNode)
{
    if (dataStorage.IsNull() || node.IsNull())
        return;

    if (parentNode.IsNotNull())
        dataStorage->Add(node, parentNode);
    else
        dataStorage->Add(node);
}

void xq_WorkspaceManager::RemoveDataNode(mitk::DataStorage::Pointer dataStorage,
                                       mitk::DataNode::Pointer node)
{
    if (dataStorage.IsNull() || node.IsNull())
        return;

    dataStorage->Remove(node);
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

std::string xq_WorkspaceManager::GetProjectPath() const
{
    return m_ProjectPath;
}

std::string xq_WorkspaceManager::GetProjectName() const
{
    return m_ProjectName;
}

bool xq_WorkspaceManager::IsProjectOpen() const
{
    return m_ProjectOpen;
}

// ---------------------------------------------------------------------------
// XML I/O — Private helpers
// ---------------------------------------------------------------------------

bool xq_WorkspaceManager::WriteProjectFile(const std::string& projFilePath,
                                         const std::string& projectName,
                                         const std::string& version)
{
    std::vector<std::pair<std::string, std::string>> emptyEntries;
    return WriteProjectFileWithData(projFilePath, projectName, version, emptyEntries);
}

bool xq_WorkspaceManager::ReadProjectFile(
    const std::string& projFilePath,
    std::string& projectName,
    std::string& version,
    std::vector<std::pair<std::string, std::string>>& dataEntries)
{
    tinyxml2::XMLDocument doc;
    if (doc.LoadFile(projFilePath.c_str()) != tinyxml2::XML_SUCCESS)
        return false;

    tinyxml2::XMLElement* root = doc.FirstChildElement("xq_project");
    if (!root)
        return false;

    // Read project attributes
    const char* nameAttr = root->Attribute("name");
    projectName = nameAttr ? nameAttr : "";

    const char* versionAttr = root->Attribute("version");
    version = versionAttr ? versionAttr : "1.0";

    // Read data entries
    dataEntries.clear();
    tinyxml2::XMLElement* dataElem = root->FirstChildElement("data");
    if (dataElem)
    {
        for (tinyxml2::XMLElement* entry = dataElem->FirstChildElement("entry");
             entry;
             entry = entry->NextSiblingElement("entry"))
        {
            const char* folderType = entry->Attribute("folder_type");
            const char* path = entry->Attribute("path");

            if (folderType && path)
            {
                dataEntries.push_back(std::make_pair(
                    std::string(folderType), std::string(path)));
            }
        }
    }

    return !projectName.empty();
}

bool xq_WorkspaceManager::WriteProjectFileWithData(
    const std::string& projFilePath,
    const std::string& projectName,
    const std::string& version,
    const std::vector<std::pair<std::string, std::string>>& dataEntries)
{
    tinyxml2::XMLDocument doc;

    tinyxml2::XMLDeclaration* decl = doc.NewDeclaration();
    doc.InsertFirstChild(decl);

    tinyxml2::XMLElement* root = doc.NewElement("xq_project");
    root->SetAttribute("name", projectName.c_str());
    root->SetAttribute("version", version.c_str());
    doc.InsertEndChild(root);

    // Write subdirectory listing
    tinyxml2::XMLElement* dirsElem = doc.NewElement("directories");
    root->InsertEndChild(dirsElem);

    for (const auto& subdir : SUBDIRECTORY_NAMES)
    {
        tinyxml2::XMLElement* dirElem = doc.NewElement("directory");
        dirElem->SetAttribute("name", subdir.c_str());
        dirsElem->InsertEndChild(dirElem);
    }

    // Write data entries
    if (!dataEntries.empty())
    {
        tinyxml2::XMLElement* dataElem = doc.NewElement("data");
        root->InsertEndChild(dataElem);

        for (const auto& entry : dataEntries)
        {
            tinyxml2::XMLElement* entryElem = doc.NewElement("entry");
            entryElem->SetAttribute("folder_type", entry.first.c_str());
            entryElem->SetAttribute("path", entry.second.c_str());
            dataElem->InsertEndChild(entryElem);
        }
    }

    return doc.SaveFile(projFilePath.c_str()) == tinyxml2::XML_SUCCESS;
}

void xq_WorkspaceManager::CreateFolderNodes(mitk::DataStorage::Pointer dataStorage,
                                          mitk::DataNode::Pointer projectNode)
{
    auto addFolder = [&](auto folderData, const std::string& nodeName)
    {
        mitk::DataNode::Pointer node = mitk::DataNode::New();
        node->SetName(nodeName);
        node->SetData(folderData);
        node->SetVisibility(false);
        dataStorage->Add(node, projectNode);
    };

    addFolder(xq_ImageFolder::New().GetPointer(),          "Images");
    addFolder(xq_PathFolder::New().GetPointer(),           "Paths");
    addFolder(xq_SegmentationFolder::New().GetPointer(),   "Segmentations");
    addFolder(xq_ModelFolder::New().GetPointer(),          "Models");
    addFolder(xq_GridFolder::New().GetPointer(),           "Meshes");
    addFolder(xq_SimulationFolder::New().GetPointer(),     "Simulations");
    addFolder(xq_ROMSimulationFolder::New().GetPointer(),  "ROMSimulations");
    addFolder(xq_MultiPhysicsFolder::New().GetPointer(),   "MultiPhysics");
    addFolder(xq_RepositoryFolder::New().GetPointer(),     "Repository");
}
