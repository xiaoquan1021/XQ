#include "Infrastructure/xq_MitkSceneExportService.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <mitkDataNode.h>
#include <mitkStandaloneDataStorage.h>

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

} // namespace

int main(int, char**)
{
    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    xq::infrastructure::MitkSceneExportService service;
    QString message;

    const QString emptyPath =
        QDir(tempDir.path()).filePath(QStringLiteral("empty.mitk"));
    auto emptyStorage = mitk::StandaloneDataStorage::New();
    if (Expect(!service.SaveScene(emptyStorage.GetPointer(),
                                  emptyPath,
                                  &message) &&
                   message == QStringLiteral("No data to save as MITK scene."),
               "empty storage should fail with a deterministic diagnostic"))
        return 1;

    auto storage = mitk::StandaloneDataStorage::New();
    auto node = mitk::DataNode::New();
    node->SetName("Scene Node");
    storage->Add(node);

    const QString scenePath =
        QDir(tempDir.path()).filePath(QStringLiteral("study-scene.mitk"));
    if (Expect(service.SaveScene(storage.GetPointer(), scenePath, &message),
               "scene export service should save non-empty DataStorage"))
        return 1;

    QFile sceneFile(scenePath);
    if (Expect(sceneFile.exists() && sceneFile.size() > 0,
               "scene export service should create a non-empty .mitk file"))
        return 1;
    if (Expect(message.isEmpty(),
               "successful scene export should clear error message"))
        return 1;

    return 0;
}
