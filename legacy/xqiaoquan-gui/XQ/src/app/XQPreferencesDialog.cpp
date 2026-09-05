#include "XQPreferencesDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace xq {

namespace {
// QSettings is opened with org "XQ" and app "XQ" everywhere, so the dialog and
// the static getters share the exact same backing store.
QSettings openSettings() {
    return QSettings(QStringLiteral("XQ"), QStringLiteral("XQ"));
}

// Setting keys (single source of truth for both the dialog and the getters).
constexpr auto kSingleViewKey = "view/defaultSingleView";
constexpr auto kCrosshairKey = "view/crosshair";
constexpr auto kSliceStepKey = "nav/sliceStep";
constexpr auto kStatusCoordsKey = "status/showCoordinates";

// Defaults.
constexpr bool kSingleViewDefault = false;
constexpr bool kCrosshairDefault = true;
constexpr int kSliceStepDefault = 1;
constexpr int kSliceStepMin = 1;
constexpr int kSliceStepMax = 20;
constexpr bool kStatusCoordsDefault = true;

int clampStep(int value) {
    if (value < kSliceStepMin) return kSliceStepMin;
    if (value > kSliceStepMax) return kSliceStepMax;
    return value;
}

constexpr auto kGeometryBudgetKey = "memory/geometryBudgetMiB";
constexpr int kGeometryBudgetDefault = 2048;
constexpr int kGeometryBudgetMin = 256;
constexpr int kGeometryBudgetMax = 65536;

int clampBudget(int value) {
    if (value < kGeometryBudgetMin) return kGeometryBudgetMin;
    if (value > kGeometryBudgetMax) return kGeometryBudgetMax;
    return value;
}
} // namespace

XQPreferencesDialog::XQPreferencesDialog(QWidget* parent) : QDialog(parent) {
    setObjectName("xqPreferencesDialog");
    setWindowTitle(tr("Preferences"));
    setModal(true);
    setMinimumWidth(420);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    // "Viewing" group: single-view default, MPR crosshair, slice step.
    auto* viewingGroup = new QGroupBox(tr("Viewing"), this);
    auto* viewingForm = new QFormLayout(viewingGroup);

    auto* singleView = new QCheckBox(tr("Default to single view"), viewingGroup);
    singleView->setObjectName("xqPrefSingleView");
    viewingForm->addRow(singleView);

    auto* crosshair = new QCheckBox(tr("Show MPR crosshair"), viewingGroup);
    crosshair->setObjectName("xqPrefCrosshair");
    viewingForm->addRow(crosshair);

    auto* sliceStep = new QSpinBox(viewingGroup);
    sliceStep->setObjectName("xqPrefSliceStep");
    sliceStep->setRange(kSliceStepMin, kSliceStepMax);
    viewingForm->addRow(tr("Slice step"), sliceStep);

    layout->addWidget(viewingGroup);

    // "Status Bar" group: coordinate readout toggle.
    auto* statusGroup = new QGroupBox(tr("Status Bar"), this);
    auto* statusForm = new QFormLayout(statusGroup);

    auto* statusCoords =
        new QCheckBox(tr("Show coordinates in status bar"), statusGroup);
    statusCoords->setObjectName("xqPrefStatusCoords");
    statusForm->addRow(statusCoords);

    layout->addWidget(statusGroup);

    // "Memory" group: geometry residency budget for lazy-loaded assets.
    auto* memoryGroup = new QGroupBox(tr("Memory"), this);
    auto* memoryForm = new QFormLayout(memoryGroup);

    auto* geometryBudget = new QSpinBox(memoryGroup);
    geometryBudget->setObjectName("xqPrefGeometryBudget");
    geometryBudget->setRange(kGeometryBudgetMin, kGeometryBudgetMax);
    geometryBudget->setSingleStep(256);
    geometryBudget->setSuffix(" MiB");
    memoryForm->addRow(tr("Geometry budget"), geometryBudget);

    layout->addWidget(memoryGroup);

    // Bottom button box: OK / Cancel / Restore Defaults.
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
            QDialogButtonBox::RestoreDefaults,
        this);
    layout->addWidget(buttons);

    // OK writes preferences then closes; Cancel discards.
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        saveToSettings();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Restore Defaults resets the controls only; nothing is persisted until OK.
    connect(buttons, &QDialogButtonBox::clicked, this,
            [this, buttons, singleView, crosshair, sliceStep, statusCoords,
             geometryBudget](QAbstractButton* button) {
                if (buttons->buttonRole(button) ==
                    QDialogButtonBox::ResetRole) {
                    singleView->setChecked(kSingleViewDefault);
                    crosshair->setChecked(kCrosshairDefault);
                    sliceStep->setValue(kSliceStepDefault);
                    statusCoords->setChecked(kStatusCoordsDefault);
                    geometryBudget->setValue(kGeometryBudgetDefault);
                }
            });

    // Populate controls from persisted values (or defaults when unset).
    loadFromSettings();
}

