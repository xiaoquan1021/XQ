#include "xq_ModelCreate.h"
#include "ui_xq_ModelCreate.h"

#include <mitkDataNode.h>
#include <mitkNodePredicateDataType.h>

#include <QMessageBox>

xq_ModelCreate::xq_ModelCreate(mitk::DataStorage::Pointer dataStorage,
                               QWidget* parent)
  : QDialog(parent)
  , m_Ui(new Ui::xq_ModelCreate)
  , m_DataStorage(dataStorage)
{
  m_Ui->setupUi(this);
  setWindowTitle("Create Model");

  // Populate segmentation groups from data storage
  if (m_DataStorage.IsNotNull())
  {
    mitk::NodePredicateDataType::Pointer predicate =
      mitk::NodePredicateDataType::New("ContourModelSet");
    mitk::DataStorage::SetOfObjects::ConstPointer segNodes =
      m_DataStorage->GetSubset(predicate);

    for (auto it = segNodes->Begin(); it != segNodes->End(); ++it)
    {
      m_Ui->listSegGroups->addItem(
        QString::fromStdString(it->Value()->GetName()));
    }
  }

  connect(m_Ui->comboModelType, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &xq_ModelCreate::OnModelTypeChanged);

  connect(m_Ui->btnOk, &QPushButton::clicked, this, &QDialog::accept);
  connect(m_Ui->btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

xq_ModelCreate::~xq_ModelCreate()
{
  delete m_Ui;
}

QString xq_ModelCreate::GetModelName() const
{
  return m_Ui->editModelName->text().trimmed();
}

QString xq_ModelCreate::GetModelType() const
{
  return m_Ui->comboModelType->currentText();
}

int xq_ModelCreate::GetNumSamplingPoints() const
{
  return m_Ui->spinSamplingPoints->value();
}

void xq_ModelCreate::OnModelTypeChanged(int index)
{
  // OCCT-specific lofting parameters only available for OCCT type
  bool isOCCT = (m_Ui->comboModelType->itemText(index) == "OCCT");
  m_Ui->grpLoftingParams->setVisible(isOCCT);
}
