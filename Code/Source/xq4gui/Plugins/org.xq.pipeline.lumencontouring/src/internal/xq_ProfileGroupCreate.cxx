#include "xq_ProfileGroupCreate.h"
#include "ui_xq_ProfileGroupCreate.h"

#include <xq_PipelineDataUtils.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>

xq_ProfileGroupCreate::xq_ProfileGroupCreate(
  mitk::DataStorage::Pointer dataStorage, QWidget* parent)
  : QDialog(parent)
  , m_Ui(new Ui::xq_ProfileGroupCreate)
  , m_DataStorage(dataStorage)
{
  m_Ui->setupUi(this);
  setWindowTitle("Create Contour Group");

  // Populate path combo box from data storage
  if (m_DataStorage.IsNotNull())
  {
    mitk::DataStorage::SetOfObjects::ConstPointer allNodes = m_DataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
      const auto& node = it->Value();
      const bool isPath = xq::pipeline::IsPathNode(node);
      if (isPath)
      {
        m_Ui->comboPath->addItem(
          QString::fromStdString(node->GetName()));
      }
    }
  }

  connect(m_Ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_Ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

xq_ProfileGroupCreate::~xq_ProfileGroupCreate()
{
  delete m_Ui;
}

QString xq_ProfileGroupCreate::GetGroupName() const
{
  return m_Ui->editGroupName->text().trimmed();
}

QString xq_ProfileGroupCreate::GetSelectedPathName() const
{
  return m_Ui->comboPath->currentText();
}
