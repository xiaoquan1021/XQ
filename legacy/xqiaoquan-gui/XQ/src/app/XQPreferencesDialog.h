#ifndef XQ_APP_PREFERENCES_DIALOG_H
#define XQ_APP_PREFERENCES_DIALOG_H
#include <QDialog>
namespace xq {
// Application preferences, persisted via QSettings under org "XQ" / app "XQ".
// Pure UI + QSettings; no core/domain dependency. Read the static getters at
// startup to apply saved preferences; the dialog writes them on accept.
class XQPreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit XQPreferencesDialog(QWidget* parent = nullptr);

    // Convenience static accessors over the same QSettings keys, so callers can
    // read current preferences without opening the dialog.
    static bool defaultSingleView();      // key "view/defaultSingleView", default false
    static bool crosshairEnabled();       // key "view/crosshair", default true
    static int  sliceStep();              // key "nav/sliceStep", default 1 (1..20)
    static bool showStatusCoordinates();  // key "status/showCoordinates", default true
    static int  geometryBudgetMiB();      // key "memory/geometryBudgetMiB", default 2048 (256..65536)

private:
    void loadFromSettings();
    void saveToSettings();
};
} // namespace xq
#endif
