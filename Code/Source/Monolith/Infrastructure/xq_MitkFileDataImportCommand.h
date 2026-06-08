#ifndef XQ_INFRASTRUCTURE_MITKFILEDATAIMPORTCOMMAND_H
#define XQ_INFRASTRUCTURE_MITKFILEDATAIMPORTCOMMAND_H

#include "Core/xq_DataImportCommand.h"
#include "xq_MitkFileImportService.h"

#include <QString>

namespace xq::infrastructure
{

class FileImportPathProvider
{
public:
    virtual ~FileImportPathProvider() = default;

    virtual QString ChooseFilePath() const = 0;
};

class MitkFileDataImportCommand : public xq::core::DataImportCommand
{
public:
    explicit MitkFileDataImportCommand(
        const FileImportPathProvider* pathProvider,
        const MitkFileReader* reader = nullptr);

    xq::core::DataImportCommandResult RunImport(
        xq::core::ApplicationContext& context) override;

private:
    const FileImportPathProvider* m_PathProvider = nullptr;
    const MitkFileReader* m_Reader = nullptr;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_MITKFILEDATAIMPORTCOMMAND_H
