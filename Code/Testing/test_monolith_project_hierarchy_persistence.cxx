#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_ProjectService.h"
#include "Core/xq_ProjectSessionService.h"

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
                                     xq::core::DataWorkflowRole role)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = id;
    entry.DisplayName = name;
    entry.SourcePath = source;
    entry.Modality = QStringLiteral("CT");
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
        QDir(tempDir.path()).filePath(QStringLiteral("HierarchyStudy.xqproj"));

    xq::core::ProjectService projectService;
    QString errorMessage;
    if (Expect(projectService.CreateProject(QStringLiteral("HierarchyStudy"),
                                            projectPath,
                                            &errorMessage),
               "CreateProject should accept a valid project"))
        return 1;

    xq::core::DataCatalogService catalog;
    if (Expect(catalog.RegisterEntry(
                   MakeEntry(QStringLiteral("image-001"),
                             QStringLiteral("CTA"),
                             QStringLiteral("C:/data/cta"),
                             xq::core::DataWorkflowRole::Image),
                   &errorMessage),
               "image entry should register"))
        return 1;
    if (Expect(catalog.RegisterEntry(
                   MakeEntry(QStringLiteral("seg-001"),
                             QStringLiteral("Segmentation"),
                             QStringLiteral("C:/data/seg"),
                             xq::core::DataWorkflowRole::Segmentation),
                   &errorMessage),
               "segmentation entry should register"))
        return 1;

    xq::core::DataHierarchyService hierarchy;
    if (Expect(hierarchy.AddFolder(QStringLiteral("images"),
                                   hierarchy.RootId(),
                                   QStringLiteral("Images"),
                                   &errorMessage),
               "image folder should be added"))
        return 1;
    if (Expect(hierarchy.AddDataEntry(QStringLiteral("cta-node"),
                                      QStringLiteral("images"),
                                      QStringLiteral("image-001"),
                                      QStringLiteral("CTA"),
                                      &errorMessage),
               "image data node should be added"))
        return 1;
    if (Expect(hierarchy.AddFolder(QStringLiteral("segmentations"),
                                   hierarchy.RootId(),
                                   QStringLiteral("Segmentations"),
                                   &errorMessage),
               "segmentation folder should be added"))
        return 1;
    if (Expect(hierarchy.AddDataEntry(QStringLiteral("seg-node"),
                                      QStringLiteral("segmentations"),
                                      QStringLiteral("seg-001"),
                                      QStringLiteral("Segmentation"),
                                      &errorMessage),
               "segmentation data node should be added"))
        return 1;

    if (Expect(projectService.SaveProject(catalog, hierarchy, &errorMessage),
               "SaveProject should write catalog and hierarchy metadata"))
        return 1;

    QFile savedFile(projectPath);
    if (Expect(savedFile.open(QIODevice::ReadOnly),
               "saved project should be readable"))
        return 1;
    const QJsonDocument savedDocument =
        QJsonDocument::fromJson(savedFile.readAll());
    const QJsonObject projectObject =
        savedDocument.object().value(QStringLiteral("project")).toObject();
    const QJsonArray savedHierarchy =
        projectObject.value(QStringLiteral("dataHierarchy")).toArray();
    if (Expect(savedHierarchy.size() == 4,
               "project JSON should persist non-root hierarchy nodes"))
        return 1;
    if (Expect(savedHierarchy.at(0).toObject().value(QStringLiteral("id"))
                   .toString() == QStringLiteral("images"),
               "hierarchy JSON should preserve node order"))
        return 1;
    if (Expect(savedHierarchy.at(1).toObject().value(QStringLiteral("kind"))
                   .toString() == QStringLiteral("data-entry"),
               "hierarchy JSON should persist data-entry kind"))
        return 1;
    if (Expect(savedHierarchy.at(1).toObject()
                   .value(QStringLiteral("dataCatalogEntryId"))
                   .toString() == QStringLiteral("image-001"),
               "hierarchy JSON should persist catalog entry references"))
        return 1;

    xq::core::ProjectService openedProject;
    xq::core::DataCatalogService openedCatalog;
    xq::core::DataHierarchyService openedHierarchy;
    if (Expect(openedProject.OpenProject(projectPath,
                                         openedCatalog,
                                         openedHierarchy,
                                         &errorMessage),
               "OpenProject should restore catalog and hierarchy metadata"))
        return 1;

    const auto rootChildren = openedHierarchy.ChildrenOf(openedHierarchy.RootId());
    if (Expect(rootChildren.size() == 2,
               "OpenProject should restore root hierarchy children"))
        return 1;
    if (Expect(rootChildren.at(0).Id == QStringLiteral("images") &&
                   rootChildren.at(1).Id == QStringLiteral("segmentations"),
               "OpenProject should preserve root child order"))
        return 1;

    const auto imageChildren =
        openedHierarchy.ChildrenOf(QStringLiteral("images"));
    if (Expect(imageChildren.size() == 1,
               "OpenProject should restore nested hierarchy children"))
        return 1;
    if (Expect(imageChildren.at(0).Kind ==
                   xq::core::DataHierarchyNodeKind::DataEntry,
               "OpenProject should restore data-entry node kind"))
        return 1;
    if (Expect(imageChildren.at(0).DataCatalogEntryId ==
                   QStringLiteral("image-001"),
               "OpenProject should restore catalog entry references"))
        return 1;

    const QString invalidPath =
        QDir(tempDir.path()).filePath(QStringLiteral("InvalidHierarchy.xqproj"));
    QFile invalidFile(invalidPath);
    if (Expect(invalidFile.open(QIODevice::WriteOnly),
               "invalid project fixture should be writable"))
        return 1;

    QJsonObject invalidNode;
    invalidNode.insert(QStringLiteral("id"), QStringLiteral("bad-node"));
    invalidNode.insert(QStringLiteral("parentId"), QStringLiteral("root"));
    invalidNode.insert(QStringLiteral("displayName"), QStringLiteral("Bad Node"));
    invalidNode.insert(QStringLiteral("kind"), QStringLiteral("bad-kind"));

    QJsonArray invalidHierarchy;
    invalidHierarchy.append(invalidNode);

    QJsonObject invalidProjectObject;
    invalidProjectObject.insert(QStringLiteral("name"),
                                QStringLiteral("BadHierarchy"));
    invalidProjectObject.insert(QStringLiteral("workspaceDirectory"),
                                QStringLiteral("workspace"));
    invalidProjectObject.insert(QStringLiteral("dataHierarchy"),
                                invalidHierarchy);

    QJsonObject invalidRoot;
    invalidRoot.insert(QStringLiteral("schemaVersion"), QStringLiteral("2.0"));
    invalidRoot.insert(QStringLiteral("project"), invalidProjectObject);

    invalidFile.write(QJsonDocument(invalidRoot).toJson(QJsonDocument::Indented));
    invalidFile.close();

    errorMessage.clear();
    if (Expect(!openedProject.OpenProject(invalidPath,
                                          openedCatalog,
                                          openedHierarchy,
                                          &errorMessage),
               "OpenProject should reject unknown hierarchy node kinds"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("hierarchy node kind")),
               "unknown hierarchy node kind should produce a useful error"))
        return 1;

    const QString sessionPath =
        QDir(tempDir.path()).filePath(QStringLiteral("SessionHierarchy.xqproj"));
    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (Expect(context->Projects()->CreateProject(QStringLiteral("Session"),
                                                  sessionPath,
                                                  &errorMessage),
               "session project should be created"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataCatalog()->RegisterEntry(
                   MakeEntry(QStringLiteral("session-image"),
                             QStringLiteral("Session Image"),
                             QStringLiteral("C:/data/session-image"),
                             xq::core::DataWorkflowRole::Image),
                   &errorMessage),
               "session catalog entry should register"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataHierarchy()->AddFolder(
                   QStringLiteral("session-images"),
                   context->DataHierarchy()->RootId(),
                   QStringLiteral("Session Images"),
                   &errorMessage),
               "session hierarchy folder should be added"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataHierarchy()->AddDataEntry(
                   QStringLiteral("session-image-node"),
                   QStringLiteral("session-images"),
                   QStringLiteral("session-image"),
                   QStringLiteral("Session Image"),
                   &errorMessage),
               "session hierarchy data node should be added"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->ProjectSession()->Save(&errorMessage),
               "ProjectSession save should persist hierarchy"))
    {
        delete context;
        return 1;
    }
    delete context;

    auto* reopenedContext = xq::core::ApplicationContext::CreateDefault();
    if (Expect(reopenedContext->ProjectSession()->Open(sessionPath,
                                                       &errorMessage),
               "ProjectSession open should restore hierarchy"))
    {
        delete reopenedContext;
        return 1;
    }
    const auto sessionChildren =
        reopenedContext->DataHierarchy()->ChildrenOf(
            QStringLiteral("session-images"));
    if (Expect(sessionChildren.size() == 1,
               "ProjectSession open should restore nested hierarchy nodes"))
    {
        delete reopenedContext;
        return 1;
    }
    if (Expect(sessionChildren.at(0).DataCatalogEntryId ==
                   QStringLiteral("session-image"),
               "ProjectSession open should restore hierarchy catalog references"))
    {
        delete reopenedContext;
        return 1;
    }

    delete reopenedContext;
    return 0;
}
