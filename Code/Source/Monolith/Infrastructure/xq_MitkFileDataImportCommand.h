#ifndef XQ_INFRASTRUCTURE_MITKFILEDATAIMPORTCOMMAND_H
#define XQ_INFRASTRUCTURE_MITKFILEDATAIMPORTCOMMAND_H

#include "Core/xq_DataImportCommand.h"
#include "Core/xq_RenderRefreshService.h"
#include "xq_MitkFileImportService.h"

#include <QString>

namespace xq::infrastructure
{

class MitkFileDataImportCommand : public xq::core::DataImportCommand
{
public:
    explicit MitkFileDataImportCommand(
        const xq::core::FileImportPathProvider* pathProvider,
        const MitkFileReader* reader = nullptr,
        xq::core::RenderRefreshService* renderRefresh = nullptr);

    xq::core::DataImportCommandResult RunImport(
        xq::core::ApplicationContext& context) override;

private:
    const xq::core::FileImportPathProvider* m_PathProvider = nullptr;
    const MitkFileReader* m_Reader = nullptr;
    xq::core::RenderRefreshService* m_RenderRefresh = nullptr;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_MITKFILEDATAIMPORTCOMMAND_H
