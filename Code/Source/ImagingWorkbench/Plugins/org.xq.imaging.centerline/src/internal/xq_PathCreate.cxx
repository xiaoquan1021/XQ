#include "xq_PathCreate.h"
#include "ui_xq_PathCreate.h"

xq_PathCreate::xq_PathCreate(QWidget* parent)
  : QDialog(parent)
  , m_Ui(new Ui::xq_PathCreate)
{
  m_Ui->setupUi(this);
  setWindowTitle("Create New Path");

  connect(m_Ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_Ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

xq_PathCreate::~xq_PathCreate()
{
  delete m_Ui;
}

QString xq_PathCreate::GetPathName() const
{
  return m_Ui->nameLineEdit->text();
}

int xq_PathCreate::GetSubdivisionNumber() const
{
  return m_Ui->subdivisionSpinBox->value();
}

int xq_PathCreate::GetCalculationNumber() const
{
  return m_Ui->calculationSpinBox->value();
}
