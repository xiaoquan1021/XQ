#include "xq_QtSceneFilePathProvider.h"

#include <QFileDialog>
#include <QWidget>

namespace xq::presentation
{

QtSceneFilePathProvider::QtSceneFilePathProvider(QWidget* parent)
    : m_Parent(parent)
{
}

QString QtSceneFilePathProvider::SceneFilePath()
{
    return QFileDialog::getSaveFileName(
        m_Parent,
        QStringLiteral("Save All as MITK Scene"),
        QStringLiteral("xq-scene.mitk"),
        QStringLiteral("MITK scenes (*.mitk);;All files (*)"));
}

} // namespace xq::presentation
