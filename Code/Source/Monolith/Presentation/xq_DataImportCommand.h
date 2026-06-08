#ifndef XQ_PRESENTATION_DATAIMPORTCOMMAND_H
#define XQ_PRESENTATION_DATAIMPORTCOMMAND_H

#include <QString>

namespace xq::core
{
class ApplicationContext;
}

namespace xq::presentation
{

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
        xq::core::ApplicationContext& context) = 0;
};

} // namespace xq::presentation

#endif // XQ_PRESENTATION_DATAIMPORTCOMMAND_H
