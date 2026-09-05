#include <app/XQMainWindow.h>
#include <visualization/XQVolumeViewWidget.h>

#include <QApplication>
#include <QAction>
#include <QDockWidget>
#include <QFile>
#include <QMenuBar>
#include <QString>
#include <QTranslator>

#include <cstdio>

namespace {

int fail(const char* message)
{
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

} // namespace

int main(int argc, char** argv)
{
    xq::XQVolumeViewWidget::configureDefaultSurfaceFormat();
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(xq_resources);

    const QString zhFile = QString::fromUtf8("\xE6\x96\x87\xE4\xBB\xB6");
    const QString zhAxial = QString::fromUtf8("\xE8\xBD\xB4\xE4\xBD\x8D");
    const QString zhDataManager =
        QString::fromUtf8("\xE6\x95\xB0\xE6\x8D\xAE\xE7\xAE\xA1\xE7\x90\x86\xE5\x99\xA8");

    const QString qmPath = QStringLiteral(":/i18n/xq_zh_CN.qm");
    if (!QFile::exists(qmPath)) {
        return fail("Chinese .qm resource exists in xq_resources");
    }

    QTranslator translator;
    if (!translator.load(qmPath)) {
        return fail("Chinese translator loads from qrc");
    }
    if (translator.isEmpty()) {
        return fail("Chinese translator is not empty");
    }

    const QString fileMenu =
        translator.translate("xq::XQMainWindow", "&File");
    if (fileMenu.isEmpty() || fileMenu == QStringLiteral("&File")
        || !fileMenu.contains(zhFile)) {
        return fail("XQMainWindow &File translates to Chinese");
    }

    const QString axial =
        translator.translate("xq::XQMprWidget", "Axial");
    if (axial.isEmpty() || axial == QStringLiteral("Axial")
        || !axial.contains(zhAxial)) {
        return fail("XQMprWidget Axial translates to Chinese");
    }

    const QString genPath =
        translator.translate("XQStageWidgets", "Generate Path");
    if (genPath.isEmpty() || genPath == QStringLiteral("Generate Path")) {
        return fail("XQStageWidgets Generate Path translates to Chinese");
    }
    const QString pickPoints =
        translator.translate("XQStageWidgets", "Pick Control Points");
    if (pickPoints.isEmpty() || pickPoints == QStringLiteral("Pick Control Points")) {
        return fail("XQStageWidgets Pick Control Points translates to Chinese");
    }
    const QString openDicom = translator.translate(
        "xq::XQMainWindow", "Open DICOM Series");
    if (openDicom.isEmpty()
        || openDicom == QStringLiteral("Open DICOM Series")) {
        return fail("XQMainWindow DICOM import translates to Chinese");
    }
    const QString modulesAction = translator.translate(
        "xq::XQMainWindow", "Modules");
    if (modulesAction.isEmpty()
        || modulesAction == QStringLiteral("Modules")) {
        return fail("XQMainWindow Modules toolbar action translates to Chinese");
    }
    const QString preparePathInput = translator.translate(
        "XQStageWidgets", "Prepare Path input");
    if (preparePathInput.isEmpty()
        || preparePathInput == QStringLiteral("Prepare Path input")) {
        return fail("XQStageWidgets Path input action translates to Chinese");
    }
    const QString buildCenterlineB = translator.translate(
        "XQStageWidgets", "Build Centerline B");
    const QString centerlineBuilt = translator.translate(
        "XQStageWidgets", "OK: Centerline B Path input built");
    if (buildCenterlineB.isEmpty()
        || buildCenterlineB == QStringLiteral("Build Centerline B")
        || centerlineBuilt.isEmpty()
        || centerlineBuilt == QStringLiteral("OK: Centerline B Path input built")) {
        return fail("XQStageWidgets Centerline B action translates to Chinese");
    }
    const QString runPathModule = translator.translate(
        "XQStageWidgets", "Run Path module");
    if (runPathModule.isEmpty()
        || runPathModule == QStringLiteral("Run Path module")) {
        return fail("XQStageWidgets Path module action translates to Chinese");
    }
    const QString copyPathDump = translator.translate(
        "XQStageWidgets", "Copy Path dump");
    const QString pathDumpCopied = translator.translate(
        "XQStageWidgets", "Path dump copied.");
    if (copyPathDump.isEmpty()
        || copyPathDump == QStringLiteral("Copy Path dump")
        || pathDumpCopied.isEmpty()
        || pathDumpCopied == QStringLiteral("Path dump copied.")) {
        return fail("XQStageWidgets Path dump copy action translates to Chinese");
    }
    const QString goldPathSource = translator.translate(
        "XQStageWidgets", "Gold Path file");
    if (goldPathSource.isEmpty()
        || goldPathSource == QStringLiteral("Gold Path file")) {
        return fail("XQStageWidgets Path source translates to Chinese");
    }
    const QString automaticSegmentation = translator.translate(
        "XQStageWidgets", "Automatic Segmentation");
    const QString liverRoi = translator.translate(
        "XQStageWidgets", "Liver ROI");
    const QString coarseVesselRoi = translator.translate(
        "XQStageWidgets", "Coarse-vessel ROI");
    const QString contourWorkbench = translator.translate(
        "XQStageWidgets", "2D Contour Workbench");
    if (automaticSegmentation.isEmpty()
        || automaticSegmentation == QStringLiteral("Automatic Segmentation")
        || liverRoi.isEmpty() || liverRoi == QStringLiteral("Liver ROI")
        || coarseVesselRoi.isEmpty()
        || coarseVesselRoi == QStringLiteral("Coarse-vessel ROI")
        || contourWorkbench.isEmpty()
        || contourWorkbench == QStringLiteral("2D Contour Workbench")) {
        return fail("XQStageWidgets ROI-v2 segmentation action translates to Chinese");
    }

    const QString emptyHint =
        translator.translate("xq::XQVolumeViewWidget", "Nothing to render");
    if (emptyHint.isEmpty() || emptyHint == QStringLiteral("Nothing to render")) {
        return fail("XQVolumeViewWidget empty-scene hint translates to Chinese");
    }
    const QString memFmt =
        translator.translate("xq::XQMainWindow", "Mem %1 MB | Geometry %2 MB");
    if (memFmt.isEmpty()
        || memFmt == QStringLiteral("Mem %1 MB | Geometry %2 MB")) {
        return fail("XQMainWindow memory status translates to Chinese");
    }

    // Dialogs must be fully translated too (they are built once per open, so
    // the constructor-time tr() calls resolve against the installed catalog).
    const QString prefTitle =
        translator.translate("xq::XQPreferencesDialog", "Preferences");
    if (prefTitle.isEmpty() || prefTitle == QStringLiteral("Preferences")) {
        return fail("XQPreferencesDialog title translates to Chinese");
    }
    const QString prefBudget =
        translator.translate("xq::XQPreferencesDialog", "Geometry budget");
    if (prefBudget.isEmpty() || prefBudget == QStringLiteral("Geometry budget")) {
        return fail("XQPreferencesDialog geometry budget translates to Chinese");
    }
    const QString aboutTitle =
        translator.translate("xq::XQAboutDialog", "About XQ");
    if (aboutTitle.isEmpty() || aboutTitle == QStringLiteral("About XQ")) {
        return fail("XQAboutDialog title translates to Chinese");
    }

    xq::XQMainWindow window;
    if (window.menuBar()->actions().isEmpty()) {
        return fail("main window exposes menu actions");
    }
    const QString firstMenu = window.menuBar()->actions().front()->text();
    if (firstMenu == QStringLiteral("&File") || !firstMenu.contains(zhFile)) {
        return fail("main window defaults to Chinese File menu");
    }

    QDockWidget* dataManagerDock = window.findChild<QDockWidget*>("xqDataManagerDock");
    if (dataManagerDock == nullptr) {
        return fail("main window exposes Data Manager dock");
    }
    const QString dataManagerTitle = dataManagerDock->windowTitle();
    if (dataManagerTitle == QStringLiteral("Data Manager")
        || !dataManagerTitle.contains(zhDataManager)) {
        return fail("main window defaults to Chinese Data Manager dock");
    }

    QAction* englishAction = window.findChild<QAction*>("xqLanguageEnAction");
    QAction* chineseAction = window.findChild<QAction*>("xqLanguageZhAction");
    if (englishAction == nullptr || chineseAction == nullptr) {
        return fail("main window exposes language actions");
    }
    englishAction->trigger();
    app.processEvents();
    const QString englishMenu = window.menuBar()->actions().front()->text();
    if (!englishMenu.contains(QStringLiteral("File")) || englishMenu.contains(zhFile)) {
        return fail("language menu switches Chinese to English");
    }
    chineseAction->trigger();
    app.processEvents();
    if (!window.menuBar()->actions().front()->text().contains(zhFile)) {
        return fail("language menu switches English back to Chinese");
    }

    std::printf("i18n qrc translator checks passed\n");
    return 0;
}