void XQPreferencesDialog::loadFromSettings() {
    QSettings settings = openSettings();

    if (auto* w = findChild<QCheckBox*>("xqPrefSingleView")) {
        w->setChecked(
            settings.value(kSingleViewKey, kSingleViewDefault).toBool());
    }
    if (auto* w = findChild<QCheckBox*>("xqPrefCrosshair")) {
        w->setChecked(settings.value(kCrosshairKey, kCrosshairDefault).toBool());
    }
    if (auto* w = findChild<QSpinBox*>("xqPrefSliceStep")) {
        w->setValue(
            clampStep(settings.value(kSliceStepKey, kSliceStepDefault).toInt()));
    }
    if (auto* w = findChild<QCheckBox*>("xqPrefStatusCoords")) {
        w->setChecked(
            settings.value(kStatusCoordsKey, kStatusCoordsDefault).toBool());
    }
    if (auto* w = findChild<QSpinBox*>("xqPrefGeometryBudget")) {
        w->setValue(clampBudget(
            settings.value(kGeometryBudgetKey, kGeometryBudgetDefault).toInt()));
    }
}

void XQPreferencesDialog::saveToSettings() {
    QSettings settings = openSettings();

    if (auto* w = findChild<QCheckBox*>("xqPrefSingleView")) {
        settings.setValue(kSingleViewKey, w->isChecked());
    }
    if (auto* w = findChild<QCheckBox*>("xqPrefCrosshair")) {
        settings.setValue(kCrosshairKey, w->isChecked());
    }
    if (auto* w = findChild<QSpinBox*>("xqPrefSliceStep")) {
        settings.setValue(kSliceStepKey, clampStep(w->value()));
    }
    if (auto* w = findChild<QCheckBox*>("xqPrefStatusCoords")) {
        settings.setValue(kStatusCoordsKey, w->isChecked());
    }
    if (auto* w = findChild<QSpinBox*>("xqPrefGeometryBudget")) {
        settings.setValue(kGeometryBudgetKey, clampBudget(w->value()));
    }
}

bool XQPreferencesDialog::defaultSingleView() {
    QSettings settings = openSettings();
    return settings.value(kSingleViewKey, kSingleViewDefault).toBool();
}

bool XQPreferencesDialog::crosshairEnabled() {
    QSettings settings = openSettings();
    return settings.value(kCrosshairKey, kCrosshairDefault).toBool();
}

int XQPreferencesDialog::sliceStep() {
    QSettings settings = openSettings();
    return clampStep(settings.value(kSliceStepKey, kSliceStepDefault).toInt());
}

bool XQPreferencesDialog::showStatusCoordinates() {
    QSettings settings = openSettings();
    return settings.value(kStatusCoordsKey, kStatusCoordsDefault).toBool();
}

int XQPreferencesDialog::geometryBudgetMiB() {
    QSettings settings = openSettings();
    return clampBudget(
        settings.value(kGeometryBudgetKey, kGeometryBudgetDefault).toInt());
}

} // namespace xq
