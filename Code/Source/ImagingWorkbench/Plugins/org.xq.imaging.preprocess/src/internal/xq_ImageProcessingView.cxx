#include "xq_ImageProcessingView.h"

#include <xq_ImageProcessingUtils.h>
#include <xq_PipelineDataUtils.h>

#include <mitkDataNode.h>
#include <mitkDataStorage.h>
#include <mitkImage.h>
#include <mitkRenderingManager.h>
#include <mitkSurface.h>
#include <mitkDataNodeSelection.h>

#include <berryISelectionService.h>

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
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

#include <array>
#include <map>
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

  xq::pipeline::MarkGeneratedNode(
    node,
    xq::pipeline::Stage::ImageProcessing,
    operation,
    "org.xq.views.imageprocessing");
  node->SetStringProperty("xq.type", "image_processing_result");
  node->SetStringProperty("xq.image.processing.operation", operation.c_str());
  if (sourceNode)
  {
    node->SetStringProperty("xq.image.processing.source_node", sourceNode->GetName().c_str());
    node->SetStringProperty("xq.source.image", sourceNode->GetName().c_str());
  }
}

QString MakeResultNodeName(const mitk::DataNode* sourceNode, const QString& suffix)
{
  const QString sourceName = sourceNode
    ? QString::fromStdString(sourceNode->GetName())
    : QString("Image");
  return sourceName + "_" + suffix;
}

std::vector<double> ParseDoubles(const QString& text)
{
  std::vector<double> values;
  for (const auto& item : text.split(',', Qt::SkipEmptyParts))
    values.push_back(item.trimmed().toDouble());
  return values;
}

std::map<QString, QString> ParseParameterMap(const std::string& parameters)
{
  std::map<QString, QString> parsed;
  for (const auto& token : QString::fromStdString(parameters).split(';', Qt::SkipEmptyParts))
  {
    const int eq = token.indexOf('=');
    if (eq <= 0)
      continue;
    parsed[token.left(eq).trimmed()] = token.mid(eq + 1).trimmed();
  }
  return parsed;
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
  , m_ContextLabel(nullptr)
  , m_DiagnosticText(nullptr)
  , m_CurrentProcessingNode(nullptr)
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

  m_ContextLabel = new QLabel(parent);
  m_ContextLabel->setWordWrap(true);
  m_ContextLabel->setStyleSheet(
    "QLabel { background: #F8FAFC; border: 1px solid #B7C9F7; "
    "border-radius: 4px; padding: 6px; }");
  mainLayout->addWidget(m_ContextLabel);

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
  connect(m_ImageComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &xq_ImageProcessingView::UpdateApplyButtonState);
  connect(m_OperationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &xq_ImageProcessingView::UpdateParameterVisibility);
  connect(m_OperationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &xq_ImageProcessingView::PersistProcessingMetadata);
  connect(m_OperationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &xq_ImageProcessingView::UpdateApplyButtonState);
  for (auto* spin : {m_LowerSpinBox, m_UpperSpinBox, m_InsideSpinBox, m_OutsideSpinBox,
                     m_SpacingXSpinBox, m_SpacingYSpinBox, m_SpacingZSpinBox,
                     m_IsoValueSpinBox})
  {
    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &xq_ImageProcessingView::PersistProcessingMetadata);
    connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &xq_ImageProcessingView::UpdateApplyButtonState);
  }
  for (auto* spin : {m_SeedXSpinBox, m_SeedYSpinBox, m_SeedZSpinBox,
                     m_CropXSpinBox, m_CropYSpinBox, m_CropZSpinBox,
                     m_CropSizeXSpinBox, m_CropSizeYSpinBox, m_CropSizeZSpinBox})
  {
    connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &xq_ImageProcessingView::PersistProcessingMetadata);
    connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &xq_ImageProcessingView::UpdateApplyButtonState);
  }
  connect(m_ApplyButton, &QPushButton::clicked, this, &xq_ImageProcessingView::ApplyOperation);

  RefreshImageList();
  UpdateParameterVisibility();
  BindCurrentDataManagerSelection();
}

void xq_ImageProcessingView::SetFocus()
{
  if (m_ImageComboBox)
    m_ImageComboBox->setFocus();
  BindCurrentDataManagerSelection();
}

