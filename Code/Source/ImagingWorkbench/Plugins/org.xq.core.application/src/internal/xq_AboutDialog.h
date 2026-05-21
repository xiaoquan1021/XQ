#ifndef XQ_ABOUT_DIALOG_H
#define XQ_ABOUT_DIALOG_H

#include <QDialog>

namespace Ui {
class xq_AboutDialog;
}

/// "About XQ" dialog that displays application identity, feature summary,
/// and build-environment metadata.  Designed with a card-based layout
/// rather than the plain label approach used elsewhere.
class xq_AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit xq_AboutDialog(QWidget* parent = nullptr,
                            Qt::WindowFlags flags = Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint);
    ~xq_AboutDialog();

    /// Structured accessors — differ from SV's flat Get/Set pattern
    struct ApplicationInfo {
        QString caption;
        QString version;
        QString description;
        QString buildEnvironment;
    };

    ApplicationInfo CollectApplicationInfo() const;

    /// Return a human-readable build-environment summary (compiler, MITK, Qt).
    QString GetBuildInfo() const;

    /// Return the short licence identifier embedded in the dialog.
    QString GetLicenseInfo() const;

private:
    void ComposeContent();

    Ui::xq_AboutDialog* m_Ui;
};

#endif // XQ_ABOUT_DIALOG_H
