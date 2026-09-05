#ifndef XQ_STARTUP_SAFETY_H
#define XQ_STARTUP_SAFETY_H

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QStringList>
#include <QStandardPaths>

namespace xq::startup
{

inline QStringList CandidateWorkbenchStateRoots()
{
    QStringList candidateRoots;

    const auto xdgDataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (!xdgDataHome.isEmpty())
        candidateRoots << QDir(xdgDataHome).filePath(QStringLiteral("XQ"));

    const auto homePath = qEnvironmentVariable("HOME");
    if (!homePath.isEmpty())
        candidateRoots << QDir(homePath).filePath(QStringLiteral(".local/share/XQ"));

    const auto genericRoots =
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const auto& root : genericRoots)
        candidateRoots << QDir(root).filePath(QStringLiteral("XQ"));

    candidateRoots.removeDuplicates();
    return candidateRoots;
}

inline bool IsUnsafeWorkbenchRestoreState(const QString& workbenchXml)
{
    static const QStringList kUnsafeActiveParts = {
        QStringLiteral("org.xq.views.pathplanning"),
        QStringLiteral("org.xq.views.segmentation"),
        QStringLiteral("org.xq.views.mitksegmentation"),
        QStringLiteral("org.xq.views.modeling"),
        QStringLiteral("org.xq.views.meshing"),
        QStringLiteral("org.xq.views.simulation"),
        QStringLiteral("org.xq.views.romsimulation"),
        QStringLiteral("org.xq.views.multiphysics"),
        QStringLiteral("org.xq.views.imageprocessing")
    };

    for (const auto& activePart : kUnsafeActiveParts)
    {
        if (workbenchXml.contains(QStringLiteral("activePart=\"%1\"").arg(activePart)))
            return true;
    }

    return false;
}

inline bool BackupUnsafeWorkbenchState()
{
    bool backedUpAny = false;

    const auto candidateRoots = CandidateWorkbenchStateRoots();
    for (const auto& root : candidateRoots)
    {
        QDirIterator iterator(
            root,
            QStringList{QStringLiteral("workbench.xml")},
            QDir::Files,
            QDirIterator::Subdirectories);
        while (iterator.hasNext())
        {
            const auto path = iterator.next();
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            const auto workbenchXml = QString::fromUtf8(file.readAll());
            file.close();

            if (!IsUnsafeWorkbenchRestoreState(workbenchXml))
                continue;

            const auto backupPath = path + QStringLiteral(".unsafe-backup");
            QFile::remove(backupPath);
            if (QFile::rename(path, backupPath))
                backedUpAny = true;
        }
    }

    return backedUpAny;
}

} // namespace xq::startup

#endif // XQ_STARTUP_SAFETY_H
