#include "XQAboutDialog.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

namespace xq {

XQAboutDialog::XQAboutDialog(QWidget* parent) : QDialog(parent) {
    setObjectName("xqAboutDialog");
    setWindowTitle(tr("About XQ"));
    setModal(true);
    setMinimumWidth(380);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    // Top product title.
    auto* title = new QLabel("XQ", this);
    title->setObjectName("xqAboutTitle");
    QFont titleFont = title->font();
    titleFont.setPointSize(22);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    // Subtitle in muted small text.
    auto* subtitle =
        new QLabel(tr("Vascular Imaging Modeling & Blood-Flow Analysis"), this);
    subtitle->setStyleSheet("color: gray;");
    layout->addWidget(subtitle);

    // Horizontal separator.
    auto* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    // Component versions the app is built against (static baseline strings).
    auto* versions = new QFormLayout();
    versions->setLabelAlignment(Qt::AlignRight);
    versions->addRow("Qt", new QLabel("6.7.0", this));
    versions->addRow("VTK", new QLabel("9.3.0", this));
    versions->addRow(tr("Build"), new QLabel("Release", this));
    layout->addLayout(versions);

    // Wrapped description blurb.
    auto* description = new QLabel(
        tr("A desktop workbench for centerline extraction, segmentation, "
           "surface/volume meshing and reduced-order flow analysis."),
        this);
    description->setWordWrap(true);
    layout->addWidget(description);

    // Bottom button box with a single Close button.
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
}

} // namespace xq
