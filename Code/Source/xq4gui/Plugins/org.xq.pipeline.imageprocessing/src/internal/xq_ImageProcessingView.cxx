#include "xq_ImageProcessingView.h"

#include <xq_ImageProcessingUtils.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>
#include <mitkImage.h>
#include <mitkRenderingManager.h>
#include <mitkSurface.h>

#include <vtkImageData.h>
#include <vtkSmartPointer.h>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

#include <array>
#include <string>
#include <vector>

const QString xq_ImageProcessingView::VIEW_ID = "org.xq.views.imageprocessing";

namespace
{

constexpr int kOperationThreshold = 0;
constexpr int kOperationConnectedThreshold = 1;
constexpr int kOperationCrop = 2;
constexpr int kOperationResample = 3;
constexpr int kOperationMarchingCubes = 4;

mitk::Image::Pointer CreateMitkImageFromVtk(vtkImageData* vtkImage)
{
  if (!vtkImage)
    return nullptr;

  auto templateImage = vtkSmartPointer<vtkImageData>::New();
  templateImage->DeepCopy(vtkImage);

  auto image = mitk::Image::New();
  image->Initialize(templateImage);
  auto* outVtk = image->GetVtkImageData();
  if (!outVtk)
    return nullptr;
  outVtk->DeepCopy(vtkImage);
  return image;
}

void SetCommonImageProcessingProperties(mitk::DataNode* node,
                                        const std::string& operation,
                                        const mitk::DataNode* sourceNode)
{
  if (!node)
    return;

  node->SetStringProperty("xq.type", "image_processing_result");
  node->SetStringProperty("xq.pipeline.stage", "image_processing");
  node->SetStringProperty("xq.image.processing.operation", operation.c_str());
  if (sourceNode)
  {
    node->SetStringProperty("xq.image.processing.source_node", sourceNode->GetName().c_str());
  }
}

QString MakeResultNodeName(const mitk::DataNode* sourceNode, const QString& suffix)
{
  const QString sourceName = sourceNode
    ? QString::fromStdString(sourceNode->GetName())
    : QString("Image");
  return sourceName + "_" + suffix;
}

} // namespace

xq_ImageProcessingView::xq_ImageProcessingView()
  : m_Control(nullptr)
  , m_ImageComboBox(nullptr)
  , m_OperationComboBox(nullptr)
  , m_ThresholdGroup(nullptr)
  , m_SeedGroup(nullptr)
  , m_CropGroup(nullptr)
  , m_ResampleGroup(nullptr)
  , m_LowerSpinBox(nullptr)
  , m_UpperSpinBox(nullptr)
  , m_InsideSpinBox(nullptr)
  , m_OutsideSpinBox(nullptr)
  , m_SeedXSpinBox(nullptr)
  , m_SeedYSpinBox(nullptr)
  , m_SeedZSpinBox(nullptr)
  , m_CropXSpinBox(nullptr)
  , m_CropYSpinBox(nullptr)
  , m_CropZSpinBox(nullptr)
  , m_CropSizeXSpinBox(nullptr)
  , m_CropSizeYSpinBox(nullptr)
  , m_CropSizeZSpinBox(nullptr)
  , m_SpacingXSpinBox(nullptr)
  , m_SpacingYSpinBox(nullptr)
  , m_SpacingZSpinBox(nullptr)
  , m_IsoValueSpinBox(nullptr)
  , m_ApplyButton(nullptr)
  , m_DiagnosticText(nullptr)
{
}

xq_ImageProcessingView::~xq_ImageProcessingView() = default;

