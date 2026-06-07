#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataImportService.h"

#include <QCoreApplication>

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

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    QString errorMessage;

    const auto firstResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-001"),
                                           QStringLiteral("CTA A")),
                                       &errorMessage);
    if (Expect(firstResult.Succeeded,
               "first context image import should succeed"))
    {
        delete context;
        return 1;
    }

    const auto rootChildren =
        context->DataHierarchy()->ChildrenOf(
            context->DataHierarchy()->RootId());
    if (Expect(rootChildren.size() == 1,
               "context image import should create one role folder"))
    {
        delete context;
        return 1;
    }
    if (Expect(rootChildren.at(0).Id == QStringLiteral("images"),
               "context image import should create the images folder"))
    {
        delete context;
        return 1;
    }
    if (Expect(rootChildren.at(0).Kind ==
                   xq::core::DataHierarchyNodeKind::Folder,
               "context image import role node should be a folder"))
    {
        delete context;
        return 1;
    }

    auto imageChildren =
        context->DataHierarchy()->ChildrenOf(QStringLiteral("images"));
    if (Expect(imageChildren.size() == 1,
               "context image import should add one data node"))
    {
        delete context;
        return 1;
    }
    if (Expect(imageChildren.at(0).Id == QStringLiteral("data-image-001"),
               "context image import should create stable data node id"))
    {
        delete context;
        return 1;
    }
    if (Expect(imageChildren.at(0).DataCatalogEntryId ==
                   QStringLiteral("image-001"),
               "context image import hierarchy node should reference catalog id"))
    {
        delete context;
        return 1;
    }
    if (Expect(imageChildren.at(0).DisplayName == QStringLiteral("CTA A"),
               "context image import hierarchy node should preserve display name"))
    {
        delete context;
        return 1;
    }

    const auto secondResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-002"),
                                           QStringLiteral("CTA B")),
                                       &errorMessage);
    if (Expect(secondResult.Succeeded,
               "second context image import should succeed"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataHierarchy()
                   ->ChildrenOf(context->DataHierarchy()->RootId())
                   .size() == 1,
               "second image import should reuse the images folder"))
    {
        delete context;
        return 1;
    }

    imageChildren =
        context->DataHierarchy()->ChildrenOf(QStringLiteral("images"));
    if (Expect(imageChildren.size() == 2,
               "second image import should add a second data node"))
    {
        delete context;
        return 1;
    }
    if (Expect(imageChildren.at(0).Id == QStringLiteral("data-image-001") &&
                   imageChildren.at(1).Id == QStringLiteral("data-image-002"),
               "image import hierarchy should preserve import order"))
    {
        delete context;
        return 1;
    }

    errorMessage.clear();
    const auto duplicateResult =
        context->DataImports()->Import(MakeImageImport(
                                           QStringLiteral("image-002"),
                                           QStringLiteral("Duplicate CTA")),
                                       &errorMessage);
    if (Expect(!duplicateResult.Succeeded,
               "duplicate context image import should fail"))
    {
        delete context;
        return 1;
    }
    if (Expect(errorMessage.contains(QStringLiteral("Duplicate")),
               "duplicate context image import should expose validation error"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataCatalog()->Entries().size() == 2,
               "duplicate context image import should not mutate catalog"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataHierarchy()
                   ->ChildrenOf(QStringLiteral("images"))
                   .size() == 2,
               "duplicate context image import should not mutate hierarchy"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
