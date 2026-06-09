#include "xq_QtProjectFilePathProvider.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QWidget>

namespace xq::presentation
{

QtProjectFilePathProvider::QtProjectFilePathProvider(QWidget* parent)
    : m_Parent(parent)
{
}

xq::core::ProjectFilePath QtProjectFilePathProvider::NewProjectFilePath()
{
    const QString projectFilePath = QFileDialog::getSaveFileName(
        m_Parent,
        QStringLiteral("New XQ Project"),
        QString(),
        QStringLiteral("XQ projects (*.xqproj);;All files (*)"));
    if (projectFilePath.trimmed().isEmpty())
        return {};

    const QString inferredName =
        QFileInfo(projectFilePath).completeBaseName().trimmed();
    bool accepted = false;
    const QString projectName = QInputDialog::getText(
        m_Parent,
        QStringLiteral("Project Name"),
        QStringLiteral("Project name:"),
        QLineEdit::Normal,
        inferredName,
        &accepted)
                                    .trimmed();
    if (!accepted || projectName.isEmpty())
        return {};

    return {projectName, projectFilePath};
}

QString QtProjectFilePathProvider::OpenProjectFilePath()
{
    return QFileDialog::getOpenFileName(
        m_Parent,
        QStringLiteral("Open XQ Project"),
        QString(),
        QStringLiteral("XQ projects (*.xqproj);;All files (*)"));
}

} // namespace xq::presentation
