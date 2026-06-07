#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataManagementService.h"

#include <QCoreApplication>
#include <QObject>

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
                                     const QString& displayName)
{
    xq::core::DataCatalogEntry entry;
    entry.Id = id;
    entry.DisplayName = displayName;
    entry.SourcePath = QStringLiteral("C:/studies/") + id;
    entry.Modality = QStringLiteral("CT");
    entry.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return entry;
}

xq::core::DataImportRequest MakeImageImport(const QString& id,
                                            const QString& displayName)
{
    xq::core::DataImportRequest request;
    request.RequestedId = id;
    request.SourcePath = QStringLiteral("C:/studies/") + id;
    request.DisplayName = displayName;
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::core::DataCatalogService catalog;
    int catalogSignals = 0;
    QObject::connect(&catalog,
                     &xq::core::DataCatalogService::EntriesChanged,
                     [&catalogSignals]() {
                         ++catalogSignals;
                     });

    QString errorMessage;
    if (Expect(catalog.RegisterEntry(MakeEntry(QStringLiteral("image-001"),
                                               QStringLiteral("CTA A")),
                                     &errorMessage),
               "catalog register should succeed"))
    {
        return 1;
    }
    if (Expect(catalogSignals == 1,
               "successful catalog register should emit once"))
    {
        return 1;
    }

    if (Expect(!catalog.RegisterEntry(MakeEntry(QStringLiteral("image-001"),
                                                QStringLiteral("Duplicate")),
                                      &errorMessage),
               "duplicate catalog register should fail"))
    {
        return 1;
    }
    if (Expect(catalogSignals == 1,
               "failed catalog register should not emit"))
    {
        return 1;
    }

    xq::core::DataHierarchyService hierarchy;
    int hierarchySignals = 0;
    QObject::connect(&hierarchy,
                     &xq::core::DataHierarchyService::NodesChanged,
                     [&hierarchySignals]() {
                         ++hierarchySignals;
                     });

    if (Expect(hierarchy.AddFolder(QStringLiteral("images"),
                                   hierarchy.RootId(),
                                   QStringLiteral("Images"),
                                   &errorMessage),
               "hierarchy folder add should succeed"))
    {
        return 1;
    }
    if (Expect(hierarchySignals == 1,
               "successful hierarchy folder add should emit once"))
    {
        return 1;
    }

    if (Expect(hierarchy.AddDataEntry(QStringLiteral("data-image-001"),
                                      QStringLiteral("images"),
                                      QStringLiteral("image-001"),
                                      QStringLiteral("CTA A"),
                                      &errorMessage),
               "hierarchy data add should succeed"))
    {
        return 1;
    }
    if (Expect(hierarchySignals == 2,
               "successful hierarchy data add should emit once"))
    {
        return 1;
    }

    if (Expect(!hierarchy.AddDataEntry(QStringLiteral("data-image-001"),
                                       QStringLiteral("images"),
                                       QStringLiteral("image-001"),
                                       QStringLiteral("Duplicate"),
                                       &errorMessage),
               "duplicate hierarchy data add should fail"))
    {
        return 1;
    }
    if (Expect(hierarchySignals == 2,
               "failed hierarchy data add should not emit"))
    {
        return 1;
    }

    auto* context = xq::core::ApplicationContext::CreateDefault();
    int contextCatalogSignals = 0;
    int contextHierarchySignals = 0;
    QObject::connect(context->DataCatalog(),
                     &xq::core::DataCatalogService::EntriesChanged,
                     [&contextCatalogSignals]() {
                         ++contextCatalogSignals;
                     });
    QObject::connect(context->DataHierarchy(),
                     &xq::core::DataHierarchyService::NodesChanged,
                     [&contextHierarchySignals]() {
                         ++contextHierarchySignals;
                     });

    const auto importResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("context-image"),
                                           QStringLiteral("Context CTA")),
                                       &errorMessage);
    if (Expect(importResult.Succeeded,
               "context import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(contextCatalogSignals == 1,
               "context import should emit one live catalog change"))
    {
        delete context;
        return 1;
    }
    if (Expect(contextHierarchySignals == 1,
               "context import should emit one live hierarchy change"))
    {
        delete context;
        return 1;
    }

    const auto duplicateImport =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("context-image"),
                                           QStringLiteral("Duplicate CTA")),
                                       &errorMessage);
    if (Expect(!duplicateImport.Succeeded,
               "duplicate context import should fail"))
    {
        delete context;
        return 1;
    }
    if (Expect(contextCatalogSignals == 1,
               "failed context import should not emit catalog change"))
    {
        delete context;
        return 1;
    }
    if (Expect(contextHierarchySignals == 1,
               "failed context import should not emit hierarchy change"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataManagement()->RenameEntry(
                   QStringLiteral("context-image"),
                   QStringLiteral("Renamed Context CTA"),
                   &errorMessage),
               "context rename should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(contextCatalogSignals == 2,
               "rename should emit one live catalog change"))
    {
        delete context;
        return 1;
    }
    if (Expect(contextHierarchySignals == 2,
               "rename should emit one live hierarchy change"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataManagement()->RemoveEntry(
                   QStringLiteral("context-image"),
                   &errorMessage),
               "context remove should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(contextCatalogSignals == 3,
               "remove should emit one live catalog change"))
    {
        delete context;
        return 1;
    }
    if (Expect(contextHierarchySignals == 3,
               "remove should emit one live hierarchy change"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
