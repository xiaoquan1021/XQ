#include "Infrastructure/xq_MitkFileDataImportCommand.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataNodeRegistryService.h"
#include "Core/xq_DataSelectionService.h"

#include <QCoreApplication>

#include <mitkBaseData.h>
#include <mitkDataStorage.h>

#include <iostream>
#include <memory>

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

class FakePathProvider : public xq::core::FileImportPathProvider
{
public:
    QString NextPath;
    mutable int Invocations = 0;

    QString ChooseFilePath() const override
    {
        ++Invocations;
        return NextPath;
    }
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

int StorageNodeCount(mitk::DataStorage::Pointer storage)
{
    auto nodes = storage->GetAll();
    return nodes.IsNull() ? 0 : nodes->Size();
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakeReader reader;
        xq::infrastructure::MitkFileDataImportCommand command(nullptr,
                                                              &reader);

        const auto result = command.RunImport(*context);
        if (Expect(!result.Succeeded,
                   "file data import command should reject missing provider"))
            return 1;
        if (Expect(result.Message ==
                       QStringLiteral("File import path provider is required."),
                   "missing provider should use command diagnostic"))
            return 1;
        if (Expect(reader.LastPath.isEmpty(),
                   "missing provider should not invoke reader"))
            return 1;
        if (Expect(context->DataCatalog()->Entries().isEmpty(),
                   "missing provider should not mutate catalog"))
            return 1;
        if (Expect(StorageNodeCount(context->DataStorage()) == 0,
                   "missing provider should not mutate storage"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakePathProvider provider;
        FakeReader reader;
        xq::infrastructure::MitkFileDataImportCommand command(&provider,
                                                              &reader);

        const auto result = command.RunImport(*context);
        if (Expect(!result.Succeeded,
                   "cancelled file import command should not succeed"))
            return 1;
        if (Expect(result.Message == QStringLiteral("File import cancelled."),
                   "cancelled provider should use command diagnostic"))
            return 1;
        if (Expect(provider.Invocations == 1,
                   "command should invoke provider once"))
            return 1;
        if (Expect(reader.LastPath.isEmpty(),
                   "cancelled provider should not invoke reader"))
            return 1;
        if (Expect(context->DataCatalog()->Entries().isEmpty(),
                   "cancelled provider should not mutate catalog"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakePathProvider provider;
        provider.NextPath = QStringLiteral("C:/studies/cta-a.nii.gz");
        FakeReader reader;
        reader.NextResult.Message = QStringLiteral("Reader failed.");
        xq::infrastructure::MitkFileDataImportCommand command(&provider,
                                                              &reader);

        const auto result = command.RunImport(*context);
        if (Expect(!result.Succeeded,
                   "reader failure should fail file data import command"))
            return 1;
        if (Expect(result.Message == QStringLiteral("Reader failed."),
                   "reader failure should propagate service diagnostic"))
            return 1;
        if (Expect(reader.LastPath == provider.NextPath,
                   "reader should receive provider path"))
            return 1;
        if (Expect(context->DataCatalog()->Entries().isEmpty(),
                   "reader failure should not mutate catalog"))
            return 1;
        if (Expect(StorageNodeCount(context->DataStorage()) == 0,
                   "reader failure should not mutate storage"))
            return 1;
    }

    {
        std::unique_ptr<xq::core::ApplicationContext> context(
            xq::core::ApplicationContext::CreateDefault());
        FakePathProvider provider;
        provider.NextPath = QStringLiteral("C:/studies/cta-a.nii.gz");
        FakeReader reader;
        reader.NextResult.Succeeded = true;
        reader.NextResult.Data.push_back(MakeData());
        xq::infrastructure::MitkFileDataImportCommand command(&provider,
                                                              &reader);

        const auto result = command.RunImport(*context);
        if (Expect(result.Succeeded,
                   "valid file data import command should succeed"))
            return 1;
        if (Expect(result.CatalogEntryId == QStringLiteral("image-cta-a-nii-gz"),
                   "valid import should report generated catalog id"))
            return 1;
        const auto* entry =
            context->DataCatalog()->FindById(result.CatalogEntryId);
        if (Expect(entry != nullptr,
                   "valid import should register catalog metadata"))
            return 1;
        if (Expect(entry->DisplayName == QStringLiteral("cta-a.nii.gz"),
                   "valid import should use filename as display name"))
            return 1;
        if (Expect(entry->SourcePath == provider.NextPath,
                   "valid import should keep selected source path"))
            return 1;
        if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                       result.CatalogEntryId,
                   "valid import should select imported catalog entry"))
            return 1;
        if (Expect(StorageNodeCount(context->DataStorage()) == 1,
                   "valid import should add one MITK storage node"))
            return 1;
        if (Expect(context->DataNodes()
                       ->FindNode(result.CatalogEntryId)
                       .IsNotNull(),
                   "valid import should bind catalog id to storage node"))
            return 1;
        if (Expect(reader.LastPath == provider.NextPath,
                   "valid import should read selected path"))
            return 1;
    }

    return 0;
}
