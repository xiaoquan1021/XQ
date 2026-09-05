#include "xq_CenterlineSmoother.h"
#include "ui_xq_CenterlineSmoother.h"

xq_CenterlineSmoother::xq_CenterlineSmoother(QWidget* parent)
  : QDialog(parent)
  , m_Ui(new Ui::xq_CenterlineSmoother)
{
  m_Ui->setupUi(this);
  setWindowTitle("Smooth Path");

  m_Ui->methodComboBox->addItem("Fourier");
  m_Ui->methodComboBox->addItem("Moving Average");

  connect(m_Ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_Ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

xq_CenterlineSmoother::~xq_CenterlineSmoother()
{
  delete m_Ui;
}

int xq_CenterlineSmoother::GetSamplingNumber() const
{
  return m_Ui->samplingSpinBox->value();
}

QString xq_CenterlineSmoother::GetSmoothingMethod() const
{
  return m_Ui->methodComboBox->currentText();
}

int xq_CenterlineSmoother::GetOutputNumber() const
{
  return m_Ui->outputSpinBox->value();
}
