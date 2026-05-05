#include "xq_Seg3DCreateAction.h"

#include <mitkDataNode.h>
#include <mitkDataStorage.h>
#include <mitkImage.h>
#include <mitkNodePredicateDataType.h>
#include <mitkRenderingManager.h>

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>

xq_Seg3DCreateAction::xq_Seg3DCreateAction(
  mitk::DataStorage::Pointer dataStorage, QObject* parent)
  : QAction("Create 3D Segmentation...", parent)
  , m_DataStorage(dataStorage)
{
  connect(this, &QAction::triggered, this, &xq_Seg3DCreateAction::Execute);
}

xq_Seg3DCreateAction::~xq_Seg3DCreateAction()
{
}

void xq_Seg3DCreateAction::SetDataStorage(mitk::DataStorage::Pointer dataStorage)
{
  m_DataStorage = dataStorage;
}

void xq_Seg3DCreateAction::Execute()
{
  if (m_DataStorage.IsNull())
  {
    QMessageBox::warning(nullptr, "3D Segmentation", "No data storage available.");
    return;
  }

  // Find image nodes
  mitk::NodePredicateDataType::Pointer predicate =
    mitk::NodePredicateDataType::New("Image");
  mitk::DataStorage::SetOfObjects::ConstPointer imageNodes =
    m_DataStorage->GetSubset(predicate);

  if (imageNodes->empty())
  {
    QMessageBox::information(nullptr, "3D Segmentation",
      "No image data available. Load an image first.");
    return;
  }

  // Build dialog
  QDialog dialog(nullptr);
  dialog.setWindowTitle("Create 3D Segmentation");
  dialog.setMinimumWidth(350);

  QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

  // Image selection
  QComboBox* comboImage = new QComboBox(&dialog);
  for (auto it = imageNodes->Begin(); it != imageNodes->End(); ++it)
  {
    comboImage->addItem(QString::fromStdString(it->Value()->GetName()));
  }

  QFormLayout* formLayout = new QFormLayout;
  formLayout->addRow("Image:", comboImage);

  // Method selection
  QComboBox* comboMethod = new QComboBox(&dialog);
  comboMethod->addItem("Threshold");
  comboMethod->addItem("Region Growing");
  formLayout->addRow("Method:", comboMethod);

  // Threshold range
  QDoubleSpinBox* spinMin = new QDoubleSpinBox(&dialog);
  spinMin->setRange(-10000.0, 10000.0);
  spinMin->setValue(0.0);
  spinMin->setDecimals(2);
  formLayout->addRow("Min Threshold:", spinMin);

  QDoubleSpinBox* spinMax = new QDoubleSpinBox(&dialog);
  spinMax->setRange(-10000.0, 10000.0);
  spinMax->setValue(1000.0);
  spinMax->setDecimals(2);
  formLayout->addRow("Max Threshold:", spinMax);

  mainLayout->addLayout(formLayout);

  QDialogButtonBox* buttonBox = new QDialogButtonBox(
    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  mainLayout->addWidget(buttonBox);

  connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

  if (dialog.exec() != QDialog::Accepted)
    return;

  // Find selected image node
  int imageIdx = comboImage->currentIndex();
  if (imageIdx < 0)
    return;

  auto it = imageNodes->Begin();
  for (int i = 0; i < imageIdx; ++i)
    ++it;

  mitk::DataNode::Pointer imageNode = it->Value();
  double minVal = spinMin->value();
  double maxVal = spinMax->value();

  if (comboMethod->currentIndex() == 0)
    PerformThresholdSegmentation(imageNode, minVal, maxVal);
  else
    PerformRegionGrowing(imageNode, minVal, maxVal);
}

void xq_Seg3DCreateAction::PerformThresholdSegmentation(
  mitk::DataNode::Pointer imageNode, double minVal, double maxVal)
{
  if (imageNode.IsNull())
    return;

  // Create result node with segmentation parameters
  mitk::DataNode::Pointer resultNode = mitk::DataNode::New();
  resultNode->SetName(imageNode->GetName() + "_threshold_seg");
  resultNode->SetBoolProperty("xq.segmentation.3d", true);
  resultNode->SetStringProperty("xq.segmentation.method", "Threshold");
  resultNode->SetDoubleProperty("xq.segmentation.threshold.min", minVal);
  resultNode->SetDoubleProperty("xq.segmentation.threshold.max", maxVal);
  resultNode->SetColor(1.0f, 0.0f, 0.0f);
  resultNode->SetFloatProperty("opacity", 0.5f);

  m_DataStorage->Add(resultNode, imageNode);
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();

  QMessageBox::information(nullptr, "3D Segmentation",
    QString("Threshold segmentation created.\nRange: [%1, %2]")
      .arg(minVal).arg(maxVal));
}

void xq_Seg3DCreateAction::PerformRegionGrowing(
  mitk::DataNode::Pointer imageNode, double minVal, double maxVal)
{
  if (imageNode.IsNull())
    return;

  mitk::DataNode::Pointer resultNode = mitk::DataNode::New();
  resultNode->SetName(imageNode->GetName() + "_regiongrow_seg");
  resultNode->SetBoolProperty("xq.segmentation.3d", true);
  resultNode->SetStringProperty("xq.segmentation.method", "RegionGrowing");
  resultNode->SetDoubleProperty("xq.segmentation.threshold.min", minVal);
  resultNode->SetDoubleProperty("xq.segmentation.threshold.max", maxVal);
  resultNode->SetColor(0.0f, 0.0f, 1.0f);
  resultNode->SetFloatProperty("opacity", 0.5f);

  m_DataStorage->Add(resultNode, imageNode);
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();

  QMessageBox::information(nullptr, "3D Segmentation",
    QString("Region growing segmentation created.\nRange: [%1, %2]")
      .arg(minVal).arg(maxVal));
}
