#ifndef XQ_PRESENTATION_QTFILEIMPORTPATHPROVIDER_H
#define XQ_PRESENTATION_QTFILEIMPORTPATHPROVIDER_H

#include "Core/xq_DataImportCommand.h"

#include <QString>

class QWidget;

namespace xq::presentation
{

class QtFileImportPathProvider : public xq::core::FileImportPathProvider
{
public:
    explicit QtFileImportPathProvider(QWidget* parent = nullptr);

    QString ChooseFilePath() const override;
    QString DialogCaption() const;
    QString FileFilter() const;

private:
    QWidget* m_Parent = nullptr;
};

} // namespace xq::presentation

#endif // XQ_PRESENTATION_QTFILEIMPORTPATHPROVIDER_H
