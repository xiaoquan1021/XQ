#include "Core/xq_DataImportCommand.h"
#include "Presentation/xq_QtFileImportPathProvider.h"

#include <QApplication>

#include <iostream>
#include <type_traits>

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
    QApplication app(argc, argv);

    static_assert(std::is_base_of<xq::core::FileImportPathProvider,
                                  xq::presentation::QtFileImportPathProvider>::value,
                  "Qt provider should implement the Core path-provider port");

    xq::presentation::QtFileImportPathProvider provider;

    if (Expect(provider.DialogCaption() == QStringLiteral("Import Medical Image"),
               "Qt file provider should expose the import dialog caption"))
        return 1;

    const QString filter = provider.FileFilter();
    if (Expect(filter.contains(QStringLiteral("Medical images")),
               "Qt file provider should name medical image filters"))
        return 1;
    if (Expect(filter.contains(QStringLiteral("*.nii")) &&
                   filter.contains(QStringLiteral("*.nii.gz")) &&
                   filter.contains(QStringLiteral("*.dcm")) &&
                   filter.contains(QStringLiteral("*.nrrd")) &&
                   filter.contains(QStringLiteral("*.mha")),
               "Qt file provider should include common medical image patterns"))
        return 1;
    if (Expect(filter.contains(QStringLiteral("All files")),
               "Qt file provider should include an all-files fallback"))
        return 1;

    return 0;
}
