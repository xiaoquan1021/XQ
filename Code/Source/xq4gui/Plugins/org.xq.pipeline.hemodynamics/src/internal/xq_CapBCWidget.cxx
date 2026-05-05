#include "xq_CapBCWidget.h"

#include <QComboBox>
#include <QStackedWidget>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QFileDialog>

xq_CapBCWidget::xq_CapBCWidget(QWidget* parent)
  : QWidget(parent)
  , m_ComboType(nullptr)
  , m_Stack(nullptr)
  , m_SpinFlowRate(nullptr)
  , m_EditWaveformFile(nullptr)
  , m_BtnBrowseWaveform(nullptr)
  , m_SpinResistance(nullptr)
  , m_SpinRp(nullptr)
  , m_SpinC(nullptr)
  , m_SpinRd(nullptr)
  , m_SpinRa(nullptr)
  , m_SpinCa(nullptr)
  , m_SpinRaMicro(nullptr)
  , m_SpinCim(nullptr)
  , m_SpinRv(nullptr)
  , m_EditImpedanceFile(nullptr)
  , m_BtnBrowseImpedance(nullptr)
{
  BuildLayout();
}

xq_CapBCWidget::~xq_CapBCWidget()
{
}

void xq_CapBCWidget::BuildLayout()
{
  QVBoxLayout* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  // Cap name label
  QLabel* lblCapName = new QLabel(m_CapName.isEmpty() ? "Cap BC" : m_CapName, this);
  mainLayout->addWidget(lblCapName);

  // BC type combo
  m_ComboType = new QComboBox(this);
  m_ComboType->addItem("Prescribed Velocities");
  m_ComboType->addItem("Resistance");
  m_ComboType->addItem("RCR");
  m_ComboType->addItem("Coronary");
  m_ComboType->addItem("Impedance");
  mainLayout->addWidget(m_ComboType);

  // Stacked widget for BC-specific parameters
  m_Stack = new QStackedWidget(this);

  // Page 0: Prescribed Velocities
  QWidget* pageVelocity = new QWidget(this);
  QFormLayout* velLayout = new QFormLayout(pageVelocity);
  m_SpinFlowRate = new QDoubleSpinBox(pageVelocity);
  m_SpinFlowRate->setRange(0.0, 1.0e+10);
  m_SpinFlowRate->setValue(0.0);
  m_SpinFlowRate->setDecimals(4);
  m_SpinFlowRate->setSingleStep(0.1);
  velLayout->addRow("Flow Rate:", m_SpinFlowRate);

  QHBoxLayout* waveLayout = new QHBoxLayout();
  m_EditWaveformFile = new QLineEdit(pageVelocity);
  m_EditWaveformFile->setPlaceholderText("Waveform file path");
  m_BtnBrowseWaveform = new QPushButton("Browse...", pageVelocity);
  waveLayout->addWidget(m_EditWaveformFile);
  waveLayout->addWidget(m_BtnBrowseWaveform);
  velLayout->addRow("Waveform File:", waveLayout);
  m_Stack->addWidget(pageVelocity);

  // Page 1: Resistance
  QWidget* pageResistance = new QWidget(this);
  QFormLayout* resLayout = new QFormLayout(pageResistance);
  m_SpinResistance = new QDoubleSpinBox(pageResistance);
  m_SpinResistance->setRange(0.0, 1.0e+15);
  m_SpinResistance->setValue(0.0);
  m_SpinResistance->setDecimals(4);
  m_SpinResistance->setSingleStep(100.0);
  resLayout->addRow("Resistance:", m_SpinResistance);
  m_Stack->addWidget(pageResistance);

  // Page 2: RCR
  QWidget* pageRCR = new QWidget(this);
  QFormLayout* rcrLayout = new QFormLayout(pageRCR);
  m_SpinRp = new QDoubleSpinBox(pageRCR);
  m_SpinRp->setRange(0.0, 1.0e+15);
  m_SpinRp->setDecimals(4);
  m_SpinRp->setSingleStep(100.0);
  rcrLayout->addRow("Proximal R:", m_SpinRp);

  m_SpinC = new QDoubleSpinBox(pageRCR);
  m_SpinC->setRange(0.0, 1.0e+15);
  m_SpinC->setDecimals(8);
  m_SpinC->setSingleStep(1.0e-5);
  rcrLayout->addRow("Capacitance:", m_SpinC);

  m_SpinRd = new QDoubleSpinBox(pageRCR);
  m_SpinRd->setRange(0.0, 1.0e+15);
  m_SpinRd->setDecimals(4);
  m_SpinRd->setSingleStep(100.0);
  rcrLayout->addRow("Distal R:", m_SpinRd);
  m_Stack->addWidget(pageRCR);

  // Page 3: Coronary
  QWidget* pageCoronary = new QWidget(this);
  QFormLayout* corLayout = new QFormLayout(pageCoronary);
  m_SpinRa = new QDoubleSpinBox(pageCoronary);
  m_SpinRa->setRange(0.0, 1.0e+15);
  m_SpinRa->setDecimals(4);
  corLayout->addRow("Ra:", m_SpinRa);

  m_SpinCa = new QDoubleSpinBox(pageCoronary);
  m_SpinCa->setRange(0.0, 1.0e+15);
  m_SpinCa->setDecimals(8);
  corLayout->addRow("Ca:", m_SpinCa);

  m_SpinRaMicro = new QDoubleSpinBox(pageCoronary);
  m_SpinRaMicro->setRange(0.0, 1.0e+15);
  m_SpinRaMicro->setDecimals(4);
  corLayout->addRow("Ra_micro:", m_SpinRaMicro);

  m_SpinCim = new QDoubleSpinBox(pageCoronary);
  m_SpinCim->setRange(0.0, 1.0e+15);
  m_SpinCim->setDecimals(8);
  corLayout->addRow("Cim:", m_SpinCim);

  m_SpinRv = new QDoubleSpinBox(pageCoronary);
  m_SpinRv->setRange(0.0, 1.0e+15);
  m_SpinRv->setDecimals(4);
  corLayout->addRow("Rv:", m_SpinRv);
  m_Stack->addWidget(pageCoronary);

  // Page 4: Impedance
  QWidget* pageImpedance = new QWidget(this);
  QFormLayout* impLayout = new QFormLayout(pageImpedance);
  QHBoxLayout* impFileLayout = new QHBoxLayout();
  m_EditImpedanceFile = new QLineEdit(pageImpedance);
  m_EditImpedanceFile->setPlaceholderText("Impedance file path");
  m_BtnBrowseImpedance = new QPushButton("Browse...", pageImpedance);
  impFileLayout->addWidget(m_EditImpedanceFile);
  impFileLayout->addWidget(m_BtnBrowseImpedance);
  impLayout->addRow("Impedance File:", impFileLayout);
  m_Stack->addWidget(pageImpedance);

  mainLayout->addWidget(m_Stack);

  // Connections
  connect(m_ComboType, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &xq_CapBCWidget::OnBCTypeChanged);
  connect(m_BtnBrowseWaveform, &QPushButton::clicked,
          this, &xq_CapBCWidget::BrowseWaveformFile);
  connect(m_BtnBrowseImpedance, &QPushButton::clicked,
          this, &xq_CapBCWidget::BrowseImpedanceFile);

  m_Stack->setCurrentIndex(0);
}

