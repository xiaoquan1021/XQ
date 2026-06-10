#include "xq_QtDicomImportPathProvider.h"

#include <QFileDialog>
#include <QWidget>

namespace xq::presentation
{

QtDicomImportPathProvider::QtDicomImportPathProvider(QWidget* parent)
    : m_Parent(parent)
{
}

QString QtDicomImportPathProvider::ChooseFilePath() const
{
    return QFileDialog::getExistingDirectory(m_Parent,
                                             DialogCaption(),
                                             QString());
}

QString QtDicomImportPathProvider::DialogCaption() const
{
    return QStringLiteral("Import DICOM Directory");
}

} // namespace xq::presentation
