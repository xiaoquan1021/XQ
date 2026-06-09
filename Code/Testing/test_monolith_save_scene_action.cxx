#include "Core/xq_ApplicationContext.h"
#include "Core/xq_SceneFilePathProvider.h"
#include "Core/xq_SceneExportService.h"
#include "Presentation/xq_MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <mitkDataNode.h>

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

class FakeScenePathProvider : public xq::core::SceneFilePathProvider
{
public:
    QString NextPath;
    int Requests = 0;

    QString SceneFilePath() override
    {
        ++Requests;
        return NextPath;
    }
};

class FakeSceneExporter : public xq::core::SceneExportService
{
public:
    QString LastPath;
    int Requests = 0;

    bool SaveScene(mitk::DataStorage::Pointer storage,
                   const QString& filePath,
                   QString* errorMessage) override
    {
        ++Requests;
        LastPath = filePath;
        if (storage.IsNull() || storage->GetAll()->empty())
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("No scene data.");
            return false;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly))
        {
            if (errorMessage)
                *errorMessage = file.errorString();
            return false;
        }
        file.write("fake scene");
        if (errorMessage)
            errorMessage->clear();
        return true;
    }
};

QAction* FindAction(xq::presentation::MainWindow& window,
                    const QString& objectName)
{
    return window.findChild<QAction*>(objectName);
}

} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    xq::presentation::MainWindow window(*context);
    FakeScenePathProvider pathProvider;
    FakeSceneExporter exporter;
    pathProvider.NextPath =
        QDir(tempDir.path()).filePath(QStringLiteral("study-scene.mitk"));
    window.SetSceneFilePathProvider(&pathProvider);
    window.SetSceneExportService(&exporter);

    auto node = mitk::DataNode::New();
    node->SetName("CTA A");
    context->DataStorage()->Add(node);

    auto* saveSceneAction =
        FindAction(window, QStringLiteral("xqSaveSceneAction"));
    if (Expect(saveSceneAction != nullptr,
               "Save MITK Scene action should exist"))
    {
        delete context;
        return 1;
    }

    QStringList diagnostics;
    QObject::connect(context,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&diagnostics](const QString& message) {
                         diagnostics.append(message);
                     });

    saveSceneAction->trigger();
    app.processEvents();

    if (Expect(pathProvider.Requests == 1,
               "Save MITK Scene should request an output file path"))
    {
        delete context;
        return 1;
    }
    if (Expect(exporter.Requests == 1 &&
                   exporter.LastPath == pathProvider.NextPath,
               "Save MITK Scene should call the scene export service"))
    {
        delete context;
        return 1;
    }
    QFile sceneFile(pathProvider.NextPath);
    if (Expect(sceneFile.exists() && sceneFile.size() > 0,
               "Save MITK Scene should create a non-empty scene file"))
    {
        delete context;
        return 1;
    }
    if (Expect(!diagnostics.contains(QStringLiteral(
                   "MITK scene export is not available in Windows monolith v1.")),
               "Save MITK Scene should no longer report unavailable"))
    {
        delete context;
        return 1;
    }
    if (Expect(diagnostics.contains(QStringLiteral("MITK scene saved.")),
               "Save MITK Scene should report a saved diagnostic"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
