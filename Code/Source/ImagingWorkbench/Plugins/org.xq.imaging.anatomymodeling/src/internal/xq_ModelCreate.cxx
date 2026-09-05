#include "xq_ModelCreate.h"
#include "ui_xq_ModelCreate.h"

#include <xq_ContourGroup.h>
#include <xq_PipelineDataUtils.h>
#include <xq_ProfileGroup.h>

#include <mitkDataNode.h>
#include <mitkNodePredicateDataType.h>

#include <QListWidget>
#include <QMessageBox>

#include <set>

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
    std::set<std::string> addedNames;

    for (const auto& node : xq::pipeline::GetNodesByStage(
           m_DataStorage, xq::pipeline::Stage::ContourGroup))
    {
      if (node.IsNull() || !addedNames.insert(node->GetName()).second)
        continue;
      m_Ui->listSegGroups->addItem(
        QString::fromStdString(node->GetName()));
    }

    auto allNodes = m_DataStorage->GetAll();
    for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
    {
      const auto& node = it->Value();
      if (node.IsNull() || !node->GetData())
        continue;
      const bool isContourGroup =
        dynamic_cast<xq_ProfileGroup*>(node->GetData()) != nullptr ||
        dynamic_cast<xq_ContourGroup*>(node->GetData()) != nullptr;
      if (!isContourGroup || !addedNames.insert(node->GetName()).second)
        continue;
      m_Ui->listSegGroups->addItem(
        QString::fromStdString(node->GetName()));
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

void xq_ModelCreate::SelectSegmentationGroups(
  const std::vector<std::string>& groupNames)
{
  if (!m_Ui || !m_Ui->listSegGroups || groupNames.empty())
    return;

  std::set<std::string> requested(groupNames.begin(), groupNames.end());
  for (int row = 0; row < m_Ui->listSegGroups->count(); ++row)
  {
    auto* item = m_Ui->listSegGroups->item(row);
    if (!item)
      continue;
    item->setSelected(requested.count(item->text().toStdString()) != 0);
  }
}

std::vector<std::string> xq_ModelCreate::GetSelectedSegmentationGroups() const
{
  std::vector<std::string> groups;
  if (!m_Ui || !m_Ui->listSegGroups)
    return groups;

  const auto selectedItems = m_Ui->listSegGroups->selectedItems();
  groups.reserve(selectedItems.size());
  for (auto* item : selectedItems)
  {
    if (item)
      groups.push_back(item->text().toStdString());
  }

  return groups;
}

void xq_ModelCreate::OnModelTypeChanged(int index)
{
  // OCCT-specific lofting parameters only available for OCCT type
  bool isOCCT = (m_Ui->comboModelType->itemText(index) == "OCCT");
  m_Ui->grpLoftingParams->setVisible(isOCCT);
}
