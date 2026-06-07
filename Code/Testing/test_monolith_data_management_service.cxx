#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataManagementService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_TaskRunner.h"

#include <QCoreApplication>

#include <mitkDataNode.h>

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

mitk::DataNode::Pointer MakeNode(const std::string& name)
{
    auto node = mitk::DataNode::New();
    node->SetName(name);
    return node;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (Expect(context->DataManagement() != nullptr,
               "ApplicationContext should expose DataManagementService"))
    {
        delete context;
        return 1;
    }

    QString errorMessage;
    const auto firstImport =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("CTA A")),
                                       &errorMessage);
    if (Expect(firstImport.Succeeded, "first image import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                   QStringLiteral("image-001"),
               "imported entry should start selected"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataManagement()->RenameEntry(
                   QStringLiteral("image-001"),
                   QStringLiteral("Renamed CTA"),
                   &errorMessage),
               "renaming an imported entry should succeed"))
    {
        delete context;
        return 1;
    }

    const auto* renamedEntry =
        context->DataCatalog()->FindById(QStringLiteral("image-001"));
    if (Expect(renamedEntry != nullptr,
               "renamed entry should remain in the catalog"))
    {
        delete context;
        return 1;
    }
    if (Expect(renamedEntry->DisplayName == QStringLiteral("Renamed CTA"),
               "rename should update catalog display name"))
    {
        delete context;
        return 1;
    }

    const auto imageChildren =
        context->DataHierarchy()->ChildrenOf(QStringLiteral("images"));
    if (Expect(imageChildren.size() == 1,
               "rename should preserve one hierarchy data node"))
    {
        delete context;
        return 1;
    }
    if (Expect(imageChildren.at(0).DisplayName ==
                   QStringLiteral("Renamed CTA"),
               "rename should update hierarchy display name"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                   QStringLiteral("image-001"),
               "rename should preserve selected catalog id"))
    {
        delete context;
        return 1;
    }
    auto boundNode = MakeNode("CTA node");
    if (Expect(context->DataNodes()->BindNode(QStringLiteral("image-001"),
                                              boundNode,
                                              &errorMessage),
               "test fixture should bind data node before removal"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Tasks()->History().size() == 2,
               "rename should record a task after import"))
    {
        delete context;
        return 1;
    }

    errorMessage.clear();
    if (Expect(!context->DataManagement()->RenameEntry(
                    QStringLiteral("image-001"),
                    QStringLiteral("   "),
                    &errorMessage),
               "empty rename should fail"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataCatalog()
                   ->FindById(QStringLiteral("image-001"))
                   ->DisplayName == QStringLiteral("Renamed CTA"),
               "failed rename should not mutate catalog display name"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataHierarchy()
                   ->ChildrenOf(QStringLiteral("images"))
                   .at(0)
                   .DisplayName == QStringLiteral("Renamed CTA"),
               "failed rename should not mutate hierarchy display name"))
    {
        delete context;
        return 1;
    }

    if (Expect(context->DataManagement()->RemoveEntry(
                   QStringLiteral("image-001"),
                   &errorMessage),
               "removing selected data should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataCatalog()->Entries().isEmpty(),
               "remove should delete the catalog entry"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataHierarchy()
                   ->ChildrenOf(QStringLiteral("images"))
                   .isEmpty(),
               "remove should delete hierarchy data nodes"))
    {
        delete context;
        return 1;
    }
    if (Expect(!context->DataSelection()->HasSelection(),
               "remove should clear selection for deleted data"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataNodes()
                   ->FindNode(QStringLiteral("image-001"))
                   .IsNull(),
               "remove should clear data node registry binding"))
    {
        delete context;
        return 1;
    }

    const auto secondImport =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-002"),
                                           QStringLiteral("CTA B")),
                                       &errorMessage);
    if (Expect(secondImport.Succeeded, "second image import should succeed"))
    {
        delete context;
        return 1;
    }
    auto secondBoundNode = MakeNode("CTA B node");
    if (Expect(context->DataNodes()->BindNode(QStringLiteral("image-002"),
                                              secondBoundNode,
                                              &errorMessage),
               "test fixture should bind second data node before failed remove"))
    {
        delete context;
        return 1;
    }
    errorMessage.clear();
    if (Expect(!context->DataManagement()->RemoveEntry(
                    QStringLiteral("missing-image"),
                    &errorMessage),
               "removing missing data should fail"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataCatalog()->Entries().size() == 1,
               "failed remove should not mutate catalog entries"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                   QStringLiteral("image-002"),
               "failed remove should preserve current selection"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataNodes()
                   ->FindNode(QStringLiteral("image-002"))
                   .GetPointer() == secondBoundNode.GetPointer(),
               "failed remove should preserve data node registry binding"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
