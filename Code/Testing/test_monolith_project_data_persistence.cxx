#include "Core/xq_DataCatalogService.h"
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
                                     const QString& source,
                                     const QString& modality,
                                     xq::core::DataWorkflowRole role)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = id;
    entry.DisplayName = name;
    entry.SourcePath = source;
    entry.Modality = modality;
    entry.WorkflowRole = role;
    return entry;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    const QString projectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("CatalogStudy.xqproj"));

    xq::core::ProjectService projectService;
    QString errorMessage;
    if (Expect(projectService.CreateProject(QStringLiteral("CatalogStudy"),
                                            projectPath,
                                            &errorMessage),
               "CreateProject should accept a valid project"))
        return 1;

    xq::core::DataCatalogService catalog;
    if (Expect(catalog.RegisterEntry(
                   MakeEntry(QStringLiteral("image-001"),
                             QStringLiteral("CTA"),
                             QStringLiteral("C:/data/cta"),
                             QStringLiteral("CT"),
                             xq::core::DataWorkflowRole::Image),
                   &errorMessage),
               "first data entry should register"))
        return 1;
    if (Expect(catalog.RegisterEntry(
                   MakeEntry(QStringLiteral("dicom-002"),
                             QStringLiteral("MR DICOM"),
                             QStringLiteral("C:/data/mr"),
                             QStringLiteral("MR"),
                             xq::core::DataWorkflowRole::DICOMSeries),
                   &errorMessage),
               "second data entry should register"))
        return 1;

    if (Expect(projectService.SaveProject(catalog, &errorMessage),
               "SaveProject should write project data catalog metadata"))
        return 1;

    xq::core::ProjectService openedProject;
    xq::core::DataCatalogService openedCatalog;
    if (Expect(openedProject.OpenProject(projectPath,
                                         openedCatalog,
                                         &errorMessage),
               "OpenProject should restore project data catalog metadata"))
        return 1;

    const auto restoredEntries = openedCatalog.Entries();
    if (Expect(restoredEntries.size() == 2,
               "OpenProject should restore all data catalog entries"))
        return 1;
    if (Expect(restoredEntries.at(0).Id == QStringLiteral("image-001"),
               "OpenProject should preserve data catalog entry order"))
        return 1;
    if (Expect(restoredEntries.at(0).WorkflowRole ==
                   xq::core::DataWorkflowRole::Image,
               "OpenProject should restore image workflow role"))
        return 1;
    if (Expect(restoredEntries.at(1).WorkflowRole ==
                   xq::core::DataWorkflowRole::DICOMSeries,
               "OpenProject should restore DICOM workflow role"))
        return 1;
    if (Expect(restoredEntries.at(1).SourcePath == QStringLiteral("C:/data/mr"),
               "OpenProject should restore source paths"))
        return 1;

    const QString invalidPath =
        QDir(tempDir.path()).filePath(QStringLiteral("InvalidCatalog.xqproj"));
    QFile invalidFile(invalidPath);
    if (Expect(invalidFile.open(QIODevice::WriteOnly),
               "invalid project fixture should be writable"))
        return 1;

    QJsonObject invalidEntry;
    invalidEntry.insert(QStringLiteral("id"), QStringLiteral("bad-role"));
    invalidEntry.insert(QStringLiteral("displayName"), QStringLiteral("Bad Role"));
    invalidEntry.insert(QStringLiteral("sourcePath"), QStringLiteral("C:/data/bad"));
    invalidEntry.insert(QStringLiteral("modality"), QStringLiteral("CT"));
    invalidEntry.insert(QStringLiteral("workflowRole"), QStringLiteral("bad-role"));

    QJsonArray invalidData;
    invalidData.append(invalidEntry);

    QJsonObject projectObject;
    projectObject.insert(QStringLiteral("name"), QStringLiteral("BadCatalog"));
    projectObject.insert(QStringLiteral("workspaceDirectory"),
                         QStringLiteral("workspace"));
    projectObject.insert(QStringLiteral("dataCatalog"), invalidData);

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), QStringLiteral("2.0"));
    root.insert(QStringLiteral("project"), projectObject);

    invalidFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    invalidFile.close();

    errorMessage.clear();
    if (Expect(!openedProject.OpenProject(invalidPath,
                                          openedCatalog,
                                          &errorMessage),
               "OpenProject should reject unknown data workflow roles"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("workflow role")),
               "unknown workflow role should produce a useful error"))
        return 1;

    return 0;
}
