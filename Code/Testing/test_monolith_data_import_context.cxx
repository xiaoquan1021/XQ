#include "Core/xq_ApplicationContext.h"
#include "Core/xq_DataCatalogService.h"
#include "Core/xq_DataImportService.h"
#include "Core/xq_TaskRunner.h"

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

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (Expect(context->DataImports() != nullptr,
               "ApplicationContext should expose DataImportService"))
    {
        delete context;
        return 1;
    }

    xq::core::DataImportRequest request;
    request.SourcePath = QStringLiteral("C:/studies/context-cta");
    request.DisplayName = QStringLiteral("Context CTA");
    request.Modality = QStringLiteral("CT");
    request.WorkflowRole = xq::core::DataWorkflowRole::Image;

    QString errorMessage;
    const auto result = context->DataImports()->Import(request, &errorMessage);
    if (Expect(result.Succeeded,
               "context data importer should import valid metadata"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataCatalog()->Entries().size() == 1,
               "context data importer should register in context catalog"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->DataCatalog()->Entries().at(0).Id == result.EntryId,
               "context catalog entry should match import result"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Tasks()->History().size() == 1,
               "context data importer should record task history"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Tasks()->History().at(0).Succeeded,
               "context data import task should succeed"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
