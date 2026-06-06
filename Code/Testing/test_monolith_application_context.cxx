#include "Core/xq_ApplicationContext.h"

#include <QCoreApplication>
#include <QObject>
#include <QStringList>

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

    auto* context = xq::core::ApplicationContext::CreateDefault();
    int emittedDiagnostics = 0;
    QString lastDiagnostic;

    QObject::connect(context,
                     &xq::core::ApplicationContext::DiagnosticPosted,
                     [&emittedDiagnostics, &lastDiagnostic](const QString& message) {
                         ++emittedDiagnostics;
                         lastDiagnostic = message;
                     });

    if (Expect(context->Diagnostics().isEmpty(),
               "new application context should not contain diagnostics"))
    {
        delete context;
        return 1;
    }

    context->PostDiagnostic(QString());
    context->PostDiagnostic(QStringLiteral("   "));

    if (Expect(context->Diagnostics().isEmpty(),
               "empty diagnostics should not be stored"))
    {
        delete context;
        return 1;
    }
    if (Expect(emittedDiagnostics == 0,
               "empty diagnostics should not emit DiagnosticPosted"))
    {
        delete context;
        return 1;
    }

    context->PostDiagnostic(QStringLiteral("Ready"));
    context->PostDiagnostic(QStringLiteral("Loaded project"));

    const QStringList diagnostics = context->Diagnostics();
    if (Expect(diagnostics.size() == 2,
               "posted diagnostics should be stored in order"))
    {
        delete context;
        return 1;
    }
    if (Expect(diagnostics.at(0) == QStringLiteral("Ready"),
               "first diagnostic was not preserved"))
    {
        delete context;
        return 1;
    }
    if (Expect(diagnostics.at(1) == QStringLiteral("Loaded project"),
               "second diagnostic was not preserved"))
    {
        delete context;
        return 1;
    }
    if (Expect(emittedDiagnostics == 2,
               "non-empty diagnostics should still emit DiagnosticPosted"))
    {
        delete context;
        return 1;
    }
    if (Expect(lastDiagnostic == QStringLiteral("Loaded project"),
               "DiagnosticPosted should emit the stored diagnostic text"))
    {
        delete context;
        return 1;
    }

    delete context;
    return 0;
}
