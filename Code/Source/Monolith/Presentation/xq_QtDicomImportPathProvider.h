#ifndef XQ_PRESENTATION_QTDICOMIMPORTPATHPROVIDER_H
#define XQ_PRESENTATION_QTDICOMIMPORTPATHPROVIDER_H

#include "Core/xq_DataImportCommand.h"

#include <QString>

class QWidget;

namespace xq::presentation
{

class QtDicomImportPathProvider : public xq::core::FileImportPathProvider
{
public:
    explicit QtDicomImportPathProvider(QWidget* parent = nullptr);

    QString ChooseFilePath() const override;
    QString DialogCaption() const;

private:
    QWidget* m_Parent = nullptr;
};

} // namespace xq::presentation

#endif // XQ_PRESENTATION_QTDICOMIMPORTPATHPROVIDER_H
