#ifndef XQ_INFRASTRUCTURE_DICOMIMPORTCOMMAND_H
#define XQ_INFRASTRUCTURE_DICOMIMPORTCOMMAND_H

#include "Core/xq_DataImportCommand.h"

namespace xq::infrastructure
{

class DicomImportCommand : public xq::core::DataImportCommand
{
public:
    explicit DicomImportCommand(
        const xq::core::FileImportPathProvider* pathProvider);

    xq::core::DataImportCommandResult RunImport(
        xq::core::ApplicationContext& context) override;

private:
    const xq::core::FileImportPathProvider* m_PathProvider = nullptr;
};

} // namespace xq::infrastructure

#endif // XQ_INFRASTRUCTURE_DICOMIMPORTCOMMAND_H
