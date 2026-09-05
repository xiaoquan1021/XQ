#include "xq_AboutDialog.h"
#include "ui_xq_AboutDialog.h"

#include <QPushButton>
#include <mitkVersion.h>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

xq_AboutDialog::xq_AboutDialog(QWidget* parent, Qt::WindowFlags flags)
    : QDialog(parent, flags)
    , m_Ui(new Ui::xq_AboutDialog)
{
    m_Ui->setupUi(this);
    setWindowTitle("About XQ");
    setMinimumSize(520, 480);

    ComposeContent();

    connect(m_Ui->okButton, &QPushButton::clicked, this, &QDialog::accept);
}

xq_AboutDialog::~xq_AboutDialog()
{
    delete m_Ui;
}

// ---------------------------------------------------------------------------
// Structured accessor — replaces the three individual getters
// ---------------------------------------------------------------------------

xq_AboutDialog::ApplicationInfo xq_AboutDialog::CollectApplicationInfo() const
{
    return {
        m_Ui->captionLabel->text(),
        m_Ui->versionLabel->text(),
        m_Ui->descriptionText->toPlainText(),
        GetBuildInfo()
    };
}

// ---------------------------------------------------------------------------
// Build-environment summary
// ---------------------------------------------------------------------------

QString xq_AboutDialog::GetBuildInfo() const
{
    return QStringLiteral("MITK %1 | Qt %2 | Compiled %3")
        .arg(QLatin1String(MITK_VERSION_STRING),
             QLatin1String(qVersion()),
             QLatin1String(__DATE__));
}

// ---------------------------------------------------------------------------
// Licence tag
// ---------------------------------------------------------------------------

QString xq_AboutDialog::GetLicenseInfo() const
{
    return QStringLiteral("BSD 3-Clause");
}

// ---------------------------------------------------------------------------
// Content composition — builds the HTML card in one place
// ---------------------------------------------------------------------------

void xq_AboutDialog::ComposeContent()
{
    m_Ui->captionLabel->setText(QStringLiteral("XQ"));
    m_Ui->captionLabel->setStyleSheet(
        QStringLiteral("font-size: 32px; font-weight: bold; color: #2563EB;"));

    m_Ui->versionLabel->setText(QStringLiteral("Version 1.0.0"));
    m_Ui->versionLabel->setStyleSheet(
        QStringLiteral("font-size: 13px; color: #1D4ED8;"));

    static constexpr const char* kFeatures[] = {
        "Path Planning — vessel centerline path definition",
        "2D Segmentation — cross-sectional vessel lumen segmentation",
        "3D Segmentation — volumetric medical image segmentation",
        "Solid Modeling — vascular surface model construction",
        "Mesh Generation — computational mesh generation",
        "Flow Simulation — full 3D hemodynamic flow simulation",
        "Reduced-Order Simulation — 0D/1D reduced-order modeling",
        "Multiphysics Simulation — coupled physics analyses",
    };

    QString rows;
    for (const char* feat : kFeatures)
        rows += QStringLiteral(
            "<tr><td style='color:#2563EB;'>&#x2022;</td>"
            "<td><b>%1</b></td></tr>").arg(QLatin1String(feat));

    const QString html = QStringLiteral(
        "<html><body style='font-size:12px; color:#1E293B;'>"
        "<p style='font-weight:bold; color:#2563EB;'>"
        "Cardiovascular Analysis &amp; Hemodynamic Simulation</p>"
        "<p><i>Empowering researchers with open-source computational "
        "cardiovascular tools.</i></p>"
        "<p>XQ provides a complete pipeline for image-based cardiovascular "
        "modeling:</p>"
        "<table cellpadding='4'>%1</table>"
        "<hr/>"
        "<p style='font-size:11px; color:#64748B;'>"
        "%2<br/>License: %3</p>"
        "</body></html>")
        .arg(rows, GetBuildInfo(), GetLicenseInfo());

    m_Ui->descriptionText->setHtml(html);
    m_Ui->descriptionText->setStyleSheet(
        QStringLiteral("border: none; background: transparent;"));
}
