#include "xq_QtFileImportPathProvider.h"

#include <QFileDialog>
#include <QWidget>

namespace xq::presentation
{

QtFileImportPathProvider::QtFileImportPathProvider(QWidget* parent)
    : m_Parent(parent)
{
}

QString QtFileImportPathProvider::ChooseFilePath() const
{
    return QFileDialog::getOpenFileName(m_Parent,
                                        DialogCaption(),
                                        QString(),
                                        FileFilter());
}

QString QtFileImportPathProvider::DialogCaption() const
{
    return QStringLiteral("Import Medical Image");
}

QString QtFileImportPathProvider::FileFilter() const
{
    return QStringLiteral(
        "Medical images (*.nii *.nii.gz *.dcm *.nrrd *.mha *.mhd);;"
        "All files (*)");
}

} // namespace xq::presentation