void xq_ImageProcessingView::CreateQtPartControl(QWidget* parent)
{
  m_Control = parent;
  auto* mainLayout = new QVBoxLayout(parent);

  auto* intro = new QLabel(
    "Run image processing operations on loaded image nodes. Outputs are added to DataStorage with source and parameter metadata.",
    parent);
  intro->setWordWrap(true);
  mainLayout->addWidget(intro);

  auto* inputGroup = new QGroupBox("Input", parent);
  auto* inputLayout = new QFormLayout(inputGroup);
  m_ImageComboBox = new QComboBox(inputGroup);
  auto* refreshButton = new QPushButton("Refresh", inputGroup);
  auto* inputRow = new QHBoxLayout();
  inputRow->addWidget(m_ImageComboBox, 1);
  inputRow->addWidget(refreshButton);
  inputLayout->addRow("Image:", inputRow);
  mainLayout->addWidget(inputGroup);

  auto* operationGroup = new QGroupBox("Operation", parent);
  auto* operationLayout = new QFormLayout(operationGroup);
  m_OperationComboBox = new QComboBox(operationGroup);
  m_OperationComboBox->addItem("Threshold", kOperationThreshold);
  m_OperationComboBox->addItem("Connected Threshold", kOperationConnectedThreshold);
  m_OperationComboBox->addItem("Crop", kOperationCrop);
  m_OperationComboBox->addItem("Resample", kOperationResample);
  m_OperationComboBox->addItem("Marching Cubes", kOperationMarchingCubes);
  operationLayout->addRow("Tool:", m_OperationComboBox);
  mainLayout->addWidget(operationGroup);

  m_ThresholdGroup = new QGroupBox("Threshold Parameters", parent);
  auto* thresholdLayout = new QFormLayout(m_ThresholdGroup);
  m_LowerSpinBox = new QDoubleSpinBox(m_ThresholdGroup);
  m_LowerSpinBox->setRange(-100000.0, 100000.0);
  m_LowerSpinBox->setValue(100.0);
  m_UpperSpinBox = new QDoubleSpinBox(m_ThresholdGroup);
  m_UpperSpinBox->setRange(-100000.0, 100000.0);
  m_UpperSpinBox->setValue(600.0);
  m_InsideSpinBox = new QDoubleSpinBox(m_ThresholdGroup);
  m_InsideSpinBox->setRange(-100000.0, 100000.0);
  m_InsideSpinBox->setValue(1.0);
  m_OutsideSpinBox = new QDoubleSpinBox(m_ThresholdGroup);
  m_OutsideSpinBox->setRange(-100000.0, 100000.0);
  m_OutsideSpinBox->setValue(0.0);
  thresholdLayout->addRow("Lower:", m_LowerSpinBox);
  thresholdLayout->addRow("Upper:", m_UpperSpinBox);
  thresholdLayout->addRow("Inside Value:", m_InsideSpinBox);
  thresholdLayout->addRow("Outside Value:", m_OutsideSpinBox);
  mainLayout->addWidget(m_ThresholdGroup);

  m_SeedGroup = new QGroupBox("Seed", parent);
  auto* seedLayout = new QFormLayout(m_SeedGroup);
  m_SeedXSpinBox = new QSpinBox(m_SeedGroup);
  m_SeedYSpinBox = new QSpinBox(m_SeedGroup);
  m_SeedZSpinBox = new QSpinBox(m_SeedGroup);
  for (auto* spin : {m_SeedXSpinBox, m_SeedYSpinBox, m_SeedZSpinBox})
    spin->setRange(0, 1000000);
  seedLayout->addRow("X:", m_SeedXSpinBox);
  seedLayout->addRow("Y:", m_SeedYSpinBox);
  seedLayout->addRow("Z:", m_SeedZSpinBox);
  mainLayout->addWidget(m_SeedGroup);

  m_CropGroup = new QGroupBox("Crop Region", parent);
  auto* cropLayout = new QFormLayout(m_CropGroup);
  m_CropXSpinBox = new QSpinBox(m_CropGroup);
  m_CropYSpinBox = new QSpinBox(m_CropGroup);
  m_CropZSpinBox = new QSpinBox(m_CropGroup);
  m_CropSizeXSpinBox = new QSpinBox(m_CropGroup);
  m_CropSizeYSpinBox = new QSpinBox(m_CropGroup);
  m_CropSizeZSpinBox = new QSpinBox(m_CropGroup);
  for (auto* spin : {m_CropXSpinBox, m_CropYSpinBox, m_CropZSpinBox,
                     m_CropSizeXSpinBox, m_CropSizeYSpinBox, m_CropSizeZSpinBox})
    spin->setRange(0, 1000000);
  m_CropSizeXSpinBox->setValue(32);
  m_CropSizeYSpinBox->setValue(32);
  m_CropSizeZSpinBox->setValue(32);
  cropLayout->addRow("Origin X:", m_CropXSpinBox);
  cropLayout->addRow("Origin Y:", m_CropYSpinBox);
  cropLayout->addRow("Origin Z:", m_CropZSpinBox);
  cropLayout->addRow("Size X:", m_CropSizeXSpinBox);
  cropLayout->addRow("Size Y:", m_CropSizeYSpinBox);
  cropLayout->addRow("Size Z:", m_CropSizeZSpinBox);
  mainLayout->addWidget(m_CropGroup);

  m_ResampleGroup = new QGroupBox("Resample / Surface", parent);
  auto* resampleLayout = new QFormLayout(m_ResampleGroup);
  m_SpacingXSpinBox = new QDoubleSpinBox(m_ResampleGroup);
  m_SpacingYSpinBox = new QDoubleSpinBox(m_ResampleGroup);
  m_SpacingZSpinBox = new QDoubleSpinBox(m_ResampleGroup);
  for (auto* spin : {m_SpacingXSpinBox, m_SpacingYSpinBox, m_SpacingZSpinBox})
  {
    spin->setRange(0.001, 1000.0);
    spin->setDecimals(3);
    spin->setValue(1.0);
  }
  m_IsoValueSpinBox = new QDoubleSpinBox(m_ResampleGroup);
  m_IsoValueSpinBox->setRange(-100000.0, 100000.0);
  m_IsoValueSpinBox->setValue(0.5);
  resampleLayout->addRow("Spacing X:", m_SpacingXSpinBox);
  resampleLayout->addRow("Spacing Y:", m_SpacingYSpinBox);
  resampleLayout->addRow("Spacing Z:", m_SpacingZSpinBox);
  resampleLayout->addRow("Isovalue:", m_IsoValueSpinBox);
  mainLayout->addWidget(m_ResampleGroup);

  m_ApplyButton = new QPushButton("Apply", parent);
  mainLayout->addWidget(m_ApplyButton);

  m_DiagnosticText = new QTextEdit(parent);
  m_DiagnosticText->setReadOnly(true);
  m_DiagnosticText->setMinimumHeight(90);
  mainLayout->addWidget(m_DiagnosticText);

  connect(refreshButton, &QPushButton::clicked, this, &xq_ImageProcessingView::RefreshImageList);
  connect(m_OperationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &xq_ImageProcessingView::UpdateParameterVisibility);
  connect(m_ApplyButton, &QPushButton::clicked, this, &xq_ImageProcessingView::ApplyOperation);

  RefreshImageList();
  UpdateParameterVisibility();
}

