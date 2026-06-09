#include "xq_QtProjectFilePathProvider.h"

#include "Core/xq_ProjectService.h"

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

xq::core::ProjectFilePath QtProjectFilePathProvider::SaveAsProjectFilePath(
    const xq::core::ProjectMetadata& currentProject)
{
    const QString projectFilePath = QFileDialog::getSaveFileName(
        m_Parent,
        QStringLiteral("Save XQ Project As"),
        currentProject.ProjectFilePath,
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
        inferredName.isEmpty() ? currentProject.Name : inferredName,
        &accepted)
                                    .trimmed();
    if (!accepted || projectName.isEmpty())
        return {};

    return {projectName, projectFilePath};
}

} // namespace xq::presentation