void xq_CapBCWidget::SetCapName(const QString& name)
{
  m_CapName = name;
}

QString xq_CapBCWidget::GetBCType() const
{
  return m_ComboType->currentText();
}

QMap<QString, double> xq_CapBCWidget::GetBCValues() const
{
  QMap<QString, double> values;
  int idx = m_ComboType->currentIndex();

  switch (idx)
  {
    case 0: // Prescribed Velocities
      values["flowRate"] = m_SpinFlowRate->value();
      break;

    case 1: // Resistance
      values["resistance"] = m_SpinResistance->value();
      break;

    case 2: // RCR
      values["Rp"] = m_SpinRp->value();
      values["C"] = m_SpinC->value();
      values["Rd"] = m_SpinRd->value();
      break;

    case 3: // Coronary
      values["Ra"] = m_SpinRa->value();
      values["Ca"] = m_SpinCa->value();
      values["Ra_micro"] = m_SpinRaMicro->value();
      values["Cim"] = m_SpinCim->value();
      values["Rv"] = m_SpinRv->value();
      break;

    case 4: // Impedance
      break;

    default:
      break;
  }

  return values;
}

void xq_CapBCWidget::OnBCTypeChanged(int index)
{
  m_Stack->setCurrentIndex(index);
}

void xq_CapBCWidget::BrowseWaveformFile()
{
  QString filePath = QFileDialog::getOpenFileName(
    this, "Select Waveform File", QString(),
    "Text Files (*.txt *.dat *.csv);;All Files (*)");
  if (!filePath.isEmpty())
    m_EditWaveformFile->setText(filePath);
}

void xq_CapBCWidget::BrowseImpedanceFile()
{
  QString filePath = QFileDialog::getOpenFileName(
    this, "Select Impedance File", QString(),
    "Text Files (*.txt *.dat *.csv);;All Files (*)");
  if (!filePath.isEmpty())
    m_EditImpedanceFile->setText(filePath);
}
