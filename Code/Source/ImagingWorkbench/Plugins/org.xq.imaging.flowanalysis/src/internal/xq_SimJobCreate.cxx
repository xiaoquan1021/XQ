#include "xq_SimJobCreate.h"
#include "ui_xq_SimJobCreate.h"

#include <mitkDataNode.h>
#include <mitkNodePredicateDataType.h>

#include <xq_MitkGrid.h>

#include <QMessageBox>

xq_SimJobCreate::xq_SimJobCreate(mitk::DataStorage::Pointer dataStorage,
                                 QWidget* parent)
  : QDialog(parent)
  , m_Ui(new Ui::xq_SimJobCreate)
  , m_DataStorage(dataStorage)
{
  m_Ui->setupUi(this);
  setWindowTitle("Create Simulation Job");

  // Populate mesh combo from data storage
  if (m_DataStorage.IsNotNull())
  {
    mitk::NodePredicateDataType::Pointer predicate =
      mitk::NodePredicateDataType::New("xq_MitkGrid");
    mitk::DataStorage::SetOfObjects::ConstPointer meshNodes =
      m_DataStorage->GetSubset(predicate);

    for (auto it = meshNodes->Begin(); it != meshNodes->End(); ++it)
    {
      m_Ui->comboMesh->addItem(
        QString::fromStdString(it->Value()->GetName()));
    }
  }

  connect(m_Ui->btnOk, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_Ui->btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

xq_SimJobCreate::~xq_SimJobCreate()
{
  delete m_Ui;
}

QString xq_SimJobCreate::GetJobName() const
{
  return m_Ui->editJobName->text().trimmed();
}

QString xq_SimJobCreate::GetSelectedMesh() const
{
  return m_Ui->comboMesh->currentText();
}

void xq_SimJobCreate::SelectMesh(const QString& meshName)
{
  if (!m_Ui || !m_Ui->comboMesh || meshName.isEmpty())
    return;

  const int index = m_Ui->comboMesh->findText(meshName);
  if (index >= 0)
    m_Ui->comboMesh->setCurrentIndex(index);
}