void xq_ImageProcessingView::SetFocus()
{
  if (m_ImageComboBox)
    m_ImageComboBox->setFocus();
}

void xq_ImageProcessingView::OnSelectionChanged(
  berry::IWorkbenchPart::Pointer,
  const QList<mitk::DataNode::Pointer>& nodes)
{
  if (!m_ImageComboBox)
    return;

  for (const auto& node : nodes)
  {
    if (node.IsNull() || dynamic_cast<mitk::Image*>(node->GetData()) == nullptr)
      continue;

    for (int i = 0; i < m_ImageNodes.size(); ++i)
    {
      if (m_ImageNodes[i] == node)
      {
        m_ImageComboBox->setCurrentIndex(i);
        return;
      }
    }
  }
}

void xq_ImageProcessingView::RefreshImageList()
{
  m_ImageNodes.clear();
  m_ImageComboBox->clear();

  auto ds = GetDataStorage();
  if (ds.IsNull())
  {
    SetDiagnostic("DataStorage is unavailable.", true);
    return;
  }

  auto allNodes = ds->GetAll();
  for (auto it = allNodes->Begin(); it != allNodes->End(); ++it)
  {
    auto node = it->Value();
    if (node.IsNull() || dynamic_cast<mitk::Image*>(node->GetData()) == nullptr)
      continue;
    m_ImageNodes.push_back(node);
    m_ImageComboBox->addItem(QString::fromStdString(node->GetName()));
  }

  SetDiagnostic(QString("Found %1 image node(s).").arg(m_ImageNodes.size()));
}

void xq_ImageProcessingView::UpdateParameterVisibility()
{
  const int op = m_OperationComboBox->currentData().toInt();
  m_ThresholdGroup->setVisible(op == kOperationThreshold || op == kOperationConnectedThreshold);
  m_SeedGroup->setVisible(op == kOperationConnectedThreshold);
  m_CropGroup->setVisible(op == kOperationCrop);
  m_ResampleGroup->setVisible(op == kOperationResample || op == kOperationMarchingCubes);

  m_InsideSpinBox->setVisible(op == kOperationThreshold);
  m_OutsideSpinBox->setVisible(op == kOperationThreshold);
}

mitk::DataNode::Pointer xq_ImageProcessingView::CurrentImageNode() const
{
  const int idx = m_ImageComboBox ? m_ImageComboBox->currentIndex() : -1;
  if (idx < 0 || idx >= m_ImageNodes.size())
    return nullptr;
  return m_ImageNodes[idx];
}

void xq_ImageProcessingView::SetDiagnostic(const QString& message, bool isError)
{
  if (!m_DiagnosticText)
    return;
  m_DiagnosticText->setPlainText(message);
  m_DiagnosticText->setStyleSheet(isError ? "color: #b00020;" : "color: #1b5e20;");
}

