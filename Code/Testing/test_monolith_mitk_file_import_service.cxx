#include "Infrastructure/xq_MitkFileImportService.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataHierarchyService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"

#include <QCoreApplication>

#include <mitkBaseData.h>
#include <mitkDataNode.h>
#include <mitkDataStorage.h>

#include <iostream>
#include <memory>
#include <vector>

namespace
{

class FakeBaseData : public mitk::BaseData
{
public:
    mitkClassMacro(FakeBaseData, mitk::BaseData);
    itkFactorylessNewMacro(Self);
    itkCloneMacro(Self);

    void SetRequestedRegionToLargestPossibleRegion() override {}
    bool RequestedRegionIsOutsideOfTheBufferedRegion() override
    {
        return false;
    }
    bool VerifyRequestedRegion() override { return true; }
    void SetRequestedRegion(const itk::DataObject*) override {}

protected:
    FakeBaseData() = default;
    ~FakeBaseData() override = default;
};

class FakeReader : public xq::infrastructure::MitkFileReader
{
public:
    xq::infrastructure::MitkFileReaderResult NextResult;
    mutable QString LastPath;

    xq::infrastructure::MitkFileReaderResult Load(
        const QString& sourcePath) const override
    {
        LastPath = sourcePath;
        return NextResult;
    }
};

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

mitk::BaseData::Pointer MakeData()
{
    return FakeBaseData::New().GetPointer();
}

xq::core::DataImportRequest MakeImport(const QString& id,
                                       const QString& sourcePath)
{
    xq::core::DataImportRequest request;
    request.RequestedId = id;
    request.SourcePath = sourcePath;
    request.DisplayName = QStringLiteral("CTA Image");
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;
    return request;
}

xq::infrastructure::MitkFileImportRequest MakeRequest(
    xq::core::ApplicationContext& context,
    const QString& id = QStringLiteral("image-001"),
    const QString& sourcePath = QStringLiteral("C:/studies/image-001.nii"))
{
    xq::infrastructure::MitkFileImportRequest request;
    request.DataStorage = context.DataStorage();
    request.DataImports = context.DataImports();
    request.DataNodes = context.DataNodes();
    request.Import = MakeImport(id, sourcePath);
    request.Reader = nullptr;
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

bool HasCatalogEntry(xq::core::ApplicationContext& context,
                     const QString& id)
{
    return context.DataCatalog()->FindById(id) != nullptr;
}

bool HasHierarchyEntry(xq::core::ApplicationContext& context,
                       const QString& id)
{
    return context.DataHierarchy()->FindNode(QStringLiteral("data-%1").arg(id)) !=
           nullptr;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::infrastructure::MitkFileImportService service;

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakeReader reader;
        auto request = MakeRequest(*context);
        request.Reader = &reader;
        request.DataStorage = nullptr;

        const auto result = service.Import(request);
        if (Expect(!result.Succeeded,
                   "file import should reject missing storage"))
            return 1;
        if (Expect(result.Message ==
                       QStringLiteral("MITK file import storage is required."),
                   "missing storage should use service diagnostic"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakeReader reader;
        auto request = MakeRequest(*context);
        request.Reader = &reader;
        request.DataImports = nullptr;

        const auto result = service.Import(request);
        if (Expect(!result.Succeeded,
                   "file import should reject missing import service"))
            return 1;
        if (Expect(result.Message ==
                       QStringLiteral("Data import service is required."),
                   "missing import service should use service diagnostic"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakeReader reader;
        auto request = MakeRequest(*context);
        request.Reader = &reader;
        request.DataNodes = nullptr;

        const auto result = service.Import(request);
        if (Expect(!result.Succeeded,
                   "file import should reject missing node registry"))
            return 1;
        if (Expect(result.Message ==
                       QStringLiteral("Data node registry is required."),
                   "missing node registry should use service diagnostic"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakeReader reader;
        auto request = MakeRequest(*context);
        request.Reader = &reader;
        request.Import.SourcePath.clear();

        const auto result = service.Import(request);
        if (Expect(!result.Succeeded,
                   "file import should reject missing source path"))
            return 1;
        if (Expect(result.Message ==
                       QStringLiteral("MITK file import source path is required."),
                   "missing source path should use service diagnostic"))
            return 1;
        if (Expect(reader.LastPath.isEmpty(),
                   "missing source path should not invoke reader"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakeReader reader;
        reader.NextResult.Message = QStringLiteral("Reader failed.");
        auto request = MakeRequest(*context);
        request.Reader = &reader;
        const int storageBeforeImport = StorageNodeCount(context->DataStorage());

        const auto result = service.Import(request);
        if (Expect(!result.Succeeded,
                   "reader failure should fail file import"))
            return 1;
        if (Expect(result.Message == QStringLiteral("Reader failed."),
                   "reader failure should preserve reader diagnostic"))
            return 1;
        if (Expect(reader.LastPath == request.Import.SourcePath,
                   "reader should receive source path"))
            return 1;
        if (Expect(StorageNodeCount(context->DataStorage()) ==
                       storageBeforeImport,
                   "reader failure should not mutate DataStorage"))
            return 1;
        if (Expect(!HasCatalogEntry(*context, QStringLiteral("image-001")),
                   "reader failure should not register catalog metadata"))
            return 1;
        if (Expect(context->DataNodes()
                       ->FindNode(QStringLiteral("image-001"))
                       .IsNull(),
                   "reader failure should not bind data node"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakeReader reader;
        reader.NextResult.Succeeded = true;
        auto request = MakeRequest(*context);
        request.Reader = &reader;

        const auto result = service.Import(request);
        if (Expect(!result.Succeeded,
                   "empty reader output should fail file import"))
            return 1;
        if (Expect(result.Message ==
                       QStringLiteral("MITK file import produced no data."),
                   "empty reader output should use service diagnostic"))
            return 1;
        if (Expect(StorageNodeCount(context->DataStorage()) == 0,
                   "empty reader output should not mutate DataStorage"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakeReader reader;
        reader.NextResult.Succeeded = true;
        reader.NextResult.Data.push_back(MakeData());
        reader.NextResult.Data.push_back(MakeData());
        auto request = MakeRequest(*context);
        request.Reader = &reader;

        const auto result = service.Import(request);
        if (Expect(!result.Succeeded,
                   "multi-object reader output should fail this service slice"))
            return 1;
        if (Expect(result.Message ==
                       QStringLiteral("MITK file import expects one data object."),
                   "multi-object output should use service diagnostic"))
            return 1;
        if (Expect(StorageNodeCount(context->DataStorage()) == 0,
                   "multi-object output should not mutate DataStorage"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakeReader reader;
        reader.NextResult.Succeeded = true;
        reader.NextResult.Data.push_back(MakeData());
        auto firstRequest = MakeRequest(*context);
        firstRequest.Reader = &reader;
        const auto firstResult = service.Import(firstRequest);
        if (Expect(firstResult.Succeeded,
                   "first file import should succeed"))
            return 1;

        FakeReader duplicateReader;
        duplicateReader.NextResult.Succeeded = true;
        duplicateReader.NextResult.Data.push_back(MakeData());
        auto duplicateRequest = MakeRequest(*context);
        duplicateRequest.Reader = &duplicateReader;
        const int storageBeforeDuplicate =
            StorageNodeCount(context->DataStorage());

        const auto duplicateResult = service.Import(duplicateRequest);
        if (Expect(!duplicateResult.Succeeded,
                   "duplicate metadata import should fail file import"))
            return 1;
        if (Expect(duplicateResult.Message ==
                       QStringLiteral("Duplicate hierarchy node id."),
                   "duplicate metadata import should preserve diagnostic"))
            return 1;
        if (Expect(StorageNodeCount(context->DataStorage()) ==
                       storageBeforeDuplicate,
                   "duplicate metadata import should not add storage nodes"))
            return 1;
        if (Expect(context->DataNodes()
                       ->FindNode(QStringLiteral("image-001"))
                       .GetPointer() == firstResult.Node.GetPointer(),
                   "duplicate metadata import should not rebind data node"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakeReader reader;
        reader.NextResult.Succeeded = true;
        reader.NextResult.Data.push_back(MakeData());
        auto request = MakeRequest(*context);
        request.Reader = &reader;

        const auto result = service.Import(request);
        if (Expect(result.Succeeded,
                   "valid file import should succeed"))
            return 1;
        if (Expect(result.CatalogEntryId == QStringLiteral("image-001"),
                   "valid file import should report catalog id"))
            return 1;
        if (Expect(result.Node.IsNotNull(),
                   "valid file import should return imported node"))
            return 1;
        if (Expect(result.Node->GetData() ==
                       reader.NextResult.Data.front().GetPointer(),
                   "valid file import should wrap loaded data in node"))
            return 1;
        if (Expect(result.Node->GetName() == std::string("CTA Image"),
                   "valid file import should name node from display name"))
            return 1;
        if (Expect(StorageContains(context->DataStorage(), result.Node),
                   "valid file import should add node to DataStorage"))
            return 1;
        if (Expect(HasCatalogEntry(*context, QStringLiteral("image-001")),
                   "valid file import should register catalog metadata"))
            return 1;
        if (Expect(HasHierarchyEntry(*context, QStringLiteral("image-001")),
                   "valid file import should register hierarchy metadata"))
            return 1;
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       QStringLiteral("image-001"),
                   "valid file import should select imported entry"))
            return 1;
        if (Expect(context->DataNodes()
                       ->FindNode(QStringLiteral("image-001"))
                       .GetPointer() == result.Node.GetPointer(),
                   "valid file import should bind catalog id to node"))
            return 1;
    }

    return 0;
}
