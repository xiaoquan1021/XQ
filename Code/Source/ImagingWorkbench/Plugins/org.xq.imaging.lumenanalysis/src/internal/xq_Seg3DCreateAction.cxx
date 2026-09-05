#include "xq_Seg3DCreateAction.h"

#include <mitkDataNode.h>
#include <mitkDataStorage.h>
#include <mitkImage.h>
#include <mitkNodePredicateDataType.h>
#include <mitkRenderingManager.h>
#include <mitkDataNodeSelection.h>

#include <berryPlatformUI.h>
#include <berryISelectionService.h>

#include <xq_PipelineDataUtils.h>

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

mitk::DataNode::Pointer xq_Seg3DCreateAction::ResolvePreferredImageNode() const
{
  if (m_DataStorage.IsNull())
    return nullptr;

  auto workbench = berry::PlatformUI::GetWorkbench();
  if (workbench != nullptr)
  {
    auto window = workbench->GetActiveWorkbenchWindow();
    if (window.IsNotNull())
    {
      auto* selectionService = window->GetSelectionService();
      if (selectionService)
      {
        berry::ISelection::ConstPointer selection =
          selectionService->GetSelection("org.xq.views.datamanager");
        mitk::DataNodeSelection::ConstPointer nodeSelection =
          selection.Cast<const mitk::DataNodeSelection>();
        if (nodeSelection.IsNull())
        {
          selection = selectionService->GetSelection();
          nodeSelection = selection.Cast<const mitk::DataNodeSelection>();
        }
        if (nodeSelection.IsNotNull())
        {
          const auto selectedNodes = nodeSelection->GetSelectedDataNodes();
          for (const auto& node : selectedNodes)
          {
            if (node.IsNull())
              continue;

            if (dynamic_cast<mitk::Image*>(node->GetData()) != nullptr)
              return node;

            auto upstreamImage = xq::pipeline::ResolveUpstreamNode(
              m_DataStorage.GetPointer(),
              node.GetPointer(),
              xq::pipeline::kSourceImageProperty,
              xq::pipeline::Stage::Unknown);
            if (upstreamImage.IsNotNull())
              return upstreamImage;
          }
        }
      }
    }
  }

  mitk::DataStorage::SetOfObjects::ConstPointer imageNodes =
    m_DataStorage->GetSubset(mitk::NodePredicateDataType::New("Image"));
  if (imageNodes->size() == 1)
    return imageNodes->Begin()->Value();

  return nullptr;
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

  mitk::DataNode::Pointer preferredImage = ResolvePreferredImageNode();

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
  if (preferredImage.IsNotNull())
  {
    const int idx = comboImage->findText(QString::fromStdString(preferredImage->GetName()));
    if (idx >= 0)
      comboImage->setCurrentIndex(idx);
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

  Q_UNUSED(minVal);
  Q_UNUSED(maxVal);
  QMessageBox::information(nullptr, "3D Segmentation",
    "This legacy action does not create placeholder 3D segmentation nodes. "
    "Open the 3D Segmentation tool and create a real working segmentation from the selected image.");
}

void xq_Seg3DCreateAction::PerformRegionGrowing(
  mitk::DataNode::Pointer imageNode, double minVal, double maxVal)
{
  if (imageNode.IsNull())
    return;

  Q_UNUSED(minVal);
  Q_UNUSED(maxVal);
  QMessageBox::information(nullptr, "3D Segmentation",
    "This legacy action does not create placeholder 3D segmentation nodes. "
    "Open the 3D Segmentation tool and create a real working segmentation from the selected image.");
}
