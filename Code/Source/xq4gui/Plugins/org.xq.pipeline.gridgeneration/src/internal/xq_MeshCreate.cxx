#include "xq_MeshCreate.h"
#include "ui_xq_MeshCreate.h"

#include <mitkDataNode.h>
#include <mitkNodePredicateDataType.h>

#include <xq_Model.h>

#include <QMessageBox>

xq_GridCreate::xq_GridCreate(mitk::DataStorage::Pointer dataStorage,
                             QWidget* parent)
  : QDialog(parent)
  , m_Ui(new Ui::xq_MeshCreate)
  , m_DataStorage(dataStorage)
{
  m_Ui->setupUi(this);
  setWindowTitle("Create Mesh");

  // Populate model list from data storage
  if (m_DataStorage.IsNotNull())
  {
    mitk::NodePredicateDataType::Pointer predicate =
      mitk::NodePredicateDataType::New("xq_Model");
    mitk::DataStorage::SetOfObjects::ConstPointer modelNodes =
      m_DataStorage->GetSubset(predicate);

    for (auto it = modelNodes->Begin(); it != modelNodes->End(); ++it)
    {
      m_Ui->comboModel->addItem(
        QString::fromStdString(it->Value()->GetName()));
    }
  }

  connect(m_Ui->btnOk, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_Ui->btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

xq_GridCreate::~xq_GridCreate()
{
  delete m_Ui;
}

QString xq_GridCreate::GetMeshName() const
{
  return m_Ui->editMeshName->text().trimmed();
}

QString xq_GridCreate::GetMeshType() const
{
  return m_Ui->comboMeshType->currentText();
}

QString xq_GridCreate::GetSelectedModelName() const
{
  return m_Ui->comboModel->currentText();
}
