#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_TaskRunner.h"

#include <QCoreApplication>
#include <QString>

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

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::core::DataCatalogService catalog;
    xq::core::TaskRunner tasks;
    xq::core::DataImportService importer(catalog, tasks);

    xq::core::DataImportRequest imageRequest;
    imageRequest.SourcePath = QStringLiteral("C:/studies/cta");
    imageRequest.DisplayName = QStringLiteral("CTA Study");
    imageRequest.Modality = QStringLiteral("CT");
    imageRequest.WorkflowRole = xq::core::DataWorkflowRole::Image;

    QString errorMessage;
    const auto imageResult = importer.Import(imageRequest, &errorMessage);
    if (Expect(imageResult.Succeeded,
               "valid image import should succeed"))
        return 1;
    if (Expect(!imageResult.EntryId.isEmpty(),
               "valid image import should return a generated entry id"))
        return 1;
    if (Expect(catalog.Entries().size() == 1,
               "valid image import should register one catalog entry"))
        return 1;
    if (Expect(catalog.Entries().at(0).Id == imageResult.EntryId,
               "import result id should match registered catalog entry"))
        return 1;
    if (Expect(catalog.Entries().at(0).DisplayName ==
                   QStringLiteral("CTA Study"),
               "image import should preserve display name"))
        return 1;
    if (Expect(tasks.History().size() == 1,
               "valid image import should create one task history record"))
        return 1;
    if (Expect(tasks.History().at(0).Succeeded,
               "valid image import task should be successful"))
        return 1;

    xq::core::DataImportRequest explicitIdRequest;
    explicitIdRequest.RequestedId = QStringLiteral("dicom-explicit");
    explicitIdRequest.SourcePath = QStringLiteral("C:/studies/dicom");
    explicitIdRequest.DisplayName = QStringLiteral("DICOM Series");
    explicitIdRequest.Modality = QStringLiteral("MR");
    explicitIdRequest.WorkflowRole = xq::core::DataWorkflowRole::DICOMSeries;

    const auto explicitIdResult =
        importer.Import(explicitIdRequest, &errorMessage);
    if (Expect(explicitIdResult.Succeeded,
               "valid import with explicit id should succeed"))
        return 1;
    if (Expect(explicitIdResult.EntryId == QStringLiteral("dicom-explicit"),
               "valid import should preserve caller-provided id"))
        return 1;
    if (Expect(catalog.FindById(QStringLiteral("dicom-explicit")) != nullptr,
               "valid import with explicit id should be discoverable"))
        return 1;

    xq::core::DataImportRequest missingSourceRequest;
    missingSourceRequest.DisplayName = QStringLiteral("No Source");
    missingSourceRequest.WorkflowRole = xq::core::DataWorkflowRole::Image;

    errorMessage.clear();
    const auto missingSourceResult =
        importer.Import(missingSourceRequest, &errorMessage);
    if (Expect(!missingSourceResult.Succeeded,
               "import without a source path should fail"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("source")),
               "missing source failure should provide a useful error"))
        return 1;
    if (Expect(catalog.Entries().size() == 2,
               "failed missing-source import should not register data"))
        return 1;

    xq::core::DataImportRequest duplicateRequest = explicitIdRequest;
    duplicateRequest.SourcePath = QStringLiteral("C:/studies/duplicate");

    errorMessage.clear();
    const auto duplicateResult = importer.Import(duplicateRequest, &errorMessage);
    if (Expect(!duplicateResult.Succeeded,
               "duplicate requested id import should fail"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("Duplicate")),
               "duplicate import failure should expose catalog validation"))
        return 1;
    if (Expect(catalog.Entries().size() == 2,
               "failed duplicate import should not register data"))
        return 1;

    return 0;
}
