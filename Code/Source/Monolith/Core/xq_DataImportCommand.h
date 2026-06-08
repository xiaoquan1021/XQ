#ifndef XQ_CORE_DATAIMPORTCOMMAND_H
#define XQ_CORE_DATAIMPORTCOMMAND_H

#include <QString>

namespace xq::core
{

class ApplicationContext;

class FileImportPathProvider
{
public:
    virtual ~FileImportPathProvider() = default;

    virtual QString ChooseFilePath() const = 0;
};

struct DataImportCommandResult
{
    bool Succeeded = false;
    QString CatalogEntryId;
    QString Message;
};

class DataImportCommand
{
public:
    virtual ~DataImportCommand() = default;

    virtual DataImportCommandResult RunImport(
        ApplicationContext& context) = 0;
};

} // namespace xq::core

#endif // XQ_CORE_DATAIMPORTCOMMAND_H
