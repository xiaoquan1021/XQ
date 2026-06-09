#ifndef XQ_PRESENTATION_QTPROJECTFILEPATHPROVIDER_H
#define XQ_PRESENTATION_QTPROJECTFILEPATHPROVIDER_H

#include "Core/xq_ProjectFilePathProvider.h"

class QWidget;

namespace xq::presentation
{

class QtProjectFilePathProvider : public xq::core::ProjectFilePathProvider
{
public:
    explicit QtProjectFilePathProvider(QWidget* parent = nullptr);

    xq::core::ProjectFilePath NewProjectFilePath() override;
    QString OpenProjectFilePath() override;

private:
    QWidget* m_Parent = nullptr;
};

} // namespace xq::presentation

#endif // XQ_PRESENTATION_QTPROJECTFILEPATHPROVIDER_H
