#ifndef XQ_WORKSPACEMANAGER_H
#define XQ_WORKSPACEMANAGER_H

#include <xqProjectManagementExports.h>

#include <mitkDataStorage.h>
#include <mitkDataNode.h>

#include <string>
#include <vector>
#include <map>

class XQPROJECTMANAGEMENT_EXPORT xq_WorkspaceManager
{
public:
    xq_WorkspaceManager();
    ~xq_WorkspaceManager();

    // Project lifecycle
    bool CreateProject(const std::string& path, const std::string& name);
    bool OpenProject(mitk::DataStorage::Pointer dataStorage, const std::string& projFilePath);
    bool SaveProject(mitk::DataStorage::Pointer dataStorage, const std::string& projDir);
    void SaveAllProjects(mitk::DataStorage::Pointer dataStorage);

    // Data node management
    void AddDataNode(mitk::DataStorage::Pointer dataStorage,
                     mitk::DataNode::Pointer node,
                     mitk::DataNode::Pointer parentNode);
    void RemoveDataNode(mitk::DataStorage::Pointer dataStorage,
                        mitk::DataNode::Pointer node);

    // Accessors
    std::string GetProjectPath() const;
    std::string GetProjectName() const;
    bool IsProjectOpen() const;

    // Static utility methods
    static bool DirExists(const std::string& path);
    static bool FileExists(const std::string& path);
    static bool CreateDir(const std::string& path);
    static bool RemoveFile(const std::string& path);
    static bool CopyFile(const std::string& src, const std::string& dst);
    static std::string GetFileExtension(const std::string& filename);
    static std::string GetFileName(const std::string& path);
    static std::string GetFileNameWithoutExtension(const std::string& path);
    static std::vector<std::string> GetFilesInDirectory(const std::string& dirPath);

    // Subdirectory names
    static const std::vector<std::string>& GetSubdirectoryNames();

    // Map folder type to subdirectory name
    static std::string GetSubdirForFolderType(const std::string& folderType);

private:
    bool WriteProjectFile(const std::string& projFilePath,
                          const std::string& projectName,
                          const std::string& version);
    bool ReadProjectFile(const std::string& projFilePath,
                         std::string& projectName,
                         std::string& version,
                         std::vector<std::pair<std::string, std::string>>& dataEntries);
    bool WriteProjectFileWithData(const std::string& projFilePath,
                                  const std::string& projectName,
                                  const std::string& version,
                                  const std::vector<std::pair<std::string, std::string>>& dataEntries);
    void CreateFolderNodes(mitk::DataStorage::Pointer dataStorage,
                           mitk::DataNode::Pointer projectNode);

    std::string m_ProjectPath;
    std::string m_ProjectName;
    bool m_ProjectOpen;
};

#endif
