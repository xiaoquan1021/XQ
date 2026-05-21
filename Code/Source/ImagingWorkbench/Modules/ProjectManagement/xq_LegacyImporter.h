#ifndef XQ_LEGACYIMPORTER_H
#define XQ_LEGACYIMPORTER_H

#include <xqProjectManagementExports.h>
#include <mitkDataStorage.h>
#include <mitkDataNode.h>
#include <string>
#include <vector>
#include <map>

class XQPROJECTMANAGEMENT_EXPORT xq_LegacyImporter
{
public:
    xq_LegacyImporter();
    ~xq_LegacyImporter();

    // Main entry point: import a compatible legacy project directory into XQ DataStorage
    bool ImportProject(mitk::DataStorage::Pointer dataStorage, const std::string& svProjDir);

    // Get import log messages
    std::vector<std::string> GetImportLog() const;
    std::string GetProjectName() const;

    // Reuse the legacy file parsers outside of full project import, e.g. for
    // upgrading previously imported legacy PointSet nodes in-place.
    mitk::DataNode::Pointer ParsePathFile(const std::string& filePath);
    mitk::DataNode::Pointer ParseContourGroupFile(const std::string& filePath);
    mitk::DataNode::Pointer ParseModelFile(const std::string& filePath, const std::string& projDir);

private:
    // Parse .svproj XML to discover project structure
    bool ParseSVProjectFile(const std::string& svprojPath);

    // Import each category
    void ImportImages(mitk::DataStorage::Pointer ds, mitk::DataNode::Pointer parentNode,
                      const std::string& projDir);
    void ImportPaths(mitk::DataStorage::Pointer ds, mitk::DataNode::Pointer parentNode,
                     const std::string& projDir);
    void ImportSegmentations(mitk::DataStorage::Pointer ds, mitk::DataNode::Pointer parentNode,
                             const std::string& projDir);
    void ImportModels(mitk::DataStorage::Pointer ds, mitk::DataNode::Pointer parentNode,
                      const std::string& projDir);
    void ImportMeshes(mitk::DataStorage::Pointer ds, mitk::DataNode::Pointer parentNode,
                      const std::string& projDir);
    void ImportSimulations(mitk::DataStorage::Pointer ds, mitk::DataNode::Pointer parentNode,
                           const std::string& projDir);

    // Utility: load standard VTK files via MITK IOUtil
    mitk::DataNode::Pointer LoadVTKFile(const std::string& filePath);

    // Utility: scan directory for files with given extensions
    std::vector<std::string> ScanDirectory(const std::string& dirPath,
                                            const std::vector<std::string>& extensions);

    void Log(const std::string& message);

    // Project metadata from .svproj
    std::string m_ProjectName;
    std::string m_ImagesFolderName;
    std::string m_PathsFolderName;
    std::string m_SegmentationsFolderName;
    std::string m_ModelsFolderName;
    std::string m_MeshesFolderName;
    std::string m_SimulationsFolderName;
    std::string m_FlowFolderName;

    // Image entries from .svproj
    struct SVImageEntry {
        std::string name;
        std::string path;
        bool inProject;
    };
    std::vector<SVImageEntry> m_ImageEntries;
    std::map<int, std::string> m_PathIdToName;

    std::vector<std::string> m_ImportLog;
};

#endif
