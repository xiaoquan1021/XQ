#include "Infrastructure/xq_DataNodeImportService.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"
#include "Core/xq_TaskRunner.h"

#include <QCoreApplication>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <iostream>
#include <memory>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

mitk::DataNode::Pointer MakeNode(const std::string& name)
{
    auto node = mitk::DataNode::New();
    node->SetName(name);
    return node;
}

xq::core::DataImportRequest MakeImport()
{
    xq::core::DataImportRequest request;
    request.RequestedId = QStringLiteral("image-001");
    request.SourcePath = QStringLiteral("C:/studies/image-001.nii");
    request.DisplayName = QStringLiteral("CTA Image");
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

xq::infrastructure::DataNodeImportRequest MakeRequest(
    xq::core::ApplicationContext& context,
    mitk::DataNode::Pointer node)
{
    xq::infrastructure::DataNodeImportRequest request;
    request.DataStorage = context.DataStorage();
    request.DataImports = context.DataImports();
    request.DataNodes = context.DataNodes();
    request.Import = MakeImport();
    request.Node = node;
    return request;
}

int StorageNodeCount(mitk::DataStorage::Pointer storage)
{
    auto nodes = storage->GetAll();
    return nodes.IsNull() ? 0 : nodes->Size();
}

bool StorageContains(mitk::DataStorage::Pointer storage,
                     mitk::DataNode::Pointer node)
{
    auto nodes = storage->GetAll();
    if (nodes.IsNull())
        return false;

    for (auto it = nodes->Begin(); it != nodes->End(); ++it)
    {
        if (it->Value().GetPointer() == node.GetPointer())
            return true;
    }

    return false;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::infrastructure::DataNodeImportService service;

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        auto request = MakeRequest(*context, MakeNode("CTA"));
        request.DataStorage = nullptr;

        const auto result = service.Import(request);
        if (Expect(!result.Succeeded,
                   "data node import should reject missing storage"))
            return 1;
        if (Expect(result.Message ==
                       QStringLiteral("Data node import storage is required."),
                   "missing storage should use service diagnostic"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        auto request = MakeRequest(*context, MakeNode("CTA"));
        request.DataImports = nullptr;

        const auto result = service.Import(request);
        if (Expect(!result.Succeeded,
                   "data node import should reject missing import service"))
            return 1;
        if (Expect(result.Message ==
                       QStringLiteral("Data import service is required."),
                   "missing import service should use service diagnostic"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        auto request = MakeRequest(*context, MakeNode("CTA"));
        request.DataNodes = nullptr;

        const auto result = service.Import(request);
        if (Expect(!result.Succeeded,
                   "data node import should reject missing node registry"))
            return 1;
        if (Expect(result.Message ==
                       QStringLiteral("Data node registry is required."),
                   "missing node registry should use service diagnostic"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        auto request = MakeRequest(*context, nullptr);

        const auto result = service.Import(request);
        if (Expect(!result.Succeeded,
                   "data node import should reject missing nodes"))
            return 1;
        if (Expect(result.Message == QStringLiteral("Data node is required."),
                   "missing node should use service diagnostic"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        auto request = MakeRequest(*context, MakeNode("CTA"));
        request.Import.SourcePath.clear();
        const int storageBeforeImport = StorageNodeCount(context->DataStorage());

        const auto result = service.Import(request);
        if (Expect(!result.Succeeded,
                   "failed metadata import should fail data node import"))
            return 1;
        if (Expect(result.Message ==
                       QStringLiteral("Import source path is required."),
                   "metadata import failure should preserve diagnostic"))
            return 1;
        if (Expect(StorageNodeCount(context->DataStorage()) ==
                       storageBeforeImport,
                   "failed metadata import should not mutate DataStorage"))
            return 1;
        if (Expect(context->DataNodes()
                       ->FindNode(QStringLiteral("image-001"))
                       .IsNull(),
                   "failed metadata import should not bind data node"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        auto node = MakeNode("CTA");
        auto request = MakeRequest(*context, node);

        const auto result = service.Import(request);
        if (Expect(result.Succeeded,
                   "valid data node import should succeed"))
            return 1;
        if (Expect(result.CatalogEntryId == QStringLiteral("image-001"),
                   "valid data node import should report catalog id"))
            return 1;
        if (Expect(result.Node.GetPointer() == node.GetPointer(),
                   "valid data node import should return imported node"))
            return 1;
        if (Expect(StorageContains(context->DataStorage(), node),
                   "valid data node import should add node to DataStorage"))
            return 1;
        if (Expect(context->DataCatalog()->FindById(
                       QStringLiteral("image-001")) != nullptr,
                   "valid data node import should register catalog metadata"))
            return 1;
        if (Expect(context->DataHierarchy()->FindNode(
                       QStringLiteral("data-image-001")) != nullptr,
                   "valid data node import should register hierarchy metadata"))
            return 1;
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("image-001"),
                   "valid data node import should select imported entry"))
            return 1;
        if (Expect(context->DataNodes()
                       ->FindNode(QStringLiteral("image-001"))
                       .GetPointer() == node.GetPointer(),
                   "valid data node import should bind catalog id to node"))
            return 1;
    }

    return 0;
}
