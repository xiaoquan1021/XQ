#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_ProjectService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

xq::core::DataCatalogEntry MakeEntry(const QString& id,
                                     const QString& name,
                                     const QString& source)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = id;
    entry.DisplayName = name;
    entry.SourcePath = source;
    entry.Modality = QStringLiteral("CT");
    entry.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return entry;
}

bool WriteProjectFixture(const QString& projectPath,
                         const QString& projectName,
                         const QString& entryId,
                         const QString& folderId,
                         const QString& nodeId,
                         QString* errorMessage)
{
    xq::core::ProjectService project;
    if (!project.CreateProject(projectName, projectPath, errorMessage))
        return false;

    xq::core::DataCatalogService catalog;
    if (!catalog.RegisterEntry(MakeEntry(entryId,
                                         projectName + QStringLiteral(" Image"),
                                         QStringLiteral("C:/data/") + entryId),
                               errorMessage))
    {
        return false;
    }

    xq::core::DataHierarchyService hierarchy;
    if (!hierarchy.AddFolder(folderId,
                             hierarchy.RootId(),
                             projectName + QStringLiteral(" Images"),
                             errorMessage))
    {
        return false;
    }
    if (!hierarchy.AddDataEntry(nodeId,
                                folderId,
                                entryId,
                                projectName + QStringLiteral(" Image"),
                                errorMessage))
    {
        return false;
    }

    return project.SaveProject(catalog, hierarchy, errorMessage);
}

bool WriteInvalidHierarchyFixture(const QString& projectPath)
{
    QJsonObject catalogEntry;
    catalogEntry.insert(QStringLiteral("id"), QStringLiteral("failed-image"));
    catalogEntry.insert(QStringLiteral("displayName"), QStringLiteral("Failed Image"));
    catalogEntry.insert(QStringLiteral("sourcePath"), QStringLiteral("C:/data/failed"));
    catalogEntry.insert(QStringLiteral("modality"), QStringLiteral("CT"));
    catalogEntry.insert(QStringLiteral("workflowRole"), QStringLiteral("image"));

    QJsonArray catalog;
    catalog.append(catalogEntry);

    QJsonObject badNode;
    badNode.insert(QStringLiteral("id"), QStringLiteral("failed-node"));
    badNode.insert(QStringLiteral("parentId"), QStringLiteral("root"));
    badNode.insert(QStringLiteral("displayName"), QStringLiteral("Failed Node"));
    badNode.insert(QStringLiteral("kind"), QStringLiteral("not-a-kind"));

    QJsonArray hierarchy;
    hierarchy.append(badNode);

    QJsonObject projectObject;
    projectObject.insert(QStringLiteral("name"), QStringLiteral("FailedStudy"));
    projectObject.insert(QStringLiteral("workspaceDirectory"),
                         QStringLiteral("workspace"));
    projectObject.insert(QStringLiteral("dataCatalog"), catalog);
    projectObject.insert(QStringLiteral("dataHierarchy"), hierarchy);

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), QStringLiteral("2.0"));
    root.insert(QStringLiteral("project"), projectObject);

    QFile file(projectPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    QString errorMessage;
    const QString firstProjectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("FirstStudy.xqproj"));
    if (Expect(WriteProjectFixture(firstProjectPath,
                                   QStringLiteral("FirstStudy"),
                                   QStringLiteral("first-image"),
                                   QStringLiteral("first-folder"),
                                   QStringLiteral("first-node"),
                                   &errorMessage),
               "first project fixture should be written"))
        return 1;

    const QString secondProjectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("SecondStudy.xqproj"));
    if (Expect(WriteProjectFixture(secondProjectPath,
                                   QStringLiteral("SecondStudy"),
                                   QStringLiteral("second-image"),
                                   QStringLiteral("second-folder"),
                                   QStringLiteral("second-node"),
                                   &errorMessage),
               "second project fixture should be written"))
        return 1;

    xq::core::ProjectService project;
    xq::core::DataCatalogService catalog;
    xq::core::DataHierarchyService hierarchy;

    if (Expect(project.OpenProject(firstProjectPath,
                                   catalog,
                                   hierarchy,
                                   &errorMessage),
               "opening first project should succeed"))
        return 1;
    if (Expect(project.CurrentProject() != nullptr &&
                   project.CurrentProject()->Name == QStringLiteral("FirstStudy"),
               "first open should set project metadata"))
        return 1;
    if (Expect(catalog.FindById(QStringLiteral("first-image")) != nullptr,
               "first open should load first catalog entry"))
        return 1;
    if (Expect(hierarchy.FindNode(QStringLiteral("first-folder")) != nullptr,
               "first open should load first hierarchy node"))
        return 1;

    if (Expect(project.OpenProject(secondProjectPath,
                                   catalog,
                                   hierarchy,
                                   &errorMessage),
               "opening second project should succeed"))
        return 1;
    if (Expect(project.CurrentProject() != nullptr &&
                   project.CurrentProject()->Name == QStringLiteral("SecondStudy"),
               "second open should replace project metadata"))
        return 1;
    if (Expect(catalog.Entries().size() == 1,
               "second open should replace catalog entries"))
        return 1;
    if (Expect(catalog.FindById(QStringLiteral("first-image")) == nullptr,
               "second open should remove stale catalog entries"))
        return 1;
    if (Expect(catalog.FindById(QStringLiteral("second-image")) != nullptr,
               "second open should load second catalog entry"))
        return 1;

    const auto rootChildren = hierarchy.ChildrenOf(hierarchy.RootId());
    if (Expect(rootChildren.size() == 1,
               "second open should replace hierarchy root children"))
        return 1;
    if (Expect(rootChildren.at(0).Id == QStringLiteral("second-folder"),
               "second open should load second hierarchy root child"))
        return 1;
    if (Expect(hierarchy.FindNode(QStringLiteral("first-folder")) == nullptr,
               "second open should remove stale hierarchy nodes"))
        return 1;

    const QString invalidProjectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("InvalidStudy.xqproj"));
    if (Expect(WriteInvalidHierarchyFixture(invalidProjectPath),
               "invalid hierarchy fixture should be written"))
        return 1;

    errorMessage.clear();
    if (Expect(!project.OpenProject(invalidProjectPath,
                                    catalog,
                                    hierarchy,
                                    &errorMessage),
               "invalid hierarchy open should fail"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("hierarchy node kind")),
               "invalid hierarchy open should report useful error"))
        return 1;
    if (Expect(project.CurrentProject() != nullptr &&
                   project.CurrentProject()->Name == QStringLiteral("SecondStudy"),
               "failed open should not replace project metadata"))
        return 1;
    if (Expect(catalog.Entries().size() == 1,
               "failed open should not mutate catalog entries"))
        return 1;
    if (Expect(catalog.FindById(QStringLiteral("failed-image")) == nullptr,
               "failed open should not leak parsed catalog entries"))
        return 1;
    if (Expect(hierarchy.ChildrenOf(hierarchy.RootId()).size() == 1 &&
                   hierarchy.ChildrenOf(hierarchy.RootId()).at(0).Id ==
                       QStringLiteral("second-folder"),
               "failed open should not mutate hierarchy entries"))
        return 1;

    return 0;
}
