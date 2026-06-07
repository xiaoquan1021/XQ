#include "Core/xq_ApplicationContext.h"
#include "Core/xq_PreferencesService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    xq::core::PreferencesService preferences;
    if (Expect(preferences.Keys().isEmpty(),
               "new PreferencesService should start empty"))
        return 1;
    if (Expect(preferences.StringValue(QStringLiteral("missing"),
                                       QStringLiteral("fallback")) ==
                   QStringLiteral("fallback"),
               "missing string preference should return fallback"))
        return 1;
    if (Expect(preferences.BoolValue(QStringLiteral("missing"), true),
               "missing bool preference should return fallback"))
        return 1;
    if (Expect(preferences.IntValue(QStringLiteral("missing"), 42) == 42,
               "missing int preference should return fallback"))
        return 1;

    preferences.SetStringValue(QStringLiteral("ui.theme"),
                               QStringLiteral("system"));
    preferences.SetBoolValue(QStringLiteral("render.showAxes"), true);
    preferences.SetIntValue(QStringLiteral("diagnostics.maxItems"), 250);

    if (Expect(preferences.StringValue(QStringLiteral("ui.theme")) ==
                   QStringLiteral("system"),
               "string preference should roundtrip in memory"))
        return 1;
    if (Expect(preferences.BoolValue(QStringLiteral("render.showAxes")),
               "bool preference should roundtrip in memory"))
        return 1;
    if (Expect(preferences.IntValue(QStringLiteral("diagnostics.maxItems")) ==
                   250,
               "int preference should roundtrip in memory"))
        return 1;
    if (Expect(preferences.Keys().size() == 3,
               "preferences should expose stored keys"))
        return 1;

    QTemporaryDir tempDir;
    if (Expect(tempDir.isValid(), "temporary directory should be available"))
        return 1;

    const QString preferencesPath =
        QDir(tempDir.path()).filePath(QStringLiteral("preferences.json"));

    QString errorMessage;
    if (Expect(preferences.Save(preferencesPath, &errorMessage),
               "Save should write preferences JSON"))
        return 1;

    xq::core::PreferencesService loadedPreferences;
    if (Expect(loadedPreferences.Load(preferencesPath, &errorMessage),
               "Load should read preferences JSON"))
        return 1;
    if (Expect(loadedPreferences.StringValue(QStringLiteral("ui.theme")) ==
                   QStringLiteral("system"),
               "loaded string preference should match saved value"))
        return 1;
    if (Expect(loadedPreferences.BoolValue(QStringLiteral("render.showAxes")),
               "loaded bool preference should match saved value"))
        return 1;
    if (Expect(loadedPreferences.IntValue(QStringLiteral("diagnostics.maxItems")) ==
                   250,
               "loaded int preference should match saved value"))
        return 1;

    const QString invalidPath =
        QDir(tempDir.path()).filePath(QStringLiteral("invalid.json"));
    QFile invalidFile(invalidPath);
    if (Expect(invalidFile.open(QIODevice::WriteOnly),
               "invalid preference fixture should be writable"))
        return 1;
    invalidFile.write("{not-json");
    invalidFile.close();

    errorMessage.clear();
    if (Expect(!loadedPreferences.Load(invalidPath, &errorMessage),
               "Load should reject invalid JSON"))
        return 1;
    if (Expect(!errorMessage.trimmed().isEmpty(),
               "invalid preference file should produce an error"))
        return 1;

    auto* context = xq::core::ApplicationContext::CreateDefault();
    if (Expect(context->Preferences() != nullptr,
               "ApplicationContext should expose PreferencesService"))
    {
        delete context;
        return 1;
    }
    if (Expect(context->Preferences()->Keys().isEmpty(),
               "ApplicationContext PreferencesService should start empty"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
