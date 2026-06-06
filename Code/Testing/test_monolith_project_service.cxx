#include "Core/xq_ApplicationContext.h"
#include "Core/xq_ProjectService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTemporaryDir>

#include <iostream>

namespace
{

int Expect(bool condition, const char* message)
{
    if (condition)
        return 0;

    std::cerr << message << '\n';
    return 1;
}

QJsonObject ReadJsonObject(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    const auto document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject())
        return {};

    return document.object();
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::core::ProjectService service;
    if (Expect(!service.HasActiveProject(),
               "new ProjectService should not have an active project"))
        return 1;
    if (Expect(service.CurrentProject() == nullptr,
               "new ProjectService should not expose project metadata"))
        return 1;

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    const QString projectPath =
        QDir(tempDir.path()).filePath(QStringLiteral("HeartStudy.xqproj"));

    QString errorMessage;
    if (Expect(service.CreateProject(QStringLiteral("HeartStudy"),
                                     projectPath,
                                     &errorMessage),
               "CreateProject should accept a valid schema 2.0 project"))
        return 1;
    if (Expect(service.HasActiveProject(),
               "CreateProject should activate project metadata"))
        return 1;
    if (Expect(service.CurrentProject()->SchemaVersion ==
                   QStringLiteral("2.0"),
               "new projects should use schema 2.0"))
        return 1;
    if (Expect(service.CurrentProject()->WorkspaceDirectory ==
                   QStringLiteral("workspace"),
               "new projects should use a relative workspace directory"))
        return 1;
    if (Expect(service.SaveProject(&errorMessage),
               "SaveProject should write the active project"))
        return 1;

    const QJsonObject saved = ReadJsonObject(projectPath);
    if (Expect(saved.value(QStringLiteral("schemaVersion")).toString() ==
                   QStringLiteral("2.0"),
               "saved .xqproj should declare schemaVersion 2.0"))
        return 1;
    const QJsonObject savedProject =
        saved.value(QStringLiteral("project")).toObject();
    if (Expect(savedProject.value(QStringLiteral("name")).toString() ==
                   QStringLiteral("HeartStudy"),
               "saved .xqproj should preserve the project name"))
        return 1;
    if (Expect(savedProject.value(QStringLiteral("workspaceDirectory")).toString() ==
                   QStringLiteral("workspace"),
               "saved .xqproj should preserve the relative workspace directory"))
        return 1;

    xq::core::ProjectService openedService;
    if (Expect(openedService.OpenProject(projectPath, &errorMessage),
               "OpenProject should read a schema 2.0 project"))
        return 1;
    if (Expect(openedService.CurrentProject() != nullptr,
               "OpenProject should activate project metadata"))
        return 1;
    if (Expect(openedService.CurrentProject()->Name == QStringLiteral("HeartStudy"),
               "OpenProject should restore the project name"))
        return 1;
    if (Expect(openedService.CurrentProject()->ProjectFilePath == projectPath,
               "OpenProject should restore the project file path"))
        return 1;

    const QString legacyPath =
        QDir(tempDir.path()).filePath(QStringLiteral("Legacy.xqproj"));
    QFile legacyFile(legacyPath);
    if (Expect(legacyFile.open(QIODevice::WriteOnly),
               "legacy project fixture should be writable"))
        return 1;
    legacyFile.write(R"({"schemaVersion":"1.0","project":{"name":"Legacy"}})");
    legacyFile.close();

    errorMessage.clear();
    if (Expect(!openedService.OpenProject(legacyPath, &errorMessage),
               "OpenProject should reject unsupported schemas"))
        return 1;
    if (Expect(errorMessage.contains(QStringLiteral("Unsupported")),
               "unsupported schema should produce a useful error"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (Expect(context->Projects() != nullptr,
               "ApplicationContext should expose ProjectService"))
    {
        delete context;
        return 1;
    }
    if (Expect(!context->Projects()->HasActiveProject(),
               "ApplicationContext ProjectService should start empty"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