void xq_ImageProcessingView::BindCurrentDataManagerSelection()
{
  if (!GetSite() || GetSite()->GetWorkbenchWindow().IsNull())
    return;

  auto* selectionService = GetSite()->GetWorkbenchWindow()->GetSelectionService();
  if (!selectionService)
    return;

  berry::ISelection::ConstPointer selection =
      selectionService->GetSelection("org.xq.views.datamanager");
  mitk::DataNodeSelection::ConstPointer nodeSelection =
      selection.Cast<const mitk::DataNodeSelection>();
  if (nodeSelection.IsNull())
  {
    selection = selectionService->GetSelection();
    nodeSelection = selection.Cast<const mitk::DataNodeSelection>();
  }
  if (nodeSelection.IsNull())
    return;

  QList<mitk::DataNode::Pointer> nodes;
  const auto selectedNodes = nodeSelection->GetSelectedDataNodes();
  for (const auto& node : selectedNodes)
  {
    if (node.IsNotNull())
      nodes.push_back(node);
  }

  if (!nodes.empty())
    OnSelectionChanged(berry::IWorkbenchPart::Pointer(), nodes);
}

void xq_ImageProcessingView::OnSelectionChanged(
  berry::IWorkbenchPart::Pointer,
  const QList<mitk::DataNode::Pointer>& nodes)
{
  if (!m_ImageComboBox)
    return;

  RefreshImageList();

  for (const auto& node : nodes)
  {
    if (node.IsNull())
      continue;

    std::string stage;
    if (node->GetStringProperty("xq.pipeline.stage", stage) &&
        stage == "image_processing" &&
        RestoreProcessingMetadataForNode(node))
    {
      m_CurrentProcessingNode = node;
      const bool hasSourceImage = CurrentImageNode().IsNotNull();
      UpdateContextStatus(hasSourceImage ? "Ready" : "Missing source image",
                          !hasSourceImage);
      UpdateApplyButtonState();
      return;
    }

    if (dynamic_cast<mitk::Image*>(node->GetData()) == nullptr)
      continue;

    for (int i = 0; i < m_ImageNodes.size(); ++i)
    {
      if (m_ImageNodes[i] == node)
      {
        m_CurrentProcessingNode = nullptr;
        m_ImageComboBox->setCurrentIndex(i);
        UpdateContextStatus("Ready");
        UpdateApplyButtonState();
        return;
      }
    }
  }

  ClearImageProcessingState();
}

bool xq_ImageProcessingView::SelectImageNodeByName(const std::string& name)
{
  if (name.empty() || !m_ImageComboBox)
    return false;

  for (int i = 0; i < m_ImageNodes.size(); ++i)
  {
    if (m_ImageNodes[i].IsNotNull() && m_ImageNodes[i]->GetName() == name)
    {
      m_ImageComboBox->setCurrentIndex(i);
      return true;
    }
  }
  return false;
}

void xq_ImageProcessingView::SetOperationByName(const std::string& operation)
{
  if (!m_OperationComboBox)
    return;

  const QString op = QString::fromStdString(operation);
  int target = kOperationThreshold;
  if (op == "connected_threshold")
    target = kOperationConnectedThreshold;
  else if (op == "crop")
    target = kOperationCrop;
  else if (op == "resample")
    target = kOperationResample;
  else if (op == "marching_cubes")
    target = kOperationMarchingCubes;

  const int idx = m_OperationComboBox->findData(target);
  if (idx >= 0)
    m_OperationComboBox->setCurrentIndex(idx);
  UpdateParameterVisibility();
}

