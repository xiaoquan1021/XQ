#include "Infrastructure/xq_DicomImportCommand.h"

#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataSelectionService.h"

#include <QCoreApplication>
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

class FakeDicomPathProvider : public xq::core::FileImportPathProvider
{
public:
    QString NextPath;
    mutable int Requests = 0;

    QString ChooseFilePath() const override
    {
        ++Requests;
        return NextPath;
    }
};

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    FakeDicomPathProvider provider;
    xq::infrastructure::DicomImportCommand command(&provider);

    const auto cancelled = command.RunImport(*context);
    if (Expect(!cancelled.Succeeded &&
                   cancelled.Message == QStringLiteral("DICOM import cancelled."),
               "empty DICOM path should cancel deterministically"))
    {
        delete context;
        return 1;
    }
    if (Expect(provider.Requests == 1,
               "DICOM command should request a path for cancellation"))
    {
        delete context;
        return 1;
    }

    QTemporaryDir dicomDir;
    if (Expect(dicomDir.isValid(), "temporary DICOM directory should exist"))
    {
        delete context;
        return 1;
    }

    provider.NextPath = dicomDir.path();
    const auto imported = command.RunImport(*context);
    if (Expect(imported.Succeeded,
               "DICOM directory import should register metadata"))
    {
        delete context;
        return 1;
    }
    const auto* entry = context->DataCatalog()->FindById(imported.CatalogEntryId);
    if (Expect(entry != nullptr &&
                   entry->WorkflowRole ==
                       xq::core::DataWorkflowRole::DICOMSeries,
               "DICOM import command should register a DICOMSeries entry"))
    {
        delete context;
        return 1;
    }
    if (Expect(entry->SourcePath == dicomDir.path(),
               "DICOM import command should keep selected source path"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataSelection()->SelectedCatalogEntryId() ==
                   imported.CatalogEntryId,
               "DICOM import command should select imported metadata"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataStorage()->GetAll()->empty(),
               "metadata-only DICOM import should not create fake MITK image data"))
    {
        delete context;
        return 1;
    }
    if (Expect(imported.Message ==
                   QStringLiteral("Imported data catalog entry."),
               "DICOM import command should use DataImportService success message"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