void xq_ImageProcessingView::ApplyOperation()
{
  auto sourceNode = CurrentImageNode();
  if (sourceNode.IsNull())
  {
    SetDiagnostic("Select an input image first.", true);
    QMessageBox::warning(m_Control, "Image Processing", "Select an input image first.");
    return;
  }

  auto image = dynamic_cast<mitk::Image*>(sourceNode->GetData());
  if (!image || !image->GetVtkImageData())
  {
    SetDiagnostic("Selected node does not contain a usable image.", true);
    QMessageBox::warning(m_Control, "Image Processing", "Selected node does not contain a usable image.");
    return;
  }

  const int op = m_OperationComboBox->currentData().toInt();
  xq_ImageResult result;
  QString suffix;
  std::string operation;

  if (op == kOperationThreshold)
  {
    operation = "threshold";
    suffix = "threshold";
    result = xq_ImageProcessingUtils::BinaryThreshold(
      image->GetVtkImageData(),
      m_LowerSpinBox->value(),
      m_UpperSpinBox->value(),
      m_InsideSpinBox->value(),
      m_OutsideSpinBox->value());
  }
  else if (op == kOperationConnectedThreshold)
  {
    operation = "connected_threshold";
    suffix = "connected_threshold";
    std::vector<std::array<int, 3>> seeds = {{
      m_SeedXSpinBox->value(),
      m_SeedYSpinBox->value(),
      m_SeedZSpinBox->value()}};
    result = xq_ImageProcessingUtils::ConnectedThreshold(
      image->GetVtkImageData(), m_LowerSpinBox->value(), m_UpperSpinBox->value(), seeds);
  }
  else if (op == kOperationCrop)
  {
    operation = "crop";
    suffix = "cropped";
    result = xq_ImageProcessingUtils::Crop(
      image->GetVtkImageData(),
      m_CropXSpinBox->value(), m_CropYSpinBox->value(), m_CropZSpinBox->value(),
      m_CropSizeXSpinBox->value(), m_CropSizeYSpinBox->value(), m_CropSizeZSpinBox->value());
  }
  else if (op == kOperationResample)
  {
    operation = "resample";
    suffix = "resampled";
    result = xq_ImageProcessingUtils::Resample(
      image->GetVtkImageData(),
      m_SpacingXSpinBox->value(), m_SpacingYSpinBox->value(), m_SpacingZSpinBox->value());
  }
  else if (op == kOperationMarchingCubes)
  {
    operation = "marching_cubes";
    suffix = "surface";
    result = xq_ImageProcessingUtils::MarchingCubes(
      image->GetVtkImageData(), m_IsoValueSpinBox->value());
  }

  if (!result.ok)
  {
    const QString message = QString::fromStdString(result.diagnostic.empty()
      ? std::string("Operation failed without diagnostic.")
      : result.diagnostic);
    SetDiagnostic(message, true);
    QMessageBox::warning(m_Control, "Image Processing", message);
    return;
  }

  auto resultNode = mitk::DataNode::New();
  resultNode->SetName(MakeResultNodeName(sourceNode, suffix).toStdString());
  SetCommonImageProcessingProperties(resultNode, operation, sourceNode);

  if (result.image)
  {
    auto resultImage = CreateMitkImageFromVtk(result.image);
    if (resultImage.IsNull())
    {
      SetDiagnostic("Operation succeeded but MITK image creation failed.", true);
      QMessageBox::warning(m_Control, "Image Processing", "Operation succeeded but MITK image creation failed.");
      return;
    }
    resultNode->SetData(resultImage);
    resultNode->SetBoolProperty("xq.image.processing.output_is_image", true);
  }
  else if (result.surface)
  {
    auto surface = mitk::Surface::New();
    surface->SetVtkPolyData(result.surface);
    resultNode->SetData(surface);
    resultNode->SetBoolProperty("xq.image.processing.output_is_surface", true);
    resultNode->SetColor(0.9, 0.3, 0.2);
    resultNode->SetOpacity(0.6);
  }
  else
  {
    SetDiagnostic("Operation produced no output image or surface.", true);
    QMessageBox::warning(m_Control, "Image Processing", "Operation produced no output image or surface.");
    return;
  }

  resultNode->SetStringProperty("xq.image.processing.parameters",
    QString("lower=%1;upper=%2;seed=%3,%4,%5;crop=%6,%7,%8,%9,%10,%11;spacing=%12,%13,%14;isovalue=%15")
      .arg(m_LowerSpinBox->value())
      .arg(m_UpperSpinBox->value())
      .arg(m_SeedXSpinBox->value())
      .arg(m_SeedYSpinBox->value())
      .arg(m_SeedZSpinBox->value())
      .arg(m_CropXSpinBox->value())
      .arg(m_CropYSpinBox->value())
      .arg(m_CropZSpinBox->value())
      .arg(m_CropSizeXSpinBox->value())
      .arg(m_CropSizeYSpinBox->value())
      .arg(m_CropSizeZSpinBox->value())
      .arg(m_SpacingXSpinBox->value())
      .arg(m_SpacingYSpinBox->value())
      .arg(m_SpacingZSpinBox->value())
      .arg(m_IsoValueSpinBox->value()).toStdString().c_str());

  GetDataStorage()->Add(resultNode, sourceNode);
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  RefreshImageList();
  SetDiagnostic(QString("Created %1 from %2.")
    .arg(QString::fromStdString(resultNode->GetName()))
    .arg(QString::fromStdString(sourceNode->GetName())));
}
