#include "xq_LoftParamWidget.h"
#include "ui_xq_LoftParamWidget.h"

xq_LoftParamWidget::xq_LoftParamWidget(QWidget* parent)
  : QWidget(parent)
  , m_Ui(new Ui::xq_LoftParamWidget)
{
  m_Ui->setupUi(this);
}

xq_LoftParamWidget::~xq_LoftParamWidget()
{
  delete m_Ui;
}

int xq_LoftParamWidget::GetNumSections() const
{
  return m_Ui->spinNumSections->value();
}

int xq_LoftParamWidget::GetSplineDegree() const
{
  return m_Ui->spinSplineDegree->value();
}

int xq_LoftParamWidget::GetSamplePerSection() const
{
  return m_Ui->spinSamplePerSection->value();
}

bool xq_LoftParamWidget::GetUseLinearSample() const
{
  return m_Ui->chkLinearSample->isChecked();
}

bool xq_LoftParamWidget::GetUseFFT() const
{
  return m_Ui->chkUseFFT->isChecked();
}

int xq_LoftParamWidget::GetNumOutputPoints() const
{
  return m_Ui->spinNumOutputPoints->value();
}

void xq_LoftParamWidget::SetDefaults()
{
  m_Ui->spinNumSections->setValue(12);
  m_Ui->spinSplineDegree->setValue(3);
  m_Ui->spinSamplePerSection->setValue(60);
  m_Ui->chkLinearSample->setChecked(false);
  m_Ui->chkUseFFT->setChecked(false);
  m_Ui->spinNumOutputPoints->setValue(120);
}