bool xq_ImageProcessingView::RestoreProcessingMetadataForNode(
  const mitk::DataNode::Pointer& node)
{
  if (node.IsNull())
    return false;

  std::string operation;
  node->GetStringProperty("xq.image.processing.operation", operation);
  if (!operation.empty())
    SetOperationByName(operation);

  std::string sourceName;
  if (!node->GetStringProperty("xq.source.image", sourceName) || sourceName.empty())
    node->GetStringProperty("xq.image.processing.source_node", sourceName);
  const bool selectedSource = SelectImageNodeByName(sourceName);

  double lower = 0.0;
  if (node->GetDoubleProperty("xq.image.processing.threshold.min", lower))
    m_LowerSpinBox->setValue(lower);
  double upper = 0.0;
  if (node->GetDoubleProperty("xq.image.processing.threshold.max", upper))
    m_UpperSpinBox->setValue(upper);

  std::string seedPoints;
  if (node->GetStringProperty("xq.image.processing.seed_points", seedPoints))
  {
    const auto seed = ParseDoubles(QString::fromStdString(seedPoints));
    if (seed.size() >= 3)
    {
      m_SeedXSpinBox->setValue(static_cast<int>(seed[0]));
      m_SeedYSpinBox->setValue(static_cast<int>(seed[1]));
      m_SeedZSpinBox->setValue(static_cast<int>(seed[2]));
    }
  }

  std::string parameters;
  if (node->GetStringProperty("xq.image.processing.parameters", parameters))
  {
    const auto parsed = ParseParameterMap(parameters);
    auto setDouble = [&](const QString& key, QDoubleSpinBox* spin) {
      auto it = parsed.find(key);
      if (it != parsed.end() && spin)
        spin->setValue(it->second.toDouble());
    };
    setDouble("lower", m_LowerSpinBox);
    setDouble("upper", m_UpperSpinBox);
    setDouble("inside", m_InsideSpinBox);
    setDouble("outside", m_OutsideSpinBox);
    setDouble("spacingX", m_SpacingXSpinBox);
    setDouble("spacingY", m_SpacingYSpinBox);
    setDouble("spacingZ", m_SpacingZSpinBox);
    setDouble("isovalue", m_IsoValueSpinBox);

    auto seedIt = parsed.find("seed");
    if (seedIt != parsed.end())
    {
      const auto seed = ParseDoubles(seedIt->second);
      if (seed.size() >= 3)
      {
        m_SeedXSpinBox->setValue(static_cast<int>(seed[0]));
        m_SeedYSpinBox->setValue(static_cast<int>(seed[1]));
        m_SeedZSpinBox->setValue(static_cast<int>(seed[2]));
      }
    }

    auto cropIt = parsed.find("crop");
    if (cropIt != parsed.end())
    {
      const auto crop = ParseDoubles(cropIt->second);
      if (crop.size() >= 6)
      {
        m_CropXSpinBox->setValue(static_cast<int>(crop[0]));
        m_CropYSpinBox->setValue(static_cast<int>(crop[1]));
        m_CropZSpinBox->setValue(static_cast<int>(crop[2]));
        m_CropSizeXSpinBox->setValue(static_cast<int>(crop[3]));
        m_CropSizeYSpinBox->setValue(static_cast<int>(crop[4]));
        m_CropSizeZSpinBox->setValue(static_cast<int>(crop[5]));
      }
    }

    auto spacingIt = parsed.find("spacing");
    if (spacingIt != parsed.end())
    {
      const auto spacing = ParseDoubles(spacingIt->second);
      if (spacing.size() >= 3)
      {
        m_SpacingXSpinBox->setValue(spacing[0]);
        m_SpacingYSpinBox->setValue(spacing[1]);
        m_SpacingZSpinBox->setValue(spacing[2]);
      }
    }
  }

  SetDiagnostic(QString("Restored image processing state for %1%2.")
    .arg(QString::fromStdString(node->GetName()))
    .arg(selectedSource ? QString() : QString(" (source image missing)")),
    !selectedSource);
  return true;
}

void xq_ImageProcessingView::PersistProcessingMetadata()
{
  if (m_CurrentProcessingNode.IsNull() || !m_OperationComboBox)
    return;

  const int op = m_OperationComboBox->currentData().toInt();
  std::string operation = "threshold";
  if (op == kOperationConnectedThreshold)
    operation = "connected_threshold";
  else if (op == kOperationCrop)
    operation = "crop";
  else if (op == kOperationResample)
    operation = "resample";
  else if (op == kOperationMarchingCubes)
    operation = "marching_cubes";

  m_CurrentProcessingNode->SetStringProperty(
    "xq.pipeline.algorithm", operation.c_str());
  m_CurrentProcessingNode->SetStringProperty(
    "xq.image.processing.operation", operation.c_str());

  auto sourceNode = CurrentImageNode();
  if (sourceNode.IsNotNull())
  {
    m_CurrentProcessingNode->SetStringProperty(
      "xq.source.image", sourceNode->GetName().c_str());
    m_CurrentProcessingNode->SetStringProperty(
      "xq.image.processing.source_node", sourceNode->GetName().c_str());
  }

  m_CurrentProcessingNode->SetStringProperty("xq.image.processing.parameters",
    QString("lower=%1;upper=%2;inside=%3;outside=%4;seed=%5,%6,%7;crop=%8,%9,%10,%11,%12,%13;spacing=%14,%15,%16;spacingX=%14;spacingY=%15;spacingZ=%16;isovalue=%17")
      .arg(m_LowerSpinBox->value())
      .arg(m_UpperSpinBox->value())
      .arg(m_InsideSpinBox->value())
      .arg(m_OutsideSpinBox->value())
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
  m_CurrentProcessingNode->SetStringProperty("xq.image.processing.seed_points",
    QString("%1,%2,%3")
      .arg(m_SeedXSpinBox->value())
      .arg(m_SeedYSpinBox->value())
      .arg(m_SeedZSpinBox->value()).toStdString().c_str());
  m_CurrentProcessingNode->SetDoubleProperty(
    "xq.image.processing.threshold.min", m_LowerSpinBox->value());
  m_CurrentProcessingNode->SetDoubleProperty(
    "xq.image.processing.threshold.max", m_UpperSpinBox->value());
  m_CurrentProcessingNode->Modified();
}

void xq_ImageProcessingView::RefreshImageList()
{
  const QSignalBlocker blocker(m_ImageComboBox);
  m_ImageComboBox->setUpdatesEnabled(false);
  m_ImageNodes.clear();
  m_ImageComboBox->clear();

  auto ds = GetDataStorage();
  if (ds.IsNull())
  {
    m_ImageComboBox->setUpdatesEnabled(true);
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
  m_ImageComboBox->setUpdatesEnabled(true);

  SetDiagnostic(QString("Found %1 image node(s).").arg(m_ImageNodes.size()));
  UpdateContextStatus(m_ImageNodes.isEmpty() ? QStringLiteral("Missing input image")
                                             : QStringLiteral("Ready"),
                      m_ImageNodes.isEmpty());
  UpdateApplyButtonState();
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
  UpdateContextStatus();
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

bool xq_ImageProcessingView::IsApplyReady(QString* reason) const
{
  auto setReason = [reason](const QString& text) {
    if (reason)
      *reason = text;
    return false;
  };

  auto sourceNode = CurrentImageNode();
  if (sourceNode.IsNull())
    return setReason(QStringLiteral("Missing input image"));

  auto* image = dynamic_cast<mitk::Image*>(sourceNode->GetData());
  if (!image || !image->GetVtkImageData())
    return setReason(QStringLiteral("Selected node does not contain a usable image"));

  if (!m_OperationComboBox)
    return setReason(QStringLiteral("Missing operation selector"));

  const int op = m_OperationComboBox->currentData().toInt();
  if ((op == kOperationThreshold || op == kOperationConnectedThreshold) &&
      m_LowerSpinBox && m_UpperSpinBox &&
      m_LowerSpinBox->value() > m_UpperSpinBox->value())
  {
    return setReason(QStringLiteral("Invalid threshold range"));
  }

  if (op == kOperationCrop &&
      ((m_CropSizeXSpinBox && m_CropSizeXSpinBox->value() <= 0) ||
       (m_CropSizeYSpinBox && m_CropSizeYSpinBox->value() <= 0) ||
       (m_CropSizeZSpinBox && m_CropSizeZSpinBox->value() <= 0)))
  {
    return setReason(QStringLiteral("Invalid crop size"));
  }

  if (reason)
    *reason = QStringLiteral("Ready");
  return true;
}

void xq_ImageProcessingView::UpdateApplyButtonState()
{
  if (!m_ApplyButton)
    return;

  QString reason;
  const bool ready = IsApplyReady(&reason);
  m_ApplyButton->setEnabled(ready);
  UpdateContextStatus(reason, !ready);
}

void xq_ImageProcessingView::UpdateContextStatus(const QString& status, bool isError)
{
  if (!m_ContextLabel)
    return;

  auto imageNode = CurrentImageNode();
  const QString imageName = imageNode.IsNotNull()
    ? QString::fromStdString(imageNode->GetName())
    : QStringLiteral("<none>");
  const QString editableNode = m_CurrentProcessingNode.IsNotNull()
    ? QString::fromStdString(m_CurrentProcessingNode->GetName())
    : QStringLiteral("<new output>");
  const QString operation = m_OperationComboBox
    ? m_OperationComboBox->currentText()
    : QStringLiteral("<unknown>");
  const QString resolvedStatus = status.isEmpty()
    ? (imageNode.IsNotNull() ? QStringLiteral("Ready") : QStringLiteral("Missing input image"))
    : status;
  m_ContextLabel->setText(
    QString("Current input image: %1\nCurrent editable node: %2\nOperation: %3\nStatus: %4\nNext: Apply creates a traced preprocessing output with source metadata.")
      .arg(imageName, editableNode, operation, resolvedStatus));
  m_ContextLabel->setStyleSheet(isError
    ? "QLabel { background: #FFF7ED; border: 1px solid #FDBA74; border-radius: 4px; padding: 6px; }"
    : "QLabel { background: #F8FAFC; border: 1px solid #B7C9F7; border-radius: 4px; padding: 6px; }");
}

void xq_ImageProcessingView::ClearImageProcessingState()
{
  m_CurrentProcessingNode = nullptr;
  if (m_ImageComboBox)
    m_ImageComboBox->setCurrentIndex(m_ImageNodes.isEmpty() ? -1 : 0);
  SetDiagnostic("No compatible image node is selected.");
  UpdateContextStatus("Missing input image", true);
  UpdateApplyButtonState();
}

void xq_ImageProcessingView::ApplyOperation()
{
  QString readiness;
  if (!IsApplyReady(&readiness))
  {
    SetDiagnostic(readiness, true);
    QMessageBox::warning(m_Control, "Image Processing", readiness);
    return;
  }

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
    QString("lower=%1;upper=%2;inside=%3;outside=%4;seed=%5,%6,%7;crop=%8,%9,%10,%11,%12,%13;spacing=%14,%15,%16;spacingX=%14;spacingY=%15;spacingZ=%16;isovalue=%17")
      .arg(m_LowerSpinBox->value())
      .arg(m_UpperSpinBox->value())
      .arg(m_InsideSpinBox->value())
      .arg(m_OutsideSpinBox->value())
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
  resultNode->SetStringProperty("xq.params.image_processing.operation", operation.c_str());
  resultNode->SetStringProperty(
    "xq.params.image_processing.parameters",
    QString("lower=%1;upper=%2;inside=%3;outside=%4;seed=%5,%6,%7;crop=%8,%9,%10,%11,%12,%13;spacing=%14,%15,%16;isovalue=%17")
      .arg(m_LowerSpinBox->value())
      .arg(m_UpperSpinBox->value())
      .arg(m_InsideSpinBox->value())
      .arg(m_OutsideSpinBox->value())
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
  resultNode->SetStringProperty("xq.units.length", "mm");
  resultNode->SetStringProperty("xq.image.processing.seed_points",
    QString("%1,%2,%3")
      .arg(m_SeedXSpinBox->value())
      .arg(m_SeedYSpinBox->value())
      .arg(m_SeedZSpinBox->value()).toStdString().c_str());
  resultNode->SetDoubleProperty("xq.image.processing.threshold.min", m_LowerSpinBox->value());
  resultNode->SetDoubleProperty("xq.image.processing.threshold.max", m_UpperSpinBox->value());
  resultNode->SetDoubleProperty("xq.params.image_processing.threshold.min", m_LowerSpinBox->value());
  resultNode->SetDoubleProperty("xq.params.image_processing.threshold.max", m_UpperSpinBox->value());
  resultNode->SetDoubleProperty("xq.params.image_processing.threshold.inside", m_InsideSpinBox->value());
  resultNode->SetDoubleProperty("xq.params.image_processing.threshold.outside", m_OutsideSpinBox->value());
  resultNode->SetStringProperty(
    "xq.params.image_processing.seed",
    QString("%1,%2,%3")
      .arg(m_SeedXSpinBox->value())
      .arg(m_SeedYSpinBox->value())
      .arg(m_SeedZSpinBox->value()).toStdString().c_str());
  resultNode->SetStringProperty(
    "xq.params.image_processing.crop",
    QString("%1,%2,%3,%4,%5,%6")
      .arg(m_CropXSpinBox->value())
      .arg(m_CropYSpinBox->value())
      .arg(m_CropZSpinBox->value())
      .arg(m_CropSizeXSpinBox->value())
      .arg(m_CropSizeYSpinBox->value())
      .arg(m_CropSizeZSpinBox->value()).toStdString().c_str());
  resultNode->SetStringProperty(
    "xq.params.image_processing.spacing",
    QString("%1,%2,%3")
      .arg(m_SpacingXSpinBox->value())
      .arg(m_SpacingYSpinBox->value())
      .arg(m_SpacingZSpinBox->value()).toStdString().c_str());
  resultNode->SetDoubleProperty("xq.params.image_processing.isovalue", m_IsoValueSpinBox->value());

  GetDataStorage()->Add(resultNode, sourceNode);
  m_CurrentProcessingNode = resultNode;
  mitk::RenderingManager::GetInstance()->RequestUpdateAll();
  RefreshImageList();
  SelectImageNodeByName(sourceNode->GetName());
  SetDiagnostic(QString("Created %1 from %2.")
    .arg(QString::fromStdString(resultNode->GetName()))
    .arg(QString::fromStdString(sourceNode->GetName())));
}
